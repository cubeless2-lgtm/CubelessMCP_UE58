#include "Compatibility/UE58/CubelessPythonExecutionApi.h"

#include "Dom/JsonValue.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/Transactor.h"
#include "Editor/TransBuffer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "IPythonScriptPlugin.h"
#include "InterchangeManager.h"
#include "Misc/CoreMisc.h"
#include "PythonScriptTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::PythonExecution
{
namespace
{
int32 ActiveExecutionDepth = 0; // Editor GT only, never a worker-thread admission check.
bool AdmissionActive = false;
uint64 AdmissionGeneration = 0;
struct FExecutionDepthScope
{
    FExecutionDepthScope() { ++ActiveExecutionDepth; }
    ~FExecutionDepthScope() { --ActiveExecutionDepth; }
};
bool Live(const UObject* Object)
{
    return Object && IsValid(Object) && !Object->HasAnyFlags(
        RF_BeginDestroyed | RF_FinishDestroyed | RF_NeedLoad | RF_NeedPostLoad);
}
// One concrete synchronous Batch owner, not a general transaction/job service.
// There is deliberately no destructor which could close a foreign transaction.
struct FBatchTransaction
{
    TStrongObjectPtr<UEditorEngine> Editor;
    TStrongObjectPtr<UTransBuffer> Buffer;
    TStrongObjectPtr<UWorld> World;
    const FTransaction* Transaction = nullptr;
    FGuid TransactionId, TransactionOperationId;
    FString ScopeId, OwnerOperationId, Context;
    uint64 Generation = 0, Frame = 0;
    int32 BeforeDepth = 0, ExpectedDepth = 0, BeginIndex = INDEX_NONE;
    bool Ready = false, Quarantined = false, BeginCalled = false, EndCalled = false, ControlBusy = false;
    FString Failure;

    bool EditorCurrent() const
    {
        return IsInGameThread() && !GExitPurge && !IsEngineExitRequested() && !IsGarbageCollecting()
            && !UE::IsSavingPackage() && AdmissionActive && AdmissionGeneration == Generation
            && GFrameCounter == Frame && GEditor == Editor.Get() && Live(Editor.Get())
            && Live(Buffer.Get()) && Buffer->GetClass() == UTransBuffer::StaticClass()
            && Editor->Trans.IsResolved() && Editor->Trans.Get() == Buffer.Get()
            && Editor->PlayWorld.IsResolved() && Editor->PlayWorld.Get() == nullptr
            && !GIsTransacting && Live(World.Get())
            && Editor->GetEditorWorldContext().World() == World.Get();
    }
    bool Current() const
    {
        if (!Ready || Quarantined || ControlBusy || EndCalled || !EditorCurrent() || Buffer->ActiveCount != ExpectedDepth
            || Buffer->UndoCount != 0 || Buffer->UndoBuffer.IsEmpty()
            || &Buffer->UndoBuffer.Last().Get() != Transaction) { return false; }
        const FTransactionContext Copied = Transaction->GetContext();
        return Copied.TransactionId == TransactionId && Copied.OperationId == TransactionOperationId
            && Copied.Context == Context;
    }
    TSharedRef<FJsonObject> Receipt(bool Success) const
    {
        auto Out = MakeShared<FJsonObject>();
        Out->SetBoolField(TEXT("success"), Success);
        Out->SetStringField(TEXT("scope_id"), ScopeId);
        Out->SetStringField(TEXT("owner_operation_id"), OwnerOperationId);
        Out->SetStringField(TEXT("transaction_context"), Context);
        Out->SetStringField(TEXT("transaction_id"), TransactionId.ToString());
        Out->SetStringField(TEXT("transaction_operation_id"), TransactionOperationId.ToString());
        Out->SetNumberField(TEXT("depth_before"), BeforeDepth);
        Out->SetNumberField(TEXT("expected_depth"), ExpectedDepth);
        Out->SetNumberField(TEXT("begin_index"), BeginIndex);
        Out->SetNumberField(TEXT("begin_calls"), BeginCalled ? 1 : 0);
        Out->SetNumberField(TEXT("end_calls"), EndCalled ? 1 : 0);
        Out->SetBoolField(TEXT("scope_quarantined"), Quarantined);
        if (Quarantined) { Out->SetField(TEXT("scope_open"), MakeShared<FJsonValueNull>()); }
        else { Out->SetBoolField(TEXT("scope_open"), Ready && !EndCalled); }
        Out->SetBoolField(TEXT("scope_outcome_unknown"), Quarantined);
        Out->SetBoolField(TEXT("automatic_retry"), false);
        Out->SetBoolField(TEXT("atomic_rollback"), false);
        Out->SetBoolField(TEXT("across_frames_supported"), false);
        Out->SetBoolField(TEXT("global_callback_purity_verified"), false);
        if (!Success)
        {
            Out->SetStringField(TEXT("error_code"), Quarantined ? TEXT("batch_transaction_quarantined") : TEXT("batch_transaction_unavailable"));
            Out->SetStringField(TEXT("error"), Failure);
        }
        return Out;
    }
};
TUniquePtr<FBatchTransaction> BatchTransaction;

TSharedRef<FJsonObject> BatchRejected(const TCHAR* Code, const TCHAR* Message)
{
    auto Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), false); Out->SetStringField(TEXT("error_code"), Code);
    Out->SetStringField(TEXT("error"), Message); Out->SetBoolField(TEXT("scope_outcome_unknown"), false);
    Out->SetBoolField(TEXT("scope_open"), false); Out->SetBoolField(TEXT("scope_quarantined"), false);
    Out->SetNumberField(TEXT("begin_calls"), 0); Out->SetNumberField(TEXT("end_calls"), 0);
    Out->SetBoolField(TEXT("automatic_retry"), false); return Out;
}
void Strings(const TSharedRef<FJsonObject>& Out, const TCHAR* Key, const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Items;
    for (const FString& Value : Values) { Items.Add(MakeShared<FJsonValueString>(Value)); }
    Out->SetArrayField(Key, Items);
}
bool ValidPolicy(const FString& Policy)
{
    return Policy == TEXT("auto") || Policy == TEXT("none") || Policy == TEXT("legacy")
        || Policy == TEXT("preserve") || Policy == TEXT("interchange");
}
void PolicyMetadata(const FRequest& Request, const FDispatch& Dispatch, const TSharedRef<FJsonObject>& Out)
{
    const bool Legacy = Request.InterchangePolicy == TEXT("auto") || Request.InterchangePolicy == TEXT("legacy");
    const bool Import = Request.InterchangePolicy != TEXT("none");
    Out->SetStringField(TEXT("interchange_policy"), Request.InterchangePolicy);
    Out->SetStringField(TEXT("interchange_policy_effective"), Legacy ? TEXT("legacy") :
        Request.InterchangePolicy == TEXT("interchange") ? TEXT("preserve") : *Request.InterchangePolicy);
    Out->SetBoolField(TEXT("interchange_policy_auto_conservative"), Request.InterchangePolicy == TEXT("auto"));
    Out->SetBoolField(TEXT("source_import_declared"), Import);
    Out->SetBoolField(TEXT("standalone_import_ticker_required"), Import);
    Out->SetBoolField(TEXT("direct_interchange_allowed"), Request.InterchangePolicy == TEXT("preserve") || Request.InterchangePolicy == TEXT("interchange"));
    Out->SetBoolField(TEXT("legacy_interchange_bypass_declared"), Legacy);
    Out->SetBoolField(TEXT("interchange_state_preserved_by_policy"), !Legacy);
    Out->SetBoolField(TEXT("interchange_preserved"), !Legacy);
    Out->SetBoolField(TEXT("ticker_dispatch_effective"), Dispatch.StandaloneTicker);
    Out->SetBoolField(TEXT("asset_import_detected"), false);
    Out->SetBoolField(TEXT("asset_import_detection_used"), false);
}
// These focused accident guards preserve the old handler's lexical semantics.
// They are deliberately not a sandbox or an import detector.
FString NormalizeGuard(const FString& Code)
{
    FString Result; Result.Reserve(Code.Len());
    for (TCHAR Character : Code)
    {
        if (!FChar::IsWhitespace(Character)) { Result.AppendChar(FChar::ToLower(Character)); }
    }
    return Result;
}
bool ContainsAny(const FString& Code, const TArray<FString>& Patterns, FString& Match)
{
    for (const FString& Pattern : Patterns)
    {
        if (Code.Contains(Pattern, ESearchCase::CaseSensitive)) { Match = Pattern; return true; }
    }
    return false;
}
bool UnsafePIE(const FString& Code, FString& Match)
{
    for (const TCHAR* Pattern : { TEXT("EditorLevelLibrary.destroy_actor"), TEXT("EditorLevelLibrary.destroy_actors"),
        TEXT("EditorActorSubsystem.destroy_actor"), TEXT("EditorActorSubsystem.destroy_actors") })
    {
        if (Code.Contains(Pattern, ESearchCase::IgnoreCase)) { Match = Pattern; return true; }
    }
    return false;
}
bool BlockedMap(const FString& Normalized, FString& Match)
{
    return ContainsAny(Normalized, {
        TEXT("editorloadingandsavingutils.new_blank_map("), TEXT(".new_blank_map("), TEXT("new_blank_map("),
        TEXT("editorloadingandsavingutils.new_map_from_template("), TEXT(".new_map_from_template("), TEXT("new_map_from_template("),
        TEXT("editorloadingandsavingutils.load_map("), TEXT(".load_map("), TEXT("load_map("),
        TEXT("editorloadingandsavingutils.load_map_with_dialog("), TEXT(".load_map_with_dialog("), TEXT("load_map_with_dialog(") }, Match);
}
bool BlockedDelete(const FString& Normalized, FString& Match)
{
    return ContainsAny(Normalized, { TEXT(".delete_asset("), TEXT(".delete_assets("), TEXT(".delete_loaded_asset("),
        TEXT(".delete_loaded_assets("), TEXT(".delete_directory(") }, Match);
}
bool BlockedShutdown(const FString& Normalized, FString& Match)
{
    if (Normalized.Contains(TEXT(".quit_editor("), ESearchCase::CaseSensitive)) { Match = TEXT(".quit_editor("); return true; }
    if (Normalized.Contains(TEXT("execute_console_command("), ESearchCase::CaseSensitive) &&
        (Normalized.Contains(TEXT("'quit_editor'"), ESearchCase::CaseSensitive) || Normalized.Contains(TEXT("\"quit_editor\""), ESearchCase::CaseSensitive)))
    { Match = TEXT("QUIT_EDITOR console command"); return true; }
    return false;
}
bool BlockedSelector(const FString& Normalized, FString& Match)
{
    const TArray<FString> Helpers = {
        TEXT("pcgattributepropertyselectorblueprinthelpers.set_attribute_name("), TEXT("pcgattributepropertyselectorblueprinthelpers.setattributename("),
        TEXT("pcgattributepropertyselectorblueprinthelpers.set_property_name("), TEXT("pcgattributepropertyselectorblueprinthelpers.setpropertyname("),
        TEXT("pcgattributepropertyselectorblueprinthelpers.set_domain_name("), TEXT("pcgattributepropertyselectorblueprinthelpers.setdomainname("),
        TEXT("pcgattributepropertyselectorblueprinthelpers.set_point_property("), TEXT("pcgattributepropertyselectorblueprinthelpers.setpointproperty("),
        TEXT("pcgattributepropertyselectorblueprinthelpers.set_extra_property("), TEXT("pcgattributepropertyselectorblueprinthelpers.setextraproperty(") };
    if (ContainsAny(Normalized, Helpers, Match)) { return true; }
    const TArray<FString> Mutators = { TEXT(".set_attribute_name("), TEXT(".setattributename("), TEXT(".set_property_name("),
        TEXT(".setpropertyname("), TEXT(".set_domain_name("), TEXT(".setdomainname("), TEXT(".set_point_property("),
        TEXT(".setpointproperty("), TEXT(".set_extra_property("), TEXT(".setextraproperty("), TEXT(".update(") };
    if (Normalized.Contains(TEXT("pcgattributeproperty"), ESearchCase::CaseSensitive) && ContainsAny(Normalized, Mutators, Match)) { return true; }
    TArray<FString> Reads, Writes;
    for (const TCHAR* Property : { TEXT("input_attribute"), TEXT("match_attribute"), TEXT("max_distance_input_attribute"),
        TEXT("input_weight_attribute"), TEXT("weight_attribute"), TEXT("set_target"), TEXT("input_source"), TEXT("output_target"),
        TEXT("target_attribute"), TEXT("threshold_attribute"), TEXT("source_weight_attribute"), TEXT("target_weight_attribute"),
        TEXT("merge_weight_attribute"), TEXT("comparison_source") })
    {
        Reads.Add(FString::Printf(TEXT("get_editor_property('%s')"), Property));
        Reads.Add(FString::Printf(TEXT("get_editor_property(\"%s\")"), Property));
        Writes.Add(FString::Printf(TEXT("set_editor_property('%s',"), Property));
        Writes.Add(FString::Printf(TEXT("set_editor_property(\"%s\","), Property));
    }
    if (ContainsAny(Normalized, Writes, Match)) { return true; }
    FString Read;
    if (ContainsAny(Normalized, Reads, Read) && ContainsAny(Normalized, Mutators, Match))
    { Match = Read + TEXT(" with ") + Match; return true; }
    return false;
}

