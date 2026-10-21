#include "Registration/CubelessContentAuditToolset.h"
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
#include <cmath>

namespace Cubeless::Registration
{
namespace
{
using FToolset = UE::ToolsetRegistry::FToolset;
using FOutput = TValueOrError<FString, FString>;
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.project_toolset.CubelessProjectTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/project_toolset_PY.CubelessProjectTools");
constexpr const TCHAR* AuditMethod = TEXT("audit_content_root_mcp");
constexpr const TCHAR* RefMethod = TEXT("replace_asset_references_mcp");
constexpr const TCHAR* RefNullableFields[] = { TEXT("replacements"), TEXT("target_paths"),
    TEXT("string_replacements"), TEXT("name_replacements") };
constexpr const TCHAR* RefBoolFields[] = { TEXT("dry_run"), TEXT("save_assets"), TEXT("scan_paths"),
    TEXT("refresh_materials"), TEXT("include_package_objects"), TEXT("allow_non_game_targets") };
constexpr const TCHAR* NullableFields[] = { TEXT("forbidden_dependency_prefixes"), TEXT("required_asset_paths"),
    TEXT("expected_asset_count"), TEXT("expected_class_counts") };
constexpr const TCHAR* BoolFields[] = { TEXT("recursive"), TEXT("scan_paths"), TEXT("include_dependencies"),
    TEXT("include_assets"), TEXT("write_report"), TEXT("fail_on_dirty_packages"), TEXT("fail_on_redirectors"), TEXT("fail_on_forbidden_dependencies") };
constexpr int32 MaxInputChars = 1024 * 1024;
constexpr int32 MaxElements = 4096;

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
    TAtomic<bool> AuditDelegation { false };
    TAtomic<bool> RefDelegation { false };
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
    R->SetStringField(TEXT("stage"), Method.Equals(AuditMethod, ESearchCase::CaseSensitive) ? TEXT("content_audit_raw_pre_dispatch")
        : Method.Equals(RefMethod, ESearchCase::CaseSensitive) ? TEXT("asset_reference_raw_pre_dispatch") : TEXT("content_audit_guard_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), Method.Equals(RefMethod, ESearchCase::CaseSensitive)
        ? TEXT("asset_reference_raw_not_dispatched") : TEXT("content_audit_raw_not_dispatched"));
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

bool IntegerValue(const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid() || Value->Type != EJson::Number) { return false; }
    const double Number = Value->AsNumber();
    return FMath::IsFinite(Number) && std::floor(Number) == Number && Number <= MAX_int32
        && Number >= MIN_int32;
}

bool NullableValue(const TCHAR* Field, const TSharedPtr<FJsonValue>& Value, FString& Error)
{
    if (!Value.IsValid()) { Error = TEXT("Invalid nullable JSON value"); return false; }
    if (Value->Type == EJson::Null) { return true; }
    if (FString(Field) == TEXT("expected_asset_count"))
    {
        if (IntegerValue(Value)) { return true; }
    }
    else if (FString(Field) == TEXT("expected_class_counts"))
    {
        if (Value->Type == EJson::Object && Value->AsObject().IsValid()
            && Value->AsObject()->Values.Num() <= MaxElements)
        {
            for (const auto& Pair : Value->AsObject()->Values)
            { if (Pair.Key.Len() >= NAME_SIZE || !IntegerValue(Pair.Value)) { Error = TEXT("Class counts require bounded string keys and exact integer values"); return false; } }
            return true;
        }
    }
    else if (Value->Type == EJson::Array && Value->AsArray().Num() <= MaxElements)
    {
        for (const auto& Item : Value->AsArray())
        { if (!Item.IsValid() || Item->Type != EJson::String || Item->AsString().Len() >= NAME_SIZE) { Error = TEXT("Nullable path arrays require bounded string items"); return false; } }
        return true;
    }
    Error = FString::Printf(TEXT("Logical audit field %s has an invalid JSON type or exceeds the existing Native bound"), Field); return false;
}

bool RefNullableValue(const TCHAR* Field, const TSharedPtr<FJsonValue>& Value, FString& Error)
{
    if (!Value.IsValid()) { Error = TEXT("Invalid reference nullable JSON value"); return false; }
    if (Value->Type == EJson::Null) { return true; }
    if (FString(Field) == TEXT("target_paths"))
    {
        if (Value->Type == EJson::Array && Value->AsArray().Num() <= MaxElements)
        {
            for (const auto& Item : Value->AsArray())
            { if (!Item.IsValid() || Item->Type != EJson::String || Item->AsString().Len() >= NAME_SIZE)
              { Error = TEXT("target_paths requires bounded JSON string items"); return false; } }
            return true;
        }
    }
    else if (Value->Type == EJson::Object && Value->AsObject().IsValid()
        && Value->AsObject()->Values.Num() <= MaxElements)
    {
        for (const auto& Pair : Value->AsObject()->Values)
        { if (Pair.Key.Len() > 4096 || !Pair.Value.IsValid() || Pair.Value->Type != EJson::String
              || Pair.Value->AsString().Len() > 4096)
          { Error = TEXT("Reference mappings require bounded string keys and exact string values"); return false; } }
        return true;
    }
    Error = FString::Printf(TEXT("Logical reference field %s requires its original map/array or null type within the Native bound"), Field); return false;
}

bool RawTransform(const FString& Input, FString& PackedInput, FString& Error, bool References = false)
{
    TSharedPtr<FJsonObject> Args;
    if (Input.Len() > MaxInputChars || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Args) || !Args.IsValid())
    { Error = TEXT("Project arguments require a bounded raw JSON object before reflected packing"); return false; }
    const TArray<const TCHAR*> Nullable = References ? TArray<const TCHAR*>{ RefNullableFields[0], RefNullableFields[1], RefNullableFields[2], RefNullableFields[3] }
        : TArray<const TCHAR*>{ NullableFields[0], NullableFields[1], NullableFields[2], NullableFields[3] };
    const TArray<const TCHAR*> Bools = References ? TArray<const TCHAR*>{ RefBoolFields[0], RefBoolFields[1], RefBoolFields[2], RefBoolFields[3], RefBoolFields[4], RefBoolFields[5] }
        : TArray<const TCHAR*>{ BoolFields[0], BoolFields[1], BoolFields[2], BoolFields[3], BoolFields[4], BoolFields[5], BoolFields[6], BoolFields[7] };
    TSet<FString> Allowed = References ? TSet<FString>{ TEXT("target_root"), TEXT("source_root") }
        : TSet<FString>{ TEXT("root_path"), TEXT("max_samples") };
    for (const TCHAR* Key : Bools) { Allowed.Add(Key); }
    for (const TCHAR* Key : Nullable) { Allowed.Add(Key); }
    for (const auto& Pair : Args->Values)
    { if (!Allowed.Contains(FString(Pair.Key.ToView()))) { Error = TEXT("Unexpected Project input field; no reflected dispatch occurred"); return false; } }
    if (References)
    {
        for (const TCHAR* Key : { TEXT("target_root"), TEXT("source_root") })
        {
            const auto* Value = Args->Values.Find(Key);
            if (Value && (!Value->IsValid() || (*Value)->Type != EJson::String))
            { Error = FString::Printf(TEXT("%s requires an exact JSON string, or omission for its original empty default"), Key); return false; }
            // The SDK physical function schema treats empty defaults as required.
            // Restore the public optional-root contract before delegated required-field validation.
            if (!Value) { Args->SetStringField(Key, TEXT("")); }
        }
    }
    else
    {
        auto Root = Args->TryGetField(TEXT("root_path"));
        if (!Root.IsValid() || Root->Type != EJson::String)
        { Error = TEXT("root_path requires an exact JSON string"); return false; }
    }
    for (const TCHAR* Key : Bools)
    {
        const auto* Value = Args->Values.Find(Key);
        if (Value && (!Value->IsValid() || (*Value)->Type != EJson::Boolean))
        { Error = FString::Printf(TEXT("%s requires an exact JSON boolean, or omission for its original default"), Key); return false; }
    }
    const auto* Max = Args->Values.Find(TEXT("max_samples"));
    if (!References && Max && !IntegerValue(*Max)) { Error = TEXT("max_samples requires an exact signed-int32 JSON integer"); return false; }
    for (const TCHAR* Key : Nullable)
    {
        auto Value = Args->TryGetField(Key);
        if (!Value.IsValid()) { Value = MakeShared<FJsonValueNull>(); }
        if (!(References ? RefNullableValue(Key, Value, Error) : NullableValue(Key, Value, Error))) { return false; }
        FString Carrier;
        const auto Writer = TJsonWriterFactory<>::Create(&Carrier);
        // SDK document Close reports false for a complete root scalar. Serialize first,
        // then flush the string writer and independently read back the supported value.
        if (!FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer, false))
        { Error = TEXT("Nullable JSON carrier serialization failed"); return false; }
        const bool DocumentClosed = Writer->Close();
        TArray<TSharedPtr<FJsonValue>> RoundTripValues;
        const bool Scalar = Value->Type == EJson::Null || Value->Type == EJson::Number;
        // Default SDK value deserialization also refuses non-composite roots.
        // A bounded one-element JSON array preserves null/0 and composite values equally.
        if ((!DocumentClosed && !Scalar) || Carrier.Len() > MaxInputChars
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Carrier + TEXT("]")), RoundTripValues)
            || RoundTripValues.Num() != 1 || !RoundTripValues[0].IsValid() || RoundTripValues[0]->Type != Value->Type
            || (Value->Type == EJson::Number && RoundTripValues[0]->AsNumber() != Value->AsNumber()))
        { Error = TEXT("Nullable carrier readback failed, changed its type/value or exceeded its Native bound"); return false; }
        // Each logical value becomes one FString; reflected null skipping and singleton unwrapping cannot alter it.
        Args->SetStringField(Key, Carrier);
    }
    PackedInput = Json(Args.ToSharedRef()); return true;
}

