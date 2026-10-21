#include "Compatibility/UE58/CubelessStateTreeMutationApi.h"
#include "Compatibility/UE58/CubelessStateTreeWork.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::StateTreeMutation
{
namespace
{
using Variable::FResult;
using namespace StateTreeWork;
constexpr int32 MaxStates = 16384, MaxDepth = 64, MaxSnapshotChars = 4 * 1024 * 1024;
struct FContext
{
    FResult& Result;
    UStateTree* Tree = nullptr;
    UStateTreeEditorData* Editor = nullptr;
    FString ObjectPath, BeforeStructure;
    uint32 BeforeHash = 0;
    bool bBeforeDirty = false;
    TArray<TStrongObjectPtr<UObject>> Held;
    TSet<const UObject*> HeldSet;
    explicit FContext(FResult& InResult) : Result(InResult) {}
};
void Init(FResult& R, const TCHAR* Operation)
{
    auto Counts = MakeShared<FJsonObject>();
    for (const TCHAR* Key : { TEXT("asset_load_calls"), TEXT("asset_load_returns"), TEXT("ready_to_run_calls"),
        TEXT("live_dependency_audit_calls"), TEXT("persisted_dependency_audit_calls"), TEXT("registry_rescan_calls"),
        TEXT("structure_capture_calls"), TEXT("path_candidates_visited"), TEXT("explicit_modify_calls"),
        TEXT("add_child_state_calls"), TEXT("add_child_state_returns"), TEXT("selection_write_calls"),
        TEXT("explicit_compile_calls"), TEXT("explicit_compile_returns"), TEXT("explicit_mark_dirty_calls"),
        TEXT("explicit_save_calls"), TEXT("explicit_save_returns"), TEXT("rollback_remove_child_calls"),
        TEXT("rollback_mark_garbage_calls"), TEXT("explicit_mark_state_tree_modified_calls"),
        TEXT("failure_mark_dirty_calls"), TEXT("transaction_scope_calls"), TEXT("runtime_execution_calls") }) { Counts->SetNumberField(Key, 0); }
    R.Data->SetObjectField(TEXT("native_call_counts"), Counts);
    R.Data->SetArrayField(TEXT("compile_save_attempts"), {});
    R.Data->SetStringField(TEXT("operation"), Operation);
    R.Data->SetBoolField(TEXT("implicit_postload_or_readiness_compile_possible"), false);
    R.Data->SetBoolField(TEXT("total_SDK_callback_effects_verified"), false);
    R.Data->SetBoolField(TEXT("runtime_execution_verified"), false);
    R.Data->SetBoolField(TEXT("cold_reload_verified"), false);
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false);
    R.Data->SetBoolField(TEXT("rollback_preserves_unobserved_properties_verified"), false);
}
FResult Fail(FResult R, const FString& Error)
{
    R.bSuccess = false; R.bPostconditionPassed = false;
    R.ErrorCode = TEXT("state_tree_command_failed"); R.Error = Error;
    R.Data->SetStringField(TEXT("message"), Error);
    R.Data->SetBoolField(TEXT("mutation_state_unknown"), !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("partial_mutation"), R.bChanged || !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("workflow_recovery_required"), R.bChanged || !R.bChangeStateKnown);
    R.Data->SetBoolField(TEXT("do_not_retry"), R.bMutationAttempted || !R.bChangeStateKnown);
    R.Data->SetStringField(TEXT("recovery_guidance"), TEXT("Inspect the actual compiler/save/current/conditional-restore prefix. No request replay or atomic rollback is provided."));
    return R;
}
bool Hold(FContext& C, UObject* Object)
{
    if (!Loaded(Object)) { return false; }
    if (!C.HeldSet.Contains(Object))
    {
        if (C.HeldSet.Num() >= MaxStates + 4) { return false; }
        C.HeldSet.Add(Object); C.Held.Emplace(Object);
    }
    return true;
}
bool Current(FContext& C)
{
    return Loaded(C.Tree) && Loaded(C.Tree->GetOutermost()) && Loaded(C.Editor)
        && C.Tree->GetPathName() == C.ObjectPath && C.Tree->EditorData.Get() == C.Editor
        && C.Tree->GetOuter() == C.Tree->GetOutermost();
}
bool SafePath(const UStateTreeState* State)
{
    TSet<const UStateTreeState*> Seen;
    for (const UStateTreeState* Cursor = State; Cursor; Cursor = Cursor->Parent.Get())
    { if (!Loaded(Cursor) || Seen.Num() >= MaxDepth + 1 || Seen.Contains(Cursor)) { return false; } Seen.Add(Cursor); }
    return true;
}
bool Structure(FContext& C, FString& Out)
{
    Count(C.Result, TEXT("structure_capture_calls"));
    if (!Current(C) || C.Editor->SubTrees.Num() > MaxStates) { return false; }
    auto J = MakeShared<FJsonObject>();
    J->SetStringField(TEXT("editor_path"), C.Editor->GetPathName());
    TArray<TSharedPtr<FJsonValue>> Rows; TSet<const UStateTreeState*> Seen;
    TFunction<bool(UStateTreeState*, int32)> Visit = [&](UStateTreeState* State, int32 Depth)
    {
        if (!State) { auto Row = MakeShared<FJsonObject>(); Row->SetBoolField(TEXT("null_slot"), true); Rows.Add(MakeShared<FJsonValueObject>(Row)); return Rows.Num() <= MaxStates; }
        if (Depth > MaxDepth || Seen.Contains(State) || Seen.Num() >= MaxStates || !Hold(C, State)
            || State->Children.Num() > MaxStates || State->GetTypedOuter<UStateTree>() != C.Tree || !SafePath(State)) { return false; }
        Seen.Add(State);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), State->GetPath());
        Row->SetStringField(TEXT("object_path"), State->GetPathName()); Row->SetStringField(TEXT("name"), State->Name.ToString());
        Row->SetStringField(TEXT("id"), State->ID.ToString(EGuidFormats::DigitsWithHyphensLower));
        Row->SetStringField(TEXT("parent"), State->Parent ? State->Parent->GetPathName() : FString());
        Row->SetNumberField(TEXT("type"), static_cast<int32>(State->Type)); Row->SetNumberField(TEXT("selection"), static_cast<int32>(State->SelectionBehavior));
        Row->SetBoolField(TEXT("enabled"), State->bEnabled); Row->SetNumberField(TEXT("children"), State->Children.Num());
        Row->SetNumberField(TEXT("tasks"), State->Tasks.Num()); Row->SetNumberField(TEXT("conditions"), State->EnterConditions.Num());
        Row->SetNumberField(TEXT("transitions"), State->Transitions.Num()); Rows.Add(MakeShared<FJsonValueObject>(Row));
        if (Rows.Num() > MaxStates) { return false; }
        for (UStateTreeState* Child : State->Children) { if (!Visit(Child, Depth + 1)) { return false; } }
        return true;
    };
    for (UStateTreeState* Root : C.Editor->SubTrees) { if (!Visit(Root, 0)) { return false; } }
    J->SetArrayField(TEXT("states"), Rows); J->SetNumberField(TEXT("global_tasks"), C.Editor->GlobalTasks.Num()); J->SetNumberField(TEXT("evaluators"), C.Editor->Evaluators.Num());
    FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Out));
    return Out.Len() <= MaxSnapshotChars;
}
bool Load(FContext& C, const FString& Raw)
{
    FString Path = FPackageName::ExportTextPathToObjectPath(Raw.TrimStartAndEnd()); Path.TrimQuotesInline(); Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (Path.StartsWith(TEXT("/")) && !Path.Contains(TEXT("."))) { Path += TEXT(".") + FPackageName::GetShortName(Path); }
    const FString Package = FPackageName::ObjectPathToPackageName(Path);
    if (Path.IsEmpty() || Path.Len() >= NAME_SIZE || Forbidden(Path) || !FPackageName::IsValidObjectPath(Path)
        || !(Package == TEXT("/Game") || Package.StartsWith(TEXT("/Game/")))) { C.Result.Error = TEXT("StateTree mutation requires a valid project /Game object path without MCP-only tokens"); return false; }
    C.Result.Data->SetStringField(TEXT("requested_asset_path"), Path);
    const UStateTree* BeforeTree = FindObject<UStateTree>(nullptr, *Path);
    const bool bBeforeLoaded = Loaded(BeforeTree);
    const uint32 BeforeHash = bBeforeLoaded ? BeforeTree->LastCompiledEditorDataHash : 0;
    const bool bBeforeDirty = bBeforeLoaded && BeforeTree->GetOutermost()->IsDirty();
    C.Result.Data->SetBoolField(TEXT("already_loaded_before_call"), bBeforeLoaded);
    if (bBeforeLoaded)
    {
        C.Result.Data->SetNumberField(TEXT("compiled_hash_before_load"), BeforeHash);
        C.Result.Data->SetBoolField(TEXT("dirty_before_load"), bBeforeDirty);
    }
    C.Result.Stage = TEXT("load_asset"); C.Result.bChangeStateKnown = false;
    C.Result.Data->SetBoolField(TEXT("implicit_postload_or_readiness_compile_possible"), true);
    Count(C.Result, TEXT("asset_load_calls")); C.Tree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(Path)); Count(C.Result, TEXT("asset_load_returns"));
    if (!Loaded(C.Tree) || !Hold(C, C.Tree)) { C.Result.Error = TEXT("StateTree not found or not readable"); return false; }
    const bool bLoadChanged = bBeforeLoaded && (BeforeHash != C.Tree->LastCompiledEditorDataHash || bBeforeDirty != C.Tree->GetOutermost()->IsDirty());
    C.Result.Data->SetBoolField(TEXT("observed_load_hash_or_dirty_change"), bLoadChanged);
    C.Result.Data->SetNumberField(TEXT("compiled_hash_immediately_after_load"), C.Tree->LastCompiledEditorDataHash);
    C.Result.Data->SetBoolField(TEXT("dirty_immediately_after_load"), C.Tree->GetOutermost()->IsDirty());
    C.Result.bChanged = bLoadChanged; C.Result.bChangeStateKnown = bLoadChanged;
    C.ObjectPath = C.Tree->GetPathName(); C.Editor = Cast<UStateTreeEditorData>(C.Tree->EditorData.Get());
    if (!Hold(C, C.Tree->GetOutermost()) || !Hold(C, C.Editor)) { C.Result.Error = TEXT("StateTree has no readable editor data"); return false; }
    C.BeforeHash = C.Tree->LastCompiledEditorDataHash; C.bBeforeDirty = C.Tree->GetOutermost()->IsDirty();
    C.Result.Data->SetStringField(TEXT("asset_path"), C.ObjectPath); C.Result.Data->SetNumberField(TEXT("compiled_hash_after_load"), C.BeforeHash);
    C.Result.Data->SetBoolField(TEXT("dirty_after_load"), C.bBeforeDirty);
    if (!Structure(C, C.BeforeStructure)) { C.Result.Error = TEXT("StateTree structure is unsafe or exceeds the finite hierarchy bounds"); return false; }
    C.Result.Data->SetStringField(TEXT("structure_before_json"), C.BeforeStructure);
    return true;
}
UStateTreeState* Find(FContext& C, const FString& Path)
{
    TArray<FString> Segments; Path.ParseIntoArray(Segments, TEXT("/"), true);
    if (Segments.IsEmpty() || Segments.Num() > MaxDepth + 1) { return nullptr; }
    for (const FString& Segment : Segments) { if (Segment.IsEmpty() || Segment.Len() > 64) { return nullptr; } }
    UStateTreeState* CurrentState = nullptr; int32 Candidates = 0;
    const auto Select = [&](const TArray<TObjectPtr<UStateTreeState>>& States, const FString& Segment, UStateTreeState*& Match)
    {
        Match = nullptr;
        for (UStateTreeState* State : States)
        {
            Count(C.Result, TEXT("path_candidates_visited")); if (++Candidates > MaxStates) { return false; }
            if (!State) { continue; }
            if (!Loaded(State) || State->GetTypedOuter<UStateTree>() != C.Tree) { return false; }
            if (State->Name.ToString().Equals(Segment, ESearchCase::IgnoreCase)) { if (Match) { return false; } Match = State; }
        }
        return Match != nullptr;
    };
    if (!Select(C.Editor->SubTrees, Segments[0], CurrentState)) { return nullptr; }
    for (int32 Index = 1; Index < Segments.Num(); ++Index)
    { UStateTreeState* Next = nullptr; if (!Select(CurrentState->Children, Segments[Index], Next)) { return nullptr; } CurrentState = Next; }
    return SafePath(CurrentState) ? CurrentState : nullptr;
}
bool CurrentParent(FContext& C, UStateTreeState* Parent, const FString& ExpectedPath)
{
    if (!Current(C) || !Loaded(Parent) || !SafePath(Parent) || Parent->GetTypedOuter<UStateTree>() != C.Tree
        || Parent->GetPath() != ExpectedPath || Find(C, ExpectedPath) != Parent) { return false; }
    // SafePath bounded the parent chain before these direct ownership/membership checks.
    for (UStateTreeState* Cursor = Parent; Cursor; Cursor = Cursor->Parent.Get())
    {
        if (!Loaded(Cursor) || Cursor->GetTypedOuter<UStateTree>() != C.Tree) { return false; }
        UStateTreeState* Ancestor = Cursor->Parent.Get();
        if (Ancestor)
        {
            if (!Loaded(Ancestor) || Cursor->GetOuter() != Ancestor || Ancestor->Children.Num() > MaxStates
                || !Ancestor->Children.Contains(Cursor)) { return false; }
        }
        else if (Cursor->GetOuter() != C.Editor || !C.Editor->SubTrees.Contains(Cursor)) { return false; }
    }
    return true;
}
bool CurrentChild(FContext& C, UStateTreeState* Parent, UStateTreeState* Child,
    const FString& ExpectedParentPath, const FString& ExpectedChildPath, FName ExpectedName,
    EStateTreeStateType ExpectedType, EStateTreeStateSelectionBehavior ExpectedSelection, bool bCheckSelection = true)
{
    return CurrentParent(C, Parent, ExpectedParentPath) && Loaded(Child)
        && Child->GetTypedOuter<UStateTree>() == C.Tree && Child->GetOuter() == Parent && Child->Parent.Get() == Parent
        && Parent->Children.Num() <= MaxStates && Parent->Children.Contains(Child) && SafePath(Child)
        && Child->Name == ExpectedName && Child->Type == ExpectedType
        && (!bCheckSelection || Child->SelectionBehavior == ExpectedSelection)
        && Child->GetPath() == ExpectedChildPath && Find(C, ExpectedChildPath) == Child;
}
bool ParseType(const FString& Text, EStateTreeStateType& Value)
{
    if (Text.Equals(TEXT("state"), ESearchCase::IgnoreCase)) { Value = EStateTreeStateType::State; return true; }
    if (Text.Equals(TEXT("group"), ESearchCase::IgnoreCase)) { Value = EStateTreeStateType::Group; return true; }
    if (Text.Equals(TEXT("subtree"), ESearchCase::IgnoreCase)) { Value = EStateTreeStateType::Subtree; return true; }
    return false;
}
bool ParseSelection(const FString& Text, EStateTreeStateSelectionBehavior& Value)
{
    if (Text.Equals(TEXT("none"), ESearchCase::IgnoreCase)) { Value = EStateTreeStateSelectionBehavior::None; return true; }
    if (Text.Equals(TEXT("enter"), ESearchCase::IgnoreCase)) { Value = EStateTreeStateSelectionBehavior::TryEnterState; return true; }
    if (Text.Equals(TEXT("children_in_order"), ESearchCase::IgnoreCase)) { Value = EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder; return true; }
    return false;
}
bool CompileSave(FContext& C, bool bSave, const TCHAR* Role, FString& Error, bool bAudit = true,
    const TFunction<bool()>& MutationIdentity = {})
{
    auto& R = C.Result;
    const auto CurrentTarget = [&]() { return Current(C) && (!MutationIdentity || MutationIdentity()); };
    auto Attempt = MakeShared<FJsonObject>(); Attempt->SetStringField(TEXT("role"), Role); Attempt->SetBoolField(TEXT("save_requested"), bSave);
    auto Attempts = R.Data->GetArrayField(TEXT("compile_save_attempts")); Attempts.Add(MakeShared<FJsonValueObject>(Attempt)); R.Data->SetArrayField(TEXT("compile_save_attempts"), Attempts);
    if (!CurrentTarget()) { R.bChangeStateKnown = false; Error = TEXT("Current target/editor/state identity is unavailable before compiler"); return false; }
    R.bCompileRequested = true; R.bCompileAttempted = true; R.bSaveRequested |= bSave; R.bMutationAttempted = true;
    const bool bKnownStructuralChange = R.bChangeStateKnown && R.bChanged;
    R.bChangeStateKnown = false; Count(R, TEXT("explicit_compile_calls")); FStateTreeCompilerLog Log;
    const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(C.Tree, Log); Count(R, TEXT("explicit_compile_returns"));
    R.Compiled = bCompiled; Attempt->SetBoolField(TEXT("compiled"), bCompiled);
    TArray<TSharedPtr<FJsonValue>> Messages;
    for (const TSharedRef<FTokenizedMessage>& Message : Log.ToTokenizedMessages())
    { auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("severity"), FTokenizedMessage::GetSeverityText(Message->GetSeverity()).ToString()); Row->SetStringField(TEXT("message"), Message->ToText().ToString()); Messages.Add(MakeShared<FJsonValueObject>(Row)); }
    Attempt->SetArrayField(TEXT("compiler_messages"), Messages);
    if (FString(Role) == TEXT("primary")) { R.Data->SetArrayField(TEXT("compiler_messages"), Messages); R.Data->SetBoolField(TEXT("compiled"), bCompiled); R.Data->SetBoolField(TEXT("save_requested"), bSave); }
    if (!CurrentTarget()) { R.bChangeStateKnown = false; Error = TEXT("Target/editor/state identity changed during compilation"); return false; }
    const bool bReady = ReadReady(C.Tree, R, TEXT("compiled_readiness")); Attempt->SetBoolField(TEXT("ready_to_run"), bReady);
    if (!CurrentTarget()) { R.bChangeStateKnown = false; Error = TEXT("Target/editor/state identity changed during readiness"); return false; }
    FString AfterStructure;
    if (!Structure(C, AfterStructure)) { R.bChangeStateKnown = false; Error = TEXT("Compiled structure is not completely observable"); return false; }
    R.Data->SetStringField(TEXT("structure_after_compile_json"), AfterStructure);
    const bool bObserved = AfterStructure != C.BeforeStructure || C.Tree->LastCompiledEditorDataHash != C.BeforeHash || C.Tree->GetOutermost()->IsDirty() != C.bBeforeDirty;
    if (bObserved || bKnownStructuralChange) { R.bChanged = true; R.bChangeStateKnown = true; }
    if (!bCompiled) { Error = TEXT("StateTree compilation failed"); return false; }
    if (bAudit && !AuditLive(C.Tree, R, TEXT("compiled_live_dependency_audit"))) { Error = TEXT("Compiled StateTree live dependency audit failed"); return false; }
    if (!CurrentTarget()) { R.bChangeStateKnown = false; Error = TEXT("Target/editor/state identity changed during compiled dependency audit"); return false; }
    if (bSave)
    {
        Count(R, TEXT("explicit_mark_dirty_calls")); C.Tree->MarkPackageDirty();
        if (!CurrentTarget()) { R.bChangeStateKnown = false; Error = TEXT("Target/editor/state identity changed while marking the package dirty; no save was dispatched"); return false; }
        R.bSaveAttempted = true;
        Count(R, TEXT("explicit_save_calls")); const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(C.Tree, false); Count(R, TEXT("explicit_save_returns"));
        Attempt->SetBoolField(TEXT("save_attempted"), true); Attempt->SetBoolField(TEXT("save_returned"), bSaved);
        if (!CurrentTarget()) { R.bChangeStateKnown = false; Error = TEXT("Target/editor/state identity became unobservable during save"); return false; }
        R.bSaved = bSaved && !C.Tree->GetOutermost()->IsDirty(); Attempt->SetBoolField(TEXT("saved"), R.bSaved);
        Attempt->SetBoolField(TEXT("package_dirty_after"), C.Tree->GetOutermost()->IsDirty()); R.Data->SetBoolField(TEXT("saved"), R.bSaved);
        if (!R.bSaved) { Error = TEXT("StateTree compiled but a clean persisted save could not be confirmed"); return false; }
    }
    else { Attempt->SetBoolField(TEXT("save_attempted"), false); Attempt->SetBoolField(TEXT("saved"), false); R.Data->SetBoolField(TEXT("saved"), false); }
    return true;
}
FResult Restore(FContext& C, UStateTreeState* Parent, UStateTreeState* NewState, const FString& OriginalError,
    const TFunction<bool()>& ChildIdentity, const TFunction<bool()>& ParentIdentity)
{
    auto& R = C.Result; R.Data->SetStringField(TEXT("primary_failure"), OriginalError); R.Stage = TEXT("conditional_structural_restore");
    auto Receipt = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("structural_restore"), Receipt); Receipt->SetBoolField(TEXT("attempted"), false);
    if (!Current(C) || !ChildIdentity())
    { R.bChangeStateKnown = false; return Fail(R, OriginalError + TEXT("; exact new child identity is unavailable, so no structural restore was replayed")); }
    Receipt->SetBoolField(TEXT("attempted"), true); Count(R, TEXT("rollback_remove_child_calls"));
    const int32 Removed = Parent->Children.Remove(NewState); Receipt->SetNumberField(TEXT("removed_child_slots"), Removed);
    Count(R, TEXT("rollback_mark_garbage_calls")); NewState->MarkAsGarbage();
    if (!Current(C) || !ParentIdentity()) { R.bChangeStateKnown = false; return Fail(R, OriginalError + TEXT("; original parent identity changed after conditional removal; no restore compiler was dispatched")); }
    Count(R, TEXT("explicit_mark_state_tree_modified_calls")); UStateTreeEditingSubsystem::MarkAsModified(C.Tree);
    FString RestoreError; const bool bRestored = CompileSave(C, true, TEXT("restoration"), RestoreError, false, ParentIdentity);
    Receipt->SetBoolField(TEXT("compile_save_succeeded"), bRestored); Receipt->SetStringField(TEXT("error"), RestoreError);
    FString After;
    const bool bObserved = bRestored && Current(C) && ParentIdentity() && R.Data->GetBoolField(TEXT("ready_to_run")) && Structure(C, After) && After == C.BeforeStructure
        && C.Tree->LastCompiledEditorDataHash == C.BeforeHash && !C.Tree->GetOutermost()->IsDirty();
    Receipt->SetBoolField(TEXT("structure_hash_and_clean_saved_observation_restored"), bObserved);
    if (!After.IsEmpty()) { Receipt->SetStringField(TEXT("after_structure_json"), After); }
    R.Data->SetBoolField(TEXT("structural_change_rolled_back"), bObserved);
    R.bChanged = !bObserved; R.bChangeStateKnown = false;
    Receipt->SetBoolField(TEXT("total_change_restoration_verified"), false);
    Receipt->SetBoolField(TEXT("unobserved_properties_and_SDK_callback_effects_restored_verified"), false);
    const bool bCanMarkOriginalDirty = !bObserved && Current(C) && ParentIdentity();
    Receipt->SetBoolField(TEXT("failure_mark_dirty_skipped_target_or_parent_identity"), !bObserved && !bCanMarkOriginalDirty);
    if (bCanMarkOriginalDirty) { Count(R, TEXT("failure_mark_dirty_calls")); C.Tree->GetOutermost()->SetDirtyFlag(true); }
    return Fail(R, OriginalError);
}
FResult Done(FResult R)
{ R.bSuccess = true; R.bPostconditionPassed = true; R.Stage = TEXT("native_readback"); return R; }
}

