#include "RXApplyLOD.h"

#include "Misc/SecureHash.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "RXWeightedPreview.h"
#include "RXWeightMetadata.h"
#include "RXImpostorAssets.h"
#include "RXImpostorPreview.h"
#include "StaticMeshAttributes.h"
#include "ScopedTransaction.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
    FString GetSourceId(UStaticMesh* Mesh)
    {
        if (!Mesh || !Mesh->IsSourceModelValid(0)) { return {}; }
        const FMeshDescriptionBulkData* Bulk = Mesh->GetSourceModel(0).GetMeshDescriptionBulkData();
        return Bulk ? Bulk->GetUnversionedIdString() : FString();
    }

    struct FStagedLOD
    {
        FMeshBuildSettings Build;
        FMeshReductionSettings Reduction;
        FPerPlatformFloat ScreenSize;
        FString ImportFilename;
        bool bImportWithBase = false;
        TUniquePtr<FMeshDescription> Description;
    };
}

bool RXApplyLOD::Fingerprint(UStaticMesh* Mesh, FString& OutHash, FString& Error)
{
    if (!IsInGameThread() || !Mesh || Mesh->IsCompiling()) { Error = TEXT("Mesh is unavailable or compiling."); return false; }
    TArray<uint8> Data;
    FObjectWriter Writer(Mesh, Data, false, false, false, PPF_DuplicateVerbatim);
    if (Writer.IsError() || Data.IsEmpty()) { Error = TEXT("Could not snapshot the original mesh."); return false; }
    OutHash = FMD5::HashBytes(Data.GetData(), uint64(Data.Num()));
    for (int32 LODIndex = 0; LODIndex < Mesh->GetNumSourceModels(); ++LODIndex)
    {
        const FMeshDescriptionBulkData* Bulk = Mesh->GetSourceModel(LODIndex).GetMeshDescriptionBulkData();
        OutHash += TEXT("|") + (Bulk ? Bulk->GetUnversionedIdString() : FString(TEXT("no-bulk")));
    }
    FString Filename;
    if (FPackageName::DoesPackageExist(Mesh->GetOutermost()->GetName(), &Filename))
    {
        const FFileStatData Stat = IFileManager::Get().GetStatData(*Filename);
        if (!Stat.bIsValid) { Error = TEXT("Could not inspect the original package file."); return false; }
        OutHash += FString::Printf(TEXT("|%lld|%lld"), Stat.FileSize, Stat.ModificationTime.GetTicks());
    }
    Error.Reset();
    return true;
}

