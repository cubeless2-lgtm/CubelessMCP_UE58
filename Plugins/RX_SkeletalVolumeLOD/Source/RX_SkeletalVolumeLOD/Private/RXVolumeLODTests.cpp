#include "RXVolumeLOD.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "PreviewScene.h"
#include "RHI.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "RenderingThread.h"
#include "Serialization/MemoryWriter.h"
#include "SkinnedAssetCompiler.h"
#include "TextureResource.h"
#include "UObject/SavePackage.h"

namespace
{
constexpr TCHAR FixturePath[] = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
constexpr TCHAR OwnedRoot[] = TEXT("/Game/_MCP_Temp/SkeletalVolumeLOD/CubelessTests/");

bool IsOwnedPath(const FString& Path)
{
    return Path.StartsWith(OwnedRoot, ESearchCase::CaseSensitive)
        && FPackageName::IsValidLongPackageName(Path)
        && FPackageName::GetLongPackagePath(Path).Len() > FCString::Strlen(OwnedRoot);
}

FString ObjectPath(const FString& PackagePath)
{
    return PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
}

FString ManifestPath(const FString& PackagePath)
{
    return FPaths::ProjectSavedDir() / TEXT("RX_SkeletalVolumeLOD") / (PackagePath.Mid(FCString::Strlen(OwnedRoot)).Replace(TEXT("/"), TEXT("_")) + TEXT(".txt"));
}

void FinishMesh(USkeletalMesh* Mesh)
{
    USkinnedAsset* Assets[] = { Mesh };
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
}

FString MeshDescriptionHash(USkeletalMesh* Mesh, int32 LODIndex)
{
    FMeshDescription Description;
    if (!Mesh->CloneMeshDescription(LODIndex, Description))
    {
        return FString();
    }
    TArray<uint8> Bytes;
    FMemoryWriter Archive(Bytes);
    Description.Serialize(Archive);
    return FMD5::HashBytes(Bytes.GetData(), Bytes.Num());
}

FString BaseColorHash(FAutomationTestBase& Test, UTexture2D* Texture, bool bKnownTexturedFixture, const FString& ImagePath = FString())
{
    TArray64<uint8> Bytes;
    if (!Test.TestTrue(TEXT("Read the baked Base Color source image"), Texture && Texture->Source.GetMipData(Bytes, 0))
        || !Test.TestEqual(TEXT("Base Color source pixel format"), Texture->Source.GetFormat(), TSF_BGRA8))
    {
        return FString();
    }

    Test.TestTrue(TEXT("Base Color texture has sRGB enabled"), Texture->SRGB);
    const int64 PixelCount = Texture->Source.GetSizeX() * Texture->Source.GetSizeY();
    if (!Test.TestTrue(TEXT("Base Color byte count matches its dimensions"), PixelCount > 0 && Bytes.Num() == PixelCount * 4))
    {
        return FString();
    }

    int64 NonBlackPixels = 0;
    TSet<uint32> QuantizedColors;
    for (int64 Pixel = 0; Pixel < PixelCount; ++Pixel)
    {
        const uint8* BGRA = Bytes.GetData() + Pixel * 4;
        if (FMath::Max3(BGRA[0], BGRA[1], BGRA[2]) > 12)
        {
            ++NonBlackPixels;
            QuantizedColors.Add((static_cast<uint32>(BGRA[2] >> 4) << 8) | (static_cast<uint32>(BGRA[1] >> 4) << 4) | (BGRA[0] >> 4));
        }
    }
    Test.AddInfo(FString::Printf(TEXT("Base Color: %lld non-black texels, %d distinct 12-bit RGB values"), NonBlackPixels, QuantizedColors.Num()));
    if (bKnownTexturedFixture)
    {
        Test.TestTrue(TEXT("The textured mannequin atlas is neither blank nor nearly empty"), NonBlackPixels > PixelCount / 10);
        Test.TestTrue(TEXT("The textured mannequin atlas retains substantial color variation"), QuantizedColors.Num() >= 8);
    }
    if (!ImagePath.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ImagePath), true);
        const TArrayView64<const FColor> Colors(reinterpret_cast<const FColor*>(Bytes.GetData()), PixelCount);
        TArray64<uint8> PNG;
        FImageUtils::PNGCompressImageArray(static_cast<int32>(Texture->Source.GetSizeX()),
            static_cast<int32>(Texture->Source.GetSizeY()), Colors, PNG);
        Test.TestTrue(TEXT("Export the observed Base Color atlas PNG"), !PNG.IsEmpty() && FFileHelper::SaveArrayToFile(PNG, *ImagePath));
    }
    return FMD5::HashBytes(Bytes.GetData(), Bytes.Num());
}

