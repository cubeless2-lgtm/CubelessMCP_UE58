#include "Registration/CubelessStateTreeStrictDryRunToolset.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Future.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/CoreMisc.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/Atomic.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/FieldIterator.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::Registration
{
namespace
{
using FToolset = UE::ToolsetRegistry::FToolset;
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.state_tree_toolset.CubelessStateTreeTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/state_tree_toolset_PY.CubelessStateTreeTools");
constexpr const TCHAR* AddMethod = TEXT("add_state_tree_state");
constexpr const TCHAR* DryField = TEXT("dry_run");

struct FAttachmentState
{
    explicit FAttachmentState(UClass* InClass) : Class(InClass) {}
    TStrongObjectPtr<UClass> Class;
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    TSharedPtr<FToolset> Original;
    TSharedPtr<FToolset> Wrapper;
    FString FullSchema;
    FString Version;
    FString Description;
    TAtomic<bool> Active { false };
    TAtomic<int32> InFlight { 0 };
    TAtomic<bool> AddDelegation { false };
    bool OriginalDetached = false;
};
// One Editor module instance owns these fields. Wrapper/state cycle is broken by removal.
TSharedPtr<FAttachmentState> Owned;
bool OperationBusy = false;
struct FBusyScope
{
    FBusyScope() { OperationBusy = true; }
    ~FBusyScope() { OperationBusy = false; }
};

FString Json(const TSharedRef<FJsonObject>& Object)
{
    FString Out;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out));
    return Out;
}

FString Rejection(const FString& Code, const FString& Message, const FString& Method)
{
    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false);
    // This receipt is not an operation journal and cannot be replayed as one.
    R->SetStringField(TEXT("operation_id"), TEXT(""));
    R->SetStringField(TEXT("stage"), Method.Equals(AddMethod, ESearchCase::CaseSensitive) ? TEXT("strict_dry_run_pre_dispatch") : TEXT("state_tree_guard_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("strict_dry_run_not_dispatched"));
    R->SetStringField(TEXT("compile_json"), Json(Compile));
    auto Save = MakeShared<FJsonObject>();
    Save->SetBoolField(TEXT("requested"), false); Save->SetBoolField(TEXT("attempted"), false); Save->SetBoolField(TEXT("saved"), false);
    R->SetStringField(TEXT("save_json"), Json(Save));
    auto Validation = MakeShared<FJsonObject>();
    Validation->SetStringField(TEXT("status"), TEXT("raw_input_rejected_before_struct_packing"));
    Validation->SetStringField(TEXT("disk_reload"), TEXT("not_run")); Validation->SetStringField(TEXT("packaged_runtime"), TEXT("not_run"));
    R->SetStringField(TEXT("validation_json"), Json(Validation));
    auto Retry = MakeShared<FJsonObject>();
    Retry->SetBoolField(TEXT("automatic_retry"), false); Retry->SetBoolField(TEXT("replayed"), false); Retry->SetBoolField(TEXT("atomic_rollback"), false);
    Retry->SetStringField(TEXT("duplicate_risk"), TEXT("none_observed"));
    R->SetStringField(TEXT("retry_json"), Json(Retry));
    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("operation"), Method);
    Data->SetBoolField(TEXT("native_dispatched"), false); Data->SetBoolField(TEXT("python_dispatched"), false);
    Data->SetBoolField(TEXT("mutation_attempted"), false); Data->SetBoolField(TEXT("mutation_performed"), false);
    Data->SetBoolField(TEXT("mutation_state_unknown"), false); Data->SetBoolField(TEXT("change_state_known"), true); Data->SetBoolField(TEXT("changed"), false);
    Data->SetBoolField(TEXT("journal_created"), false); Data->SetBoolField(TEXT("automatic_action_retry"), false);
    Data->SetNumberField(TEXT("explicit_compile_calls"), 0); Data->SetNumberField(TEXT("explicit_save_calls"), 0);
    Data->SetBoolField(TEXT("unique_external_pipeline_complete"), false); Data->SetBoolField(TEXT("packaged_runtime_verified"), false);
    R->SetStringField(TEXT("data_json"), Json(Data));
    auto Outer = MakeShared<FJsonObject>(); Outer->SetObjectField(TEXT("returnValue"), R);
    return Json(Outer);
}

bool RawAllowed(const FString& Input, FString& Error)
{
    TSharedPtr<FJsonObject> Args;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Args) || !Args.IsValid())
    { Error = TEXT("StateTree arguments must be a JSON object; no reflected packing or dispatch occurred"); return false; }
    const TSharedPtr<FJsonValue>* Value = Args->Values.Find(DryField);
    if (Value && (!Value->IsValid() || (*Value)->Type != EJson::Boolean))
    { Error = TEXT("add_state_tree_state.dry_run requires an exact JSON boolean, or omission for default true; no reflected packing or dispatch occurred"); return false; }
    return true;
}

