#include "Registration/CubelessPostProcessAnimDemoVariantToolset.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.postprocess_anim_demo_variant_toolset.CubelessPostProcessAnimDemoVariantTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/postprocess_anim_demo_variant_toolset_PY.CubelessPostProcessAnimDemoVariantTools");
constexpr const TCHAR* PostProcessAnimDemoVariantMethod = TEXT("ensure_postprocess_anim_demo_variant");
constexpr const TCHAR* StringFields[] = {TEXT("source_blueprint_name"), TEXT("source_skeletal_mesh"), TEXT("variant_name"), TEXT("target_root"), TEXT("target_blueprint_name"), TEXT("target_skeletal_mesh"), TEXT("bone_name"), TEXT("graph_name"), TEXT("graph_id"), TEXT("graph_type")};
constexpr const TCHAR* StringDefaults[] = {TEXT(""), TEXT(""), TEXT("Variant"), TEXT("/Game/_MCP_Sample/AnimStudy"), TEXT(""), TEXT(""), TEXT("head"), TEXT("AnimGraph"), TEXT(""), TEXT("function")};
constexpr const TCHAR* BoolFields[] = {TEXT("replace_existing"), TEXT("overwrite_existing"), TEXT("compile"), TEXT("save"), TEXT("dry_run"), TEXT("allow_non_sample")};
constexpr bool BoolDefaults[] = {true, false, true, true, false, false};
constexpr int32 MaxInputChars = 8 * 1024 * 1024, MaxStringChars = 1024 * 1024, MaxCarrierChars = 1024 * 1024;

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
    TAtomic<bool> PostProcessAnimDemoVariantDelegation { false };
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
    R->SetStringField(TEXT("stage"), TEXT("postprocess_anim_demo_variant_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("postprocess_anim_demo_variant_raw_not_dispatched"));
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

bool RawTransform(const FString& Input, FString& PackedInput, FString& Error)
{
    if (Input.Len() > MaxInputChars) { Error = TEXT("Variant raw input exceeds 8Mi UTF16"); return false; }
    int32 Depth = 0; bool Quoted = false, Escaped = false;
    for (TCHAR Ch : Input)
    {
        if (Ch == 0) { Error = TEXT("Raw NUL is refused"); return false; }
        if (Quoted) { if (Escaped) { Escaped = false; } else if (Ch == TCHAR('\\')) { Escaped = true; } else if (Ch == TCHAR('"')) { Quoted = false; } continue; }
        if (Ch == TCHAR('"')) { Quoted = true; }
        else if (Ch == TCHAR('[') || Ch == TCHAR('{')) { if (++Depth > 65) { Error = TEXT("Raw JSON exceeds outer object plus rotation depth64"); return false; } }
        else if (Ch == TCHAR(']') || Ch == TCHAR('}')) { if (--Depth < 0) { Error = TEXT("Malformed raw JSON"); return false; } }
    }
    if (Depth || Quoted) { Error = TEXT("Malformed raw JSON"); return false; }
    TSharedPtr<FJsonObject> Args;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Args, FJsonSerializer::EFlags::StoreNumbersAsStrings) || !Args.IsValid())
    { Error = TEXT("A raw JSON object is required"); return false; }
    const TSet<FString> Allowed {TEXT("source_blueprint_name"), TEXT("source_skeletal_mesh"), TEXT("variant_name"), TEXT("target_root"), TEXT("target_blueprint_name"), TEXT("target_skeletal_mesh"), TEXT("bone_name"), TEXT("rotation"), TEXT("graph_name"), TEXT("graph_id"), TEXT("graph_type"), TEXT("replace_existing"), TEXT("overwrite_existing"), TEXT("compile"), TEXT("save"), TEXT("dry_run"), TEXT("allow_non_sample")};
    for (const auto& Pair : Args->Values)
    { if (!Allowed.Contains(FString(Pair.Key.ToView()))) { Error = TEXT("Unexpected public17 input; legacy aliases remain Batch-only"); return false; } }
    for (int32 I = 0; I < 10; ++I)
    {
        const TCHAR* Key = StringFields[I]; auto Value = Args->TryGetField(Key);
        if (!Value.IsValid())
        {
            if (I < 2) { Error = TEXT("Both source_blueprint_name and source_skeletal_mesh are required"); return false; }
            Args->SetStringField(Key, StringDefaults[I]); continue;
        }
        if (Value->Type != EJson::String || Value->AsString().Len() > MaxStringChars) { Error = TEXT("Exact bounded public strings are required; null is not an empty default"); return false; }
        for (TCHAR Ch : Value->AsString()) { if (Ch == 0) { Error = TEXT("Decoded string NUL is refused"); return false; } }
    }
    for (int32 I = 0; I < 6; ++I)
    {
        auto Value = Args->TryGetField(BoolFields[I]);
        if (Value.IsValid() && Value->Type != EJson::Boolean) { Error = TEXT("Exact public JSON boolean required"); return false; }
        if (!Value.IsValid()) { Args->SetBoolField(BoolFields[I], BoolDefaults[I]); }
    }
    auto Rotation = Args->TryGetField(TEXT("rotation"));
    if (!Rotation.IsValid()) { Rotation = MakeShared<FJsonValueNull>(); }
    // The original unannotated rotation is whole Any JSON, as in the existing EC tool.
    // None->default rotation and all rotation/domain parsing remain in the specific domain/Native operation.
    struct FValue { TSharedPtr<FJsonValue> Value; int32 Depth; }; TArray<FValue> Work {{Rotation, 0}}; int32 Slots = 0;
    while (!Work.IsEmpty())
    {
        auto Entry = Work.Pop(EAllowShrinking::No);
        if (!Entry.Value.IsValid() || ++Slots > 65536) { Error = TEXT("Rotation JSON value-slot bound exceeded"); return false; }
        if (Entry.Value->Type == EJson::Number) { double Number = 0; if (!Entry.Value->TryGetNumber(Number) || !FMath::IsFinite(Number)) { Error = TEXT("Rotation JSON numbers must be finite double values"); return false; } }
        if (Entry.Value->Type == EJson::String) { for (TCHAR Ch : Entry.Value->AsString()) { if (Ch == 0) { Error = TEXT("Decoded rotation NUL is refused"); return false; } } }
        if (Entry.Value->Type == EJson::Array || Entry.Value->Type == EJson::Object)
        {
            if (Entry.Depth + 1 > 64) { Error = TEXT("Rotation JSON depth64 exceeded"); return false; }
            if (Entry.Value->Type == EJson::Array) { for (const auto& Child : Entry.Value->AsArray()) { Work.Add({Child, Entry.Depth + 1}); } }
            else { for (const auto& Pair : Entry.Value->AsObject()->Values) { for (TCHAR Ch : Pair.Key.ToView()) { if (Ch == 0) { Error = TEXT("Decoded rotation key NUL is refused"); return false; } } Work.Add({Pair.Value, Entry.Depth + 1}); } }
        }
    }
    FString Carrier; auto Writer = TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Carrier);
    if (!FJsonSerializer::Serialize(Rotation.ToSharedRef(), TEXT(""), Writer, false)) { Error = TEXT("Rotation carrier serialization failed"); return false; }
    const bool Closed = Writer->Close(); const bool Scalar = Rotation->Type != EJson::Array && Rotation->Type != EJson::Object;
    TArray<TSharedPtr<FJsonValue>> Back;
    if ((!Closed && !Scalar) || Carrier.Len() > MaxCarrierChars
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Carrier + TEXT("]")), Back, FJsonSerializer::EFlags::None)
        || Back.Num() != 1 || !Back[0].IsValid() || Back[0]->Type != Rotation->Type)
    { Error = TEXT("Rotation JSON carrier readback/type/bound failed"); return false; }
    Args->SetStringField(TEXT("rotation"), Carrier);
    PackedInput = Json(Args.ToSharedRef()); return true;
}

