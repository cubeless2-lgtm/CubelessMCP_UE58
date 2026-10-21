#include "Registration/CubelessNiagaraRuntimeInspectionToolset.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.niagara_runtime_inspection_toolset.CubelessNiagaraRuntimeInspectionTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/niagara_runtime_inspection_toolset_PY.CubelessNiagaraRuntimeInspectionTools");
constexpr const TCHAR* Methods[]={TEXT("inspect_niagara_component_runtime_state"),TEXT("inspect_niagara_component_parameter_store"),TEXT("inspect_niagara_component_grid2d_runtime"),TEXT("sample_render_target2d_region_stats")};
constexpr const TCHAR* Fields0[]={TEXT("actor_name"),TEXT("actor_label"),TEXT("component_name"),TEXT("system_path"),TEXT("parameter_name_contains"),TEXT("max_components"),TEXT("max_parameters"),TEXT("include_parameter_values"),TEXT("include_raw_bytes"),TEXT("max_raw_bytes"),TEXT("include_asset_exposed_parameters"),TEXT("include_instance_parameters"),TEXT("include_parameter_object_properties"),TEXT("include_emitter_runtime"),TEXT("max_emitter_instances"),TEXT("max_object_properties"),TEXT("max_array_elements"),TEXT("max_property_value_chars")};
constexpr const TCHAR* Fields1[]={TEXT("actor_name"),TEXT("actor_label"),TEXT("component_name"),TEXT("system_path"),TEXT("parameter_name_contains"),TEXT("max_components"),TEXT("max_parameters"),TEXT("include_raw_bytes"),TEXT("max_raw_bytes")};
constexpr const TCHAR* Fields2[]={TEXT("actor_name"),TEXT("actor_label"),TEXT("component_name"),TEXT("system_path"),TEXT("max_components"),TEXT("max_grid_interfaces"),TEXT("include_internal")};
constexpr const TCHAR* Fields3[]={TEXT("render_target_path"),TEXT("x0"),TEXT("x1"),TEXT("y0"),TEXT("y1"),TEXT("channel"),TEXT("threshold"),TEXT("max_first_hits"),TEXT("probe_x"),TEXT("probe_y"),TEXT("probe_radius")};
struct FLayout{const TCHAR* const* Fields;int32 Count;};
constexpr FLayout Layouts[]={{Fields0,18},{Fields1,9},{Fields2,7},{Fields3,11}};
constexpr int32 MaxInputChars=8*1024*1024,MaxCarrierChars=1024*1024,MaxRawDepth=65,MaxValueSlots=262144;
int32 MethodIndex(const FString& Method){for(int32 I=0;I<UE_ARRAY_COUNT(Methods);++I){if(Method.Equals(Methods[I],ESearchCase::CaseSensitive)){return I;}}return INDEX_NONE;}
constexpr const TCHAR* LogicalInputSchemas[]={TEXT("{\"properties\":{\"actor_name\":{\"default\":\"\",\"title\":\"Actor Name\",\"type\":\"string\"},\"actor_label\":{\"default\":\"\",\"title\":\"Actor Label\",\"type\":\"string\"},\"component_name\":{\"default\":\"\",\"title\":\"Component Name\",\"type\":\"string\"},\"system_path\":{\"default\":\"\",\"title\":\"System Path\",\"type\":\"string\"},\"parameter_name_contains\":{\"default\":\"\",\"title\":\"Parameter Name Contains\",\"type\":\"string\"},\"max_components\":{\"default\":32,\"title\":\"Max Components\",\"type\":\"integer\"},\"max_parameters\":{\"default\":256,\"title\":\"Max Parameters\",\"type\":\"integer\"},\"include_parameter_values\":{\"default\":true,\"title\":\"Include Parameter Values\",\"type\":\"boolean\"},\"include_raw_bytes\":{\"default\":false,\"title\":\"Include Raw Bytes\",\"type\":\"boolean\"},\"max_raw_bytes\":{\"default\":64,\"title\":\"Max Raw Bytes\",\"type\":\"integer\"},\"include_asset_exposed_parameters\":{\"default\":false,\"title\":\"Include Asset Exposed Parameters\",\"type\":\"boolean\"},\"include_instance_parameters\":{\"default\":false,\"title\":\"Include Instance Parameters\",\"type\":\"boolean\"},\"include_parameter_object_properties\":{\"default\":false,\"title\":\"Include Parameter Object Properties\",\"type\":\"boolean\"},\"include_emitter_runtime\":{\"default\":true,\"title\":\"Include Emitter Runtime\",\"type\":\"boolean\"},\"max_emitter_instances\":{\"default\":128,\"title\":\"Max Emitter Instances\",\"type\":\"integer\"},\"max_object_properties\":{\"default\":80,\"title\":\"Max Object Properties\",\"type\":\"integer\"},\"max_array_elements\":{\"default\":16,\"title\":\"Max Array Elements\",\"type\":\"integer\"},\"max_property_value_chars\":{\"default\":512,\"title\":\"Max Property Value Chars\",\"type\":\"integer\"}},\"title\":\"inspect_niagara_component_runtime_stateArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"actor_name\":{\"default\":\"\",\"title\":\"Actor Name\",\"type\":\"string\"},\"actor_label\":{\"default\":\"\",\"title\":\"Actor Label\",\"type\":\"string\"},\"component_name\":{\"default\":\"\",\"title\":\"Component Name\",\"type\":\"string\"},\"system_path\":{\"default\":\"\",\"title\":\"System Path\",\"type\":\"string\"},\"parameter_name_contains\":{\"default\":\"\",\"title\":\"Parameter Name Contains\",\"type\":\"string\"},\"max_components\":{\"default\":8,\"title\":\"Max Components\",\"type\":\"integer\"},\"max_parameters\":{\"default\":64,\"title\":\"Max Parameters\",\"type\":\"integer\"},\"include_raw_bytes\":{\"default\":false,\"title\":\"Include Raw Bytes\",\"type\":\"boolean\"},\"max_raw_bytes\":{\"default\":64,\"title\":\"Max Raw Bytes\",\"type\":\"integer\"}},\"title\":\"inspect_niagara_component_parameter_storeArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"actor_name\":{\"default\":\"\",\"title\":\"Actor Name\",\"type\":\"string\"},\"actor_label\":{\"default\":\"\",\"title\":\"Actor Label\",\"type\":\"string\"},\"component_name\":{\"default\":\"\",\"title\":\"Component Name\",\"type\":\"string\"},\"system_path\":{\"default\":\"\",\"title\":\"System Path\",\"type\":\"string\"},\"max_components\":{\"default\":16,\"title\":\"Max Components\",\"type\":\"integer\"},\"max_grid_interfaces\":{\"default\":16,\"title\":\"Max Grid Interfaces\",\"type\":\"integer\"},\"include_internal\":{\"default\":true,\"title\":\"Include Internal\",\"type\":\"boolean\"}},\"title\":\"inspect_niagara_component_grid2d_runtimeArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"render_target_path\":{\"title\":\"Render Target Path\",\"type\":\"string\"},\"x0\":{\"default\":0,\"title\":\"X0\",\"type\":\"integer\"},\"x1\":{\"default\":-1,\"title\":\"X1\",\"type\":\"integer\"},\"y0\":{\"default\":0,\"title\":\"Y0\",\"type\":\"integer\"},\"y1\":{\"default\":-1,\"title\":\"Y1\",\"type\":\"integer\"},\"channel\":{\"default\":\"max\",\"title\":\"Channel\",\"type\":\"string\"},\"threshold\":{\"default\":0.01,\"title\":\"Threshold\",\"type\":\"number\"},\"max_first_hits\":{\"default\":8,\"title\":\"Max First Hits\",\"type\":\"integer\"},\"probe_x\":{\"default\":-1,\"title\":\"Probe X\",\"type\":\"integer\"},\"probe_y\":{\"default\":-1,\"title\":\"Probe Y\",\"type\":\"integer\"},\"probe_radius\":{\"default\":8,\"title\":\"Probe Radius\",\"type\":\"integer\"}},\"required\":[\"render_target_path\"],\"title\":\"sample_render_target2d_region_statsArguments\",\"type\":\"object\"}")};
bool BoundText(const FString& Text,int32 Maximum)
{if(Text.Len()>Maximum){return false;}for(TCHAR C:Text){if(C==0){return false;}}return true;}
// Wrapper parsing and Native carrier parsing have separate bounds; neither permits arbitrary nested payloads.
bool RawDepth(const FString& Text)
{
    int32 Depth=0;bool Quoted=false,Escaped=false;
    for(TCHAR C:Text)
    {
        if(Quoted){if(Escaped){Escaped=false;}else if(C==TEXT('\\')){Escaped=true;}else if(C==TEXT('"')){Quoted=false;}continue;}
        if(C==TEXT('"')){Quoted=true;}
        else if(C==TEXT('[')||C==TEXT('{')){if(++Depth>MaxRawDepth){return false;}}
        else if(C==TEXT(']')||C==TEXT('}')){if(--Depth<0){return false;}}
    }
    return !Quoted&&Depth==0;
}
bool FiniteValues(const TSharedRef<FJsonObject>& Args)
{
    TArray<TSharedPtr<FJsonValue>> Pending;for(const auto& Pair:Args->Values){Pending.Add(Pair.Value);}
    int32 Slots=0;
    while(!Pending.IsEmpty())
    {
        if(++Slots>MaxValueSlots){return false;}const auto Value=Pending.Pop();if(!Value.IsValid()){return false;}
        if(Value->Type==EJson::Number&&!FMath::IsFinite(Value->AsNumber())){return false;}
        if(Value->Type==EJson::Array){Pending.Append(Value->AsArray());}
        else if(Value->Type==EJson::Object){for(const auto& Pair:Value->AsObject()->Values){Pending.Add(Pair.Value);}}
    }
    return true;
}
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
    TAtomic<int32> DelegatedMethodIndex { INDEX_NONE };
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
    R->SetStringField(TEXT("stage"), TEXT("niagara_runtime_inspection_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("niagara_runtime_inspection_raw_not_dispatched"));
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

// Public JSON FString carriers include scalars. Close() requires a completed object/array,
// so use the installed public one-element array overload and remove only its outer brackets.
// StoreNumbersAsStrings + PreferStringRepresentation preserve original numeric lexemes.
bool SerializePublicValue(const TSharedPtr<FJsonValue>& Value,FString& Carrier,FString& Error)
{
    if(!Value.IsValid()){Error=TEXT("Public JSON value is invalid");return false;}
    const TArray<TSharedPtr<FJsonValue>> Envelope{Value};FString EnvelopeJson;
    auto Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&EnvelopeJson);
    if(!FJsonSerializer::Serialize(Envelope,Writer)||EnvelopeJson.Len()<2||EnvelopeJson[0]!=TEXT('[')||EnvelopeJson[EnvelopeJson.Len()-1]!=TEXT(']')){Error=TEXT("Public JSON value carrier serialization failed");return false;}
    Carrier=EnvelopeJson.Mid(1,EnvelopeJson.Len()-2);TArray<TSharedPtr<FJsonValue>> Back;
    if(!BoundText(Carrier,MaxCarrierChars)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(EnvelopeJson),Back,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Back.Num()!=1||!Back[0].IsValid()||Back[0]->Type!=Value->Type){Error=TEXT("Public JSON carrier bound/readback failed");return false;}
    return true;
}

bool RawTransform(int32 Which,const FString& Input,FString& PackedInput,FString& Error)
{
 TSharedPtr<FJsonObject> Args;if(Which<0||Which>=UE_ARRAY_COUNT(Methods)||!BoundText(Input,MaxInputChars)||!RawDepth(Input)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid()||!FiniteValues(Args.ToSharedRef())){Error=TEXT("Niagara runtime inspection requires bounded finite raw object");return false;}
 auto Packed=MakeShared<FJsonObject>();
 if(Which==0){
  {auto Value=Args->TryGetField(TEXT("actor_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("actor_name"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("actor_label"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("actor_label"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("component_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("component_name"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("system_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("system_path"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("parameter_name_contains"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("parameter_name_contains"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_components"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[32]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_components"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_parameters"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[256]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_parameters"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("include_parameter_values"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("include_parameter_values"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("include_raw_bytes"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[false]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("include_raw_bytes"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_raw_bytes"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[64]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_raw_bytes"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("include_asset_exposed_parameters"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[false]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("include_asset_exposed_parameters"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("include_instance_parameters"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[false]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("include_instance_parameters"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("include_parameter_object_properties"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[false]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("include_parameter_object_properties"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("include_emitter_runtime"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("include_emitter_runtime"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_emitter_instances"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[128]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_emitter_instances"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_object_properties"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[80]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_object_properties"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_array_elements"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[16]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_array_elements"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_property_value_chars"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[512]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_property_value_chars"),Carrier);}
 }
 if(Which==1){
  {auto Value=Args->TryGetField(TEXT("actor_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("actor_name"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("actor_label"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("actor_label"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("component_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("component_name"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("system_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("system_path"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("parameter_name_contains"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("parameter_name_contains"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_components"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[8]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_components"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_parameters"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[64]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_parameters"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("include_raw_bytes"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[false]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("include_raw_bytes"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_raw_bytes"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[64]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_raw_bytes"),Carrier);}
 }
 if(Which==2){
  {auto Value=Args->TryGetField(TEXT("actor_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("actor_name"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("actor_label"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("actor_label"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("component_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("component_name"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("system_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("system_path"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_components"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[16]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_components"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_grid_interfaces"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[16]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_grid_interfaces"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("include_internal"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("include_internal"),Carrier);}
 }
 if(Which==3){
  {auto Value=Args->TryGetField(TEXT("render_target_path"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("render_target_path"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("x0"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("x0"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("x1"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[-1]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("x1"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("y0"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("y0"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("y1"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[-1]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("y1"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("channel"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"max\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("channel"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("threshold"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0.01]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("threshold"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("max_first_hits"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[8]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("max_first_hits"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("probe_x"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[-1]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("probe_x"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("probe_y"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[-1]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("probe_y"),Carrier);}
  {auto Value=Args->TryGetField(TEXT("probe_radius"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[8]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("probe_radius"),Carrier);}
 }
 PackedInput=Json(Packed);return BoundText(PackedInput,MaxInputChars);
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!IsValid(Class)||Class->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||!Class->GetPathName().Equals(CanonicalClass,ESearchCase::CaseSensitive)||!Class->IsChildOf(UToolsetDefinition::StaticClass())){Error=TEXT("Exact live canonical NiagaraRuntimeInspection class required");return false;}
    for(int32 Which=0;Which<UE_ARRAY_COUNT(Methods);++Which){
        UFunction* Function=Class->FindFunctionByName(FName(Methods[Which]));if(!Function||Function->GetOuter()!=Class||!Function->GetName().Equals(Methods[Which],ESearchCase::CaseSensitive)){Error=TEXT("Exact owned NiagaraRuntimeInspection method required");return false;}
        auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable NiagaraRuntimeInspection method required");return false;}
        const auto& Layout=Layouts[Which];int32 Index=0;
        for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}if(Index>=Layout.Count||!It->GetName().Equals(Layout.Fields[Index],ESearchCase::CaseSensitive)||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||!CastField<FStrProperty>(*It)){Error=TEXT("Exact physical public JSON FString fields18/9/7/11 required");return false;}++Index;}
        if(Index!=Layout.Count){Error=TEXT("Exact physical arity required");return false;}
        if(Which==0){
            if(!Function->HasMetaData(TEXT("CPP_Default_actor_name"))||!Function->GetMetaData(TEXT("CPP_Default_actor_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_actor_label"))||!Function->GetMetaData(TEXT("CPP_Default_actor_label")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_component_name"))||!Function->GetMetaData(TEXT("CPP_Default_component_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_system_path"))||!Function->GetMetaData(TEXT("CPP_Default_system_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_parameter_name_contains"))||!Function->GetMetaData(TEXT("CPP_Default_parameter_name_contains")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_components"))||!Function->GetMetaData(TEXT("CPP_Default_max_components")).Equals(TEXT("32"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_parameters"))||!Function->GetMetaData(TEXT("CPP_Default_max_parameters")).Equals(TEXT("256"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_include_parameter_values"))||!Function->GetMetaData(TEXT("CPP_Default_include_parameter_values")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_include_raw_bytes"))||!Function->GetMetaData(TEXT("CPP_Default_include_raw_bytes")).Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_raw_bytes"))||!Function->GetMetaData(TEXT("CPP_Default_max_raw_bytes")).Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_include_asset_exposed_parameters"))||!Function->GetMetaData(TEXT("CPP_Default_include_asset_exposed_parameters")).Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_include_instance_parameters"))||!Function->GetMetaData(TEXT("CPP_Default_include_instance_parameters")).Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_include_parameter_object_properties"))||!Function->GetMetaData(TEXT("CPP_Default_include_parameter_object_properties")).Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_include_emitter_runtime"))||!Function->GetMetaData(TEXT("CPP_Default_include_emitter_runtime")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_emitter_instances"))||!Function->GetMetaData(TEXT("CPP_Default_max_emitter_instances")).Equals(TEXT("128"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_object_properties"))||!Function->GetMetaData(TEXT("CPP_Default_max_object_properties")).Equals(TEXT("80"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_array_elements"))||!Function->GetMetaData(TEXT("CPP_Default_max_array_elements")).Equals(TEXT("16"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_property_value_chars"))||!Function->GetMetaData(TEXT("CPP_Default_max_property_value_chars")).Equals(TEXT("512"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
        }
        if(Which==1){
            if(!Function->HasMetaData(TEXT("CPP_Default_actor_name"))||!Function->GetMetaData(TEXT("CPP_Default_actor_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_actor_label"))||!Function->GetMetaData(TEXT("CPP_Default_actor_label")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_component_name"))||!Function->GetMetaData(TEXT("CPP_Default_component_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_system_path"))||!Function->GetMetaData(TEXT("CPP_Default_system_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_parameter_name_contains"))||!Function->GetMetaData(TEXT("CPP_Default_parameter_name_contains")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_components"))||!Function->GetMetaData(TEXT("CPP_Default_max_components")).Equals(TEXT("8"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_parameters"))||!Function->GetMetaData(TEXT("CPP_Default_max_parameters")).Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_include_raw_bytes"))||!Function->GetMetaData(TEXT("CPP_Default_include_raw_bytes")).Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_raw_bytes"))||!Function->GetMetaData(TEXT("CPP_Default_max_raw_bytes")).Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
        }
        if(Which==2){
            if(!Function->HasMetaData(TEXT("CPP_Default_actor_name"))||!Function->GetMetaData(TEXT("CPP_Default_actor_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_actor_label"))||!Function->GetMetaData(TEXT("CPP_Default_actor_label")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_component_name"))||!Function->GetMetaData(TEXT("CPP_Default_component_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_system_path"))||!Function->GetMetaData(TEXT("CPP_Default_system_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_components"))||!Function->GetMetaData(TEXT("CPP_Default_max_components")).Equals(TEXT("16"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_grid_interfaces"))||!Function->GetMetaData(TEXT("CPP_Default_max_grid_interfaces")).Equals(TEXT("16"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_include_internal"))||!Function->GetMetaData(TEXT("CPP_Default_include_internal")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
        }
        if(Which==3){
            if(Function->HasMetaData(TEXT("CPP_Default_render_target_path"))){Error=TEXT("Required public field has reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_x0"))||!Function->GetMetaData(TEXT("CPP_Default_x0")).Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_x1"))||!Function->GetMetaData(TEXT("CPP_Default_x1")).Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_y0"))||!Function->GetMetaData(TEXT("CPP_Default_y0")).Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_y1"))||!Function->GetMetaData(TEXT("CPP_Default_y1")).Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_channel"))||!Function->GetMetaData(TEXT("CPP_Default_channel")).Equals(TEXT("\"max\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_threshold"))||!Function->GetMetaData(TEXT("CPP_Default_threshold")).Equals(TEXT("0.01"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_max_first_hits"))||!Function->GetMetaData(TEXT("CPP_Default_max_first_hits")).Equals(TEXT("8"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_probe_x"))||!Function->GetMetaData(TEXT("CPP_Default_probe_x")).Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_probe_y"))||!Function->GetMetaData(TEXT("CPP_Default_probe_y")).Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_probe_radius"))||!Function->GetMetaData(TEXT("CPP_Default_probe_radius")).Equals(TEXT("8"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
        }
    }return true;
}

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
    FString Value; return Object.IsValid() && Object->TryGetStringField(TEXT("type"), Value) && Value.Equals(Expected,ESearchCase::CaseSensitive);
}

bool Output11(const TSharedPtr<FJsonObject>& Tool)
{
    const TSharedPtr<FJsonObject>* Output=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;const TSharedPtr<FJsonObject>* Return=nullptr;const TSharedPtr<FJsonObject>* Fields=nullptr;
    const TSet<FString> Names {TEXT("success"),TEXT("operation_id"),TEXT("stage"),TEXT("error_code"),TEXT("error"),TEXT("changes_json"),TEXT("compile_json"),TEXT("save_json"),TEXT("validation_json"),TEXT("retry_json"),TEXT("data_json")};
    if(!Tool->TryGetObjectField(TEXT("outputSchema"),Output)||!Type(*Output,TEXT("object"))||!Required(*Output,{TEXT("returnValue")})
        ||!(*Output)->TryGetObjectField(TEXT("properties"),Props)||(*Props)->Values.Num()!=1||!(*Props)->TryGetObjectField(TEXT("returnValue"),Return)
        ||!Type(*Return,TEXT("object"))||!Required(*Return,Names)||!(*Return)->TryGetObjectField(TEXT("properties"),Fields)||(*Fields)->Values.Num()!=Names.Num()){return false;}
    for(const FString& Name:Names){const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Fields)->TryGetObjectField(Name,Field)||!Type(*Field,Name==TEXT("success")?TEXT("boolean"):TEXT("string"))){return false;}}
    return true;
}
bool RequiredOrAbsentEmpty(const TSharedPtr<FJsonObject>& Input,const TSet<FString>& Expected)
{return Expected.IsEmpty()&&!Input->HasField(TEXT("required"))?true:Required(Input,Expected);}

bool NiagaraRuntimeInspectionSchema(const FString& Physical,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Tools=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Physical),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||!Name.Equals(CanonicalName,ESearchCase::CaseSensitive)||!Root->TryGetArrayField(TEXT("tools"),Tools)||!Tools||Tools->Num()!=4){Error=TEXT("Exact canonical NiagaraRuntimeInspection Toolset4 required");return false;}
    TSet<int32> Seen;
    for(const auto& Value:*Tools){
        if(!Value.IsValid()||Value->Type!=EJson::Object){return false;}auto Tool=Value->AsObject();if(!Tool->TryGetStringField(TEXT("name"),Name)){return false;}
        int32 Which=INDEX_NONE;for(int32 I=0;I<4;++I){if(Name.Equals(FString(CanonicalName)+TEXT(".")+Methods[I],ESearchCase::CaseSensitive)){Which=I;}}
        if(Which==INDEX_NONE||Seen.Contains(Which)){Error=TEXT("One exact Niagara runtime inspection methods required");return false;}Seen.Add(Which);
        const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;
        if(!Tool->TryGetObjectField(TEXT("inputSchema"),Input)||!Type(*Input,TEXT("object"))){return false;}
        if(!(*Input)->TryGetObjectField(TEXT("properties"),Props)||(*Props)->Values.Num()!=Layouts[Which].Count){Error=TEXT("Exact physical Niagara runtime inspection fields required");return false;}
        if(Which==0){if(!RequiredOrAbsentEmpty(*Input,{})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("actor_label"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("component_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("system_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("parameter_name_contains"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_components"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("32"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_parameters"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("256"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("include_parameter_values"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("include_raw_bytes"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_raw_bytes"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("include_asset_exposed_parameters"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("include_instance_parameters"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("include_parameter_object_properties"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("include_emitter_runtime"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_emitter_instances"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("128"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_object_properties"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("80"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_array_elements"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("16"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_property_value_chars"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("512"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==1){if(!RequiredOrAbsentEmpty(*Input,{})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("actor_label"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("component_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("system_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("parameter_name_contains"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_components"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("8"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_parameters"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("include_raw_bytes"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_raw_bytes"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==2){if(!RequiredOrAbsentEmpty(*Input,{})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("actor_label"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("component_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("system_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_components"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("16"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_grid_interfaces"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("16"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("include_internal"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==3){if(!RequiredOrAbsentEmpty(*Input,{TEXT("render_target_path")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("render_target_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("x0"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("x1"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("y0"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("y1"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("channel"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"max\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("threshold"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0.01"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("max_first_hits"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("8"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("probe_x"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("probe_y"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("probe_radius"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("8"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        TSharedPtr<FJsonObject> OriginalInput;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LogicalInputSchemas[Which]),OriginalInput)||!OriginalInput.IsValid()){Error=TEXT("Frozen original input schema invalid");return false;}
        Tool->SetObjectField(TEXT("inputSchema"),OriginalInput.ToSharedRef());
        if(!Output11(Tool)){Error=TEXT("Exact standard11 output required for Niagara runtime inspection1");return false;}
    }Logical=Json(Root.ToSharedRef());return true;
}

class FNiagaraRuntimeInspectionToolset final : public FToolset
{
public:
    explicit FNiagaraRuntimeInspectionToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("niagara_runtime_inspection_raw_guard_unavailable"), TEXT("A live active game-thread NiagaraRuntimeInspection guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("niagara_runtime_inspection_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("niagara_runtime_inspection_raw_guard_definition_changed"), Error); }
        const int32 Which=MethodIndex(Method);
        if (Which==INDEX_NONE)
        { return Reject(TEXT("niagara_runtime_inspection_method_refused"), TEXT("Only the four exact original NiagaraRuntimeInspection methods are guarded")); }
        FString PackedInput;
        if (!RawTransform(Which, Input, PackedInput, Error))
        { return Reject(TEXT("niagara_runtime_inspection_raw_arguments_invalid"), Error); }
        const TSharedRef<FAttachmentState> CallState = State;
        const TSharedPtr<FToolset> OriginalLifetime = State->Original;
        const TSharedPtr<FToolset> WrapperLifetime = State->Wrapper;
        State->DelegatedMethodIndex.Store(INDEX_NONE);
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
            State->DelegatedMethodIndex.Store(INDEX_NONE); State->InFlight.Store(0);
            if (Error.IsEmpty()) { Error = TEXT("NiagaraRuntimeInspection ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("niagara_runtime_inspection_raw_guard_changed_before_dispatch"), Error);
        }
        // Only the actual guarded Named method delegation may ask Verify for its exact admission marker.
        State->DelegatedMethodIndex.Store(Which);
        // Direct public delegation avoids recursion through the same canonical Registry name.
        return OriginalLifetime->ExecuteTool(Method, PackedInput).Next(
            [CallState, OriginalLifetime, WrapperLifetime](FOutput&& Output) -> FOutput
            {
                // Official reflected handler completions run on the game thread; no UObject work here.
                if (IsInGameThread()) { OriginalLifetime->SetEnabled(false); }
                else { CallState->Active.Store(false); }
                CallState->DelegatedMethodIndex.Store(INDEX_NONE); CallState->InFlight.Store(0);
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
    Report->SetStringField(TEXT("guard_scope"),TEXT("Four original live Niagara runtime/store/Grid2D/render-target observation contracts retain Named scalar coercion, truthiness and RT omission while raw field presence stays Native-owned. Native explicit mutation/compile/save are zero; virtual getter/loader/render-thread synchronization effects may remain globally unknown. Store raw Runtime18 forced5, unavailable data, latent GPU count estimates and internal RT readback/flush remain qualified; no synthetic payload, simulation creation, extra wait, replay or Cubeless runtime dependency."));
    for(const TCHAR* Key : {TEXT("active"),TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("niagara_runtime_public18_9_7_11_schema_verified"),TEXT("required0_0_0_1_default18_9_7_10_verified"),TEXT("json_value_carrier45_schema_verified"),TEXT("scalar_original_model_and_body_coercion_preserved"),TEXT("body_truthiness_RT_omission_and_raw_presence_preserved"),TEXT("Runtime_raw18_Store_forced5_native_owned"),TEXT("niagara_runtime_inspection4_native_contract_owned")}){Report->SetBoolField(Key,false);}
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured NiagaraRuntimeInspection guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured NiagaraRuntimeInspection wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!NiagaraRuntimeInspectionSchema(CurrentSchema, CurrentLogical, Error) || !CurrentSchema.Equals(State->ReflectedSchema,ESearchCase::CaseSensitive) || !CurrentLogical.Equals(State->FullSchema,ESearchCase::CaseSensitive))
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original NiagaraRuntimeInspection schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("NiagaraRuntimeInspection wrapper owner/class changed during reflected schema callbacks"); } return false; }
    for(const TCHAR* Key : {TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("niagara_runtime_public18_9_7_11_schema_verified"),TEXT("required0_0_0_1_default18_9_7_10_verified"),TEXT("json_value_carrier45_schema_verified"),TEXT("scalar_original_model_and_body_coercion_preserved"),TEXT("body_truthiness_RT_omission_and_raw_presence_preserved"),TEXT("Runtime_raw18_Store_forced5_native_owned"),TEXT("niagara_runtime_inspection4_native_contract_owned")}){Report->SetBoolField(Key,true);}
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("NiagaraRuntimeInspection guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered NiagaraRuntimeInspection wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered NiagaraRuntimeInspection logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned NiagaraRuntimeInspection guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallNiagaraRuntimeInspectionToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("NiagaraRuntimeInspection guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained NiagaraRuntimeInspection owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!NiagaraRuntimeInspectionSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FNiagaraRuntimeInspectionToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original NiagaraRuntimeInspection handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("NiagaraRuntimeInspection raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyNiagaraRuntimeInspectionToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("NiagaraRuntimeInspection guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const int32 DelegatedIndex=Owned->DelegatedMethodIndex.Load();
        const bool Admitted = Owned->InFlight.Load() == 1 && DelegatedIndex>=0 && DelegatedIndex<UE_ARRAY_COUNT(Methods) && Owned->Active.Load()
            && Owned->Class.Get() == ToolsetClass && Current.HasValue()
            && Current.GetValue().Get() == Owned->Subsystem.Get() && Owned->Wrapper.IsValid()
            && Current.GetValue()->ToolsetRegistry.Find(CanonicalName) == Owned->Wrapper;
        Report->SetBoolField(TEXT("delegated_call_admitted"), Admitted);
        Report->SetBoolField(TEXT("delegated_admission_only"), true);
        Report->SetStringField(TEXT("delegated_method"), Admitted ? Methods[DelegatedIndex] : TEXT(""));
        Report->SetBoolField(TEXT("active"), Admitted); Report->SetBoolField(TEXT("guard_attached"), Admitted);
        // This is only the current wrapper's admission check. No schema/metadata callbacks,
        // install/remove/asset work, or general readiness proof is performed here.
        // False intentionally keeps Native helper ready/verified=False even when admitted.
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named NiagaraRuntimeInspection delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed NiagaraRuntimeInspection raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original NiagaraRuntimeInspection registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveNiagaraRuntimeInspectionToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("NiagaraRuntimeInspection guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained NiagaraRuntimeInspection class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownNiagaraRuntimeInspectionToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
