// Seven fixed PCG methods on the existing raw FToolset seam. No generic evaluator.
#include "Registration/CubelessPCGRawToolset.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"
#include "Misc/CoreMisc.h"
#include "PythonScriptTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogCubelessPCGRaw, Log, All);

namespace Cubeless::Registration
{
struct FPCGRawCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
};
namespace
{
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* ToolsetName = TEXT("cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools");
bool KnownMethod(const FString& Name)
{
    return Name.Equals(TEXT("create_pcg_graph_from_spec"), ESearchCase::CaseSensitive)
        || Name.Equals(TEXT("set_pcg_attribute_selector"), ESearchCase::CaseSensitive)
        || Name.Equals(TEXT("compile_or_notify_pcg_graph"), ESearchCase::CaseSensitive)
        || Name.Equals(TEXT("save_pcg_graph"), ESearchCase::CaseSensitive)
        || Name.Equals(TEXT("validate_pcg_source_independence"), ESearchCase::CaseSensitive)
        || Name.Equals(TEXT("promote_pcg_temp_graph"), ESearchCase::CaseSensitive)
        || Name.Equals(TEXT("pcg_actor_smoke_test"), ESearchCase::CaseSensitive);
}
constexpr const TCHAR* ResultPrefix = TEXT("__CUBELESS_PCG_RESULT__");
constexpr int32 MaxInputBytes = 2 * 1024 * 1024, MaxResultBytes = 16 * 1024 * 1024;

FString Schema()
{
    // Raw JSON schema: original object/array inputs, no reflected Optional
    // frame conversion, type-schema Duplicate callback or new UClass.
    return FString(TEXT("{\"name\":\"cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools\",\"version\":\"1.0\",\"description\":\"Seven original PCG input contracts on fixed raw Registry handlers; registration does not establish construction health.\",\"tools\":[{\"name\":\"cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools.create_pcg_graph_from_spec\",\"description\":\"Preserve original graph_spec object, normalization and durable journal. Existing finite9 plus exact PointExtentsModifier/BoundsModifier seven-field profiles and exact stock StaticMeshSpawner use one scoped C++ JSON composite for scratch preflight, fresh asset factory, settings/direct edges and optional single save with recursive dependency audit. Spawner retains its bounded nine general fields and admitted inline mesh_entries/static_mesh_entries/spawner_mesh_entries/mesh_selector carrier: ByAttribute first when requested, then Weighted from the first truthy entries alias, with empty entries alone a no-op. Only bounded inline defaults/coercion are admitted, with actual ordered selector readback and scratch/persistent/partial/save prefixes distinct from standalone setter policy. Known-current input errors are retained while Weighted continues, then the earliest error fails scratch before persistent creation; unknown SDK/source/identity outcomes stop immediately. Material-attribute names accept arrays, Unicode string characters and dictionary keys in received order; explicit null is an iterable error. General fields retain JSON Boolean, bounded Seed numeric truncation and FName admission; full coercion and numeric/container-to-FName string-representation parity remain incomplete. Mesh/material references must already be loaded and admitted; source loaders, arbitrary fields/custom selectors and full parity remain incomplete. General construction retains the historical GC gate; this route does not resolve GC root cause. Existing targets are never overwritten. Runtime generation and host binding remain unverified.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"graph_spec\":{\"type\":\"object\"},\"overwrite_existing\":{\"type\":\"boolean\",\"default\":false},\"allow_overwrite_non_temp\":{\"type\":\"boolean\",\"default\":false},\"save\":{\"type\":\"boolean\",\"default\":true},\"dry_run\":{\"type\":\"boolean\",\"default\":false},\"operation_id\":{\"type\":[\"string\",\"null\"],\"default\":null}},\"required\":[\"graph_spec\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools.set_pcg_attribute_selector\",\"description\":\"Restore original extra_names array/null and empty string defaults; use existing selector preview/commit/journal semantics. Loaded graph snapshots use the native JSON inspection path; construction and transient construction preflight remain blocked. Exact public accessors, partial writes, save receipts and unknown outcomes remain authoritative; execution health and the GC cause remain unverified.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"graph_path\":{\"type\":\"string\"},\"node_id\":{\"type\":\"string\"},\"selector_property_name\":{\"type\":\"string\"},\"selector_type\":{\"type\":\"string\",\"default\":\"attribute\"},\"attribute_name\":{\"type\":\"string\",\"default\":\"\"},\"selected_property_name\":{\"type\":\"string\",\"default\":\"\"},\"domain_name\":{\"type\":\"string\",\"default\":\"\"},\"point_property\":{\"type\":\"string\",\"default\":\"\"},\"extra_property\":{\"type\":\"string\",\"default\":\"\"},\"extra_names\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"string\"},\"default\":null},\"reset_extra_names\":{\"type\":\"boolean\",\"default\":true},\"save\":{\"type\":\"boolean\",\"default\":true},\"dry_run\":{\"type\":\"boolean\",\"default\":false},\"operation_id\":{\"type\":[\"string\",\"null\"],\"default\":null}},\"required\":[\"graph_path\",\"node_id\",\"selector_property_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools.compile_or_notify_pcg_graph\",\"description\":\"Existing structural notification plus exact graph-cache recompile result, not shader/Blueprint compilation or generation. No explicit save. Loaded snapshots use native JSON inspection. General construction remains blocked; finite fresh graph-spec construction is separate, and execution health and the GC cause remain unverified.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"graph_path\":{\"type\":\"string\"},\"operation_id\":{\"type\":[\"string\",\"null\"],\"default\":null}},\"required\":[\"graph_path\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":"))
        + TEXT("\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools.save_pcg_graph\",\"description\":\"Existing exact loaded graph force-save and disk/topology/dependency checks. No extra save/compiler/retry or GC repair claim. Loaded snapshots use native JSON inspection. General construction remains blocked; finite fresh graph-spec construction is separate, and execution health and the GC cause remain unverified.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"graph_path\":{\"type\":\"string\"},\"operation_id\":{\"type\":[\"string\",\"null\"],\"default\":null}},\"required\":[\"graph_path\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools.validate_pcg_source_independence\",\"description\":\"Validate original source independence through the existing PCG contract audit: direct on-disk hard and soft Package dependencies only, with forbidden hits as policy authority. Dependencies outside allowed prefixes are diagnostics, not a failure gate. Default prefix lists are selected only for null; explicit empty lists remain empty. Existing audit/source/SDK failures and actual receipts are retained. This is not transitive, in-memory, runtime or cook independence proof.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"graph_path\":{\"type\":\"string\"},\"forbidden_dependency_prefixes\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"string\"},\"default\":null},\"allowed_dependency_prefixes\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"string\"},\"default\":null}},\"required\":[\"graph_path\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools.promote_pcg_temp_graph\",\"description\":\"Promote a validated temporary PCG graph with the original seven inputs and dry_run=True by default. Validate first; allow_non_temp_source is explicit. Target must be a new permanent /Game package outside /Game/_MCP_Temp. overwrite_existing is wire-compatible only and never permits replacement. Dry-run reports an existing target without writing. Actual duplication/save use public Editor APIs with retained source/returned-object/current and target-absence checks. A candidate created in memory followed by failed or uncertain save retains created/mutation/save prefixes and an unknown no-retry outcome; it is not rolled back or retried. No map mutation, explicit graph compilation, runtime generation, arbitrary source-field preservation or global GC repair claim.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"source_graph_path\":{\"type\":\"string\"},\"target_graph_path\":{\"type\":\"string\"},\"forbidden_dependency_prefixes\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"string\"},\"default\":null},\"allowed_dependency_prefixes\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"string\"},\"default\":null},\"overwrite_existing\":{\"type\":\"boolean\",\"default\":false},\"allow_non_temp_source\":{\"type\":\"boolean\",\"default\":false},\"dry_run\":{\"type\":\"boolean\",\"default\":true}},\"required\":[\"source_graph_path\",\"target_graph_path\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools.pcg_actor_smoke_test\",\"description\":\"Audit the original graph contract before matching existing Editor actors/components by graph package, actor label/path substring and selected_only. dry_run=True lists matches without cleanup/generate. dry_run=False invokes exact public local noarg PCG Cleanup then Generate through an internal Editor compatibility wrapper; no force or immediate cleanup substitution. Returned dispatch is not task acceptance or completion. Existing actor/component/graph anchors are rechecked between callbacks; unknown stops without retry. No map/actor creation, explicit compile/save or runtime completion proof.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"graph_path\":{\"type\":\"string\"},\"actor_label\":{\"type\":\"string\",\"default\":\"\"},\"selected_only\":{\"type\":\"boolean\",\"default\":false},\"cleanup\":{\"type\":\"boolean\",\"default\":true},\"generate\":{\"type\":\"boolean\",\"default\":true},\"dry_run\":{\"type\":\"boolean\",\"default\":true}},\"required\":[\"graph_path\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}}]}");
}

bool TypedResult(const FString& Text)
{
    TSharedPtr<FJsonObject> O;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid() || O->Values.Num() != 1) { return false; }
    const TSharedPtr<FJsonObject>* Value = nullptr;
    if (!O->TryGetObjectField(TEXT("returnValue"), Value) || !Value || !Value->IsValid() || (*Value)->Values.Num() != 11) { return false; }
    const auto Success = (*Value)->TryGetField(TEXT("success"));
    if (!Success.IsValid() || Success->Type != EJson::Boolean) { return false; }
    for (const TCHAR* Key : { TEXT("operation_id"), TEXT("stage"), TEXT("error_code"), TEXT("error"), TEXT("changes_json"),
        TEXT("compile_json"), TEXT("save_json"), TEXT("validation_json"), TEXT("retry_json"), TEXT("data_json") })
    { const auto Field = (*Value)->TryGetField(Key); if (!Field.IsValid() || Field->Type != EJson::String) { return false; } }
    return true;
}

FString FailureReceipt(const FPythonCommandEx& Command, bool Executed, const TCHAR* Reason)
{
    auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("error"), Reason);
    O->SetStringField(TEXT("scope"), TEXT("Python statement was dispatched; mutation outcome is unverified unless the exact emitted operation receipt is independently recovered. No retry/rollback/secondary save."));
    O->SetBoolField(TEXT("python_statement_completed"), Executed); O->SetStringField(TEXT("command_result"), Command.CommandResult);
    TArray<TSharedPtr<FJsonValue>> Logs;
    for (const auto& Entry : Command.LogOutput)
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("type"), LexToString(Entry.Type)); Row->SetStringField(TEXT("output"), Entry.Output);
        Logs.Add(MakeShared<FJsonValueObject>(Row));
    }
    O->SetArrayField(TEXT("captured_log_output"), Logs);
    FString Out; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Out)); return Out;
}

