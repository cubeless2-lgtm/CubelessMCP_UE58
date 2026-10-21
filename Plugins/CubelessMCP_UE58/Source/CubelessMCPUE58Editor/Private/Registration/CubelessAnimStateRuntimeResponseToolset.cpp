#include "Registration/CubelessAnimStateRuntimeResponseToolset.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.anim_state_machine_runtime_response_toolset.CubelessAnimStateRuntimeResponseTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/anim_state_machine_runtime_response_toolset_PY.CubelessAnimStateRuntimeResponseTools");
constexpr const TCHAR* AnimStateRuntimeResponseMethod = TEXT("sample_anim_state_machine_runtime_response");
constexpr const TCHAR* StringFields[] = {TEXT("actor_label"), TEXT("actor_name"), TEXT("actor_path"), TEXT("component_name"), TEXT("name"), TEXT("state_machine_name")};
constexpr const TCHAR* StringDefaults[] = {TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT("case_0"), TEXT("")};
constexpr const TCHAR* NullableFields[] = {TEXT("cases"), TEXT("properties"), TEXT("curve_names")};
constexpr const TCHAR* BoolFields[] = {TEXT("refresh_bone_transforms"), TEXT("restore_after_case"), TEXT("include_baseline"), TEXT("include_states"), TEXT("include_state_weights"), TEXT("include_relevant_anim_times"), TEXT("include_transition_progress"), TEXT("include_inactive_transition_progress"), TEXT("include_montages"), TEXT("include_curves"), TEXT("prefer_pie_world"), TEXT("require_pie_world")};
constexpr bool BoolDefaults[] = {true, true, true, true, true, false, true, false, true, false, true, false};
constexpr const TCHAR* LimitFields[] = {TEXT("tick_count"), TEXT("max_state_machines"), TEXT("max_state_machine_instances_to_probe"), TEXT("max_states_per_machine"), TEXT("max_transitions_per_machine"), TEXT("max_curves")};
constexpr const TCHAR* LimitDefaults[] = {TEXT("1"), TEXT("32"), TEXT("256"), TEXT("64"), TEXT("128"), TEXT("128")};
constexpr int64 LimitDefaultValues[] = {1,32,256,64,128,128};
constexpr const TCHAR* NumberField = TEXT("tick_delta_time");
constexpr const TCHAR* NumberDefault = TEXT("0.03333333333333333");
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
    TAtomic<bool> AnimStateRuntimeResponseDelegation { false };
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
    R->SetStringField(TEXT("stage"), TEXT("anim_state_machine_runtime_response_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("anim_state_machine_runtime_response_raw_not_dispatched"));
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
// Public integer6 remain typed; private decimal FString2 avoid Python int32 reflection. A separate number1 text carrier avoids Python float32 reflection.
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
    if(Input.Len()>MaxInputChars){Error=TEXT("Runtime response sampler raw input exceeds 8Mi UTF16");return false;}
    int32 Depth=0;bool Quoted=false,Escaped=false;
    for(TCHAR Ch:Input)
    {
        if(Ch==0){Error=TEXT("Raw NUL is refused");return false;}
        if(Quoted){if(Escaped){Escaped=false;}else if(Ch==TCHAR('\\')){Escaped=true;}else if(Ch==TCHAR('"')){Quoted=false;}continue;}
        if(Ch==TCHAR('"')){Quoted=true;}else if(Ch==TCHAR('[')||Ch==TCHAR('{')){if(++Depth>65){Error=TEXT("Raw JSON exceeds outer wrapper plus Native depth64");return false;}}
        else if(Ch==TCHAR(']')||Ch==TCHAR('}')){if(--Depth<0){Error=TEXT("Malformed raw JSON");return false;}}
    }
    if(Depth||Quoted){Error=TEXT("Malformed raw JSON");return false;}
    TSharedPtr<FJsonObject> Args;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid())
    {Error=TEXT("A raw JSON object is required");return false;}
    const TSet<FString> Allowed {TEXT("actor_label"), TEXT("actor_name"), TEXT("actor_path"), TEXT("component_name"), TEXT("cases"), TEXT("properties"), TEXT("name"), TEXT("tick_count"), TEXT("tick_delta_time"), TEXT("refresh_bone_transforms"), TEXT("restore_after_case"), TEXT("include_baseline"), TEXT("state_machine_name"), TEXT("include_states"), TEXT("include_state_weights"), TEXT("include_relevant_anim_times"), TEXT("include_transition_progress"), TEXT("include_inactive_transition_progress"), TEXT("include_montages"), TEXT("include_curves"), TEXT("curve_names"), TEXT("max_state_machines"), TEXT("max_state_machine_instances_to_probe"), TEXT("max_states_per_machine"), TEXT("max_transitions_per_machine"), TEXT("max_curves"), TEXT("prefer_pie_world"), TEXT("require_pie_world")};
    for(const auto& Pair:Args->Values){if(!Allowed.Contains(FString(Pair.Key.ToView()))){Error=TEXT("Unexpected public28 input; hidden Native source is not a Named input");return false;}}
    for(int32 I=0;I<6;++I)
    {
        const TCHAR* Key=StringFields[I];
        auto Value=Args->TryGetField(Key);if(!Value.IsValid()){Args->SetStringField(Key,StringDefaults[I]);continue;}
        if(Value->Type!=EJson::String||Value->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded strings are required; null is not an empty default");return false;}
        for(TCHAR Ch:Value->AsString()){if(Ch==0){Error=TEXT("Decoded NUL is refused");return false;}}
    }
    for(int32 I=0;I<12;++I)
    {auto Value=Args->TryGetField(BoolFields[I]);if(Value.IsValid()&&Value->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!Value.IsValid()){Args->SetBoolField(BoolFields[I],BoolDefaults[I]);}}
    for(int32 I=0;I<6;++I)
    {auto Value=Args->TryGetField(LimitFields[I]);FString Decimal=LimitDefaults[I];if(Value.IsValid()&&!IntegralDecimal(Value,Decimal)){Error=TEXT("Exact int64 JSON integer required; bool/string/fraction/overflow refused");return false;}Args->SetStringField(LimitFields[I],Decimal);}
    auto Number=Args->TryGetField(NumberField);FString NumberText=NumberDefault;
    if(Number.IsValid())
    {double Value=0;if(Number->Type!=EJson::Number||!Number->TryGetNumber(Value)||!FMath::IsFinite(Value)||!Number->TryGetString(NumberText)||NumberText.Len()>MaxCarrierChars){Error=TEXT("tick_delta_time requires a finite JSON number representable as double");return false;}}
    Args->SetStringField(NumberField,NumberText);
    for(const TCHAR* Key:NullableFields)
    {
        auto Value=Args->TryGetField(Key);if(!Value.IsValid()){Value=MakeShared<FJsonValueNull>();}
        const bool IsProperties=FCString::Strcmp(Key,TEXT("properties"))==0,IsCases=FCString::Strcmp(Key,TEXT("cases"))==0,IsCurves=FCString::Strcmp(Key,TEXT("curve_names"))==0;
        if(Value->Type!=EJson::Null&&((IsProperties&&Value->Type!=EJson::Object)||((IsCases||IsCurves)&&Value->Type!=EJson::Array)))
        {Error=TEXT("Public cases/properties/curve_names require nullable array/object/array respectively");return false;}
        if((IsCases||IsCurves)&&Value->Type==EJson::Array){for(const auto& Row:Value->AsArray()){if(!Row.IsValid()||(IsCases&&Row->Type!=EJson::Object)||(IsCurves&&Row->Type!=EJson::String)){Error=TEXT("Formal Named cases require object rows and curves require string rows; Native owns row content");return false;}}}
        struct FValue{TSharedPtr<FJsonValue> V;int32 Depth;};TArray<FValue> Work{{Value,IsProperties?1:0}};int32 Slots=IsProperties?1:0;
        while(!Work.IsEmpty())
        {
            auto Entry=Work.Pop(EAllowShrinking::No);if(!Entry.V.IsValid()||++Slots>65536){Error=TEXT("Native JSON value-slot bound exceeded");return false;}
            if(Entry.V->Type==EJson::Number){double V=0;if(!Entry.V->TryGetNumber(V)||!FMath::IsFinite(V)){Error=TEXT("Any JSON numbers must be finite double values");return false;}}
            if(Entry.V->Type==EJson::String){for(TCHAR Ch:Entry.V->AsString()){if(Ch==0){Error=TEXT("Decoded Native JSON text cannot contain NUL");return false;}}}
            if(Entry.V->Type==EJson::Array||Entry.V->Type==EJson::Object)
            {
                if(Entry.Depth+1>64){Error=TEXT("Native JSON carrier depth64 exceeded");return false;}
                if(Entry.V->Type==EJson::Array){for(const auto& Child:Entry.V->AsArray()){Work.Add({Child,Entry.Depth+1});}}
                else{for(const auto& Pair:Entry.V->AsObject()->Values){for(TCHAR Ch:Pair.Key.ToView()){if(Ch==0){Error=TEXT("Decoded Native property keys cannot contain NUL");return false;}}Work.Add({Pair.Value,Entry.Depth+1});}}
            }
        }
        FString Carrier;auto Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Carrier);
        if(!FJsonSerializer::Serialize(Value.ToSharedRef(),TEXT(""),Writer,false)){Error=TEXT("Nullable carrier serialization failed");return false;}
        const bool Closed=Writer->Close();const bool Scalar=Value->Type!=EJson::Array&&Value->Type!=EJson::Object;
        TArray<TSharedPtr<FJsonValue>> Back;
        if((!Closed&&!Scalar)||Carrier.Len()>MaxCarrierChars||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Carrier+TEXT("]")),Back,FJsonSerializer::EFlags::None)||Back.Num()!=1||!Back[0].IsValid()||Back[0]->Type!=Value->Type)
        {Error=TEXT("Nullable JSON carrier readback/type/bound failed");return false;}
        Args->SetStringField(Key,Carrier);
    }
    PackedInput=Json(Args.ToSharedRef());return true;
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!Class||!IsValid(Class)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||!Class->GetPathName().Equals(CanonicalClass,ESearchCase::CaseSensitive)||!Class->IsChildOf(UToolsetDefinition::StaticClass()))
    {Error=TEXT("Exact live canonical runtime response sampler class required");return false;}
    UFunction* Function=Class->FindFunctionByName(FName(AnimStateRuntimeResponseMethod));
    if(!Function||Function->GetOuter()!=Class||!Function->GetName().Equals(AnimStateRuntimeResponseMethod,ESearchCase::CaseSensitive)){Error=TEXT("Exact owned property-probe method required");return false;}
    auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable method required");return false;}
    const TCHAR* Order[]={TEXT("actor_label"), TEXT("actor_name"), TEXT("actor_path"), TEXT("component_name"), TEXT("cases"), TEXT("properties"), TEXT("name"), TEXT("tick_count"), TEXT("tick_delta_time"), TEXT("refresh_bone_transforms"), TEXT("restore_after_case"), TEXT("include_baseline"), TEXT("state_machine_name"), TEXT("include_states"), TEXT("include_state_weights"), TEXT("include_relevant_anim_times"), TEXT("include_transition_progress"), TEXT("include_inactive_transition_progress"), TEXT("include_montages"), TEXT("include_curves"), TEXT("curve_names"), TEXT("max_state_machines"), TEXT("max_state_machine_instances_to_probe"), TEXT("max_states_per_machine"), TEXT("max_transitions_per_machine"), TEXT("max_curves"), TEXT("prefer_pie_world"), TEXT("require_pie_world")};const TSet<int32> BooleanIndices{9,10,11,13,14,15,16,17,18,19,26,27};int32 Index=0;
    for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It)
    {
        if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}
        if(Index>=28||!It->GetName().Equals(Order[Index],ESearchCase::CaseSensitive)||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||(BooleanIndices.Contains(Index)?!CastField<FBoolProperty>(*It):!CastField<FStrProperty>(*It)))
        {Error=TEXT("Physical28 must remain FString16/bool12; Python float/int reflection is not used for Native double/int64");return false;}++Index;
    }
    if(Index!=28){Error=TEXT("Physical28 arity required");return false;}
    for(int32 I=0;I<6;++I){const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"),StringFields[I]));const FString Expected=StringDefaults[I];if(Expected.IsEmpty()?(Function->HasMetaData(DefaultKey)&&!Function->GetMetaData(DefaultKey).IsEmpty()):(!Function->HasMetaData(DefaultKey)||Function->GetMetaData(DefaultKey)!=Expected)){Error=TEXT("Original ordinary string6 defaults (empty5/name case_0) differ");return false;}}
    for(const TCHAR* Key:NullableFields){const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"),Key));if(!Function->HasMetaData(DefaultKey)||Function->GetMetaData(DefaultKey)!=TEXT("null")){Error=TEXT("Nullable carriers require null text defaults");return false;}}
    for(int32 I=0;I<6;++I){const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"),LimitFields[I]));if(!Function->HasMetaData(DefaultKey)||Function->GetMetaData(DefaultKey)!=LimitDefaults[I]){Error=TEXT("Private int64 decimal6 defaults differ");return false;}}
    const FName NumberKey(TEXT("CPP_Default_tick_delta_time"));if(!Function->HasMetaData(NumberKey)||Function->GetMetaData(NumberKey)!=NumberDefault){Error=TEXT("Private double text default must preserve original1/30");return false;}
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

bool AnimStateRuntimeResponseSchema(const FString& Reflected,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reflected),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||Name!=CanonicalName||!Root->TryGetArrayField(TEXT("tools"),Rows)||!Rows||Rows->Num()!=1)
    {Error=TEXT("Exactly one canonical runtime response sampler Toolset required");return false;}
    const TSharedPtr<FJsonObject>* Tool=nullptr;const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Inputs=nullptr;const TSharedPtr<FJsonObject>* Output=nullptr;
    const TSharedPtr<FJsonObject>* Outer=nullptr;const TSharedPtr<FJsonObject>* Result=nullptr;const TSharedPtr<FJsonObject>* Fields=nullptr;
    const TSet<FString> ResultKeys{TEXT("success"),TEXT("operation_id"),TEXT("stage"),TEXT("error_code"),TEXT("error"),TEXT("changes_json"),TEXT("compile_json"),TEXT("save_json"),TEXT("validation_json"),TEXT("retry_json"),TEXT("data_json")};
    if(!(*Rows)[0].IsValid()||!(*Rows)[0]->TryGetObject(Tool)||!Tool||!Tool->IsValid()||!(*Tool)->TryGetStringField(TEXT("name"),Name)||Name!=FString(CanonicalName)+TEXT(".")+AnimStateRuntimeResponseMethod
        ||!(*Tool)->TryGetObjectField(TEXT("inputSchema"),Input)||!Type(*Input,TEXT("object"))||!(*Input)->TryGetObjectField(TEXT("properties"),Inputs)||(*Inputs)->Values.Num()!=28||!Required(*Input,{TEXT("actor_label"), TEXT("actor_name"), TEXT("actor_path"), TEXT("component_name"), TEXT("state_machine_name")}))
    {Error=TEXT("Exact physical28 fields and required ordinary empty string5 required");return false;}
    for(int32 I=0;I<6;++I)
    {const TCHAR* Key=StringFields[I];const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(Key,Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical ordinary string6 required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));const FString Expected=StringDefaults[I];if(Expected.IsEmpty()?Default.IsValid():(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=Expected)){Error=TEXT("SDK empty5 defaults must be absent; name must keep case_0");return false;}(*Field)->SetStringField(TEXT("default"),Expected);}
    for(const TCHAR* Key:NullableFields)
    {
        const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(Key,Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical nullable3 JSON FString required");return false;}
        auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("null")){Error=TEXT("Physical nullable3 defaults must be null text");return false;}
        (*Field)->SetField(TEXT("default"),MakeShared<FJsonValueNull>());
        const bool IsProperties=FCString::Strcmp(Key,TEXT("properties"))==0,IsCases=FCString::Strcmp(Key,TEXT("cases"))==0;
        (*Field)->SetArrayField(TEXT("type"),{MakeShared<FJsonValueString>(IsProperties?TEXT("object"):TEXT("array")),MakeShared<FJsonValueString>(TEXT("null"))});
        auto Item=MakeShared<FJsonObject>();if(IsProperties){(*Field)->SetObjectField(TEXT("additionalProperties"),Item);}else{Item->SetStringField(TEXT("type"),IsCases?TEXT("object"):TEXT("string"));(*Field)->SetObjectField(TEXT("items"),Item);}
    }
    for(int32 I=0;I<12;++I)
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(BoolFields[I],Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Original boolean12 schema required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=BoolDefaults[I]){Error=TEXT("Original boolean12 defaults differ");return false;}}
    for(int32 I=0;I<6;++I)
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(LimitFields[I],Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical integer decimal6 strings required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=LimitDefaults[I]){Error=TEXT("Physical integer decimal6 defaults differ");return false;}(*Field)->SetStringField(TEXT("type"),TEXT("integer"));(*Field)->SetNumberField(TEXT("default"),LimitDefaultValues[I]);}
    const TSharedPtr<FJsonObject>* Number=nullptr;
    if(!(*Inputs)->TryGetObjectField(NumberField,Number)||!Type(*Number,TEXT("string"))){Error=TEXT("Physical double decimal text string required; Python float reflection must not reduce precision");return false;}
    auto NumberValue=(*Number)->TryGetField(TEXT("default"));if(!NumberValue.IsValid()||NumberValue->Type!=EJson::String||NumberValue->AsString()!=NumberDefault){Error=TEXT("Physical double text1 default differs");return false;}
    (*Number)->SetStringField(TEXT("type"),TEXT("number"));(*Number)->SetNumberField(TEXT("default"),1.0/30.0);(*Input)->SetArrayField(TEXT("required"),{});
    if (!(*Tool)->TryGetObjectField(TEXT("outputSchema"), Output) || !Type(*Output, TEXT("object")) || !Required(*Output, {TEXT("returnValue")})
        || !(*Output)->TryGetObjectField(TEXT("properties"), Outer) || (*Outer)->Values.Num() != 1 || !(*Outer)->TryGetObjectField(TEXT("returnValue"), Result)
        || !Type(*Result, TEXT("object")) || !Required(*Result, ResultKeys) || !(*Result)->TryGetObjectField(TEXT("properties"), Fields) || (*Fields)->Values.Num() != 11)
    { Error = TEXT("Original synchronous standard11 output envelope required"); return false; }
    for (const auto& Key : ResultKeys)
    { const TSharedPtr<FJsonObject>* Field = nullptr; if (!(*Fields)->TryGetObjectField(Key, Field) || !Type(*Field, Key == TEXT("success") ? TEXT("boolean") : TEXT("string")))
      { Error = TEXT("Original standard11 field types required"); return false; } }
    Logical = Json(Root.ToSharedRef()); return true;
}