struct FCVarState
{
    FString Name, Value;
    IConsoleVariable* Variable = nullptr;
    EConsoleVariableFlags Flags = ECVF_Default;
    bool Required = false, Disable = false, Captured = false;
};
// Mandatory native console cleanup only. It never examines script-held UObjects.
struct FLegacyImportScope
{
    TArray<FCVarState> States;
    TArray<TSharedPtr<FJsonValue>> Observations;
    bool RestoreAttempted = false, RestoreVerified = false, ActivationAttempted = false;
    TArray<FString> SetupErrors, SetupWarnings, RestoreErrors;

    FLegacyImportScope()
    {
        auto Add = [this](const TCHAR* Name, bool Required, bool Disable)
        { FCVarState Row; Row.Name = Name; Row.Required = Required; Row.Disable = Disable; States.Add(MoveTemp(Row)); };
        Add(TEXT("Interchange.FeatureFlags.Import.Enable"), true, true);
        Add(TEXT("Interchange.FeatureFlags.Import.FBX"), false, true);
        Add(TEXT("Interchange.FeatureFlags.Import.FBX.ToLevel"), false, true);
        Add(TEXT("Interchange.FeatureFlags.Import.OBJ"), false, true);
        for (const TCHAR* Format : { TEXT("BMP"), TEXT("DDS"), TEXT("EXR"), TEXT("HDR"), TEXT("JPG"),
            TEXT("PNG"), TEXT("PSD"), TEXT("TGA"), TEXT("TIFF"), TEXT("UEJPEG") })
        { Add(*(FString(TEXT("Interchange.FeatureFlags.Import.")) + Format), false, false); }
    }
    ~FLegacyImportScope() { if (ActivationAttempted && !RestoreAttempted) { Restore(); } }
    bool Registry(const FCVarState& State) const
    { return IsInGameThread() && !IsEngineExitRequested() && IConsoleManager::Get().FindConsoleVariable(*State.Name) == State.Variable; }
    bool Activate(TFunctionRef<bool()> Current)
    {
        ActivationAttempted = true;
        if (!Current()) { SetupErrors.Add(TEXT("Execution admission changed before legacy scope")); return false; }
        // Capture all entries before changing any value, including optional texture history.
        for (FCVarState& State : States)
        {
            auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("name"), State.Name); Observations.Add(MakeShared<FJsonValueObject>(Row));
            State.Variable = IConsoleManager::Get().FindConsoleVariable(*State.Name);
            Row->SetBoolField(TEXT("available"), State.Variable != nullptr);
            if (!State.Variable)
            {
                if (State.Required) { SetupErrors.Add(TEXT("Required Interchange CVar missing: ") + State.Name); }
                else if (State.Disable) { SetupWarnings.Add(TEXT("Optional Interchange CVar missing: ") + State.Name); }
                continue;
            }
            State.Value = State.Variable->GetString(); State.Flags = State.Variable->GetFlags(); State.Captured = true;
            Row->SetStringField(TEXT("value_before"), State.Value); Row->SetNumberField(TEXT("flags_before"), static_cast<uint32>(State.Flags));
            if (!Registry(State) || !Current()) { SetupErrors.Add(TEXT("CVar/admission changed during capture: ") + State.Name); return false; }
        }
        if (!SetupErrors.IsEmpty()) { return false; }
        for (FCVarState& State : States)
        {
            if (!State.Captured || !State.Disable) { continue; }
            if (!Registry(State) || !Current()) { SetupErrors.Add(TEXT("CVar/admission changed before setter: ") + State.Name); return false; }
            // Constructor priority is valid too; UE58 SetWithCurrentPriority defaults exclude it.
            State.Variable->Set(false, static_cast<EConsoleVariableFlags>(State.Flags & ECVF_SetByMask));
            if (!Registry(State) || !Current()) { SetupErrors.Add(TEXT("CVar/admission changed after setter: ") + State.Name); return false; }
            if (State.Variable->GetBool() || State.Variable->GetFlags() != State.Flags)
            { SetupErrors.Add(TEXT("Failed to disable while preserving flags: ") + State.Name); return false; }
        }
        // A later setter callback may have changed an earlier captured entry.
        for (const FCVarState& State : States)
        {
            if (!State.Captured) { continue; }
            if (!Registry(State) || !Current()) { SetupErrors.Add(TEXT("Final scope admission/registry differs: ") + State.Name); return false; }
            if (State.Variable->GetFlags() != State.Flags || (State.Disable ? State.Variable->GetBool() : State.Variable->GetString() != State.Value))
            { SetupErrors.Add(TEXT("Final scope values differ: ") + State.Name); return false; }
        }
        return true;
    }
    bool Restore()
    {
        if (RestoreAttempted) { return RestoreVerified; }
        RestoreAttempted = true;
        for (int32 Index = States.Num() - 1; Index >= 0; --Index)
        {
            FCVarState& State = States[Index]; if (!State.Captured) { continue; }
            auto Row = Observations[Index]->AsObject();
            if (!Registry(State)) { RestoreErrors.Add(TEXT("CVar unsafe/removed before restore: ") + State.Name); Row->SetBoolField(TEXT("restore_skipped_unsafe"), true); continue; }
            if (State.Variable->GetString() != State.Value || State.Variable->GetFlags() != State.Flags)
            {
                const auto Priority = static_cast<EConsoleVariableFlags>(State.Flags & ECVF_SetByMask);
                bool Usable = true;
                for (int32 Attempt = 0; Attempt < 16; ++Attempt)
                {
                    const auto Visible = static_cast<EConsoleVariableFlags>(State.Variable->GetFlags() & ECVF_SetByMask);
                    if (Visible == Priority) { break; }
                    State.Variable->Unset(Visible);
                    if (!Registry(State)) { Usable = false; break; }
                    if ((State.Variable->GetFlags() & ECVF_SetByMask) == Visible) { break; }
                }
                if (!Usable) { RestoreErrors.Add(TEXT("CVar replaced during priority restore: ") + State.Name); continue; }
                if ((State.Variable->GetFlags() & ECVF_SetByMask) != Priority)
                { RestoreErrors.Add(TEXT("Captured priority not recovered: ") + State.Name); }
                State.Variable->SetFlags(State.Flags);
                if (!Registry(State)) { RestoreErrors.Add(TEXT("CVar replaced during flag restore: ") + State.Name); continue; }
                State.Variable->Set(*State.Value, Priority);
                if (!Registry(State)) { RestoreErrors.Add(TEXT("CVar replaced during value restore: ") + State.Name); continue; }
            }
            const bool Same = State.Variable->GetString() == State.Value && State.Variable->GetFlags() == State.Flags;
            Row->SetBoolField(TEXT("restored"), Same); Row->SetStringField(TEXT("value_after"), State.Variable->GetString());
            Row->SetNumberField(TEXT("flags_after"), static_cast<uint32>(State.Variable->GetFlags()));
            if (!Same) { RestoreErrors.Add(TEXT("Value/flags restoration differs: ") + State.Name); }
        }
        // Reverse-order restoration also invokes callbacks: verify all captured
        // entries again after the last setter, without trying another cleanup pass.
        for (const FCVarState& State : States)
        {
            if (!State.Captured) { continue; }
            if (!Registry(State)) { RestoreErrors.Add(TEXT("Final restore registry differs: ") + State.Name); continue; }
            if (State.Variable->GetString() != State.Value || State.Variable->GetFlags() != State.Flags)
            { RestoreErrors.Add(TEXT("Final restored value/flags differ: ") + State.Name); }
        }
        RestoreVerified = RestoreErrors.IsEmpty(); return RestoreVerified;
    }
    bool VerifyAfterTransaction()
    {
        // EndTransaction broadcasts callbacks after the original restoration.
        // Observe again, but never replay setters to conceal a later change.
        for (const FCVarState& State : States)
        {
            if (!State.Captured) { continue; }
            if (!Registry(State)) { RestoreErrors.Add(TEXT("Post-transaction CVar registry differs: ") + State.Name); continue; }
            if (State.Variable->GetString() != State.Value || State.Variable->GetFlags() != State.Flags)
            { RestoreErrors.Add(TEXT("Post-transaction restored CVar differs: ") + State.Name); }
        }
        RestoreVerified = RestoreVerified && RestoreErrors.IsEmpty(); return RestoreVerified;
    }
    void Publish(const TSharedRef<FJsonObject>& Out) const
    {
        Out->SetArrayField(TEXT("interchange_cvar_scope"), Observations);
        Strings(Out, TEXT("interchange_setup_errors"), SetupErrors); Strings(Out, TEXT("interchange_setup_warnings"), SetupWarnings);
        Strings(Out, TEXT("interchange_restore_errors"), RestoreErrors);
        Out->SetBoolField(TEXT("interchange_restore_attempted"), RestoreAttempted);
        Out->SetBoolField(TEXT("interchange_restore_verified"), RestoreVerified);
    }
};
void Unknown(const TSharedRef<FJsonObject>& Out, const TCHAR* Code, const FString& Error, const TCHAR* Stage)
{
    Out->SetBoolField(TEXT("success"), false); Out->SetStringField(TEXT("error_code"), Code); Out->SetStringField(TEXT("error"), Error);
    Out->SetField(TEXT("mutation_performed"), MakeShared<FJsonValueNull>());
    Out->SetBoolField(TEXT("mutation_state_unknown"), true); Out->SetBoolField(TEXT("change_state_known"), false);
    Out->SetBoolField(TEXT("do_not_retry"), true); Out->SetBoolField(TEXT("workflow_recovery_required"), true);
    Out->SetStringField(TEXT("failed_stage"), Stage); Out->SetStringField(TEXT("failed_stage_effect"), TEXT("mutation"));
}
}

