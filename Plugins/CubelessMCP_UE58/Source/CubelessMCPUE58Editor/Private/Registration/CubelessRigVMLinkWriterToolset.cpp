#include "Registration/CubelessRigVMLinkWriterToolset.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Future.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/CoreMisc.h"
#include "Policies/CondensedJsonPrintPolicy.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.rigvm_link_writer_toolset.CubelessRigVMLinkWriterTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/rigvm_link_writer_toolset_PY.CubelessRigVMLinkWriterTools");
constexpr const TCHAR* RigVMLinkWriterMethod = TEXT("add_controlrig_rigvm_links");
constexpr const TCHAR* PathFields[] = {TEXT("control_rig_path"), TEXT("control_rig_asset_path"), TEXT("control_rig_class_path")};
constexpr const TCHAR* NullableFields[] = {TEXT("links"), TEXT("unit_links")};
constexpr const TCHAR* BoolFields[] = {TEXT("allow_type_casting"), TEXT("dry_run"), TEXT("compile"), TEXT("save_asset")};
constexpr bool BoolDefaults[] = {false, false, true, false};
constexpr const TCHAR* LimitField = TEXT("max_links");
constexpr const TCHAR* LimitDefault = TEXT("64");
constexpr int32 MaxInputChars = 4 * 1024 * 1024, MaxPathChars = 1024 * 1024, MaxCarrierChars = 1024 * 1024;