bool Metadata(UClass* Class, FString& Error)
{
    if (!Class || !IsValid(Class) || Class->HasAnyClassFlags(CLASS_NewerVersionExists) || !Class->GetPathName().Equals(CanonicalClass, ESearchCase::CaseSensitive) || !Class->IsChildOf(UToolsetDefinition::StaticClass()))
    { Error = TEXT("Exact live canonical Post Process variant class required"); return false; }
    UFunction* Function = Class->FindFunctionByName(FName(PostProcessAnimDemoVariantMethod));
    if (!Function || Function->GetOuter() != Class || !Function->GetName().Equals(PostProcessAnimDemoVariantMethod, ESearchCase::CaseSensitive)) { Error = TEXT("Exact owned variant method required"); return false; }
    auto Callable = UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));
    if (!Callable.HasValue() || !Callable.GetValue()) { Error = TEXT("Officially callable method required"); return false; }
    const TCHAR* Order[] = {TEXT("source_blueprint_name"), TEXT("source_skeletal_mesh"), TEXT("variant_name"), TEXT("target_root"), TEXT("target_blueprint_name"), TEXT("target_skeletal_mesh"), TEXT("bone_name"), TEXT("rotation"), TEXT("graph_name"), TEXT("graph_id"), TEXT("graph_type"), TEXT("replace_existing"), TEXT("overwrite_existing"), TEXT("compile"), TEXT("save"), TEXT("dry_run"), TEXT("allow_non_sample")}; int32 Index = 0;
    for (TFieldIterator<FProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm) || It->HasAnyPropertyFlags(CPF_ReturnParm)) { continue; }
        if (Index >= 17 || !It->GetName().Equals(Order[Index], ESearchCase::CaseSensitive) || It->GetOwnerStruct() != Function || It->ArrayDim != 1 || It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm)
            || (Index >= 11 ? !CastField<FBoolProperty>(*It) : !CastField<FStrProperty>(*It)))
        { Error = TEXT("Physical17 must remain FString11/bool6 with one private rotation JSON carrier"); return false; } ++Index;
    }
    if (Index != 17) { Error = TEXT("Physical17 arity required"); return false; }
    for (int32 I = 0; I < 10; ++I)
    {
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), StringFields[I]));
        if (I < 2) { if (Function->HasMetaData(Key)) { Error = TEXT("Required source2 cannot gain metadata defaults"); return false; } continue; }
        const FString Expected = StringDefaults[I];
        if (Expected.IsEmpty() ? (Function->HasMetaData(Key) && !Function->GetMetaData(Key).IsEmpty()) : (!Function->HasMetaData(Key) || Function->GetMetaData(Key) != Expected))
        { Error = TEXT("Original optional string8 defaults differ"); return false; }
    }
    const FName RotationKey(TEXT("CPP_Default_rotation"));
    if (!Function->HasMetaData(RotationKey) || Function->GetMetaData(RotationKey) != TEXT("null")) { Error = TEXT("Private Any rotation carrier must default to null text"); return false; }
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

