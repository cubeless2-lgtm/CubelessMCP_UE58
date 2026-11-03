#include "RXImpostorPreview.h"
#include "RXImpostorDirections.h"

#include "RXImpostorAtlas.h"
#include "RXImpostorCapture.h"
#include "RXImpostorMesh.h"

bool RXImpostorPreview::Build(UStaticMesh* Source, FIntPoint TileSize, int32 Padding, int32 Subdivisions,
    FRXImpostorPreview& Output, FString& Error, int32 Directions)
{
    if (!RXImpostorDirections::IsValid(Directions) || !Source || !IsInGameThread() || Source->IsCompiling())
    {
        Error = TEXT("A ready source mesh and the game thread are required.");
        return false;
    }
    FMeshDescription Geometry;
    if (!RXImpostorMesh::Build(Source->GetBoundingBox(), Subdivisions, TileSize, Padding, Geometry, Error, Directions))
    {
        return false;
    }
    TArray<TArray<FColor>> Tiles;
    if (!RXImpostorCapture::Capture(Source, TileSize, Tiles, Error, Directions))
    {
        return false;
    }
    TArray<FColor> Pixels;
    FIntPoint AtlasSize;
    if (!RXImpostorAtlas::Pack(Tiles, TileSize, Padding, Pixels, AtlasSize, Error))
    {
        return false;
    }
    FRXImpostorPreview Result;
    Result.Directions = Directions;
    if (!RXImpostorMaterial::CreatePreview(Pixels, AtlasSize, Result.Material, Error))
    {
        return false;
    }
    Result.Mesh.Reset(NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient));
    Result.Mesh->GetStaticMaterials().Add(FStaticMaterial(Result.Material.Material.Get(), FName("Impostor")));
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bMarkPackageDirty = false;
    Params.bBuildSimpleCollision = false;
    Params.bCommitMeshDescription = false;
    Params.bFastBuild = true;
    const TArray<const FMeshDescription*> Descriptions = { &Geometry };
    if (!Result.Mesh->BuildFromMeshDescriptions(Descriptions, Params))
    {
        Error = TEXT("Could not build the transient impostor preview mesh.");
        return false;
    }
    Result.Geometry = MakeUnique<FMeshDescription>(MoveTemp(Geometry));
    Output = MoveTemp(Result);
    return true;
}
