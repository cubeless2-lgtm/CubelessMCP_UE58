// PhysicsAsset15 plus fixture13/inspector3 on the same fixed raw seam. No generic evaluator.
#include "Registration/CubelessSkeletalPhysicsRawToolset.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Base64.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/CoreMisc.h"
#include "PythonScriptTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogCubelessSkeletalPhysicsRaw, Log, All);

namespace Cubeless::Registration
{
struct FSkeletalPhysicsRawCallState
{
    TAtomic<bool> Active { false };
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    const UE::ToolsetRegistry::FToolset* Identity = nullptr;
    FString ProjectFile;
    FGuid InstanceId;
};
namespace
{
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* ToolsetName = TEXT("CubelessMCPUE58Editor.CubelessSkeletalPhysicsTools");
bool KnownMethod(const FString& Name)
{
    return Name.Equals(TEXT("create_skeletal_physics_asset"), ESearchCase::CaseSensitive)
        || Name.Equals(TEXT("create_skeletal_bone_constraint_fixture"), ESearchCase::CaseSensitive)
        || Name.Equals(TEXT("inspect_skeletal_bone_constraint_fixture"), ESearchCase::CaseSensitive);
}
constexpr const TCHAR* ResultPrefix = TEXT("__CUBELESS_SKELETAL_PHYSICS_RESULT__");
constexpr const TCHAR* ConstraintResultPrefix = TEXT("__CUBELESS_SKELETAL_CONSTRAINT_RESULT__");
constexpr int32 MaxInputBytes = 131072, MaxResultBytes = 16 * 1024 * 1024;

FString Schema()
{
    // Literal original public15 schema; nullable array/defaultNull stays raw JSON,
    // with no reflected class, Optional frame conversion or TYPE-schema Duplicate.
    return TEXT("{\"name\":\"CubelessMCPUE58Editor.CubelessSkeletalPhysicsTools\",\"tools\":[{\"name\":\"CubelessMCPUE58Editor.CubelessSkeletalPhysicsTools.create_skeletal_physics_asset\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"skeletal_mesh_path\":{\"type\":\"string\"},\"target_physics_asset_path\":{\"type\":\"string\"},\"expected_bones\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"string\"},\"default\":null},\"kinematic_root_bone\":{\"type\":\"string\",\"default\":\"\"},\"root_kinematic\":{\"type\":\"boolean\",\"default\":true},\"disable_collision\":{\"type\":\"boolean\",\"default\":true},\"swing1_limit_degrees\":{\"type\":\"number\",\"default\":45.0},\"swing2_limit_degrees\":{\"type\":\"number\",\"default\":45.0},\"twist_limit_degrees\":{\"type\":\"number\",\"default\":25.0},\"linear_damping\":{\"type\":\"number\",\"default\":0.15},\"angular_damping\":{\"type\":\"number\",\"default\":0.5},\"tip_mass_scale\":{\"type\":\"number\",\"default\":3.0},\"overwrite_existing\":{\"type\":\"boolean\",\"default\":false},\"save\":{\"type\":\"boolean\",\"default\":true},\"dry_run\":{\"type\":\"boolean\",\"default\":true}},\"required\":[\"skeletal_mesh_path\",\"target_physics_asset_path\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessSkeletalPhysicsTools.create_skeletal_bone_constraint_fixture\",\"description\":\"Original native transient skeletal constraint fixture and point inspector contracts. World selectors remain literal; named calls can target original active play contexts while shared Batch retains Editor-idle admission. Actual component/bone/force/angle/simulation and partial cleanup receipts remain authoritative, without asset save/compile, play lifecycle control, stable simulation/cold persistence or full original variant claims.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"skeletal_mesh_path\":{\"type\":\"string\"},\"actor_name\":{\"type\":\"string\",\"default\":\"MCP_SkeletalBoneConstraint_Ghost\"},\"bone_name1\":{\"type\":\"string\",\"default\":\"hand_l\"},\"bone_name2\":{\"type\":\"string\",\"default\":\"hand_r\"},\"world_context\":{\"type\":\"string\",\"default\":\"editor\"},\"location\":{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"number\"},\"default\":null},\"constraint_component_name\":{\"type\":\"string\",\"default\":\"MCP_SkeletalBoneConstraint\"},\"swing1_limit_degrees\":{\"type\":\"number\",\"default\":45.0},\"swing2_limit_degrees\":{\"type\":\"number\",\"default\":45.0},\"twist_limit_degrees\":{\"type\":\"number\",\"default\":30.0},\"disable_collision\":{\"type\":\"boolean\",\"default\":true},\"simulate_all_bodies\":{\"type\":\"boolean\",\"default\":true},\"dry_run\":{\"type\":\"boolean\",\"default\":true}},\"required\":[\"skeletal_mesh_path\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}},{\"name\":\"CubelessMCPUE58Editor.CubelessSkeletalPhysicsTools.inspect_skeletal_bone_constraint_fixture\",\"description\":\"Original native transient skeletal constraint fixture and point inspector contracts. World selectors remain literal; named calls can target original active play contexts while shared Batch retains Editor-idle admission. Actual component/bone/force/angle/simulation and partial cleanup receipts remain authoritative, without asset save/compile, play lifecycle control, stable simulation/cold persistence or full original variant claims. Exact native Actor compositions are also admitted only in the current idle Editor world with exact native selected SkeletalMeshComponent/PhysicsConstraintComponent ownership and guarded getter point reads. Missing tag or same-component/bone-name predicates still fail; other Actor/component classes and new live variants remain incomplete.\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"actor_name\":{\"type\":\"string\"},\"world_context\":{\"type\":\"string\",\"default\":\"auto\"},\"constraint_component_name\":{\"type\":\"string\",\"default\":\"\"}},\"required\":[\"actor_name\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"returnValue\":{\"type\":\"object\",\"properties\":{\"success\":{\"type\":\"boolean\"},\"operation_id\":{\"type\":\"string\"},\"stage\":{\"type\":\"string\"},\"error_code\":{\"type\":\"string\"},\"error\":{\"type\":\"string\"},\"changes_json\":{\"type\":\"string\"},\"compile_json\":{\"type\":\"string\"},\"save_json\":{\"type\":\"string\"},\"validation_json\":{\"type\":\"string\"},\"retry_json\":{\"type\":\"string\"},\"data_json\":{\"type\":\"string\"}},\"required\":[\"success\",\"operation_id\",\"stage\",\"error_code\",\"error\",\"changes_json\",\"compile_json\",\"save_json\",\"validation_json\",\"retry_json\",\"data_json\"],\"additionalProperties\":false}},\"required\":[\"returnValue\"],\"additionalProperties\":false}}]}");
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
    O->SetBoolField(TEXT("python_statement_dispatched"), true);
    O->SetBoolField(TEXT("mutation_state_unknown"), true);
    O->SetBoolField(TEXT("python_statement_completed"), Executed);
    O->SetNumberField(TEXT("raw_python_dispatch_attempts"), 1);
    O->SetNumberField(TEXT("raw_python_dispatch_returns"), 1);
    // These are raw statement counters; native loads/generator/save counts are not inferred.
    const auto Prefix = [](const FString& Value, int32 Budget)
    {
        int32 Count = FMath::Min(Value.Len(), Budget);
        // Windows TCHAR is UTF16; do not split a trailing high-surrogate pair in the retained prefix.
        if (Count > 0 && static_cast<uint32>(Value[Count - 1]) >= 0xd800 && static_cast<uint32>(Value[Count - 1]) <= 0xdbff) { --Count; }
        return Value.Left(Count);
    };
    const FString CommandPrefix = Prefix(Command.Command, 16384), ResultPrefixValue = Prefix(Command.CommandResult, 16384);
    O->SetStringField(TEXT("command_prefix"), CommandPrefix);
    O->SetNumberField(TEXT("command_character_count"), Command.Command.Len());
    O->SetBoolField(TEXT("command_prefix_truncated"), CommandPrefix.Len() != Command.Command.Len());
    O->SetStringField(TEXT("command_result"), ResultPrefixValue);
    O->SetNumberField(TEXT("command_result_character_count"), Command.CommandResult.Len());
    O->SetBoolField(TEXT("command_result_truncated"), ResultPrefixValue.Len() != Command.CommandResult.Len());
    O->SetNumberField(TEXT("captured_log_entry_count"), Command.LogOutput.Num());
    TArray<TSharedPtr<FJsonValue>> Logs;
    int32 Remaining = 65536; bool LogPrefixTruncated = false;
    for (const auto& Entry : Command.LogOutput)
    {
        if (Remaining <= 0 || Logs.Num() >= 64) { LogPrefixTruncated = true; break; }
        const FString OutputPrefix = Prefix(Entry.Output, Remaining);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("type"), LexToString(Entry.Type)); Row->SetStringField(TEXT("output"), OutputPrefix);
        Row->SetNumberField(TEXT("original_character_count"), Entry.Output.Len());
        Row->SetBoolField(TEXT("truncated"), OutputPrefix.Len() != Entry.Output.Len());
        LogPrefixTruncated |= OutputPrefix.Len() != Entry.Output.Len(); Remaining -= OutputPrefix.Len();
        Logs.Add(MakeShared<FJsonValueObject>(Row));
    }
    O->SetArrayField(TEXT("captured_log_output"), Logs);
    O->SetBoolField(TEXT("captured_log_prefix_truncated"), LogPrefixTruncated);
    O->SetStringField(TEXT("diagnostic_scope"), TEXT("Bounded actual command/result/log prefixes only; omitted suffixes and native outcomes remain unverified."));
    FString Out; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Out)); return Out;
}

