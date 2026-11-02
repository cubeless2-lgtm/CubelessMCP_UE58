#include "RXWeightedPreview.h"

#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"

bool RXWeightedPreview::Build(UStaticMesh* Source, int32 TargetLOD, const TArray<FRXSectionWeight>& Weights,
    TStrongObjectPtr<UStaticMesh>& Output, TArray<FRXSectionReductionResult>& Results, FString& Error)
{
    Error.Reset();
    if (!IsInGameThread() || !Source || Source->IsCompiling() || Source->IsNaniteEnabled() ||
        TargetLOD < 1 || !Source->IsSourceModelValid(TargetLOD) || !Source->IsMeshDescriptionValid(0))
    {
        Error = TEXT("A ready non-Nanite preview mesh with LOD 0 source data and an existing target LOD is required.");
        return false;
    }
    const FMeshReductionSettings Settings = Source->GetSourceModel(TargetLOD).ReductionSettings;
    if (Settings.BaseLODModel != 0)
    {
        Error = TEXT("Weighted preview currently requires LOD 0 as the base LOD.");
        return false;
    }
    const FMeshDescription* Base = Source->GetMeshDescription(0);
    if (!Base)
    {
        Error = TEXT("LOD 0 source geometry is unavailable.");
        return false;
    }
    FMeshDescription Reduced;
    TArray<FRXSectionReductionResult> LocalResults;
    if (!RXSectionReduction::Reduce(*Base, Settings, Weights, Reduced, LocalResults, Error))
    {
        return false;
    }
    TStrongObjectPtr<UStaticMesh> Display(DuplicateObject<UStaticMesh>(Source, GetTransientPackage(), NAME_None));
    if (!Display.IsValid() || !Display->CreateMeshDescription(TargetLOD, MoveTemp(Reduced)))
    {
        Error = TEXT("Could not create the display-only LOD mesh description.");
        return false;
    }
    Display->SetFlags(RF_Transient);
    Display->ClearFlags(RF_Public | RF_Standalone);
    FStaticMeshSourceModel& DisplayModel = Display->GetSourceModel(TargetLOD);
    DisplayModel.ReductionSettings = FMeshReductionSettings();
    DisplayModel.ReductionSettings.BaseLODModel = TargetLOD;
    UStaticMesh::FCommitMeshDescriptionParams CommitParams;
    CommitParams.bMarkPackageDirty = false;
    Display->CommitMeshDescription(TargetLOD, CommitParams);
    Display->PostEditChange();
    TArray<UStaticMesh*> Pending = { Display.Get() };
    FStaticMeshCompilingManager::Get().FinishCompilation(Pending);
    if (Display->IsCompiling() || !Display->GetRenderData() || Display->GetRenderData()->LODResources.Num() <= TargetLOD)
    {
        Error = TEXT("The display-only mesh did not finish building the requested LOD.");
        return false;
    }
    Output = MoveTemp(Display);
    Results = MoveTemp(LocalResults);
    return true;
}
