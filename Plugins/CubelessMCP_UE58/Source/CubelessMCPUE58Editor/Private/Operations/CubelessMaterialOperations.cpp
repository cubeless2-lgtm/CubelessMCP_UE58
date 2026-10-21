#include "Operations/CubelessMaterialOperations.h"

#include "Editor.h"
#include "Templates/UnrealTemplate.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace Cubeless::Operations
{
namespace
{
// Official tool calls run on the game thread. This additionally rejects
// re-entrant edits during engine callbacks; the public Python layer owns the
// durable journal and cross-operation mutation admission gate.
bool GMaterialMutationActive = false;
}

FString RunMaterialOperation(UE58::Material::EOperation Operation, const TSharedPtr<FJsonObject>& InputParams, int32 MaxGraphNodes)
{
    namespace Api = UE58::Material;
    Api::FReadMetricsScope ReadMetrics;
    TSharedPtr<FJsonObject> Params = InputParams;
    const bool bMutation = Api::IsMutation(Operation);
    auto Result = MakeShared<FJsonObject>();
    Result->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());
    Result->SetStringField(TEXT("operation"), Api::OperationName(Operation));
    Result->SetStringField(TEXT("stage"), TEXT("preflight"));
    Result->SetBoolField(TEXT("success"), false);
    Result->SetBoolField(TEXT("mutation_attempted"), false);
    Result->SetBoolField(TEXT("changed"), false);
    Result->SetBoolField(TEXT("change_state_known"), true);
    Result->SetBoolField(TEXT("postcondition_passed"), false);
    Result->SetBoolField(TEXT("atomic_rollback"), false);
    Result->SetBoolField(TEXT("auto_retry_safe"), !bMutation);
    Result->SetBoolField(TEXT("implicit_compile_possible"), bMutation);
    Result->SetStringField(TEXT("compile_status"), TEXT("not_requested"));
    Result->SetStringField(TEXT("save_status"), TEXT("not_requested"));
    Result->SetStringField(TEXT("journal_owner"), TEXT("public_python_material_toolset; native helpers alone do not write durable journals"));
    Result->SetStringField(TEXT("validation_scope"), TEXT("loaded graph readback only; no compile, save, reload, render or packaging proof"));
    auto Limits = MakeShared<FJsonObject>();
    Limits->SetNumberField(TEXT("max_graph_nodes"), MaxGraphNodes);
    Result->SetObjectField(TEXT("limits"), Limits);
    if (Operation == Api::EOperation::GetCollectionValues)
    {
        Result->SetStringField(TEXT("mutation_attempted_scope"), TEXT("explicit asset/parameter value writes only; runtime getter initialization is reported separately"));
        Result->SetStringField(TEXT("changed_scope"), TEXT("observed collection dirty/value payload after first runtime getter; pre-getter world instances/render resources excluded"));
        Result->SetStringField(TEXT("validation_scope"), TEXT("collection dirty/value payload only; public runtime getter can lazily initialize world instances/render resources; no renderer proof"));
    }

    auto Fail = [&](const FString& Code, const FString& Message)
    {
        Result->SetStringField(TEXT("error_code"), Code);
        Result->SetStringField(TEXT("error"), Message);
        Result->SetObjectField(TEXT("performance"), ReadMetrics.ToJson());
        return Api::ToJson(Result);
    };
    if (!IsInGameThread()) { return Fail(TEXT("game_thread_required"), TEXT("Material editor APIs require the game thread.")); }
    if (!GEditor) { return Fail(TEXT("editor_unavailable"), TEXT("Editor is unavailable.")); }
    if (MaxGraphNodes < 1 || MaxGraphNodes > 10000) { return Fail(TEXT("invalid_budget"), TEXT("max_graph_nodes must be in 1..10000.")); }
    if (bMutation && (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor))
    {
        return Fail(TEXT("pie_mutation_rejected"), TEXT("Material mutations are disabled during PIE/SIE."));
    }
    if (bMutation && GMaterialMutationActive) { return Fail(TEXT("mutation_busy"), TEXT("Another native Material mutation is active.")); }

    if (Operation == Api::EOperation::AddCustomNode)
    {
        FString Code, Message;
        if (!Api::PrepareCustomNodeRequest(InputParams, Params, Code, Message)) { return Fail(Code, Message); }
    }

    Api::FState Before;
    FString Error;
    if (!Api::CaptureState(Operation, Params, MaxGraphNodes, Before, Error))
    {
        if (Before.Snapshot.IsValid()) { Result->SetObjectField(TEXT("data"), Before.Snapshot); }
        return Fail(TEXT("material_preflight_failed"), Error);
    }
    Result->SetStringField(TEXT("asset_path"), Before.AssetPath);
    Result->SetBoolField(TEXT("dirty_before"), Before.bDirty);
    Result->SetObjectField(TEXT("before"), Before.Snapshot);
    if (Operation == Api::EOperation::GetCollectionValues)
    {
        bool bGetterDispatched = false;
        Before.Snapshot->TryGetBoolField(TEXT("runtime_instance_getter_dispatched"), bGetterDispatched);
        Result->SetBoolField(TEXT("runtime_instance_getter_dispatched"), bGetterDispatched);
        Result->SetBoolField(TEXT("runtime_instance_creation_possible"), bGetterDispatched);
        Result->SetStringField(TEXT("runtime_instance_creation_observed"), TEXT("not_measured"));
        Result->SetBoolField(TEXT("runtime_render_resource_side_effects_possible"), bGetterDispatched);
    }
    if (bMutation && (!Api::ValidateMutationTarget(Before, Error) || !Api::ValidateMutationInputs(Operation, Params, Before, MaxGraphNodes, Error)))
    {
        Result->SetBoolField(TEXT("dirty_after"), Before.bDirty);
        Result->SetObjectField(TEXT("after"), Before.Snapshot);
        return Fail(TEXT("material_mutation_target_rejected"), Error);
    }

    TSharedPtr<FJsonObject> Data;
    {
        TGuardValue<bool> MutationGuard(GMaterialMutationActive, bMutation || GMaterialMutationActive);
        TUniquePtr<FScopedTransaction> Transaction;
        if (bMutation)
        {
            Transaction = MakeUnique<FScopedTransaction>(FText::FromString(TEXT("Cubeless ") + Api::OperationName(Operation)));
            // Capture the root object before the legacy domain helper changes it.
            // Modify(false) records undo without dirtying a preflight-only failure.
            Before.Asset->Modify(false);
            Result->SetBoolField(TEXT("mutation_attempted"), true);
        }
        Result->SetStringField(TEXT("stage"), TEXT("execute"));
        // These two exact read results were already obtained by CaptureState.
        // Reuse only the local payload; mutations and the independent after read
        // always retain their existing paths. Missing payload falls back to Execute.
        const bool bReused = !bMutation && Api::TryReuseCapturedReadData(Operation, Before, Data);
        Result->SetStringField(TEXT("read_data_source"), bReused ? TEXT("local_before_capture") : TEXT("execute"));
        if (!bReused) { Data = Api::Execute(Operation, Params); }
    }
    if (!Data.IsValid()) { Data = MakeShared<FJsonObject>(); Data->SetStringField(TEXT("error"), TEXT("Native material operation returned no result.")); }
    Result->SetObjectField(TEXT("data"), Data);

    bool bSucceeded = true;
    Data->TryGetBoolField(TEXT("success"), bSucceeded);
    FString OperationError;
    if (Data->TryGetStringField(TEXT("error"), OperationError) && !OperationError.IsEmpty()) { bSucceeded = false; }
    bool bResolved = true;
    if (Operation == Api::EOperation::ResolveGraph && Data->TryGetBoolField(TEXT("resolved"), bResolved) && !bResolved) { bSucceeded = false; }

    Api::FState After;
    Error.Reset();
    if (!Api::CaptureState(Operation, Params, MaxGraphNodes, After, Error))
    {
        Result->SetStringField(TEXT("stage"), TEXT("readback"));
        Result->SetBoolField(TEXT("change_state_known"), false);
        Result->RemoveField(TEXT("changed"));
        Result->SetBoolField(TEXT("auto_retry_safe"), !bMutation);
        return Fail(TEXT("material_readback_failed"), Error);
    }
    Result->SetObjectField(TEXT("after"), After.Snapshot);
    Result->SetBoolField(TEXT("dirty_after"), After.bDirty);
    const bool bChanged = Api::ToJson(Before.Snapshot) != Api::ToJson(After.Snapshot) || Before.bDirty != After.bDirty;
    Result->SetBoolField(TEXT("changed"), bChanged);
    Result->SetObjectField(TEXT("delta"), Api::DescribeDelta(Before, After));

    FString VerificationMethod = TEXT("read_only_snapshot_unchanged");
    if (Operation == Api::EOperation::GetCollectionValues)
    {
        VerificationMethod = TEXT("collection_payload_unchanged_after_initial_runtime_getter");
    }
    bool bVerified = !bChanged;
    if (bMutation)
    {
        bVerified = bSucceeded && Api::VerifyMutation(Operation, Params, Before, After, VerificationMethod, Error);
    }
    auto Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(TEXT("method"), VerificationMethod);
    Verification->SetBoolField(TEXT("passed"), bVerified);
    Verification->SetStringField(TEXT("scope"), Operation == Api::EOperation::LayoutNodes
        ? TEXT("coordinates observed and topology preserved; aesthetic layout quality is not evaluated")
        : TEXT("loaded graph state only; shader/render behavior is not evaluated"));
    if (Operation == Api::EOperation::GetCollectionValues)
    {
        Verification->SetStringField(TEXT("scope"), TEXT("collection dirty/value payload only; no pre-getter world instance inventory or render-resource/GPU verification"));
    }
    Result->SetObjectField(TEXT("verification"), Verification);
    Result->SetBoolField(TEXT("postcondition_passed"), bVerified);
    if (!bSucceeded)
    {
        Result->SetStringField(TEXT("stage"), TEXT("execute"));
        return Fail(TEXT("material_operation_failed"), OperationError.IsEmpty() ? TEXT("Material operation reported failure.") : OperationError);
    }
    if (!bVerified)
    {
        Result->SetStringField(TEXT("stage"), TEXT("verify"));
        return Fail(TEXT("material_postcondition_failed"), Error.IsEmpty() ? TEXT("Read-only operation changed the observed state.") : Error);
    }
    Result->SetStringField(TEXT("stage"), TEXT("complete"));
    Result->SetBoolField(TEXT("success"), true);
    Result->SetObjectField(TEXT("performance"), ReadMetrics.ToJson());
    return Api::ToJson(Result);
}
}