void CheckOpacityMask(FAutomationTestBase& Test, const USkeletalMesh& Mesh, const UTexture2D* BaseColor,
    const UMaterial* Material, const UTexture2D* ExpectedMask, bool bRequireMask)
{
    const bool bMasked = Material && Material->GetBlendMode() == BLEND_Masked;
    if (!bRequireMask && !ExpectedMask && !bMasked)
    {
        return;
    }
    if (!Test.TestTrue(TEXT("LOD1 uses a masked material when an opacity mask is required"), bMasked))
    {
        return;
    }

    const FExpressionInput& Opacity = Material->GetEditorOnlyData()->OpacityMask;
    const auto* Sample = Cast<UMaterialExpressionTextureSample>(Opacity.Expression);
    UTexture2D* Texture = Sample ? Cast<UTexture2D>(Sample->Texture.Get()) : nullptr;
    if (!Test.TestTrue(TEXT("LOD1 connects the generated mask red channel on UV0"), Texture && Texture != BaseColor
            && (!ExpectedMask || Texture == ExpectedMask) && Opacity.OutputIndex == 1
            && Sample->ConstCoordinate == 0 && !Sample->Coordinates.IsConnected() && Sample->SamplerType == SAMPLERTYPE_Masks))
    {
        return;
    }
    Test.TestTrue(TEXT("LOD1 opacity mask is owned by the generated asset folder"),
        FPackageName::GetLongPackagePath(Texture->GetOutermost()->GetName()) == FPackageName::GetLongPackagePath(Mesh.GetOutermost()->GetName()));
    Test.TestTrue(TEXT("LOD1 opacity mask uses linear mask compression"), !Texture->SRGB && Texture->CompressionSettings == TC_Masks);

    TArray64<uint8> Pixels;
    const int64 Width = Texture->Source.GetSizeX();
    const int64 Height = Texture->Source.GetSizeY();
    if (!Test.TestTrue(TEXT("LOD1 opacity mask is a readable BGRA8 atlas matching Base Color dimensions"), BaseColor
            && Width == BaseColor->Source.GetSizeX() && Height == BaseColor->Source.GetSizeY()
            && Width > 0 && Height > 0 && Texture->Source.GetFormat() == TSF_BGRA8
            && Texture->Source.GetMipData(Pixels, 0) && Pixels.Num() == Width * Height * 4))
    {
        return;
    }
    const float Clip = Material->GetOpacityMaskClipValue();
    Test.TestTrue(TEXT("LOD1 opacity mask has a valid clip threshold"), FMath::IsFinite(Clip) && Clip >= 0.0f && Clip <= 1.0f);
    int64 VisiblePixels = 0;
    for (int64 Index = 2; Index < Pixels.Num(); Index += 4)
    {
        VisiblePixels += Pixels[Index] / 255.0f >= Clip;
    }
    Test.TestTrue(TEXT("LOD1 opacity mask contains visible texels"), VisiblePixels > 0);
    Test.AddInfo(FString::Printf(TEXT("LOD1 Opacity Mask: %s, clip %.4f, %lld/%lld atlas texels at or above clip"),
        *Texture->GetPathName(), Clip, VisiblePixels, Width * Height));
}