bool RefMetadata(UFunction* Function, FString& Error)
{
    const TCHAR* Order[] = { TEXT("replacements"), TEXT("target_root"), TEXT("target_paths"), TEXT("source_root"),
        TEXT("string_replacements"), TEXT("name_replacements"), TEXT("dry_run"), TEXT("save_assets"),
        TEXT("scan_paths"), TEXT("refresh_materials"), TEXT("include_package_objects"), TEXT("allow_non_game_targets") };
    int32 Index = 0;
    for (TFieldIterator<FProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        if (!It->HasAnyPropertyFlags(CPF_Parm) || It->HasAnyPropertyFlags(CPF_ReturnParm)) { continue; }
        if (Index >= 12 || !It->GetName().Equals(Order[Index], ESearchCase::CaseSensitive) || It->GetOwnerStruct() != Function
            || It->ArrayDim != 1 || It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReferenceParm)
            || (Index < 6 ? !CastField<FStrProperty>(*It) : !CastField<FBoolProperty>(*It)))
        { Error = TEXT("Original reference12 ordered roots/carriers/boolean scalar properties required"); return false; }
        ++Index;
    }
    if (Index != 12) { Error = TEXT("Original reference12 reflected arity required"); return false; }
    for (const TCHAR* Key : RefNullableFields)
    {
        const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"), Key));
        if (!Function->HasMetaData(DefaultKey) || Function->GetMetaData(DefaultKey) != TEXT("null"))
        { Error = TEXT("Reference nullable4 FString defaults must remain JSON null text"); return false; }
    }
    return true;
}

