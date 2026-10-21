// UE56 StateTree bounded asset listing and hierarchy observation, using installed UE58 public APIs.
#include "Compatibility/UE58/CubelessStateTreeReadApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeSchema.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::StateTreeRead
{
namespace
{
using Variable::FResult;

void Init(FResult& Result, const TCHAR* Operation)
{
    auto Counts = MakeShared<FJsonObject>();
    for (const TCHAR* Key : { TEXT("registry_get_assets_calls"), TEXT("registry_assets_returned"),
        TEXT("asset_load_calls"), TEXT("asset_load_returns"), TEXT("ready_to_run_calls"),
        TEXT("states_serialized"), TEXT("state_projection_attempts"), TEXT("state_get_path_calls"), TEXT("explicit_modify_calls"),
        TEXT("explicit_compile_calls"), TEXT("explicit_save_calls"), TEXT("runtime_execution_calls") })
    {
        Counts->SetNumberField(Key, 0);
    }
    Result.Data->SetObjectField(TEXT("native_call_counts"), Counts);
    Result.Data->SetStringField(TEXT("operation"), Operation);
    Result.Data->SetBoolField(TEXT("explicit_authoring_read_only"), true);
    Result.Data->SetBoolField(TEXT("observation_incomplete"), false);
    Result.Data->SetBoolField(TEXT("implicit_postload_possible"), false);
    Result.Data->SetBoolField(TEXT("implicit_state_tree_compile_possible"), false);
    Result.Data->SetBoolField(TEXT("total_SDK_load_or_compile_side_effects_verified"), false);
    Result.Data->SetBoolField(TEXT("runtime_execution_verified"), false);
    Result.Data->SetStringField(TEXT("change_state_scope"),
        TEXT("No explicit authoring, compiler, saver or runtime executor is invoked. UE58 LoadAsset/PostLoad and IsReadyToRun may invoke the StateTree compiler internally; unknown total changes are not reported as known unchanged."));
}

void Count(FResult& Result, const TCHAR* Key, int32 Amount = 1)
{
    const auto Counts = Result.Data->GetObjectField(TEXT("native_call_counts"));
    Counts->SetNumberField(Key, Counts->GetNumberField(Key) + Amount);
}

FResult Fail(FResult Result, const FString& Message)
{
    Result.bSuccess = false;
    Result.ErrorCode = TEXT("state_tree_command_failed");
    Result.Error = Message;
    Result.Data->SetStringField(TEXT("message"), Message);
    Result.Data->SetBoolField(TEXT("explicit_authoring_mutation_performed"), false);
    return Result;
}

bool IsDependencyToken(const FString& Value)
{
    for (const TCHAR* Token : { TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"),
        TEXT("CubelessControlRigTools") })
    {
        if (Value.Contains(Token, ESearchCase::IgnoreCase)) { return true; }
    }
    const FString Package = FPackageName::ObjectPathToPackageName(Value);
    for (const TCHAR* Plugin : { TEXT("CubelessMCP_UE58"), TEXT("ModelContextProtocol"),
        TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin"), TEXT("EditorScriptingUtilities") })
    {
        if (Value.StartsWith(FString::Printf(TEXT("/%s/"), Plugin), ESearchCase::IgnoreCase)
            || Value.Equals(FString::Printf(TEXT("/%s"), Plugin), ESearchCase::IgnoreCase)) { return true; }
    }
    for (const TCHAR* Module : { TEXT("CubelessMCPUE58Editor"), TEXT("ModelContextProtocol"),
        TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin"), TEXT("EditorScriptingUtilities") })
    {
        if (Package.Equals(FString::Printf(TEXT("/Script/%s"), Module), ESearchCase::IgnoreCase)) { return true; }
    }
    return false;
}

bool IsInt32Value(int64 Value)
{
    return Value >= TNumericLimits<int32>::Min() && Value <= TNumericLimits<int32>::Max();
}

bool Loaded(const UObject* Object)
{
    return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects
        | RF_BeginDestroyed | RF_FinishDestroyed);
}

FString NormalizeDirectory(FString Directory)
{
    Directory.TrimStartAndEndInline();
    Directory.TrimQuotesInline();
    Directory.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (Directory.Len() > 1 && Directory.EndsWith(TEXT("/"))) { Directory.LeftChopInline(1); }
    return Directory;
}

FString NormalizeAssetPath(FString AssetPath)
{
    AssetPath = FPackageName::ExportTextPathToObjectPath(AssetPath).TrimStartAndEnd();
    AssetPath.TrimQuotesInline();
    AssetPath.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (AssetPath.StartsWith(TEXT("/")) && !AssetPath.Contains(TEXT(".")))
    {
        AssetPath += TEXT(".") + FPackageName::GetShortName(AssetPath);
    }
    return AssetPath;
}

FString StateType(EStateTreeStateType Type)
{
    switch (Type)
    {
    case EStateTreeStateType::State: return TEXT("state");
    case EStateTreeStateType::Group: return TEXT("group");
    case EStateTreeStateType::Linked: return TEXT("linked");
    case EStateTreeStateType::LinkedAsset: return TEXT("linked_asset");
    case EStateTreeStateType::Subtree: return TEXT("subtree");
    default: return TEXT("unknown");
    }
}

FString Selection(EStateTreeStateSelectionBehavior Behavior)
{
    switch (Behavior)
    {
    case EStateTreeStateSelectionBehavior::None: return TEXT("none");
    case EStateTreeStateSelectionBehavior::TryEnterState: return TEXT("enter");
    case EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder: return TEXT("children_in_order");
    case EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandom: return TEXT("children_at_random");
    case EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility: return TEXT("highest_utility");
    case EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility: return TEXT("weighted_random_utility");
    case EStateTreeStateSelectionBehavior::TryFollowTransitions: return TEXT("follow_transitions");
    default: return TEXT("unknown");
    }
}

// GetPath follows Parent without a cycle/depth guard in UE58. Check that chain first.
bool CanReadStatePath(const UStateTreeState* State)
{
    TSet<const UStateTreeState*> Parents;
    for (const UStateTreeState* Cursor = State; Cursor; Cursor = Cursor->Parent.Get())
    {
        if (!Loaded(Cursor) || Parents.Contains(Cursor) || Parents.Num() >= 65) { return false; }
        Parents.Add(Cursor);
    }
    return true;
}

TSharedPtr<FJsonObject> StateJson(const UStateTreeState* State, int32& StateCount, int32 MaxStates,
    int32 Depth, TSet<const UStateTreeState*>& Visited, bool& bTruncated, int32& ProjectionAttempts,
    int32 MaxProjectionAttempts, FResult& Result)
{
    auto Json = MakeShared<FJsonObject>();
    ++ProjectionAttempts;
    Count(Result, TEXT("state_projection_attempts"));
    if (!State || StateCount >= MaxStates || Depth > 64 || Visited.Contains(State))
    {
        bTruncated = true;
        Json->SetBoolField(TEXT("truncated"), true);
        return Json;
    }
    if (!Loaded(State) || !CanReadStatePath(State))
    {
        bTruncated = true;
        Result.Data->SetBoolField(TEXT("observation_incomplete"), true);
        Json->SetBoolField(TEXT("truncated"), true);
        Json->SetStringField(TEXT("observation_error"), TEXT("State or parent path is unreadable, cyclic or beyond depth 64."));
        return Json;
    }
    Visited.Add(State);
    ++StateCount;
    Count(Result, TEXT("states_serialized"));
    Json->SetStringField(TEXT("name"), State->Name.ToString());
    Count(Result, TEXT("state_get_path_calls"));
    Json->SetStringField(TEXT("path"), State->GetPath());
    Json->SetStringField(TEXT("id"), State->ID.ToString(EGuidFormats::DigitsWithHyphensLower));
    Json->SetStringField(TEXT("type"), StateType(State->Type));
    Json->SetStringField(TEXT("selection_behavior"), Selection(State->SelectionBehavior));
    Json->SetBoolField(TEXT("enabled"), State->bEnabled);
    Json->SetNumberField(TEXT("task_count"), State->Tasks.Num());
    Json->SetNumberField(TEXT("enter_condition_count"), State->EnterConditions.Num());
    Json->SetNumberField(TEXT("transition_count"), State->Transitions.Num());
    TArray<TSharedPtr<FJsonValue>> Children;
    for (const UStateTreeState* Child : State->Children)
    {
        if (StateCount >= MaxStates) { bTruncated = true; break; }
        if (ProjectionAttempts >= MaxProjectionAttempts)
        {
            bTruncated = true;
            Result.Data->SetBoolField(TEXT("observation_incomplete"), true);
            Result.Data->SetBoolField(TEXT("projection_attempt_limit_reached"), true);
            break;
        }
        Children.Add(MakeShared<FJsonValueObject>(StateJson(Child, StateCount, MaxStates, Depth + 1, Visited, bTruncated,
            ProjectionAttempts, MaxProjectionAttempts, Result)));
    }
    Json->SetArrayField(TEXT("children"), Children);
    Json->SetNumberField(TEXT("child_count"), State->Children.Num());
    return Json;
}
}

Variable::FResult List(const FString& RawDirectory, bool bRecursive, int64 MaxResultsValue, int64 OffsetValue)
{
    FResult Result;
    Init(Result, TEXT("list_state_trees"));
    if (!IsEditorGameThread()) { return Fail(Result, TEXT("StateTree listing requires the editor game thread.")); }
    if (!IsInt32Value(MaxResultsValue)) { return Fail(Result, TEXT("'max_results' must be a finite 32-bit integer")); }
    if (!IsInt32Value(OffsetValue)) { return Fail(Result, TEXT("'offset' must be a finite 32-bit integer")); }
    const FString Directory = NormalizeDirectory(RawDirectory);
    const bool bGame = Directory == TEXT("/Game") || Directory.StartsWith(TEXT("/Game/"));
    const bool bEngine = Directory == TEXT("/Engine") || Directory.StartsWith(TEXT("/Engine/"));
    if (Directory.Len() >= NAME_SIZE || (!bGame && !bEngine) || !FPackageName::IsValidLongPackageName(Directory, false))
    {
        return Fail(Result, FString::Printf(TEXT("directory must be a valid /Game or /Engine package path: %s"), *Directory));
    }
    if (IsDependencyToken(Directory)) { return Fail(Result, TEXT("directory must not contain an MCP-only dependency token")); }
    const int32 MaxResults = FMath::Clamp(static_cast<int32>(MaxResultsValue), 1, 1000);
    const int32 Offset = FMath::Max(static_cast<int32>(OffsetValue), 0);
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*Directory));
    Filter.bRecursivePaths = bRecursive;
    Filter.ClassPaths.Add(UStateTree::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Result.Stage = TEXT("asset_registry");
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;
    Count(Result, TEXT("registry_get_assets_calls"));
    const bool bQueried = Registry.GetAssets(Filter, Assets);
    Count(Result, TEXT("registry_assets_returned"), Assets.Num());
    Result.Data->SetBoolField(TEXT("asset_registry_query_returned"), bQueried);
    Result.Data->SetStringField(TEXT("registry_result_scope"),
        TEXT("Legacy projection of the returned AssetRegistry array is retained, including an empty array when GetAssets returns false; no full inventory completeness claim."));
    Assets.Sort([](const FAssetData& Left, const FAssetData& Right)
    {
        return Left.GetObjectPathString() < Right.GetObjectPathString();
    });
    TArray<TSharedPtr<FJsonValue>> Items;
    for (int32 Index = Offset; Index < Assets.Num() && Items.Num() < MaxResults; ++Index)
    {
        const FAssetData& Asset = Assets[Index];
        auto Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Asset.AssetName.ToString());
        Item->SetStringField(TEXT("asset_path"), Asset.GetObjectPathString());
        Item->SetStringField(TEXT("package_path"), Asset.PackagePath.ToString());
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    const int64 NextOffset = static_cast<int64>(Offset) + Items.Num();
    Result.Data->SetStringField(TEXT("directory"), Directory);
    Result.Data->SetArrayField(TEXT("assets"), Items);
    Result.Data->SetNumberField(TEXT("count"), Items.Num());
    Result.Data->SetNumberField(TEXT("total"), Assets.Num());
    Result.Data->SetNumberField(TEXT("offset"), Offset);
    Result.Data->SetBoolField(TEXT("has_more"), NextOffset < Assets.Num());
    if (NextOffset < Assets.Num()) { Result.Data->SetNumberField(TEXT("next_offset"), static_cast<double>(NextOffset)); }
    Result.Data->SetBoolField(TEXT("explicit_authoring_mutation_performed"), false);
    Result.Stage = TEXT("native_readback");
    Result.bSuccess = true;
    Result.bPostconditionPassed = true;
    return Result;
}

Variable::FResult Inspect(const FString& RawAssetPath, int64 MaxStatesValue)
{
    FResult Result;
    Init(Result, TEXT("inspect_state_tree"));
    if (!IsEditorGameThread()) { return Fail(Result, TEXT("StateTree inspection requires the editor game thread.")); }
    if (!IsInt32Value(MaxStatesValue)) { return Fail(Result, TEXT("'max_states' must be a finite 32-bit integer")); }
    const FString AssetPath = NormalizeAssetPath(RawAssetPath);
    if (AssetPath.IsEmpty() || AssetPath.Len() >= NAME_SIZE || IsDependencyToken(AssetPath))
    {
        return Fail(Result, TEXT("asset_path is required and must not contain an MCP-only dependency token"));
    }
    FText InvalidReason;
    if (!FPackageName::IsValidObjectPath(AssetPath, &InvalidReason))
    {
        return Fail(Result, FString::Printf(TEXT("asset_path must be a valid Unreal object path: %s"), *InvalidReason.ToString()));
    }
    Result.Data->SetStringField(TEXT("requested_asset_path"), AssetPath);
    UStateTree* Before = FindObject<UStateTree>(nullptr, *AssetPath);
    const bool bBeforeLoaded = Loaded(Before);
    Result.Data->SetBoolField(TEXT("already_loaded_before_call"), bBeforeLoaded);
    const uint32 BeforeHash = bBeforeLoaded ? Before->LastCompiledEditorDataHash : 0;
    const bool bBeforeDirty = bBeforeLoaded && Before->GetOutermost()->IsDirty();
    if (bBeforeLoaded)
    {
        Result.Data->SetNumberField(TEXT("compiled_editor_data_hash_before_load"), BeforeHash);
        Result.Data->SetBoolField(TEXT("package_dirty_before_load"), bBeforeDirty);
    }
    Result.Stage = TEXT("load_asset");
    Count(Result, TEXT("asset_load_calls"));
    Result.Data->SetBoolField(TEXT("implicit_postload_possible"), true);
    Result.Data->SetBoolField(TEXT("implicit_state_tree_compile_possible"), true);
    Result.bChangeStateKnown = false;
    UStateTree* StateTree = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(AssetPath));
    Count(Result, TEXT("asset_load_returns"));
    if (!Loaded(StateTree)) { return Fail(Result, FString::Printf(TEXT("StateTree not found or not fully loaded: %s"), *AssetPath)); }
    const TStrongObjectPtr<UStateTree> Hold(StateTree);
    const uint32 AfterLoadHash = StateTree->LastCompiledEditorDataHash;
    const bool bAfterLoadDirty = StateTree->GetOutermost()->IsDirty();
    Result.Data->SetNumberField(TEXT("compiled_editor_data_hash_after_load"), AfterLoadHash);
    Result.Data->SetBoolField(TEXT("package_dirty_after_load"), bAfterLoadDirty);
    const bool bObservedLoadChange = bBeforeLoaded && (BeforeHash != AfterLoadHash || bBeforeDirty != bAfterLoadDirty);
    Result.Data->SetBoolField(TEXT("observed_load_hash_or_dirty_change"), bObservedLoadChange);
    Result.bChanged = bObservedLoadChange;
    Result.bChangeStateKnown = bObservedLoadChange;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData.Get());
    if (!Loaded(EditorData)) { return Fail(Result, TEXT("StateTree has no readable UE 5.8 editor data")); }
    const TStrongObjectPtr<UStateTreeEditorData> HoldEditorData(EditorData);
    if (EditorData->Schema && !Loaded(EditorData->Schema.Get())) { return Fail(Result, TEXT("StateTree schema is not fully loaded")); }
    const int32 MaxStates = FMath::Clamp(static_cast<int32>(MaxStatesValue), 1, 2000);
    int32 StateCount = 0;
    int32 ProjectionAttempts = 0;
    const int32 MaxProjectionAttempts = MaxStates * 2 + 1;
    Result.Data->SetNumberField(TEXT("max_projection_attempts"), MaxProjectionAttempts);
    Result.Data->SetBoolField(TEXT("projection_attempt_limit_reached"), false);
    bool bTruncated = false;
    TSet<const UStateTreeState*> Visited;
    TArray<TSharedPtr<FJsonValue>> Roots;
    Result.Stage = TEXT("hierarchy_readback");
    for (const UStateTreeState* Root : EditorData->SubTrees)
    {
        if (StateCount >= MaxStates) { bTruncated = true; break; }
        if (ProjectionAttempts >= MaxProjectionAttempts)
        {
            bTruncated = true;
            Result.Data->SetBoolField(TEXT("observation_incomplete"), true);
            Result.Data->SetBoolField(TEXT("projection_attempt_limit_reached"), true);
            break;
        }
        Roots.Add(MakeShared<FJsonValueObject>(StateJson(Root, StateCount, MaxStates, 0, Visited, bTruncated,
            ProjectionAttempts, MaxProjectionAttempts, Result)));
    }
    Result.Data->SetStringField(TEXT("asset_path"), StateTree->GetPathName());
    Result.Data->SetStringField(TEXT("schema_class"), EditorData->Schema ? EditorData->Schema->GetClass()->GetPathName() : FString());
    Result.Data->SetNumberField(TEXT("state_count"), StateCount);
    Result.Data->SetBoolField(TEXT("truncated"), bTruncated);
    Result.Data->SetArrayField(TEXT("root_states"), Roots);
    Result.Data->SetNumberField(TEXT("global_task_count"), EditorData->GlobalTasks.Num());
    Result.Data->SetNumberField(TEXT("evaluator_count"), EditorData->Evaluators.Num());
    Result.Data->SetBoolField(TEXT("hierarchy_observed_before_readiness"), true);
    Result.Stage = TEXT("readiness_readback");
    const uint32 BeforeReadyHash = StateTree->LastCompiledEditorDataHash;
    const bool bBeforeReadyDirty = StateTree->GetOutermost()->IsDirty();
    Result.Data->SetNumberField(TEXT("compiled_editor_data_hash_before_readiness"), BeforeReadyHash);
    Result.Data->SetBoolField(TEXT("package_dirty_before_readiness"), bBeforeReadyDirty);
    Count(Result, TEXT("ready_to_run_calls"));
    const bool bReady = StateTree->IsReadyToRun();
    Result.Data->SetBoolField(TEXT("ready_to_run"), bReady);
    if (!Loaded(StateTree))
    {
        Result.bChangeStateKnown = false;
        Result.Data->SetBoolField(TEXT("observation_incomplete"), true);
        return Fail(Result, TEXT("StateTree is no longer readable after readiness observation; the actual load and pre-readiness prefix is retained."));
    }
    const uint32 AfterHash = StateTree->LastCompiledEditorDataHash;
    const bool bAfterDirty = StateTree->GetOutermost()->IsDirty();
    Result.Data->SetBoolField(TEXT("ready_to_run"), bReady);
    Result.Data->SetNumberField(TEXT("compiled_editor_data_hash"), AfterHash);
    Result.Data->SetBoolField(TEXT("package_dirty_after_readiness"), bAfterDirty);
    const bool bObservedChange = BeforeReadyHash != AfterHash || bBeforeReadyDirty != bAfterDirty
        || (bBeforeLoaded && (BeforeHash != AfterHash || bBeforeDirty != bAfterDirty));
    Result.Data->SetBoolField(TEXT("observed_hash_or_dirty_change"), bObservedChange);
    Result.Data->SetBoolField(TEXT("observed_hash_and_dirty_equal_implies_no_total_change"), false);
    Result.bChanged = bObservedChange;
    Result.bChangeStateKnown = bObservedChange; // Positive observed change is known; equal observations do not prove no SDK change.
    Result.Data->SetBoolField(TEXT("explicit_authoring_mutation_performed"), false);
    const bool bEditorIdentityRetained = Loaded(EditorData)
        && StateTree->EditorData.Get() == EditorData;
    Result.Data->SetBoolField(TEXT("editor_data_identity_retained_after_readiness"), bEditorIdentityRetained);
    if (!bEditorIdentityRetained)
    {
        Result.Data->SetBoolField(TEXT("observation_incomplete"), true);
        return Fail(Result, TEXT("StateTree editor data identity changed during readiness observation; pre-readiness hierarchy and actual hash/dirty prefix are retained."));
    }
    if (Result.Data->GetBoolField(TEXT("observation_incomplete")))
    {
        return Fail(Result, TEXT("StateTree hierarchy is unreadable, has unsafe parent paths or exceeds the projection allowance; bounded partial output is retained."));
    }
    Result.Stage = TEXT("native_readback");
    Result.bSuccess = true;
    Result.bPostconditionPassed = true;
    return Result;
}
}