bool Metadata(UClass* Class, FString& Error)
{
    if (!Class || !IsValid(Class) || Class->HasAnyClassFlags(CLASS_NewerVersionExists)
        || !Class->GetPathName().Equals(CanonicalClass, ESearchCase::CaseSensitive)
        || !Class->IsChildOf(UToolsetDefinition::StaticClass()))
    { Error = TEXT("Exact live canonical StateTree Toolset class required; no metadata is written"); return false; }
    UFunction* Function = Class->FindFunctionByName(FName(AddMethod));
    if (!Function || Function->GetOuter() != Class || !Function->GetName().Equals(AddMethod, ESearchCase::CaseSensitive))
    { Error = TEXT("Exact canonical add_state_tree_state function required"); return false; }
    const auto Callable = UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));
    if (!Callable.HasValue() || !Callable.GetValue()) { Error = TEXT("Original add_state_tree_state must remain officially callable"); return false; }
    FProperty* Property = Function->FindPropertyByName(FName(DryField));
    if (!CastField<FBoolProperty>(Property) || Property->GetOwnerStruct() != Function || Property->ArrayDim != 1
        || !Property->GetName().Equals(DryField, ESearchCase::CaseSensitive)
        || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
        || !Function->HasMetaData(TEXT("CPP_Default_dry_run"))
        || !Function->GetMetaData(TEXT("CPP_Default_dry_run")).Equals(TEXT("True"), ESearchCase::IgnoreCase))
    { Error = TEXT("Original scalar FBool dry_run with unchanged default True required"); return false; }
    return true;
}

// This only adjusts the detached retained handler's local filters. Registry user lists are untouched.
void SyncOriginalFilters(const TSharedRef<FAttachmentState>& State)
{
    if (UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get())
    { State->Original->SetNameFilters(Subsystem->ToolsetRegistry.GetBlockedNames(), Subsystem->ToolsetRegistry.GetAllowedNames()); }
}

FString UnfilteredOriginalSchema(const TSharedRef<FAttachmentState>& State)
{
    // Public GetJsonSchema is filtered. Obtain all tools without editing any registry settings.
    State->Original->SetNameFilters({}, {});
    const FString Full = State->Original->GetJsonSchema();
    SyncOriginalFilters(State);
    // Retained unguarded handler is disabled between authorized wrapper delegations.
    State->Original->SetEnabled(false);
    return Full;
}