bool RXApplyLOD::RestorePreviewSettings(UStaticMesh* Original, UStaticMesh* Preview,
    TMap<int32, TArray<FRXSectionWeight>>& OutWeights, bool& bOutIncludeImpostor, FString& Error)
{
    if (!Original || !Preview || !IsInGameThread()) { Error = TEXT("Invalid preview or source."); return false; }
    const FString Json = Original->GetOutermost()->GetMetaData().GetValue(Original, RXWeightMetadata::Key());
    if (Json.IsEmpty()) { OutWeights.Reset(); bOutIncludeImpostor = false; Error.Reset(); return true; }
    FString SourceId;
    TMap<int32, FRXStoredLODWeights> LODs;
    int32 ImpostorLOD = INDEX_NONE, ImpostorSlot = INDEX_NONE;
    if (!RXWeightMetadata::Decode(Json, SourceId, LODs, ImpostorLOD, ImpostorSlot, Error)) { return false; }
    if (SourceId != GetSourceId(Original)) { Error = TEXT("Stored section weights do not match the current LOD 0 source."); return false; }
    if (ImpostorLOD != INDEX_NONE && (ImpostorLOD != Preview->GetNumSourceModels() - 1 ||
        !Preview->GetStaticMaterials().IsValidIndex(ImpostorSlot)))
    { Error = TEXT("Stored impostor LOD or material no longer matches the mesh."); return false; }
    for (const TPair<int32, FRXStoredLODWeights>& Pair : LODs)
    {
        if (!Preview->IsSourceModelValid(Pair.Key) || Pair.Key == ImpostorLOD) { Error = TEXT("Stored weight LOD no longer exists."); return false; }
        const FMeshDescription* Base = Preview->GetMeshDescription(0);
        if (!Base || Pair.Value.Sections.Num() != Base->PolygonGroups().Num()) { Error = TEXT("Stored section layout no longer matches LOD 0."); return false; }
        for (const FRXSectionWeight& Section : Pair.Value.Sections)
        {
            if (!Base->IsPolygonGroupValid(Section.Group)) { Error = TEXT("Stored section identity is missing."); return false; }
        }
    }
    if (ImpostorLOD != INDEX_NONE)
    {
        Preview->SetNumSourceModels(ImpostorLOD);
        for (int32 Section = Preview->GetSectionInfoMap().GetSectionNumber(ImpostorLOD) - 1; Section >= 0; --Section)
        {
            Preview->GetSectionInfoMap().Remove(ImpostorLOD, Section);
            Preview->GetOriginalSectionInfoMap().Remove(ImpostorLOD, Section);
        }
        bool bUsedByOtherLOD = false;
        for (int32 LODIndex = 0; LODIndex < ImpostorLOD; ++LODIndex)
        {
            for (int32 Section = 0; Section < Preview->GetSectionInfoMap().GetSectionNumber(LODIndex); ++Section)
            {
                bUsedByOtherLOD |= Preview->GetSectionInfoMap().Get(LODIndex, Section).MaterialIndex == ImpostorSlot;
            }
        }
        if (!bUsedByOtherLOD && ImpostorSlot == Preview->GetStaticMaterials().Num() - 1)
        { Preview->GetStaticMaterials().RemoveAt(ImpostorSlot); }
    }
    TMap<int32, TArray<FRXSectionWeight>> Weights;
    for (const TPair<int32, FRXStoredLODWeights>& Pair : LODs)
    {
        Preview->ClearMeshDescription(Pair.Key);
        Preview->GetSourceModel(Pair.Key).ReductionSettings = Pair.Value.Settings;
        Weights.Add(Pair.Key, Pair.Value.Sections);
    }
    OutWeights = MoveTemp(Weights);
    bOutIncludeImpostor = ImpostorLOD != INDEX_NONE;
    Error.Reset();
    return true;
}