FOutput Run(const FString& Name, const FString& Input, const TSharedRef<FPCGRawCallState, ESPMode::ThreadSafe>& State)
{
    if (!KnownMethod(Name) || !IsInGameThread() || IsEngineExitRequested() || !State->Active.Load())
    { return MakeError(FString(TEXT("pcg_raw_not_dispatched: game-thread/live owned handler required."))); }
    auto Subsystem = State->Subsystem.Get();
    if (!Subsystem || Subsystem->ToolsetRegistry.Find(ToolsetName).Get() != State->Identity)
    { return MakeError(FString(TEXT("pcg_raw_not_dispatched: exact retained Registry handler is unavailable or filtered."))); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    { return MakeError(FString(TEXT("pcg_raw_not_dispatched: existing initialized Python required; no load/force-enable/fallback."))); }
    const FTCHARToUTF8 UTF8(*Input, Input.Len());
    if (UTF8.Length() > MaxInputBytes)
    { return MakeError(FString(TEXT("pcg_raw_not_dispatched: raw request exceeds2MiB UTF8 limit."))); }
    const FUTF8ToTCHAR InputRoundTrip(reinterpret_cast<const UTF8CHAR*>(UTF8.Get()), UTF8.Length());
    if (!FString(InputRoundTrip.Length(), InputRoundTrip.Get()).Equals(Input, ESearchCase::CaseSensitive))
    { return MakeError(FString(TEXT("pcg_raw_not_dispatched: input is not lossless valid Unicode."))); }
    const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
    const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    FPythonCommandEx Command; Command.Flags = EPythonCommandFlags::Unattended;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_pcg import emit_result as _cubeless_pcg_emit; _cubeless_pcg_emit('%s','%s','%s')"), *Name, *Encoded, *Nonce);
    // Interpolated values are only an exact fixed whitelist method, local Base64 and lowercase GUID.
    const bool Executed = Python->ExecPythonCommandEx(Command);
    const FString Prefix = FString(ResultPrefix) + Nonce + TEXT(":"); FString Payload; int32 Markers = 0;
    for (const auto& Entry : Command.LogOutput)
    {
        TArray<FString> Lines; Entry.Output.ParseIntoArrayLines(Lines, false);
        for (const auto& Line : Lines) { if (Line.StartsWith(Prefix)) { ++Markers; Payload = Line.Mid(Prefix.Len()); } }
    }
    TArray<uint8> Bytes;
    if (Markers != 1 || Payload.Len() > ((MaxResultBytes + 2) / 3) * 4 || !FBase64::Decode(Payload, Bytes)
        || Bytes.Num() == 0 || Bytes.Num() > MaxResultBytes || FBase64::Encode(Bytes) != Payload)
    { return MakeError(FailureReceipt(Command, Executed, TEXT("pcg_raw_return_unverified: missing/multiple/invalid/oversized exact nonce marker."))); }
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
    const FString Result(Decoded.Length(), Decoded.Get()); const FTCHARToUTF8 RoundTrip(*Result, Result.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0 || !TypedResult(Result))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("pcg_raw_return_unverified: emitted bytes are not exact UTF8/standard typed returnValue JSON."))); }
    // A valid emitted receipt remains authoritative even if later Python logging
    // reported an exception. Never synthesize known0 after Python dispatch.
    if (!Executed) { UE_LOG(LogCubelessPCGRaw, Warning, TEXT("PCG Python statement reported failure after a valid operation receipt; returned receipt preserved.")); }
    return MakeValue(Result);
}