struct FAttachmentState
{
    explicit FAttachmentState(UClass* InClass) : Class(InClass) {}
    TStrongObjectPtr<UClass> Class;
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    TSharedPtr<FToolset> Original;
    TSharedPtr<FToolset> Wrapper;
    FString ReflectedSchema;
    FString FullSchema;
    FString Version;
    FString Description;
    TAtomic<bool> Active { false };
    TAtomic<int32> InFlight { 0 };
    TAtomic<bool> RigVMLinkWriterDelegation { false };
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
    R->SetStringField(TEXT("stage"), TEXT("rigvm_link_writer_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("rigvm_link_writer_raw_not_dispatched"));
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

// Keep Number tokens exact before the official reflected handler converts JSON values.
// Public integer1 remains typed; private decimal FString1 avoids Python int32 reflection.
bool IntegralDecimal(const TSharedPtr<FJsonValue>& Value, FString& Decimal)
{
    FString Token;
    if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetString(Token) || Token.IsEmpty()) { return false; }
    const bool Negative = Token[0] == TEXT('-'); int32 At = Negative ? 1 : 0;
    FString Digits; int32 FractionDigits = 0; bool Fraction = false;
    while (At < Token.Len() && Token[At] != TEXT('e') && Token[At] != TEXT('E'))
    {
        const TCHAR C = Token[At++];
        if (C == TEXT('.') && !Fraction) { Fraction = true; continue; }
        if (C < TEXT('0') || C > TEXT('9')) { return false; }
        Digits.AppendChar(C); if (Fraction) { ++FractionDigits; }
    }
    int32 Exponent = 0;
    if (At < Token.Len())
    {
        ++At; bool ExponentNegative = false;
        if (At < Token.Len() && (Token[At] == TEXT('+') || Token[At] == TEXT('-')))
        { ExponentNegative = Token[At++] == TEXT('-'); }
        if (At == Token.Len()) { return false; }
        // Larger magnitudes cannot make a nonzero bounded input fit int64.
        while (At < Token.Len())
        {
            const TCHAR C = Token[At++]; if (C < TEXT('0') || C > TEXT('9')) { return false; }
            Exponent = FMath::Min(Exponent * 10 + static_cast<int32>(C - TEXT('0')), MaxInputChars + 32);
        }
        if (ExponentNegative) { Exponent = -Exponent; }
    }
    int32 Leading = 0; while (Leading < Digits.Len() && Digits[Leading] == TEXT('0')) { ++Leading; }
    if (Leading == Digits.Len()) { Decimal = TEXT("0"); return true; }
    Digits = Digits.Mid(Leading);
    int32 Shift = Exponent - FractionDigits;
    if (Shift < 0)
    {
        if (-Shift > Digits.Len()) { return false; }
        for (int32 I = Digits.Len() + Shift; I < Digits.Len(); ++I) { if (Digits[I] != TEXT('0')) { return false; } }
        Digits.LeftChopInline(-Shift); Shift = 0;
    }
    if (static_cast<int64>(Digits.Len()) + Shift > 19) { return false; }
    while (Shift-- > 0) { Digits.AppendChar(TEXT('0')); }
    const uint64 Limit = static_cast<uint64>(MAX_int64) + (Negative ? 1u : 0u); uint64 Magnitude = 0;
    for (const TCHAR C : Digits)
    {
        const uint64 Digit = static_cast<uint64>(C - TEXT('0'));
        if (Magnitude > (Limit - Digit) / 10u) { return false; }
        Magnitude = Magnitude * 10u + Digit;
    }
    Decimal = Negative ? TEXT("-") + Digits : Digits; return true;
}

bool RawTransform(const FString& Input, FString& PackedInput, FString& Error)
{
    TSharedPtr<FJsonObject> Args;
    if (Input.Len() > MaxInputChars || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Args,
        FJsonSerializer::EFlags::StoreNumbersAsStrings) || !Args.IsValid())
    { Error = TEXT("RigVM link writer requires a bounded raw JSON object"); return false; }
    const TSet<FString> Allowed {TEXT("control_rig_path"), TEXT("control_rig_asset_path"), TEXT("control_rig_class_path"),
        TEXT("links"), TEXT("unit_links"), TEXT("allow_type_casting"), TEXT("dry_run"), TEXT("compile"), TEXT("save_asset"), TEXT("max_links")};
    for (const auto& Pair : Args->Values)
    { if (!Allowed.Contains(FString(Pair.Key.ToView()))) { Error = TEXT("Unexpected link writer input field; no reflected dispatch"); return false; } }
    for (const TCHAR* Key : PathFields)
    {
        const auto Value = Args->TryGetField(Key);
        if (!Value.IsValid()) { Args->SetStringField(Key, TEXT("")); continue; }
        if (Value->Type != EJson::String || Value->AsString().Len() > MaxPathChars)
        { Error = TEXT("RigVM path3 requires bounded exact strings; null is not an empty default"); return false; }
        for (const TCHAR C : Value->AsString())
        { if (C == TCHAR(0)) { Error = TEXT("RigVM path strings cannot contain NUL"); return false; } }
    }
    for (int32 I = 0; I < 4; ++I)
    {
        const auto Value = Args->TryGetField(BoolFields[I]);
        if (Value.IsValid() && Value->Type != EJson::Boolean)
        { Error = FString::Printf(TEXT("%s requires an exact JSON boolean; no reflected coercion"), BoolFields[I]); return false; }
        if (!Value.IsValid()) { Args->SetBoolField(BoolFields[I], BoolDefaults[I]); }
    }
    const auto Maximum = Args->TryGetField(LimitField); FString Decimal = LimitDefault;
    if (Maximum.IsValid() && !IntegralDecimal(Maximum, Decimal))
    { Error = TEXT("max_links requires an exact finite JSON integer within int64; bool/string/fraction/overflow refused"); return false; }
    Args->SetStringField(LimitField, Decimal);
    for (const TCHAR* Key : NullableFields)
    {
        auto Value = Args->TryGetField(Key); if (!Value.IsValid()) { Value = MakeShared<FJsonValueNull>(); }
        if (Value->Type != EJson::Null && Value->Type != EJson::Array)
        { Error = TEXT("links/unit_links require a JSON array of Any values or null"); return false; }
        // Only the outer shape is inspected here. Native validates its selected clamped prefix,
        // including string/object rows, dictionary value coercion and ignored-tail semantics.
        FString Carrier;
        const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Carrier);
        if (!FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer, false))
        { Error = TEXT("Link array carrier serialization failed"); return false; }
        const bool Closed = Writer->Close();
        TArray<TSharedPtr<FJsonValue>> Readback;
        if ((!Closed && Value->Type != EJson::Null) || Carrier.Len() > MaxCarrierChars
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Carrier + TEXT("]")), Readback)
            || Readback.Num() != 1 || !Readback[0].IsValid() || Readback[0]->Type != Value->Type
            || (Value->Type == EJson::Array && Readback[0]->AsArray().Num() != Value->AsArray().Num()))
        { Error = TEXT("Link nullable-array carrier readback failed or exceeded the Native bound"); return false; }
        Args->SetStringField(Key, Carrier);
    }
    PackedInput = Json(Args.ToSharedRef()); return true;
}
bool Metadata(UClass* Class, FString& Error)
{
    if (!Class || !IsValid(Class) || Class->HasAnyClassFlags(CLASS_NewerVersionExists)
        || !Class->GetPathName().Equals(CanonicalClass, ESearchCase::CaseSensitive) || !Class->IsChildOf(UToolsetDefinition::StaticClass()))
    { Error = TEXT("Exact live canonical RigVM link writer class required; metadata is never written"); return false; }
    UFunction* Function = Class->FindFunctionByName(FName(RigVMLinkWriterMethod));
    if (!Function || Function->GetOuter() != Class || !Function->GetName().Equals(RigVMLinkWriterMethod, ESearchCase::CaseSensitive))
    { Error = TEXT("Exact owned link writer method required"); return false; }
    const auto Callable = UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));
    if (!Callable.HasValue() || !Callable.GetValue()) { Error = TEXT("Link writer method must remain officially callable"); return false; }
    const TCHAR* Order[] = {TEXT("control_rig_path"), TEXT("control_rig_asset_path"), TEXT("control_rig_class_path"),
        TEXT("links"), TEXT("unit_links"), TEXT("allow_type_casting"), TEXT("dry_run"), TEXT("compile"), TEXT("save_asset"), TEXT("max_links")};
    int32 Index = 0;
    for (TFieldIterator<FProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm) || It->HasAnyPropertyFlags(CPF_ReturnParm)) { continue; }
        if (Index >= 10 || !It->GetName().Equals(Order[Index], ESearchCase::CaseSensitive) || It->GetOwnerStruct() != Function
            || It->ArrayDim != 1 || It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm)
            || (Index >= 5 && Index <= 8 ? !CastField<FBoolProperty>(*It) : !CastField<FStrProperty>(*It)))
        { Error = TEXT("Link writer10 ordered path3/nullable-carrier2/bool4/decimal1 properties required"); return false; }
        ++Index;
    }
    if (Index != 10) { Error = TEXT("Link writer10 reflected arity required"); return false; }
    for (const TCHAR* Key : PathFields)
    {
        const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"), Key));
        if (Function->HasMetaData(DefaultKey) && !Function->GetMetaData(DefaultKey).IsEmpty())
        { Error = TEXT("Original RigVM path3 defaults must stay empty"); return false; }
    }
    for (const TCHAR* Key : NullableFields)
    {
        const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"), Key));
        if (!Function->HasMetaData(DefaultKey) || Function->GetMetaData(DefaultKey) != TEXT("null"))
        { Error = TEXT("Nullable array2 carrier defaults must remain JSON null text"); return false; }
    }
    const FName MaximumDefault(TEXT("CPP_Default_max_links"));
    if (!Function->HasMetaData(MaximumDefault) || Function->GetMetaData(MaximumDefault) != LimitDefault)
    { Error = TEXT("Private max_links decimal default must remain 64"); return false; }
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

