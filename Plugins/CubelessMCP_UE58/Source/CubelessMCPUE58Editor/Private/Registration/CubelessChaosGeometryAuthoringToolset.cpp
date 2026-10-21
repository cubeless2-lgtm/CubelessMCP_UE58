#include "Registration/CubelessChaosGeometryAuthoringToolset.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.chaos_geometry_authoring_toolset.CubelessChaosGeometryAuthoringTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/chaos_geometry_authoring_toolset_PY.CubelessChaosGeometryAuthoringTools");
constexpr const TCHAR* Methods[]={TEXT("create_geometry_collection_from_mesh"),TEXT("configure_building_fracture"),TEXT("configure_anchor_field")};
constexpr const TCHAR* Fields0[]={TEXT("source_static_mesh_paths"),TEXT("output_geometry_collection_path"),TEXT("source_transforms"),TEXT("root_mesh_proxy_static_mesh_path"),TEXT("material_reindex"),TEXT("overwrite_existing")};
constexpr const TCHAR* Fields1[]={TEXT("source_geometry_collection_path"),TEXT("output_geometry_collection_path"),TEXT("fracture_method"),TEXT("site_count"),TEXT("random_seed"),TEXT("cluster_levels"),TEXT("cluster_size"),TEXT("damage_thresholds"),TEXT("merge_tiny_geometry"),TEXT("tiny_relative_threshold"),TEXT("collision_object_reduction_percentage"),TEXT("maximum_collision_particles"),TEXT("collision_implicit_type"),TEXT("remove_on_sleep"),TEXT("remove_on_break"),TEXT("sleep_time_seconds"),TEXT("removal_duration_seconds"),TEXT("break_timer_seconds"),TEXT("root_mesh_proxy_static_mesh_path"),TEXT("overwrite_existing")};
constexpr const TCHAR* Fields2[]={TEXT("geometry_collection_actor_name"),TEXT("field_actor_name"),TEXT("box_center"),TEXT("box_extent"),TEXT("field_magnitude"),TEXT("save_level")};
struct FLayout{const TCHAR* const* Fields;int32 Count;};
constexpr FLayout Layouts[]={{Fields0,6},{Fields1,20},{Fields2,6}};
constexpr int32 MaxInputChars=8*1024*1024,MaxCarrierChars=1024*1024,MaxRawDepth=65,MaxValueSlots=262144;
int32 MethodIndex(const FString& Method){for(int32 I=0;I<3;++I){if(Method.Equals(Methods[I],ESearchCase::CaseSensitive)){return I;}}return INDEX_NONE;}
constexpr const TCHAR* LogicalInputSchemas[]={TEXT("{\"properties\":{\"source_static_mesh_paths\":{\"items\":{\"type\":\"string\"},\"title\":\"Source Static Mesh Paths\",\"type\":\"array\"},\"output_geometry_collection_path\":{\"title\":\"Output Geometry Collection Path\",\"type\":\"string\"},\"source_transforms\":{\"anyOf\":[{\"items\":{\"type\":\"object\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Source Transforms\"},\"root_mesh_proxy_static_mesh_path\":{\"default\":\"\",\"title\":\"Root Mesh Proxy Static Mesh Path\",\"type\":\"string\"},\"material_reindex\":{\"default\":true,\"title\":\"Material Reindex\",\"type\":\"boolean\"},\"overwrite_existing\":{\"default\":false,\"title\":\"Overwrite Existing\",\"type\":\"boolean\"}},\"required\":[\"source_static_mesh_paths\",\"output_geometry_collection_path\"],\"title\":\"create_geometry_collection_from_meshArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"source_geometry_collection_path\":{\"title\":\"Source Geometry Collection Path\",\"type\":\"string\"},\"output_geometry_collection_path\":{\"title\":\"Output Geometry Collection Path\",\"type\":\"string\"},\"fracture_method\":{\"default\":\"uniform_voronoi\",\"title\":\"Fracture Method\",\"type\":\"string\"},\"site_count\":{\"default\":64,\"title\":\"Site Count\",\"type\":\"integer\"},\"random_seed\":{\"default\":1337,\"title\":\"Random Seed\",\"type\":\"integer\"},\"cluster_levels\":{\"default\":2,\"title\":\"Cluster Levels\",\"type\":\"integer\"},\"cluster_size\":{\"default\":8,\"title\":\"Cluster Size\",\"type\":\"integer\"},\"damage_thresholds\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Damage Thresholds\"},\"merge_tiny_geometry\":{\"default\":true,\"title\":\"Merge Tiny Geometry\",\"type\":\"boolean\"},\"tiny_relative_threshold\":{\"default\":0.01,\"title\":\"Tiny Relative Threshold\",\"type\":\"number\"},\"collision_object_reduction_percentage\":{\"default\":20,\"title\":\"Collision Object Reduction Percentage\",\"type\":\"number\"},\"maximum_collision_particles\":{\"default\":60,\"title\":\"Maximum Collision Particles\",\"type\":\"integer\"},\"collision_implicit_type\":{\"default\":\"convex\",\"title\":\"Collision Implicit Type\",\"type\":\"string\"},\"remove_on_sleep\":{\"default\":true,\"title\":\"Remove On Sleep\",\"type\":\"boolean\"},\"remove_on_break\":{\"default\":true,\"title\":\"Remove On Break\",\"type\":\"boolean\"},\"sleep_time_seconds\":{\"default\":5,\"title\":\"Sleep Time Seconds\",\"type\":\"number\"},\"removal_duration_seconds\":{\"default\":1,\"title\":\"Removal Duration Seconds\",\"type\":\"number\"},\"break_timer_seconds\":{\"default\":1,\"title\":\"Break Timer Seconds\",\"type\":\"number\"},\"root_mesh_proxy_static_mesh_path\":{\"default\":\"\",\"title\":\"Root Mesh Proxy Static Mesh Path\",\"type\":\"string\"},\"overwrite_existing\":{\"default\":false,\"title\":\"Overwrite Existing\",\"type\":\"boolean\"}},\"required\":[\"source_geometry_collection_path\",\"output_geometry_collection_path\"],\"title\":\"configure_building_fractureArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"geometry_collection_actor_name\":{\"title\":\"Geometry Collection Actor Name\",\"type\":\"string\"},\"field_actor_name\":{\"default\":\"Chaos_AnchorField\",\"title\":\"Field Actor Name\",\"type\":\"string\"},\"box_center\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Box Center\"},\"box_extent\":{\"anyOf\":[{\"items\":{\"type\":\"number\"},\"type\":\"array\"},{\"type\":\"null\"}],\"default\":null,\"title\":\"Box Extent\"},\"field_magnitude\":{\"default\":1,\"title\":\"Field Magnitude\",\"type\":\"integer\"},\"save_level\":{\"default\":true,\"title\":\"Save Level\",\"type\":\"boolean\"}},\"required\":[\"geometry_collection_actor_name\"],\"title\":\"configure_anchor_fieldArguments\",\"type\":\"object\"}")};
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
    R->SetStringField(TEXT("stage"), TEXT("chaos_geometry_authoring_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("chaos_geometry_authoring_raw_not_dispatched"));
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
    if(Which<0||Which>=3||!BoundText(Input,MaxInputChars)||!RawDepth(Input)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid()||!FiniteValues(Args.ToSharedRef())){Error=TEXT("Chaos geometry authoring requires bounded finite raw object");return false;}
    auto Packed=MakeShared<FJsonObject>();
    // Original public FastMCP arg_model ignores extra fields, including Native-only session controls.
    // Select known fields with exact JsonObject key lookup; legacy raw bypasses this wrapper.
    if(Which==0){
        {auto Value=Args->TryGetField(TEXT("source_static_mesh_paths"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("source_static_mesh_paths"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("output_geometry_collection_path"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("output_geometry_collection_path"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("source_transforms"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("source_transforms"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("root_mesh_proxy_static_mesh_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("root_mesh_proxy_static_mesh_path"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("material_reindex"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("material_reindex"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("overwrite_existing"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[false]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("overwrite_existing"),Carrier);}
    }
    if(Which==1){
        {auto Value=Args->TryGetField(TEXT("source_geometry_collection_path"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("source_geometry_collection_path"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("output_geometry_collection_path"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("output_geometry_collection_path"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("fracture_method"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"uniform_voronoi\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("fracture_method"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("site_count"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[64]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("site_count"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("random_seed"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[1337]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("random_seed"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("cluster_levels"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[2]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("cluster_levels"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("cluster_size"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[8]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("cluster_size"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("damage_thresholds"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("damage_thresholds"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("merge_tiny_geometry"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("merge_tiny_geometry"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("tiny_relative_threshold"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0.01]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("tiny_relative_threshold"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("collision_object_reduction_percentage"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[20]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("collision_object_reduction_percentage"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("maximum_collision_particles"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[60]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("maximum_collision_particles"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("collision_implicit_type"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"convex\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("collision_implicit_type"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("remove_on_sleep"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("remove_on_sleep"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("remove_on_break"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("remove_on_break"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("sleep_time_seconds"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[5]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("sleep_time_seconds"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("removal_duration_seconds"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[1]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("removal_duration_seconds"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("break_timer_seconds"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[1]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("break_timer_seconds"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("root_mesh_proxy_static_mesh_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("root_mesh_proxy_static_mesh_path"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("overwrite_existing"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[false]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("overwrite_existing"),Carrier);}
    }
    if(Which==2){
        {auto Value=Args->TryGetField(TEXT("geometry_collection_actor_name"));if(!Value.IsValid()){Error=TEXT("Required public field missing");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("geometry_collection_actor_name"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("field_actor_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"Chaos_AnchorField\"]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("field_actor_name"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("box_center"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("box_center"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("box_extent"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[null]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("box_extent"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("field_magnitude"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[1]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("field_magnitude"),Carrier);}
        {auto Value=Args->TryGetField(TEXT("save_level"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[true]")),Defaults)||Defaults.Num()!=1){Error=TEXT("Internal default JSON invalid");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error)){return false;}Packed->SetStringField(TEXT("save_level"),Carrier);}
    }
    PackedInput=Json(Packed);return true;
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!IsValid(Class)||Class->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||!Class->GetPathName().Equals(CanonicalClass,ESearchCase::CaseSensitive)||!Class->IsChildOf(UToolsetDefinition::StaticClass())){Error=TEXT("Exact live canonical ChaosGeometryAuthoring class required");return false;}
    for(int32 Which=0;Which<3;++Which){
        UFunction* Function=Class->FindFunctionByName(FName(Methods[Which]));if(!Function||Function->GetOuter()!=Class||!Function->GetName().Equals(Methods[Which],ESearchCase::CaseSensitive)){Error=TEXT("Exact owned ChaosGeometryAuthoring method required");return false;}
        auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable ChaosGeometryAuthoring method required");return false;}
        const auto& Layout=Layouts[Which];int32 Index=0;
        for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}if(Index>=Layout.Count||!It->GetName().Equals(Layout.Fields[Index],ESearchCase::CaseSensitive)||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||!CastField<FStrProperty>(*It)){Error=TEXT("Exact physical public JSON FString fields6/20/6 required");return false;}++Index;}
        if(Index!=Layout.Count){Error=TEXT("Exact physical arity required");return false;}
        if(Which==0){
            if(Function->HasMetaData(TEXT("CPP_Default_source_static_mesh_paths"))){Error=TEXT("Required public field has reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_output_geometry_collection_path"))){Error=TEXT("Required public field has reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_source_transforms"))||!Function->GetMetaData(TEXT("CPP_Default_source_transforms")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_root_mesh_proxy_static_mesh_path"))||!Function->GetMetaData(TEXT("CPP_Default_root_mesh_proxy_static_mesh_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_material_reindex"))||!Function->GetMetaData(TEXT("CPP_Default_material_reindex")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_overwrite_existing"))||!Function->GetMetaData(TEXT("CPP_Default_overwrite_existing")).Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
        }
        if(Which==1){
            if(Function->HasMetaData(TEXT("CPP_Default_source_geometry_collection_path"))){Error=TEXT("Required public field has reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_output_geometry_collection_path"))){Error=TEXT("Required public field has reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_fracture_method"))||!Function->GetMetaData(TEXT("CPP_Default_fracture_method")).Equals(TEXT("\"uniform_voronoi\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_site_count"))||!Function->GetMetaData(TEXT("CPP_Default_site_count")).Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_random_seed"))||!Function->GetMetaData(TEXT("CPP_Default_random_seed")).Equals(TEXT("1337"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_cluster_levels"))||!Function->GetMetaData(TEXT("CPP_Default_cluster_levels")).Equals(TEXT("2"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_cluster_size"))||!Function->GetMetaData(TEXT("CPP_Default_cluster_size")).Equals(TEXT("8"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_damage_thresholds"))||!Function->GetMetaData(TEXT("CPP_Default_damage_thresholds")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_merge_tiny_geometry"))||!Function->GetMetaData(TEXT("CPP_Default_merge_tiny_geometry")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_tiny_relative_threshold"))||!Function->GetMetaData(TEXT("CPP_Default_tiny_relative_threshold")).Equals(TEXT("0.01"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_collision_object_reduction_percentage"))||!Function->GetMetaData(TEXT("CPP_Default_collision_object_reduction_percentage")).Equals(TEXT("20"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_maximum_collision_particles"))||!Function->GetMetaData(TEXT("CPP_Default_maximum_collision_particles")).Equals(TEXT("60"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_collision_implicit_type"))||!Function->GetMetaData(TEXT("CPP_Default_collision_implicit_type")).Equals(TEXT("\"convex\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_remove_on_sleep"))||!Function->GetMetaData(TEXT("CPP_Default_remove_on_sleep")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_remove_on_break"))||!Function->GetMetaData(TEXT("CPP_Default_remove_on_break")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_sleep_time_seconds"))||!Function->GetMetaData(TEXT("CPP_Default_sleep_time_seconds")).Equals(TEXT("5"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_removal_duration_seconds"))||!Function->GetMetaData(TEXT("CPP_Default_removal_duration_seconds")).Equals(TEXT("1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_break_timer_seconds"))||!Function->GetMetaData(TEXT("CPP_Default_break_timer_seconds")).Equals(TEXT("1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_root_mesh_proxy_static_mesh_path"))||!Function->GetMetaData(TEXT("CPP_Default_root_mesh_proxy_static_mesh_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_overwrite_existing"))||!Function->GetMetaData(TEXT("CPP_Default_overwrite_existing")).Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
        }
        if(Which==2){
            if(Function->HasMetaData(TEXT("CPP_Default_geometry_collection_actor_name"))){Error=TEXT("Required public field has reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_field_actor_name"))||!Function->GetMetaData(TEXT("CPP_Default_field_actor_name")).Equals(TEXT("\"Chaos_AnchorField\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_box_center"))||!Function->GetMetaData(TEXT("CPP_Default_box_center")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_box_extent"))||!Function->GetMetaData(TEXT("CPP_Default_box_extent")).Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_field_magnitude"))||!Function->GetMetaData(TEXT("CPP_Default_field_magnitude")).Equals(TEXT("1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_save_level"))||!Function->GetMetaData(TEXT("CPP_Default_save_level")).Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON carrier default required");return false;}
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

bool ChaosGeometryAuthoringSchema(const FString& Physical,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Tools=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Physical),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||!Name.Equals(CanonicalName,ESearchCase::CaseSensitive)||!Root->TryGetArrayField(TEXT("tools"),Tools)||!Tools||Tools->Num()!=3){Error=TEXT("Exact canonical ChaosGeometryAuthoring Toolset3 required");return false;}
    TSet<int32> Seen;
    for(const auto& Value:*Tools){
        if(!Value.IsValid()||Value->Type!=EJson::Object){return false;}auto Tool=Value->AsObject();if(!Tool->TryGetStringField(TEXT("name"),Name)){return false;}
        int32 Which=INDEX_NONE;for(int32 I=0;I<3;++I){if(Name.Equals(FString(CanonicalName)+TEXT(".")+Methods[I],ESearchCase::CaseSensitive)){Which=I;}}
        if(Which==INDEX_NONE||Seen.Contains(Which)){Error=TEXT("Exactly three distinct Chaos geometry authoring methods required");return false;}Seen.Add(Which);
        const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;
        if(!Tool->TryGetObjectField(TEXT("inputSchema"),Input)||!Type(*Input,TEXT("object"))){return false;}
        if(!(*Input)->TryGetObjectField(TEXT("properties"),Props)||(*Props)->Values.Num()!=Layouts[Which].Count){Error=TEXT("Exact physical Chaos geometry authoring fields required");return false;}
        if(Which==0){if(!RequiredOrAbsentEmpty(*Input,{TEXT("source_static_mesh_paths"),TEXT("output_geometry_collection_path")})){Error=TEXT("Physical required2/2/1 differs");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("source_static_mesh_paths"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("output_geometry_collection_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("source_transforms"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("root_mesh_proxy_static_mesh_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("material_reindex"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("overwrite_existing"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==1){if(!RequiredOrAbsentEmpty(*Input,{TEXT("source_geometry_collection_path"),TEXT("output_geometry_collection_path")})){Error=TEXT("Physical required2/2/1 differs");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("source_geometry_collection_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("output_geometry_collection_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("fracture_method"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"uniform_voronoi\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("site_count"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("64"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("random_seed"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("1337"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("cluster_levels"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("2"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("cluster_size"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("8"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("damage_thresholds"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("merge_tiny_geometry"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("tiny_relative_threshold"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0.01"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("collision_object_reduction_percentage"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("20"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("maximum_collision_particles"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("60"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("collision_implicit_type"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"convex\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("remove_on_sleep"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("remove_on_break"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("sleep_time_seconds"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("5"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("removal_duration_seconds"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("break_timer_seconds"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("root_mesh_proxy_static_mesh_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("overwrite_existing"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("false"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==2){if(!RequiredOrAbsentEmpty(*Input,{TEXT("geometry_collection_actor_name")})){Error=TEXT("Physical required2/2/1 differs");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("geometry_collection_actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("field_actor_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"Chaos_AnchorField\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("box_center"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("box_extent"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("null"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("field_magnitude"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("1"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!(*Props)->TryGetObjectField(TEXT("save_level"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("true"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        TSharedPtr<FJsonObject> OriginalInput;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LogicalInputSchemas[Which]),OriginalInput)||!OriginalInput.IsValid()){Error=TEXT("Frozen original input schema invalid");return false;}
        Tool->SetObjectField(TEXT("inputSchema"),OriginalInput.ToSharedRef());
        if(!Output11(Tool)){Error=TEXT("Exact standard11 output required for Chaos geometry authorings2");return false;}
    }Logical=Json(Root.ToSharedRef());return true;
}

class FChaosGeometryAuthoringToolset final : public FToolset
{
public:
    explicit FChaosGeometryAuthoringToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("chaos_geometry_authoring_raw_guard_unavailable"), TEXT("A live active game-thread ChaosGeometryAuthoring guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("chaos_geometry_authoring_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("chaos_geometry_authoring_raw_guard_definition_changed"), Error); }
        const int32 Which=MethodIndex(Method);
        if (Which==INDEX_NONE)
        { return Reject(TEXT("chaos_geometry_authoring_method_refused"), TEXT("Only the four exact original ChaosGeometryAuthoring methods are guarded")); }
        FString PackedInput;
        if (!RawTransform(Which, Input, PackedInput, Error))
        { return Reject(TEXT("chaos_geometry_authoring_raw_arguments_invalid"), Error); }
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
            if (Error.IsEmpty()) { Error = TEXT("ChaosGeometryAuthoring ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("chaos_geometry_authoring_raw_guard_changed_before_dispatch"), Error);
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
    Report->SetStringField(TEXT("guard_scope"),TEXT("Dedicated native Geometry Collection conversion, deterministic fracture and serialized anchor authoring. Public6/20/6 required2/2/1, original nested Any/nullable arrays and raw hidden0/4/1 remain distinct. Preserve source bytes/dirty, strict and spatial hashes, reload, actual partial save/cleanup, Named anchor default1 versus raw omitted Kinematic2, original post-mutation rejection prefix. Original legacy vacuous or assertion-gated saved leaves never replace actual SDK save. Hidden inspect_saved remains a raw observer. No automatic retry, atomic rollback, Engine global purity, cold reload, physics, packaging or pipeline completion claim."));
    for(const TCHAR* Key : {TEXT("active"),TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("ordinary_string9_schema_verified"),TEXT("integer6_number5_schema_verified"),TEXT("array5_nestedAny_schema_verified"),TEXT("nullable_array4_schema_verified"),TEXT("chaos_geometry_authoring_public6_20_6_schema_verified"),TEXT("required2_2_1_default4_18_5_verified"),TEXT("json_value_carrier32_schema_verified")}){Report->SetBoolField(Key,false);}
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured ChaosGeometryAuthoring guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured ChaosGeometryAuthoring wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!ChaosGeometryAuthoringSchema(CurrentSchema, CurrentLogical, Error) || CurrentSchema != State->ReflectedSchema || CurrentLogical != State->FullSchema)
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original ChaosGeometryAuthoring schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("ChaosGeometryAuthoring wrapper owner/class changed during reflected schema callbacks"); } return false; }
    for(const TCHAR* Key : {TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("ordinary_string9_schema_verified"),TEXT("integer6_number5_schema_verified"),TEXT("array5_nestedAny_schema_verified"),TEXT("nullable_array4_schema_verified"),TEXT("chaos_geometry_authoring_public6_20_6_schema_verified"),TEXT("required2_2_1_default4_18_5_verified"),TEXT("json_value_carrier32_schema_verified")}){Report->SetBoolField(Key,true);}
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("ChaosGeometryAuthoring guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered ChaosGeometryAuthoring wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered ChaosGeometryAuthoring logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned ChaosGeometryAuthoring guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool InstallChaosGeometryAuthoringToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("ChaosGeometryAuthoring guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained ChaosGeometryAuthoring owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!ChaosGeometryAuthoringSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FChaosGeometryAuthoringToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original ChaosGeometryAuthoring handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("ChaosGeometryAuthoring raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyChaosGeometryAuthoringToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("ChaosGeometryAuthoring guard verification requires an idle live editor game thread"); return false; }
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
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named ChaosGeometryAuthoring delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed ChaosGeometryAuthoring raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original ChaosGeometryAuthoring registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveChaosGeometryAuthoringToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("ChaosGeometryAuthoring guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained ChaosGeometryAuthoring class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownChaosGeometryAuthoringToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