bool RXApplyLOD::Apply(UStaticMesh* Original, UStaticMesh* Preview, const FString& BaselineHash,
    const TMap<int32, TArray<FRXSectionWeight>>& Weights, bool bIncludeImpostor,
    bool& bMutationStarted, TArray<FString>& OutCreatedPackages, FString& Error, const FRXImpostorPreview* PreparedImpostor)
{
    bMutationStarted = false;
    OutCreatedPackages.Reset();
    Error.Reset();
    if (!IsInGameThread() || !Original || !Preview || Original == Preview ||
        Original->IsCompiling() || Preview->IsCompiling() || Preview->GetNumSourceModels() < 1)
    { Error = TEXT("Source or preview is unavailable or still compiling."); return false; }
    if (!GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
    { Error = TEXT("Stop the play or simulation session before applying."); return false; }
    if (UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
    {
        if (Editors->FindEditorForAsset(Original, false))
        { Error = TEXT("Close the original mesh editor before applying; it will not be closed automatically."); return false; }
    }
    else { Error = TEXT("Asset editor state is unavailable."); return false; }
    if (Original->GetOutermost()->IsDirty())
    { Error = TEXT("The original package has unsaved changes. Save or resolve them before applying."); return false; }
    FString CurrentHash;
    if (!Fingerprint(Original, CurrentHash, Error) || CurrentHash != BaselineHash)
    { Error = TEXT("The original mesh changed after preview started. Reopen the preview."); return false; }
    if (GetSourceId(Original).IsEmpty() || GetSourceId(Original) != GetSourceId(Preview))
    { Error = TEXT("The preview's LOD 0 source changed; reopen the preview."); return false; }
    if ((!Weights.IsEmpty() || bIncludeImpostor) && Preview->GetLODGroup() != NAME_None)
    { Error = TEXT("Weighted custom LODs require LOD Group None to prevent group settings from reducing them again."); return false; }
    for (const FStaticMaterial& Slot : Preview->GetStaticMaterials())
    {
        if (Slot.MaterialInterface && Slot.MaterialInterface->GetOutermost() == GetTransientPackage())
        { Error = TEXT("A transient preview material cannot be saved on the original."); return false; }
    }

    TArray<FStagedLOD> Staged;
    TMap<int32, FRXStoredLODWeights> Stored;
    for (int32 LODIndex = 0; LODIndex < Preview->GetNumSourceModels(); ++LODIndex)
    {
        const FStaticMeshSourceModel& Model = Preview->GetSourceModel(LODIndex);
        FStagedLOD& Entry = Staged.AddDefaulted_GetRef();
        Entry.Build = Model.BuildSettings;
        Entry.Reduction = Model.ReductionSettings;
        Entry.ScreenSize = Model.ScreenSize;
        Entry.ImportFilename = Model.SourceImportFilename;
        Entry.bImportWithBase = Model.bImportWithBaseMesh;
        if (const TArray<FRXSectionWeight>* SectionWeights = Weights.Find(LODIndex))
        {
            TStrongObjectPtr<UStaticMesh> Display;
            TArray<FRXSectionReductionResult> Results;
            if (!RXWeightedPreview::Build(Preview, LODIndex, *SectionWeights, Display, Results, Error)) { return false; }
            const FMeshDescription* Reduced = Display->GetMeshDescription(LODIndex);
            if (!Reduced) { Error = TEXT("Weighted preview source geometry is unavailable."); return false; }
            Entry.Description = MakeUnique<FMeshDescription>(*Reduced);
            FRXStoredLODWeights Record;
            Record.Settings = Model.ReductionSettings;
            Record.Sections = *SectionWeights;
            Stored.Add(LODIndex, MoveTemp(Record));
            Entry.Reduction = FMeshReductionSettings();
            Entry.Reduction.BaseLODModel = LODIndex;
        }
        else if (LODIndex > 0 && Preview->IsMeshDescriptionValid(LODIndex))
        {
            const FMeshDescription* Description = Preview->GetMeshDescription(LODIndex);
            if (!Description) { Error = TEXT("Preview LOD source geometry is unavailable."); return false; }
            Entry.Description = MakeUnique<FMeshDescription>(*Description);
        }
    }
    for (const TPair<int32, TArray<FRXSectionWeight>>& Pair : Weights)
    {
        if (Pair.Key < 1 || !Staged.IsValidIndex(Pair.Key)) { Error = TEXT("A weighted LOD no longer exists."); return false; }
    }
    FRXImpostorPreview Impostor;
    const FRXImpostorPreview* SelectedImpostor = PreparedImpostor;
    int32 ImpostorLOD = INDEX_NONE;
    int32 ImpostorSlot = INDEX_NONE;
    if (bIncludeImpostor)
    {
        if (Preview->IsNaniteEnabled() || Staged.Num() >= MAX_STATIC_MESH_LODS)
        { Error = TEXT("Impostor LOD requires a non-Nanite mesh and an available LOD slot."); return false; }
        if (!Preview->GetAutoComputeLODScreenSize() && Staged.Last().ScreenSize.Default <= 0.0f)
        { Error = TEXT("The last screen size is too small to append an impostor LOD; adjust it first."); return false; }
        if (!SelectedImpostor)
        {
            if (!RXImpostorPreview::Build(Preview, FIntPoint(512, 512), 4, 4, Impostor, Error)) { return false; }
            SelectedImpostor = &Impostor;
        }
        if (!SelectedImpostor->Geometry || !SelectedImpostor->Material.Texture.IsValid() || !SelectedImpostor->Material.Material.IsValid()) { Error = TEXT("Prepared impostor is incomplete."); return false; }
        ImpostorLOD = Staged.Num();
        ImpostorSlot = Preview->GetStaticMaterials().Num();
        FStagedLOD& Entry = Staged.AddDefaulted_GetRef();
        Entry.Build = Preview->GetSourceModel(0).BuildSettings;
        Entry.Build.bRecomputeNormals = false;
        Entry.Build.bRecomputeTangents = false;
        Entry.Build.bGenerateLightmapUVs = false;
        Entry.Reduction = FMeshReductionSettings();
        Entry.Reduction.BaseLODModel = ImpostorLOD;
        Entry.ScreenSize.Default = Staged[ImpostorLOD - 1].ScreenSize.Default * 0.5f;
        Entry.Description = MakeUnique<FMeshDescription>(*SelectedImpostor->Geometry);
    }
    FString Metadata;
    if (!RXWeightMetadata::Encode(GetSourceId(Original), Stored, ImpostorLOD, ImpostorSlot, Metadata, Error)) { return false; }
    if (!Fingerprint(Original, CurrentHash, Error) || CurrentHash != BaselineHash || Original->GetOutermost()->IsDirty())
    { Error = TEXT("Original mesh changed while the preview was being prepared."); return false; }

    FRXImpostorAssets Assets;
    if (bIncludeImpostor)
    {
        const bool bCreated = RXImpostorAssets::Create(Original, *SelectedImpostor, Assets, Error);
        OutCreatedPackages = Assets.CreatedPackages;
        if (!bCreated) { return false; }
        FStaticMeshAttributes Attributes(*Staged[ImpostorLOD].Description);
        auto Names = Attributes.GetPolygonGroupMaterialSlotNames();
        if (!Names.IsValid() || Staged[ImpostorLOD].Description->PolygonGroups().Num() != 1)
        { Error = TEXT("Impostor geometry has an invalid section; created packages remain unsaved."); return false; }
        Names[Staged[ImpostorLOD].Description->PolygonGroups().GetFirstValidID()] = Assets.Material->GetFName();
        if (!Fingerprint(Original, CurrentHash, Error) || CurrentHash != BaselineHash || Original->GetOutermost()->IsDirty())
        { Error = TEXT("Original changed after generated packages were created; inspect the unsaved packages."); return false; }
    }
    const FScopedTransaction Transaction(NSLOCTEXT("RXSMLODGen", "ApplyLODTransaction", "Apply RX LOD preview"));
    Original->Modify();
    bMutationStarted = true;
    RXImpostorAssets::CommitReplacement(Assets);
    if (Original->GetLODGroup() != Preview->GetLODGroup()) { Original->SetLODGroup(Preview->GetLODGroup(), false, false); }
    Original->SetNumSourceModels(Staged.Num());
    Original->SetAutoComputeLODScreenSize(Preview->GetAutoComputeLODScreenSize());
    Original->SetMinLOD(Preview->GetMinLOD());
    Original->GetNumStreamedLODs() = Preview->GetNumStreamedLODs();
    Original->SetQualityLevelMinLOD(Preview->GetQualityLevelMinLOD());
    Original->SetLightMapResolution(Preview->GetLightMapResolution());
    if (const FProperty* Streaming = FindFProperty<FProperty>(UStaticMesh::StaticClass(), TEXT("LODStreaming")))
    {
        Streaming->CopyCompleteValue_InContainer(Original, Preview);
    }
    if (const FProperty* Bias = FindFProperty<FProperty>(UStaticMesh::StaticClass(), TEXT("NoRefStreamingLODBias")))
    {
        Bias->CopyCompleteValue_InContainer(Original, Preview);
    }
    Original->GetStaticMaterials() = Preview->GetStaticMaterials();
    Original->GetSectionInfoMap() = Preview->GetSectionInfoMap();
    Original->GetOriginalSectionInfoMap() = Preview->GetOriginalSectionInfoMap();
    if (bIncludeImpostor)
    {
        const FName SlotName = Assets.Material->GetFName();
        Original->GetStaticMaterials().Add(FStaticMaterial(Assets.Material.Get(), SlotName, SlotName));
        FMeshSectionInfo SectionInfo(ImpostorSlot);
        SectionInfo.bEnableCollision = false;
        Original->GetSectionInfoMap().Set(ImpostorLOD, 0, SectionInfo);
        Original->GetOriginalSectionInfoMap().Set(ImpostorLOD, 0, SectionInfo);
    }
    for (int32 LODIndex = 0; LODIndex < Staged.Num(); ++LODIndex)
    {
        FStaticMeshSourceModel& Model = Original->GetSourceModel(LODIndex);
        FStagedLOD& Entry = Staged[LODIndex];
        Model.BuildSettings = Entry.Build;
        Model.ReductionSettings = Entry.Reduction;
        Model.ScreenSize = Entry.ScreenSize;
        Model.SourceImportFilename = Entry.ImportFilename;
        Model.bImportWithBaseMesh = Entry.bImportWithBase;
        if (LODIndex > 0)
        {
            Original->ClearMeshDescription(LODIndex);
            if (Entry.Description)
            {
                if (!Original->CreateMeshDescription(LODIndex, MoveTemp(*Entry.Description)))
                { Error = TEXT("Applying source geometry failed after mutation began; inspect the original before retrying."); return false; }
                Original->CommitMeshDescription(LODIndex);
            }
        }
    }
    Original->GetOutermost()->GetMetaData().SetValue(Original, RXWeightMetadata::Key(), *Metadata);
    Original->PostEditChange();
    Original->MarkPackageDirty();
    return true;
}