bool PostProcessAnimDemoVariantSchema(const FString& Reflected, FString& Logical, FString& Error)
{
    TSharedPtr<FJsonObject> Root; const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr; FString Name;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reflected), Root) || !Root.IsValid() || !Root->TryGetStringField(TEXT("name"), Name) || Name != CanonicalName || !Root->TryGetArrayField(TEXT("tools"), Rows) || !Rows || Rows->Num() != 1)
    { Error = TEXT("Exactly one canonical Post Process variant Toolset required"); return false; }
    const TSharedPtr<FJsonObject>* Tool = nullptr; const TSharedPtr<FJsonObject>* Input = nullptr; const TSharedPtr<FJsonObject>* Inputs = nullptr; const TSharedPtr<FJsonObject>* Output = nullptr;
    const TSharedPtr<FJsonObject>* Outer = nullptr; const TSharedPtr<FJsonObject>* Result = nullptr; const TSharedPtr<FJsonObject>* Fields = nullptr;
    const TSet<FString> ResultKeys {TEXT("success"),TEXT("operation_id"),TEXT("stage"),TEXT("error_code"),TEXT("error"),TEXT("changes_json"),TEXT("compile_json"),TEXT("save_json"),TEXT("validation_json"),TEXT("retry_json"),TEXT("data_json")};
    if (!(*Rows)[0].IsValid() || !(*Rows)[0]->TryGetObject(Tool) || !Tool || !Tool->IsValid() || !(*Tool)->TryGetStringField(TEXT("name"), Name) || Name != FString(CanonicalName) + TEXT(".") + PostProcessAnimDemoVariantMethod
        || !(*Tool)->TryGetObjectField(TEXT("inputSchema"), Input) || !Type(*Input, TEXT("object")) || !(*Input)->TryGetObjectField(TEXT("properties"), Inputs) || (*Inputs)->Values.Num() != 17
        || !Required(*Input, {TEXT("source_blueprint_name"),TEXT("source_skeletal_mesh"),TEXT("target_blueprint_name"),TEXT("target_skeletal_mesh"),TEXT("graph_id")}))
    { Error = TEXT("Exact physical17 fields/required source2 plus SDK-required empty3 must remain"); return false; }
    for (int32 I = 0; I < 10; ++I)
    {
        const TCHAR* Key = StringFields[I]; const TSharedPtr<FJsonObject>* Field = nullptr;
        if (!(*Inputs)->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("string"))) { Error = TEXT("Physical ordinary string10 required"); return false; }
        auto Default = (*Field)->TryGetField(TEXT("default"));
        if (I < 2) { if (Default.IsValid()) { Error = TEXT("Required source2 cannot gain defaults"); return false; } continue; }
        const FString Expected = StringDefaults[I];
        if (Expected.IsEmpty() ? Default.IsValid() : (!Default.IsValid() || Default->Type != EJson::String || Default->AsString() != Expected))
        { Error = TEXT("Original optional string defaults differ from SDK physical empty3/nonempty5"); return false; }
        (*Field)->SetStringField(TEXT("default"), Expected);
    }
    const TSharedPtr<FJsonObject>* Rotation = nullptr;
    if (!(*Inputs)->TryGetObjectField(TEXT("rotation"), Rotation) || !Type(*Rotation, TEXT("string"))) { Error = TEXT("Physical Any rotation JSON FString required"); return false; }
    auto RotationDefault = (*Rotation)->TryGetField(TEXT("default"));
    if (!RotationDefault.IsValid() || RotationDefault->Type != EJson::String || RotationDefault->AsString() != TEXT("null")) { Error = TEXT("Physical rotation default must be null text"); return false; }
    (*Rotation)->RemoveField(TEXT("type")); (*Rotation)->SetField(TEXT("default"), MakeShared<FJsonValueNull>());
    for (int32 I = 0; I < 6; ++I)
    {
        const TSharedPtr<FJsonObject>* Field = nullptr;
        if (!(*Inputs)->TryGetObjectField(BoolFields[I], Field) || !Type(*Field, TEXT("boolean"))) { Error = TEXT("Original boolean6 schema required"); return false; }
        auto Default = (*Field)->TryGetField(TEXT("default"));
        if (!Default.IsValid() || Default->Type != EJson::Boolean || Default->AsBool() != BoolDefaults[I]) { Error = TEXT("Original boolean6 defaults differ"); return false; }
    }
    (*Input)->SetArrayField(TEXT("required"), {MakeShared<FJsonValueString>(TEXT("source_blueprint_name")),MakeShared<FJsonValueString>(TEXT("source_skeletal_mesh"))});
    (*Input)->SetBoolField(TEXT("additionalProperties"), false);
    if (!(*Tool)->TryGetObjectField(TEXT("outputSchema"), Output) || !Type(*Output, TEXT("object")) || !Required(*Output, {TEXT("returnValue")})
        || !(*Output)->TryGetObjectField(TEXT("properties"), Outer) || (*Outer)->Values.Num() != 1 || !(*Outer)->TryGetObjectField(TEXT("returnValue"), Result)
        || !Type(*Result, TEXT("object")) || !Required(*Result, ResultKeys) || !(*Result)->TryGetObjectField(TEXT("properties"), Fields) || (*Fields)->Values.Num() != 11)
    { Error = TEXT("Original synchronous standard11 output envelope required"); return false; }
    for (const auto& Key : ResultKeys)
    { const TSharedPtr<FJsonObject>* Field = nullptr; if (!(*Fields)->TryGetObjectField(Key, Field) || !Type(*Field, Key == TEXT("success") ? TEXT("boolean") : TEXT("string")))
      { Error = TEXT("Original standard11 field types required"); return false; } }
    Logical = Json(Root.ToSharedRef()); return true;
}