bool Schema(const FString& Full, FString& Error)
{
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Full), Root) || !Root.IsValid())
    { Error = TEXT("Original Toolset JSON schema is unavailable"); return false; }
    FString Name; const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!Root->TryGetStringField(TEXT("name"), Name) || !Name.Equals(CanonicalName, ESearchCase::CaseSensitive)
        || !Root->TryGetArrayField(TEXT("tools"), Rows) || !Rows || Rows->Num() != 5)
    { Error = TEXT("Exactly the original canonical StateTree tools5 schema required"); return false; }
    TSet<FString> Expected { TEXT("list_state_trees"), TEXT("inspect_state_tree"), TEXT("create_state_tree"), TEXT("add_state_tree_state"), TEXT("compile_state_tree") };
    for (const TSharedPtr<FJsonValue>& Row : *Rows)
    {
        const TSharedPtr<FJsonObject>* Tool = nullptr;
        if (!Row.IsValid() || !Row->TryGetObject(Tool) || !Tool || !Tool->IsValid() || !(*Tool)->TryGetStringField(TEXT("name"), Name))
        { Error = TEXT("Original tool schema row invalid"); return false; }
        const FString Prefix = FString(CanonicalName) + TEXT(".");
        if (!Name.StartsWith(Prefix, ESearchCase::CaseSensitive) || Expected.Remove(Name.RightChop(Prefix.Len())) != 1)
        { Error = TEXT("Original tool names changed or repeated"); return false; }
        const TSharedPtr<FJsonObject>* Output = nullptr; const TSharedPtr<FJsonObject>* Props = nullptr;
        const TSharedPtr<FJsonObject>* Return = nullptr; const TSharedPtr<FJsonObject>* Fields = nullptr;
        if (!(*Tool)->TryGetObjectField(TEXT("outputSchema"), Output) || !(*Output)->TryGetObjectField(TEXT("properties"), Props)
            || (*Props)->Values.Num() != 1 || !(*Props)->TryGetObjectField(TEXT("returnValue"), Return)
            || !(*Return)->TryGetObjectField(TEXT("properties"), Fields) || (*Fields)->Values.Num() != 11)
        { Error = TEXT("Original standard11 returnValue schema required"); return false; }
        for (const TCHAR* Key : { TEXT("success"), TEXT("operation_id"), TEXT("stage"), TEXT("error_code"), TEXT("error"), TEXT("changes_json"), TEXT("compile_json"), TEXT("save_json"), TEXT("validation_json"), TEXT("retry_json"), TEXT("data_json") })
        {
            const TSharedPtr<FJsonObject>* Field = nullptr; FString Type;
            if (!(*Fields)->TryGetObjectField(Key, Field) || !(*Field)->TryGetStringField(TEXT("type"), Type)
                || Type != (FString(Key) == TEXT("success") ? TEXT("boolean") : TEXT("string")))
            { Error = TEXT("Original standard11 field types required"); return false; }
        }
        if (Name == FString(CanonicalName) + TEXT(".") + AddMethod)
        {
            const TSharedPtr<FJsonObject>* Input = nullptr; const TSharedPtr<FJsonObject>* Inputs = nullptr;
            if (!(*Tool)->TryGetObjectField(TEXT("inputSchema"), Input) || !(*Input)->TryGetObjectField(TEXT("properties"), Inputs) || (*Inputs)->Values.Num() != 6)
            { Error = TEXT("Original add6 input schema required"); return false; }
            for (const TCHAR* Key : { TEXT("asset_path"), TEXT("state_name"), TEXT("parent_path"), TEXT("state_type"), TEXT("selection_behavior"), DryField })
            {
                const TSharedPtr<FJsonObject>* Field = nullptr; FString Type;
                if (!(*Inputs)->TryGetObjectField(Key, Field) || !(*Field)->TryGetStringField(TEXT("type"), Type)
                    || Type != (FString(Key) == DryField ? TEXT("boolean") : TEXT("string")))
                { Error = TEXT("Original add6 input field types required"); return false; }
                if (FString(Key) == DryField)
                {
                    const auto Default = (*Field)->TryGetField(TEXT("default")); bool Value = false;
                    if (!Default.IsValid() || Default->Type != EJson::Boolean || !Default->TryGetBool(Value) || !Value)
                    { Error = TEXT("Original omitted dry_run Boolean true schema default required"); return false; }
                }
            }
        }
    }
    return Expected.IsEmpty();
}

class FStrictDryRunToolset final : public FToolset
{
public:
    explicit FStrictDryRunToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return CanonicalName; }
    virtual FString GetToolsetVersion() const override { return State->Version; }
    virtual FString GetToolsetDescription() const override { return State->Description; }
    virtual UClass* GetToolsetClass() const override { return State->Class.Get(); }
