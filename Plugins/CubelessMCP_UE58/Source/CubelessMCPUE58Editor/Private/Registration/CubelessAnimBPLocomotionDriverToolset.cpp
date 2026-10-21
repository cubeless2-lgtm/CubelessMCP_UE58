#include "Registration/CubelessAnimBPLocomotionDriverToolset.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.anim_bp_locomotion_driver_toolset.CubelessAnimBPLocomotionDriverTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/anim_bp_locomotion_driver_toolset_PY.CubelessAnimBPLocomotionDriverTools");
constexpr const TCHAR* AnimBPLocomotionDriverMethod = TEXT("ensure_anim_bp_event_graph_locomotion_driver");
constexpr int32 MaxInputChars=8*1024*1024,MaxStringChars=1024*1024,MaxCarrierChars=1024*1024;

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
    TAtomic<bool> AnimBPLocomotionDriverDelegation { false };
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
    R->SetStringField(TEXT("stage"), TEXT("anim_bp_locomotion_driver_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("anim_bp_locomotion_driver_raw_not_dispatched"));
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

bool RawTransform(const FString& Input,FString& PackedInput,FString& Error)
{
    if(Input.Len()>MaxInputChars){Error=TEXT("AnimBP locomotion raw input exceeds 8Mi UTF16");return false;}
    int32 Depth=0;bool Quoted=false,Escaped=false;
    for(TCHAR Ch:Input){if(Ch==0){Error=TEXT("Raw NUL is refused");return false;}if(Quoted){if(Escaped){Escaped=false;}else if(Ch==TCHAR('\\')){Escaped=true;}else if(Ch==TCHAR('"')){Quoted=false;}continue;}if(Ch==TCHAR('"')){Quoted=true;}else if(Ch==TCHAR('[')||Ch==TCHAR('{')){if(++Depth>65){Error=TEXT("Raw JSON exceeds outer wrapper plus carrier depth64");return false;}}else if(Ch==TCHAR(']')||Ch==TCHAR('}')){if(--Depth<0){Error=TEXT("Malformed raw JSON");return false;}}}
    if(Depth||Quoted){Error=TEXT("Malformed raw JSON");return false;}
    TSharedPtr<FJsonObject> Args;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid()){Error=TEXT("A raw JSON object is required");return false;}
    const TSet<FString> Allowed{TEXT("source_anim_bp_path"),TEXT("target_anim_bp_path"),TEXT("target_root"),TEXT("variant_name"),TEXT("speed_variable"),TEXT("create_variable_if_missing"),TEXT("include_direction"),TEXT("direction_variable"),TEXT("create_direction_variable_if_missing"),TEXT("include_is_in_air"),TEXT("is_in_air_variable"),TEXT("create_is_in_air_variable_if_missing"),TEXT("include_acceleration"),TEXT("acceleration_variable"),TEXT("create_acceleration_variable_if_missing"),TEXT("include_is_accelerating"),TEXT("is_accelerating_variable"),TEXT("create_is_accelerating_variable_if_missing"),TEXT("is_accelerating_threshold"),TEXT("guard_pawn_owner"),TEXT("replace_existing_links"),TEXT("allow_existing_target_reuse"),TEXT("overwrite_existing"),TEXT("compile"),TEXT("save_asset"),TEXT("dry_run"),TEXT("validate_only")};
    for(const auto& Pair:Args->Values){if(!Allowed.Contains(FString(Pair.Key.ToView()))){Error=TEXT("Unexpected public27 input; command aliases remain Batch-only");return false;}}
    {const TCHAR* Key=TEXT("source_anim_bp_path");auto V=Args->TryGetField(Key);if(!V.IsValid()){Error=TEXT("Required source input is missing");return false;}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("target_anim_bp_path");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("target_root");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("/Game/_MCP_Sample/AnimStudy"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("variant_name");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("LocomotionEventGraph"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("speed_variable");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("Speed"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("direction_variable");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("Direction"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("is_in_air_variable");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("IsInAir"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("acceleration_variable");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("Acceleration"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("is_accelerating_variable");auto V=Args->TryGetField(Key);if(!V.IsValid()){Args->SetStringField(Key,TEXT("IsAccelerating"));V=Args->TryGetField(Key);}if(V->Type!=EJson::String||V->AsString().Len()>MaxStringChars){Error=TEXT("Exact bounded string required; null is not an omitted value");return false;}for(TCHAR Ch:V->AsString()){if(Ch==0){Error=TEXT("Decoded string NUL is refused");return false;}}}
    {const TCHAR* Key=TEXT("create_variable_if_missing");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("include_direction");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("create_direction_variable_if_missing");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("include_is_in_air");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("create_is_in_air_variable_if_missing");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("include_acceleration");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("create_acceleration_variable_if_missing");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("include_is_accelerating");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("create_is_accelerating_variable_if_missing");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("guard_pawn_owner");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("replace_existing_links");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("allow_existing_target_reuse");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("overwrite_existing");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("compile");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("save_asset");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,true);}}
    {const TCHAR* Key=TEXT("dry_run");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}
    {const TCHAR* Key=TEXT("validate_only");auto V=Args->TryGetField(Key);if(V.IsValid()&&V->Type!=EJson::Boolean){Error=TEXT("Exact public JSON boolean required");return false;}if(!V.IsValid()){Args->SetBoolField(Key,false);}}

    {const TCHAR* Key=TEXT("is_accelerating_threshold");auto V=Args->TryGetField(Key);FString Decimal=TEXT("3.0");if(V.IsValid()){double Number=0;if(V->Type!=EJson::Number||!V->TryGetNumber(Number)||!FMath::IsFinite(Number)||!V->TryGetString(Decimal)||Decimal.Len()>MaxCarrierChars){Error=TEXT("Finite double JSON number required");return false;}}Args->SetStringField(Key,Decimal);}
    PackedInput=Json(Args.ToSharedRef());return true;
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!Class||!IsValid(Class)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||!Class->GetPathName().Equals(CanonicalClass,ESearchCase::CaseSensitive)||!Class->IsChildOf(UToolsetDefinition::StaticClass())){Error=TEXT("Exact live canonical AnimBP locomotion class required");return false;}
    UFunction* Function=Class->FindFunctionByName(FName(AnimBPLocomotionDriverMethod));
    if(!Function||Function->GetOuter()!=Class||!Function->GetName().Equals(AnimBPLocomotionDriverMethod,ESearchCase::CaseSensitive)){Error=TEXT("Exact owned AnimBP locomotion method required");return false;}
    auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable method required");return false;}
    const TCHAR* Order[]={TEXT("source_anim_bp_path"),TEXT("target_anim_bp_path"),TEXT("target_root"),TEXT("variant_name"),TEXT("speed_variable"),TEXT("create_variable_if_missing"),TEXT("include_direction"),TEXT("direction_variable"),TEXT("create_direction_variable_if_missing"),TEXT("include_is_in_air"),TEXT("is_in_air_variable"),TEXT("create_is_in_air_variable_if_missing"),TEXT("include_acceleration"),TEXT("acceleration_variable"),TEXT("create_acceleration_variable_if_missing"),TEXT("include_is_accelerating"),TEXT("is_accelerating_variable"),TEXT("create_is_accelerating_variable_if_missing"),TEXT("is_accelerating_threshold"),TEXT("guard_pawn_owner"),TEXT("replace_existing_links"),TEXT("allow_existing_target_reuse"),TEXT("overwrite_existing"),TEXT("compile"),TEXT("save_asset"),TEXT("dry_run"),TEXT("validate_only")};const TSet<int32> BooleanIndices{5,6,8,9,11,12,14,15,17,19,20,21,22,23,24,25,26};int32 Index=0;
    for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}if(Index>=27||!It->GetName().Equals(Order[Index],ESearchCase::CaseSensitive)||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||(BooleanIndices.Contains(Index)?!CastField<FBoolProperty>(*It):!CastField<FStrProperty>(*It))){Error=TEXT("Exact physical27 field order and FString/bool types required");return false;}++Index;}
    if(Index!=27){Error=TEXT("Exact physical arity required");return false;}
    {const FName Key(TEXT("CPP_Default_target_anim_bp_path"));if(Function->HasMetaData(Key)&&!Function->GetMetaData(Key).IsEmpty()){Error=TEXT("Original default or private carrier default differs: target_anim_bp_path");return false;}}
    {const FName Key(TEXT("CPP_Default_target_root"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("/Game/_MCP_Sample/AnimStudy")){Error=TEXT("Original default or private carrier default differs: target_root");return false;}}
    {const FName Key(TEXT("CPP_Default_variant_name"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("LocomotionEventGraph")){Error=TEXT("Original default or private carrier default differs: variant_name");return false;}}
    {const FName Key(TEXT("CPP_Default_speed_variable"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("Speed")){Error=TEXT("Original default or private carrier default differs: speed_variable");return false;}}
    {const FName Key(TEXT("CPP_Default_direction_variable"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("Direction")){Error=TEXT("Original default or private carrier default differs: direction_variable");return false;}}
    {const FName Key(TEXT("CPP_Default_is_in_air_variable"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("IsInAir")){Error=TEXT("Original default or private carrier default differs: is_in_air_variable");return false;}}
    {const FName Key(TEXT("CPP_Default_acceleration_variable"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("Acceleration")){Error=TEXT("Original default or private carrier default differs: acceleration_variable");return false;}}
    {const FName Key(TEXT("CPP_Default_is_accelerating_variable"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("IsAccelerating")){Error=TEXT("Original default or private carrier default differs: is_accelerating_variable");return false;}}
    {const FName Key(TEXT("CPP_Default_is_accelerating_threshold"));if(!Function->HasMetaData(Key)||Function->GetMetaData(Key)!=TEXT("3.0")){Error=TEXT("Original default or private carrier default differs: is_accelerating_threshold");return false;}}
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

bool AnimBPLocomotionDriverSchema(const FString& Reflected,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reflected),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||Name!=CanonicalName||!Root->TryGetArrayField(TEXT("tools"),Rows)||!Rows||Rows->Num()!=1){Error=TEXT("Exactly one canonical AnimBP locomotion Toolset required");return false;}
    const TSharedPtr<FJsonObject>* Tool=nullptr;const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Inputs=nullptr;const TSharedPtr<FJsonObject>* Output=nullptr;const TSharedPtr<FJsonObject>* Outer=nullptr;const TSharedPtr<FJsonObject>* Result=nullptr;const TSharedPtr<FJsonObject>* Fields=nullptr;
    const TSet<FString> ResultKeys{TEXT("success"),TEXT("operation_id"),TEXT("stage"),TEXT("error_code"),TEXT("error"),TEXT("changes_json"),TEXT("compile_json"),TEXT("save_json"),TEXT("validation_json"),TEXT("retry_json"),TEXT("data_json")};
    if(!(*Rows)[0].IsValid()||!(*Rows)[0]->TryGetObject(Tool)||!Tool||!Tool->IsValid()||!(*Tool)->TryGetStringField(TEXT("name"),Name)||Name!=FString(CanonicalName)+TEXT(".")+AnimBPLocomotionDriverMethod||!(*Tool)->TryGetObjectField(TEXT("inputSchema"),Input)||!Type(*Input,TEXT("object"))||!(*Input)->TryGetObjectField(TEXT("properties"),Inputs)||(*Inputs)->Values.Num()!=27||!Required(*Input,{TEXT("source_anim_bp_path"),TEXT("target_anim_bp_path")})){Error=TEXT("Exact physical27 fields and SDK-required source/empty fields required");return false;}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("source_anim_bp_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: source_anim_bp_path");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical source/empty default must be absent: source_anim_bp_path");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("target_anim_bp_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: target_anim_bp_path");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Physical source/empty default must be absent: target_anim_bp_path");return false;}(*Field)->SetStringField(TEXT("default"),TEXT(""));}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("target_root"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: target_root");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("/Game/_MCP_Sample/AnimStudy")){Error=TEXT("Physical carrier/default differs: target_root");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("variant_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: variant_name");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("LocomotionEventGraph")){Error=TEXT("Physical carrier/default differs: variant_name");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("speed_variable"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: speed_variable");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("Speed")){Error=TEXT("Physical carrier/default differs: speed_variable");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("create_variable_if_missing"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: create_variable_if_missing");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: create_variable_if_missing");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("include_direction"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: include_direction");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Physical carrier/default differs: include_direction");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("direction_variable"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: direction_variable");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("Direction")){Error=TEXT("Physical carrier/default differs: direction_variable");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("create_direction_variable_if_missing"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: create_direction_variable_if_missing");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: create_direction_variable_if_missing");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("include_is_in_air"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: include_is_in_air");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Physical carrier/default differs: include_is_in_air");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("is_in_air_variable"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: is_in_air_variable");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("IsInAir")){Error=TEXT("Physical carrier/default differs: is_in_air_variable");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("create_is_in_air_variable_if_missing"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: create_is_in_air_variable_if_missing");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: create_is_in_air_variable_if_missing");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("include_acceleration"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: include_acceleration");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Physical carrier/default differs: include_acceleration");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("acceleration_variable"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: acceleration_variable");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("Acceleration")){Error=TEXT("Physical carrier/default differs: acceleration_variable");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("create_acceleration_variable_if_missing"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: create_acceleration_variable_if_missing");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: create_acceleration_variable_if_missing");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("include_is_accelerating"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: include_is_accelerating");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Physical carrier/default differs: include_is_accelerating");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("is_accelerating_variable"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: is_accelerating_variable");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("IsAccelerating")){Error=TEXT("Physical carrier/default differs: is_accelerating_variable");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("create_is_accelerating_variable_if_missing"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: create_is_accelerating_variable_if_missing");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: create_is_accelerating_variable_if_missing");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("is_accelerating_threshold"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical field type differs: is_accelerating_threshold");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||Default->AsString()!=TEXT("3.0")){Error=TEXT("Physical carrier/default differs: is_accelerating_threshold");return false;}(*Field)->SetStringField(TEXT("type"),TEXT("number"));(*Field)->SetNumberField(TEXT("default"),3);}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("guard_pawn_owner"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: guard_pawn_owner");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: guard_pawn_owner");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("replace_existing_links"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: replace_existing_links");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Physical carrier/default differs: replace_existing_links");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("allow_existing_target_reuse"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: allow_existing_target_reuse");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: allow_existing_target_reuse");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("overwrite_existing"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: overwrite_existing");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Physical carrier/default differs: overwrite_existing");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("compile"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: compile");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: compile");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("save_asset"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: save_asset");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=true){Error=TEXT("Physical carrier/default differs: save_asset");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("dry_run"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: dry_run");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Physical carrier/default differs: dry_run");return false;}}
    {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Inputs)->TryGetObjectField(TEXT("validate_only"),Field)||!Type(*Field,TEXT("boolean"))){Error=TEXT("Physical field type differs: validate_only");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::Boolean||Default->AsBool()!=false){Error=TEXT("Physical carrier/default differs: validate_only");return false;}}

    (*Input)->SetArrayField(TEXT("required"),{MakeShared<FJsonValueString>(TEXT("source_anim_bp_path"))});(*Input)->SetBoolField(TEXT("additionalProperties"),false);
    if (!(*Tool)->TryGetObjectField(TEXT("outputSchema"), Output) || !Type(*Output, TEXT("object")) || !Required(*Output, {TEXT("returnValue")})
        || !(*Output)->TryGetObjectField(TEXT("properties"), Outer) || (*Outer)->Values.Num() != 1 || !(*Outer)->TryGetObjectField(TEXT("returnValue"), Result)
        || !Type(*Result, TEXT("object")) || !Required(*Result, ResultKeys) || !(*Result)->TryGetObjectField(TEXT("properties"), Fields) || (*Fields)->Values.Num() != 11)
    { Error = TEXT("Original synchronous standard11 output envelope required"); return false; }
    for (const auto& Key : ResultKeys)
    { const TSharedPtr<FJsonObject>* Field = nullptr; if (!(*Fields)->TryGetObjectField(Key, Field) || !Type(*Field, Key == TEXT("success") ? TEXT("boolean") : TEXT("string")))
      { Error = TEXT("Original standard11 field types required"); return false; } }
    Logical = Json(Root.ToSharedRef()); return true;
}