FOutput Run(const FString& Name, const FString& Input, const TSharedRef<FSkeletalPhysicsRawCallState, ESPMode::ThreadSafe>& State)
{
    const bool PhysicsAssetMethod = Name.Equals(TEXT("create_skeletal_physics_asset"), ESearchCase::CaseSensitive);
    // CN15 keeps its original idle admission. CO named methods delegate Editor-vs-play selection to Native.
    if (!KnownMethod(Name) || !Cubeless::UE58::IsEditorGameThread() || (PhysicsAssetMethod && Cubeless::UE58::IsPlaySessionActive())
        || IsEngineExitRequested() || !State->Active.Load())
    { return MakeError(FString(TEXT("skeletal_physics_raw_not_dispatched: Editor game-thread/live owned handler required; PhysicsAsset generation requires idle Editor."))); }
    if (!State->InstanceId.IsValid() || FApp::GetInstanceId() != State->InstanceId || State->ProjectFile.IsEmpty()
        || !FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()).Equals(State->ProjectFile, ESearchCase::CaseSensitive))
    { return MakeError(FString(TEXT("skeletal_physics_raw_not_dispatched: exact retained Editor instance/project required."))); }
    auto Subsystem = State->Subsystem.Get();
    if (!Subsystem || Subsystem->ToolsetRegistry.Find(ToolsetName).Get() != State->Identity)
    { return MakeError(FString(TEXT("skeletal_physics_raw_not_dispatched: exact retained Registry handler is unavailable or filtered."))); }
    IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
    if (!Python || !Python->IsPythonAvailable() || !Python->IsPythonInitialized())
    { return MakeError(FString(TEXT("skeletal_physics_raw_not_dispatched: existing initialized Python required; no load/force-enable/fallback."))); }
    const FTCHARToUTF8 UTF8(*Input, Input.Len());
    if (UTF8.Length() > MaxInputBytes)
    { return MakeError(FString(TEXT("skeletal_physics_raw_not_dispatched: raw request exceeds128KiB UTF8 limit."))); }
    const FUTF8ToTCHAR InputRoundTrip(reinterpret_cast<const UTF8CHAR*>(UTF8.Get()), UTF8.Length());
    if (!FString(InputRoundTrip.Length(), InputRoundTrip.Get()).Equals(Input, ESearchCase::CaseSensitive))
    { return MakeError(FString(TEXT("skeletal_physics_raw_not_dispatched: input is not lossless valid Unicode."))); }
    const FString Encoded = FBase64::Encode(reinterpret_cast<const uint8*>(UTF8.Get()), UTF8.Length());
    const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    FPythonCommandEx Command; Command.Flags = EPythonCommandFlags::Unattended;
    Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
    if (PhysicsAssetMethod)
    {
    Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_skeletal_physics import emit_result as _cubeless_skeletal_physics_emit; _cubeless_skeletal_physics_emit('%s','%s')"), *Encoded, *Nonce);
    }
    else
    {
        Command.Command = FString::Printf(TEXT("from cubeless_mcp_ue58.registration.raw_skeletal_constraint import emit_result as _cubeless_skeletal_constraint_emit; _cubeless_skeletal_constraint_emit('%s','%s','%s')"), *Name, *Encoded, *Nonce);
    }
    // Only exact whitelisted method (CO2 branch), local Base64 and lowercase GUID are interpolated; both modules/helpers are fixed.
    const bool Executed = Python->ExecPythonCommandEx(Command);
    const FString Prefix = FString(PhysicsAssetMethod ? ResultPrefix : ConstraintResultPrefix) + Nonce + TEXT(":"); FString Payload; int32 Markers = 0;
    for (const auto& Entry : Command.LogOutput)
    {
        TArray<FString> Lines; Entry.Output.ParseIntoArrayLines(Lines, false);
        for (const auto& Line : Lines) { if (Line.StartsWith(Prefix, ESearchCase::CaseSensitive)) { ++Markers; Payload = Line.Mid(Prefix.Len()); } }
    }
    TArray<uint8> Bytes;
    if (Markers != 1 || Payload.Len() > ((MaxResultBytes + 2) / 3) * 4 || !FBase64::Decode(Payload, Bytes)
        || Bytes.Num() == 0 || Bytes.Num() > MaxResultBytes || !FBase64::Encode(Bytes).Equals(Payload, ESearchCase::CaseSensitive))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("skeletal_physics_raw_return_unverified: missing/multiple/invalid/oversized exact nonce marker."))); }
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const UTF8CHAR*>(Bytes.GetData()), Bytes.Num());
    const FString Result(Decoded.Length(), Decoded.Get()); const FTCHARToUTF8 RoundTrip(*Result, Result.Len());
    if (RoundTrip.Length() != Bytes.Num() || FMemory::Memcmp(RoundTrip.Get(), Bytes.GetData(), Bytes.Num()) != 0 || !TypedResult(Result))
    { return MakeError(FailureReceipt(Command, Executed, TEXT("skeletal_physics_raw_return_unverified: emitted bytes are not exact UTF8/standard typed returnValue JSON."))); }
    // A valid emitted receipt remains authoritative even if later Python logging
    // reported an exception. Never synthesize known0 after Python dispatch.
    if (!Executed) { UE_LOG(LogCubelessSkeletalPhysicsRaw, Warning, TEXT("PhysicsAsset Python statement reported failure after a valid operation receipt; returned receipt preserved.")); }
    return MakeValue(Result);
}