protected:
    // Immutable full original schema is visible at the registration event, even while dispatch is inactive.
    // Base FToolset applies current wrapper filters. No UObjects or filter changes on schema reads.
    virtual FString GetJsonSchemaInternal() const override { return State->FullSchema; }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Method, const FString& Input) override
    {
        auto Reject = [&Method](const TCHAR* Code, const FString& Error)
        { return MakeFulfilledPromise<FOutput>(MakeValue(Rejection(Code, Error, Method))).GetFuture(); };
        if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || !State->Active.Load() || State->InFlight.Load() != 0)
        { return Reject(TEXT("state_tree_strict_guard_unavailable"), TEXT("A live active game-thread StateTree guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("state_tree_strict_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("state_tree_strict_guard_definition_changed"), Error); }
        if (Method.Equals(AddMethod, ESearchCase::CaseSensitive) && !RawAllowed(Input, Error))
        { return Reject(TEXT("state_tree_strict_dry_run_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->AddDelegation.Store(false);
        // Public SetNameFilters generates schema and can invoke converter callbacks.
        // Capture lifetimes and reject nested delegation before entering that path.
        State->InFlight.Store(1);
        SyncOriginalFilters(State);
        auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
        if (!State->Active.Load() || !CurrentSubsystem.HasValue()
            || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
            || Subsystem->ToolsetRegistry.Find(CanonicalName) != WrapperLifetime
            || !WrapperLifetime->IsToolEnabled(FString(CanonicalName) + TEXT(".") + Method)
            || !Metadata(State->Class.Get(), Error) || OriginalLifetime->GetToolsetClass() != State->Class.Get())
        {
            State->Active.Store(false); OriginalLifetime->SetEnabled(false);
            State->AddDelegation.Store(false); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("StateTree ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("state_tree_strict_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named add delegation may ask Verify for the admission marker.
        State->AddDelegation.Store(Method.Equals(AddMethod, ESearchCase::CaseSensitive));
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, Input).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->AddDelegation.Store(false); CallState->InFlight.Store(0);
                return MoveTemp(Output);
            });
    }
private:
    TSharedRef<FAttachmentState> State;
};

void BaseReport(const TSharedRef<FJsonObject>& Report)
{
    Report->SetBoolField(TEXT("ready"), false); Report->SetBoolField(TEXT("verified"), false);
    Report->SetBoolField(TEXT("asset_mutation"), false); Report->SetBoolField(TEXT("metadata_mutation"), false);
    Report->SetBoolField(TEXT("actual_named_wire_verified"), false); Report->SetBoolField(TEXT("registry_changed"), false);
    Report->SetBoolField(TEXT("guard_attached"), false); Report->SetBoolField(TEXT("original_handler_detached"), false);
    Report->SetBoolField(TEXT("cleanup_required"), Owned.IsValid());
    Report->SetBoolField(TEXT("class_unregister_allowed"), !Owned.IsValid());
    Report->SetBoolField(TEXT("scratch_converter_ready_is_wire_proof"), false);
    Report->SetBoolField(TEXT("delegated_call_admitted"), false);
    Report->SetBoolField(TEXT("delegated_admission_only"), false);
    Report->SetBoolField(TEXT("original_handler_restored"), false);
    Report->SetStringField(TEXT("guard_scope"), TEXT("canonical add_state_tree_state.dry_run raw JSON only"));
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured StateTree guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured StateTree wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    if (!Schema(CurrentSchema, Error) || CurrentSchema != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original StateTree schema changed; no automatic replacement or dispatch"); } return false; }
    Report->SetBoolField(TEXT("boolean_schema_verified"), true); Report->SetBoolField(TEXT("default_verified"), true);
    Report->SetBoolField(TEXT("raw_guard_installed"), true); Report->SetBoolField(TEXT("original5_schema_preserved"), true);
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("StateTree guard is installed but disabled by existing toolset state or user filters"); return false; }
    return true;
}

bool Cleanup(const TSharedRef<FJsonObject>& Report, FString& Error)
{
    if (!Owned.IsValid()) { Report->SetBoolField(TEXT("cleanup_required"), false); return true; }
    const TSharedRef<FAttachmentState> State = Owned.ToSharedRef(); State->Active.Store(false);
    if (State->Original.IsValid()) { State->Original->SetEnabled(false); }
    UToolsetRegistrySubsystem* Subsystem = UObjectInitialized() ? State->Subsystem.Get() : nullptr;
    bool Changed = false;
    if (Subsystem)
    {
        // SDK removal is name-only. Probe and remove only retained identities, including filtered owners.
        if (State->Wrapper.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper))
        { Changed |= Subsystem->ToolsetRegistry.UnregisterToolset(State->Wrapper); }
        // Failed installation can leave the captured, disabled original registered. Never restore it.
        if (State->Original.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Original))
        { Changed |= Subsystem->ToolsetRegistry.UnregisterToolset(State->Original); }
        const bool Remains = (State->Wrapper.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper))
            || (State->Original.IsValid() && HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Original));
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned StateTree guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallStateTreeStrictDryRunToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("StateTree guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained StateTree owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
        const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
        Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load()); return Ok;
    }
    TSharedPtr<FToolset> Original = Current.GetValue()->ToolsetRegistry.Find(CanonicalName);
    if (!Original.IsValid() || Original->GetToolsetClass() != ToolsetClass || !Original->GetToolsetName().Equals(CanonicalName, ESearchCase::CaseSensitive))
    { Error = TEXT("Existing enabled exact canonical class handler required after Root-owned class registration; foreign/disabled entries not replaced"); return false; }
    const TSharedRef<FAttachmentState> State = MakeShared<FAttachmentState>(ToolsetClass);
    State->Subsystem = Current.GetValue().Get(); State->Original = Original;
    State->Version = Original->GetToolsetVersion(); State->Description = Original->GetToolsetDescription();
    // Ownership is captured before any local filtering, including a schema-verification failure.
    Owned = State;
    Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    State->FullSchema = UnfilteredOriginalSchema(State);
    if (!Schema(State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FStrictDryRunToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original StateTree handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("StateTree raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyStateTreeStrictDryRunToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("StateTree guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const bool Admitted = Owned->InFlight.Load() == 1 && Owned->AddDelegation.Load() && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named add delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed StateTree raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original StateTree registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveStateTreeStrictDryRunToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("StateTree guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained StateTree class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownStateTreeStrictDryRunToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