TSharedRef<FJsonObject> Unexecuted(const FString& Code, const FString& Error)
{
    auto Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("success"), false);
    Out->SetStringField(TEXT("error_code"), Code); Out->SetStringField(TEXT("error"), Error);
    Out->SetBoolField(TEXT("python_executed"), false); Out->SetBoolField(TEXT("mutation_attempted"), false);
    Out->SetBoolField(TEXT("mutation_performed"), false); Out->SetBoolField(TEXT("mutation_state_unknown"), false);
    Out->SetBoolField(TEXT("change_state_known"), true); Out->SetBoolField(TEXT("automatic_retry"), false);
    if (Code == TEXT("invalid_interchange_policy"))
    { Strings(Out, TEXT("allowed_interchange_policies"), { TEXT("auto"), TEXT("interchange"), TEXT("legacy"), TEXT("none"), TEXT("preserve") }); }
    return Out;
}
static bool ParseRequest(const TSharedRef<FJsonObject>& Input, FRequest& Request, FString& ErrorCode, FString& Error, bool Native)
{
    Request = FRequest();
    if (Native) { Request.Mode = TEXT("ExecuteStatement"); }
    TSet<FString> Allowed = { TEXT("code"), TEXT("mode"), TEXT("defer_to_ticker"),
        TEXT("allow_unsafe_editor_scripting_during_pie"), TEXT("interchange_policy") };
    if (Native) { Allowed.Add(TEXT("transaction_policy")); Allowed.Add(TEXT("allow_unsafe_pcg_selector_python")); }
    // Original Python has five caller values plus injected FastMCP Context; no ctx carrier.
    for (const auto& Pair : Input->Values)
    {
        const FString Key(Pair.Key.ToView());
        if (!Allowed.Contains(Key)) { ErrorCode = TEXT("invalid_python_argument"); Error = TEXT("Unknown public execute_unreal_python argument: ") + Key; return false; }
    }
    if (!Input->TryGetStringField(TEXT("code"), Request.Code)) { ErrorCode = TEXT("missing_python_code"); Error = TEXT("Missing 'code' parameter"); return false; }
    auto ReadString = [&](const TCHAR* Key, FString& Value)
    { if (!Input->HasField(Key)) { return true; } const auto Item = Input->TryGetField(Key); return Item.IsValid() && Item->Type == EJson::String && Input->TryGetStringField(Key, Value); };
    auto ReadBool = [&](const TCHAR* Key, bool& Value)
    { if (!Input->HasField(Key)) { return true; } const auto Item = Input->TryGetField(Key); return Item.IsValid() && Item->Type == EJson::Boolean && Input->TryGetBoolField(Key, Value); };
    if (!ReadString(TEXT("mode"), Request.Mode) || !ReadString(TEXT("interchange_policy"), Request.InterchangePolicy)
        || !ReadBool(TEXT("defer_to_ticker"), Request.DeferToTicker) || !ReadBool(TEXT("allow_unsafe_editor_scripting_during_pie"), Request.AllowUnsafeEditorScriptingDuringPIE))
    { ErrorCode = TEXT("invalid_python_argument"); Error = TEXT("Original strings and booleans required"); return false; }
    if (Native && (!ReadString(TEXT("transaction_policy"), Request.TransactionPolicy)
        || !ReadBool(TEXT("allow_unsafe_pcg_selector_python"), Request.AllowUnsafePCGSelectorPython)))
    { ErrorCode = TEXT("invalid_python_argument"); Error = TEXT("Native transaction string and unsafe-PCG Boolean required"); return false; }
    if (Native && Request.TransactionPolicy != TEXT("command") && Request.TransactionPolicy != TEXT("script"))
    { ErrorCode = TEXT("invalid_transaction_policy"); Error = TEXT("transaction_policy must be command or script."); return false; }
    Request.InterchangePolicy.TrimStartAndEndInline(); Request.InterchangePolicy.ToLowerInline();
    if (!ValidPolicy(Request.InterchangePolicy))
    { ErrorCode = TEXT("invalid_interchange_policy"); Error = TEXT("interchange_policy must be one of: auto, none, legacy, preserve, interchange"); return false; }
    return true;
}
bool ParsePublicRequest(const TSharedRef<FJsonObject>& Input, FRequest& Request, FString& ErrorCode, FString& Error)
{ return ParseRequest(Input, Request, ErrorCode, Error, false); }
bool ParseNativeRequest(const TSharedRef<FJsonObject>& Input, FRequest& Request, FString& ErrorCode, FString& Error)
{ return ParseRequest(Input, Request, ErrorCode, Error, true); }
bool RequiresStandaloneTicker(const FRequest& Request)
{ return Request.DeferToTicker || Request.InterchangePolicy != TEXT("none"); }
bool IsExecuting() { return IsInGameThread() && ActiveExecutionDepth > 0; }
void Activate() { if (IsInGameThread()) { ++AdmissionGeneration; AdmissionActive = true; } }
void Deactivate() { if (IsInGameThread()) { AdmissionActive = false; ++AdmissionGeneration; } }