class FAnimBPLocomotionDriverToolset final : public FToolset
{
public:
    explicit FAnimBPLocomotionDriverToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("anim_bp_locomotion_driver_raw_guard_unavailable"), TEXT("A live active game-thread AnimBPLocomotionDriver guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("anim_bp_locomotion_driver_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("anim_bp_locomotion_driver_raw_guard_definition_changed"), Error); }
        if (!Method.Equals(AnimBPLocomotionDriverMethod, ESearchCase::CaseSensitive))
        { return Reject(TEXT("anim_bp_locomotion_driver_method_refused"), TEXT("Only the exact original anim_bp_locomotion_driver adapter method is guarded")); }
        FString PackedInput;
        if (!RawTransform(Input, PackedInput, Error))
        { return Reject(TEXT("anim_bp_locomotion_driver_raw_arguments_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->AnimBPLocomotionDriverDelegation.Store(false);
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
            State->AnimBPLocomotionDriverDelegation.Store(false); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("AnimBPLocomotionDriver ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("anim_bp_locomotion_driver_raw_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named method delegation may ask Verify for its exact admission marker.
        State->AnimBPLocomotionDriverDelegation.Store(Method.Equals(AnimBPLocomotionDriverMethod, ESearchCase::CaseSensitive));
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, PackedInput).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->AnimBPLocomotionDriverDelegation.Store(false); CallState->InFlight.Store(0);
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
    Report->SetStringField(TEXT("guard_scope"), TEXT("Create or reuse a sample AnimBP locomotion EventGraph driver. Preserve public27 defaults, ordered Speed/optional Direction, IsInAir, Acceleration(float) and IsAccelerating variables; guarded pawn/movement exec and data wiring, reuse/conflicts, requested compiler/save and topology readback in one Native operation. Named optional create flags remain True by default; only legacy Batch key omission inherits the global create flag before defaults. Native owns source-target scope, alias normalization, partial duplicate/delete/variable/node/link/default changes, compiler/save and unknown SDK effects. Structural APIs can trigger implicit skeleton compilation even when explicit compile is False. No atomic rollback or automatic retry is provided. Official Named HTTP numbers may already be rounded before this adapter."));
    for (const TCHAR* Key : { TEXT("active"), TEXT("raw_guard_installed"), TEXT("default_verified"), TEXT("reflected_carrier_schema_verified"), TEXT("logical_schema_verified"), TEXT("output11_schema_preserved"), TEXT("ordinary_string9_schema_verified"), TEXT("boolean17_schema_verified"), TEXT("number1_schema_verified"), TEXT("locomotion_driver27_schema_verified"), TEXT("required_source1_verified"), TEXT("empty_string_defaults_verified") })
    { Report->SetBoolField(Key, false); }
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured AnimBPLocomotionDriver guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured AnimBPLocomotionDriver wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!AnimBPLocomotionDriverSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original AnimBPLocomotionDriver schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("AnimBPLocomotionDriver wrapper owner/class changed during reflected schema callbacks"); } return false; }
    Report->SetBoolField(TEXT("reflected_carrier_schema_verified"), true); Report->SetBoolField(TEXT("logical_schema_verified"), true);
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("raw_guard_installed"), true);
    for (const TCHAR* Key : { TEXT("output11_schema_preserved"), TEXT("ordinary_string9_schema_verified"), TEXT("boolean17_schema_verified"), TEXT("number1_schema_verified"), TEXT("locomotion_driver27_schema_verified"), TEXT("required_source1_verified"), TEXT("empty_string_defaults_verified") })
    { Report->SetBoolField(Key, true); }
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("AnimBPLocomotionDriver guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered AnimBPLocomotionDriver wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered AnimBPLocomotionDriver logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned AnimBPLocomotionDriver guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallAnimBPLocomotionDriverToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("AnimBPLocomotionDriver guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained AnimBPLocomotionDriver owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!AnimBPLocomotionDriverSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FAnimBPLocomotionDriverToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original AnimBPLocomotionDriver handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("AnimBPLocomotionDriver raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyAnimBPLocomotionDriverToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("AnimBPLocomotionDriver guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const bool Admitted = Owned->InFlight.Load() == 1 && Owned->AnimBPLocomotionDriverDelegation.Load() && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetStringField(TEXT("delegated_method"), Admitted ? AnimBPLocomotionDriverMethod : TEXT(""));
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named anim_bp_locomotion_driver adapter delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed AnimBPLocomotionDriver raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original AnimBPLocomotionDriver registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveAnimBPLocomotionDriverToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("AnimBPLocomotionDriver guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained AnimBPLocomotionDriver class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownAnimBPLocomotionDriverToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