bool Metadata(UClass* Class, FString& Error)
{
    if (!Class || !IsValid(Class) || Class->HasAnyClassFlags(CLASS_NewerVersionExists)
        || !Class->GetPathName().Equals(CanonicalClass, ESearchCase::CaseSensitive)
        || !Class->IsChildOf(UToolsetDefinition::StaticClass()))
    { Error = TEXT("Exact live canonical Project Toolset class required; no metadata is written"); return false; }
    for (const TCHAR* Method : { TEXT("create_input_mapping"), AuditMethod, RefMethod })
    {
        UFunction* Function = Class->FindFunctionByName(FName(Method));
        if (!Function || Function->GetOuter() != Class || !Function->GetName().Equals(Method, ESearchCase::CaseSensitive))
        { Error = TEXT("Exact canonical Project method required"); return false; }
        const auto Callable = UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));
        if (!Callable.HasValue() || !Callable.GetValue()) { Error = TEXT("Original Project method must remain officially callable"); return false; }
        int32 Count = 0;
        for (TFieldIterator<FProperty> It(Function, EFieldIteratorFlags::ExcludeSuper); It; ++It)
        { if (It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_ReturnParm)) { ++Count; } }
        if (Count != (FString(Method) == AuditMethod ? 14 : FString(Method) == RefMethod ? 12 : 3))
        { Error = TEXT("Original Input3/audit14/reference12 reflected arities required"); return false; }
        if (FString(Method) == RefMethod) { if (!RefMetadata(Function, Error)) { return false; } continue; }
        if (FString(Method) != AuditMethod) { continue; }
        TSet<FString> Strings { TEXT("root_path") }; TSet<FString> Bools;
        for (const TCHAR* Key : NullableFields) { Strings.Add(Key); }
        for (const TCHAR* Key : BoolFields) { Bools.Add(Key); }
        for (const auto& Key : Strings)
        {
            FProperty* Property = Function->FindPropertyByName(FName(*Key));
            if (!CastField<FStrProperty>(Property) || Property->GetOwnerStruct() != Function || Property->ArrayDim != 1
                || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm))
            { Error = TEXT("Audit FString root and nullable4 carriers required"); return false; }
        }
        for (const auto& Key : Bools)
        {
            FProperty* Property = Function->FindPropertyByName(FName(*Key));
            if (!CastField<FBoolProperty>(Property) || Property->GetOwnerStruct() != Function || Property->ArrayDim != 1
                || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm))
            { Error = TEXT("Original audit scalar boolean properties required"); return false; }
        }
        FProperty* Max = Function->FindPropertyByName(FName(TEXT("max_samples")));
        if (!CastField<FIntProperty>(Max) || Max->ArrayDim != 1 || Max->GetOwnerStruct() != Function
            || !Max->HasAnyPropertyFlags(CPF_Parm) || Max->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm))
        { Error = TEXT("Original max_samples reflected integer required"); return false; }
        for (const TCHAR* Key : NullableFields)
        {
            const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"), Key));
            if (!Function->HasMetaData(DefaultKey) || Function->GetMetaData(DefaultKey) != TEXT("null"))
            { Error = TEXT("Hidden nullable4 string default must remain JSON null text"); return false; }
        }
    }
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