bool Required(const TSharedPtr<FJsonObject>& Schema, const TSet<FString>& Expected)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Schema.IsValid() || !Schema->TryGetArrayField(TEXT("required"), Values) || !Values || Values->Num() != Expected.Num()) { return false; }
    TSet<FString> Remaining = Expected;
    for (const auto& Value : *Values)
    { if (!Value.IsValid() || Value->Type != EJson::String || Remaining.Remove(Value->AsString()) != 1) { return false; } }
    return Remaining.IsEmpty();
}

bool Type(const TSharedPtr<FJsonObject>& Object, const TCHAR* Expected)
{
    FString Value; return Object.IsValid() && Object->TryGetStringField(TEXT("type"), Value) && Value == Expected;
}

bool RigVMLinkWriterSchema(const FString& Reflected, FString& Logical, FString& Error)
{
    TSharedPtr<FJsonObject> Root; const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr; FString Name;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reflected), Root) || !Root.IsValid()
        || !Root->TryGetStringField(TEXT("name"), Name) || Name != CanonicalName || !Root->TryGetArrayField(TEXT("tools"), Rows) || !Rows || Rows->Num() != 1)
    { Error = TEXT("Exactly one canonical RigVM link writer Toolset schema required"); return false; }
    const TSharedPtr<FJsonObject>* Tool = nullptr; const TSharedPtr<FJsonObject>* Input = nullptr;
    const TSharedPtr<FJsonObject>* Inputs = nullptr; const TSharedPtr<FJsonObject>* Output = nullptr;
    const TSharedPtr<FJsonObject>* Outer = nullptr; const TSharedPtr<FJsonObject>* Result = nullptr; const TSharedPtr<FJsonObject>* Fields = nullptr;
    const TSet<FString> ResultKeys { TEXT("success"), TEXT("operation_id"), TEXT("stage"), TEXT("error_code"), TEXT("error"),
        TEXT("changes_json"), TEXT("compile_json"), TEXT("save_json"), TEXT("validation_json"), TEXT("retry_json"), TEXT("data_json") };
    if (!(*Rows)[0].IsValid() || !(*Rows)[0]->TryGetObject(Tool) || !Tool || !Tool->IsValid()
        || !(*Tool)->TryGetStringField(TEXT("name"), Name) || Name != FString(CanonicalName) + TEXT(".") + RigVMLinkWriterMethod
        || !(*Tool)->TryGetObjectField(TEXT("inputSchema"), Input) || !Type(*Input, TEXT("object"))
        || !(*Input)->TryGetObjectField(TEXT("properties"), Inputs) || (*Inputs)->Values.Num() != 10
        || !Required(*Input, {TEXT("control_rig_path"), TEXT("control_rig_asset_path"), TEXT("control_rig_class_path")}))
    { Error = TEXT("Exact link writer10 physical fields and required empty-default path3 required"); return false; }
    for (const TCHAR* Key : PathFields)
    {
        const TSharedPtr<FJsonObject>* Field = nullptr;
        if (!(*Inputs)->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("string")) || (*Field)->TryGetField(TEXT("default")).IsValid())
        { Error = TEXT("Physical path3 must be strings without emitted empty defaults"); return false; }
        (*Field)->SetStringField(TEXT("default"), TEXT(""));
    }
    for (const TCHAR* Key : NullableFields)
    {
        const TSharedPtr<FJsonObject>* Field = nullptr;
        if (!(*Inputs)->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("string")))
        { Error = TEXT("Nullable link array2 must be private reflected FString carriers"); return false; }
        const auto Default = (*Field)->TryGetField(TEXT("default"));
        if (!Default.IsValid() || Default->Type != EJson::String || Default->AsString() != TEXT("null"))
        { Error = TEXT("Nullable link array2 defaults must remain JSON null text"); return false; }
        (*Field)->SetArrayField(TEXT("type"), {MakeShared<FJsonValueString>(TEXT("array")), MakeShared<FJsonValueString>(TEXT("null"))});
        (*Field)->SetField(TEXT("default"), MakeShared<FJsonValueNull>());
        // List[Any] preserves selected-prefix validation, empty-array errors and ignored tails in Native.
        (*Field)->SetObjectField(TEXT("items"), MakeShared<FJsonObject>());
    }
    for (int32 I = 0; I < 4; ++I)
    {
        const TSharedPtr<FJsonObject>* Field = nullptr;
        if (!(*Inputs)->TryGetObjectField(BoolFields[I], Field) || !Type(*Field, TEXT("boolean")))
        { Error = TEXT("Original link writer boolean4 schema required"); return false; }
        const auto Default = (*Field)->TryGetField(TEXT("default"));
        if (!Default.IsValid() || Default->Type != EJson::Boolean || Default->AsBool() != BoolDefaults[I])
        { Error = TEXT("Original link writer boolean defaults False/False/True/False required"); return false; }
    }
    const TSharedPtr<FJsonObject>* Limit = nullptr;
    if (!(*Inputs)->TryGetObjectField(LimitField, Limit) || !Type(*Limit, TEXT("string")))
    { Error = TEXT("Private max_links decimal FString schema required"); return false; }
    const auto MaximumDefault = (*Limit)->TryGetField(TEXT("default"));
    if (!MaximumDefault.IsValid() || MaximumDefault->Type != EJson::String || MaximumDefault->AsString() != LimitDefault)
    { Error = TEXT("Private max_links default must remain64"); return false; }
    (*Limit)->SetStringField(TEXT("type"), TEXT("integer")); (*Limit)->SetNumberField(TEXT("default"), 64);
    (*Input)->SetArrayField(TEXT("required"), {});
    if (!(*Tool)->TryGetObjectField(TEXT("outputSchema"), Output) || !Type(*Output, TEXT("object")) || !Required(*Output, {TEXT("returnValue")})
        || !(*Output)->TryGetObjectField(TEXT("properties"), Outer) || (*Outer)->Values.Num() != 1 || !(*Outer)->TryGetObjectField(TEXT("returnValue"), Result)
        || !Type(*Result, TEXT("object")) || !Required(*Result, ResultKeys) || !(*Result)->TryGetObjectField(TEXT("properties"), Fields) || (*Fields)->Values.Num() != 11)
    { Error = TEXT("Original synchronous standard11 output envelope required"); return false; }
    for (const auto& Key : ResultKeys)
    { const TSharedPtr<FJsonObject>* Field = nullptr; if (!(*Fields)->TryGetObjectField(Key, Field) || !Type(*Field, Key == TEXT("success") ? TEXT("boolean") : TEXT("string")))
      { Error = TEXT("Original standard11 field types required"); return false; } }
    Logical = Json(Root.ToSharedRef()); return true;
}

