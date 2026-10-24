// DM: original Import13 and Repair6 on fixed named official routes.
#include "Registration/CubelessBlueprintGraphTransferRawToolset.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogCubelessBlueprintGraphTransferRaw, Log, All);

namespace Cubeless::Registration
{
struct FBlueprintGraphTransferRawCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
};
namespace
{
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* ToolsetName = TEXT("CubelessMCPUE58Editor.CubelessBlueprintGraphTransferTools");
constexpr const TCHAR* ImportMethod = TEXT("import_blueprint_graph_nodes_from_text");
constexpr const TCHAR* RepairMethod = TEXT("repair_blueprint_pin_links");
bool AllowedMethod(const FString& Name) { return Name == ImportMethod || Name == RepairMethod; }
constexpr const TCHAR* ResultPrefix = TEXT("__CUBELESS_BLUEPRINT_GRAPH_TRANSFER_RESULT__");
constexpr int32 MaxInputBytes = 131072, MaxResultBytes = 16 * 1024 * 1024;

FString Schema()
{
    return FString(TEXT("{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintGraphTransferTools\",\"version\":\"1.0\",\"tools\":[{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintGraphTransferTools.import_blueprint_graph_nodes_from_text\",\"description\":\"When no currently loaded Timeline template matches the clipboard selector GUID, an exact native Timeline with ten base pins additionally follows installed PostPasteNode/AddNewTimeline to create an empty default template. The existing resolved native TimelineTemplate CDO, actual class outer/package/flags/default values and loaded GUID absence are held through boundaries; no CDO creation or source asset/module loading is added in preflight. Existing source-present duplication, target/curve/owner/skeleton/GUID/compile/save/partial/unknown guards remain. Loaded absence does not prove no disk source exists. Stale authored track pins, modified CDO, multiple Timelines, wider owners/callbacks and full original support remain incomplete or usage pending. Flat no-enum Select and Knot closed data clusters additionally use the common import writer: Select primitive values and bool/int index, bounded options and typed anchor propagation. Pre-SDK exact Knot pin count, Select authored roles and closed reciprocal links are checked; after SDK reconstruction logical pin names, full authored types/defaults and actual links are reacquired. Physical pin addresses/order and reconstruction caches are not frozen. Clear, GUID remap, compile/save, partial/unknown and current object protections remain. Enum/container/reference/mixed-other-class and linked Knot-only clusters remain incomplete or usage pending. Exact flat native SwitchInteger, SwitchName and SwitchString additionally reuse the original text import flow with held native comparator function/CDO identity, typed bounded fiel")) + TEXT("ds/cases, compiler hidden pin and returned-node checks. Known normal pin reconstruction is observed without freezing physical order. Original clear/paste/GUID/structural/compile/forced-save and partial/unknown state remain. Custom/enum/wider Switch and actual paste/connection/save usage remain incomplete or usage pending. Flat text additionally admits exact native UK2Node_IfThenElse and UK2Node_ExecutionSequence through the existing held class/source checks, CanImport, ImportNodesFromText, GUID, structural, compile and save path. No clipboard/UI requirement. Authored pin links/defaults use the same import contract; Select/Knot and broader Switch/class variants remain incomplete. A direct native UObject/Actor MacroLibrary additionally admits its existing own stock K2 MacroGraphs through the common import/repair implementation. Import requires an explicit existing macro selector and never synthesizes an EventGraph; specialized Timeline/Collapsed Actor routes are unchanged. Actual macro paste/repair/compile/save is implemented / usage validation pending. Direct native BlueprintFunctionLibrary import additionally admits an explicitly selected existing own function graph; implicit or whitespace-only selection is refused before graph creation. Library pin-link repair reuses existing owner/type/partial/compiler/save protections. Actual BFL paste and repair usage validation is pending. Original Import13 preserves inline text and text_file_path with exact sample/validate/dry/clear/root/compileTrue/saveFalse defaults. Native owns import safety/ordered clear/paste callbacks/current source and actual fullcompiler/direct forced-save prefixes; legacy GeneratedClass compiled is separate from current errors/status. Nonatomic partial state survives failure; no Python file read, second c") + TEXT("ompiler/save, replay or full runtime/cold pipeline claim. Nonempty inline text takes precedence over a supplied file. File-only source is copied once for existing generic, Timeline and Collapsed import profiles; cached file errors remain separate from authoritative generic target/graph errors. Physical file reading occurs before profile selection; generic result error precedence and specialized parse-before-target protection remain. There is no second file read, target load, transaction or paste. Actual file paste/save/reload usage remains pending and larger files/profiles remain incomplete. An additive finite loaded normal Actor K2 profile imports inline or once-read file text for one collapsed Composite, one owned BoundGraph, two complementary stock Tunnels and optional unlinked K2Node_Self. The bounded body can contain multiple exact plain K2Node_CallFunction nodes for loaded native KismetMathLibrary static pure callable functions with by-value int32/bool parameters and outputs, plus exact stock Branch and ordered Sequence nodes. Native observes each actual function signature, parameter storage, class/CDO and default metadata; it checks canonical scalar defaults and the actual closed reciprocal exec/data topology, including scalar output fanout, without a fixed function name, call count or entry-to-exit path. Sequence output names follow the SDK reconstruction order. Clipboard ports remain externally unlinked and use exec/int32/bool; no runtime arithmetic or branch execution is inferred. The existing clear_graph=False collapsed profile remains available. A further bounded inline collapsed import admits clear_graph=True with keep_root_node=True on a Native-admitted existing normal Actor K2 graph. Native first calls CanImportNodesFromText, then applies the original ord") + TEXT("ered per-node Modify/RemoveNode clear, imports the text, assigns new GUIDs, notifies the graph and marks it structurally modified before the requested compiler/save stages. The original keep-root rule protects FunctionEntry/FunctionResult roles; Event/CustomEvent nodes are removed. Existing removable nodes are bounded to exact stock Event/CustomEvent/Branch/Sequence/plain Call/VariableGet/VariableSet/Self, with affected peers in that set or the retained Entry/Result roles. For the collapsed-text profile, existing nested graphs, Timeline/Macro removal and keep_root_node=False remain incomplete. Actual retained and removed roles are Native current observations. Removal and paste are a compound operation with possible partial state, not atomic replacement or rollback. Returned clear/import/current/compiler/save prefixes survive failure; Python performs no cleanup, second compiler/save or automatic retry. This does not evaluate the runtime sum or establish broader existing-nested-graph, root/type or cold-persistence parity. Target admission captures one native Scene SCS root and direct K2 graphs; arbitrary SCS profiles or pre-existing nested graphs remain incomplete. Native owns actual root/subgraph/outer/node/pin membership and post-paste/structural/compile reacquisition; Python does not flatten the text or invent nested counts. Previous simple import/clear and Repair6 remain unchanged. Composite file input, other native owners or nonpure/impure/ref/container/other scalar signatures, metadata-driven pin expansion, other member/node classes, deeper or mixed top-level trees, broader default coercion, custom callbacks and external references remain valid incomplete scope; imported/compiled/saved prefixes are not runtime or cold-persistence proof. A separate inline native Time") + TEXT("line profile supports an existing direct Actor EventGraph with bounded prior Timelines, clear_graph=False append or clear_graph=True replacement. The original per-node Modify then Graph.RemoveNode removes admitted old graph nodes, including Timeline nodes, and their links; it does not invoke DestroyNode/RemoveTimeline or delete prior authored Timeline templates/curves. Prior ordered Blueprint templates, GUID/name/cache/track/curve values and logical ownership are preserved across implicit skeleton or requested full compiler reacquisition; compiler-derived class template arrays are checked separately. Removed nodes are retired before the SDK call and never read afterward. This EventGraph profile has no FunctionEntry/Result or AnimGraphRoot; the original keep-root rule is unchanged and ordinary Event/CustomEvent nodes are removed by clear. Other old selected nodes/affected peers use the same QZ finite stock callback set plus exact native Timeline. Target requires one native Scene SCS root, no own variable/interface/component-template declarations or pre-existing nested graphs, at most63 prior templates and one appended template. Copied removal/import/known-partial/compiler/save prefixes are preserved; requested save still follows a known failed compile, while unknown stops further SDK calls. No atomic rollback or destruction of old templates is inferred. Timeline nested clipboard, wider missing-source fallback and wider owner/callback/root variants remain valid incomplete scope; source/template selector GUID differs from the newly generated template GUID. Explicit compile/save defaults and all input/output fields remain unchanged. An additive inline mixed clipboard profile admits exactly one native Timeline with stock Branch and ordered Sequence peers, using only closed r") + TEXT("eciprocal links among admitted nodes. Existing source-template/curve ownership, implicit skeleton and logical pin reacquisition, original new-GUID and partial compiler/save receipt paths are reused for all imported nodes. Dangling/external links, additional Timelines, nested mixed objects, wider node/types and general missing-source fallback remain incomplete. Implementation and registration are not actual mixed-paste, saved/cold-reload or runtime behavior proof.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"text\":{\"type\":\"string\",\"default\":\"\"},\"text_file_path\":{\"type\":\"string\",\"default\":\"\"},\"graph_name\":{\"type\":\"string\",\"default\":\"\"},\"graph_id\":{\"type\":\"string\",\"default\":\"\"},\"graph_type\":{\"type\":\"string\",\"default\":\"\"},\"clear_graph\":{\"type\":\"boolean\",\"default\":false},\"keep_root_node\":{\"type\":\"boolean\",\"default\":true},\"allow_non_sample\":{\"type\":\"boolean\",\"default\":false},\"validate_only\":{\"type\":\"boolean\",\"default\":false},\"dry_run\":{\"type\":\"boolean\",\"default\":false},\"compile\":{\"type\":\"boolean\",\"default\":true},\"save_asset\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"return") + TEXT("Value\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessBlueprintGraphTransferTools.repair_blueprint_pin_links\",\"description\":\"A direct native UObject/Actor MacroLibrary additionally admits its existing own stock K2 MacroGraphs through the common import/repair implementation. Import requires an explicit existing macro selector and never synthesizes an EventGraph; specialized Timeline/Collapsed Actor routes are unchanged. Actual macro paste/repair/compile/save is implemented / usage validation pending. Original Repair6 preserves sample/validate/dry/compileTrue/saveFalse defaults. Native tolerantly reads actual malformed raw link arrays and owns ordered cleanup/reciprocal additions and callbacks, then requested compiler/save even matching apply. Validate second pass reads original arrays; no fabricated clean clone, atomic restore or schema converter substitution. Full actual raw/count/partial/compiler/save prefix survives errors; no replay or full runtime/cold pipeline claim. Additionally admits bounded already-loaded Blueprint Interface signature Entry/Result graphs through the existing Repair validate/dry-run/apply/compiler/save path. Authored signature types/defaults/ownership and links are protected; normal compiler reconstruction does not promise stable physical pin addresses or order. Graph import is not expanded. Actual BPI repair variants and persistence remain implemented / usage validation pending.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"blueprint_name\":{\"type\":\"string\"},\"allow_non_sample\":{\"type\":\"boolean\",\"default\":false},\"validate_only\":{\"type\":\"boolean\",\"default\":false},\"dry_run\":{\"type\":\"boolean\",\"default\":false},\"compile\":{\"type\":\"boolean\",\"default\":true},\"save_asset\":{\"type\":\"boolean\",\"default\":false}},\"required\":[\"blueprint_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_") + TEXT("json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}}],\"description\":\"Guarded Blueprint text import and pin-link repair, with flat Branch/Sequence and bounded stock SwitchInteger/Name/String comparator validation.\"}");
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

FOutput Run(const FString& Name, const FString& Input, const TSharedRef<FBlueprintGraphTransferRawCallState, ESPMode::ThreadSafe>& State)
{
    if (!AllowedMethod(Name) || !IsInGameThread() || IsEngineExitRequested() || !State->Active.Load())
    { return MakeError(FString(TEXT("graph_transfer_raw_not_dispatched: game-thread/live owned handler required."))); }
    auto Subsystem = State->Subsystem.Get();
    if (!Subsystem || Subsystem->ToolsetRegistry.Find(ToolsetName).Get() != State->Identity)
    { return MakeError(FString(TEXT("graph_transfer_raw_not_dispatched: exact retained Registry handler is unavailable or filtered."))); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    { return MakeError(FString(TEXT("graph_transfer_raw_not_dispatched: existing initialized Python required; no load/force-enable/fallback."))); }
    const FTCHARToUTF8 UTF8(*Input, Input.Len());
    if (UTF8.Length() > MaxInputBytes)
    { return MakeError(FString(TEXT("graph_transfer_raw_not_dispatched: raw request exceeds128KiB UTF8 limit."))); }
    const FUTF8ToTCHAR InputRoundTrip(reinterpret_cast<const UTF8CHAR*>(UTF8.Get()), UTF8.Length());
    if (FString(InputRoundTrip.Length(), InputRoundTrip.Get()) != Input)
    { return MakeError(FString(TEXT("graph_transfer_raw_not_dispatched: input is not lossless valid Unicode."))); }
    const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
    const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    FPythonCommandEx Command; Command.Flags = EPythonCommandFlags::Unattended;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_graph_transfer import emit_result as _cubeless_graph_transfer_emit; _cubeless_graph_transfer_emit('%s','%s','%s')"), *Name, *Encoded, *Nonce);
    // Interpolated method is one of two exact literals; remaining values are local Base64/GUID.
    const bool Executed = Python->ExecPythonCommandEx(Command);
    const FString Prefix = FString(ResultPrefix) + Nonce + TEXT(":"); FString Payload; int32 Markers = 0;
    for (const auto& Entry : Command.LogOutput)
    {
        TArray<FString> Lines; Entry.Output.ParseIntoArrayLines(Lines, false);
        for (const auto& Line : Lines) { if (Line.StartsWith(Prefix, ESearchCase::CaseSensitive)) { ++Markers; Payload = Line.Mid(Prefix.Len()); } }
    }
    TArray<uint8> Bytes;
    if (Markers != 1 || Payload.Len() > ((MaxResultBytes + 2) / 3) * 4 || !FBase64::Decode(Payload, Bytes)
        || Bytes.Num() == 0 || Bytes.Num() > MaxResultBytes || FBase64::Encode(Bytes) != Payload)
    { return MakeError(FailureReceipt(Command, Executed, TEXT("graph_transfer_raw_return_unverified: missing/multiple/invalid/oversized exact nonce marker."))); }
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
    const FString Result(Decoded.Length(), Decoded.Get()); const FTCHARToUTF8 RoundTrip(*Result, Result.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0 || !TypedResult(Result))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("graph_transfer_raw_return_unverified: emitted bytes are not exact UTF8/standard typed returnValue JSON."))); }
    // A valid emitted receipt remains authoritative even if later Python logging
    // reported an exception. Never synthesize known0 after Python dispatch.
    if (!Executed) { UE_LOG(LogCubelessBlueprintGraphTransferRaw, Warning, TEXT("Graph transfer Python statement reported failure after a valid operation receipt; returned receipt preserved.")); }
    return MakeValue(Result);
}