bool ProjectReferenceSchema(const TSharedPtr<FJsonObject>& Input, const TSharedPtr<FJsonObject>& Inputs, FString& Error)
{
    if (Inputs->Values.Num() != 12 || !Required(Input, { TEXT("target_root"), TEXT("source_root") }))
    { Error = TEXT("Actual reflected reference12 fields and required empty-default roots2 required"); return false; }
    for (const TCHAR* Key : { TEXT("target_root"), TEXT("source_root") })
    {
        const TSharedPtr<FJsonObject>* Field = nullptr;
        if (!Inputs->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("string")))
        { Error = TEXT("Original reference root string schema required"); return false; }
        const auto Default = (*Field)->TryGetField(TEXT("default"));
        if (Default.IsValid())
        { Error = TEXT("Actual reflected empty root defaults must remain absent"); return false; }
        // This changes only the wrapper schema, never UFunction metadata or the retained handler.
        (*Field)->SetStringField(TEXT("default"), TEXT(""));
    }
    for (const TCHAR* Key : RefBoolFields)
    {
        const TSharedPtr<FJsonObject>* Field = nullptr;
        const bool Expected = FString(Key) == TEXT("dry_run") || FString(Key) == TEXT("scan_paths") || FString(Key) == TEXT("refresh_materials");
        if (!Inputs->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("boolean")))
        { Error = TEXT("Original reference boolean schema required"); return false; }
        const auto Default = (*Field)->TryGetField(TEXT("default"));
        if (!Default.IsValid() || Default->Type != EJson::Boolean || Default->AsBool() != Expected)
        { Error = TEXT("Original reference boolean defaults required"); return false; }
    }
    for (const TCHAR* Key : RefNullableFields)
    {
        const TSharedPtr<FJsonObject>* Field = nullptr;
        if (!Inputs->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("string")))
        { Error = TEXT("Reference nullable4 must be reflected FString carriers"); return false; }
        const auto Default = (*Field)->TryGetField(TEXT("default"));
        if (!Default.IsValid() || Default->Type != EJson::String || Default->AsString() != TEXT("null"))
        { Error = TEXT("Reference carrier default must be JSON null text"); return false; }
        const bool Array = FString(Key) == TEXT("target_paths");
        TArray<TSharedPtr<FJsonValue>> Types { MakeShared<FJsonValueString>(Array ? TEXT("array") : TEXT("object")),
            MakeShared<FJsonValueString>(TEXT("null")) };
        (*Field)->SetArrayField(TEXT("type"), Types);
        (*Field)->SetField(TEXT("default"), MakeShared<FJsonValueNull>());
        auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("type"), TEXT("string"));
        (*Field)->SetObjectField(Array ? TEXT("items") : TEXT("additionalProperties"), Item);
    }
    Input->SetArrayField(TEXT("required"), {});
    return true;
}