class FAnimStateRuntimeResponseToolset final : public FToolset
{
public:
    explicit FAnimStateRuntimeResponseToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("anim_state_machine_runtime_response_raw_guard_unavailable"), TEXT("A live active game-thread AnimStateRuntimeResponse guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("anim_state_machine_runtime_response_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("anim_state_machine_runtime_response_raw_guard_definition_changed"), Error); }
        if (!Method.Equals(AnimStateRuntimeResponseMethod, ESearchCase::CaseSensitive))
        { return Reject(TEXT("anim_state_machine_runtime_response_method_refused"), TEXT("Only the exact original AnimInstance runtime response method is guarded")); }
        FString PackedInput;
        if (!RawTransform(Input, PackedInput, Error))
        { return Reject(TEXT("anim_state_machine_runtime_response_raw_arguments_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->AnimStateRuntimeResponseDelegation.Store(false);
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
            State->AnimStateRuntimeResponseDelegation.Store(false); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("AnimStateRuntimeResponse ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("anim_state_machine_runtime_response_raw_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named method delegation may ask Verify for its exact admission marker.
        State->AnimStateRuntimeResponseDelegation.Store(Method.Equals(AnimStateRuntimeResponseMethod, ESearchCase::CaseSensitive));
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, PackedInput).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->AnimStateRuntimeResponseDelegation.Store(false); CallState->InFlight.Store(0);
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
    Report->SetStringField(TEXT("guard_scope"), TEXT("canonical runtime response sampler logical28: ordinary string6 (empty5/name case_0), nullable array-object/object/array-string3, exact bool12/int64integer6 and finite number1 with private JSON3/integer-decimal6/double-text1 carriers; PIE/SIE target access allowed"));
    for (const TCHAR* Key : { TEXT("active"), TEXT("raw_guard_installed"), TEXT("default_verified"), TEXT("reflected_carrier_schema_verified"),
        TEXT("logical_schema_verified"), TEXT("ordinary_string6_schema_verified"), TEXT("nullable3_schema_verified"), TEXT("boolean12_schema_verified"), TEXT("integer6_schema_verified"), TEXT("empty_string_defaults_verified"), TEXT("output11_schema_preserved"), TEXT("runtime_response28_schema_verified"), TEXT("number1_schema_verified") })
    { Report->SetBoolField(Key, false); }
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured AnimStateRuntimeResponse guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured AnimStateRuntimeResponse wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!AnimStateRuntimeResponseSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original AnimStateRuntimeResponse schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("AnimStateRuntimeResponse wrapper owner/class changed during reflected schema callbacks"); } return false; }
    Report->SetBoolField(TEXT("reflected_carrier_schema_verified"), true); Report->SetBoolField(TEXT("logical_schema_verified"), true);
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("raw_guard_installed"), true);
    Report->SetBoolField(TEXT("ordinary_string6_schema_verified"), true); Report->SetBoolField(TEXT("nullable3_schema_verified"), true); Report->SetBoolField(TEXT("integer6_schema_verified"), true); Report->SetBoolField(TEXT("empty_string_defaults_verified"), true); Report->SetBoolField(TEXT("output11_schema_preserved"), true);
    Report->SetBoolField(TEXT("boolean12_schema_verified"), true);
    Report->SetBoolField(TEXT("runtime_response28_schema_verified"), true); Report->SetBoolField(TEXT("number1_schema_verified"), true);
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("AnimStateRuntimeResponse guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered AnimStateRuntimeResponse wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered AnimStateRuntimeResponse logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned AnimStateRuntimeResponse guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallAnimStateRuntimeResponseToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("AnimStateRuntimeResponse guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained AnimStateRuntimeResponse owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!AnimStateRuntimeResponseSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FAnimStateRuntimeResponseToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original AnimStateRuntimeResponse handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("AnimStateRuntimeResponse raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyAnimStateRuntimeResponseToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("AnimStateRuntimeResponse guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const bool Admitted = Owned->InFlight.Load() == 1 && Owned->AnimStateRuntimeResponseDelegation.Load() && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetStringField(TEXT("delegated_method"), Admitted ? AnimStateRuntimeResponseMethod : TEXT(""));
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named AnimInstance runtime inspector delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed AnimStateRuntimeResponse raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original AnimStateRuntimeResponse registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveAnimStateRuntimeResponseToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("AnimStateRuntimeResponse guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained AnimStateRuntimeResponse class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownAnimStateRuntimeResponseToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