int32 CheckLOD1(FAutomationTestBase& Test, USkeletalMesh* Mesh, UTexture2D* BaseColor, TArray<UMaterialInterface*>& Materials,
    bool bRequireOpacityMask, const UTexture2D* ExpectedOpacityMask = nullptr)
{
    const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
    const FSkeletalMeshLODInfo* Info = Mesh->GetLODInfo(1);
    if (!Test.TestTrue(TEXT("LOD1 has authored and built render geometry"), Info && Mesh->GetMeshDescription(1)
            && RenderData && RenderData->LODRenderData.IsValidIndex(1)))
    {
        return 0;
    }

    const FSkeletalMeshLODRenderData& LOD = RenderData->LODRenderData[1];
    const FSkinWeightVertexBuffer* Weights = LOD.GetSkinWeightVertexBuffer();
    if (!Test.TestTrue(TEXT("LOD1 has triangles, UVs and a skin-weight buffer"), LOD.GetTotalFaces() > 0
            && LOD.GetNumTexCoords() > 0 && Weights && Weights->GetNumVertices() == LOD.GetNumVertices()))
    {
        return 0;
    }

    const int32 BoneCount = Mesh->GetRefSkeleton().GetNum();
    TSet<int32> WeightedBones;
    int32 DeformingBone = INDEX_NONE;
    uint32 CheckedVertices = 0;
    uint32 InvalidWeights = 0;
    for (int32 SectionIndex = 0; SectionIndex < LOD.RenderSections.Num(); ++SectionIndex)
    {
        const FSkelMeshRenderSection& Section = LOD.RenderSections[SectionIndex];
        int32 MaterialIndex = Section.MaterialIndex;
        if (Info->LODMaterialMap.IsValidIndex(SectionIndex) && Mesh->IsValidMaterialIndex(Info->LODMaterialMap[SectionIndex]))
        {
            MaterialIndex = Info->LODMaterialMap[SectionIndex];
        }
        if (Mesh->GetMaterials().IsValidIndex(MaterialIndex))
        {
            UMaterialInterface* Material = Mesh->GetMaterials()[MaterialIndex].MaterialInterface;
            if (Test.TestNotNull(TEXT("LOD1 section has a material"), Material))
            {
                Materials.AddUnique(Material);
            }
        }
        else
        {
            Test.AddError(TEXT("LOD1 section resolves to an invalid material slot"));
        }

        for (uint32 Vertex = Section.BaseVertexIndex; Vertex < Section.BaseVertexIndex + Section.NumVertices; ++Vertex)
        {
            if (Vertex >= Weights->GetNumVertices())
            {
                ++InvalidWeights;
                break;
            }
            ++CheckedVertices;
            uint32 WeightSum = 0;
            for (uint32 Influence = 0; Influence < Weights->GetMaxBoneInfluences(); ++Influence)
            {
                const uint16 Weight = Weights->GetBoneWeight(Vertex, Influence);
                if (Weight == 0)
                {
                    continue;
                }
                WeightSum += Weight;
                const uint32 LocalBone = Weights->GetBoneIndex(Vertex, Influence);
                if (!Section.BoneMap.IsValidIndex(LocalBone) || Section.BoneMap[LocalBone] >= BoneCount)
                {
                    ++InvalidWeights;
                    continue;
                }
                const int32 Bone = Section.BoneMap[LocalBone];
                WeightedBones.Add(Bone);
                if (Bone > 0 && Weight >= 16384)
                {
                    DeformingBone = Bone;
                }
            }
            if (FMath::Abs(static_cast<int32>(WeightSum) - 65535) > 257)
            {
                ++InvalidWeights;
            }
        }
    }

    Test.TestTrue(TEXT("A generated LOD1 material exists"), !Materials.IsEmpty());
    for (UMaterialInterface* Material : Materials)
    {
        const UMaterial* BakedMaterial = Cast<UMaterial>(Material);
        const UMaterialEditorOnlyData* Inputs = BakedMaterial ? BakedMaterial->GetEditorOnlyData() : nullptr;
        const UMaterialExpressionTextureSample* BaseColorSample = Inputs
            ? Cast<UMaterialExpressionTextureSample>(Inputs->BaseColor.Expression) : nullptr;
        Test.TestTrue(TEXT("LOD1 material samples the generated Base Color atlas on UV0"), BaseColorSample
            && BaseColorSample->Texture == BaseColor && BaseColorSample->ConstCoordinate == 0 && !BaseColorSample->Coordinates.IsConnected());
        Test.TestTrue(TEXT("LOD1 material is enabled for skeletal meshes"), BakedMaterial && BakedMaterial->GetUsageByFlag(MATUSAGE_SkeletalMesh));
        CheckOpacityMask(Test, *Mesh, BaseColor, BakedMaterial, ExpectedOpacityMask, bRequireOpacityMask);
    }

    Test.TestEqual(TEXT("All rendered LOD1 vertices have validated weights"), CheckedVertices, LOD.GetNumVertices());
    Test.TestEqual(TEXT("LOD1 weights are normalized and target valid section bones"), InvalidWeights, 0u);
    Test.TestTrue(TEXT("LOD1 retains multiple driven bones"), WeightedBones.Num() > 1 && DeformingBone != INDEX_NONE);
    Test.AddInfo(FString::Printf(TEXT("LOD1: %d faces, %u vertices, %d weighted bones"), LOD.GetTotalFaces(), CheckedVertices, WeightedBones.Num()));

    if (InvalidWeights == 0 && DeformingBone != INDEX_NONE)
    {
        USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>();
        Component->SetSkeletalMeshAsset(Mesh);
        Component->SetForcedLOD(2);
        TArray<FMatrix44f> BoneMatrices;
        BoneMatrices.Init(FMatrix44f::Identity, BoneCount);
        TArray<FVector3f> BindPositions;
        USkinnedMeshComponent::ComputeSkinnedPositions(Component, BindPositions, BoneMatrices, LOD, *Weights);
        BoneMatrices[DeformingBone].SetOrigin(FVector3f(0, 0, 15));
        TArray<FVector3f> PosedPositions;
        USkinnedMeshComponent::ComputeSkinnedPositions(Component, PosedPositions, BoneMatrices, LOD, *Weights);
        if (Test.TestTrue(TEXT("Engine CPU skinning returns the LOD1 vertex count"), BindPositions.Num() == CheckedVertices
                && PosedPositions.Num() == CheckedVertices))
        {
            int32 Moved = 0;
            for (int32 Index = 0; Index < BindPositions.Num(); ++Index)
            {
                Moved += FVector3f::DistSquared(BindPositions[Index], PosedPositions[Index]) > 1.0f;
            }
            Test.TestTrue(TEXT("Moving a non-root bone deforms part of LOD1"), Moved > 0 && Moved < BindPositions.Num());
            Test.AddInfo(FString::Printf(TEXT("Engine CPU skinning moved %d vertices on one non-root bone"), Moved));
        }
    }
    return LOD.GetTotalFaces();
}

