#include "Registration/CubelessChaosDestructionPerformanceToolset.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.chaos_destruction_performance_toolset.CubelessChaosDestructionPerformanceTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/chaos_destruction_performance_toolset_PY.CubelessChaosDestructionPerformanceTools");
constexpr const TCHAR* Methods[]={TEXT("run_chaos_destruction_performance_smoke")};
constexpr const TCHAR* Fields0[]={TEXT("operation"),TEXT("actor_name"),TEXT("solver_actor_name"),TEXT("start"),TEXT("end"),TEXT("expected_hit"),TEXT("trace_count"),TEXT("benchmark_path"),TEXT("strain"),TEXT("radius"),TEXT("breaking_velocity"),TEXT("breaking_velocity_vector"),TEXT("item_index"),TEXT("expected_direction"),TEXT("dispatch_world_time_seconds"),TEXT("witness_actor_name"),TEXT("floor_actor_name"),TEXT("one_way_level"),TEXT("initial_witness_location"),TEXT("initial_witness_velocity"),TEXT("per_advance_breaks_allowed"),TEXT("per_advance_breaks_reschedule_limit"),TEXT("particle_release_throttling_min_count"),TEXT("particle_release_throttling_max_count"),TEXT("optimize_for_runtime_memory"),TEXT("performance_scenario"),TEXT("capture_limit_frames"),TEXT("require_play_session")};
struct FLayout{const TCHAR* const* Fields;int32 Count;};
constexpr FLayout Layouts[]={{Fields0,28}};
constexpr int32 MaxInputChars=8*1024*1024,MaxCarrierChars=1024*1024,MaxRawDepth=65,MaxValueSlots=262144;
int32 MethodIndex(const FString& Method){for(int32 I=0;I<1;++I){if(Method.Equals(Methods[I],ESearchCase::CaseSensitive)){return I;}}return INDEX_NONE;}
constexpr const TCHAR* LogicalInputSchemas[]={TEXT("{\"properties\":{\"operation\":{\"default\":\"sample\",\"title\":\"Operation\",\"type\":\"string\"},\"actor_name\":{\"default\":\"\",\"title\":\"Actor Name\",\"type\":\"string\"},\"solver_actor_name\":{\"default\":\"\",\"title\":\"Solver Actor Name\",\"type\":\"string\"},\"start\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Start\"},\"end\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"End\"},\"expected_hit\":{\"default\":true,\"title\":\"Expected Hit\",\"type\":\"boolean\"},\"trace_count\":{\"default\":100,\"title\":\"Trace Count\",\"type\":\"integer\"},\"benchmark_path\":{\"default\":\"both\",\"title\":\"Benchmark Path\",\"type\":\"string\"},\"strain\":{\"anyOf\":[{\"type\":\"number\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Strain\"},\"radius\":{\"default\":100,\"title\":\"Radius\",\"type\":\"number\"},\"breaking_velocity\":{\"default\":600,\"title\":\"Breaking Velocity\",\"type\":\"number\"},\"breaking_velocity_vector\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Breaking Velocity Vector\"},\"item_index\":{\"default\":-1,\"title\":\"Item Index\",\"type\":\"integer\"},\"expected_direction\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Expected Direction\"},\"dispatch_world_time_seconds\":{\"default\":-1,\"title\":\"Dispatch World Time Seconds\",\"type\":\"number\"},\"witness_actor_name\":{\"default\":\"\",\"title\":\"Witness Actor Name\",\"type\":\"string\"},\"floor_actor_name\":{\"default\":\"Chaos_StaticFloor\",\"title\":\"Floor Actor Name\",\"type\":\"string\"},\"one_way_level\":{\"default\":-1,\"title\":\"One Way Level\",\"type\":\"integer\"},\"initial_witness_location\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Initial Witness Location\"},\"initial_witness_velocity\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Initial Witness Velocity\"},\"per_advance_breaks_allowed\":{\"default\":2,\"title\":\"Per Advance Breaks Allowed\",\"type\":\"integer\"},\"per_advance_breaks_reschedule_limit\":{\"default\":64,\"title\":\"Per Advance Breaks Reschedule Limit\",\"type\":\"integer\"},\"particle_release_throttling_min_count\":{\"default\":64,\"title\":\"Particle Release Throttling Min Count\",\"type\":\"integer\"},\"particle_release_throttling_max_count\":{\"default\":128,\"title\":\"Particle Release Throttling Max Count\",\"type\":\"integer\"},\"optimize_for_runtime_memory\":{\"default\":true,\"title\":\"Optimize For Runtime Memory\",\"type\":\"boolean\"},\"performance_scenario\":{\"default\":\"\",\"title\":\"Performance Scenario\",\"type\":\"string\"},\"capture_limit_frames\":{\"default\":90,\"title\":\"Capture Limit Frames\",\"type\":\"integer\"},\"require_play_session\":{\"default\":true,\"title\":\"Require Play Session\",\"type\":\"boolean\"}},\"title\":\"run_chaos_destruction_performance_smokeArguments\",\"type\":\"object\"}")};
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
    R->SetStringField(TEXT("stage"), TEXT("chaos_destruction_performance_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("chaos_destruction_performance_raw_not_dispatched"));
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
    TSharedPtr<FJsonObject> Args;
    if(Which<0||Which>=1||!BoundText(Input,MaxInputChars)||!RawDepth(Input)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid()||!FiniteValues(Args.ToSharedRef())){Error=TEXT("Chaos destruction performance requires bounded finite raw object");return false;}
    auto Packed=MakeShared<FJsonObject>();
    // Original public FastMCP arg_model ignores extra fields, including Native-only session controls.
    // Select known fields with exact JsonObject key lookup; legacy raw bypasses this wrapper.
    if(Which==0){
        {auto Value=Args->TryGetField(TEXT("operation"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"sample\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("operation"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("actor_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("actor_name"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("solver_actor_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("solver_actor_name"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("start"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("start"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("end"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("end"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("expected_hit"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("expected_hit"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("trace_count"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[100]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("trace_count"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("benchmark_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"both\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("benchmark_path"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("strain"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("strain"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("radius"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[100]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("radius"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("breaking_velocity"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[600]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("breaking_velocity"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("breaking_velocity_vector"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("breaking_velocity_vector"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("item_index"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[-1]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("item_index"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("expected_direction"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("expected_direction"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("dispatch_world_time_seconds"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[-1]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("dispatch_world_time_seconds"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("witness_actor_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("witness_actor_name"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("floor_actor_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"Chaos_StaticFloor\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("floor_actor_name"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("one_way_level"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[-1]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("one_way_level"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("initial_witness_location"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("initial_witness_location"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("initial_witness_velocity"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("initial_witness_velocity"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("per_advance_breaks_allowed"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[2]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("per_advance_breaks_allowed"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("per_advance_breaks_reschedule_limit"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[64]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("per_advance_breaks_reschedule_limit"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("particle_release_throttling_min_count"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[64]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("particle_release_throttling_min_count"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("particle_release_throttling_max_count"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[128]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("particle_release_throttling_max_count"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("optimize_for_runtime_memory"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("optimize_for_runtime_memory"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("performance_scenario"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("performance_scenario"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("capture_limit_frames"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[90]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("capture_limit_frames"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("require_play_session"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("require_play_session"),Carrier);}
    }
    PackedInput=Json(Packed);return true;
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!IsValid(Class)||Class->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||!Class->GetPathName().Equals(CanonicalClass,ESearchCase::CaseSensitive)||!Class->IsChildOf(UToolsetDefinition::StaticClass())){Error=TEXT("Exact live canonical ChaosDestructionPerformance class required");return false;}
    for(int32 Which=0;Which<1;++Which){
        UFunction* Function=Class->FindFunctionByName(FName(Methods[Which]));if(!Function||Function->GetOuter()!=Class||!Function->GetName().Equals(Methods[Which],ESearchCase::CaseSensitive)){Error=TEXT("Exact owned ChaosDestructionPerformance method required");return false;}
        auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable ChaosDestructionPerformance method required");return false;}
        const auto& Layout=Layouts[Which];int32 Index=0;
        for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}if(Index>=Layout.Count||!It->GetName().Equals(Layout.Fields[Index],ESearchCase::CaseSensitive)||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||!CastField<FStrProperty>(*It)){Error=TEXT("Exact physical public JSON FString fields28 required");return false;}++Index;}
        if(Index!=Layout.Count){Error=TEXT("Exact physical arity required");return false;}
        if(Which==0){
            if(!Function->HasMetaData(TEXT("CPP_Default_operation"))||!Function->GetMetaData(TEXT("CPP_Default_operation")).Equals(TEXT("\"sample\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_actor_name"))||!Function->GetMetaData(TEXT("CPP_Default_actor_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_solver_actor_name"))||!Function->GetMetaData(TEXT("CPP_Default_solver_actor_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_start"))||!Function->GetMetaData(TEXT("CPP_Default_start")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_end"))||!Function->GetMetaData(TEXT("CPP_Default_end")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_expected_hit"))||!Function->GetMetaData(TEXT("CPP_Default_expected_hit")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_trace_count"))||!Function->GetMetaData(TEXT("CPP_Default_trace_count")).Equals(TEXT("100"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_benchmark_path"))||!Function->GetMetaData(TEXT("CPP_Default_benchmark_path")).Equals(TEXT("\"both\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_strain"))||!Function->GetMetaData(TEXT("CPP_Default_strain")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_radius"))||!Function->GetMetaData(TEXT("CPP_Default_radius")).Equals(TEXT("100"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_breaking_velocity"))||!Function->GetMetaData(TEXT("CPP_Default_breaking_velocity")).Equals(TEXT("600"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_breaking_velocity_vector"))||!Function->GetMetaData(TEXT("CPP_Default_breaking_velocity_vector")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_item_index"))||!Function->GetMetaData(TEXT("CPP_Default_item_index")).Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_expected_direction"))||!Function->GetMetaData(TEXT("CPP_Default_expected_direction")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_dispatch_world_time_seconds"))||!Function->GetMetaData(TEXT("CPP_Default_dispatch_world_time_seconds")).Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_witness_actor_name"))||!Function->GetMetaData(TEXT("CPP_Default_witness_actor_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_floor_actor_name"))||!Function->GetMetaData(TEXT("CPP_Default_floor_actor_name")).Equals(TEXT("\"Chaos_StaticFloor\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_one_way_level"))||!Function->GetMetaData(TEXT("CPP_Default_one_way_level")).Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_initial_witness_location"))||!Function->GetMetaData(TEXT("CPP_Default_initial_witness_location")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_initial_witness_velocity"))||!Function->GetMetaData(TEXT("CPP_Default_initial_witness_velocity")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_per_advance_breaks_allowed"))||!Function->GetMetaData(TEXT("CPP_Default_per_advance_breaks_allowed")).Equals(TEXT("2"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_per_advance_breaks_reschedule_limit"))||!Function->GetMetaData(TEXT("CPP_Default_per_advance_breaks_reschedule_limit")).Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_particle_release_throttling_min_count"))||!Function->GetMetaData(TEXT("CPP_Default_particle_release_throttling_min_count")).Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_particle_release_throttling_max_count"))||!Function->GetMetaData(TEXT("CPP_Default_particle_release_throttling_max_count")).Equals(TEXT("128"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_optimize_for_runtime_memory"))||!Function->GetMetaData(TEXT("CPP_Default_optimize_for_runtime_memory")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_performance_scenario"))||!Function->GetMetaData(TEXT("CPP_Default_performance_scenario")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_capture_limit_frames"))||!Function->GetMetaData(TEXT("CPP_Default_capture_limit_frames")).Equals(TEXT("90"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_require_play_session"))||!Function->GetMetaData(TEXT("CPP_Default_require_play_session")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
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
    FString Value; return Object.IsValid() && Object->TryGetStringField(TEXT("type"), Value) && Value == Expected;
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

bool ChaosDestructionPerformanceSchema(const FString& Physical,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Tools=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Physical),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||!Name.Equals(CanonicalName,ESearchCase::CaseSensitive)||!Root->TryGetArrayField(TEXT("tools"),Tools)||!Tools||Tools->Num()!=1){Error=TEXT("Exact canonical ChaosDestructionPerformance Toolset1 required");return false;}
    TSet<int32> Seen;
    for(const auto& Value:*Tools){
        if(!Value.IsValid()||Value->Type!=EJson::Object){return false;}auto Tool=Value->AsObject();if(!Tool->TryGetStringField(TEXT("name"),Name)){return false;}
        int32 Which=INDEX_NONE;for(int32 I=0;I<1;++I){if(Name.Equals(FString(CanonicalName)+TEXT(".")+Methods[I],ESearchCase::CaseSensitive)){Which=I;}}
        if(Which==INDEX_NONE||Seen.Contains(Which)){Error=TEXT("One exact Chaos destruction performance methods required");return false;}Seen.Add(Which);
        const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;
        if(!Tool->TryGetObjectField(TEXT("inputSchema"),Input)||!Type(*Input,TEXT("object"))){return false;}
        if(!(*Input)->TryGetObjectField(TEXT("properties"),Props)||(*Props)->Values.Num()!=Layouts[Which].Count){Error=TEXT("Exact physical Chaos destruction performance fields required");return false;}
        if(Which==0){if(!RequiredOrAbsentEmpty(*Input,{})){Error=TEXT("Physical required0 differs");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("operation"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"sample\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("solver_actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("start"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("end"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("expected_hit"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("trace_count"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("100"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("benchmark_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"both\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("strain"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("radius"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("100"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("breaking_velocity"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("600"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("breaking_velocity_vector"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("item_index"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("expected_direction"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("dispatch_world_time_seconds"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("witness_actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("floor_actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"Chaos_StaticFloor\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("one_way_level"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("-1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("initial_witness_location"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("initial_witness_velocity"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("per_advance_breaks_allowed"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("2"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("per_advance_breaks_reschedule_limit"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("particle_release_throttling_min_count"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("particle_release_throttling_max_count"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("128"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("optimize_for_runtime_memory"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("performance_scenario"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("capture_limit_frames"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("90"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("require_play_session"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        TSharedPtr<FJsonObject> OriginalInput;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LogicalInputSchemas[Which]),OriginalInput)||!OriginalInput.IsValid()){Error=TEXT("Frozen original input schema invalid");return false;}
        Tool->SetObjectField(TEXT("inputSchema"),OriginalInput.ToSharedRef());
        if(!Output11(Tool)){Error=TEXT("Exact standard11 output required for Chaos destruction performance1");return false;}
    }Logical=Json(Root.ToSharedRef());return true;
}

class FChaosDestructionPerformanceToolset final : public FToolset
{
public:
    explicit FChaosDestructionPerformanceToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("chaos_destruction_performance_raw_guard_unavailable"), TEXT("A live active game-thread ChaosDestructionPerformance guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("chaos_destruction_performance_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("chaos_destruction_performance_raw_guard_definition_changed"), Error); }
        const int32 Which=MethodIndex(Method);
        if (Which==INDEX_NONE)
        { return Reject(TEXT("chaos_destruction_performance_method_refused"), TEXT("Only the four exact original ChaosDestructionPerformance methods are guarded")); }
        FString PackedInput;
        if (!RawTransform(Which, Input, PackedInput, Error))
        { return Reject(TEXT("chaos_destruction_performance_raw_arguments_invalid"), Error); }
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
            if (Error.IsEmpty()) { Error = TEXT("ChaosDestructionPerformance ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("chaos_destruction_performance_raw_guard_changed_before_dispatch"), Error);
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
    Report->SetStringField(TEXT("guard_scope"),TEXT("Dedicated Chaos destruction performance public28/raw38: preserve all18 Native operations/7behavior groups, original eager admission/defaults, seven nullable omissions, raw hidden10 and scenario omissions. Native owns real per-frame metrics, removal timelines, one-way native contact and throttle/trace diagnostics; this connection is not their implementation or behavior proof. GT submission/enqueue differs from physics completion. Current request save/partial and historical in-flight session snapshots remain separate. No automatic retry/atomic rollback/packaged completion claim."));
    for(const TCHAR* Key : {TEXT("active"),TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("chaos_destruction_performance_public28_schema_verified"),TEXT("required0_default28_verified"),TEXT("json_value_carrier28_schema_verified"),TEXT("ordinary_string7_integer8_number4_boolean3_array6_schema_verified"),TEXT("nullable7_original_omission_schema_verified"),TEXT("hidden10_not_public_schema_verified"),TEXT("operation18_contract_native_owned")}){Report->SetBoolField(Key,false);}
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured ChaosDestructionPerformance guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured ChaosDestructionPerformance wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!ChaosDestructionPerformanceSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original ChaosDestructionPerformance schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("ChaosDestructionPerformance wrapper owner/class changed during reflected schema callbacks"); } return false; }
    for(const TCHAR* Key : {TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("chaos_destruction_performance_public28_schema_verified"),TEXT("required0_default28_verified"),TEXT("json_value_carrier28_schema_verified"),TEXT("ordinary_string7_integer8_number4_boolean3_array6_schema_verified"),TEXT("nullable7_original_omission_schema_verified"),TEXT("hidden10_not_public_schema_verified"),TEXT("operation18_contract_native_owned")}){Report->SetBoolField(Key,true);}
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("ChaosDestructionPerformance guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered ChaosDestructionPerformance wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered ChaosDestructionPerformance logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned ChaosDestructionPerformance guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallChaosDestructionPerformanceToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("ChaosDestructionPerformance guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained ChaosDestructionPerformance owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!ChaosDestructionPerformanceSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FChaosDestructionPerformanceToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original ChaosDestructionPerformance handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("ChaosDestructionPerformance raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyChaosDestructionPerformanceToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("ChaosDestructionPerformance guard verification requires an idle live editor game thread"); return false; }
    if (Owned.IsValid() && Owned->InFlight.Load() != 0)
    {
        auto Current = UToolsetRegistrySubsystem::Get();
        const int32 DelegatedIndex=Owned->DelegatedMethodIndex.Load();
        const bool Admitted = Owned->InFlight.Load() == 1 && DelegatedIndex>=0 && DelegatedIndex<4 && Owned->Active.Load()
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
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named ChaosDestructionPerformance delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed ChaosDestructionPerformance raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original ChaosDestructionPerformance registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveChaosDestructionPerformanceToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("ChaosDestructionPerformance guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained ChaosDestructionPerformance class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownChaosDestructionPerformanceToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