class FBlueprintGraphTransferRawToolset final : public UE::ToolsetRegistry::FToolset
{
public:
    explicit FBlueprintGraphTransferRawToolset(const TSharedRef<FBlueprintGraphTransferRawCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("Original Import13 and Repair6 through public Engine graph attachment APIs. Preserves text/file, sample, validate/dry, clear/root and requested compiler/forced-save contracts with actual partial receipts. No atomic rollback, automatic replay or complete runtime/pipeline proof."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        if (!AllowedMethod(Name)) { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("graph_transfer_raw_not_dispatched: exact named method required.")))).GetFuture(); }
        const FTCHARToUTF8 InputBytes(*Input, Input.Len());
        if (InputBytes.Length() > MaxInputBytes)
        { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("graph_transfer_raw_not_dispatched: raw request exceeds128KiB UTF8 limit.")))).GetFuture(); }
        // Capture only strings and shared lifetime state; no UObject is accessed off the game thread.
        if (IsInGameThread()) { return MakeFulfilledPromise<FOutput>(Run(Name, Input, State)).GetFuture(); }
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        const auto LiveState = State;
        AsyncTask(ENamedThreads::GameThread, [Promise, LiveState, Name, Input]() { Promise->SetValue(Run(Name, Input, LiveState)); });
        return Future;
    }
private:
    TSharedRef<FBlueprintGraphTransferRawCallState, ESPMode::ThreadSafe> State;
};
}

bool FBlueprintGraphTransferRawRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto Subsystem = RegistrySubsystem.Get(); return Subsystem && Subsystem->ToolsetRegistry.Find(ToolsetName) == Handler; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless original blueprint graph transfer/repair raw registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; } // Never replace any existing handler.
    auto State = MakeShared<FBlueprintGraphTransferRawCallState, ESPMode::ThreadSafe>(); State->Subsystem = Subsystem.GetValue().Get();
    TSharedPtr<UE::ToolsetRegistry::FToolset> Candidate = MakeShared<FBlueprintGraphTransferRawToolset>(State);
    State->Identity = Candidate.Get();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get(); Handler = Candidate; CallState = State; State->Active.Store(true);
    return true;
}

void FBlueprintGraphTransferRawRegistration::Unregister()
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
