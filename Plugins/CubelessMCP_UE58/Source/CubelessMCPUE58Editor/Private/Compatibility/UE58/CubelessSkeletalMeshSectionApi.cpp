// HI: Editor-only persistent skeletal section flags. No runtime class or transport.
#include "Compatibility/UE58/CubelessSkeletalMeshSectionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "ScopedTransaction.h"
#include "SkeletalMeshTypes.h"
#include "SkinnedAssetCompiler.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::SkeletalMeshSections
{
namespace
{
constexpr int32 MaxSections = 4096;
constexpr int32 MaxObjects = 65536;
bool Running = false;
bool Live(const UObject* O)
{
    return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed);
}
bool GlobalSafe()
{
    return IsInGameThread() && !IsGarbageCollecting() && !IsEngineExitRequested() && !GExitPurge && !UE::IsSavingPackage();
}
bool Same(const FString& A, const FString& B) { return A.Equals(B, ESearchCase::CaseSensitive); }
bool PathSafe(const FString& S)
{
    if (S.IsEmpty() || S.Len() >= NAME_SIZE) return false;
    for (int32 I = 0; I < S.Len(); ++I) if (S[I] == 0) return false;
    // Pure FString parsing mirrors only the SDK length-expansion path, not its load/validation work.
    FString T = FPackageName::ExportTextPathToObjectPath(S);
    int32 Spaces = 0, SpaceAt = INDEX_NONE;
    for (int32 I = 0; I < T.Len(); ++I) if (T[I] == TCHAR(32)) { ++Spaces; SpaceAt = I; }
    if (Spaces > 1) return false;
    T = Spaces == 1 ? T.Mid(SpaceAt + 1) : T.TrimStartAndEnd();
    T.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
    FPaths::RemoveDuplicateSlashes(T);
    int32 Colon = INDEX_NONE; if (T.FindChar(TCHAR(58), Colon)) T.LeftInline(Colon);
    int32 Dot = INDEX_NONE; FString ObjectName;
    if (T.FindChar(TCHAR(46), Dot)) { ObjectName = T.Mid(Dot + 1); T.LeftInline(Dot); }
    else { int32 Slash = INDEX_NONE; T.FindLastChar(TCHAR(47), Slash); ObjectName = T.Mid(Slash + 1); }
    return !T.IsEmpty() && !ObjectName.IsEmpty() && (T.Len() + 1 + ObjectName.Len()) < NAME_SIZE;
}
struct FIdentity
{
    TStrongObjectPtr<UObject> Hold{nullptr};
    TWeakObjectPtr<UObject> Object, Outer;
    TWeakObjectPtr<UClass> Class;
    TWeakObjectPtr<UPackage> Package, Outermost;
    FString Path;
    explicit FIdentity(UObject* O) : Hold(O), Object(O), Outer(O->GetOuter()), Class(O->GetClass()),
        Package(O->GetPackage()), Outermost(O->GetOutermost()), Path(O->GetPathName()) {}
    bool Exact() const
    {
        auto* O = Object.Get(); auto* C = Class.Get();
        return Live(O) && Live(C) && Live(Package.Get()) && Live(Outermost.Get()) &&
            !C->HasAnyClassFlags(CLASS_NewerVersionExists) && O->GetClass() == C && O->GetOuter() == Outer.Get() &&
            O->GetPackage() == Package.Get() && O->GetOutermost() == Outermost.Get() && Same(O->GetPathName(), Path);
    }
};
struct FUserState
{
    bool Recompute = false, Shadow = false, Ray = false, Disabled = false;
    int32 Mask = 0, Cloth = INDEX_NONE, ClothLOD = INDEX_NONE, Generate = INDEX_NONE;
    FGuid ClothGuid;
    static FUserState From(const FSkelMeshSourceSectionUserData& U)
    {
        FUserState S; S.Recompute = U.bRecomputeTangent; S.Mask = static_cast<int32>(U.RecomputeTangentsVertexMaskChannel);
        S.Shadow = U.bCastShadow; S.Ray = U.bVisibleInRayTracing; S.Disabled = U.bDisabled;
        S.Cloth = U.CorrespondClothAssetIndex; S.ClothGuid = U.ClothingData.AssetGuid;
        S.ClothLOD = U.ClothingData.AssetLodIndex; S.Generate = U.GenerateUpToLodIndex; return S;
    }
    static FUserState From(const FSkelMeshSection& U)
    {
        FUserState S; S.Recompute = U.bRecomputeTangent; S.Mask = static_cast<int32>(U.RecomputeTangentsVertexMaskChannel);
        S.Shadow = U.bCastShadow; S.Ray = U.bVisibleInRayTracing; S.Disabled = U.bDisabled;
        S.Cloth = U.CorrespondClothAssetIndex; S.ClothGuid = U.ClothingData.AssetGuid;
        S.ClothLOD = U.ClothingData.AssetLodIndex; S.Generate = U.GenerateUpToLodIndex; return S;
    }
    bool operator==(const FUserState& B) const
    {
        return Recompute == B.Recompute && Mask == B.Mask && Shadow == B.Shadow && Ray == B.Ray && Disabled == B.Disabled &&
            Cloth == B.Cloth && ClothGuid == B.ClothGuid && ClothLOD == B.ClothLOD && Generate == B.Generate;
    }
};
struct FSectionState
{
    int32 Original = INDEX_NONE, Parent = INDEX_NONE, Material = INDEX_NONE;
    FUserState Fields;
    static FSectionState From(const FSkelMeshSection& S)
    {
        FSectionState O; O.Original = S.OriginalDataSectionIndex; O.Parent = S.ChunkedParentSectionIndex;
        O.Material = S.MaterialIndex; O.Fields = FUserState::From(S); return O;
    }
    bool operator==(const FSectionState& B) const
    { return Original == B.Original && Parent == B.Parent && Material == B.Material && Fields == B.Fields; }
};
struct FProjection
{
    TArray<FSectionState> Sections;
    TMap<int32, FUserState> User;
    bool operator==(const FProjection& B) const
    {
        if (Sections.Num() != B.Sections.Num() || User.Num() != B.User.Num()) return false;
        for (int32 I = 0; I < Sections.Num(); ++I) if (!(Sections[I] == B.Sections[I])) return false;
        for (const auto& P : User) { const auto* Q = B.User.Find(P.Key); if (!Q || !(P.Value == *Q)) return false; }
        return true;
    }
};
struct FWork
{
    Variable::FResult R;
    TStrongObjectPtr<UEditorEngine> Editor{nullptr};
    TStrongObjectPtr<UWorld> World{nullptr};
    TArray<TUniquePtr<FIdentity>> Objects;
    TSet<UObject*> Seen;
    TArray<TWeakObjectPtr<USkeletalMeshComponent>> Components;
    USkeletalMesh* Mesh = nullptr;
    UPackage* Package = nullptr;
    FString MeshPath;
    int32 LODIndex = 0, SectionIndex = 0, Original = INDEX_NONE;
    bool Enabled = false, DryRun = true, Save = true, Lost = false, WroteFlags = false;
    bool PreviouslyEnabled = false, NeedsChange = false, SDKEntered = false, Verified = false;
    TArray<int32> Affected;
    FProjection Before, Final;
    TArray<TSharedPtr<FJsonValue>> Calls;
    TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Active;
    FWork()
    {
        R.Compiled = false;
        R.Data->SetObjectField(TEXT("native_call_counts"), Counts);
        R.Data->SetBoolField(TEXT("automatic_retry_permitted"), false);
        R.Data->SetBoolField(TEXT("cleanup_performed"), false);
        R.Data->SetBoolField(TEXT("atomic_rollback"), false);
        R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"), false);
        R.Data->SetStringField(TEXT("compile_wait_scope"), TEXT("Public mesh FinishCompilation and stock PostEdit scope may synchronously block; dependency processing, SDK callbacks/render-state flush are not cancellable or atomic. No GT sleep/poll, async Job, GPU freshness or cold persistence proof."));
    }
    bool Fail(const TCHAR* Code, const FString& Text)
    {
        if (R.ErrorCode.IsEmpty()) { R.ErrorCode = Code; R.Error = Text; }
        Verified = false; return false;
    }
    bool Pin(UObject* O)
    {
        if (!GlobalSafe() || !Live(O) || !Live(O->GetClass()) || !Live(O->GetPackage()) || !Live(O->GetOutermost()))
            return Fail(TEXT("skeletal_section_object_unavailable"), TEXT("Current object/class/package is unavailable."));
        if (Seen.Contains(O)) return true;
        if (Objects.Num() >= MaxObjects) return Fail(TEXT("skeletal_section_object_budget"), TEXT("Target/component object capture exceeded budget."));
        Seen.Add(O); Objects.Add(MakeUnique<FIdentity>(O)); return true;
    }
    bool Init()
    {
        if (!GlobalSafe() || !Live(GEditor) || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
            return Fail(TEXT("skeletal_section_editor_unavailable"), TEXT("Requires the editor game thread outside PIE/SIE/GC/exit/save."));
        Editor.Reset(GEditor); World.Reset(GEditor->GetEditorWorldContext().World());
        if (!Live(World.Get()) || World->bIsTearingDown || !Pin(World.Get()))
            return Fail(TEXT("skeletal_section_world_unavailable"), TEXT("Exact live editor world is required."));
        return true;
    }
    bool Guard()
    {
        if (Lost || !R.ErrorCode.IsEmpty()) return false;
        if (!GlobalSafe() || !Live(GEditor) || GEditor != Editor.Get() || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor ||
            !Live(World.Get()) || World->bIsTearingDown || GEditor->GetEditorWorldContext().World() != World.Get())
        { Lost = true; return Fail(TEXT("skeletal_section_context_lost"), TEXT("Editor/world/GC/exit/save context changed; no follow-up plugin writer or replay.")); }
        for (const auto& I : Objects) if (!I->Exact())
        { Lost = true; return Fail(TEXT("skeletal_section_identity_changed"), TEXT("Captured object/class/outer/effective package/path identity changed.")); }
        for (const auto& Weak : Components) if (!Weak.Get() || Weak->GetSkeletalMeshAsset() != Mesh)
        { Lost = true; return Fail(TEXT("skeletal_section_component_role_changed"), TEXT("Captured stock scope component no longer references the exact mesh.")); }
        return true;
    }
    TSharedRef<FJsonObject> Enter(const TCHAR* Stage, bool Writes)
    {
        R.Stage = Stage; R.bMutationAttempted |= Writes; R.bChangeStateKnown = false;
        SDKEntered = true; R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"), true);
        double N = 0; Counts->TryGetNumberField(Stage, N); Counts->SetNumberField(Stage, N + 1);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("stage"), Stage);
        Row->SetBoolField(TEXT("mutation_attempted"), Writes); Row->SetBoolField(TEXT("SDK_returned"), false);
        Calls.Add(MakeShared<FJsonValueObject>(Row)); Active = Row; return Row;
    }
    template<class F> bool Call(const TCHAR* Stage, bool Writes, F&& Fn)
    {
        if (!Guard()) return false;
        auto Row = Enter(Stage, Writes); Fn(Row); Row->SetBoolField(TEXT("SDK_returned"), true); return Guard();
    }
    bool Bind(USkeletalMesh* M)
    {
        if (!Guard() || !Live(M) || !M->IsAsset() || !Pin(M)) return Fail(TEXT("skeletal_section_mesh_unavailable"), TEXT("skeletal_mesh_path must resolve to a SkeletalMesh asset."));
        Mesh = M; Package = Mesh->GetOutermost(); MeshPath = Mesh->GetPathName();
        if (Mesh->GetPackage() != Package || !Pin(Package)) return Fail(TEXT("skeletal_section_package_mismatch"), TEXT("Mesh effective package must match its outermost package."));
        R.Data->SetStringField(TEXT("actual_mesh_path"), MeshPath); return true;
    }
    bool FinishCompilation(bool IncludeDependents, const TCHAR* Stage)
    {
        if (!Guard()) return false;
        if (!Call(Stage, false, [&](const auto& Row)
        {
            FSkinnedAssetCompilingManager::FFinishCompilationOptions Options;
            Options.bIncludeDependentAssets = IncludeDependents;
            Row->SetBoolField(TEXT("include_dependent_assets"), IncludeDependents);
            USkinnedAsset* Assets[] = { Mesh };
            FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets, Options);
        })) return false;
        if (Mesh->IsCompiling()) return Fail(TEXT("skeletal_section_compile_pending"), TEXT("Mesh is still compiling after SDK FinishCompilation; no direct flag write/save."));
        return true;
    }
    bool Capture(FProjection& Out, FSkeletalMeshLODModel** CurrentLOD = nullptr)
    {
        if (!Guard()) return false;
        if (Mesh->IsCompiling()) return Fail(TEXT("skeletal_section_compile_pending"), TEXT("Imported data observation requires the exact mesh to be idle."));
        FSkeletalMeshModel* Model = nullptr;
        if (!Call(TEXT("get_imported_model"), false, [&](const auto&) { Model = Mesh->GetImportedModel(); })) return false;
        if (Mesh->IsCompiling()) return Fail(TEXT("skeletal_section_compile_pending"), TEXT("Mesh compilation became active during imported-model getter."));
        if (!Model || !Model->LODModels.IsValidIndex(LODIndex)) return Fail(TEXT("skeletal_section_lod_invalid"), TEXT("Invalid LOD index or missing imported model."));
        const auto& LOD = Model->LODModels[LODIndex];
        if (LOD.Sections.Num() > MaxSections || LOD.UserSectionsData.Num() > MaxSections)
            return Fail(TEXT("skeletal_section_capture_budget"), TEXT("Imported section/source-map count exceeds4096 capture budget."));
        Out.Sections.Reset(); Out.User.Reset();
        for (const auto& S : LOD.Sections) Out.Sections.Add(FSectionState::From(S));
        for (const auto& P : LOD.UserSectionsData) Out.User.Add(P.Key, FUserState::From(P.Value));
        if (!Guard()) return false;
        if (CurrentLOD) *CurrentLOD = &Model->LODModels[LODIndex];
        return true;
    }
    bool Unchanged()
    {
        FProjection Current; if (!Capture(Current)) return false;
        if (!(Current == Before)) return Fail(TEXT("skeletal_section_prewrite_changed"), TEXT("Imported section/source values changed across SDK callback before the first flag write."));
        return true;
    }
    bool CaptureComponents()
    {
        if (!Guard()) return false;
        for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
        {
            auto* C = *It;
            if (Live(C) && C->GetSkeletalMeshAsset() == Mesh)
            { if (!Pin(C)) return false; Components.Add(C); }
        }
        return Guard();
    }
    bool PackageOwned()
    {
        if (!Guard()) return false;
        TArray<UObject*> Exports; GetObjectsWithPackage(Package, Exports, EGetObjectsFlags::IncludeNestedObjects);
        if (Exports.Num() > MaxObjects) return Fail(TEXT("skeletal_section_export_budget"), TEXT("Mesh package export budget exceeded."));
        for (auto* O : Exports)
            if (!Live(O) || !(O == Mesh || O->IsIn(Mesh)) || O->GetPackage() != Package || O->GetOutermost() != Package)
                return Fail(TEXT("skeletal_section_foreign_export"), TEXT("Exact mesh/subtree ownership is required for package save; foreign export is not overwritten."));
        return true;
    }
    bool VerifyRebuilt()
    {
        if (!Capture(Final)) return false;
        const auto* User = Final.User.Find(Original);
        if (!User || User->Disabled != !Enabled) return Fail(TEXT("skeletal_section_readback_mismatch"), TEXT("Source section readback mismatch after rebuild; not saved."));
        FUserState Expected = Before.User.Contains(Original) ? Before.User[Original] : Before.Sections[SectionIndex].Fields;
        Expected.Disabled = !Enabled;
        if (!(*User == Expected)) return Fail(TEXT("skeletal_section_source_settings_changed"), TEXT("Selected source cloth/tangent/shadow/ray/settings were not preserved."));
        if (Final.User.Num() != Before.User.Num() + (Before.User.Contains(Original) ? 0 : 1))
            return Fail(TEXT("skeletal_section_source_map_changed"), TEXT("Unrelated source-map membership changed during rebuild."));
        for (const auto& P : Before.User) if (P.Key != Original)
        { const auto* U = Final.User.Find(P.Key); if (!U || !(P.Value == *U)) return Fail(TEXT("skeletal_section_unrelated_source_changed"), TEXT("Unrelated source section values changed during rebuild.")); }
        bool Found = false;
        for (const auto& S : Final.Sections) if (S.Original == Original)
        { Found = true; if (S.Fields.Disabled != !Enabled) return Fail(TEXT("skeletal_section_readback_mismatch"), TEXT("A rebuilt section of the selected original ID has the wrong flag; not saved.")); }
        if (!Found) return Fail(TEXT("skeletal_section_original_id_missing"), TEXT("Selected original section identity disappeared after rebuild; not saved."));
        // Keep unrelated section identity/visible flags observable without pretending to prove vertex-buffer or whole-RAM equality.
        for (int32 BeforeIndex = 0; BeforeIndex < Before.Sections.Num(); ++BeforeIndex)
        {
            if (Affected.Contains(BeforeIndex)) continue;
            const auto& S = Before.Sections[BeforeIndex];
            bool Present = false;
            for (const auto& T : Final.Sections) if (T.Original == S.Original && T.Material == S.Material && T.Fields.Disabled == S.Fields.Disabled) { Present = true; break; }
            if (!Present) return Fail(TEXT("skeletal_section_unrelated_flag_changed"), TEXT("An unrelated original section/material/visible flag was not preserved."));
        }
        return Guard();
    }
    TSharedRef<FJsonObject> Readback(const FProjection& P)
    {
        auto Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("success"), true);
        Out->SetStringField(TEXT("skeletal_mesh_path"), MeshPath); Out->SetNumberField(TEXT("lod_index"), LODIndex);
        Out->SetBoolField(TEXT("package_dirty"), Package->IsDirty());
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 I = 0; I < P.Sections.Num(); ++I)
        {
            const auto& S = P.Sections[I]; auto Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("section_index"), I); Row->SetNumberField(TEXT("material_index"), S.Material);
            Row->SetNumberField(TEXT("original_data_section_index"), S.Original); Row->SetNumberField(TEXT("chunked_parent_section_index"), S.Parent);
            Row->SetBoolField(TEXT("enabled"), !S.Fields.Disabled);
            if (const auto* U = P.User.Find(S.Original)) Row->SetBoolField(TEXT("source_enabled"), !U->Disabled);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Out->SetArrayField(TEXT("sections"), Rows); Out->SetBoolField(TEXT("dry_run"), DryRun);
        Out->SetBoolField(TEXT("previously_enabled"), PreviouslyEnabled); Out->SetBoolField(TEXT("requested_enabled"), Enabled);
        Out->SetBoolField(TEXT("would_change"), NeedsChange); Out->SetBoolField(TEXT("mutation_performed"), WroteFlags);
        Out->SetBoolField(TEXT("saved"), R.bSaved); TArray<TSharedPtr<FJsonValue>> Indices;
        for (int32 I : Affected) Indices.Add(MakeShared<FJsonValueNumber>(I));
        Out->SetArrayField(TEXT("affected_section_indices_before_rebuild"), Indices); return Out;
    }
};
// Stock SDK scope destructors are not cancellable. Their entered/returned receipts are plain data;
// callbacks may already have run when a plugin guard stops subsequent flag writes/save.
bool Edit(FWork& W)
{
    if (!W.CaptureComponents() || !W.Unchanged()) return false;
    // Public compiler documentation requires reverse dependents to finish before editing.
    if (!W.FinishCompilation(true, TEXT("finish_before_edit_with_dependents")) || !W.Unchanged()) return false;
    auto Preview = W.Enter(TEXT("suspend_alternate_skin_weight_preview"), true);
    {
        FScopedSuspendAlternateSkinWeightPreview Suspend(W.Mesh);
        Preview->SetBoolField(TEXT("constructor_returned"), true);
        if (W.Guard() && W.Unchanged())
        {
            auto Post = W.Enter(TEXT("skeletal_post_edit_scope"), true);
            {
                FScopedSkeletalMeshPostEditChange PostEdit(W.Mesh);
                Post->SetBoolField(TEXT("constructor_returned"), true);
                if (W.Guard() && W.Unchanged())
                {
                    auto TransactionRow = W.Enter(TEXT("section_transaction"), false);
                    {
                        FScopedTransaction Transaction(NSLOCTEXT("CubelessHI", "SetSectionEnabled", "Set skeletal mesh section enabled"));
                        TransactionRow->SetBoolField(TEXT("constructor_returned"), true);
                        if (W.Guard() && W.Call(TEXT("mesh_modify"), true, [&](const auto& Row) { Row->SetBoolField(TEXT("accepted"), W.Mesh->Modify()); }) &&
                            W.Unchanged() && W.Call(TEXT("mesh_pre_edit_change"), true, [&](const auto&) { W.Mesh->PreEditChange(nullptr); }) && W.Unchanged())
                        {
                            // The getter can wait/callback. Capture and compare first, then use only the freshly returned LOD.
                            FSkeletalMeshLODModel* CurrentLOD = nullptr; FProjection Current;
                            if (!W.Capture(Current, &CurrentLOD) || !(Current == W.Before))
                            { W.Fail(TEXT("skeletal_section_prewrite_changed"), TEXT("Fresh imported data changed immediately before flag writes.")); }
                            else
                            {
                            auto& LOD = *CurrentLOD;
                            auto Row = W.Enter(TEXT("write_section_and_source_flags"), true);
                            Row->SetNumberField(TEXT("original_data_section_index"), W.Original);
                            Row->SetBoolField(TEXT("requested_enabled"), W.Enabled);
                            TArray<TSharedPtr<FJsonValue>> Written;
                            for (int32 I : W.Affected) { LOD.Sections[I].bDisabled = !W.Enabled; Written.Add(MakeShared<FJsonValueNumber>(I)); }
                            Row->SetArrayField(TEXT("section_indices_written"), Written);
                            FSkelMeshSourceSectionUserData::GetSourceSectionUserData(LOD.UserSectionsData, LOD.Sections[W.SectionIndex]).bDisabled = !W.Enabled;
                            Row->SetBoolField(TEXT("source_flag_written"), true); Row->SetBoolField(TEXT("SDK_returned"), true);
                            W.WroteFlags = true; W.R.bChanged = true;
                            }
                        }
                        TransactionRow->SetBoolField(TEXT("destructor_entered"), true);
                    }
                    TransactionRow->SetBoolField(TEXT("SDK_returned"), true);
                }
                Post->SetBoolField(TEXT("destructor_entered"), true);
                W.R.bCompileAttempted = true; W.R.Compiled.Reset();
            }
            Post->SetBoolField(TEXT("SDK_returned"), true);
        }
        Preview->SetBoolField(TEXT("destructor_entered"), true);
    }
    Preview->SetBoolField(TEXT("SDK_returned"), true);
    if (!W.Guard() || !W.FinishCompilation(false, TEXT("finish_after_post_edit"))) return false;
    W.R.Compiled = true;
    return W.VerifyRebuilt();
}
FString Finish(FWork& W)
{
    W.R.bSuccess = W.Verified && W.R.ErrorCode.IsEmpty(); W.R.bPostconditionPassed = W.R.bSuccess;
    if (W.R.bSuccess) { W.R.bChangeStateKnown = true; W.R.Stage = TEXT("complete"); }
    else if (W.SDKEntered) W.R.bChangeStateKnown = false;
    if (!W.R.bSuccess)
    {
        auto Error = MakeShared<FJsonObject>(); Error->SetBoolField(TEXT("success"), false);
        Error->SetStringField(TEXT("error"), W.R.Error); Error->SetBoolField(TEXT("mutation_performed"), W.WroteFlags);
        // Preserve original SectionError shape; actual SDK true history is authoritative in typed state/data.
        Error->SetBoolField(TEXT("saved"), false); W.R.Data->SetObjectField(TEXT("legacy_result"), Error);
    }
    W.R.Data->SetArrayField(TEXT("actual_mutation_prefix"), W.Calls);
    W.R.Data->SetBoolField(TEXT("actual_section_flags_written"), W.WroteFlags);
    W.R.Data->SetBoolField(TEXT("actual_saved_API_true"), W.R.bSaved);
    W.R.Data->SetBoolField(TEXT("operation_complete"), true);
    W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"), W.R.bChangeStateKnown);
    W.R.Data->SetBoolField(TEXT("mutation_state_unknown"), !W.R.bChangeStateKnown);
    W.R.Data->SetBoolField(TEXT("do_not_retry"), !W.R.bSuccess && W.R.bMutationAttempted);
    W.R.Data->SetBoolField(TEXT("post_failure_live_observation_skipped"), !W.R.bSuccess);
    W.R.Data->SetStringField(TEXT("verification_scope"), TEXT("Current imported section identity/flags and source-user-data readback, exact mesh/context, SDK finish/save return and current dirty state only. No vertex-buffer/whole-RAM equality, GPU/render completion, cold reload, cancellation or atomic rollback proof."));
    return Cubeless::Operations::EncodeVariableResult(W.R, TEXT("set_skeletal_mesh_section_enabled"));
}
}
FString SetSectionEnabled(const FString& SkeletalMeshPath, int32 LODIndex, int32 SectionIndex, bool bEnabled, bool bSave, bool bDryRun)
{
    FWork W; W.LODIndex = LODIndex; W.SectionIndex = SectionIndex; W.Enabled = bEnabled; W.Save = bSave; W.DryRun = bDryRun;
    W.R.bSaveRequested = bSave;
    if (SkeletalMeshPath.Len() <= 2048) W.R.Data->SetStringField(TEXT("requested_mesh_path"), SkeletalMeshPath);
    else { W.R.Data->SetBoolField(TEXT("requested_mesh_path_omitted"), true); W.R.Data->SetNumberField(TEXT("requested_mesh_path_UTF16_length"), SkeletalMeshPath.Len()); }
    W.R.Data->SetNumberField(TEXT("requested_lod_index"), LODIndex);
    W.R.Data->SetNumberField(TEXT("requested_section_index"), SectionIndex); W.R.Data->SetBoolField(TEXT("requested_enabled"), bEnabled);
    W.R.Data->SetBoolField(TEXT("dry_run"), bDryRun);
    if (!IsInGameThread() || Running) { W.Fail(TEXT("skeletal_section_thread_or_reentry"), TEXT("Game thread and non-reentrant section operation required.")); return Finish(W); }
    TGuardValue<bool> Scope(Running, true);
    if (!PathSafe(SkeletalMeshPath) || LODIndex < 0 || SectionIndex < 0) { W.Fail(TEXT("skeletal_section_input_invalid"), TEXT("Raw and SDK-expanded mesh object paths must fit NAME_SIZE without embedded NUL; nonnegative indices required.")); return Finish(W); }
    if (!W.Init()) return Finish(W);
    UObject* Loaded = nullptr;
    if (!W.Call(TEXT("load_skeletal_mesh"), false, [&](const auto& Row) { Loaded = UEditorAssetLibrary::LoadAsset(SkeletalMeshPath); Row->SetBoolField(TEXT("nonnull_return"), Loaded != nullptr); })) return Finish(W);
    if (!Live(Loaded)) { W.Fail(TEXT("skeletal_section_load_failed"), TEXT("skeletal_mesh_path must resolve to a live SkeletalMesh asset.")); return Finish(W); }
    if (!W.Bind(Cast<USkeletalMesh>(Loaded))) return Finish(W);
    if (W.Mesh->IsCompiling() && !W.FinishCompilation(false, TEXT("finish_existing_mesh_compilation"))) return Finish(W);
    if (!W.Capture(W.Before)) return Finish(W);
    if (!W.Before.Sections.IsValidIndex(SectionIndex)) { W.Fail(TEXT("skeletal_section_index_invalid"), TEXT("Invalid section index.")); return Finish(W); }
    const auto Selected = W.Before.Sections[SectionIndex];
    if (Selected.Parent != INDEX_NONE) { W.Fail(TEXT("skeletal_section_child_selected"), TEXT("Select the parent section; child chunks follow their parent.")); return Finish(W); }
    if (Selected.Original < 0) { W.Fail(TEXT("skeletal_section_original_id_missing"), TEXT("Missing original section identity; cannot safely persist this edit.")); return Finish(W); }
    W.Original = Selected.Original; W.PreviouslyEnabled = !Selected.Fields.Disabled;
    for (int32 I = 0; I < W.Before.Sections.Num(); ++I) if (I == SectionIndex || W.Before.Sections[I].Parent == SectionIndex)
    { W.Affected.Add(I); W.NeedsChange |= W.Before.Sections[I].Fields.Disabled != !bEnabled; }
    const auto* Existing = W.Before.User.Find(W.Original); W.NeedsChange |= !Existing || Existing->Disabled != !bEnabled;
    W.R.bCompileRequested = !bDryRun && W.NeedsChange;
    W.R.Data->SetNumberField(TEXT("original_data_section_index"), W.Original);
    W.R.Data->SetBoolField(TEXT("would_change"), W.NeedsChange);
    W.R.Data->SetBoolField(TEXT("previously_enabled"), W.PreviouslyEnabled);
    W.R.Data->SetBoolField(TEXT("source_user_data_existed_before"), Existing != nullptr);
    if (Existing) W.R.Data->SetBoolField(TEXT("source_enabled_before"), !Existing->Disabled);
    TArray<TSharedPtr<FJsonValue>> AffectedBefore; for (int32 I : W.Affected) AffectedBefore.Add(MakeShared<FJsonValueNumber>(I));
    W.R.Data->SetArrayField(TEXT("affected_section_indices_before_rebuild"), AffectedBefore);
    if (!bDryRun && W.NeedsChange && !Edit(W)) return Finish(W);
    if (!bDryRun && !W.VerifyRebuilt()) return Finish(W);
    if (!bDryRun && bSave)
    {
        if (!W.PackageOwned()) return Finish(W);
        const bool DirtyBefore = W.Package->IsDirty(); bool Saved = false;
        if (!W.Call(TEXT("save_loaded_mesh"), true, [&](const auto& Row)
        {
            W.R.bSaveAttempted = true; Saved = UEditorAssetLibrary::SaveLoadedAsset(W.Mesh, false); W.R.bSaved |= Saved;
            Row->SetBoolField(TEXT("saved_API_true"), Saved); Row->SetBoolField(TEXT("only_if_dirty"), false);
            Row->SetBoolField(TEXT("dirty_before"), DirtyBefore); W.R.Data->SetBoolField(TEXT("physical_file_write_proven"), false);
        })) return Finish(W);
        if (!Saved) { W.Fail(TEXT("skeletal_section_save_failed"), TEXT("Mesh edit applied but save failed; inspect dirty package before retrying.")); return Finish(W); }
        if (!W.PackageOwned() || !W.VerifyRebuilt()) return Finish(W);
        W.R.Data->SetBoolField(TEXT("target_dirty_after_save"), W.Package->IsDirty());
        if (W.Package->IsDirty()) { W.Fail(TEXT("skeletal_section_dirty_after_save"), TEXT("Save API returned true but current mesh package is dirty; saved history preserved.")); return Finish(W); }
    }
    if (bDryRun) { if (!W.Capture(W.Final) || !(W.Final == W.Before)) { W.Fail(TEXT("skeletal_section_dry_run_changed"), TEXT("Section/source projection changed during dry-run observation.")); return Finish(W); } }
    if (!W.Guard()) return Finish(W);
    W.R.Data->SetObjectField(TEXT("legacy_result"), W.Readback(W.Final));
    W.Verified = W.Guard(); return Finish(W);
}
}