bool ProjectSchema(const FString& Reflected, FString& Logical, FString& Error)
{
    TSharedPtr<FJsonObject> Root; const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr; FString Name;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reflected), Root) || !Root.IsValid()
        || !Root->TryGetStringField(TEXT("name"), Name) || Name != CanonicalName
        || !Root->TryGetArrayField(TEXT("tools"), Rows) || !Rows || Rows->Num() != 3)
    { Error = TEXT("Exactly canonical Project tools3 schema required"); return false; }
    TSet<FString> Methods { TEXT("create_input_mapping"), AuditMethod, RefMethod };
    const TSet<FString> ResultKeys { TEXT("success"), TEXT("operation_id"), TEXT("stage"), TEXT("error_code"), TEXT("error"),
        TEXT("changes_json"), TEXT("compile_json"), TEXT("save_json"), TEXT("validation_json"), TEXT("retry_json"), TEXT("data_json") };
    for (const auto& Row : *Rows)
    {
        const TSharedPtr<FJsonObject>* Tool = nullptr; const TSharedPtr<FJsonObject>* Output = nullptr;
        const TSharedPtr<FJsonObject>* OutputProps = nullptr; const TSharedPtr<FJsonObject>* Return = nullptr;
        const TSharedPtr<FJsonObject>* Fields = nullptr; const TSharedPtr<FJsonObject>* Input = nullptr;
        const TSharedPtr<FJsonObject>* Inputs = nullptr;
        const FString Prefix = FString(CanonicalName) + TEXT(".");
        if (!Row.IsValid() || !Row->TryGetObject(Tool) || !Tool || !Tool->IsValid()
            || !(*Tool)->TryGetStringField(TEXT("name"), Name) || !Name.StartsWith(Prefix, ESearchCase::CaseSensitive)
            || Methods.Remove(Name.RightChop(Prefix.Len())) != 1)
        { Error = TEXT("Project tool names changed or repeated"); return false; }
        if (!(*Tool)->TryGetObjectField(TEXT("outputSchema"), Output) || !Type(*Output, TEXT("object"))
            || !Required(*Output, { TEXT("returnValue") }) || !(*Output)->TryGetObjectField(TEXT("properties"), OutputProps)
            || (*OutputProps)->Values.Num() != 1 || !(*OutputProps)->TryGetObjectField(TEXT("returnValue"), Return)
            || !Type(*Return, TEXT("object")) || !Required(*Return, ResultKeys) || !(*Return)->TryGetObjectField(TEXT("properties"), Fields)
            || (*Fields)->Values.Num() != 11)
        { Error = TEXT("Original standard11 output envelope required"); return false; }
        for (const auto& Key : ResultKeys)
        {
            const TSharedPtr<FJsonObject>* Field = nullptr;
            if (!(*Fields)->TryGetObjectField(Key, Field) || !Type(*Field, Key == TEXT("success") ? TEXT("boolean") : TEXT("string")))
            { Error = TEXT("Original standard11 output types required"); return false; }
        }
        if (!(*Tool)->TryGetObjectField(TEXT("inputSchema"), Input) || !Type(*Input, TEXT("object"))
            || !(*Input)->TryGetObjectField(TEXT("properties"), Inputs))
        { Error = TEXT("Original Project input object required"); return false; }
        if (Name == Prefix + TEXT("create_input_mapping"))
        {
            if ((*Inputs)->Values.Num() != 3 || !Required(*Input, { TEXT("action_name"), TEXT("key") }))
            { Error = TEXT("Original Named Input3 fields and required2 must remain unchanged"); return false; }
            for (const TCHAR* Key : { TEXT("action_name"), TEXT("key"), TEXT("input_type") })
            {
                const TSharedPtr<FJsonObject>* Field = nullptr;
                if (!(*Inputs)->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("string")))
                { Error = TEXT("Original Input3 string schema required"); return false; }
                const auto Default = (*Field)->TryGetField(TEXT("default"));
                if (FString(Key) == TEXT("input_type"))
                { if (!Default.IsValid() || Default->Type != EJson::String || Default->AsString() != TEXT("Action")) { Error = TEXT("Original Input Action default required"); return false; } }
                else if (Default.IsValid()) { Error = TEXT("Required Input fields cannot gain defaults"); return false; }
            }
            // This complete row, including current r5 Input description, is retained byte-for-value.
            continue;
        }
        if (Name == Prefix + RefMethod)
        {
            if (!ProjectReferenceSchema(*Input, *Inputs, Error)) { return false; }
            continue;
        }
        if ((*Inputs)->Values.Num() != 14 || !Required(*Input, { TEXT("root_path") }))
        { Error = TEXT("Original audit14 fields and required root_path required"); return false; }
        const TSharedPtr<FJsonObject>* RootField = nullptr; const TSharedPtr<FJsonObject>* MaxField = nullptr;
        if (!(*Inputs)->TryGetObjectField(TEXT("root_path"), RootField) || !Type(*RootField, TEXT("string"))
            || (*RootField)->TryGetField(TEXT("default")).IsValid()
            || !(*Inputs)->TryGetObjectField(TEXT("max_samples"), MaxField) || !Type(*MaxField, TEXT("integer")))
        { Error = TEXT("Original audit root string and max_samples integer required"); return false; }
        const auto MaxDefault = (*MaxField)->TryGetField(TEXT("default"));
        if (!MaxDefault.IsValid() || MaxDefault->Type != EJson::Number || MaxDefault->AsNumber() != 50)
        { Error = TEXT("Original max_samples=50 required"); return false; }
        for (const TCHAR* Key : BoolFields)
        {
            const TSharedPtr<FJsonObject>* Field = nullptr;
            const bool Expected = FString(Key) != TEXT("include_assets") && FString(Key) != TEXT("write_report") && FString(Key) != TEXT("fail_on_dirty_packages");
            if (!(*Inputs)->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("boolean")))
            { Error = TEXT("Original audit boolean schema required"); return false; }
            const auto Default = (*Field)->TryGetField(TEXT("default"));
            if (!Default.IsValid() || Default->Type != EJson::Boolean || Default->AsBool() != Expected)
            { Error = TEXT("Original audit boolean defaults required"); return false; }
        }
        for (const TCHAR* Key : NullableFields)
        {
            const TSharedPtr<FJsonObject>* Field = nullptr;
            if (!(*Inputs)->TryGetObjectField(Key, Field) || !Type(*Field, TEXT("string")))
            { Error = TEXT("Reflected nullable4 must be FString carriers"); return false; }
            const auto Default = (*Field)->TryGetField(TEXT("default"));
            if (!Default.IsValid() || Default->Type != EJson::String || Default->AsString() != TEXT("null"))
            { Error = TEXT("Reflected carrier default must be JSON null text"); return false; }
            const TCHAR* LogicalType = FString(Key) == TEXT("expected_asset_count") ? TEXT("integer")
                : FString(Key) == TEXT("expected_class_counts") ? TEXT("object") : TEXT("array");
            TArray<TSharedPtr<FJsonValue>> Types { MakeShared<FJsonValueString>(LogicalType), MakeShared<FJsonValueString>(TEXT("null")) };
            (*Field)->SetArrayField(TEXT("type"), Types);
            (*Field)->SetField(TEXT("default"), MakeShared<FJsonValueNull>());
            if (FString(LogicalType) == TEXT("array"))
            { auto Items = MakeShared<FJsonObject>(); Items->SetStringField(TEXT("type"), TEXT("string")); (*Field)->SetObjectField(TEXT("items"), Items); }
            if (FString(LogicalType) == TEXT("object"))
            { auto Items = MakeShared<FJsonObject>(); Items->SetStringField(TEXT("type"), TEXT("integer")); (*Field)->SetObjectField(TEXT("additionalProperties"), Items); }
        }
    }
    if (!Methods.IsEmpty()) { Error = TEXT("Missing original Project tool"); return false; }
    Logical = Json(Root.ToSharedRef()); return true;
}

