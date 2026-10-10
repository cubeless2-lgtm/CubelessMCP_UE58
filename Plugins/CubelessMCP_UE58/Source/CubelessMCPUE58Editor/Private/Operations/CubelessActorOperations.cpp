#include "Operations/CubelessActorOperations.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"

namespace Cubeless::Operations
{
FString RunActorOperation(const UE58::Actor::FRequest& Request)
{
    namespace Api = UE58::Actor;
    const double Start = FPlatformTime::Seconds();
    const bool bMutation = Api::IsMutation(Request.Operation);
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("operation"), Request.BlueprintPath.IsEmpty() ? Api::OperationName(Request.Operation) : TEXT("spawn_blueprint_actor"));
    Result->SetStringField(TEXT("stage"), TEXT("preflight"));
    Result->SetBoolField(TEXT("mutation_attempted"), false);
    Result->SetBoolField(TEXT("changed"), false);
    Result->SetBoolField(TEXT("change_state_known"), true);
    Result->SetBoolField(TEXT("postcondition_passed"), false);
    Result->SetBoolField(TEXT("compile_requested"), false);
    Result->SetBoolField(TEXT("save_requested"), false);
    Result->SetBoolField(TEXT("saved"), false);
    Result->SetBoolField(TEXT("atomic_rollback"), false);
    Result->SetBoolField(TEXT("auto_retry_safe"), !bMutation);
    Result->SetStringField(TEXT("validation_scope"), TEXT("loaded editor-world actor observations; no save, reload, runtime package or arbitrary delegate side-effect proof"));
    Result->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());
    if (!Request.BlueprintPath.IsEmpty())
    { Result->SetStringField(TEXT("blueprint_load_scope"), TEXT("Loading may trigger PostLoad or engine compilation. compile_requested describes explicit Cubeless compile only; actor observations do not audit arbitrary asset side effects.")); }
    int32 CaptureCalls = 0, ActorVisits = 0;
    auto Finish = [&]()
    {
        auto Performance = MakeShared<FJsonObject>();
        Performance->SetNumberField(TEXT("actor_snapshot_enumerations"), CaptureCalls);
        Performance->SetNumberField(TEXT("actor_iterator_visits"), ActorVisits);
        Performance->SetNumberField(TEXT("native_elapsed_ms_before_final_serialization"), (FPlatformTime::Seconds() - Start) * 1000.0);
        Performance->SetStringField(TEXT("scope"), TEXT("this native call only; excludes Python journal, transport, final serialization and engine-internal traversal"));
        Result->SetObjectField(TEXT("performance"), Performance);
        return Api::ToJson(Result);
    };
    auto Fail = [&](const FString& Code, const FString& Error)
    {
        Result->SetStringField(TEXT("error_code"), Code);
        Result->SetStringField(TEXT("error"), Error);
        return Finish();
    };
    Api::FState Before;
    FString Code, Error;
    ++CaptureCalls;
    const bool bCaptured = Api::Capture(Request, Before, Code, Error);
    ActorVisits += Before.ActorVisits;
    if (Before.Snapshot.IsValid()) { Result->SetObjectField(TEXT("before"), Before.Snapshot); }
    if (Before.Level.IsValid())
    {
        Result->SetBoolField(TEXT("dirty_before"), Before.bDirty);
        Result->SetBoolField(TEXT("dirty_after"), Before.bDirty);
    }
    if (!bCaptured) { return Fail(Code, Error); }
    if (!bMutation)
    {
        if (Request.Operation == Api::EOperation::Inspect && !Before.TargetSnapshot.IsValid())
        {
            return Fail(TEXT("actor_not_found"), TEXT("No actor has the exact UObject name; labels are not selectors."));
        }
        Result->SetObjectField(TEXT("data"), Api::ReadPayload(Request, Before));
        auto Verification = MakeShared<FJsonObject>();
        Verification->SetStringField(TEXT("method"), TEXT("bounded_loaded_editor_world_enumeration"));
        Verification->SetBoolField(TEXT("independent_after_read"), false);
        Result->SetObjectField(TEXT("verification"), Verification);
        Result->SetBoolField(TEXT("postcondition_passed"), true);
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("stage"), TEXT("complete"));
        return Finish();
    }
    Api::FPreparedMutation Prepared;
    if (!Api::PrepareMutation(Request, Before, Prepared, Code, Error))
    {
        if (Prepared.bBlueprintLoadAttempted)
        {
            Api::FState AfterLoad; FString ReadCode, ReadError; ++CaptureCalls;
            const bool bObserved = Api::Capture(Request, AfterLoad, ReadCode, ReadError);
            ActorVisits += AfterLoad.ActorVisits;
            if (bObserved && Before.World == AfterLoad.World && Before.Level == AfterLoad.Level)
            {
                auto Changes = Api::DescribeChanges(Before, AfterLoad);
                Result->SetObjectField(TEXT("after"), AfterLoad.Snapshot);
                Result->SetObjectField(TEXT("actor_changes"), Changes);
                Result->SetBoolField(TEXT("dirty_after"), AfterLoad.bDirty);
                Result->SetBoolField(TEXT("changed"), Changes->GetNumberField(TEXT("affected_count")) > 0 || Before.bDirty != AfterLoad.bDirty);
            }
            else
            {
                Result->RemoveField(TEXT("changed")); Result->RemoveField(TEXT("dirty_after"));
                Result->SetBoolField(TEXT("change_state_known"), false);
                Result->SetStringField(TEXT("preflight_after_observation_error"), ReadError);
            }
        }
        else { Result->SetObjectField(TEXT("after"), Before.Snapshot); }
        return Fail(Code, Error);
    }
    Result->SetStringField(TEXT("stage"), TEXT("execute"));
    Result->SetBoolField(TEXT("mutation_attempted"), !Prepared.bNoOp);
    Result->SetBoolField(TEXT("skipped_noop"), Prepared.bNoOp);
    const bool bAccepted = Api::ExecuteMutation(Request, Before, Prepared);
    Result->SetBoolField(TEXT("engine_api_accepted"), bAccepted);
    Api::FState After;
    ++CaptureCalls;
    const bool bReadback = Api::Capture(Request, After, Code, Error);
    ActorVisits += After.ActorVisits;
    Result->SetStringField(TEXT("stage"), TEXT("readback"));
    if (After.Snapshot.IsValid()) { Result->SetObjectField(TEXT("after"), After.Snapshot); }
    const bool bSameWorld = Before.World == After.World && Before.Level == After.Level;
    if (bSameWorld && After.Level.IsValid()) { Result->SetBoolField(TEXT("dirty_after"), After.bDirty); }
    else { Result->RemoveField(TEXT("dirty_after")); }
    if (!bReadback || !bSameWorld)
    {
        Result->RemoveField(TEXT("changed"));
        Result->SetBoolField(TEXT("change_state_known"), false);
        return Fail(TEXT("actor_readback_failed"), bSameWorld ? Code + TEXT(": ") + Error : TEXT("Editor world/level identity changed during dispatch; original actor state is unknown."));
    }
    auto Changes = Api::DescribeChanges(Before, After);
    const bool bChanged = Changes->GetNumberField(TEXT("affected_count")) > 0 || Before.bDirty != After.bDirty;
    Result->SetBoolField(TEXT("changed"), bChanged);
    Result->SetObjectField(TEXT("actor_changes"), Changes);
    const bool bVerified = Api::VerifyMutation(Request, Before, After, Prepared, bAccepted);
    Result->SetBoolField(TEXT("postcondition_passed"), bVerified);
    auto Data = Api::ReadPayload(Request, After);
    if (!Request.BlueprintPath.IsEmpty())
    {
        Data->SetStringField(TEXT("blueprint_input"), Request.BlueprintPath);
        Data->SetStringField(TEXT("resolved_blueprint_path"), Request.BlueprintPath);
        Data->SetStringField(TEXT("spawned_class"), Prepared.SpawnClass->GetPathName());
        Data->SetStringField(TEXT("blueprint_preflight"), TEXT("clean compiled saved /Game Actor Blueprint; parent chain and saved recursive dependencies, max 256 packages"));
        Data->SetStringField(TEXT("construction_scope"), TEXT("Construction scripts execute normally. Readback covers loaded actor identity/basic transforms, not arbitrary construction-script side effects; no rollback."));
    }
    Data->SetBoolField(TEXT("postcondition_verified"), bVerified);
    // Preserve legacy per-request counters separately from observed changes to
    // other loaded actors (for example editor selection bookkeeping).
    auto Delta = MakeShared<FJsonObject>();
    const bool bTargetUpdated = Request.Operation == Api::EOperation::SetTransform && After.TargetSnapshot.IsValid()
        && !Before.TargetTransform.Equals(After.TargetTransform, 0.01);
    Delta->SetNumberField(TEXT("affected_count"), 1);
    Delta->SetNumberField(TEXT("created_count"), Request.Operation == Api::EOperation::Spawn && After.TargetSnapshot.IsValid() ? 1 : 0);
    Delta->SetNumberField(TEXT("removed_count"), Request.Operation == Api::EOperation::Delete && !After.TargetSnapshot.IsValid() ? 1 : 0);
    Delta->SetNumberField(TEXT("updated_count"), bTargetUpdated ? 1 : 0);
    Delta->SetNumberField(TEXT("skipped_count"), Request.Operation == Api::EOperation::SetTransform && !bTargetUpdated ? 1 : 0);
    Delta->SetNumberField(TEXT("failed_count"), bVerified ? 0 : 1);
    Data->SetObjectField(TEXT("delta"), Delta);
    if (Request.Operation == Api::EOperation::Spawn) { Data->SetBoolField(TEXT("spawned"), After.TargetSnapshot.IsValid()); }
    if (Request.Operation == Api::EOperation::Delete)
    {
        Data->SetObjectField(TEXT("deleted_actor"), Before.TargetSnapshot);
        Data->SetBoolField(TEXT("deleted"), bAccepted && !After.TargetSnapshot.IsValid());
    }
    if (Request.Operation == Api::EOperation::SetTransform) { Data->SetBoolField(TEXT("set_accepted"), bAccepted); }
    Result->SetObjectField(TEXT("data"), Data);
    auto Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(TEXT("method"), TEXT("independent_editor_world_actor_relookup_and_transform_comparison"));
    Verification->SetBoolField(TEXT("independent_after_read"), true);
    Verification->SetNumberField(TEXT("transform_tolerance"), 0.01);
    Verification->SetBoolField(TEXT("passed"), bVerified);
    Verification->SetStringField(TEXT("status"), bVerified ? TEXT("passed") : TEXT("failed"));
    Verification->SetBoolField(TEXT("before_observed"), true);
    Verification->SetBoolField(TEXT("after_observed"), true);
    Verification->SetBoolField(TEXT("strict"), true);
    TArray<TSharedPtr<FJsonValue>> Checks;
    Checks.Add(MakeShared<FJsonValueString>(TEXT("editor_world_and_level_unchanged")));
    Checks.Add(MakeShared<FJsonValueString>(Request.Operation == Api::EOperation::Delete ? TEXT("actor_absent_after") : TEXT("actor_identity_class_and_transform_readback")));
    Verification->SetArrayField(TEXT("checks"), Checks);
    Result->SetObjectField(TEXT("verification"), Verification);
    Data->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1"));
    Data->SetStringField(TEXT("mode"), TEXT("apply"));
    Data->SetBoolField(TEXT("dry_run"), false);
    Data->SetBoolField(TEXT("DRY_RUN"), false);
    Data->SetBoolField(TEXT("changed"), bChanged);
    Data->SetBoolField(TEXT("mutation_performed"), !Prepared.bNoOp && (bAccepted || bChanged));
    auto Mutation = MakeShared<FJsonObject>();
    Mutation->SetBoolField(TEXT("attempted"), !Prepared.bNoOp);
    Mutation->SetBoolField(TEXT("performed"), !Prepared.bNoOp && (bAccepted || bChanged));
    Mutation->SetStringField(TEXT("state"), Prepared.bNoOp ? TEXT("not_performed") : bAccepted || bChanged ? TEXT("performed") : TEXT("not_performed"));
    Data->SetObjectField(TEXT("mutation"), Mutation);
    Data->SetObjectField(TEXT("before"), Before.Snapshot);
    Data->SetObjectField(TEXT("after"), After.Snapshot);
    Data->SetObjectField(TEXT("verification"), Verification);
    Data->SetBoolField(TEXT("verified_success"), bVerified);
    Data->SetBoolField(TEXT("success"), bVerified);
    Data->SetStringField(TEXT("success_semantics"), bVerified ? TEXT("verified_postcondition") : bAccepted ? TEXT("postcondition_failed") : TEXT("failed_execution"));
    if (!bVerified)
    {
        return Fail(bAccepted ? TEXT("actor_postcondition_failed") : TEXT("actor_engine_operation_rejected"), TEXT("Inspect before/after and actor_changes. No automatic retry or rollback was performed."));
    }
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("stage"), TEXT("complete"));
    return Finish();
}
}