class FRigVMLinkWriterToolset final : public FToolset
{
public:
    explicit FRigVMLinkWriterToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
    virtual FString GetToolsetName() const override { return CanonicalName; }
    virtual FString GetToolsetVersion() const override { return State->Version; }
    virtual FString GetToolsetDescription() const override { return State->Description; }
    virtual UClass* GetToolsetClass() const override { return State->Class.Get(); }
protected:
    // Immutable full projected logical schema is visible at the registration event, even while dispatch is inactive.
    // Base FToolset applies current wrapper filters. No UObjects or filter changes on schema reads.
    virtual FString GetJsonSchemaInternal() const override { return State->FullSchema; }
    virtual TFuture<FOutput> ExecuteToolInternal(const FString& Method, const FString& Input) override
    {
        auto Reject = [&Method](const TCHAR* Code, const FString& Error)
        { return MakeFulfilledPromise<FOutput>(MakeValue(Rejection(Code, Error, Method))).GetFuture(); };
        if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || !State->Active.Load() || State->InFlight.Load() != 0)
        { return Reject(TEXT("rigvm_link_writer_raw_guard_unavailable"), TEXT("A live active game-thread RigVMLinkWriter guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("rigvm_link_writer_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("rigvm_link_writer_raw_guard_definition_changed"), Error); }
        if (!Method.Equals(RigVMLinkWriterMethod, ESearchCase::CaseSensitive))
        { return Reject(TEXT("rigvm_link_writer_method_refused"), TEXT("Only the exact original link writer method is guarded")); }
        FString PackedInput;
        if (!RawTransform(Input, PackedInput, Error))
        { return Reject(TEXT("rigvm_link_writer_raw_arguments_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->RigVMLinkWriterDelegation.Store(false);
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
            State->RigVMLinkWriterDelegation.Store(false); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("RigVMLinkWriter ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("rigvm_link_writer_raw_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named method delegation may ask Verify for its exact admission marker.
        State->RigVMLinkWriterDelegation.Store(Method.Equals(RigVMLinkWriterMethod, ESearchCase::CaseSensitive));
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, PackedInput).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->RigVMLinkWriterDelegation.Store(false); CallState->InFlight.Store(0);
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
    Report->SetBoolField(TEXT("schema_adapter_ready_is_wire_proof"), false);
    Report->SetBoolField(TEXT("delegated_call_admitted"), false);
    Report->SetBoolField(TEXT("delegated_admission_only"), false);
    Report->SetStringField(TEXT("delegated_method"), TEXT(""));
    Report->SetStringField(TEXT("logical_schema_json"), TEXT(""));
    Report->SetStringField(TEXT("logical_schema_source"), TEXT(""));
    Report->SetBoolField(TEXT("logical_schema_registry_identity_verified"), false);
    Report->SetBoolField(TEXT("original_handler_restored"), false);
    Report->SetStringField(TEXT("guard_scope"), TEXT("canonical add_controlrig_rigvm_links logical10: optional empty path3, nullable array[Any]2, exact bool4 and integer1 with private JSON2/decimal1 carriers"));
    for (const TCHAR* Key : { TEXT("active"), TEXT("raw_guard_installed"), TEXT("default_verified"), TEXT("reflected_carrier_schema_verified"),
        TEXT("logical_schema_verified"), TEXT("path3_schema_verified"), TEXT("nullable2_schema_verified"), TEXT("boolean4_schema_verified"), TEXT("integer1_schema_verified"), TEXT("empty_path_defaults_verified"), TEXT("output11_schema_preserved"), TEXT("link_writer10_schema_verified") })
    { Report->SetBoolField(Key, false); }
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured RigVMLinkWriter guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured RigVMLinkWriter wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!RigVMLinkWriterSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original RigVMLinkWriter schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("RigVMLinkWriter wrapper owner/class changed during reflected schema callbacks"); } return false; }
    Report->SetBoolField(TEXT("reflected_carrier_schema_verified"), true); Report->SetBoolField(TEXT("logical_schema_verified"), true);
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("raw_guard_installed"), true);
    Report->SetBoolField(TEXT("path3_schema_verified"), true); Report->SetBoolField(TEXT("nullable2_schema_verified"), true); Report->SetBoolField(TEXT("integer1_schema_verified"), true); Report->SetBoolField(TEXT("empty_path_defaults_verified"), true); Report->SetBoolField(TEXT("output11_schema_preserved"), true);
    Report->SetBoolField(TEXT("boolean4_schema_verified"), true);
    Report->SetBoolField(TEXT("link_writer10_schema_verified"), true);
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("RigVMLinkWriter guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered RigVMLinkWriter wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered RigVMLinkWriter logical schema/owner/class receipt could not be confirmed"); }
      return false; }
    Report->SetStringField(TEXT("logical_schema_json"), RegisteredLogicalSchema);
    Report->SetStringField(TEXT("logical_schema_source"), TEXT("actual_registered_wrapper_GetJsonSchema"));
    Report->SetBoolField(TEXT("logical_schema_registry_identity_verified"), true);
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned RigVMLinkWriter guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallRigVMLinkWriterToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("RigVMLinkWriter guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained RigVMLinkWriter owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    State->ReflectedSchema = UnfilteredOriginalSchema(State);
    Report->SetStringField(TEXT("reflected_schema_json"), State->ReflectedSchema);
    if (!RigVMLinkWriterSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FRigVMLinkWriterToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original RigVMLinkWriter handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("RigVMLinkWriter raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyRigVMLinkWriterToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("RigVMLinkWriter guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const bool Admitted = Owned->InFlight.Load() == 1 && Owned->RigVMLinkWriterDelegation.Load() && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetStringField(TEXT("delegated_method"), Admitted ? RigVMLinkWriterMethod : TEXT(""));
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named link writer delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed RigVMLinkWriter raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original RigVMLinkWriter registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveRigVMLinkWriterToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("RigVMLinkWriter guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained RigVMLinkWriter class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownRigVMLinkWriterToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