Variable::FResult Add(const FString& AssetPath, const FString& RawStateName, const FString& RawParentPath,
    const FString& RawStateType, const FString& RawSelectionBehavior, bool bDryRun)
{
    FResult R; Init(R, TEXT("add_state_tree_state")); FContext C(R);
    if (!IsEditorGameThread()) { return Fail(R, TEXT("StateTree state authoring requires the editor game thread")); }
    TUniquePtr<FScopedTransaction> Transaction;
    if (!bDryRun) { Count(R, TEXT("transaction_scope_calls")); Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("Cubeless", "StateTreeAdd", "Cubeless add StateTree state")); }
    if (!Load(C, AssetPath)) { return Fail(R, R.Error); }
    if (!bDryRun && IsPlaySessionActive()) { return Fail(R, TEXT("Cannot modify StateTree assets while PIE/SIE is active")); }
    const FString StateName = RawStateName.TrimStartAndEnd(), ParentPath = RawParentPath.TrimStartAndEnd();
    const FString TypeText = RawStateType.TrimStartAndEnd(), SelectionText = RawSelectionBehavior.TrimStartAndEnd();
    bool bControl = false; for (const TCHAR Character : StateName) { bControl |= FChar::IsControl(Character); }
    FText Reason;
    if (StateName.IsEmpty() || StateName.Len() > 64 || StateName.Contains(TEXT(".")) || StateName.Contains(TEXT("/")) || StateName.Contains(TEXT("\\"))
        || bControl || StateName.Equals(TEXT("None"), ESearchCase::IgnoreCase) || !FName::IsValidXName(StateName, TEXT("/.\\"), &Reason) || Forbidden(StateName))
    { return Fail(R, TEXT("state_name must be a valid non-None FName of1-64 characters without control characters, '.', '/', or backslash")); }
    UStateTreeState* Parent = Find(C, ParentPath);
    if (!Parent) { return Fail(R, TEXT("Parent state path not found, ambiguous or beyond the bounded hierarchy scope")); }
    const FString PinnedParentPath = Parent->GetPath();
    EStateTreeStateType StateType; EStateTreeStateSelectionBehavior Selection;
    if (!ParseType(TypeText, StateType) || !ParseSelection(SelectionText, Selection)) { return Fail(R, TEXT("state_type must be state/group/subtree; selection_behavior must be none/enter/children_in_order")); }
    const FName StateFName(*StateName);
    for (UStateTreeState* Child : Parent->Children)
    {
        if (Child && Child->Name == StateFName)
        {
            if (Child->Type != StateType || Child->SelectionBehavior != Selection) { return Fail(R, TEXT("State already exists with different settings")); }
            R.Data->SetStringField(TEXT("state_path"), Child->GetPath()); R.Data->SetBoolField(TEXT("created"), false); R.Data->SetBoolField(TEXT("already_existed"), true);
            return Done(R);
        }
    }
    const FString PlannedPath = PinnedParentPath + TEXT("/") + StateName;
    TArray<FString> PlannedSegments; PlannedPath.ParseIntoArray(PlannedSegments, TEXT("/"), true);
    if (PlannedSegments.Num() > MaxDepth + 1) { return Fail(R, TEXT("Planned child path exceeds the bounded depth0-64 hierarchy; no state was created")); }
    R.Data->SetStringField(TEXT("parent_path"), PinnedParentPath); R.Data->SetStringField(TEXT("state_path"), PlannedPath);
    R.Data->SetStringField(TEXT("state_type"), TypeText.ToLower()); R.Data->SetStringField(TEXT("selection_behavior"), SelectionText.ToLower());
    R.Data->SetBoolField(TEXT("dry_run"), bDryRun); R.Data->SetBoolField(TEXT("created"), false);
    if (bDryRun) { return Done(R); }
    R.Stage = TEXT("mutation_preconditions");
    if (!ReadReady(C.Tree, R, TEXT("pre_add_readiness")) || !CurrentParent(C, Parent, PinnedParentPath)) { R.bChangeStateKnown = false; return Fail(R, TEXT("StateTree readiness or original parent identity is unavailable; inspect or compile before adding a state")); }
    if (C.Tree->GetOutermost()->IsDirty()) { return Fail(R, TEXT("StateTree has unsaved changes; save or discard them before adding a state")); }
    if (!AuditPersisted(C.Tree, R)) { return Fail(R, TEXT("StateTree pre-add persisted/live dependency independence could not be confirmed")); }
    if (!CurrentParent(C, Parent, PinnedParentPath)) { R.bChangeStateKnown = false; return Fail(R, TEXT("Target/editor/parent identity changed during pre-add dependency audit")); }
    C.BeforeHash = C.Tree->LastCompiledEditorDataHash; C.bBeforeDirty = C.Tree->GetOutermost()->IsDirty();
    if (!Structure(C, C.BeforeStructure) || !CurrentParent(C, Parent, PinnedParentPath)) { return Fail(R, TEXT("Current parent/structure changed before explicit mutation")); }
    R.Data->SetStringField(TEXT("structure_before_mutation_json"), C.BeforeStructure);
    R.Stage = TEXT("add_child_state"); R.bMutationAttempted = true; R.bChangeStateKnown = false;
    for (UObject* Object : {static_cast<UObject*>(C.Tree), static_cast<UObject*>(C.Editor), static_cast<UObject*>(Parent)})
    { Count(R, TEXT("explicit_modify_calls")); Object->Modify(); if (!CurrentParent(C, Parent, PinnedParentPath)) { R.bChangeStateKnown = false; return Fail(R, TEXT("Target/editor/parent identity became unavailable during Modify callbacks")); } }
    Count(R, TEXT("add_child_state_calls")); UStateTreeState& NewStateRef = Parent->AddChildState(StateFName, StateType); Count(R, TEXT("add_child_state_returns"));
    UStateTreeState* NewState = &NewStateRef;
    if (!Hold(C, NewState) || !CurrentChild(C, Parent, NewState, PinnedParentPath, PlannedPath, StateFName, StateType, Selection, false)) { R.bChangeStateKnown = false; return Fail(R, TEXT("New child state ownership/path/type is unobservable after AddChildState; no selection write was dispatched")); }
    R.bChanged = true; R.bChangeStateKnown = true; R.Data->SetBoolField(TEXT("created"), true);
    Count(R, TEXT("selection_write_calls")); NewState->SelectionBehavior = Selection; Count(R, TEXT("explicit_modify_calls")); NewState->Modify();
    const TFunction<bool()> ParentIdentity = [&]() { return CurrentParent(C, Parent, PinnedParentPath); };
    const TFunction<bool()> ChildIdentity = [&]() { return CurrentChild(C, Parent, NewState, PinnedParentPath, PlannedPath, StateFName, StateType, Selection); };
    if (!ChildIdentity()) { R.bChangeStateKnown = false; return Fail(R, TEXT("New child ownership/path/settings changed during its Modify callback; no compile/save or structural restore was dispatched")); }
    R.Stage = TEXT("compile_save_added_state"); FString Error;
    if (!CompileSave(C, true, TEXT("primary"), Error, true, ChildIdentity)) { return Restore(C, Parent, NewState, Error, ChildIdentity, ParentIdentity); }
    R.Stage = TEXT("post_add_dependency_audit");
    if (!AuditPersisted(C.Tree, R)) { return Fail(R, TEXT("StateTree dependency audit failed after adding and saving; persisted partial state is retained")); }
    if (!ChildIdentity()) { R.bChangeStateKnown = false; return Fail(R, TEXT("Target/editor/child identity changed during post-add dependency audit; saved prefix is retained")); }
    UStateTreeState* Readback = Find(C, PlannedPath);
    if (!Readback || Readback != NewState || !ChildIdentity())
    { return Restore(C, Parent, NewState, TEXT("StateTree state readback failed after compile/save"), ChildIdentity, ParentIdentity); }
    R.Data->SetStringField(TEXT("state_id"), Readback->ID.ToString(EGuidFormats::DigitsWithHyphensLower));
    R.Data->SetStringField(TEXT("state_path"), Readback->GetPath()); R.Data->SetBoolField(TEXT("dry_run"), false);
    R.bChanged = true; R.bChangeStateKnown = true;
    return Done(R);
}

