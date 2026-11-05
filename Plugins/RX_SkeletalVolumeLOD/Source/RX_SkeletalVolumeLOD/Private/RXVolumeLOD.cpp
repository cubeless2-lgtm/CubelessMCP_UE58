#include "RXVolumeLOD.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Animation/MorphTarget.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/PackageName.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "RXVolumeMesh.h"
#include "RXVolumeTextureBake.h"
#include "SkeletalMeshAttributes.h"
#include "SkeletalMeshTypes.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "RXVolumeLOD"

namespace RXVolumeLOD
{
namespace
{
constexpr int32 TargetLOD = 1;

bool ReadMaterialAssignments(const USkeletalMesh& Source, const FMeshDescription& Description,
    TArray<UMaterialInterface*>& OutMaterials, TArray<int32>& OutGroupSlots, FText& Error)
{
    const TArray<FSkeletalMaterial>& Slots = Source.GetMaterials();
    const FSkeletalMeshLODModel& SourceModel = Source.GetImportedModel()->LODModels[0];
    const TArray<int32>& MaterialOverrides = Source.GetLODInfo(0)->LODMaterialMap;
    for (const FSkeletalMaterial& Slot : Slots)
    {
        OutMaterials.Add(Slot.MaterialInterface);
    }

    const FSkeletalMeshConstAttributes Attributes(Description);
    const auto GroupNames = Attributes.GetPolygonGroupMaterialSlotNames();
    OutGroupSlots.Init(INDEX_NONE, Description.PolygonGroups().GetArraySize());
    for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
    {
        if (Description.GetPolygonGroupPolygonIDs(Group).IsEmpty())
        {
            continue;
        }

        const FName Name = GroupNames[Group];
        int32 SlotIndex = Name.IsNone() ? INDEX_NONE : Slots.IndexOfByPredicate([Name](const FSkeletalMaterial& Slot)
        {
            return Slot.ImportedMaterialSlotName == Name;
        });
        if (SlotIndex == INDEX_NONE && !Name.IsNone())
        {
            SlotIndex = Slots.IndexOfByPredicate([Name](const FSkeletalMaterial& Slot)
            {
                return Slot.MaterialSlotName == Name;
            });
        }
        if (SlotIndex == INDEX_NONE)
        {
            const FSkelMeshSection* Section = SourceModel.Sections.FindByPredicate([Group](const FSkelMeshSection& Item)
            {
                return Item.OriginalDataSectionIndex == Group.GetValue();
            });
            if (Section)
            {
                SlotIndex = Section->MaterialIndex;
            }
        }

        int32 EffectiveSlot = INDEX_NONE;
        for (int32 Index = 0; Index < SourceModel.Sections.Num(); ++Index)
        {
            if (SourceModel.Sections[Index].MaterialIndex != SlotIndex)
            {
                continue;
            }
            const int32 Override = MaterialOverrides.IsValidIndex(Index) ? MaterialOverrides[Index] : INDEX_NONE;
            const int32 SectionSlot = Slots.IsValidIndex(Override) ? Override : SlotIndex;
            if (EffectiveSlot != INDEX_NONE && EffectiveSlot != SectionSlot)
            {
                Error = FText::Format(LOCTEXT("MixedOverride", "LOD0 sections sharing material {0} use different overrides."),
                    FText::FromName(Name));
                return false;
            }
            EffectiveSlot = SectionSlot;
        }
        if (EffectiveSlot == INDEX_NONE)
        {
            EffectiveSlot = SlotIndex;
        }
        if (!OutMaterials.IsValidIndex(EffectiveSlot) || !OutMaterials[EffectiveSlot])
        {
            Error = FText::Format(LOCTEXT("MissingSourceMaterial", "No source material was found for LOD0 group {0}."),
                FText::FromName(Name));
            return false;
        }
        OutGroupSlots[Group.GetValue()] = EffectiveSlot;
    }
    return true;
}

UTexture2D* CreateTexture(const TArray<FColor>& Pixels, int32 Size, TextureCompressionSettings Compression, bool bSRGB)
{
    UTexture2D* Texture = NewObject<UTexture2D>(GetTransientPackage());
    Texture->Source.Init(Size, Size, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
    Texture->SRGB = bSRGB;
    Texture->CompressionSettings = Compression;
    Texture->CompressionNoAlpha = true;
    Texture->LODGroup = TEXTUREGROUP_Character;
    Texture->PostEditChange();
    return Texture;
}

void ConnectTexture(UMaterial& Material, FExpressionInput& Input, UTexture2D* Texture, EMaterialSamplerType Sampler,
    int32 PositionY, int32 OutputIndex = 0)
{
    UMaterialExpressionTextureSample* Sample = NewObject<UMaterialExpressionTextureSample>(&Material);
    Sample->Texture = Texture;
    Sample->SamplerType = Sampler;
    Sample->MaterialExpressionEditorX = -480;
    Sample->MaterialExpressionEditorY = PositionY;
    Material.GetExpressionCollection().AddExpression(Sample);
    Input.Connect(OutputIndex, Sample);
}

bool InstallLOD(USkeletalMesh& Mesh, FMeshDescription&& Description, UMaterial& Material, FText& Error)
{
    FAssetCompilingManager::Get().FinishCompilationForObjects({ &Mesh });
    for (const UMorphTarget* Morph : Mesh.GetMorphTargets())
    {
        if (Morph && Morph->HasDataForLOD(TargetLOD) && !Morph->IsIn(&Mesh))
        {
            Error = LOCTEXT("ExternalMorph", "LOD1 references a morph target owned by another asset.");
            return false;
        }
    }
    FName SlotName(TEXT("RX_VolumeLOD1"));
    for (int32 Suffix = 2; Mesh.GetMaterials().ContainsByPredicate([&SlotName](const FSkeletalMaterial& Slot)
        { return Slot.MaterialSlotName == SlotName || Slot.ImportedMaterialSlotName == SlotName; }); ++Suffix)
    {
        SlotName = FName(*FString::Printf(TEXT("RX_VolumeLOD1_%d"), Suffix));
    }
    const int32 SlotIndex = Mesh.GetMaterials().Num();
    {
        FScopedSkeletalMeshPostEditChange Edit(&Mesh);
        FSkeletalMeshModel* Imported = Mesh.GetImportedModel();
        if (Mesh.GetLODNum() == TargetLOD)
        {
            Mesh.AddSourceModel(false);
        }
        if (Imported->LODModels.Num() == TargetLOD)
        {
            Imported->LODModels.Add(new FSkeletalMeshLODModel());
        }
        else
        {
            const FSkeletalMeshLODModel EmptyLOD;
            FSkeletalMeshLODModel::CopyStructure(&Imported->LODModels[TargetLOD], &EmptyLOD);
        }
        for (UMorphTarget* Morph : Mesh.GetMorphTargets())
        {
            if (Morph && Morph->IsIn(&Mesh) && Morph->GetMorphLODModels().IsValidIndex(TargetLOD))
            {
                Morph->GetMorphLODModels()[TargetLOD].Reset();
            }
        }

        auto GroupNames = FSkeletalMeshAttributes(Description).GetPolygonGroupMaterialSlotNames();
        for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
        {
            GroupNames[Group] = SlotName;
        }
        Mesh.GetMaterials().Emplace(&Material, SlotName, SlotName);

        // An asset-level settings object reapplies its rules on load. Retain the copied per-LOD settings.
        if (Mesh.GetLODSettings())
        {
            Mesh.SetLODSettings(nullptr);
        }
        FSkeletalMeshLODInfo& Info = *Mesh.GetLODInfo(TargetLOD);
        Info.LODMaterialMap.Reset();
        Info.BonesToRemove.Reset();
        Info.BonesToPrioritize.Reset();
        Info.SectionsToPrioritize.Reset();
        Info.SourceImportFilename.Reset();
        Info.bImportWithBaseMesh = false;
        Info.bHasBeenSimplified = false;
        Info.ReductionSettings = FSkeletalMeshOptimizationSettings();
        Info.ReductionSettings.BaseLOD = TargetLOD;
        Info.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
        Info.ReductionSettings.NumOfVertPercentage = 1.0f;
        Info.ReductionSettings.MaxNumOfTriangles = MAX_uint32;
        Info.ReductionSettings.MaxNumOfTrianglesPercentage = MAX_uint32;
        Info.ReductionSettings.MaxNumOfVerts = MAX_uint32;
        Info.ReductionSettings.MaxNumOfVertsPercentage = MAX_uint32;
        Info.ReductionSettings.MaxDeviationPercentage = 0.0f;
        Info.BuildSettings.bRecomputeNormals = false;
        Info.BuildSettings.bRecomputeTangents = false;
        Info.BuildSettings.bUseMikkTSpace = true;
        if (!Mesh.CreateMeshDescription(TargetLOD, MoveTemp(Description)) || !Mesh.CommitMeshDescription(TargetLOD))
        {
            Error = LOCTEXT("LODCommitFailed", "The editor could not commit the new LOD1 mesh description.");
            return false;
        }
    }

    FAssetCompilingManager::Get().FinishCompilationForObjects({ &Mesh });
    const FSkeletalMeshRenderData* RenderData = Mesh.GetResourceForRendering();
    if (!RenderData || !RenderData->LODRenderData.IsValidIndex(TargetLOD))
    {
        Error = LOCTEXT("NoRenderLOD", "The editor did not build render data for LOD1.");
        return false;
    }
    const FSkeletalMeshLODRenderData& RenderLOD = RenderData->LODRenderData[TargetLOD];
    const bool bValidSections = !RenderLOD.RenderSections.IsEmpty() && Algo::AllOf(RenderLOD.RenderSections,
        [](const FSkelMeshRenderSection& Section) { return Section.NumTriangles > 0; });
    if (!bValidSections || RenderLOD.GetNumVertices() == 0)
    {
        Error = LOCTEXT("BadRenderLOD", "LOD1 has no usable vertices or did not bind the baked material.");
        return false;
    }
    // Imported LOD sections can use local slot indices even when their slot names match the asset.
    Mesh.GetLODInfo(TargetLOD)->LODMaterialMap.Init(SlotIndex, RenderLOD.RenderSections.Num());
    return true;
}

bool IsNewPackage(const FString& PackagePath, FText& Error)
{
    if (!FPackageName::IsValidLongPackageName(PackagePath, false, &Error) || !PackagePath.StartsWith(TEXT("/Game/")))
    {
        Error = LOCTEXT("BadDestination", "Choose a valid new asset package under /Game.");
        return false;
    }
    if (FindPackage(nullptr, *PackagePath) || FPackageName::DoesPackageExist(PackagePath))
    {
        Error = FText::Format(LOCTEXT("DestinationExists", "An asset package already exists at {0}."),
            FText::FromString(PackagePath));
        return false;
    }
    return true;
}

struct FOutputAsset
{
    FString Path;
    TStrongObjectPtr<UObject> Object;
};

bool PublishAssets(TArray<FOutputAsset>& Assets, FText& Error)
{
    constexpr ERenameFlags Flags = REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty;
    TArray<UPackage*> Packages;
    for (const FOutputAsset& Asset : Assets)
    {
        if (!IsNewPackage(Asset.Path, Error))
        {
            return false;
        }
    }
    for (const FOutputAsset& Asset : Assets)
    {
        UPackage* Package = CreatePackage(*Asset.Path);
        if (!Asset.Object->Rename(*FPackageName::GetShortName(Asset.Path), Package, Flags | REN_Test))
        {
            Error = LOCTEXT("RenameCheck", "The output asset names could not be reserved.");
            return false;
        }
        Packages.Add(Package);
    }
    for (int32 Index = 0; Index < Assets.Num(); ++Index)
    {
        if (!Assets[Index].Object->Rename(*FPackageName::GetShortName(Assets[Index].Path), Packages[Index], Flags))
        {
            for (int32 Previous = 0; Previous < Index; ++Previous)
            {
                UObject* Asset = Assets[Previous].Object.Get();
                const FName TransientName = MakeUniqueObjectName(GetTransientPackage(), Asset->GetClass());
                Asset->Rename(*TransientName.ToString(), GetTransientPackage(), Flags);
            }
            Error = LOCTEXT("RenameFailed", "The editor could not publish the staged assets.");
            return false;
        }
    }
    for (const FOutputAsset& Asset : Assets)
    {
        Asset.Object->ClearFlags(RF_Transient);
        Asset.Object->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
        FAssetRegistryModule::AssetCreated(Asset.Object.Get());
        Asset.Object->MarkPackageDirty();
    }
    return true;
}
}

bool GenerateLOD1(USkeletalMesh* Source, const FString& DestinationPackagePath, const FOptions& Options,
    FResult& Out, FText& Error)
{
    Out = {};
    Error = FText::GetEmpty();
    if (!IsInGameThread() || !IsValid(Source) || Source->GetLODNum() == 0 || !Source->GetSkeleton())
    {
        Error = LOCTEXT("BadSource", "Select a skeletal mesh with a skeleton and LOD0 in the editor.");
        return false;
    }
    if (GEditor && GEditor->PlayWorld)
    {
        Error = LOCTEXT("Playing", "End the play or simulation session before generating LOD1.");
        return false;
    }
    if (Options.VoxelCount < 32 || Options.VoxelCount > 256 || Options.TargetTriangleCount < 128
        || Options.TargetTriangleCount > 100000 || Options.TextureSize < 128 || Options.TextureSize > 2048
        || !FMath::IsPowerOfTwo(Options.TextureSize))
    {
        Error = LOCTEXT("BadSettings", "Use 32–256 voxels, 128–100000 target triangles, and a 128–2048 power-of-two atlas.");
        return false;
    }
    if (!IsNewPackage(DestinationPackagePath, Error))
    {
        return false;
    }

    FAssetCompilingManager::Get().FinishCompilationForObjects({ Source });
    const FSkeletalMeshModel* Imported = Source->GetImportedModel();
    FMeshDescription SourceDescription;
    if (!Imported || !Imported->LODModels.IsValidIndex(0) || !Source->CloneMeshDescription(0, SourceDescription)
        || SourceDescription.Triangles().Num() == 0)
    {
        Error = LOCTEXT("NoSourceDescription", "The source has no editable LOD0 mesh description.");
        return false;
    }
    if (Imported->LODModels.IsValidIndex(TargetLOD) && Imported->LODModels[TargetLOD].HasClothData())
    {
        Error = LOCTEXT("LOD1HasCloth", "LOD1 has bound clothing and requires a separate clothing transfer.");
        return false;
    }
    if (Algo::AnyOf(Imported->LODModels[0].Sections, [](const FSkelMeshSection& Section) { return Section.bDisabled; }))
    {
        Error = LOCTEXT("DisabledSourceSections", "LOD0 has disabled sections. Remove them from the source geometry before generating a volume LOD.");
        return false;
    }
    for (int32 Index = TargetLOD + 1; Index < Source->GetLODNum(); ++Index)
    {
        if (Source->GetLODInfo(Index)->ReductionSettings.BaseLOD == TargetLOD && Source->IsReductionActive(Index))
        {
            Error = FText::Format(LOCTEXT("DependentLOD", "LOD{0} is generated from LOD1. Choose a mesh without dependent generated LODs."),
                FText::AsNumber(Index));
            return false;
        }
    }

    TArray<UMaterialInterface*> SourceMaterials;
    TArray<int32> GroupSlots;
    if (!ReadMaterialAssignments(*Source, SourceDescription, SourceMaterials, GroupSlots, Error))
    {
        return false;
    }

    FScopedSlowTask Progress(3.f, LOCTEXT("Generating", "Generating volume-remeshed LOD1"));
    Progress.MakeDialog();
    Progress.EnterProgressFrame(1.f, LOCTEXT("Remeshing", "Remeshing and transferring skin weights"));
    UE::Geometry::FDynamicMesh3 TargetMesh;
    FMeshDescription LODDescription;
    const int32 SourceTriangleCount = SourceDescription.Triangles().Num();
    const int32 TriangleTarget = FMath::Min(Options.TargetTriangleCount, FMath::Max(4, SourceTriangleCount * 2 / 3));
    if (!BuildVolumeMesh(*Source, SourceDescription, Options.VoxelCount, TriangleTarget,
        TargetMesh, LODDescription, Error))
    {
        return false;
    }
    Progress.EnterProgressFrame(1.f, LOCTEXT("Baking", "Baking the new texture atlas"));
    FTextureBake Baked;
    if (!BakeTextures(SourceDescription, TargetMesh, SourceMaterials, GroupSlots, Options.TextureSize, Baked, Error))
    {
        return false;
    }
    const int32 PixelCount = Options.TextureSize * Options.TextureSize;
    const auto ValidOptionalTexture = [PixelCount](const TArray<FColor>& Pixels)
    {
        return Pixels.IsEmpty() || Pixels.Num() == PixelCount;
    };
    if (Baked.BaseColor.Num() != PixelCount || !ValidOptionalTexture(Baked.Normal)
        || !ValidOptionalTexture(Baked.Roughness) || !ValidOptionalTexture(Baked.Metallic)
        || (Baked.OpacityMaskClipValue.IsSet() ? Baked.OpacityMask.Num() != PixelCount : !Baked.OpacityMask.IsEmpty())
        || Baked.CoveredTexels <= 0 || Baked.MatchedTexels <= 0 || Baked.MatchedTexels > Baked.CoveredTexels)
    {
        Error = LOCTEXT("BadBake", "The baker did not return valid color data and coverage for the new atlas.");
        return false;
    }

    Progress.EnterProgressFrame(1.f, LOCTEXT("Installing", "Building the copied skeletal mesh and its material"));
    const FString Folder = FPackageName::GetLongPackagePath(DestinationPackagePath);
    const FString Name = FPackageName::GetShortName(DestinationPackagePath);
    TArray<FOutputAsset> Assets;
    const auto StageTexture = [&](const TArray<FColor>& Pixels, const TCHAR* Suffix,
        TextureCompressionSettings Compression, bool bSRGB) -> UTexture2D*
    {
        if (Pixels.IsEmpty())
        {
            return nullptr;
        }
        UTexture2D* Texture = CreateTexture(Pixels, Options.TextureSize, Compression, bSRGB);
        Assets.Add({ Folder / (TEXT("T_") + Name + Suffix), TStrongObjectPtr<UObject>(Texture) });
        return Texture;
    };
    FResult Result;
    Result.BaseColor = StageTexture(Baked.BaseColor, TEXT("_BaseColor"), TC_Default, true);
    Result.Normal = StageTexture(Baked.Normal, TEXT("_Normal"), TC_Normalmap, false);
    Result.Roughness = StageTexture(Baked.Roughness, TEXT("_Roughness"), TC_Masks, false);
    Result.Metallic = StageTexture(Baked.Metallic, TEXT("_Metallic"), TC_Masks, false);
    Result.OpacityMask = StageTexture(Baked.OpacityMask, TEXT("_OpacityMask"), TC_Masks, false);

    Result.Material = NewObject<UMaterial>(GetTransientPackage());
    Assets.Add({ Folder / (TEXT("M_") + Name), TStrongObjectPtr<UObject>(Result.Material) });
    UMaterialEditorOnlyData& Inputs = *Result.Material->GetEditorOnlyData();
    ConnectTexture(*Result.Material, Inputs.BaseColor, Result.BaseColor, SAMPLERTYPE_Color, 0);
    if (Result.Normal)
    {
        ConnectTexture(*Result.Material, Inputs.Normal, Result.Normal, SAMPLERTYPE_Normal, 280);
    }
    if (Result.Roughness)
    {
        ConnectTexture(*Result.Material, Inputs.Roughness, Result.Roughness, SAMPLERTYPE_Masks, 560, 1);
    }
    if (Result.Metallic)
    {
        ConnectTexture(*Result.Material, Inputs.Metallic, Result.Metallic, SAMPLERTYPE_Masks, 840, 1);
    }
    if (Result.OpacityMask)
    {
        Result.Material->BlendMode = BLEND_Masked;
        Result.Material->OpacityMaskClipValue = Baked.OpacityMaskClipValue.GetValue();
        ConnectTexture(*Result.Material, Inputs.OpacityMask, Result.OpacityMask, SAMPLERTYPE_Masks, 1120, 1);
    }
    Result.Material->SetUsageByFlag(MATUSAGE_SkeletalMesh, true);
    Result.Material->PostEditChange();

    const FName StagedMeshName = MakeUniqueObjectName(GetTransientPackage(), Source->GetClass(), Source->GetFName());
    Result.Mesh = DuplicateObject<USkeletalMesh>(Source, GetTransientPackage(), StagedMeshName);
    if (!Result.Mesh)
    {
        Error = LOCTEXT("DuplicateFailed", "The editor could not duplicate the skeletal mesh.");
        return false;
    }
    Result.Mesh->SetFlags(RF_Transient);
    Assets.Add({ DestinationPackagePath, TStrongObjectPtr<UObject>(Result.Mesh) });
    Result.SourceTriangles = SourceTriangleCount;
    Result.TargetTriangles = LODDescription.Triangles().Num();
    Result.CoveredTexels = Baked.CoveredTexels;
    Result.MatchedTexels = Baked.MatchedTexels;
    if (!InstallLOD(*Result.Mesh, MoveTemp(LODDescription), *Result.Material, Error) || !PublishAssets(Assets, Error))
    {
        return false;
    }
    Out = Result;
    return true;
}
}

#undef LOCTEXT_NAMESPACE