class FPostProcessAnimDemoVariantToolset final : public FToolset
{
public:
    explicit FPostProcessAnimDemoVariantToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("postprocess_anim_demo_variant_raw_guard_unavailable"), TEXT("A live active game-thread PostProcessAnimDemoVariant guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("postprocess_anim_demo_variant_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("postprocess_anim_demo_variant_raw_guard_definition_changed"), Error); }
        if (!Method.Equals(PostProcessAnimDemoVariantMethod, ESearchCase::CaseSensitive))
        { return Reject(TEXT("postprocess_anim_demo_variant_method_refused"), TEXT("Only the exact original Post Process AnimBP variant method is guarded")); }
        FString PackedInput;
        if (!RawTransform(Input, PackedInput, Error))
        { return Reject(TEXT("postprocess_anim_demo_variant_raw_arguments_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->PostProcessAnimDemoVariantDelegation.Store(false);
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
            State->PostProcessAnimDemoVariantDelegation.Store(false); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("PostProcessAnimDemoVariant ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("postprocess_anim_demo_variant_raw_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named method delegation may ask Verify for its exact admission marker.
        State->PostProcessAnimDemoVariantDelegation.Store(Method.Equals(PostProcessAnimDemoVariantMethod, ESearchCase::CaseSensitive));
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, PackedInput).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->PostProcessAnimDemoVariantDelegation.Store(false); CallState->InFlight.Store(0);
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
    Report->SetStringField(TEXT("guard_scope"), TEXT("canonical Post Process variant logical17: required string sources2, optional string8 including empty defaults3, whole Any rotation/default null and exact bool6; private rotation JSON1 only, no new numeric carriers"));
    for (const TCHAR* Key : { TEXT("active"), TEXT("raw_guard_installed"), TEXT("default_verified"), TEXT("reflected_carrier_schema_verified"), TEXT("logical_schema_verified"), TEXT("output11_schema_preserved"), TEXT("ordinary_string10_schema_verified"), TEXT("rotation_any1_schema_verified"), TEXT("boolean6_schema_verified"), TEXT("required_source2_schema_verified"), TEXT("postprocess_variant17_schema_verified"), TEXT("empty_string_defaults_verified") })
    { Report->SetBoolField(Key, false); }
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured PostProcessAnimDemoVariant guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured PostProcessAnimDemoVariant wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!PostProcessAnimDemoVariantSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original PostProcessAnimDemoVariant schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("PostProcessAnimDemoVariant wrapper owner/class changed during reflected schema callbacks"); } return false; }
    Report->SetBoolField(TEXT("reflected_carrier_schema_verified"), true); Report->SetBoolField(TEXT("logical_schema_verified"), true);
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("raw_guard_installed"), true);
    for (const TCHAR* Key : { TEXT("output11_schema_preserved"), TEXT("ordinary_string10_schema_verified"), TEXT("rotation_any1_schema_verified"), TEXT("boolean6_schema_verified"), TEXT("required_source2_schema_verified"), TEXT("postprocess_variant17_schema_verified"), TEXT("empty_string_defaults_verified") })
    { Report->SetBoolField(Key, true); }
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("PostProcessAnimDemoVariant guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered PostProcessAnimDemoVariant wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered PostProcessAnimDemoVariant logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned PostProcessAnimDemoVariant guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallPostProcessAnimDemoVariantToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("PostProcessAnimDemoVariant guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained PostProcessAnimDemoVariant owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!PostProcessAnimDemoVariantSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FPostProcessAnimDemoVariantToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original PostProcessAnimDemoVariant handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("PostProcessAnimDemoVariant raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyPostProcessAnimDemoVariantToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("PostProcessAnimDemoVariant guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const bool Admitted = Owned->InFlight.Load() == 1 && Owned->PostProcessAnimDemoVariantDelegation.Load() && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetStringField(TEXT("delegated_method"), Admitted ? PostProcessAnimDemoVariantMethod : TEXT(""));
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named Post Process AnimBP variant delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed PostProcessAnimDemoVariant raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original PostProcessAnimDemoVariant registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemovePostProcessAnimDemoVariantToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("PostProcessAnimDemoVariant guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained PostProcessAnimDemoVariant class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownPostProcessAnimDemoVariantToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