Variable::FResult Compile(const FString& AssetPath, bool bSave)
{
    FResult R; Init(R, TEXT("compile_state_tree")); FContext C(R);
    if (!IsEditorGameThread()) { return Fail(R, TEXT("StateTree compilation requires the editor game thread")); }
    Count(R, TEXT("transaction_scope_calls")); FScopedTransaction Transaction(NSLOCTEXT("Cubeless", "StateTreeCompile", "Cubeless compile StateTree"));
    if (IsPlaySessionActive()) { return Fail(R, TEXT("Cannot compile or save StateTree assets while PIE/SIE is active")); }
    if (!Load(C, AssetPath)) { return Fail(R, R.Error); }
    R.Stage = TEXT("pre_compile_dependency_audit");
    if (!AuditLive(C.Tree, R, TEXT("pre_compile_live_dependency_audit"))) { return Fail(R, TEXT("StateTree live dependency independence could not be confirmed before explicit compile")); }
    R.Stage = TEXT("compile_save_state_tree"); FString Error;
    if (!CompileSave(C, bSave, TEXT("primary"), Error)) { return Fail(R, Error); }
    if (bSave)
    {
        R.Stage = TEXT("post_compile_dependency_audit");
        if (!AuditPersisted(C.Tree, R)) { return Fail(R, TEXT("StateTree persisted dependency audit failed after compile/save; actual partial prefix is retained")); }
        if (!Current(C)) { R.bChangeStateKnown = false; return Fail(R, TEXT("Target/editor data identity changed during post-compile dependency audit; actual saved prefix is retained")); }
    }
    return Done(R);
}
}
