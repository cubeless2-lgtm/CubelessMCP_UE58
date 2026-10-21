#include "Registration/CubelessTextureMaterialToolset.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Future.h"
#include "Containers/Ticker.h"
#include "CubelessTextureMaterialAttachmentLibrary.h"
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
constexpr const TCHAR* CanonicalName = TEXT("cubeless_mcp_ue58.registration.texture_material_authoring_toolset.CubelessTextureMaterialTools");
constexpr const TCHAR* CanonicalClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/texture_material_authoring_toolset_PY.CubelessTextureMaterialTools");
constexpr const TCHAR* Methods[]={TEXT("get_static_mesh_uv_layout"),TEXT("create_material_instance_with_texture"),TEXT("create_material_with_texture_set"),TEXT("import_create_and_apply_texture_set"),TEXT("generate_pbr_companion_textures"),TEXT("import_texture_to_unreal"),TEXT("import_texture_set_to_unreal"),TEXT("apply_material_to_mesh"),TEXT("generate_texture_from_prompt"),TEXT("generate_texture_for_mesh_uv"),TEXT("generate_and_apply_ai_texture")};
constexpr const TCHAR* Fields0[]={TEXT("mesh_path"),TEXT("uv_channel"),TEXT("output_path")};
constexpr const TCHAR* Fields1[]={TEXT("texture_asset_path"),TEXT("material_name"),TEXT("unreal_folder"),TEXT("base_material_path")};
constexpr const TCHAR* Fields2[]={TEXT("base_color_texture_asset_path"),TEXT("material_name"),TEXT("unreal_folder"),TEXT("normal_texture_asset_path"),TEXT("roughness_texture_asset_path"),TEXT("ambient_occlusion_texture_asset_path"),TEXT("metallic_texture_asset_path"),TEXT("base_material_path")};
constexpr const TCHAR* Fields3[]={TEXT("base_color_image_path"),TEXT("output_name"),TEXT("unreal_folder"),TEXT("normal_image_path"),TEXT("roughness_image_path"),TEXT("ambient_occlusion_image_path"),TEXT("metallic_image_path"),TEXT("target"),TEXT("material_slot"),TEXT("base_material_path")};
constexpr const TCHAR* Fields4[]={TEXT("base_color_image_path"),TEXT("output_name"),TEXT("output_dir"),TEXT("normal_strength"),TEXT("roughness"),TEXT("roughness_detail"),TEXT("ambient_occlusion_strength"),TEXT("metallic")};
constexpr const TCHAR* Fields5[]={TEXT("image_path"),TEXT("unreal_folder"),TEXT("asset_name")};
constexpr const TCHAR* Fields6[]={TEXT("base_color_image_path"),TEXT("unreal_folder"),TEXT("asset_name"),TEXT("normal_image_path"),TEXT("roughness_image_path"),TEXT("ambient_occlusion_image_path"),TEXT("metallic_image_path")};
constexpr const TCHAR* Fields7[]={TEXT("target"),TEXT("material_asset_path"),TEXT("material_slot")};
constexpr const TCHAR* Fields8[]={TEXT("prompt"),TEXT("output_name"),TEXT("output_dir"),TEXT("size")};
constexpr const TCHAR* Fields9[]={TEXT("mesh_path"),TEXT("prompt"),TEXT("uv_channel"),TEXT("output_name"),TEXT("output_dir"),TEXT("size")};
constexpr const TCHAR* Fields10[]={TEXT("target"),TEXT("prompt"),TEXT("output_name"),TEXT("unreal_folder"),TEXT("uv_channel"),TEXT("size")};
struct FLayout{const TCHAR* const* Fields;int32 Count;};
constexpr FLayout Layouts[]={{Fields0,3},{Fields1,4},{Fields2,8},{Fields3,10},{Fields4,8},{Fields5,3},{Fields6,7},{Fields7,3},{Fields8,4},{Fields9,6},{Fields10,6}};
constexpr int32 MaxInputChars=8*1024*1024,MaxCarrierChars=1024*1024,MaxRawDepth=65,MaxValueSlots=262144;
int32 MethodIndex(const FString& Method){for(int32 I=0;I<UE_ARRAY_COUNT(Methods);++I){if(Method.Equals(Methods[I],ESearchCase::CaseSensitive)){return I;}}return INDEX_NONE;}
constexpr const TCHAR* LogicalInputSchemas[]={TEXT("{\"properties\":{\"mesh_path\":{\"title\":\"Mesh Path\",\"type\":\"string\"},\"uv_channel\":{\"default\":0,\"title\":\"Uv Channel\",\"type\":\"integer\"},\"output_path\":{\"default\":\"\",\"title\":\"Output Path\",\"type\":\"string\"}},\"required\":[\"mesh_path\"],\"title\":\"get_static_mesh_uv_layoutArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"texture_asset_path\":{\"title\":\"Texture Asset Path\",\"type\":\"string\"},\"material_name\":{\"title\":\"Material Name\",\"type\":\"string\"},\"unreal_folder\":{\"title\":\"Unreal Folder\",\"type\":\"string\"},\"base_material_path\":{\"default\":\"\",\"title\":\"Base Material Path\",\"type\":\"string\"}},\"required\":[\"texture_asset_path\",\"material_name\",\"unreal_folder\"],\"title\":\"create_material_instance_with_textureArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"base_color_texture_asset_path\":{\"title\":\"Base Color Texture Asset Path\",\"type\":\"string\"},\"material_name\":{\"title\":\"Material Name\",\"type\":\"string\"},\"unreal_folder\":{\"title\":\"Unreal Folder\",\"type\":\"string\"},\"normal_texture_asset_path\":{\"default\":\"\",\"title\":\"Normal Texture Asset Path\",\"type\":\"string\"},\"roughness_texture_asset_path\":{\"default\":\"\",\"title\":\"Roughness Texture Asset Path\",\"type\":\"string\"},\"ambient_occlusion_texture_asset_path\":{\"default\":\"\",\"title\":\"Ambient Occlusion Texture Asset Path\",\"type\":\"string\"},\"metallic_texture_asset_path\":{\"default\":\"\",\"title\":\"Metallic Texture Asset Path\",\"type\":\"string\"},\"base_material_path\":{\"default\":\"\",\"title\":\"Base Material Path\",\"type\":\"string\"}},\"required\":[\"base_color_texture_asset_path\",\"material_name\",\"unreal_folder\"],\"title\":\"create_material_with_texture_setArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"base_color_image_path\":{\"title\":\"Base Color Image Path\",\"type\":\"string\"},\"output_name\":{\"title\":\"Output Name\",\"type\":\"string\"},\"unreal_folder\":{\"title\":\"Unreal Folder\",\"type\":\"string\",\"default\":\"/Game/AI_Generated\"},\"normal_image_path\":{\"title\":\"Normal Image Path\",\"type\":\"string\",\"default\":\"\"},\"roughness_image_path\":{\"title\":\"Roughness Image Path\",\"type\":\"string\",\"default\":\"\"},\"ambient_occlusion_image_path\":{\"title\":\"Ambient Occlusion Image Path\",\"type\":\"string\",\"default\":\"\"},\"metallic_image_path\":{\"title\":\"Metallic Image Path\",\"type\":\"string\",\"default\":\"\"},\"target\":{\"title\":\"Target\",\"type\":\"string\",\"default\":\"\"},\"material_slot\":{\"title\":\"Material Slot\",\"type\":\"integer\",\"default\":0},\"base_material_path\":{\"title\":\"Base Material Path\",\"type\":\"string\",\"default\":\"\"}},\"required\":[\"base_color_image_path\",\"output_name\"],\"title\":\"import_create_and_apply_texture_setArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"base_color_image_path\":{\"title\":\"Base Color Image Path\",\"type\":\"string\"},\"output_name\":{\"default\":\"\",\"title\":\"Output Name\",\"type\":\"string\"},\"output_dir\":{\"default\":\"\",\"title\":\"Output Dir\",\"type\":\"string\"},\"normal_strength\":{\"default\":2.0,\"title\":\"Normal Strength\",\"type\":\"number\"},\"roughness\":{\"default\":0.75,\"title\":\"Roughness\",\"type\":\"number\"},\"roughness_detail\":{\"default\":0.15,\"title\":\"Roughness Detail\",\"type\":\"number\"},\"ambient_occlusion_strength\":{\"default\":0.35,\"title\":\"Ambient Occlusion Strength\",\"type\":\"number\"},\"metallic\":{\"default\":0.0,\"title\":\"Metallic\",\"type\":\"number\"}},\"required\":[\"base_color_image_path\"],\"title\":\"generate_pbr_companion_texturesArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"image_path\":{\"title\":\"Image Path\",\"type\":\"string\"},\"unreal_folder\":{\"title\":\"Unreal Folder\",\"type\":\"string\"},\"asset_name\":{\"title\":\"Asset Name\",\"type\":\"string\"}},\"required\":[\"image_path\",\"unreal_folder\",\"asset_name\"],\"title\":\"import_texture_to_unrealArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"base_color_image_path\":{\"title\":\"Base Color Image Path\",\"type\":\"string\"},\"unreal_folder\":{\"title\":\"Unreal Folder\",\"type\":\"string\"},\"asset_name\":{\"title\":\"Asset Name\",\"type\":\"string\"},\"normal_image_path\":{\"default\":\"\",\"title\":\"Normal Image Path\",\"type\":\"string\"},\"roughness_image_path\":{\"default\":\"\",\"title\":\"Roughness Image Path\",\"type\":\"string\"},\"ambient_occlusion_image_path\":{\"default\":\"\",\"title\":\"Ambient Occlusion Image Path\",\"type\":\"string\"},\"metallic_image_path\":{\"default\":\"\",\"title\":\"Metallic Image Path\",\"type\":\"string\"}},\"required\":[\"base_color_image_path\",\"unreal_folder\",\"asset_name\"],\"title\":\"import_texture_set_to_unrealArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"target\":{\"title\":\"Target\",\"type\":\"string\"},\"material_asset_path\":{\"title\":\"Material Asset Path\",\"type\":\"string\"},\"material_slot\":{\"default\":0,\"title\":\"Material Slot\",\"type\":\"integer\"}},\"required\":[\"target\",\"material_asset_path\"],\"title\":\"apply_material_to_meshArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"prompt\":{\"title\":\"Prompt\",\"type\":\"string\"},\"output_name\":{\"title\":\"Output Name\",\"type\":\"string\"},\"output_dir\":{\"title\":\"Output Dir\",\"type\":\"string\"},\"size\":{\"default\":\"1024x1024\",\"title\":\"Size\",\"type\":\"string\"}},\"required\":[\"prompt\",\"output_name\",\"output_dir\"],\"title\":\"generate_texture_from_promptArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"mesh_path\":{\"title\":\"Mesh Path\",\"type\":\"string\"},\"prompt\":{\"title\":\"Prompt\",\"type\":\"string\"},\"uv_channel\":{\"default\":0,\"title\":\"Uv Channel\",\"type\":\"integer\"},\"output_name\":{\"default\":\"T_AI_Texture\",\"title\":\"Output Name\",\"type\":\"string\"},\"output_dir\":{\"default\":\"\",\"title\":\"Output Dir\",\"type\":\"string\"},\"size\":{\"default\":\"1024x1024\",\"title\":\"Size\",\"type\":\"string\"}},\"required\":[\"mesh_path\",\"prompt\"],\"title\":\"generate_texture_for_mesh_uvArguments\",\"type\":\"object\"}"),TEXT("{\"properties\":{\"target\":{\"title\":\"Target\",\"type\":\"string\"},\"prompt\":{\"title\":\"Prompt\",\"type\":\"string\"},\"output_name\":{\"title\":\"Output Name\",\"type\":\"string\"},\"unreal_folder\":{\"default\":\"/Game/AI_Generated\",\"title\":\"Unreal Folder\",\"type\":\"string\"},\"uv_channel\":{\"default\":0,\"title\":\"Uv Channel\",\"type\":\"integer\"},\"size\":{\"default\":\"1024x1024\",\"title\":\"Size\",\"type\":\"string\"}},\"required\":[\"target\",\"prompt\",\"output_name\"],\"title\":\"generate_and_apply_ai_textureArguments\",\"type\":\"object\"}")};
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
    TAtomic<bool> ExecutingStandaloneTicker { false };
    TAtomic<int32> QueuedMethodIndex { INDEX_NONE };
    FTSTicker::FDelegateHandle Ticker;
    TSharedPtr<TPromise<FOutput>,ESPMode::ThreadSafe> PendingPromise;
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
    R->SetStringField(TEXT("stage"), TEXT("texture_material_authoring_raw_pre_dispatch"));
    R->SetStringField(TEXT("error_code"), Code);
    R->SetStringField(TEXT("error"), Message);
    R->SetStringField(TEXT("changes_json"), TEXT("[]"));
    auto Compile = MakeShared<FJsonObject>();
    Compile->SetBoolField(TEXT("requested"), false); Compile->SetBoolField(TEXT("attempted"), false);
    Compile->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Compile->SetStringField(TEXT("status"), TEXT("texture_material_authoring_raw_not_dispatched"));
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
 TSharedPtr<FJsonObject> Args;if(Which<0||Which>=UE_ARRAY_COUNT(Methods)||!BoundText(Input,MaxInputChars)||!RawDepth(Input)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid()||!FiniteValues(Args.ToSharedRef())){Error=TEXT("Texture/material authoring requires a bounded finite public object");return false;}
 auto Packed=MakeShared<FJsonObject>();
 if(Which==0){
 {auto Value=Args->TryGetField(TEXT("mesh_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: mesh_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("mesh_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("uv_channel"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("uv_channel"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_path"),Carrier);}
 }
 if(Which==1){
 {auto Value=Args->TryGetField(TEXT("texture_asset_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: texture_asset_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("texture_asset_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("material_name"));if(!Value.IsValid()){Error=TEXT("Missing required public field: material_name");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("material_name"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("unreal_folder"));if(!Value.IsValid()){Error=TEXT("Missing required public field: unreal_folder");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("unreal_folder"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("base_material_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("base_material_path"),Carrier);}
 }
 if(Which==2){
 {auto Value=Args->TryGetField(TEXT("base_color_texture_asset_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: base_color_texture_asset_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("base_color_texture_asset_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("material_name"));if(!Value.IsValid()){Error=TEXT("Missing required public field: material_name");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("material_name"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("unreal_folder"));if(!Value.IsValid()){Error=TEXT("Missing required public field: unreal_folder");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("unreal_folder"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("normal_texture_asset_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("normal_texture_asset_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("roughness_texture_asset_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("roughness_texture_asset_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("ambient_occlusion_texture_asset_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("ambient_occlusion_texture_asset_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("metallic_texture_asset_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("metallic_texture_asset_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("base_material_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("base_material_path"),Carrier);}
 }
 if(Which==3){
 {auto Value=Args->TryGetField(TEXT("base_color_image_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: base_color_image_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("base_color_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_name"));if(!Value.IsValid()){Error=TEXT("Missing required public field: output_name");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_name"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("unreal_folder"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"/Game/AI_Generated\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("unreal_folder"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("normal_image_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("normal_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("roughness_image_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("roughness_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("ambient_occlusion_image_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("ambient_occlusion_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("metallic_image_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("metallic_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("target"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("target"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("material_slot"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("material_slot"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("base_material_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("base_material_path"),Carrier);}
 }
 if(Which==4){
 {auto Value=Args->TryGetField(TEXT("base_color_image_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: base_color_image_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("base_color_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_name"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_dir"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_dir"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("normal_strength"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[2.0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("normal_strength"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("roughness"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0.75]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("roughness"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("roughness_detail"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0.15]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("roughness_detail"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("ambient_occlusion_strength"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0.35]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("ambient_occlusion_strength"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("metallic"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0.0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("metallic"),Carrier);}
 }
 if(Which==5){
 {auto Value=Args->TryGetField(TEXT("image_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: image_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("unreal_folder"));if(!Value.IsValid()){Error=TEXT("Missing required public field: unreal_folder");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("unreal_folder"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("asset_name"));if(!Value.IsValid()){Error=TEXT("Missing required public field: asset_name");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("asset_name"),Carrier);}
 }
 if(Which==6){
 {auto Value=Args->TryGetField(TEXT("base_color_image_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: base_color_image_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("base_color_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("unreal_folder"));if(!Value.IsValid()){Error=TEXT("Missing required public field: unreal_folder");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("unreal_folder"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("asset_name"));if(!Value.IsValid()){Error=TEXT("Missing required public field: asset_name");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("asset_name"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("normal_image_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("normal_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("roughness_image_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("roughness_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("ambient_occlusion_image_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("ambient_occlusion_image_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("metallic_image_path"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("metallic_image_path"),Carrier);}
 }
 if(Which==7){
 {auto Value=Args->TryGetField(TEXT("target"));if(!Value.IsValid()){Error=TEXT("Missing required public field: target");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("target"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("material_asset_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: material_asset_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("material_asset_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("material_slot"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("material_slot"),Carrier);}
 }
 if(Which==8){
 {auto Value=Args->TryGetField(TEXT("prompt"));if(!Value.IsValid()){Error=TEXT("Missing required public field: prompt");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("prompt"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_name"));if(!Value.IsValid()){Error=TEXT("Missing required public field: output_name");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_name"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_dir"));if(!Value.IsValid()){Error=TEXT("Missing required public field: output_dir");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_dir"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("size"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"1024x1024\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("size"),Carrier);}
 }
 if(Which==9){
 {auto Value=Args->TryGetField(TEXT("mesh_path"));if(!Value.IsValid()){Error=TEXT("Missing required public field: mesh_path");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("mesh_path"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("prompt"));if(!Value.IsValid()){Error=TEXT("Missing required public field: prompt");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("prompt"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("uv_channel"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("uv_channel"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_name"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"T_AI_Texture\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_name"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_dir"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_dir"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("size"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"1024x1024\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("size"),Carrier);}
 }
 if(Which==10){
 {auto Value=Args->TryGetField(TEXT("target"));if(!Value.IsValid()){Error=TEXT("Missing required public field: target");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("target"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("prompt"));if(!Value.IsValid()){Error=TEXT("Missing required public field: prompt");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("prompt"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("output_name"));if(!Value.IsValid()){Error=TEXT("Missing required public field: output_name");return false;}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("output_name"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("unreal_folder"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"/Game/AI_Generated\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("unreal_folder"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("uv_channel"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[0]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("uv_channel"),Carrier);}
 {auto Value=Args->TryGetField(TEXT("size"));if(!Value.IsValid()){TArray<TSharedPtr<FJsonValue>> Defaults;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[\"1024x1024\"]")),Defaults,FJsonSerializer::EFlags::StoreNumbersAsStrings)||Defaults.Num()!=1){Error=TEXT("Frozen public default unavailable");return false;}Value=Defaults[0];}FString Carrier;if(!SerializePublicValue(Value,Carrier,Error))return false;Packed->SetStringField(TEXT("size"),Carrier);}
 }
 PackedInput=Json(Packed);return true;
}

bool Metadata(UClass* Class,FString& Error)
{
    if(!IsValid(Class)||Class->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)||Class->HasAnyClassFlags(CLASS_NewerVersionExists)||!Class->GetPathName().Equals(CanonicalClass,ESearchCase::CaseSensitive)||!Class->IsChildOf(UToolsetDefinition::StaticClass())){Error=TEXT("Exact live canonical TextureMaterial class required");return false;}
    for(int32 Which=0;Which<UE_ARRAY_COUNT(Methods);++Which){
        UFunction* Function=Class->FindFunctionByName(FName(Methods[Which]));if(!Function||Function->GetOuter()!=Class||!Function->GetName().Equals(Methods[Which],ESearchCase::CaseSensitive)){Error=TEXT("Exact owned TextureMaterial method required");return false;}
        auto Callable=UToolsetDefinition::IsFunctionAICallable(TObjectPtr<const UFunction>(Function));if(!Callable.HasValue()||!Callable.GetValue()){Error=TEXT("Officially callable TextureMaterial method required");return false;}
        const auto& Layout=Layouts[Which];int32 Index=0;
        for(TFieldIterator<FProperty> It(Function,EFieldIteratorFlags::ExcludeSuper);It;++It){if(!It->HasAnyPropertyFlags(CPF_Parm)||It->HasAnyPropertyFlags(CPF_ReturnParm)){continue;}if(Index>=Layout.Count||!It->GetName().Equals(Layout.Fields[Index],ESearchCase::CaseSensitive)||It->GetOwnerStruct()!=Function||It->ArrayDim!=1||It->HasAnyPropertyFlags(CPF_OutParm|CPF_ReferenceParm)||!CastField<FStrProperty>(*It)){Error=TEXT("Exact physical public JSON FString method fields required");return false;}++Index;}
        if(Index!=Layout.Count){Error=TEXT("Exact physical arity required");return false;}
        if(Which==0){
            if(Function->HasMetaData(TEXT("CPP_Default_mesh_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_uv_channel"))||!Function->GetMetaData(TEXT("CPP_Default_uv_channel")).Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_output_path"))||!Function->GetMetaData(TEXT("CPP_Default_output_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==1){
            if(Function->HasMetaData(TEXT("CPP_Default_texture_asset_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_material_name"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_unreal_folder"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_base_material_path"))||!Function->GetMetaData(TEXT("CPP_Default_base_material_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==2){
            if(Function->HasMetaData(TEXT("CPP_Default_base_color_texture_asset_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_material_name"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_unreal_folder"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_normal_texture_asset_path"))||!Function->GetMetaData(TEXT("CPP_Default_normal_texture_asset_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_roughness_texture_asset_path"))||!Function->GetMetaData(TEXT("CPP_Default_roughness_texture_asset_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_ambient_occlusion_texture_asset_path"))||!Function->GetMetaData(TEXT("CPP_Default_ambient_occlusion_texture_asset_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_metallic_texture_asset_path"))||!Function->GetMetaData(TEXT("CPP_Default_metallic_texture_asset_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_base_material_path"))||!Function->GetMetaData(TEXT("CPP_Default_base_material_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==3){
            if(Function->HasMetaData(TEXT("CPP_Default_base_color_image_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_output_name"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_unreal_folder"))||!Function->GetMetaData(TEXT("CPP_Default_unreal_folder")).Equals(TEXT("\"/Game/AI_Generated\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_normal_image_path"))||!Function->GetMetaData(TEXT("CPP_Default_normal_image_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_roughness_image_path"))||!Function->GetMetaData(TEXT("CPP_Default_roughness_image_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_ambient_occlusion_image_path"))||!Function->GetMetaData(TEXT("CPP_Default_ambient_occlusion_image_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_metallic_image_path"))||!Function->GetMetaData(TEXT("CPP_Default_metallic_image_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_target"))||!Function->GetMetaData(TEXT("CPP_Default_target")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_material_slot"))||!Function->GetMetaData(TEXT("CPP_Default_material_slot")).Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_base_material_path"))||!Function->GetMetaData(TEXT("CPP_Default_base_material_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==4){
            if(Function->HasMetaData(TEXT("CPP_Default_base_color_image_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_output_name"))||!Function->GetMetaData(TEXT("CPP_Default_output_name")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_output_dir"))||!Function->GetMetaData(TEXT("CPP_Default_output_dir")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_normal_strength"))||!Function->GetMetaData(TEXT("CPP_Default_normal_strength")).Equals(TEXT("2.0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_roughness"))||!Function->GetMetaData(TEXT("CPP_Default_roughness")).Equals(TEXT("0.75"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_roughness_detail"))||!Function->GetMetaData(TEXT("CPP_Default_roughness_detail")).Equals(TEXT("0.15"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_ambient_occlusion_strength"))||!Function->GetMetaData(TEXT("CPP_Default_ambient_occlusion_strength")).Equals(TEXT("0.35"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_metallic"))||!Function->GetMetaData(TEXT("CPP_Default_metallic")).Equals(TEXT("0.0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==5){
            if(Function->HasMetaData(TEXT("CPP_Default_image_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_unreal_folder"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_asset_name"))){Error=TEXT("Required public field has a reflected default");return false;}
        }
        if(Which==6){
            if(Function->HasMetaData(TEXT("CPP_Default_base_color_image_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_unreal_folder"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_asset_name"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_normal_image_path"))||!Function->GetMetaData(TEXT("CPP_Default_normal_image_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_roughness_image_path"))||!Function->GetMetaData(TEXT("CPP_Default_roughness_image_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_ambient_occlusion_image_path"))||!Function->GetMetaData(TEXT("CPP_Default_ambient_occlusion_image_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_metallic_image_path"))||!Function->GetMetaData(TEXT("CPP_Default_metallic_image_path")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==7){
            if(Function->HasMetaData(TEXT("CPP_Default_target"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_material_asset_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_material_slot"))||!Function->GetMetaData(TEXT("CPP_Default_material_slot")).Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==8){
            if(Function->HasMetaData(TEXT("CPP_Default_prompt"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_output_name"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_output_dir"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_size"))||!Function->GetMetaData(TEXT("CPP_Default_size")).Equals(TEXT("\"1024x1024\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==9){
            if(Function->HasMetaData(TEXT("CPP_Default_mesh_path"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_prompt"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_uv_channel"))||!Function->GetMetaData(TEXT("CPP_Default_uv_channel")).Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_output_name"))||!Function->GetMetaData(TEXT("CPP_Default_output_name")).Equals(TEXT("\"T_AI_Texture\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_output_dir"))||!Function->GetMetaData(TEXT("CPP_Default_output_dir")).Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_size"))||!Function->GetMetaData(TEXT("CPP_Default_size")).Equals(TEXT("\"1024x1024\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
        }
        if(Which==10){
            if(Function->HasMetaData(TEXT("CPP_Default_target"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_prompt"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(Function->HasMetaData(TEXT("CPP_Default_output_name"))){Error=TEXT("Required public field has a reflected default");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_unreal_folder"))||!Function->GetMetaData(TEXT("CPP_Default_unreal_folder")).Equals(TEXT("\"/Game/AI_Generated\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_uv_channel"))||!Function->GetMetaData(TEXT("CPP_Default_uv_channel")).Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
            if(!Function->HasMetaData(TEXT("CPP_Default_size"))||!Function->GetMetaData(TEXT("CPP_Default_size")).Equals(TEXT("\"1024x1024\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default required");return false;}
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

bool TextureMaterialSchema(const FString& Physical,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Tools=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Physical),Root)||!Root.IsValid()||!Root->TryGetStringField(TEXT("name"),Name)||!Name.Equals(CanonicalName,ESearchCase::CaseSensitive)||!Root->TryGetArrayField(TEXT("tools"),Tools)||!Tools||Tools->Num()!=UE_ARRAY_COUNT(Methods)){Error=TEXT("Exact canonical tool count and identity required");return false;}
    TSet<int32> Seen;
    for(const auto& Value:*Tools){
        if(!Value.IsValid()||Value->Type!=EJson::Object){return false;}auto Tool=Value->AsObject();if(!Tool.IsValid()||!Tool->TryGetStringField(TEXT("name"),Name)){return false;}
        int32 Which=INDEX_NONE;for(int32 I=0;I<UE_ARRAY_COUNT(Methods);++I){if(Name.Equals(FString(CanonicalName)+TEXT(".")+Methods[I],ESearchCase::CaseSensitive)){Which=I;}}
        if(Which==INDEX_NONE||Seen.Contains(Which)){Error=TEXT("Each exact original method is required once");return false;}Seen.Add(Which);
        const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;
        if(!Tool->TryGetObjectField(TEXT("inputSchema"),Input)||!Type(*Input,TEXT("object"))){return false;}
        if((!(*Input)->TryGetObjectField(TEXT("properties"),Props)&&Layouts[Which].Count!=0)||(Props&&(*Props)->Values.Num()!=Layouts[Which].Count)){Error=TEXT("Exact physical field set required");return false;}
        if(Which==0){if(!RequiredOrAbsentEmpty(*Input,{TEXT("mesh_path")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("mesh_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("uv_channel"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==1){if(!RequiredOrAbsentEmpty(*Input,{TEXT("texture_asset_path"),TEXT("material_name"),TEXT("unreal_folder")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("texture_asset_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("material_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("unreal_folder"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("base_material_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==2){if(!RequiredOrAbsentEmpty(*Input,{TEXT("base_color_texture_asset_path"),TEXT("material_name"),TEXT("unreal_folder")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("base_color_texture_asset_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("material_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("unreal_folder"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("normal_texture_asset_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("roughness_texture_asset_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("ambient_occlusion_texture_asset_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("metallic_texture_asset_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("base_material_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==3){if(!RequiredOrAbsentEmpty(*Input,{TEXT("base_color_image_path"),TEXT("output_name")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("base_color_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("unreal_folder"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"/Game/AI_Generated\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("normal_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("roughness_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("ambient_occlusion_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("metallic_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("target"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("material_slot"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("base_material_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==4){if(!RequiredOrAbsentEmpty(*Input,{TEXT("base_color_image_path")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("base_color_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_dir"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("normal_strength"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("2.0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("roughness"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0.75"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("roughness_detail"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0.15"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("ambient_occlusion_strength"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0.35"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("metallic"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0.0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==5){if(!RequiredOrAbsentEmpty(*Input,{TEXT("image_path"),TEXT("unreal_folder"),TEXT("asset_name")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("unreal_folder"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("asset_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
        }
        if(Which==6){if(!RequiredOrAbsentEmpty(*Input,{TEXT("base_color_image_path"),TEXT("unreal_folder"),TEXT("asset_name")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("base_color_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("unreal_folder"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("asset_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("normal_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("roughness_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("ambient_occlusion_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("metallic_image_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==7){if(!RequiredOrAbsentEmpty(*Input,{TEXT("target"),TEXT("material_asset_path")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("target"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("material_asset_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("material_slot"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==8){if(!RequiredOrAbsentEmpty(*Input,{TEXT("prompt"),TEXT("output_name"),TEXT("output_dir")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("prompt"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_dir"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("size"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"1024x1024\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==9){if(!RequiredOrAbsentEmpty(*Input,{TEXT("mesh_path"),TEXT("prompt")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("mesh_path"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("prompt"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("uv_channel"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"T_AI_Texture\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_dir"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("size"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"1024x1024\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        if(Which==10){if(!RequiredOrAbsentEmpty(*Input,{TEXT("target"),TEXT("prompt"),TEXT("output_name")})){Error=TEXT("Physical required fields differ");return false;}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("target"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("prompt"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("output_name"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(Default.IsValid()){Error=TEXT("Required public field has physical default");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("unreal_folder"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"/Game/AI_Generated\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("uv_channel"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("0"),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
            {const TSharedPtr<FJsonObject>* Field=nullptr;if(!Props||!(*Props)->TryGetObjectField(TEXT("size"),Field)||!Type(*Field,TEXT("string"))){Error=TEXT("Physical JSON carrier FString required");return false;}auto Default=(*Field)->TryGetField(TEXT("default"));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().Equals(TEXT("\"1024x1024\""),ESearchCase::CaseSensitive)){Error=TEXT("Exact public JSON default differs");return false;}}
        }
        TSharedPtr<FJsonObject> OriginalInput;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LogicalInputSchemas[Which]),OriginalInput)||!OriginalInput.IsValid()){Error=TEXT("Frozen original input schema invalid");return false;}
        Tool->SetObjectField(TEXT("inputSchema"),OriginalInput.ToSharedRef());
        if(!Output11(Tool)){Error=TEXT("Exact standard11 output required");return false;}
    }Logical=Json(Root.ToSharedRef());return true;
}

class FTextureMaterialToolset final : public FToolset
{
public:
    explicit FTextureMaterialToolset(const TSharedRef<FAttachmentState>& InState) : State(InState) {}
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
        { return Reject(TEXT("texture_material_authoring_raw_guard_unavailable"), TEXT("A live active game-thread TextureMaterial guard is required; no delegate dispatched")); }
        UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
        if (!Subsystem || Subsystem->ToolsetRegistry.Find(CanonicalName).Get() != this)
        { return Reject(TEXT("texture_material_authoring_raw_guard_ownership_lost"), TEXT("Exact original-subsystem wrapper identity is unavailable; no delegate dispatched")); }
        FString Error;
        if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
        { State->Active.Store(false); State->Original->SetEnabled(false); return Reject(TEXT("texture_material_authoring_raw_guard_definition_changed"), Error); }
        const int32 Which=MethodIndex(Method);
        if (Which==INDEX_NONE)
        { return Reject(TEXT("texture_material_authoring_method_refused"), TEXT("Only the exact original TextureMaterial methods are guarded")); }
        FString PackedInput;
        if (!RawTransform(Which, Input, PackedInput, Error))
        { return Reject(TEXT("texture_material_authoring_raw_arguments_invalid"), Error); }
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
            if (Error.IsEmpty()) { Error = TEXT("TextureMaterial ownership/filter/class changed during local filter callbacks; no delegate dispatched"); }
            return Reject(TEXT("texture_material_authoring_raw_guard_changed_before_dispatch"), Error);
        }
        if(Which==3||Which==5||Which==6)
        {
            auto Promise=MakeShared<TPromise<FOutput>,ESPMode::ThreadSafe>();auto Future=Promise->GetFuture();
            CallState->PendingPromise=Promise;CallState->QueuedMethodIndex.Store(Which);
            CallState->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
                [CallState,OriginalLifetime,WrapperLifetime,Promise,Method,PackedInput,Which](float)->bool
                {
                    CallState->Ticker.Reset();CallState->QueuedMethodIndex.Store(INDEX_NONE);
                    if(!IsInGameThread()||IsEngineExitRequested()||IsGarbageCollecting())
                    {
                        CallState->Active.Store(false);CallState->DelegatedMethodIndex.Store(INDEX_NONE);CallState->InFlight.Store(0);CallState->PendingPromise.Reset();
                        Promise->SetValue(MakeValue(Rejection(TEXT("texture_material_standalone_tick_context_lost"),TEXT("Queued ticker did not enter a safe Editor context; Python/import not dispatched"),Method)));return false;
                    }
                    FString TickError;auto Current=UToolsetRegistrySubsystem::Get();
                    if(!CallState->Active.Load()||CallState->InFlight.Load()!=1||!Current.HasValue()||Current.GetValue().Get()!=CallState->Subsystem.Get()
                        ||Current.GetValue()->ToolsetRegistry.Find(CanonicalName)!=WrapperLifetime
                        ||!WrapperLifetime->IsToolEnabled(FString(CanonicalName)+TEXT(".")+Method)
                        ||!Metadata(CallState->Class.Get(),TickError)||OriginalLifetime->GetToolsetClass()!=CallState->Class.Get())
                    {
                        OriginalLifetime->SetEnabled(false);CallState->DelegatedMethodIndex.Store(INDEX_NONE);CallState->InFlight.Store(0);CallState->PendingPromise.Reset();
                        Promise->SetValue(MakeValue(Rejection(TEXT("texture_material_standalone_tick_not_dispatched"),TickError.IsEmpty()?TEXT("Owned live Named ticker boundary changed; Python/import not dispatched"):TickError,Method)));return false;
                    }
                    CallState->DelegatedMethodIndex.Store(Which);CallState->ExecutingStandaloneTicker.Store(true);
                    // The installed reflected handler invokes Python synchronously through ProcessEvent.
                    // Keep exact owner/class/future holds until its actual completion; no retry/query path.
                    auto Delegated=OriginalLifetime->ExecuteTool(Method,PackedInput);
                    CallState->ExecutingStandaloneTicker.Store(false);
                    Delegated.Next([CallState,OriginalLifetime,WrapperLifetime,Promise](FOutput&& Output)
                    {
                        if(IsInGameThread())OriginalLifetime->SetEnabled(false);else CallState->Active.Store(false);
                        CallState->DelegatedMethodIndex.Store(INDEX_NONE);CallState->InFlight.Store(0);CallState->PendingPromise.Reset();
                        Promise->SetValue(MoveTemp(Output));
                    });
                    return false;
                }),0.0f);
            if(!CallState->Ticker.IsValid())
            {
                OriginalLifetime->SetEnabled(false);CallState->PendingPromise.Reset();CallState->QueuedMethodIndex.Store(INDEX_NONE);CallState->InFlight.Store(0);
                Promise->SetValue(MakeValue(Rejection(TEXT("texture_material_standalone_tick_unavailable"),TEXT("Next-tick registration failed before Python/import"),Method)));
            }
            return Future;
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
    Report->SetStringField(TEXT("guard_scope"),TEXT("Original texture/material public3/4/8 and required1/3/3 use existing public Editor Python SDK. Preserve LOD0 polygon corner order, original PNG1024/transparentTrue, seven BaseColor aliases and five PBR slot presets. Actual file writes, material creation, implicit compile and saved/partial history remain domain-owned; this registration receipt is not Engine behavior or complete shader proof."));
    for(const TCHAR* Key : {TEXT("active"),TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("texture_material_public3_4_8_schema_verified"),TEXT("required1_3_3_defaults2_1_5_verified"),TEXT("json_value_carrier15_schema_verified"),TEXT("original_named_defaults_and_direct_values_preserved"),TEXT("public_Editor_Python_SDK3_no_Native_leaf"),TEXT("uv_PNG_and_material_partial_save_no_replay_preserved"),TEXT("texture_material3_Python_contract_owned")}){Report->SetBoolField(Key,false);}
    Report->SetBoolField(TEXT("texture_material_import_apply10_contract_owned"),false);
    Report->SetBoolField(TEXT("texture_material_pbr_companion8_contract_owned"),false);
    Report->SetBoolField(TEXT("texture_material_import3_importset7_apply3_contract_owned"),false);
    Report->SetBoolField(TEXT("texture_material_image_handoff4_6_6_contract_owned"),false);
    Report->SetNumberField(TEXT("delegated_calls_in_flight"), Owned.IsValid() ? Owned->InFlight.Load() : 0);
}

bool Ready(const TSharedRef<FAttachmentState>& State, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    UToolsetRegistrySubsystem* Subsystem = State->Subsystem.Get();
    if (!Subsystem || !State->Wrapper.IsValid() || !State->Original.IsValid())
    { Error = TEXT("Captured TextureMaterial guard owner/subsystem unavailable"); return false; }
    // Temporarily hide dispatch while probing a retained disabled object without changing global filters.
    const bool WasActive = State->Active.Load(); State->Active.Store(false);
    const bool Identical = HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper);
    State->Active.Store(WasActive && Identical);
    Report->SetBoolField(TEXT("guard_attached"), Identical);
    Report->SetBoolField(TEXT("original_handler_detached"), State->OriginalDetached);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false);
    Report->SetBoolField(TEXT("cleanup_required"), true);
    if (!Identical) { State->Original->SetEnabled(false); Error = TEXT("Captured TextureMaterial wrapper no longer owns its canonical registry entry; foreign entry preserved"); return false; }
    if (!Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get()) { State->Active.Store(false); State->Original->SetEnabled(false); return false; }
    const FString CurrentSchema = UnfilteredOriginalSchema(State);
    FString CurrentLogical;
    Report->SetStringField(TEXT("reflected_schema_json"), CurrentSchema);
    if (!TextureMaterialSchema(CurrentSchema, CurrentLogical, Error) || !CurrentSchema.Equals(State->ReflectedSchema,ESearchCase::CaseSensitive) || !CurrentLogical.Equals(State->FullSchema,ESearchCase::CaseSensitive))
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("Original TextureMaterial schema changed; no automatic replacement or dispatch"); } return false; }
    auto CurrentSubsystem = UToolsetRegistrySubsystem::Get();
    if (!CurrentSubsystem.HasValue() || CurrentSubsystem.GetValue().Get() != State->Subsystem.Get()
        || !HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry, State->Wrapper)
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false); if (Error.IsEmpty()) { Error = TEXT("TextureMaterial wrapper owner/class changed during reflected schema callbacks"); } return false; }
    for(const TCHAR* Key : {TEXT("raw_guard_installed"),TEXT("default_verified"),TEXT("reflected_carrier_schema_verified"),TEXT("logical_schema_verified"),TEXT("output11_schema_preserved"),TEXT("texture_material_public3_4_8_schema_verified"),TEXT("required1_3_3_defaults2_1_5_verified"),TEXT("json_value_carrier15_schema_verified"),TEXT("original_named_defaults_and_direct_values_preserved"),TEXT("public_Editor_Python_SDK3_no_Native_leaf"),TEXT("uv_PNG_and_material_partial_save_no_replay_preserved"),TEXT("texture_material3_Python_contract_owned")}){Report->SetBoolField(Key,true);}
    Report->SetBoolField(TEXT("texture_material_import_apply10_contract_owned"),true);
    Report->SetBoolField(TEXT("texture_material_pbr_companion8_contract_owned"),true);
    Report->SetBoolField(TEXT("texture_material_import3_importset7_apply3_contract_owned"),true);
    Report->SetBoolField(TEXT("texture_material_image_handoff4_6_6_contract_owned"),true);
    Report->SetBoolField(TEXT("toolset_enabled"), State->Wrapper->IsEnabled());
    if (!State->Wrapper->IsEnabled()) { Error = TEXT("TextureMaterial guard is installed but disabled by existing toolset state or user filters"); return false; }
    // The public Python class getter constructs a new physical FunctionLibraryToolset.
    // Read the actual registered wrapper instead, retaining current user filters.
    const TSharedPtr<FToolset> Registered = Subsystem->ToolsetRegistry.Find(CanonicalName);
    if (Registered != State->Wrapper)
    { State->Active.Store(false); Error = TEXT("Current registered TextureMaterial wrapper identity is unavailable for logical schema receipt"); return false; }
    const FString RegisteredLogicalSchema = Registered->GetJsonSchema();
    auto AfterSchemaSubsystem = UToolsetRegistrySubsystem::Get();
    if (RegisteredLogicalSchema.IsEmpty() || !AfterSchemaSubsystem.HasValue()
        || AfterSchemaSubsystem.GetValue().Get() != State->Subsystem.Get()
        || Subsystem->ToolsetRegistry.Find(CanonicalName) != Registered
        || !Metadata(State->Class.Get(), Error) || State->Original->GetToolsetClass() != State->Class.Get())
    { State->Active.Store(false); State->Original->SetEnabled(false);
      if (Error.IsEmpty()) { Error = TEXT("Current registered TextureMaterial logical schema/owner/class receipt could not be confirmed"); }
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
        if (Remains) { Report->SetBoolField(TEXT("cleanup_required"), true); Report->SetBoolField(TEXT("registry_changed"), Changed); Error = TEXT("Owned TextureMaterial guard cleanup was not confirmed; inactive retained owner preserved"); return false; }
    }
    // Detached/cached wrappers are inactive, so they cannot dispatch after this cycle is broken.
    State->Wrapper.Reset(); State->Original.Reset(); Owned.Reset();
    Report->SetBoolField(TEXT("registry_changed"), Changed); Report->SetBoolField(TEXT("cleanup_required"), false);
    Report->SetBoolField(TEXT("class_unregister_allowed"), false); Report->SetBoolField(TEXT("owned_handler_removed"), true);
    Report->SetBoolField(TEXT("foreign_handler_preserved"), true);
    return true;
}
}

bool TextureMaterialStandaloneImportAdmitted()
{
    if(!IsInGameThread()||IsEngineExitRequested()||IsGarbageCollecting()||OperationBusy||!Owned.IsValid()
        ||!Owned->Active.Load()||!Owned->ExecutingStandaloneTicker.Load()||Owned->InFlight.Load()!=1||(Owned->DelegatedMethodIndex.Load()!=3&&Owned->DelegatedMethodIndex.Load()!=5&&Owned->DelegatedMethodIndex.Load()!=6))return false;
    auto Current=UToolsetRegistrySubsystem::Get();
    return Current.HasValue()&&Current.GetValue().Get()==Owned->Subsystem.Get()&&Owned->Wrapper.IsValid()&&Owned->Original.IsValid()
        &&Current.GetValue()->ToolsetRegistry.Find(CanonicalName)==Owned->Wrapper&&IsValid(Owned->Class.Get())
        &&!Owned->Class->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)&&!Owned->Class->HasAnyClassFlags(CLASS_NewerVersionExists)
        &&Owned->Original->GetToolsetClass()==Owned->Class.Get();
}

bool InstallTextureMaterialToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("TextureMaterial guard installation requires an idle live editor game thread; no attachment retried"); return false; }
    FBusyScope Busy;
    if (!Metadata(ToolsetClass, Error)) { return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue()) { Error = TEXT("Original official Toolset Registry subsystem unavailable"); return false; }
    if (Owned.IsValid())
    {
        if (Owned->Subsystem.Get() != Current.GetValue().Get() || Owned->Class.Get() != ToolsetClass)
        { Error = TEXT("Different retained TextureMaterial owner/subsystem/class exists; explicit cleanup required, no automatic reattachment"); return false; }
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
    if (!TextureMaterialSchema(State->ReflectedSchema, State->FullSchema, Error)) { Original->SetEnabled(false); return false; }
    State->Wrapper = MakeShared<FTextureMaterialToolset>(State);
    // Exact identity is checked immediately before SDK's name-only removal, with dispatch inactive.
    if (!HasRetainedToolsetIdentity(Current.GetValue()->ToolsetRegistry, Original)
        || !Current.GetValue()->ToolsetRegistry.UnregisterToolset(Original))
    { Error = TEXT("Original TextureMaterial handler detach failed or identity changed; captured owner remains inactive for explicit cleanup"); return false; }
    State->OriginalDetached = true; Report->SetBoolField(TEXT("registry_changed"), true); Report->SetBoolField(TEXT("original_handler_detached"), true);
    // Register broadcasts callbacks. Reentry is busy-rejected and wrapper dispatch remains inactive.
    if (!Current.GetValue()->ToolsetRegistry.RegisterToolset(State->Wrapper))
    { Error = TEXT("TextureMaterial raw wrapper registration failed or collided; no unguarded handler restored, inspect actual registry before cleanup"); return false; }
    if (!Ready(State, Report, Error)) { return false; }
    State->Active.Store(true); State->Original->SetEnabled(false);
    Report->SetBoolField(TEXT("ready"), true); Report->SetBoolField(TEXT("verified"), true);
    Report->SetBoolField(TEXT("active"), true);
    // The registration event already exposed the immutable complete schema; activation changes no catalogue.
    Report->SetBoolField(TEXT("extra_catalogue_broadcast_required"), false);
    return true;
}

bool VerifyTextureMaterialToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || IsEngineExitRequested() || OperationBusy) { Error = TEXT("TextureMaterial guard verification requires an idle live editor game thread"); return false; }
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
        if (!Admitted) { Error = TEXT("Only the current exact guarded Named TextureMaterial delegation is admitted"); }
        return false;
    }
    FBusyScope Busy;
    if (!Owned.IsValid() || Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact installed TextureMaterial raw guard required; verification never installs one"); return false; }
    auto Current = UToolsetRegistrySubsystem::Get();
    if (!Current.HasValue() || Current.GetValue().Get() != Owned->Subsystem.Get()) { Error = TEXT("Original TextureMaterial registry subsystem identity changed; no retargeting"); return false; }
    const bool Ok = Ready(Owned.ToSharedRef(), Report, Error) && Owned->Active.Load();
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok); Report->SetBoolField(TEXT("active"), Owned->Active.Load());
    return Ok;
}

bool RemoveTextureMaterialToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    BaseReport(Report);
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { Error = TEXT("TextureMaterial guard removal requires an idle editor game thread; no foreign entry removed"); return false; }
    FBusyScope Busy;
    if (Owned.IsValid() && Owned->Class.Get() != ToolsetClass) { Error = TEXT("Exact retained TextureMaterial class owner required for removal"); return false; }
    const bool Ok = Cleanup(Report, Error); Report->SetBoolField(TEXT("removed"), Ok); return Ok;
}

void ShutdownTextureMaterialToolset()
{
    if (Owned.IsValid()) { Owned->Active.Store(false); }
    if(IsInGameThread()&&Owned.IsValid()&&Owned->Ticker.IsValid()&&!Owned->ExecutingStandaloneTicker.Load())
    {
        // Cancel only a queued, not-entered ticker. A dispatched callback keeps its
        // original lifetime and is never replayed or relabelled as writer0.
        FTSTicker::GetCoreTicker().RemoveTicker(Owned->Ticker);Owned->Ticker.Reset();
        const int32 QueuedMethod=Owned->QueuedMethodIndex.Load();Owned->QueuedMethodIndex.Store(INDEX_NONE);
        auto Promise=Owned->PendingPromise;Owned->PendingPromise.Reset();Owned->DelegatedMethodIndex.Store(INDEX_NONE);Owned->InFlight.Store(0);
        if(Owned->Original.IsValid())Owned->Original->SetEnabled(false);
        if(Promise.IsValid())Promise->SetValue(MakeValue(Rejection(TEXT("texture_material_standalone_shutdown_before_dispatch"),TEXT("Owned queued ticker cancelled before Python/import"),(QueuedMethod==3||QueuedMethod==5||QueuedMethod==6)?Methods[QueuedMethod]:TEXT("unknown_queued_import"))));
    }
    if (!IsInGameThread() || OperationBusy || (Owned.IsValid() && Owned->InFlight.Load() != 0)) { return; }
    FBusyScope Busy; auto Report = MakeShared<FJsonObject>(); FString Error;
    Cleanup(Report, Error);
}
}