TSharedRef<FJsonObject> BeginBatchTransaction(const FString& OperationId)
{
    if (!IsInGameThread() || GExitPurge || IsEngineExitRequested() || IsGarbageCollecting()
        || UE::IsSavingPackage() || !AdmissionActive || !Live(GEditor)
        || !GEditor->PlayWorld.IsResolved() || GEditor->PlayWorld.Get() != nullptr || GIsTransacting)
    { return BatchRejected(TEXT("batch_transaction_editor_unavailable"), TEXT("Idle current Editor transaction scope is unavailable; no Begin call.")); }
    if (BatchTransaction)
    { return BatchRejected(TEXT("batch_transaction_scope_exists"), TEXT("A previous batch transaction is active or quarantined; no new Begin or close is dispatched.")); }
    if (!GEditor->Trans.IsResolved() || !Live(GEditor->Trans.Get())
        || GEditor->Trans->GetClass() != UTransBuffer::StaticClass())
    { return BatchRejected(TEXT("batch_transaction_class_pending"), TEXT("Exact native UTransBuffer required; custom transactors remain incomplete.")); }
    auto Owner = MakeUnique<FBatchTransaction>();
    Owner->Editor.Reset(GEditor); Owner->Buffer.Reset(static_cast<UTransBuffer*>(GEditor->Trans.Get()));
    Owner->World.Reset(GEditor->GetEditorWorldContext().World());
    Owner->Generation = AdmissionGeneration; Owner->Frame = GFrameCounter;
    Owner->ScopeId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Owner->OwnerOperationId = OperationId; Owner->BeforeDepth = Owner->Buffer->ActiveCount;
    if (!Owner->EditorCurrent() || Owner->BeforeDepth < 0 || Owner->BeforeDepth == MAX_int32
        || (Owner->BeforeDepth > 0 && (Owner->Buffer->UndoCount || Owner->Buffer->UndoBuffer.IsEmpty())))
    { return BatchRejected(TEXT("batch_transaction_initial_state_invalid"), TEXT("Actual initial transaction owner/depth is unavailable; no Begin call.")); }
    // Existing outer transactions retain their identity. A fresh batch uses a
    // unique context so a Begin callback cannot replace it unnoticed.
    const FTransaction* Prior = Owner->BeforeDepth ? &Owner->Buffer->UndoBuffer.Last().Get() : nullptr;
    FGuid PriorId, PriorOperation;
    if (Prior)
    {
        const FTransactionContext Copied = Prior->GetContext();
        PriorId = Copied.TransactionId; PriorOperation = Copied.OperationId; Owner->Context = Copied.Context;
    }
    else { Owner->Context = TEXT("CubelessBatch_") + Owner->ScopeId; }
    BatchTransaction = MoveTemp(Owner); auto& Scope = *BatchTransaction;
    Scope.BeginCalled = true; Scope.ControlBusy = true;
    Scope.BeginIndex = Scope.Editor->BeginTransaction(*Scope.Context, FText::FromString(TEXT("MCP batch")), nullptr);
    Scope.ControlBusy = false;
    if (Scope.EditorCurrent() && Scope.BeginIndex == Scope.BeforeDepth
        && Scope.Buffer->ActiveCount == Scope.BeforeDepth + 1 && !Scope.Buffer->UndoCount
        && !Scope.Buffer->UndoBuffer.IsEmpty())
    {
        Scope.Transaction = &Scope.Buffer->UndoBuffer.Last().Get();
        const FTransactionContext Copied = Scope.Transaction->GetContext();
        if (Copied.Context == Scope.Context && (!Prior || (Scope.Transaction == Prior
            && Copied.TransactionId == PriorId && Copied.OperationId == PriorOperation)))
        {
            Scope.TransactionId = Copied.TransactionId; Scope.TransactionOperationId = Copied.OperationId;
            Scope.ExpectedDepth = Scope.BeforeDepth + 1; Scope.Ready = true;
            return Scope.Receipt(true);
        }
    }
    Scope.Quarantined = true;
    Scope.Failure = TEXT("Begin returned without the captured expected transaction role; no speculative End or retry.");
    return Scope.Receipt(false);
}