class FContentAuditToolset final : public FToolset
{
public:
    explicit FContentAuditToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("content_audit_raw_guard_unavailable"), TEXT("A live active game-thread Content audit guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("content_audit_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("content_audit_raw_guard_definition_changed"), Error); }
        FString PackedInput = Input;
        if (Method.Equals(AuditMethod, ESearchCase::CaseSensitive) && !RawTransform(Input, PackedInput, Error))
        { return Reject(TEXT("content_audit_raw_arguments_invalid"), Error); }
        if (Method.Equals(RefMethod, ESearchCase::CaseSensitive) && !RawTransform(Input, PackedInput, Error, true))
        { return Reject(TEXT("asset_reference_raw_arguments_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->AuditDelegation.Store(false);
        State->RefDelegation.Store(false);
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
            State->AuditDelegation.Store(false); State->RefDelegation.Store(false); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("Content audit ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("content_audit_raw_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named method delegation may ask Verify for its exact admission marker.
        State->AuditDelegation.Store(Method.Equals(AuditMethod, ESearchCase::CaseSensitive));
        State->RefDelegation.Store(Method.Equals(RefMethod, ESearchCase::CaseSensitive));
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, PackedInput).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->AuditDelegation.Store(false); CallState->RefDelegation.Store(false); CallState->InFlight.Store(0);
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
    Report->SetStringField(TEXT("guard_scope"), TEXT("canonical audit_content_root_mcp logical14 and replace_asset_references_mcp logical12 to each nullable4 FString carriers"));
    for (const TCHAR* Key : { TEXT("active"), TEXT("raw_guard_installed"), TEXT("default_verified"), TEXT("reflected_carrier_schema_verified"),
        TEXT("logical_schema_verified"), TEXT("input3_schema_preserved"), TEXT("output11_schema_preserved"), TEXT("reference12_schema_verified") })
    { Report->SetBoolField(Key, false); }
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured Content audit guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured Content audit wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!ProjectSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original Content audit schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Project wrapper owner/class changed during reflected schema callbacks"); } return false; }
    Report->SetBoolField(TEXT("reflected_carrier_schema_verified"), true); Report->SetBoolField(TEXT("logical_schema_verified"), true);
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("raw_guard_installed"), true);
    Report->SetBoolField(TEXT("input3_schema_preserved"), true); Report->SetBoolField(TEXT("output11_schema_preserved"), true);
    Report->SetBoolField(TEXT("reference12_schema_verified"), true);
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("Content audit guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered Project wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered Project logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned Content audit guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallContentAuditToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("Content audit guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained Content audit owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!ProjectSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FContentAuditToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original Content audit handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("Content audit raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyContentAuditToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("Content audit guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const bool Audit = Owned->AuditDelegation.Load(); const bool References = Owned->RefDelegation.Load();
        const bool Admitted = Owned->InFlight.Load() == 1 && (Audit != References) && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetStringField(TEXT("delegated_method"), Admitted ? (Audit ? AuditMethod : RefMethod) : TEXT(""));
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named audit/reference delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed Content audit raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original Content audit registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveContentAuditToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("Content audit guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained Content audit class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownContentAuditToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