class FSkeletalPhysicsRawToolset final : public UE::ToolsetRegistry::FToolset
{
public:
    explicit FSkeletalPhysicsRawToolset(const TSharedRef<FSkeletalPhysicsRawCallState, ESPMode::ThreadSafe>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return ToolsetName; }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("PhysicsAsset15 plus original transient fixture13/live inspector3, fixed named domains and current Editor-host world selection; no source mesh assignment, automatic PIE lifecycle or full runtime/overwrite parity claim."); }
protected:
    virtual FString GetJsonSchemaInternal() const override { return Schema(); }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Name, const FString& Input) override
    {
        if (!KnownMethod(Name)) { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("skeletal_physics_raw_not_dispatched: exact named method required.")))).GetFuture(); }
        const FTCHARToUTF8 InputBytes(*Input, Input.Len());
        if (InputBytes.Length() > MaxInputBytes)
        { return MakeFulfilledPromise<FOutput>(MakeError(FString(TEXT("skeletal_physics_raw_not_dispatched: raw request exceeds128KiB UTF8 limit.")))).GetFuture(); }
        // Capture only strings and shared lifetime state; no UObject is accessed off the game thread.
        if (IsInGameThread()) { return MakeFulfilledPromise<FOutput>(Run(Name, Input, State)).GetFuture(); }
        auto Promise = MakeShared<TPromise<FOutput>, ESPMode::ThreadSafe>(); auto Future = Promise->GetFuture();
        const auto LiveState = State;
        AsyncTask(ENamedThreads::GameThread, [Promise, LiveState, Name, Input]() { Promise->SetValue(Run(Name, Input, LiveState)); });
        return Future;
    }