bool SaveAsset(FAutomationTestBase& Test, UObject* Asset, const FString& RunPath)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    if (!Test.TestTrue(TEXT("Save only an asset owned by this verification run"), Package
            && Package->GetName().StartsWith(RunPath + TEXT("/"), ESearchCase::CaseSensitive)))
    {
        return false;
    }
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    return Test.TestTrue(FString::Printf(TEXT("Save %s"), *Asset->GetPathName()), UPackage::SavePackage(Package, Asset, *Filename, Args));
}

bool ExportLODComparison(USkeletalMesh* Mesh, const FString& Path)
{
    constexpr int32 TileSize = 512;
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    if (GUsingNullRHI || Bounds.BoxExtent.ContainsNaN() || Bounds.SphereRadius <= UE_SMALL_NUMBER)
    {
        return false;
    }

    for (const FSkeletalMaterial& Slot : Mesh->GetMaterials())
    {
        if (Slot.MaterialInterface)
        {
            if (FMaterialResource* Resource = Slot.MaterialInterface->GetMaterialResource(GMaxRHIShaderPlatform))
            {
                Resource->FinishCompilation();
            }
        }
    }

    ON_SCOPE_EXIT { FlushRenderingCommands(); };
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false)
        .SetCreatePhysicsScene(false).SetTransactional(false));
    USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(GetTransientPackage());
    Component->SetDisablePostProcessBlueprint(true);
    Component->SetSkeletalMeshAsset(Mesh);
    Scene.AddComponent(Component, FTransform::Identity);

    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
    Target->RenderTargetFormat = RTF_RGBA16f;
    Target->ClearColor = FLinearColor::Black;
    Target->InitAutoFormat(TileSize, TileSize);
    Target->UpdateResourceImmediate(true);

    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(GetTransientPackage());
    Capture->TextureTarget = Target;
    Capture->CaptureSource = SCS_BaseColor;
    Capture->ProjectionType = ECameraProjectionMode::Orthographic;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->ShowOnlyComponent(Component);
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->OrthoWidth = static_cast<float>(2.3 * Bounds.BoxExtent.GetMax());
    const FVector CameraLocation = Bounds.Origin + FVector(1.0, 0.2, 0.0).GetSafeNormal() * (Bounds.SphereRadius * 3.0 + 100.0);
    Scene.AddComponent(Capture, FTransform((Bounds.Origin - CameraLocation).Rotation(), CameraLocation));

    FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return false;
    }
    TArray64<FColor> Tiles;
    Tiles.SetNumUninitialized(TileSize * TileSize * 2);
    for (int32 LOD = 0; LOD < 2; ++LOD)
    {
        Component->SetForcedLOD(LOD + 1);
        Component->UpdateLODStatus();
        Component->RefreshBoneTransforms();
        Component->MarkRenderDynamicDataDirty();
        if (Component->GetPredictedLODLevel() != LOD)
        {
            return false;
        }
        Capture->CaptureScene();
        FlushRenderingCommands();
        TArray<FFloat16Color> Pixels;
        if (!Resource->ReadFloat16Pixels(Pixels) || Pixels.Num() != TileSize * TileSize)
        {
            return false;
        }

        int32 Foreground = 0;
        for (int32 Y = 0; Y < TileSize; ++Y)
        {
            for (int32 X = 0; X < TileSize; ++X)
            {
                const FColor Color = FLinearColor(Pixels[Y * TileSize + X]).ToFColorSRGB();
                Foreground += FMath::Max3(Color.R, Color.G, Color.B) > 12;
                Tiles[Y * 2 * TileSize + LOD * TileSize + X] = FColor(Color.R, Color.G, Color.B);
            }
        }
        if (Foreground < TileSize * TileSize / 100)
        {
            return false;
        }
    }

    TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(2 * TileSize, TileSize, Tiles, PNG);
    return !PNG.IsEmpty() && FFileHelper::SaveArrayToFile(PNG, *Path);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXVolumeLODGenerateTest, "RX.SkeletalVolumeLOD.Generate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXVolumeLODGenerateTest::RunTest(const FString& Parameters)
{
    FString PackagePath;
    const bool bPersist = FParse::Value(FCommandLine::Get(), TEXT("RXVolumeLODTestPath="), PackagePath);
    if (!bPersist)
    {
        PackagePath = FString(OwnedRoot) + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("/SK_Test");
    }
    if (!TestTrue(TEXT("Use a new package in the owned test root"), IsOwnedPath(PackagePath)
            && !FPackageName::DoesPackageExist(PackagePath) && !FindPackage(nullptr, *PackagePath)))
    {
        return false;
    }

    FString SourcePath;
    if (!FParse::Value(FCommandLine::Get(), TEXT("RXVolumeLODSourcePath="), SourcePath))
    {
        SourcePath = FixturePath;
    }
    if (!TestTrue(TEXT("The source is a full SkeletalMesh object path"), SourcePath.Contains(TEXT("."))
            && !SourcePath.Contains(TEXT(":")) && FPackageName::IsValidObjectPath(SourcePath)))
    {
        return false;
    }
    const bool bKnownTexturedFixture = SourcePath.Equals(FixturePath, ESearchCase::IgnoreCase);
    RXVolumeLOD::FOptions Options;
    FString TriangleBudget;
    if (FParse::Value(FCommandLine::Get(), TEXT("RXVolumeLODTargetTriangles="), TriangleBudget))
    {
        TCHAR* End = nullptr;
        const int64 Budget = FCString::Strtoi64(*TriangleBudget, &End, 10);
        if (!TestTrue(TEXT("The requested triangle budget is an integer from 128 to 100000"),
                End && End != *TriangleBudget && *End == TEXT('\0') && Budget >= 128 && Budget <= 100000))
        {
            return false;
        }
        Options.TargetTriangleCount = static_cast<int32>(Budget);
    }
    USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, *SourcePath);
    if (!TestNotNull(FString::Printf(TEXT("Load source skeletal mesh %s"), *SourcePath), Source))
    {
        return false;
    }
    FinishMesh(Source);
    const int32 SourceLODCount = Source->GetLODNum();
    const bool bSourceWasDirty = Source->GetOutermost()->IsDirty();
    const FString SourceLOD0Hash = MeshDescriptionHash(Source, 0);
    if (!TestTrue(TEXT("Source mesh has an authored LOD0"), !SourceLOD0Hash.IsEmpty()))
    {
        return false;
    }

    RXVolumeLOD::FResult Result;
    FText Error;
    if (!RXVolumeLOD::GenerateLOD1(Source, PackagePath, Options, Result, Error))
    {
        AddError(FString::Printf(TEXT("Volume LOD1 generation failed: %s"), *Error.ToString()));
        return false;
    }
    if (!TestTrue(TEXT("Volume generation creates a separate mesh"), Result.Mesh && Result.Mesh != Source))
    {
        return false;
    }
    FinishMesh(Result.Mesh);
    TestEqual(TEXT("Source LOD count remains unchanged"), Source->GetLODNum(), SourceLODCount);
    TestEqual(TEXT("Source package dirty state remains unchanged"), Source->GetOutermost()->IsDirty(), bSourceWasDirty);
    TestEqual(TEXT("Source LOD0 mesh description remains unchanged"), MeshDescriptionHash(Source, 0), SourceLOD0Hash);
    TestEqual(TEXT("Output LOD0 retains source geometry, UVs and weights"), MeshDescriptionHash(Result.Mesh, 0), SourceLOD0Hash);
    TestEqual(TEXT("Output preserves the source skeleton"), Result.Mesh->GetSkeleton(), Source->GetSkeleton());
    const FMeshDescription* OutputLOD1 = Result.Mesh->GetMeshDescription(1);
    TestTrue(TEXT("LOD1 geometry differs from LOD0"), OutputLOD1 && MeshDescriptionHash(Result.Mesh, 1) != SourceLOD0Hash);
    TestTrue(TEXT("LOD1 has fewer triangles than LOD0"), Result.TargetTriangles > 0 && Result.TargetTriangles < Result.SourceTriangles);
    TestTrue(TEXT("At least 90 percent of covered texels matched the source"), Result.CoveredTexels > 0
        && 10ll * Result.MatchedTexels >= 9ll * Result.CoveredTexels && Result.MatchedTexels <= Result.CoveredTexels);
    AddInfo(FString::Printf(TEXT("LOD bake: %d -> %d triangles, %d/%d matched texels, output %s"), Result.SourceTriangles,
        Result.TargetTriangles, Result.MatchedTexels, Result.CoveredTexels, *Result.Mesh->GetPathName()));

    const FString AtlasImage = bPersist ? FPaths::GetBaseFilename(ManifestPath(PackagePath), false) + TEXT("_BaseColor.png") : FString();
    const FString AtlasHash = BaseColorHash(*this, Result.BaseColor, bKnownTexturedFixture, AtlasImage);
    TestTrue(TEXT("Base Color is baked at the requested size"), Result.BaseColor
        && Result.BaseColor->Source.GetSizeX() == Options.TextureSize && Result.BaseColor->Source.GetSizeY() == Options.TextureSize);
    TArray<UMaterialInterface*> Materials;
    const bool bRequireOpacityMask = FParse::Param(FCommandLine::Get(), TEXT("RXVolumeLODRequireOpacityMask"));
    TestTrue(TEXT("Generation reports its opacity mask when required"), !bRequireOpacityMask || Result.OpacityMask);
    const int32 RenderFaces = CheckLOD1(*this, Result.Mesh, Result.BaseColor, Materials, bRequireOpacityMask, Result.OpacityMask);
    TestEqual(TEXT("LOD1 built face count matches generated geometry"), RenderFaces, Result.TargetTriangles);
    TestTrue(FString::Printf(TEXT("LOD1 render triangles obey budget (%d <= %d)"), RenderFaces, Options.TargetTriangleCount),
        RenderFaces > 0 && RenderFaces <= Options.TargetTriangleCount);
    AddInfo(FString::Printf(TEXT("LOD1 render triangles: %d of %d for %s"), RenderFaces, Options.TargetTriangleCount, *SourcePath));

    if (bPersist && !HasAnyErrors())
    {
        const FString RunPath = FPackageName::GetLongPackagePath(PackagePath);
        TArray<UObject*> ToSave = { Result.BaseColor, Result.Normal, Result.Roughness, Result.Metallic, Result.OpacityMask };
        for (UMaterialInterface* Material : Materials)
        {
            ToSave.AddUnique(Material);
        }
        ToSave.AddUnique(Result.Mesh);
        TSet<UPackage*> Saved;
        for (UObject* Asset : ToSave)
        {
            if (Asset && !Saved.Contains(Asset->GetOutermost()))
            {
                if (!SaveAsset(*this, Asset, RunPath))
                {
                    return false;
                }
                Saved.Add(Asset->GetOutermost());
            }
        }
        const FString Manifest = ManifestPath(PackagePath);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Manifest), true);
        TArray<FString> Lines = { Result.Mesh->GetPathName(), Result.BaseColor->GetPathName(), AtlasHash,
            SourceLOD0Hash, FString::FromInt(RenderFaces) };
        if (!bKnownTexturedFixture)
        {
            Lines.Add(SourcePath);
        }
        TestTrue(TEXT("Write the persistence fingerprint"), FFileHelper::SaveStringArrayToFile(Lines, *Manifest));
        const FString Comparison = FPaths::GetBaseFilename(Manifest, false) + TEXT("_LOD0_LOD1_BaseColor.png");
        if (ExportLODComparison(Result.Mesh, Comparison))
        {
            AddInfo(FString::Printf(TEXT("Base Color comparison, LOD0 left and LOD1 right: %s"), *Comparison));
        }
        else
        {
            AddWarning(TEXT("Could not export a valid forced-LOD Base Color comparison. The atlas and numeric checks are reported separately."));
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXVolumeLODReloadTest, "RX.SkeletalVolumeLODPersistence.Reload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXVolumeLODReloadTest::RunTest(const FString& Parameters)
{
    FString PackagePath;
    if (!TestTrue(TEXT("Provide -RXVolumeLODTestPath with the saved test package"),
            FParse::Value(FCommandLine::Get(), TEXT("RXVolumeLODTestPath="), PackagePath) && IsOwnedPath(PackagePath)))
    {
        return false;
    }
    TArray<FString> Lines;
    if (!TestTrue(TEXT("Read the matching verification run manifest"),
            FFileHelper::LoadFileToStringArray(Lines, *ManifestPath(PackagePath))
            && (Lines.Num() == 5 || (Lines.Num() == 6 && FPackageName::IsValidObjectPath(Lines[5])))
            && Lines[0] == ObjectPath(PackagePath) && Lines[1].StartsWith(FPackageName::GetLongPackagePath(PackagePath) + TEXT("/"))))
    {
        return false;
    }
    if (!TestTrue(TEXT("The mesh starts unloaded and is opened from disk"), !FindPackage(nullptr, *PackagePath)))
    {
        return false;
    }
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Lines[0]);
    UTexture2D* BaseColor = LoadObject<UTexture2D>(nullptr, *Lines[1]);
    if (!TestNotNull(TEXT("Load the saved skeletal mesh"), Mesh) || !TestNotNull(TEXT("Load the saved atlas"), BaseColor))
    {
        return false;
    }
    FinishMesh(Mesh);
    TestEqual(TEXT("LOD0 stays byte-identical after reload"), MeshDescriptionHash(Mesh, 0), Lines[3]);
    TestEqual(TEXT("Atlas source pixels stay byte-identical after reload"), BaseColorHash(*this, BaseColor, Lines.Num() == 5), Lines[2]);
    TArray<UMaterialInterface*> Materials;
    const bool bRequireOpacityMask = FParse::Param(FCommandLine::Get(), TEXT("RXVolumeLODRequireOpacityMask"));
    TestEqual(TEXT("LOD1 stays renderable and skinned after reload"),
        CheckLOD1(*this, Mesh, BaseColor, Materials, bRequireOpacityMask), FCString::Atoi(*Lines[4]));
    return !HasAnyErrors();
}

#endif