class FPCGRawToolset final : public UE::ToolsetRegistry::FToolset
{
public:
    explicit FPCGRawToolset(const TSharedRef<FPCGRawCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("Seven original PCG input contracts with fixed named domains; construction health remains separate."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        if (!KnownMethod(Name)) { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("pcg_raw_not_dispatched: exact named method required.")))).GetFuture(); }
        const FTCHARToUTF8 InputBytes(*Input, Input.Len());
        if (InputBytes.Length() > MaxInputBytes)
        { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("pcg_raw_not_dispatched: raw request exceeds2MiB UTF8 limit.")))).GetFuture(); }
        // Capture only strings and shared lifetime state; no UObject is accessed off the game thread.
        if (IsInGameThread()) { return MakeFulfilledPromise<FOutput>(Run(Name, Input, State)).GetFuture(); }
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        const auto LiveState = State;
        AsyncTask(ENamedThreads::GameThread, [Promise, LiveState, Name, Input]() { Promise->SetValue(Run(Name, Input, LiveState)); });
        return Future;
    }
private:
    TSharedRef<FPCGRawCallState, ESPMode::ThreadSafe> State;
};
}

bool FPCGRawRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto Subsystem = RegistrySubsystem.Get(); return Subsystem && Subsystem->ToolsetRegistry.Find(ToolsetName) == Handler; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless fixed PCG raw registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; } // Never replace any existing handler.
    auto State = MakeShared<FPCGRawCallState, ESPMode::ThreadSafe>(); State->Subsystem = Subsystem.GetValue().Get();
    TSharedPtr<UE::ToolsetRegistry::FToolset> Candidate = MakeShared<FPCGRawToolset>(State);
    State->Identity = Candidate.Get();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get(); Handler = Candidate; CallState = State; State->Active.Store(true);
    return true;
}

void FPCGRawRegistration::Unregister()
{
    if (CallState.IsValid()) { CallState->Active.Store(false); } // Queued requests must not begin Python after shutdown.
    if (IsInGameThread() && UObjectInitialized())
    {
        if (auto Subsystem = RegistrySubsystem.Get())
        {
            if (Handler.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, Handler))
            { Subsystem->ToolsetRegistry.UnregisterToolset(Handler); }
        }
    }
    Handler.Reset(); CallState.Reset(); RegistrySubsystem.Reset(); // No new subsystem discovery during teardown.
}
}