TSharedRef<FJsonObject> ObserveBatchTransaction(const FString& ScopeId, const FString& Action)
{
    // Invalid control requests never clear, adopt or close an existing scope.
    if (!IsInGameThread())
    { return BatchRejected(TEXT("batch_transaction_wrong_thread"), TEXT("Batch transaction controls require the original game thread.")); }
    if (!BatchTransaction || BatchTransaction->ScopeId != ScopeId
        || (Action != TEXT("check") && Action != TEXT("end") && Action != TEXT("abandon")))
    { return BatchRejected(TEXT("batch_transaction_scope_mismatch"), TEXT("Unknown batch scope/control; no SDK operation dispatched.")); }
    auto& Scope = *BatchTransaction;
    // Begin/End broadcasts may reenter Python before ActiveCount changes. The
    // reentrant request must neither close nor clear the outer owner's storage.
    if (Scope.ControlBusy || Scope.EndCalled)
    { return BatchRejected(TEXT("batch_transaction_control_busy"), TEXT("Native batch Begin/End is already entered; no reentrant control or scope release.")); }
    if (Action == TEXT("abandon"))
    {
        Scope.Quarantined = true; Scope.Failure = TEXT("Batch observer was lost; transaction retained without any further Editor SDK call.");
        return Scope.Receipt(false);
    }
    if (!Scope.Current())
    {
        Scope.Quarantined = true; Scope.Failure = TEXT("Batch Editor/frame/transaction/depth/ID/context changed; no End, continuation SDK or retry.");
        return Scope.Receipt(false);
    }
    if (Action == TEXT("check")) { return Scope.Receipt(true); }
    // Scope ownership, not child success, decides whether mandatory closing is
    // safe. Never Cancel: known/partial child mutations remain user-undoable.
    // End callbacks may remove the buffer entry. Retain this exact transaction
    // for the post-End no-op/transient versus recorded-Undo distinction.
    const TSharedRef<FTransaction> EndingTransaction = Scope.Buffer->UndoBuffer.Last();
    Scope.EndCalled = true; Scope.ControlBusy = true;
    const int32 EndDepth = Scope.Editor->EndTransaction();
    Scope.ControlBusy = false;
    const bool Closed = Scope.EditorCurrent() && EndDepth == Scope.ExpectedDepth
        && Scope.Buffer->ActiveCount == Scope.BeforeDepth;
    bool WasTransient = false;
    if (Closed && Scope.BeforeDepth == 0)
    {
        WasTransient = EndingTransaction->IsTransient();
        if (!Scope.EditorCurrent() || Scope.Buffer->ActiveCount != 0)
        { Scope.Quarantined = true; }
        else if (WasTransient)
        {
            // UTransBuffer::End removes only this ineffective entry and may
            // restore the previous Redo stack. Empty buffer is a normal case.
            Scope.Quarantined = Scope.Buffer->UndoBuffer.Contains(EndingTransaction);
        }
        else if (Scope.Buffer->UndoCount || Scope.Buffer->UndoBuffer.IsEmpty()
            || &Scope.Buffer->UndoBuffer.Last().Get() != &EndingTransaction.Get())
        { Scope.Quarantined = true; }
        else
        {
            const FTransactionContext Copied = EndingTransaction->GetContext();
            // EndOperation invalidates OperationId only on outermost End.
            Scope.Quarantined = Copied.TransactionId != Scope.TransactionId
                || Copied.OperationId.IsValid() || Copied.Context != Scope.Context;
        }
    }
    if (Closed && Scope.BeforeDepth)
    {
        // A nested batch must leave exactly the prior outer operation alive.
        if (Scope.Buffer->UndoCount || Scope.Buffer->UndoBuffer.IsEmpty()
            || &Scope.Buffer->UndoBuffer.Last().Get() != Scope.Transaction)
        { Scope.Quarantined = true; }
        else
        {
            const FTransactionContext Copied = Scope.Transaction->GetContext();
            Scope.Quarantined = Copied.TransactionId != Scope.TransactionId
                || Copied.OperationId != Scope.TransactionOperationId || Copied.Context != Scope.Context;
        }
    }
    if (!Closed || Scope.Quarantined)
    {
        Scope.Quarantined = true; Scope.Failure = TEXT("End returned but original depth/owner restoration was not certified; no second End or retry.");
        auto Out = Scope.Receipt(false); Out->SetNumberField(TEXT("end_return_depth"), EndDepth); return Out;
    }
    auto Out = Scope.Receipt(true); Out->SetNumberField(TEXT("end_return_depth"), EndDepth);
    Out->SetNumberField(TEXT("depth_after"), Scope.Buffer->ActiveCount);
    Out->SetBoolField(TEXT("scope_closed"), true);
    Out->SetBoolField(TEXT("outer_noop_or_transient"), Scope.BeforeDepth == 0 && WasTransient);
    Out->SetBoolField(TEXT("recorded_undo_entry_preserved"), Scope.BeforeDepth == 0 && !WasTransient);
    Out->SetBoolField(TEXT("nested_outer_identity_preserved"), Scope.BeforeDepth > 0);
    // End may legally remove a no-op/transient outer transaction and restore
    // its previous Redo stack (EditorTransaction.cpp UTransBuffer::End).
    Out->SetNumberField(TEXT("undo_buffer_count_after"), Scope.Buffer->UndoBuffer.Num());
    Out->SetNumberField(TEXT("redo_count_after"), Scope.Buffer->UndoCount);
    BatchTransaction.Reset(); return Out;
}

