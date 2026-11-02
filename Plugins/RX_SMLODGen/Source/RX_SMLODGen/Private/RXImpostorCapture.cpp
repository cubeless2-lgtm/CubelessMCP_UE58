#include "RXImpostorCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/ScopeExit.h"
#include "PreviewScene.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "UnrealClient.h"

bool RXImpostorCapture::Capture(UStaticMesh* Source, FIntPoint TileSize, TArray<TArray<FColor>>& Tiles, FString& Error)
{
    Error.Reset();
    if (!IsInGameThread() || !Source || Source->IsCompiling() || GUsingNullRHI ||
        TileSize.X < 16 || TileSize.Y < 16 || TileSize.X > 2048 || TileSize.Y > 2048)
    {
        Error = TEXT("Capture requires the game thread, a ready mesh, a rendering device, and tile dimensions from 16 to 2048.");
        return false;
    }
    const FBox Bounds = Source->GetBoundingBox();
    const FVector Center = Bounds.GetCenter();
    const FVector Extent = Bounds.GetExtent();
    if (!Bounds.IsValid || Center.ContainsNaN() || Extent.ContainsNaN() ||
        Extent.X <= UE_SMALL_NUMBER || Extent.Y <= UE_SMALL_NUMBER || Extent.Z <= UE_SMALL_NUMBER)
    {
        Error = TEXT("The source mesh must have finite, nonzero bounds on all axes.");
        return false;
    }

    // FPreviewScene destruction enqueues scene/component teardown. Wait for
    // that work before returning, including on error paths.
    ON_SCOPE_EXIT { FlushRenderingCommands(); };
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreatePhysicsScene(false).SetTransactional(false));
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
    Component->SetStaticMesh(Source);
    Component->SetForcedLodModel(1);
    Scene.AddComponent(Component, FTransform::Identity);

    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
    Target->RenderTargetFormat = RTF_RGBA16f;
    Target->ClearColor = FLinearColor(0, 0, 0, 1);
    Target->InitAutoFormat(TileSize.X, TileSize.Y);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
    Capture->TextureTarget = Target;
    Capture->ProjectionType = ECameraProjectionMode::Orthographic;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->ShowOnlyComponents.Add(Component);
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Scene.AddComponent(Capture, FTransform::Identity);

    TArray<TArray<FColor>> LocalTiles;
    for (int32 Plane = 0; Plane < 4; ++Plane)
    {
        const double Angle = UE_PI * double(Plane) / 4.0;
        const FVector Facing(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
        const FVector Along(Facing.Y, -Facing.X, 0.0);
        const double ProjectedHalfWidth = FMath::Abs(Along.X) * Extent.X + FMath::Abs(Along.Y) * Extent.Y;
        const double Aspect = double(TileSize.X) / TileSize.Y;
        Capture->OrthoWidth = float(2.1 * FMath::Max(ProjectedHalfWidth, Extent.Z * Aspect));
        const double Distance = Extent.Size() * 3.0 + 100.0;
        const FVector Location = Center + Facing * Distance;
        Capture->SetWorldLocationAndRotation(Location, (Center - Location).Rotation());
        FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
        if (!Resource)
        {
            Error = TEXT("The scene capture render target resource is unavailable.");
            return false;
        }
        TArray<FFloat16Color> BaseColors;
        TArray<FFloat16Color> Opacity;
        Capture->CaptureSource = SCS_BaseColor;
        Capture->CaptureScene();
        FlushRenderingCommands();
        const bool bReadBase = Resource->ReadFloat16Pixels(BaseColors);
        Capture->CaptureSource = SCS_SceneColorHDR;
        Capture->CaptureScene();
        FlushRenderingCommands();
        const bool bReadOpacity = Resource->ReadFloat16Pixels(Opacity);
        if (!bReadBase || !bReadOpacity || BaseColors.Num() != TileSize.X * TileSize.Y || Opacity.Num() != BaseColors.Num())
        {
            Error = TEXT("Could not read base color and opacity from the scene capture render target.");
            return false;
        }
        TArray<FColor>& Tile = LocalTiles.AddDefaulted_GetRef();
        Tile.Reserve(BaseColors.Num());
        for (int32 PixelIndex = 0; PixelIndex < BaseColors.Num(); ++PixelIndex)
        {
            FLinearColor Linear(BaseColors[PixelIndex]);
            // SCS_SceneColorHDR stores inverse opacity in alpha.
            Linear.A = 1.0f - FMath::Clamp(float(Opacity[PixelIndex].A), 0.0f, 1.0f);
            Tile.Add(Linear.ToFColorSRGB());
        }
    }
    Tiles = MoveTemp(LocalTiles);
    return true;
}