private:
    TSharedRef<FSkeletalPhysicsRawCallState, ESPMode::ThreadSafe> State;
};
}

bool FSkeletalPhysicsRawRegistration::Register()
{
    if (!IsInGameThread() || !UObjectInitialized() || IsEngineExitRequested()) { return false; }
    if (Handler.IsValid())
    { auto Subsystem = RegistrySubsystem.Get(); return Subsystem && Subsystem->ToolsetRegistry.Find(ToolsetName) == Handler; }
    const FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
    const FGuid InstanceId = FApp::GetInstanceId();
    if (FPaths::GetProjectFilePath().IsEmpty() || ProjectFile.IsEmpty() || !InstanceId.IsValid()) { return false; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless fixed skeletal PhysicsAsset raw registration"));
    if (!Subsystem.HasValue()) { return false; }
    if (Subsystem.GetValue()->ToolsetRegistry.Find(ToolsetName).IsValid()) { return false; } // Never replace any existing handler.
    auto State = MakeShared<FSkeletalPhysicsRawCallState, ESPMode::ThreadSafe>(); State->Subsystem = Subsystem.GetValue().Get();
    State->ProjectFile = ProjectFile; State->InstanceId = InstanceId;
    TSharedPtr<UE::ToolsetRegistry::FToolset> Candidate = MakeShared<FSkeletalPhysicsRawToolset>(State);
    State->Identity = Candidate.Get();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterToolset(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get(); Handler = Candidate; CallState = State; State->Active.Store(true);
    return true;
}

void FSkeletalPhysicsRawRegistration::Unregister()
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