TSharedRef<FJsonObject> Execute(const FRequest& Request, const FDispatch& Dispatch)
{
    auto Error = [&](const TCHAR* Code, const FString& Message)
    { auto Out = Unexecuted(Code, Message); if (ValidPolicy(Request.InterchangePolicy)) { PolicyMetadata(Request, Dispatch, Out); } return Out; };
    if (IsInGameThread() && BatchTransaction && BatchTransaction->Quarantined)
    { return Error(TEXT("batch_transaction_quarantined"), TEXT("A batch transaction requires reconciliation; no Python execution or new command transaction.")); }
    if (!IsInGameThread() || IsGarbageCollecting() || IsEngineExitRequested() || !Live(GEditor))
    { return Error(TEXT("editor_shutdown_in_progress"), TEXT("Live Editor game thread outside exit/GC required before Python execution")); }
    if (!AdmissionActive) { return Error(TEXT("python_execution_not_dispatched"), TEXT("Editor module Python execution admission is inactive")); }
    const uint64 Generation = AdmissionGeneration;
    TStrongObjectPtr<UEditorEngine> Editor(GEditor);
    auto EditorCurrent = [&]()
    { return IsInGameThread() && !IsGarbageCollecting() && !IsEngineExitRequested() && Live(Editor.Get()) && GEditor == Editor.Get(); };
    auto Current = [&]() { return EditorCurrent() && AdmissionActive && AdmissionGeneration == Generation && Dispatch.IsCurrent && Dispatch.IsCurrent(); };
    if (!Current()) { return Error(TEXT("python_execution_admission_changed"), TEXT("Exact retained execution owner is unavailable")); }
    if (!ValidPolicy(Request.InterchangePolicy)) { return Error(TEXT("invalid_interchange_policy"), TEXT("Invalid structured import policy")); }
    const bool Legacy = Request.InterchangePolicy == TEXT("auto") || Request.InterchangePolicy == TEXT("legacy");
    const bool Import = Request.InterchangePolicy != TEXT("none");
    if (RequiresStandaloneTicker(Request) && (!Dispatch.StandaloneTicker || Dispatch.SynchronousNested || IsExecuting()))
    { return Error(TEXT("python_import_requires_standalone_ticker"), TEXT("Declared import/defer requires standalone next-tick dispatch; synchronous Batch/reentrant execution cannot wait for its ticker")); }
    if (Request.TransactionPolicy != TEXT("command") && Request.TransactionPolicy != TEXT("script"))
    { return Error(TEXT("invalid_transaction_policy"), TEXT("transaction_policy must be command or script.")); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized() || !Current())
    { return Error(TEXT("python_plugin_unavailable"), TEXT("Existing initialized PythonScriptPlugin required; no force-enable or fallback")); }
    const bool AlreadyImporting = UInterchangeManager::IsImporting();
    if (!Current()) { return Error(TEXT("python_execution_admission_changed"), TEXT("Admission changed during import preflight")); }
    if (AlreadyImporting)
    { auto Out = Error(TEXT("interchange_import_in_progress"), TEXT("execute_python is unavailable while an Interchange import is active")); Out->SetBoolField(TEXT("interchange_import_active"), true); Out->SetBoolField(TEXT("retry_after_interchange_idle"), true); return Out; }
    FString Match; const FString Normalized = NormalizeGuard(Request.Code);
    if (!Request.AllowUnsafeEditorScriptingDuringPIE && Editor->GetPIEWorldContext() && UnsafePIE(Request.Code, Match))
    { return Error(TEXT("unsafe_editor_scripting_during_pie"), TEXT("Unsafe editor scripting during PIE/SIE: ") + Match); }
    if (BlockedMap(Normalized, Match)) { return Error(TEXT("python_map_transition_blocked"), TEXT("Python map transition blocked; use native map lifecycle: ") + Match); }
    if (BlockedDelete(Normalized, Match)) { return Error(TEXT("python_asset_delete_blocked"), TEXT("Python Force Delete blocked; use reviewed native cleanup: ") + Match); }
    if (BlockedShutdown(Normalized, Match)) { return Error(TEXT("python_editor_shutdown_blocked"), TEXT("Python editor shutdown blocked: ") + Match); }
    if (!Request.AllowUnsafePCGSelectorPython && BlockedSelector(Normalized, Match))
    { return Error(TEXT("unsafe_pcg_selector_python"), TEXT("Unsafe PCG selector mutation blocked; use native selector authoring: ") + Match); }

    // Explicit close replaces FScopedTransaction's unconditional destructor GEditor getter.
    if (!Editor->Trans.IsResolved()) { return Error(TEXT("python_transaction_unavailable"), TEXT("Editor transaction slot is unresolved")); }
    TStrongObjectPtr<UTransactor> Transactor(Editor->Trans.Get());
    auto CleanupTransactionCurrent = [&]()
    { return EditorCurrent() && Editor->Trans.IsResolved() && Editor->Trans.Get() == Transactor.Get()
        && (!Transactor.Get() || Live(Transactor.Get())); };
    auto TransactionCurrent = [&]() { return Current() && CleanupTransactionCurrent(); };
    if (!TransactionCurrent()) { return Error(TEXT("python_transaction_unavailable"), TEXT("Captured transaction role unavailable")); }
    if (Request.TransactionPolicy == TEXT("script") && (Dispatch.SynchronousNested || IsExecuting() || (Transactor.Get() && Transactor->IsActive())))
    { return Error(TEXT("script_transaction_requires_standalone"), TEXT("Script-managed transactions require no active editor/batch transaction")); }
    auto Out = Unexecuted(TEXT("python_execution_not_started"), TEXT("Python has not been dispatched")); PolicyMetadata(Request, Dispatch, Out);
    Out->SetBoolField(TEXT("explicit_compile_requested"), false); Out->SetBoolField(TEXT("explicit_save_requested"), false);
    Out->SetBoolField(TEXT("full_effect_state_known"), false); Out->SetBoolField(TEXT("atomic_rollback"), false);
    FExecutionDepthScope Executing;
    int32 TransactionIndex = INDEX_NONE;
    const bool CanTransact = Request.TransactionPolicy == TEXT("command") && Editor->CanTransact();
    if (!TransactionCurrent()) { return Error(TEXT("python_transaction_unavailable"), TEXT("Captured role changed during CanTransact; no transaction or Python")); }
    UTransBuffer* Buffer = nullptr;
    const FTransaction* Transaction = nullptr;
    FGuid TransactionId, OperationId;
    int32 ExpectedDepth = 0;
    bool OwnTransactionReady = false;
    FString TransactionContext;
    auto OwnTransactionCurrent = [&]()
    {
        if (TransactionIndex == INDEX_NONE) { return true; }
        if (!OwnTransactionReady || !CleanupTransactionCurrent() || Transactor.Get() != Buffer
            || Buffer->GetClass() != UTransBuffer::StaticClass() || Buffer->ActiveCount != ExpectedDepth
            || Buffer->UndoCount != 0 || Buffer->UndoBuffer.IsEmpty()) { return false; }
        const FTransaction* Actual = &Buffer->UndoBuffer.Last().Get();
        if (Actual != Transaction) { return false; }
        const FTransactionContext Copied = Actual->GetContext(); // Exact stock FTransaction, inline pure copy.
        return Copied.TransactionId == TransactionId && Copied.OperationId == OperationId && Copied.Context == TransactionContext;
    };
    if (CanTransact && !GIsTransacting)
    {
        if (!Transactor.Get() || Transactor->GetClass() != UTransBuffer::StaticClass())
        { return Error(TEXT("python_transaction_class_pending"), TEXT("Command transaction requires the installed native UTransBuffer; custom UTransactor remains incomplete")); }
        Buffer = static_cast<UTransBuffer*>(Transactor.Get());
        const int32 BeforeDepth = Buffer->ActiveCount;
        if (BeforeDepth < 0 || BeforeDepth == MAX_int32 || (BeforeDepth > 0 && (Buffer->UndoCount != 0 || Buffer->UndoBuffer.IsEmpty())))
        { return Error(TEXT("python_transaction_unavailable"), TEXT("Actual transaction depth/buffer is inconsistent")); }
        // Nested command execution belongs to the existing transaction. A fresh
        // transaction gets an internal unique context so a Begin callback cannot
        // replace it and have that replacement silently adopted as our baseline.
        const FTransaction* Prior = BeforeDepth > 0 ? &Buffer->UndoBuffer.Last().Get() : nullptr;
        FGuid PriorId, PriorOperation;
        if (Prior)
        {
            const FTransactionContext Copied = Prior->GetContext();
            PriorId = Copied.TransactionId; PriorOperation = Copied.OperationId; TransactionContext = Copied.Context;
        }
        else { TransactionContext = TEXT("CubelessUP_") + FGuid::NewGuid().ToString(EGuidFormats::Digits); }
        TransactionIndex = Editor->BeginTransaction(*TransactionContext, FText::FromString(TEXT("MCP execute_python")), nullptr);
        Out->SetNumberField(TEXT("transaction_begin_index"), TransactionIndex);
        if (TransactionCurrent() && TransactionIndex == BeforeDepth && Buffer->ActiveCount == BeforeDepth + 1
            && Buffer->UndoCount == 0 && !Buffer->UndoBuffer.IsEmpty())
        {
            Transaction = &Buffer->UndoBuffer.Last().Get();
            const FTransactionContext Copied = Transaction->GetContext();
            if (Copied.Context == TransactionContext && (!Prior || (Transaction == Prior && Copied.TransactionId == PriorId && Copied.OperationId == PriorOperation)))
            {
                TransactionId = Copied.TransactionId; OperationId = Copied.OperationId; ExpectedDepth = BeforeDepth + 1;
                OwnTransactionReady = true;
                Out->SetStringField(TEXT("transaction_id"), TransactionId.ToString());
                Out->SetStringField(TEXT("transaction_operation_id"), OperationId.ToString());
                Out->SetNumberField(TEXT("transaction_expected_depth"), ExpectedDepth);
            }
        }
    }
    auto Continue = [&]() { return TransactionCurrent() && OwnTransactionCurrent(); };
    auto CloseTransaction = [&]()
    {
        if (TransactionIndex == INDEX_NONE) { return true; }
        // Check actual native identity/ID/depth BEFORE End, not after closing an
        // arbitrary transaction the script may have opened in place of ours.
        if (!OwnTransactionCurrent())
        { Out->SetBoolField(TEXT("transaction_close_skipped_unknown"), true); return false; }
        const int32 EndDepth = Editor->EndTransaction(); Out->SetNumberField(TEXT("transaction_end_depth"), EndDepth);
        const int32 PriorDepth = TransactionIndex;
        const bool ExpectedEndDepth = EndDepth == PriorDepth + 1;
        TransactionIndex = INDEX_NONE;
        if (!CleanupTransactionCurrent() || !ExpectedEndDepth || Buffer->ActiveCount != PriorDepth) { return false; }
        if (PriorDepth > 0)
        {
            if (Buffer->UndoCount != 0 || Buffer->UndoBuffer.IsEmpty() || &Buffer->UndoBuffer.Last().Get() != Transaction) { return false; }
            const FTransactionContext Copied = Buffer->UndoBuffer.Last()->GetContext();
            if (Copied.TransactionId != TransactionId || Copied.OperationId != OperationId || Copied.Context != TransactionContext) { return false; }
        }
        return true;
    };
    FLegacyImportScope Scope;
    if (!Continue())
    { Unknown(Out, TEXT("python_execution_admission_changed"), TEXT("Admission changed after transaction Begin; no Python dispatch"), TEXT("begin_transaction")); CloseTransaction(); return Out; }
    const bool Activated = !Legacy || Scope.Activate(Continue);
    if (!Activated)
    {
        const bool Restored = Scope.Restore(); Scope.Publish(Out);
        const bool Closed = CloseTransaction();
        Out->SetStringField(TEXT("error_code"), TEXT("legacy_import_guard_setup_failed"));
        Out->SetStringField(TEXT("error"), Scope.SetupErrors.IsEmpty() ? TEXT("Failed to activate legacy import scope") : Scope.SetupErrors[0]);
        if (!Restored || !Closed || !Current()) { Unknown(Out, TEXT("legacy_import_guard_setup_failed"), TEXT("Legacy setup/transaction cleanup outcome unknown; Python not dispatched"), TEXT("activate_import_scope")); }
        return Out;
    }
    FPythonCommandEx Command; Command.Command = Request.Code;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement; // Old raw fallback; public supplies ExecuteFile.
    Command.FileExecutionScope = EPythonFileExecutionScope::Public;
    if (!Request.Mode.IsEmpty()) { EPythonCommandExecutionMode Parsed; if (LexTryParseString(Parsed, *Request.Mode)) { Command.ExecutionMode = Parsed; } }
    // Python module and exact execution owner are reacquired immediately before SDK entry.
    if (!Continue() || IPythonScriptPlugin::Get() != Python || !Python->IsPythonInitialized())
    {
        if (Legacy) { Scope.Restore(); Scope.Publish(Out); }
        const bool Closed = CloseTransaction();
        Out->SetStringField(TEXT("error_code"), TEXT("python_execution_admission_changed"));
        Out->SetStringField(TEXT("error"), TEXT("Execution owner/Python changed before dispatch"));
        if (!Closed || (Legacy && !Scope.RestoreVerified)) { Unknown(Out, TEXT("python_execution_admission_changed"), TEXT("Pre-dispatch cleanup outcome unknown"), TEXT("before_python")); }
        return Out;
    }
    Out->SetBoolField(TEXT("mutation_attempted"), true); Out->SetBoolField(TEXT("change_state_known"), false);
    Out->SetStringField(TEXT("execution_mode_actual"), LexToString(Command.ExecutionMode));
    const bool Executed = Python->ExecPythonCommandEx(Command);
    // Only copied native data until the owner/Editor check. No script-created UObject result is retained.
    Out->SetBoolField(TEXT("python_executed"), true); Out->SetBoolField(TEXT("python_command_returned"), true);
    Out->SetBoolField(TEXT("python_execution_success"), Executed); Out->SetStringField(TEXT("command_result"), Command.CommandResult);
    TArray<TSharedPtr<FJsonValue>> Logs;
    for (const FPythonLogOutputEntry& Entry : Command.LogOutput)
    { auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("type"), LexToString(Entry.Type)); Row->SetStringField(TEXT("output"), Entry.Output); Logs.Add(MakeShared<FJsonValueObject>(Row)); }
    Out->SetArrayField(TEXT("logs"), Logs);
    const bool ReturnedCurrent = Continue();
    bool Restored = !Legacy || Scope.Restore();
    if (Legacy) { Scope.Publish(Out); }
    else { Out->SetBoolField(TEXT("interchange_restore_not_required"), true); }
    Out->SetBoolField(TEXT("interchange_bypassed"), Legacy); Out->SetBoolField(TEXT("interchange_restore_required"), Legacy);
    const bool Closed = CloseTransaction();
    // Mandatory console restoration/own transaction closing are cleanup, not continuation writers.
    if (!ReturnedCurrent || !Closed || !TransactionCurrent())
    { Unknown(Out, TEXT("python_execution_current_unknown"), TEXT("Python returned after Editor/retained owner/transaction role changed; logs/result retained, no later UObject observation"), TEXT("after_python")); return Out; }
    if (Legacy) { Restored = Scope.VerifyAfterTransaction(); Scope.Publish(Out); }
    const bool ImportingAtReturn = UInterchangeManager::IsImporting();
    Out->SetBoolField(TEXT("interchange_import_active_at_return"), ImportingAtReturn);
    if (!Current()) { Unknown(Out, TEXT("python_execution_current_unknown"), TEXT("Owner changed after import-state observation"), TEXT("after_python")); return Out; }
    Out->RemoveField(TEXT("error_code")); Out->RemoveField(TEXT("error"));
    Out->SetBoolField(TEXT("success"), Executed && Restored && !ImportingAtReturn);
    Out->SetBoolField(TEXT("change_state_known"), true); Out->SetBoolField(TEXT("mutation_state_unknown"), false);
    // Dispatch is not proof of a concrete mutation or global purity, even on success.
    Out->SetField(TEXT("mutation_performed"), MakeShared<FJsonValueNull>());
    if (!Restored)
    { Unknown(Out, TEXT("legacy_import_guard_restore_failed"), TEXT("Python finished but CVar value/priority restoration was not verified"), TEXT("restore_interchange_state")); Out->SetBoolField(TEXT("partial_mutation"), true); }
    else if (ImportingAtReturn)
    { Unknown(Out, TEXT("interchange_async_outcome_unknown"), TEXT("Python returned while Interchange import is active"), TEXT("await_interchange_completion")); }
    else if (!Executed && Command.CommandResult.IsEmpty()) { Out->SetStringField(TEXT("error"), TEXT("Python execution failed")); }
    if (ImportingAtReturn) { Out->SetBoolField(TEXT("command_may_still_be_in_flight"), true); }
    if (!Executed) { Out->SetBoolField(TEXT("do_not_retry"), true); Out->SetBoolField(TEXT("partial_mutation_possible"), true); }
    return Out;
}
}
