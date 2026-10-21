#include "Registration/CubelessJsonConverters.h"
#include "Registration/CubelessActorTransformToolset.h"

#include "Compatibility/UE58/CubelessMaterialArrayConverter.h"
#include "Compatibility/UE58/CubelessAIDiscoveryDefaultsConverter.h"
#include "Compatibility/UE58/CubelessBlueprintDefaultsConverter.h"
#include "Compatibility/UE58/CubelessDataTableDefaultsConverter.h"
#include "Compatibility/UE58/CubelessSequencerFloatDefaultsConverter.h"
#include "Compatibility/UE58/CubelessSequencerMaintenanceDefaultsConverter.h"
#include "Compatibility/UE58/CubelessSequencerCameraDefaultsConverter.h"
#include "Compatibility/UE58/CubelessBlueprintMakeStructDefaultsConverter.h"
#include "Registration/CubelessNiagaraStageConverters.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/Class.h"
#include "UObject/PropertyOptional.h"
#include "UObject/UnrealType.h"


// CJ: original ListSkeletonBones3 exact empty filter default on new dedicated class.

// CK exact nullable transform carrier: omission/null != explicit empty array.
namespace
{
constexpr const TCHAR* SocketClass=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/skeletal_toolset_PY.CubelessSkeletalTools");
constexpr const TCHAR* SocketConverterName=TEXT("CubelessMCP_UE58.SkeletalSocketDefaults.v1");
constexpr const TCHAR* SocketEmpty=TEXT("\"\"");
const FProperty* MatchSocketField(const FProperty* P)
{
 const auto F=P?Cast<UFunction>(P->GetOwnerUObject()):nullptr;const auto C=F?F->GetOwnerClass():nullptr;
 if(!P||!C||C->GetPathName()!=SocketClass||P->ArrayDim!=1||!P->HasAllPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm|CPF_ReferenceParm)) { return nullptr; }
 const bool Add=F->GetName()==TEXT("add_skeletal_socket"),Set=F->GetName()==TEXT("set_skeletal_socket");if(!Add&&!Set) { return nullptr; }
 const FString N=P->GetName(),Default=F->GetMetaData(*FString::Printf(TEXT("CPP_Default_%s"),*N));
 if(!F->HasMetaData(*FString::Printf(TEXT("CPP_Default_%s"),*N))||!F->GetMetaData(*N).IsEmpty()) { return nullptr; }
 if(N==TEXT("location")||N==TEXT("rotation")||N==TEXT("scale"))
 { const auto A=CastField<FArrayProperty>(P);return A&&A->Inner&&A->Inner->ArrayDim==1&&CastField<FStrProperty>(A->Inner)&&(Default.IsEmpty()||Default==TEXT("()"))?P:nullptr; }
 return Set&&(N==TEXT("bone_name")||N==TEXT("new_socket_name"))&&CastField<FStrProperty>(P)&&(Default.IsEmpty()||Default==SocketEmpty)?P:nullptr;
}
TSharedPtr<FJsonValue> SocketDefault(const FProperty* P,const FString& T)
{
 if(!MatchSocketField(P)) { return nullptr; }FDefaultConstructedPropertyElement Scratch(P);
 if(const auto A=CastField<FArrayProperty>(P))
 { if(!T.IsEmpty()&&T!=TEXT("()")) { return nullptr; }const TCHAR* End=A->ImportText_Direct(TEXT("()"),Scratch.GetObjAddress(),nullptr,PPF_None);
 if(!End||*End!=TCHAR('\0')||FScriptArrayHelper(A,Scratch.GetObjAddress()).Num()!=0) { return nullptr; }return MakeShared<FJsonValueNull>(); }
 const auto S=CastField<FStrProperty>(P);if(!T.IsEmpty()&&T!=SocketEmpty) { return nullptr; }const TCHAR* End=S->ImportText_Direct(SocketEmpty,Scratch.GetObjAddress(),nullptr,PPF_Delimited);
 if(!End||*End!=TCHAR('\0')||!S->GetPropertyValue(Scratch.GetObjAddress()).IsEmpty()) { return nullptr; }return MakeShared<FJsonValueString>(FString());
}
class FSkeletalSocketDefaultsConverter final:public UE::ToolsetRegistry::FToolsetJsonConverter
{
public:
 virtual FString GetName() const override { return SocketConverterName; }
 virtual bool CanConvertProperty(TNotNull<const FProperty*> P) override { return MatchSocketField(P)!=nullptr; }
 virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> P) override
 {
 auto J=MakeShared<FJsonObject>();if(!MatchSocketField(P)) { return J; }
 if(CastField<FArrayProperty>(P)) { J->SetArrayField(TEXT("type"),{MakeShared<FJsonValueString>(TEXT("array")),MakeShared<FJsonValueString>(TEXT("null"))});auto Items=MakeShared<FJsonObject>();Items->SetStringField(TEXT("type"),TEXT("number"));J->SetObjectField(TEXT("items"),Items); }
 else { J->SetStringField(TEXT("type"),TEXT("string")); }return J;
 }
 virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> P,const FString& T) override
 { auto D=SocketDefault(P,T);if(D.IsValid()) { return D; }UKismetSystemLibrary::RaiseScriptError(TEXT("Exact skeletal socket default invalid."));return MakeShared<FJsonValueNull>(); }
 virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> P,const void* V) override
 {
 if(!V||!MatchSocketField(P)) { return nullptr; }if(const auto S=CastField<FStrProperty>(P)) { return MakeShared<FJsonValueString>(S->GetPropertyValue(V)); }
 const auto A=CastField<FArrayProperty>(P);FScriptArrayHelper H(A,V);if(H.Num()==0) { return MakeShared<FJsonValueNull>(); }if(H.Num()!=1) { return nullptr; }
 TSharedPtr<FJsonValue> J;const FString Text=CastFieldChecked<FStrProperty>(A->Inner)->GetPropertyValue(H.GetRawPtr(0));if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J)||!J.IsValid()||J->Type!=EJson::Array) { return nullptr; }return J;
 }
 virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& J,TNotNull<FProperty*> P,void* Out,UObject* Outer) override
 {
 if(!Out||!MatchSocketField(P)||!J.IsValid()) { return false; }
 if(const auto S=CastField<FStrProperty>(P)) { if(J->Type!=EJson::String) { return false; }S->SetPropertyValue(Out,J->AsString());return true; }
 const auto A=CastField<FArrayProperty>(P);FScriptArrayHelper H(A,Out);if(J->Type==EJson::Null) { H.EmptyValues();return true; }
 if(J->Type!=EJson::Array||J->AsArray().Num()>64) { return false; }for(const auto& V:J->AsArray()) { if(!V.IsValid()||V->Type!=EJson::Number||!FMath::IsFinite(V->AsNumber())) { return false; } }
 FString Encoded;if(!FJsonSerializer::Serialize(J,TEXT(""),TJsonWriterFactory<>::Create(&Encoded))||Encoded.Len()>4096) { return false; }
 H.EmptyAndAddValues(1);CastFieldChecked<FStrProperty>(A->Inner)->SetPropertyValue(H.GetRawPtr(0),Encoded);return true;
 }
};
}
bool Cubeless::Registration::VerifySkeletalSocketToolset(UClass* C,const TSharedRef<FJsonObject>& R,FString& Error)
{
 auto Defaults=MakeShared<FJsonObject>();R->SetObjectField(TEXT("defaults"),Defaults);TArray<TSharedPtr<FJsonValue>> Rows;R->SetArrayField(TEXT("property_observations"),Rows);R->SetBoolField(TEXT("actual_call_packing_verified"),false);
 if(!C||C->GetPathName()!=SocketClass) { Error=TEXT("Exact canonical SkeletalTools class required.");return false; }auto Sub=UToolsetRegistrySubsystem::Get(TEXT("Skeletal socket exact schema verification"));if(!Sub.HasValue()) { Error=TEXT("Registry unavailable; no retry.");return false; }
 struct FPending { UFunction* F;FProperty* P;FString Before;TSharedPtr<FJsonValue> Default;TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Converter;TSharedPtr<FJsonObject> Row; };
 TArray<FPending> Pending;
 for(const TCHAR* FunctionName:{TEXT("add_skeletal_socket"),TEXT("set_skeletal_socket")})
 {
 UFunction* F=C->FindFunctionByName(FunctionName);if(!F||F->GetOwnerClass()!=C) { Error=TEXT("Canonical socket methods unavailable.");return false; }
 TArray<FString> Names={TEXT("location"),TEXT("rotation"),TEXT("scale")};if(F->GetName()==TEXT("set_skeletal_socket")) { Names.Add(TEXT("bone_name"));Names.Add(TEXT("new_socket_name")); }
 for(const FString& N:Names)
 {
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("function"),F->GetName());Row->SetStringField(TEXT("property"),N);Rows.Add(MakeShared<FJsonValueObject>(Row));R->SetArrayField(TEXT("property_observations"),Rows);
 FProperty* P=FindFProperty<FProperty>(F,*N);const FString Before=F->GetMetaData(*FString::Printf(TEXT("CPP_Default_%s"),*N));Row->SetStringField(TEXT("metadata_before"),Before);
 auto Conv=P?Sub.GetValue()->ToolsetRegistry.GetConverterForProperty(P):nullptr;auto D=SocketDefault(P,Before);Row->SetBoolField(TEXT("exact_converter_active"),Conv.IsValid()&&Conv->GetName()==SocketConverterName);
 if(!MatchSocketField(P)||!Conv.IsValid()||Conv->GetName()!=SocketConverterName||!D.IsValid()) { Error=TEXT("Socket kind/default/converter prevalidation failed; no metadata writes.");return false; }
 auto Type=Conv->PropertyToJsonSchema(P);if(!Type.IsValid()) { Error=TEXT("Socket type schema missing.");return false; }
 if(CastField<FArrayProperty>(P)) { const TArray<TSharedPtr<FJsonValue>>* Types=nullptr;const TSharedPtr<FJsonObject>* Items=nullptr;FString Kind;
 if(Type->Values.Num()!=2||!Type->TryGetArrayField(TEXT("type"),Types)||Types->Num()!=2||(*Types)[0]->Type!=EJson::String||(*Types)[0]->AsString()!=TEXT("array")||(*Types)[1]->Type!=EJson::String||(*Types)[1]->AsString()!=TEXT("null")||!Type->TryGetObjectField(TEXT("items"),Items)||(*Items)->Values.Num()!=1||!(*Items)->TryGetStringField(TEXT("type"),Kind)||Kind!=TEXT("number")||D->Type!=EJson::Null) { Error=TEXT("Exact nullable numeric array/default null schema required.");return false; } }
 else { FString Kind;if(Type->Values.Num()!=1||!Type->TryGetStringField(TEXT("type"),Kind)||Kind!=TEXT("string")||D->Type!=EJson::String||!D->AsString().IsEmpty()) { Error=TEXT("Exact empty string default required.");return false; } }
 Row->SetObjectField(TEXT("type_schema"),Type);Row->SetField(TEXT("public_default"),D);Pending.Add({F,P,Before,D,Conv,Row});
 }
 }
 // All eight rows checked before at-most-eight canonical array/string metadata writes.
 for(auto& V:Pending)
 {
 const FName Key(*FString::Printf(TEXT("CPP_Default_%s"),*V.P->GetName()));const TCHAR* Prepared=CastField<FArrayProperty>(V.P)?TEXT("()"):SocketEmpty;if(V.Before.IsEmpty())
 { R->SetBoolField(TEXT("metadata_mutation_attempted"),true);R->SetNumberField(TEXT("metadata_write_attempt_count"),R->GetNumberField(TEXT("metadata_write_attempt_count"))+1);V.F->SetMetaData(Key,Prepared);
 if(V.Before!=V.F->GetMetaData(Key)) { R->SetBoolField(TEXT("metadata_changed"),true);R->SetNumberField(TEXT("metadata_changed_count"),R->GetNumberField(TEXT("metadata_changed_count"))+1); } }
 V.Row->SetStringField(TEXT("metadata_after"),V.F->GetMetaData(Key));auto Conv=Sub.GetValue()->ToolsetRegistry.GetConverterForProperty(V.P);auto D=Conv.IsValid()?Conv->PropertyToDefault(V.P,V.F->GetMetaData(Key)):nullptr;
 if(!Conv.IsValid()||Conv->GetName()!=SocketConverterName||!D.IsValid()||D->Type!=V.Default->Type||(D->Type==EJson::String&&!D->AsString().IsEmpty())) { Error=TEXT("Socket prepared default/converter differs; raw RAM prefix retained.");return false; }
 Defaults->SetField(V.F->GetName()+TEXT(".")+V.P->GetName(),D);V.Row->SetBoolField(TEXT("default_verified"),true);R->SetNumberField(TEXT("verified_property_count"),R->GetNumberField(TEXT("verified_property_count"))+1);
 }
 R->SetBoolField(TEXT("default_verified"),true);R->SetBoolField(TEXT("exact_converter_active"),true);R->SetBoolField(TEXT("nullable_schema_verified"),true);R->SetBoolField(TEXT("carrier_kind_verified"),true);return true;
}
namespace
{
constexpr const TCHAR* SkeletalClass=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/skeletal_toolset_PY.CubelessSkeletalTools");
constexpr const TCHAR* SkeletalConverterName=TEXT("CubelessMCP_UE58.SkeletalReadDefaults.v1");
constexpr const TCHAR* SkeletalPreparedEmpty=TEXT("\"\"");
const FStrProperty* MatchSkeletalFilter(const FProperty* P)
{
 const auto F=P?Cast<UFunction>(P->GetOwnerUObject()):nullptr;const auto C=F?F->GetOwnerClass():nullptr;
 if(!P||!C||C->GetPathName()!=SkeletalClass||F->GetName()!=TEXT("list_skeleton_bones")||P->GetName()!=TEXT("bone_name_contains")
  ||P->ArrayDim!=1||!P->HasAllPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm|CPF_ReferenceParm)
  ||!F->HasMetaData(TEXT("CPP_Default_bone_name_contains"))||!F->GetMetaData(TEXT("bone_name_contains")).IsEmpty()) { return nullptr; }
 const FString T=F->GetMetaData(TEXT("CPP_Default_bone_name_contains"));return (T.IsEmpty()||T==SkeletalPreparedEmpty)?CastField<FStrProperty>(P):nullptr;
}
// DY: exact new observer's empty filter reuses the retained Skeletal owner.
constexpr const TCHAR* AnimStateMachineClass=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/anim_state_machine_inspection_toolset_PY.CubelessAnimStateMachineInspectionTools");
const FStrProperty* MatchAnimStateMachineFilter(const FProperty* P)
{
 const auto F=P?Cast<UFunction>(P->GetOwnerUObject()):nullptr;const auto C=F?F->GetOwnerClass():nullptr;
 if(!P||!F||!C||!C->GetPathName().Equals(AnimStateMachineClass,ESearchCase::CaseSensitive)
  ||!F->GetName().Equals(TEXT("inspect_anim_state_machine_transitions"),ESearchCase::CaseSensitive)
  ||!P->GetName().Equals(TEXT("state_machine_name"),ESearchCase::CaseSensitive)||P->ArrayDim!=1
  ||!P->HasAllPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm|CPF_ReferenceParm)
  ||!F->HasMetaData(TEXT("CPP_Default_state_machine_name"))||!F->GetMetaData(TEXT("state_machine_name")).IsEmpty()) { return nullptr; }
 const FString T=F->GetMetaData(TEXT("CPP_Default_state_machine_name"));
 return (T.IsEmpty()||T.Equals(SkeletalPreparedEmpty,ESearchCase::CaseSensitive))?CastField<FStrProperty>(P):nullptr;
}
// EA: only the original Physics inspector's two empty path defaults reuse this owner.
constexpr const TCHAR* PhysicsAssetInspectionClass=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/physics_asset_inspection_toolset_PY.CubelessPhysicsAssetInspectionTools");
const FStrProperty* MatchPhysicsAssetInspectionPath(const FProperty* P)
{
 const auto F=P?Cast<UFunction>(P->GetOwnerUObject()):nullptr;const auto C=F?F->GetOwnerClass():nullptr;
 if(!P||!F||!C||!C->GetPathName().Equals(PhysicsAssetInspectionClass,ESearchCase::CaseSensitive)
  ||!F->GetName().Equals(TEXT("inspect_physics_asset_constraints_guarded"),ESearchCase::CaseSensitive)
  ||P->ArrayDim!=1||!P->HasAllPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm|CPF_ReferenceParm)) { return nullptr; }
 const FString Name=P->GetName();
 if(!Name.Equals(TEXT("physics_asset_path"),ESearchCase::CaseSensitive)&&!Name.Equals(TEXT("skeletal_mesh_path"),ESearchCase::CaseSensitive)) { return nullptr; }
 const FName Key(*FString::Printf(TEXT("CPP_Default_%s"),*Name));
 if(!F->HasMetaData(Key)||!F->GetMetaData(*Name).IsEmpty()) { return nullptr; }
 const FString T=F->GetMetaData(Key);return (T.IsEmpty()||T.Equals(SkeletalPreparedEmpty,ESearchCase::CaseSensitive))?CastField<FStrProperty>(P):nullptr;
}
const FStrProperty* MatchSkeletalReadScope(const FProperty* P)
{
 if(const auto Old=MatchSkeletalFilter(P)) { return Old; }
 if(const auto Existing=MatchAnimStateMachineFilter(P)) { return Existing; }
 return MatchPhysicsAssetInspectionPath(P);
}

TSharedPtr<FJsonValue> SkeletalDefault(const FProperty* P,const FString& T)
{
 const auto S=MatchSkeletalReadScope(P);if(!S||(!T.IsEmpty()&&T!=SkeletalPreparedEmpty)) { return nullptr; }
 FDefaultConstructedPropertyElement Scratch(S);const TCHAR* End=S->ImportText_Direct(SkeletalPreparedEmpty,Scratch.GetObjAddress(),nullptr,PPF_Delimited);
 if(!End||*End!=TCHAR('\0')||!S->GetPropertyValue(Scratch.GetObjAddress()).IsEmpty()) { return nullptr; }return MakeShared<FJsonValueString>(FString());
}
class FSkeletalReadDefaultsConverter final:public UE::ToolsetRegistry::FToolsetJsonConverter
{
public:
 virtual FString GetName() const override { return SkeletalConverterName; }
 virtual bool CanConvertProperty(TNotNull<const FProperty*> P) override { return MatchSkeletalReadScope(P)!=nullptr; }
 virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> P) override
 { auto J=MakeShared<FJsonObject>();if(MatchSkeletalReadScope(P)) { J->SetStringField(TEXT("type"),TEXT("string")); }return J; }
 virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> P,const FString& T) override
 { auto D=SkeletalDefault(P,T);if(D.IsValid()) { return D; }UKismetSystemLibrary::RaiseScriptError(TEXT("Exact skeletal empty filter default invalid."));return MakeShared<FJsonValueNull>(); }
 virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> P,const void* V) override
 { return V&&MatchSkeletalReadScope(P)?FJsonObjectConverter::UPropertyToJsonValue(P,V):nullptr; }
 virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& V,TNotNull<FProperty*> P,void* Out,UObject* Outer) override
 { const auto S=MatchSkeletalReadScope(P);if(!Out||!S||!V.IsValid()||V->Type!=EJson::String) { return false; }S->SetPropertyValue(Out,V->AsString());return true; }
};
}
bool Cubeless::Registration::VerifySkeletalReadToolset(UClass* C,const TSharedRef<FJsonObject>& R,FString& Error)
{
 auto Defaults=MakeShared<FJsonObject>();R->SetObjectField(TEXT("defaults"),Defaults);auto Row=MakeShared<FJsonObject>();R->SetArrayField(TEXT("property_observations"),{MakeShared<FJsonValueObject>(Row)});
 Row->SetStringField(TEXT("property"),TEXT("bone_name_contains"));R->SetBoolField(TEXT("actual_call_packing_verified"),false);
 if(!C||C->GetPathName()!=SkeletalClass) { Error=TEXT("Exact canonical SkeletalTools class required.");return false; }
 UFunction* F=C->FindFunctionByName(TEXT("list_skeleton_bones"));UFunction* Sockets=C->FindFunctionByName(TEXT("list_skeletal_sockets"));
 if(!F||!Sockets||F->GetOwnerClass()!=C||Sockets->GetOwnerClass()!=C) { Error=TEXT("Original owned Skeletal methods2 required.");return false; }
 const auto P=FindFProperty<FProperty>(F,TEXT("bone_name_contains"));const auto Max=FindFProperty<FIntProperty>(F,TEXT("max_bones"));
 const FName Key(TEXT("CPP_Default_bone_name_contains"));const FString Prior=F->GetMetaData(Key);Row->SetBoolField(TEXT("metadata_present_before"),F->HasMetaData(Key));Row->SetStringField(TEXT("metadata_before"),Prior);
 if(!MatchSkeletalFilter(P)||!Max||F->GetMetaData(TEXT("CPP_Default_max_bones"))!=TEXT("500")) { Error=TEXT("Exact original string-empty/int500 declarations required before metadata write.");return false; }
 auto Sub=UToolsetRegistrySubsystem::Get(TEXT("Verify skeletal filter default"));if(!Sub.HasValue()) { Error=TEXT("Registry unavailable; no metadata retry.");return false; }
 auto Conv=Sub.GetValue()->ToolsetRegistry.GetConverterForProperty(P);const bool Active=Conv.IsValid()&&Conv->GetName()==SkeletalConverterName;
 Row->SetBoolField(TEXT("exact_converter_active"),Active);if(!Active) { Error=TEXT("Exact skeletal filter converter unavailable.");return false; }
 auto Type=Conv->PropertyToJsonSchema(P);auto Default=SkeletalDefault(P,Prior);FString Kind;
 if(!Type.IsValid()||Type->Values.Num()!=1||!Type->TryGetStringField(TEXT("type"),Kind)||Kind!=TEXT("string")||!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().IsEmpty())
 { Error=TEXT("Skeletal type/default proof failed before metadata write.");return false; }
 Row->SetObjectField(TEXT("type_schema"),Type);Row->SetField(TEXT("public_default"),Default);
 if(Prior.IsEmpty())
 {
  R->SetBoolField(TEXT("metadata_mutation_attempted"),true);R->SetNumberField(TEXT("metadata_write_attempt_count"),1);F->SetMetaData(Key,SkeletalPreparedEmpty);
  const bool Changed=Prior!=F->GetMetaData(Key);R->SetBoolField(TEXT("metadata_changed"),Changed);R->SetNumberField(TEXT("metadata_changed_count"),Changed?1:0);
 }
 Row->SetStringField(TEXT("metadata_after"),F->GetMetaData(Key));Conv=Sub.GetValue()->ToolsetRegistry.GetConverterForProperty(P);
 if(!Conv.IsValid()||Conv->GetName()!=SkeletalConverterName||F->GetMetaData(Key)!=SkeletalPreparedEmpty) { Error=TEXT("Skeletal metadata/converter readback failed; actual RAM prefix retained.");return false; }
 Default=Conv->PropertyToDefault(P,F->GetMetaData(Key));if(!Default.IsValid()||Default->Type!=EJson::String||!Default->AsString().IsEmpty()) { Error=TEXT("Prepared skeletal default differs.");return false; }
 Defaults->SetField(TEXT("list_skeleton_bones.bone_name_contains"),Default);R->SetBoolField(TEXT("default_verified"),true);R->SetBoolField(TEXT("exact_converter_active"),true);R->SetBoolField(TEXT("type_schema_verified"),true);R->SetNumberField(TEXT("verified_property_count"),1);
 Row->SetBoolField(TEXT("type_schema_verified"),true);Row->SetBoolField(TEXT("default_verified"),true);return true;
}
bool Cubeless::Registration::PrepareAnimStateMachineInspectionToolset(UClass* C,const TSharedRef<FJsonObject>& R,FString& Error)
{
 for(const TCHAR* K:{TEXT("default_verified"),TEXT("exact_converter_active"),TEXT("type_schema_verified"),TEXT("actual_call_packing_verified"),TEXT("metadata_mutation_attempted"),TEXT("metadata_changed")}) { R->SetBoolField(K,false); }
 for(const TCHAR* K:{TEXT("verified_property_count"),TEXT("metadata_write_attempt_count"),TEXT("metadata_write_return_count"),TEXT("metadata_changed_count")}) { R->SetNumberField(K,0); }
 auto Defaults=MakeShared<FJsonObject>();R->SetObjectField(TEXT("defaults"),Defaults);R->SetArrayField(TEXT("property_observations"),TArray<TSharedPtr<FJsonValue>>{});
 R->SetStringField(TEXT("converter_name"),SkeletalConverterName);
 R->SetStringField(TEXT("preparation_scope"),TEXT("Exact unregistered new observer6 filter only: at most one CPP_Default RAM metadata write; stock scratch default/type/readback. No asset modification/compiler/save or actual call-packing proof."));
 if(!IsInGameThread()||!IsValid(C)||!C->GetPathName().Equals(AnimStateMachineClass,ESearchCase::CaseSensitive)) { Error=TEXT("Exact new state-machine observer class on the Editor game thread required.");return false; }
 if(UToolsetRegistry::IsToolsetClassRegistered(C)) { Error=TEXT("Exact class must be unregistered before metadata preparation; no invalidation/retry here.");return false; }
 UFunction* F=C->FindFunctionByName(TEXT("inspect_anim_state_machine_transitions"));
 if(!F||F->GetOwnerClass()!=C||!F->GetName().Equals(TEXT("inspect_anim_state_machine_transitions"),ESearchCase::CaseSensitive)) { Error=TEXT("Exact owned original observer function required.");return false; }
 int32 ParamCount=0;for(TFieldIterator<FProperty> I(F);I;++I) { if(I->HasAnyPropertyFlags(CPF_Parm)&&!I->HasAnyPropertyFlags(CPF_ReturnParm)) { ++ParamCount; } }
 if(ParamCount!=6) { Error=TEXT("Exact original six parameters required before metadata write.");return false; }
 const auto ValidParam=[F](const TCHAR* Name)->FProperty* { auto P=FindFProperty<FProperty>(F,Name);return P&&P->ArrayDim==1&&P->HasAllPropertyFlags(CPF_Parm)&&!P->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm|CPF_ReferenceParm)?P:nullptr; };
 const auto Blueprint=CastField<FStrProperty>(ValidParam(TEXT("blueprint_name")));auto P=ValidParam(TEXT("state_machine_name"));
 const FName Key(TEXT("CPP_Default_state_machine_name"));const FString Prior=F->GetMetaData(Key);
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("method"),F->GetName());Row->SetStringField(TEXT("property"),TEXT("state_machine_name"));
 Row->SetBoolField(TEXT("metadata_present_before"),F->HasMetaData(Key));Row->SetStringField(TEXT("metadata_before"),Prior);
 Row->SetBoolField(TEXT("metadata_write_attempted"),false);Row->SetBoolField(TEXT("metadata_write_returned"),false);
 R->SetArrayField(TEXT("property_observations"),{MakeShared<FJsonValueObject>(Row)});
 if(!Blueprint||F->HasMetaData(TEXT("CPP_Default_blueprint_name"))||!MatchAnimStateMachineFilter(P)) { Error=TEXT("Exact required blueprint string and original empty filter required before write.");return false; }
 for(const TCHAR* Name:{TEXT("include_pins"),TEXT("include_rule_graph_nodes"),TEXT("include_state_nodes")})
 {
  auto B=CastField<FBoolProperty>(ValidParam(Name));const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"),Name));
  if(!B||!F->HasMetaData(DefaultKey)) { Error=TEXT("Original true boolean declarations required before write.");return false; }
  FDefaultConstructedPropertyElement Scratch(B);const FString T=F->GetMetaData(DefaultKey);const TCHAR* End=B->ImportText_Direct(*T,Scratch.GetObjAddress(),nullptr,PPF_Delimited);
  if(!End||*End!=TCHAR('\0')||!B->GetPropertyValue(Scratch.GetObjAddress())) { Error=TEXT("Original true bool default proof failed before write.");return false; }
 }
 auto Max=CastField<FIntProperty>(ValidParam(TEXT("max_rule_graph_nodes")));
 if(!Max||F->GetMetaData(TEXT("CPP_Default_max_rule_graph_nodes"))!=TEXT("256")) { Error=TEXT("Original reflected int256 default required before write; Native int64 carrier owns clamp.");return false; }
 auto Sub=UToolsetRegistrySubsystem::Get(TEXT("Prepare exact state-machine filter default"));if(!Sub.HasValue()) { Error=TEXT("Registry unavailable; no fallback converter.");return false; }
 auto Conv=Sub.GetValue()->ToolsetRegistry.GetConverterForProperty(P);const bool Active=Conv.IsValid()&&Conv->GetName()==SkeletalConverterName;Row->SetBoolField(TEXT("exact_converter_active"),Active);
 if(!Active) { Error=TEXT("Retained exact Skeletal default converter unavailable.");return false; }
 auto Type=Conv->PropertyToJsonSchema(P);auto D=SkeletalDefault(P,Prior);FString Kind;
 if(!Type.IsValid()||Type->Values.Num()!=1||!Type->TryGetStringField(TEXT("type"),Kind)||Kind!=TEXT("string")||!D.IsValid()||D->Type!=EJson::String||!D->AsString().IsEmpty()) { Error=TEXT("Exact string/default proof failed before metadata write.");return false; }
 Row->SetObjectField(TEXT("type_schema"),Type);Row->SetField(TEXT("public_default"),D);
 if(Prior.IsEmpty())
 {
  Row->SetBoolField(TEXT("metadata_write_attempted"),true);R->SetBoolField(TEXT("metadata_mutation_attempted"),true);R->SetNumberField(TEXT("metadata_write_attempt_count"),1);
  F->SetMetaData(Key,SkeletalPreparedEmpty);
  Row->SetBoolField(TEXT("metadata_write_returned"),true);R->SetNumberField(TEXT("metadata_write_return_count"),1);
  const bool Changed=!Prior.Equals(F->GetMetaData(Key),ESearchCase::CaseSensitive);R->SetBoolField(TEXT("metadata_changed"),Changed);R->SetNumberField(TEXT("metadata_changed_count"),Changed?1:0);
 }
 Row->SetStringField(TEXT("metadata_after"),F->GetMetaData(Key));Conv=Sub.GetValue()->ToolsetRegistry.GetConverterForProperty(P);
 if(!Conv.IsValid()||Conv->GetName()!=SkeletalConverterName||!F->GetMetaData(Key).Equals(SkeletalPreparedEmpty,ESearchCase::CaseSensitive)) { Error=TEXT("Prepared filter metadata/converter readback failed; actual RAM prefix retained.");return false; }
 D=Conv->PropertyToDefault(P,F->GetMetaData(Key));if(!D.IsValid()||D->Type!=EJson::String||!D->AsString().IsEmpty()) { Error=TEXT("Prepared original empty filter default differs; no retry.");return false; }
 Defaults->SetField(TEXT("inspect_anim_state_machine_transitions.state_machine_name"),D);
 R->SetBoolField(TEXT("default_verified"),true);R->SetBoolField(TEXT("exact_converter_active"),true);R->SetBoolField(TEXT("type_schema_verified"),true);R->SetNumberField(TEXT("verified_property_count"),1);
 Row->SetBoolField(TEXT("type_schema_verified"),true);Row->SetBoolField(TEXT("default_verified"),true);return true;
}

bool Cubeless::Registration::PreparePhysicsAssetInspectionToolset(UClass* C,const TSharedRef<FJsonObject>& R,FString& Error)
{
 for(const TCHAR* K:{TEXT("default_verified"),TEXT("exact_converter_active"),TEXT("type_schema_verified"),TEXT("actual_call_packing_verified"),TEXT("metadata_mutation_attempted"),TEXT("metadata_changed")}) { R->SetBoolField(K,false); }
 for(const TCHAR* K:{TEXT("verified_property_count"),TEXT("metadata_write_attempt_count"),TEXT("metadata_write_return_count"),TEXT("metadata_changed_count")}) { R->SetNumberField(K,0); }
 auto Defaults=MakeShared<FJsonObject>();R->SetObjectField(TEXT("defaults"),Defaults);R->SetArrayField(TEXT("property_observations"),TArray<TSharedPtr<FJsonValue>>{});
 R->SetStringField(TEXT("converter_name"),SkeletalConverterName);
 R->SetStringField(TEXT("preparation_scope"),TEXT("Exact unregistered Physics inspector12 paths2 only: at most two CPP_Default RAM metadata writes; stock scratch default/type/readback. No asset modification/compiler/save or actual call-packing proof."));
 if(!IsInGameThread()||!IsValid(C)||!C->GetPathName().Equals(PhysicsAssetInspectionClass,ESearchCase::CaseSensitive)) { Error=TEXT("Exact Physics inspector class on the Editor game thread required.");return false; }
 if(UToolsetRegistry::IsToolsetClassRegistered(C)) { Error=TEXT("Exact class must be unregistered before metadata preparation; no invalidation/retry here.");return false; }
 UFunction* F=C->FindFunctionByName(TEXT("inspect_physics_asset_constraints_guarded"));
 if(!F||F->GetOwnerClass()!=C||!F->GetName().Equals(TEXT("inspect_physics_asset_constraints_guarded"),ESearchCase::CaseSensitive)) { Error=TEXT("Exact owned original Physics observer function required.");return false; }
 int32 ParamCount=0;for(TFieldIterator<FProperty> I(F);I;++I) { if(I->HasAnyPropertyFlags(CPF_Parm)&&!I->HasAnyPropertyFlags(CPF_ReturnParm)) { ++ParamCount; } }
 if(ParamCount!=12) { Error=TEXT("Exact original twelve parameters required before metadata write.");return false; }
 const auto ValidParam=[F](const TCHAR* Name)->FProperty* { auto P=FindFProperty<FProperty>(F,Name);return P&&P->ArrayDim==1&&P->HasAllPropertyFlags(CPF_Parm)&&!P->HasAnyPropertyFlags(CPF_ReturnParm|CPF_OutParm|CPF_ReferenceParm)?P:nullptr; };
 for(const TCHAR* Name:{TEXT("include_bodies"),TEXT("include_constraints"),TEXT("include_profiles"),TEXT("include_solver_settings"),TEXT("include_collision_summary"),TEXT("require_supported_class")})
 {
  auto B=CastField<FBoolProperty>(ValidParam(Name));const FName Key(*FString::Printf(TEXT("CPP_Default_%s"),Name));
  if(!B||!F->HasMetaData(Key)) { Error=TEXT("Original boolean declarations required before write.");return false; }
  FDefaultConstructedPropertyElement Scratch(B);const FString T=F->GetMetaData(Key);const TCHAR* End=B->ImportText_Direct(*T,Scratch.GetObjAddress(),nullptr,PPF_Delimited);
  const bool Expected=!FString(Name).Equals(TEXT("require_supported_class"),ESearchCase::CaseSensitive);
  if(!End||*End!=TCHAR('\0')||B->GetPropertyValue(Scratch.GetObjAddress())!=Expected) { Error=TEXT("Original true/false bool default proof failed before write.");return false; }
 }
 const TCHAR* LimitNames[]={TEXT("max_bodies"),TEXT("max_constraints"),TEXT("max_profiles_per_item"),TEXT("max_shapes_per_body")};const int32 LimitDefaults[]={128,128,16,32};
 for(int32 Index=0;Index<4;++Index)
 {
  auto P=CastField<FIntProperty>(ValidParam(LimitNames[Index]));const FName Key(*FString::Printf(TEXT("CPP_Default_%s"),LimitNames[Index]));
  if(!P||!F->HasMetaData(Key)) { Error=TEXT("Original reflected int declarations required before write.");return false; }
  FDefaultConstructedPropertyElement Scratch(P);const FString T=F->GetMetaData(Key);const TCHAR* End=P->ImportText_Direct(*T,Scratch.GetObjAddress(),nullptr,PPF_Delimited);
  if(!End||*End!=TCHAR('\0')||P->GetPropertyValue(Scratch.GetObjAddress())!=LimitDefaults[Index]) { Error=TEXT("Original int128/128/16/32 default proof failed before write; Native owns int64 clamps.");return false; }
 }
 auto Sub=UToolsetRegistrySubsystem::Get(TEXT("Prepare exact Physics inspector path defaults"));if(!Sub.HasValue()) { Error=TEXT("Registry unavailable; no fallback converter.");return false; }
 struct FPreparedPath { FProperty* P; FName Key; FString Before; TSharedRef<FJsonObject> Row; };
 TArray<FPreparedPath> Prepared;TArray<TSharedPtr<FJsonValue>> Observations;
 for(const TCHAR* Name:{TEXT("physics_asset_path"),TEXT("skeletal_mesh_path")})
 {
  auto P=ValidParam(Name);const FName Key(*FString::Printf(TEXT("CPP_Default_%s"),Name));const FString Prior=F->GetMetaData(Key);
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("method"),F->GetName());Row->SetStringField(TEXT("property"),Name);
  Row->SetBoolField(TEXT("metadata_present_before"),F->HasMetaData(Key));Row->SetStringField(TEXT("metadata_before"),Prior);
  Row->SetBoolField(TEXT("metadata_write_attempted"),false);Row->SetBoolField(TEXT("metadata_write_returned"),false);
  Observations.Add(MakeShared<FJsonValueObject>(Row));R->SetArrayField(TEXT("property_observations"),Observations);
  if(!MatchPhysicsAssetInspectionPath(P)) { Error=TEXT("Exact optional path string-empty declaration required before all writes.");return false; }
  auto Conv=Sub.GetValue()->ToolsetRegistry.GetConverterForProperty(P);const bool Active=Conv.IsValid()&&Conv->GetName()==SkeletalConverterName;Row->SetBoolField(TEXT("exact_converter_active"),Active);
  if(!Active) { Error=TEXT("Retained exact Skeletal path-default converter unavailable.");return false; }
  auto Type=Conv->PropertyToJsonSchema(P);auto D=SkeletalDefault(P,Prior);FString Kind;
  if(!Type.IsValid()||Type->Values.Num()!=1||!Type->TryGetStringField(TEXT("type"),Kind)||Kind!=TEXT("string")||!D.IsValid()||D->Type!=EJson::String||!D->AsString().IsEmpty()) { Error=TEXT("Exact path string/default proof failed before all metadata writes.");return false; }
  Row->SetObjectField(TEXT("type_schema"),Type);Row->SetField(TEXT("public_default"),D);Prepared.Add({P,Key,Prior,Row});
 }
 for(auto& V:Prepared)
 {
  if(V.Before.IsEmpty())
  {
   V.Row->SetBoolField(TEXT("metadata_write_attempted"),true);R->SetBoolField(TEXT("metadata_mutation_attempted"),true);R->SetNumberField(TEXT("metadata_write_attempt_count"),R->GetNumberField(TEXT("metadata_write_attempt_count"))+1);
   F->SetMetaData(V.Key,SkeletalPreparedEmpty);
   V.Row->SetBoolField(TEXT("metadata_write_returned"),true);R->SetNumberField(TEXT("metadata_write_return_count"),R->GetNumberField(TEXT("metadata_write_return_count"))+1);
   const bool Changed=!V.Before.Equals(F->GetMetaData(V.Key),ESearchCase::CaseSensitive);
   if(Changed) { R->SetBoolField(TEXT("metadata_changed"),true);R->SetNumberField(TEXT("metadata_changed_count"),R->GetNumberField(TEXT("metadata_changed_count"))+1); }
  }
  V.Row->SetStringField(TEXT("metadata_after"),F->GetMetaData(V.Key));auto Conv=Sub.GetValue()->ToolsetRegistry.GetConverterForProperty(V.P);
  if(!Conv.IsValid()||Conv->GetName()!=SkeletalConverterName||!F->GetMetaData(V.Key).Equals(SkeletalPreparedEmpty,ESearchCase::CaseSensitive)) { Error=TEXT("Prepared path metadata/converter readback failed; actual RAM prefix retained.");return false; }
  auto D=Conv->PropertyToDefault(V.P,F->GetMetaData(V.Key));if(!D.IsValid()||D->Type!=EJson::String||!D->AsString().IsEmpty()) { Error=TEXT("Prepared original empty path default differs; no retry.");return false; }
  Defaults->SetField(F->GetName()+TEXT(".")+V.P->GetName(),D);
  R->SetNumberField(TEXT("verified_property_count"),R->GetNumberField(TEXT("verified_property_count"))+1);V.Row->SetBoolField(TEXT("type_schema_verified"),true);V.Row->SetBoolField(TEXT("default_verified"),true);
 }
 R->SetBoolField(TEXT("default_verified"),true);R->SetBoolField(TEXT("exact_converter_active"),true);R->SetBoolField(TEXT("type_schema_verified"),true);return true;
}


// CF: original PawnProperties6 exact empty enum string and four optional booleans only.
namespace
{
    constexpr const TCHAR* PawnPropertiesClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/toolsets_PY.CubelessBlueprintTools");
    constexpr const TCHAR* PawnPropertiesConverterName = TEXT("CubelessMCP_UE58.PawnPropertiesDefaults.v1");
    constexpr const TCHAR* PawnPreparedEmpty = TEXT("\"\"");
    const FProperty* MatchPawnPropertiesProperty(const FProperty* Property)
    {
        const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
        if (!Property || Property->ArrayDim != 1 || !Property->HasAllPropertyFlags(CPF_Parm)
            || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || !Owner || Owner->GetPathName() != PawnPropertiesClass
            || Function->GetName() != TEXT("set_pawn_properties")) { return nullptr; }
        const FString Name = Property->GetName();
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), *Name));
        if (!Function->HasMetaData(Key) || !Function->GetMetaData(*Name).IsEmpty()) { return nullptr; }
        if (Name == TEXT("use_controller_rotation_yaw") || Name == TEXT("use_controller_rotation_pitch")
            || Name == TEXT("use_controller_rotation_roll") || Name == TEXT("can_be_damaged"))
        {
            const auto Optional = CastField<FOptionalProperty>(Property);
            const auto Inner = Optional ? Optional->GetValueProperty() : nullptr;
            return Optional && Inner && Inner->ArrayDim == 1 && CastField<FBoolProperty>(Inner)
                && Function->GetMetaData(Key) == TEXT("()") ? Property : nullptr;
        }
        if (Name != TEXT("auto_possess_player")) { return nullptr; }
        const FString Text = Function->GetMetaData(Key);
        return CastField<FStrProperty>(Property) && (Text.IsEmpty() || Text == PawnPreparedEmpty) ? Property : nullptr;
    }

    // DX: only the reflected sheet13 srgb parameter reuses the retained Pawn converter.
    const FProperty* MatchVolumeTextureOptionalProperty(const FProperty* Property)
    {
        const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
        if (!Property || Property->ArrayDim != 1 || !Property->HasAllPropertyFlags(CPF_Parm)
            || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || !Owner || !Owner->GetPathName().Equals(TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/volume_texture_toolset_PY.CubelessVolumeTextureTools"), ESearchCase::CaseSensitive)
            || !Function->GetName().Equals(TEXT("create_volume_texture_from_2d_sheet_mcp"), ESearchCase::CaseSensitive)
            || !Property->GetName().Equals(TEXT("srgb"), ESearchCase::CaseSensitive)) { return nullptr; }
        const FName Key(TEXT("CPP_Default_srgb"));
        if (!Function->HasMetaData(Key) || !Function->GetMetaData(TEXT("srgb")).IsEmpty()
            || Function->GetMetaData(Key) != TEXT("()")) { return nullptr; }
        const auto Optional = CastField<FOptionalProperty>(Property);
        const auto Inner = Optional ? Optional->GetValueProperty() : nullptr;
        return Optional && Inner && Inner->ArrayDim == 1 && CastField<FBoolProperty>(Inner) ? Property : nullptr;
    }

    TSharedPtr<FJsonValue> PawnPropertiesPublicDefault(const FProperty* Property, const FString& Text)
    {
        if (!MatchPawnPropertiesProperty(Property) && !MatchVolumeTextureOptionalProperty(Property)) { return nullptr; }
        if (const auto Optional = CastField<FOptionalProperty>(Property))
        {
            if (Text != TEXT("()")) { return nullptr; }
            FDefaultConstructedPropertyElement Scratch(Optional);
            const TCHAR* End = Optional->ImportText_Direct(*Text, Scratch.GetObjAddress(), nullptr, PPF_None);
            if (!End || *End != TCHAR('\0') || Optional->IsSet(Scratch.GetObjAddress())) { return nullptr; }
            return FJsonObjectConverter::UPropertyToJsonValue(const_cast<FOptionalProperty*>(Optional), Scratch.GetObjAddress());
        }
        const auto String = CastField<FStrProperty>(Property);
        if (!String || (!Text.IsEmpty() && Text != PawnPreparedEmpty)) { return nullptr; }
        FDefaultConstructedPropertyElement Scratch(String);
        const TCHAR* End = String->ImportText_Direct(PawnPreparedEmpty, Scratch.GetObjAddress(), nullptr, PPF_Delimited);
        if (!End || *End != TCHAR('\0') || !String->GetPropertyValue(Scratch.GetObjAddress()).IsEmpty()) { return nullptr; }
        return MakeShared<FJsonValueString>(FString());
    }
    class FPawnPropertiesDefaultsConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
    {
    public:
        virtual FString GetName() const override { return PawnPropertiesConverterName; }
        virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override
        { return MatchPawnPropertiesProperty(Property) != nullptr || MatchVolumeTextureOptionalProperty(Property) != nullptr; }
        virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
        {
            auto Schema = MakeShared<FJsonObject>(); if (!MatchPawnPropertiesProperty(Property) && !MatchVolumeTextureOptionalProperty(Property)) { return Schema; }
            if (CastField<FOptionalProperty>(Property))
            { Schema->SetArrayField(TEXT("type"), { MakeShared<FJsonValueString>(TEXT("boolean")), MakeShared<FJsonValueString>(TEXT("null")) }); }
            else { Schema->SetStringField(TEXT("type"), TEXT("string")); }
            return Schema; // No embedded TYPE-schema null; default has its separate callback.
        }
        virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> Property, const FString& Text) override
        {
            const auto Default = PawnPropertiesPublicDefault(Property, Text); if (Default.IsValid()) { return Default; }
            UKismetSystemLibrary::RaiseScriptError(TEXT("Exact Pawn properties default is invalid; registration must fail."));
            return MakeShared<FJsonValueNull>();
        }
        virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
        { return Value && (MatchPawnPropertiesProperty(Property) || MatchVolumeTextureOptionalProperty(Property)) ? FJsonObjectConverter::UPropertyToJsonValue(Property, Value) : nullptr; }
        virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& JsonValue, TNotNull<FProperty*> Property, void* OutValue, UObject* Outer) override
        {
            if (!OutValue || !JsonValue.IsValid() || (!MatchPawnPropertiesProperty(Property) && !MatchVolumeTextureOptionalProperty(Property))) { return false; }
            if (const auto Optional = CastField<FOptionalProperty>(Property))
            {
                if (JsonValue->Type == EJson::Null) { Optional->MarkUnset(OutValue); return true; }
                if (JsonValue->Type == EJson::Boolean)
                { return FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, OutValue, 0, 0, false, nullptr, nullptr); }
            }
            else if (const auto String = CastField<FStrProperty>(Property))
            {
                if (JsonValue->Type == EJson::String) { String->SetPropertyValue(OutValue, JsonValue->AsString()); return true; }
            }
            UKismetSystemLibrary::RaiseScriptError(TEXT("Pawn properties require auto_possess_player string and four rotation/damage fields require boolean/null; no coercions."));
            return false;
        }
    };
}

bool Cubeless::Registration::VerifyPawnPropertiesToolset(UClass* ToolsetClass,
    const TSharedRef<FJsonObject>& Report, FString& Error)
{
    Report->SetBoolField(TEXT("default_verified"), false); Report->SetBoolField(TEXT("exact_converter_active"), false);
    Report->SetBoolField(TEXT("nullable_schema_verified"), false); Report->SetBoolField(TEXT("actual_call_packing_verified"), false);
    Report->SetBoolField(TEXT("metadata_mutation_attempted"), false); Report->SetBoolField(TEXT("metadata_changed"), false);
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetNumberField(TEXT("verified_property_count"), 0);
    auto Defaults = MakeShared<FJsonObject>(); Report->SetObjectField(TEXT("defaults"), Defaults);
    TArray<TSharedPtr<FJsonValue>> BeforeRows, Rows, Changes;
    Report->SetArrayField(TEXT("metadata_before"), BeforeRows); Report->SetArrayField(TEXT("property_observations"), Rows);
    Report->SetArrayField(TEXT("metadata_changes"), Changes);
    Report->SetStringField(TEXT("metadata_mutation_scope"), TEXT("At most one RAM-only quoted-empty CPP_Default attachment on one unregistered canonical Pawn-properties method. All five property kinds/converters/defaults are proven first. No optional default, asset/source/disk/default-value write or compiler/save."));
    if (!ToolsetClass || ToolsetClass->GetPathName() != PawnPropertiesClass)
    { Error = TEXT("Exact canonical Blueprint Pawn toolset class required."); return false; }
    UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("set_pawn_properties"));
    if (!Function || Function->GetOwnerClass() != ToolsetClass)
    { Error = TEXT("Exact canonical Pawn properties method required."); return false; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Verify exact Pawn properties defaults"));
    if (!Subsystem.HasValue()) { Error = TEXT("Toolset registry unavailable; no default changes."); return false; }
    const TCHAR* Names[] = { TEXT("auto_possess_player"), TEXT("use_controller_rotation_yaw"), TEXT("use_controller_rotation_pitch"), TEXT("use_controller_rotation_roll"), TEXT("can_be_damaged") };
    TArray<FProperty*> Properties; TArray<FString> Prior;
    for (const TCHAR* Name : Names)
    {
        FProperty* Property = FindFProperty<FProperty>(Function, Name);
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), Name));
        auto Before = MakeShared<FJsonObject>(); Before->SetStringField(TEXT("property"), Name);
        Before->SetBoolField(TEXT("property_present"), Property != nullptr); Before->SetBoolField(TEXT("metadata_present"), Function->HasMetaData(Key));
        Before->SetStringField(TEXT("metadata"), Function->GetMetaData(Key)); BeforeRows.Add(MakeShared<FJsonValueObject>(Before));
        Report->SetArrayField(TEXT("metadata_before"), BeforeRows);
        if (!MatchPawnPropertiesProperty(Property)) { Error = TEXT("Exact five optional property kinds/raw defaults required before any metadata writes."); return false; }
        const auto Converter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
        const bool Active = Converter.IsValid() && Converter->GetName() == PawnPropertiesConverterName;
        Before->SetBoolField(TEXT("exact_converter_active"), Active);
        if (!Active) { Error = TEXT("Exact field-attached Pawn-properties converter unavailable; no fallback/retry."); return false; }
        const auto Schema = Converter->PropertyToJsonSchema(Property);
        const auto Default = PawnPropertiesPublicDefault(Property, Function->GetMetaData(Key));
        if (Schema.IsValid()) { Before->SetObjectField(TEXT("type_schema"), Schema); }
        if (Default.IsValid()) { Before->SetField(TEXT("public_default"), Default); }
        const bool Optional = CastField<FOptionalProperty>(Property) != nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Types = nullptr; FString Kind;
        const bool TypeOK = Schema.IsValid() && Schema->Values.Num() == 1 && (Optional
            ? Schema->TryGetArrayField(TEXT("type"), Types) && Types && Types->Num() == 2 && (*Types)[0].IsValid() && (*Types)[1].IsValid()
                && (*Types)[0]->Type == EJson::String && (*Types)[1]->Type == EJson::String
                && (*Types)[0]->AsString() == TEXT("boolean") && (*Types)[1]->AsString() == TEXT("null")
            : Schema->TryGetStringField(TEXT("type"), Kind) && Kind == TEXT("string"));
        const bool DefaultOK = Default.IsValid() && (Optional ? Default->Type == EJson::Null : Default->Type == EJson::String && Default->AsString().IsEmpty());
        Before->SetBoolField(TEXT("type_schema_verified"), TypeOK); Before->SetBoolField(TEXT("default_verified"), DefaultOK);
        if (!TypeOK || !DefaultOK) { Error = TEXT("Exact default/type proof differs; no metadata writes."); return false; }
        Defaults->SetField(FString::Printf(TEXT("set_pawn_properties.%s"), Name), Default);
        Properties.Add(Property); Prior.Add(Function->GetMetaData(Key));
    }
    int32 Writes = 0, Changed = 0, Verified = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        FProperty* Property = Properties[Index]; const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), Names[Index]));
        if (CastField<FStrProperty>(Property) && Prior[Index].IsEmpty())
        {
            Report->SetBoolField(TEXT("metadata_mutation_attempted"), true); Report->SetNumberField(TEXT("metadata_write_attempt_count"), ++Writes);
            Function->SetMetaData(Key, PawnPreparedEmpty);
            auto Change = MakeShared<FJsonObject>(); Change->SetStringField(TEXT("property"), Names[Index]);
            Change->SetStringField(TEXT("before"), Prior[Index]); Change->SetStringField(TEXT("after"), Function->GetMetaData(Key));
            Changes.Add(MakeShared<FJsonValueObject>(Change)); Report->SetArrayField(TEXT("metadata_changes"), Changes);
            const bool DidChange = Prior[Index] != Function->GetMetaData(Key); Changed += DidChange ? 1 : 0;
            Report->SetBoolField(TEXT("metadata_changed"), Changed > 0); Report->SetNumberField(TEXT("metadata_changed_count"), Changed);
            if (Function->GetMetaData(Key) != PawnPreparedEmpty) { Error = TEXT("Quoted-empty metadata attachment did not read back; partial RAM-only receipt retained, no retry."); return false; }
        }
        const auto Converter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
        const bool Active = Converter.IsValid() && Converter->GetName() == PawnPropertiesConverterName;
        const auto Default = Active ? Converter->PropertyToDefault(Property, Function->GetMetaData(Key)) : TSharedPtr<FJsonValue>();
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("property"), Names[Index]); Row->SetStringField(TEXT("metadata"), Function->GetMetaData(Key));
        Row->SetBoolField(TEXT("exact_converter_active"), Active); if (Default.IsValid()) { Row->SetField(TEXT("public_default"), Default); }
        Rows.Add(MakeShared<FJsonValueObject>(Row)); Report->SetArrayField(TEXT("property_observations"), Rows);
        if (!Active || !Default.IsValid()) { Error = TEXT("Prepared converter/default proof failed; actual partial metadata receipt retained."); return false; }
        Report->SetNumberField(TEXT("verified_property_count"), ++Verified);
    }
    Report->SetStringField(TEXT("converter_name"), PawnPropertiesConverterName);
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("exact_converter_active"), true);
    Report->SetBoolField(TEXT("nullable_schema_verified"), true);
    Report->SetStringField(TEXT("verification_scope"), TEXT("Five exact parsed defaults and field-converter schemas on canonical Blueprint Pawn method only. Actual official schema/null/False/enum argument packing are separate gates; metadata changes are RAM-only with exact0..1 prefix."));
    return true;
}

bool Cubeless::Registration::VerifyVolumeTextureToolset(UClass* ToolsetClass,
    const TSharedRef<FJsonObject>& Report, FString& Error)
{
    Report->SetBoolField(TEXT("default_verified"), false);
    Report->SetBoolField(TEXT("exact_converter_active"), false);
    Report->SetBoolField(TEXT("nullable_schema_verified"), false);
    Report->SetBoolField(TEXT("actual_call_packing_verified"), false);
    Report->SetBoolField(TEXT("metadata_mutation_attempted"), false);
    Report->SetBoolField(TEXT("metadata_changed"), false);
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0);
    Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetNumberField(TEXT("verified_property_count"), 0);
    auto Defaults = MakeShared<FJsonObject>(); Report->SetObjectField(TEXT("defaults"), Defaults);
    Report->SetArrayField(TEXT("property_observations"), TArray<TSharedPtr<FJsonValue>>{});
    Report->SetStringField(TEXT("verification_scope"), TEXT("Read-only exact unregistered VolumeTextureTools sheet.srgb optional boolean/unset default and retained Pawn converter. No metadata/property/asset/source/disk write or compiler/save. Official schema/null/False packing remain separate gates."));
    if (!IsInGameThread() || !ToolsetClass
        || !ToolsetClass->GetPathName().Equals(TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/volume_texture_toolset_PY.CubelessVolumeTextureTools"), ESearchCase::CaseSensitive))
    { Error = TEXT("Exact current VolumeTextureTools class on the Editor game thread required."); return false; }
    if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass))
    { Error = TEXT("Exact class must be unregistered before read-only defaults verification; no invalidation/retry performed here."); return false; }
    UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("create_volume_texture_from_2d_sheet_mcp"));
    if (!Function || Function->GetOwnerClass() != ToolsetClass)
    { Error = TEXT("Exact own sheet method required."); return false; }
    FProperty* Property = FindFProperty<FProperty>(Function, TEXT("srgb"));
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("property"), TEXT("srgb"));
    Row->SetStringField(TEXT("method"), Function->GetName());
    Row->SetBoolField(TEXT("property_present"), Property != nullptr);
    Row->SetBoolField(TEXT("metadata_present"), Function->HasMetaData(TEXT("CPP_Default_srgb")));
    Row->SetStringField(TEXT("metadata"), Function->GetMetaData(TEXT("CPP_Default_srgb")));
    Row->SetBoolField(TEXT("metadata_write_attempted"), false);
    Row->SetBoolField(TEXT("metadata_write_returned"), false);
    Report->SetArrayField(TEXT("property_observations"), {MakeShared<FJsonValueObject>(Row)});
    if (!MatchVolumeTextureOptionalProperty(Property))
    { Error = TEXT("Exact srgb optional boolean property and unmodified unset CPP_Default required."); return false; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Verify exact VolumeTexture srgb optional default"));
    if (!Subsystem.HasValue()) { Error = TEXT("Toolset registry unavailable; no fallback converter."); return false; }
    const auto Converter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
    const bool Active = Converter.IsValid() && Converter->GetName() == PawnPropertiesConverterName;
    Row->SetBoolField(TEXT("exact_converter_active"), Active);
    if (!Active) { Error = TEXT("Retained exact Pawn optional-boolean converter is not active for sheet.srgb."); return false; }
    const auto Schema = Converter->PropertyToJsonSchema(Property);
    const auto Default = Converter->PropertyToDefault(Property, Function->GetMetaData(TEXT("CPP_Default_srgb")));
    const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
    const bool TypeOK = Schema.IsValid() && Schema->Values.Num() == 1
        && Schema->TryGetArrayField(TEXT("type"), Types) && Types && Types->Num() == 2
        && (*Types)[0].IsValid() && (*Types)[1].IsValid()
        && (*Types)[0]->Type == EJson::String && (*Types)[1]->Type == EJson::String
        && (*Types)[0]->AsString() == TEXT("boolean") && (*Types)[1]->AsString() == TEXT("null");
    const bool DefaultOK = Default.IsValid() && Default->Type == EJson::Null;
    if (Schema.IsValid()) { Row->SetObjectField(TEXT("type_schema"), Schema); }
    if (Default.IsValid()) { Row->SetField(TEXT("public_default"), Default); }
    Row->SetBoolField(TEXT("type_schema_verified"), TypeOK);
    Row->SetBoolField(TEXT("default_verified"), DefaultOK);
    if (!TypeOK || !DefaultOK)
    { Error = TEXT("Exact optional boolean/null schema or scratch-imported unset default differs."); return false; }
    Defaults->SetField(TEXT("create_volume_texture_from_2d_sheet_mcp.srgb"), Default);
    Report->SetStringField(TEXT("converter_name"), PawnPropertiesConverterName);
    Report->SetBoolField(TEXT("default_verified"), true);
    Report->SetBoolField(TEXT("exact_converter_active"), true);
    Report->SetBoolField(TEXT("nullable_schema_verified"), true);
    Report->SetNumberField(TEXT("verified_property_count"), 1);
    return true;
}


// CE: original CustomEventMetadata10 exact strings and optional boolean only.
namespace
{
    constexpr const TCHAR* CustomEventMetadataClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/graph_toolset_PY.CubelessBlueprintGraphTools");
    constexpr const TCHAR* CustomEventMetadataConverterName = TEXT("CubelessMCP_UE58.BlueprintCustomEventMetadataDefaults.v1");
    constexpr const TCHAR* CustomEventPreparedEmpty = TEXT("\"\"");
    const FProperty* MatchCustomEventMetadataProperty(const FProperty* Property, FString* FailureKind = nullptr)
    {
        if (FailureKind) { FailureKind->Reset(); }
        auto Reject = [&](const TCHAR* Kind) -> const FProperty* { if (FailureKind) { *FailureKind = Kind; } return nullptr; };
        const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
        if (!Property || Property->ArrayDim != 1 || !Property->HasAllPropertyFlags(CPF_Parm)
            || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || !Owner || Owner->GetPathName() != CustomEventMetadataClass
            || Function->GetName() != TEXT("set_blueprint_custom_event_metadata")) { return Reject(TEXT("exact_owner_method_parameter_required")); }
        const FString Name = Property->GetName();
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), *Name));
        if (!Function->HasMetaData(Key)) { return Reject(TEXT("CPP_Default_metadata_missing")); }
        // The exact tooltip argument shares FName with the standard function ToolTip doc.
        // Its declared CPP_Default is authoritative; other five named overrides remain refused.
        if (Name != TEXT("tooltip") && !Function->GetMetaData(*Name).IsEmpty()) { return Reject(TEXT("named_parameter_metadata_override")); }
        if (Name == TEXT("call_in_editor"))
        {
            const auto Optional = CastField<FOptionalProperty>(Property);
            const auto Inner = Optional ? Optional->GetValueProperty() : nullptr;
            if (!Optional || !Inner || Inner->ArrayDim != 1 || !CastField<FBoolProperty>(Inner)) { return Reject(TEXT("optional_boolean_kind_required")); }
            return Function->GetMetaData(Key) == TEXT("()") ? Property : Reject(TEXT("optional_unset_CPP_Default_required"));
        }
        if (Name != TEXT("custom_event_name") && Name != TEXT("description") && Name != TEXT("node_id")
            && Name != TEXT("event_name") && Name != TEXT("tooltip")) { return Reject(TEXT("exact_six_field_name_required")); }
        const FString Text = Function->GetMetaData(Key);
        if (!CastField<FStrProperty>(Property)) { return Reject(TEXT("string_kind_required")); }
        return Text.IsEmpty() || Text == CustomEventPreparedEmpty ? Property : Reject(TEXT("empty_CPP_Default_required"));
    }
    TSharedPtr<FJsonValue> CustomEventMetadataPublicDefault(const FProperty* Property, const FString& Text)
    {
        if (!MatchCustomEventMetadataProperty(Property)) { return nullptr; }
        if (const auto Optional = CastField<FOptionalProperty>(Property))
        {
            if (Text != TEXT("()")) { return nullptr; }
            FDefaultConstructedPropertyElement Scratch(Optional);
            const TCHAR* End = Optional->ImportText_Direct(*Text, Scratch.GetObjAddress(), nullptr, PPF_None);
            if (!End || *End != TCHAR('\0') || Optional->IsSet(Scratch.GetObjAddress())) { return nullptr; }
            return FJsonObjectConverter::UPropertyToJsonValue(const_cast<FOptionalProperty*>(Optional), Scratch.GetObjAddress());
        }
        const auto String = CastField<FStrProperty>(Property);
        // Stock K2 default lookup prioritizes function ToolTip over CPP_Default_tooltip.
        // Project only this exact argument from its separately validated declared default.
        const auto Function = Cast<UFunction>(Property->GetOwnerUObject());
        const FString DeclaredText = Property->GetName() == TEXT("tooltip")
            ? Function->GetMetaData(TEXT("CPP_Default_tooltip")) : Text;
        if (!String || (!DeclaredText.IsEmpty() && DeclaredText != CustomEventPreparedEmpty)) { return nullptr; }
        FDefaultConstructedPropertyElement Scratch(String);
        const TCHAR* End = String->ImportText_Direct(CustomEventPreparedEmpty, Scratch.GetObjAddress(), nullptr, PPF_Delimited);
        if (!End || *End != TCHAR('\0') || !String->GetPropertyValue(Scratch.GetObjAddress()).IsEmpty()) { return nullptr; }
        return MakeShared<FJsonValueString>(FString());
    }
    class FBlueprintCustomEventMetadataDefaultsConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
    {
    public:
        virtual FString GetName() const override { return CustomEventMetadataConverterName; }
        virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override
        { return MatchCustomEventMetadataProperty(Property) != nullptr; }
        virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
        {
            auto Schema = MakeShared<FJsonObject>(); if (!MatchCustomEventMetadataProperty(Property)) { return Schema; }
            if (CastField<FOptionalProperty>(Property))
            { Schema->SetArrayField(TEXT("type"), { MakeShared<FJsonValueString>(TEXT("boolean")), MakeShared<FJsonValueString>(TEXT("null")) }); }
            else { Schema->SetStringField(TEXT("type"), TEXT("string")); }
            return Schema; // No embedded TYPE-schema null; default has its separate callback.
        }
        virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> Property, const FString& Text) override
        {
            const auto Default = CustomEventMetadataPublicDefault(Property, Text); if (Default.IsValid()) { return Default; }
            UKismetSystemLibrary::RaiseScriptError(TEXT("Exact custom-event metadata default is invalid; registration must fail."));
            return MakeShared<FJsonValueNull>();
        }
        virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
        { return Value && MatchCustomEventMetadataProperty(Property) ? FJsonObjectConverter::UPropertyToJsonValue(Property, Value) : nullptr; }
        virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& JsonValue, TNotNull<FProperty*> Property, void* OutValue, UObject* Outer) override
        {
            if (!OutValue || !JsonValue.IsValid() || !MatchCustomEventMetadataProperty(Property)) { return false; }
            if (const auto Optional = CastField<FOptionalProperty>(Property))
            {
                if (JsonValue->Type == EJson::Null) { Optional->MarkUnset(OutValue); return true; }
                if (JsonValue->Type == EJson::Boolean)
                { return FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, OutValue, 0, 0, false, nullptr, nullptr); }
            }
            else if (const auto String = CastField<FStrProperty>(Property))
            {
                if (JsonValue->Type == EJson::String) { String->SetPropertyValue(OutValue, JsonValue->AsString()); return true; }
            }
            UKismetSystemLibrary::RaiseScriptError(TEXT("Custom-event metadata fields require strings and call_in_editor requires boolean/null; no coercions."));
            return false;
        }
    };
}

bool Cubeless::Registration::VerifyBlueprintCustomEventMetadataToolset(UClass* ToolsetClass,
    const TSharedRef<FJsonObject>& Report, FString& Error)
{
    Report->SetBoolField(TEXT("default_verified"), false); Report->SetBoolField(TEXT("exact_converter_active"), false);
    Report->SetBoolField(TEXT("nullable_schema_verified"), false); Report->SetBoolField(TEXT("actual_call_packing_verified"), false);
    Report->SetBoolField(TEXT("metadata_mutation_attempted"), false); Report->SetBoolField(TEXT("metadata_changed"), false);
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetNumberField(TEXT("verified_property_count"), 0);
    auto Defaults = MakeShared<FJsonObject>(); Report->SetObjectField(TEXT("defaults"), Defaults);
    TArray<TSharedPtr<FJsonValue>> BeforeRows, Rows, Changes;
    Report->SetArrayField(TEXT("metadata_before"), BeforeRows); Report->SetArrayField(TEXT("property_observations"), Rows);
    Report->SetArrayField(TEXT("metadata_changes"), Changes);
    Report->SetStringField(TEXT("metadata_mutation_scope"), TEXT("At most five ordered RAM-only quoted-empty CPP_Default attachments on one unregistered canonical custom-event method. All six property kinds/converters/defaults are proven first. No optional default, asset/source/disk/default-value write or compiler/save."));
    if (!ToolsetClass || ToolsetClass->GetPathName() != CustomEventMetadataClass)
    { Error = TEXT("Exact canonical Blueprint Graph toolset class required."); return false; }
    UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("set_blueprint_custom_event_metadata"));
    if (!Function || Function->GetOwnerClass() != ToolsetClass)
    { Error = TEXT("Exact canonical custom-event metadata method required."); return false; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Verify exact custom-event metadata defaults"));
    if (!Subsystem.HasValue()) { Error = TEXT("Toolset registry unavailable; no default changes."); return false; }
    const TCHAR* Names[] = { TEXT("custom_event_name"), TEXT("description"), TEXT("node_id"), TEXT("event_name"), TEXT("tooltip"), TEXT("call_in_editor") };
    TArray<FProperty*> Properties; TArray<FString> Prior;
    for (const TCHAR* Name : Names)
    {
        FProperty* Property = FindFProperty<FProperty>(Function, Name);
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), Name));
        auto Before = MakeShared<FJsonObject>(); Before->SetStringField(TEXT("property"), Name);
        Before->SetBoolField(TEXT("property_present"), Property != nullptr); Before->SetBoolField(TEXT("metadata_present"), Function->HasMetaData(Key));
        Before->SetStringField(TEXT("metadata"), Function->GetMetaData(Key)); BeforeRows.Add(MakeShared<FJsonValueObject>(Before));
        Report->SetArrayField(TEXT("metadata_before"), BeforeRows);
        Before->SetStringField(TEXT("named_function_metadata"), Function->GetMetaData(FName(Name)));
        Before->SetStringField(TEXT("public_default_source"), FString(Name) == TEXT("tooltip") ? TEXT("CPP_Default_tooltip; preserve colliding function ToolTip documentation") : TEXT("validated CPP_Default metadata with no named override"));
        FString FailureKind; const bool Matched = MatchCustomEventMetadataProperty(Property, &FailureKind) != nullptr;
        Before->SetBoolField(TEXT("property_match_verified"), Matched); Before->SetStringField(TEXT("match_failure_kind"), FailureKind);
        if (!Matched)
        {
            Report->SetStringField(TEXT("prevalidation_failure_property"), Name); Report->SetStringField(TEXT("prevalidation_failure_kind"), FailureKind);
            Error = FString::Printf(TEXT("Custom-event property '%s' failed '%s' before any metadata writes."), Name, *FailureKind); return false;
        }
        const auto Converter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
        const bool Active = Converter.IsValid() && Converter->GetName() == CustomEventMetadataConverterName;
        Before->SetBoolField(TEXT("exact_converter_active"), Active);
        if (!Active) { Error = TEXT("Exact field-attached custom-event converter unavailable; no fallback/retry."); return false; }
        const auto Schema = Converter->PropertyToJsonSchema(Property);
        const auto Default = CustomEventMetadataPublicDefault(Property, Function->GetMetaData(Key));
        if (Schema.IsValid()) { Before->SetObjectField(TEXT("type_schema"), Schema); }
        if (Default.IsValid()) { Before->SetField(TEXT("public_default"), Default); }
        const bool Optional = CastField<FOptionalProperty>(Property) != nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Types = nullptr; FString Kind;
        const bool TypeOK = Schema.IsValid() && Schema->Values.Num() == 1 && (Optional
            ? Schema->TryGetArrayField(TEXT("type"), Types) && Types && Types->Num() == 2 && (*Types)[0].IsValid() && (*Types)[1].IsValid()
                && (*Types)[0]->Type == EJson::String && (*Types)[1]->Type == EJson::String
                && (*Types)[0]->AsString() == TEXT("boolean") && (*Types)[1]->AsString() == TEXT("null")
            : Schema->TryGetStringField(TEXT("type"), Kind) && Kind == TEXT("string"));
        const bool DefaultOK = Default.IsValid() && (Optional ? Default->Type == EJson::Null : Default->Type == EJson::String && Default->AsString().IsEmpty());
        Before->SetBoolField(TEXT("type_schema_verified"), TypeOK); Before->SetBoolField(TEXT("default_verified"), DefaultOK);
        if (!TypeOK || !DefaultOK) { Error = TEXT("Exact default/type proof differs; no metadata writes."); return false; }
        Defaults->SetField(FString::Printf(TEXT("set_blueprint_custom_event_metadata.%s"), Name), Default);
        Properties.Add(Property); Prior.Add(Function->GetMetaData(Key));
    }
    int32 Writes = 0, Changed = 0, Verified = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        FProperty* Property = Properties[Index]; const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), Names[Index]));
        if (CastField<FStrProperty>(Property) && Prior[Index].IsEmpty())
        {
            Report->SetBoolField(TEXT("metadata_mutation_attempted"), true); Report->SetNumberField(TEXT("metadata_write_attempt_count"), ++Writes);
            Function->SetMetaData(Key, CustomEventPreparedEmpty);
            auto Change = MakeShared<FJsonObject>(); Change->SetStringField(TEXT("property"), Names[Index]);
            Change->SetStringField(TEXT("before"), Prior[Index]); Change->SetStringField(TEXT("after"), Function->GetMetaData(Key));
            Changes.Add(MakeShared<FJsonValueObject>(Change)); Report->SetArrayField(TEXT("metadata_changes"), Changes);
            const bool DidChange = Prior[Index] != Function->GetMetaData(Key); Changed += DidChange ? 1 : 0;
            Report->SetBoolField(TEXT("metadata_changed"), Changed > 0); Report->SetNumberField(TEXT("metadata_changed_count"), Changed);
            if (Function->GetMetaData(Key) != CustomEventPreparedEmpty) { Error = TEXT("Quoted-empty metadata attachment did not read back; partial RAM-only receipt retained, no retry."); return false; }
        }
        const auto Converter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
        const bool Active = Converter.IsValid() && Converter->GetName() == CustomEventMetadataConverterName;
        const auto Default = Active ? Converter->PropertyToDefault(Property, Function->GetMetaData(Key)) : TSharedPtr<FJsonValue>();
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("property"), Names[Index]); Row->SetStringField(TEXT("metadata"), Function->GetMetaData(Key));
        Row->SetBoolField(TEXT("exact_converter_active"), Active); if (Default.IsValid()) { Row->SetField(TEXT("public_default"), Default); }
        Rows.Add(MakeShared<FJsonValueObject>(Row)); Report->SetArrayField(TEXT("property_observations"), Rows);
        if (!Active || !Default.IsValid()) { Error = TEXT("Prepared converter/default proof failed; actual partial metadata receipt retained."); return false; }
        Report->SetNumberField(TEXT("verified_property_count"), ++Verified);
    }
    Report->SetStringField(TEXT("converter_name"), CustomEventMetadataConverterName);
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("exact_converter_active"), true);
    Report->SetBoolField(TEXT("nullable_schema_verified"), true);
    Report->SetStringField(TEXT("verification_scope"), TEXT("Six exact parsed defaults and field-converter schemas on canonical Graph method only. Actual official schema/null/False/alias argument packing are separate gates; metadata changes are RAM-only with exact0..5 prefix."));
    return true;
}

// BZ: exact field-attached schema union; retain stock optional value packing.
namespace
{
    constexpr const TCHAR* VariableLifecycleClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/toolsets_PY.CubelessBlueprintTools");
    constexpr const TCHAR* VariableLifecycleConverterName = TEXT("CubelessMCP_UE58.BlueprintVariableMetadataNullable.v1");

    const FOptionalProperty* MatchVariableMetadataOptional(const FProperty* Property)
    {
        const auto Optional = CastField<FOptionalProperty>(Property);
        const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
        if (!Optional || Optional->ArrayDim != 1 || !Optional->HasAllPropertyFlags(CPF_Parm)
            || Optional->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || !Owner || Owner->GetPathName() != VariableLifecycleClass
            || Function->GetName() != TEXT("set_blueprint_variable_metadata")) { return nullptr; }
        const FString Name = Optional->GetName();
        const bool IsMetadata = Name == TEXT("metadata");
        if (!IsMetadata && Name != TEXT("is_editable") && Name != TEXT("instance_editable") && Name != TEXT("expose_on_spawn")) { return nullptr; }
        const FProperty* Inner = Optional->GetValueProperty();
        const auto Struct = CastField<FStructProperty>(Inner);
        if (!Inner || Inner->ArrayDim != 1 || (IsMetadata
            ? (!Struct || !Struct->Struct || Struct->Struct->GetPathName() != TEXT("/Script/JsonUtilities.JsonObjectWrapper"))
            : CastField<FBoolProperty>(Inner) == nullptr)) { return nullptr; }
        const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"), *Name));
        if (!Function->HasMetaData(DefaultKey) || Function->GetMetaData(DefaultKey) != TEXT("()")
            || !Function->GetMetaData(*Name).IsEmpty()) { return nullptr; }
        return Optional;
    }

    TSharedPtr<FJsonValue> VariableOptionalNullDefault(const FProperty* Property, const FString& Text)
    {
        const auto Optional = MatchVariableMetadataOptional(Property);
        if (!Optional || Text != TEXT("()")) { return nullptr; }
        FDefaultConstructedPropertyElement Scratch(Optional);
        const TCHAR* End = Optional->ImportText_Direct(*Text, Scratch.GetObjAddress(), nullptr, PPF_None);
        if (!End || *End != TCHAR('\0') || Optional->IsSet(Scratch.GetObjAddress())) { return nullptr; }
        return FJsonObjectConverter::UPropertyToJsonValue(const_cast<FOptionalProperty*>(Optional), Scratch.GetObjAddress());
    }

    class FBlueprintVariableMetadataNullableConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
    {
    public:
        virtual FString GetName() const override { return VariableLifecycleConverterName; }
        virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override
        { return MatchVariableMetadataOptional(Property) != nullptr; }
        virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
        {
            auto Schema = MakeShared<FJsonObject>();
            const auto Optional = MatchVariableMetadataOptional(Property); if (!Optional) { return Schema; }
            Schema->SetArrayField(TEXT("type"), {
                MakeShared<FJsonValueString>(Optional->GetName() == TEXT("metadata") ? TEXT("object") : TEXT("boolean")),
                MakeShared<FJsonValueString>(TEXT("null")) });
            // TYPE schema contains strings only. Parsed defaultNull uses the separate callback.
            return Schema;
        }
        virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> Property, const FString& DefaultString) override
        {
            const auto Default = VariableOptionalNullDefault(Property, DefaultString);
            if (Default.IsValid() && Default->Type == EJson::Null) { return Default; }
            UKismetSystemLibrary::RaiseScriptError(TEXT("Exact Blueprint metadata optional-unset default is invalid; registration must fail."));
            return MakeShared<FJsonValueNull>();
        }
        virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
        {
            if (!Value || !MatchVariableMetadataOptional(Property)) { return nullptr; }
            return FJsonObjectConverter::UPropertyToJsonValue(Property, Value);
        }
        virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& JsonValue, TNotNull<FProperty*> Property,
            void* OutValue, UObject* Outer) override
        {
            const auto Optional = MatchVariableMetadataOptional(Property);
            if (!Optional || !OutValue || !JsonValue.IsValid()) { return false; }
            if (JsonValue->Type == EJson::Null) { Optional->MarkUnset(OutValue); return true; }
            const EJson Expected = Optional->GetName() == TEXT("metadata") ? EJson::Object : EJson::Boolean;
            if (JsonValue->Type != Expected)
            {
                UKismetSystemLibrary::RaiseScriptError(TEXT("Blueprint variable metadata requires object/null and optional flags require boolean/null; no coercions."));
                return false;
            }
            // No Toolset callback recursion; stock Optional and JsonObjectWrapper own non-null packing.
            return FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, OutValue, 0, 0, false, nullptr, nullptr);
        }
    };
}

bool Cubeless::Registration::VerifyBlueprintVariableLifecycleToolset(UClass* ToolsetClass,
    const TSharedRef<FJsonObject>& Report, FString& Error)
{
    Report->SetBoolField(TEXT("default_verified"), false);
    Report->SetBoolField(TEXT("exact_converter_active"), false);
    Report->SetBoolField(TEXT("nullable_schema_verified"), false);
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0);
    Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetNumberField(TEXT("verified_property_count"), 0);
    Report->SetBoolField(TEXT("actual_call_packing_verified"), false);
    auto Defaults = MakeShared<FJsonObject>(); Report->SetObjectField(TEXT("defaults"), Defaults);
    TArray<TSharedPtr<FJsonValue>> Rows; Report->SetArrayField(TEXT("property_observations"), Rows);
    if (!IsInGameThread() || !ToolsetClass || ToolsetClass->GetPathName() != VariableLifecycleClass)
    { Error = TEXT("Expected exact canonical CubelessBlueprintTools generated class on the game thread."); return false; }
    UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("set_blueprint_variable_metadata"));
    if (!Function) { Error = TEXT("Canonical metadata method is unavailable."); return false; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Blueprint variable metadata nullable verification"));
    if (!Subsystem.HasValue()) { Error = TEXT("ToolsetRegistry subsystem unavailable; no registration retry."); return false; }
    int32 Verified = 0;
    for (const TCHAR* Name : { TEXT("metadata"), TEXT("is_editable"), TEXT("instance_editable"), TEXT("expose_on_spawn") })
    {
        FProperty* Property = FindFProperty<FProperty>(Function, Name);
        const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"), Name));
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("function"), Function->GetName());
        Row->SetStringField(TEXT("property"), Name); Row->SetBoolField(TEXT("property_present"), Property != nullptr);
        Row->SetBoolField(TEXT("metadata_present"), Function->HasMetaData(DefaultKey));
        Row->SetStringField(TEXT("metadata"), Function->GetMetaData(DefaultKey));
        Rows.Add(MakeShared<FJsonValueObject>(Row)); Report->SetArrayField(TEXT("property_observations"), Rows);
        const auto Optional = MatchVariableMetadataOptional(Property);
        Row->SetBoolField(TEXT("exact_optional_kind"), Optional != nullptr);
        if (!Optional) { Error = TEXT("Exact optional kinds/default-unset CPP metadata required; no metadata writes."); return false; }
        Row->SetStringField(TEXT("inner_cpp_type"), Optional->GetValueProperty()->GetCPPType());
        const auto Converter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property);
        const bool Active = Converter.IsValid() && Converter->GetName() == VariableLifecycleConverterName;
        Row->SetBoolField(TEXT("exact_converter_active"), Active);
        if (!Active) { Error = TEXT("Exact field-attached nullable converter unavailable; no fallback/retry."); return false; }
        const auto Schema = Converter->PropertyToJsonSchema(Property);
        const auto Default = Converter->PropertyToDefault(Property, Function->GetMetaData(DefaultKey));
        if (Schema.IsValid()) { Row->SetObjectField(TEXT("type_schema"), Schema); }
        if (Default.IsValid()) { Row->SetField(TEXT("public_default"), Default); Defaults->SetField(FString::Printf(TEXT("set_blueprint_variable_metadata.%s"), Name), Default); }
        const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
        const FString Expected = FString(Name) == TEXT("metadata") ? TEXT("object") : TEXT("boolean");
        const bool TypeOK = Schema.IsValid() && Schema->Values.Num() == 1 && Schema->TryGetArrayField(TEXT("type"), Types)
            && Types && Types->Num() == 2 && (*Types)[0].IsValid() && (*Types)[1].IsValid()
            && (*Types)[0]->Type == EJson::String && (*Types)[1]->Type == EJson::String
            && (*Types)[0]->AsString() == Expected && (*Types)[1]->AsString() == TEXT("null");
        const bool DefaultOK = Default.IsValid() && Default->Type == EJson::Null;
        Row->SetBoolField(TEXT("nullable_type_verified"), TypeOK); Row->SetBoolField(TEXT("default_unset_verified"), DefaultOK);
        if (!TypeOK || !DefaultOK) { Error = TEXT("Converter type union or parsed stock optional-unset default differs."); return false; }
        Report->SetNumberField(TEXT("verified_property_count"), ++Verified);
    }
    Report->SetStringField(TEXT("converter_name"), VariableLifecycleConverterName);
    Report->SetBoolField(TEXT("default_verified"), true);
    Report->SetBoolField(TEXT("exact_converter_active"), true);
    Report->SetBoolField(TEXT("nullable_schema_verified"), true);
    Report->SetStringField(TEXT("verification_scope"), TEXT("Direct registered converter type union and parsed CPP optional-unset defaults for four canonical fields. Official full schema and null/False call packing are separate observed gates; no metadata writes/asset work."));
    return true;
}


// CB: direct internal Array(FString)/empty default projects public array/null/defaultNone.
namespace
{
    constexpr const TCHAR* CategorySortingClass = TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/toolsets_PY.CubelessBlueprintTools");
    constexpr const TCHAR* CategorySortingConverterName = TEXT("CubelessMCP_UE58.BlueprintCategorySortingDefaults.v1");
    constexpr const TCHAR* CategoryPreparedEmpty = TEXT("\"\"");
    constexpr const TCHAR* CategoryPreparedArray = TEXT("()");
    const FProperty* MatchCategorySortingProperty(const FProperty* Property)
    {
        const auto Function = Property ? Cast<UFunction>(Property->GetOwnerUObject()) : nullptr;
        const auto Owner = Function ? Function->GetOwnerClass() : nullptr;
        if (!Property || Property->ArrayDim != 1 || !Property->HasAllPropertyFlags(CPF_Parm)
            || Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm | CPF_ReferenceParm)
            || !Owner || Owner->GetPathName() != CategorySortingClass || Function->GetName() != TEXT("set_blueprint_category_sorting")) { return nullptr; }
        const FString Name = Property->GetName(); if (Name != TEXT("category_order") && Name != TEXT("category_to_front")) { return nullptr; }
        const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), *Name));
        if (!Function->HasMetaData(Key) || !Function->GetMetaData(*Name).IsEmpty()) { return nullptr; }
        const FString Text = Function->GetMetaData(Key);
        if (Name == TEXT("category_order"))
        {
            const auto Array = CastField<FArrayProperty>(Property); const auto String = Array ? CastField<FStrProperty>(Array->Inner) : nullptr;
            return Array && String && String->ArrayDim == 1 && (Text.IsEmpty() || Text == CategoryPreparedArray) ? Property : nullptr;
        }
        return CastField<FStrProperty>(Property) && (Text.IsEmpty() || Text == CategoryPreparedEmpty) ? Property : nullptr;
    }
    TSharedPtr<FJsonValue> CategorySortingPublicDefault(const FProperty* Property, const FString& Text)
    {
        if (!MatchCategorySortingProperty(Property)) { return nullptr; }
        if (const auto Array = CastField<FArrayProperty>(Property))
        {
            if (!Text.IsEmpty() && Text != CategoryPreparedArray) { return nullptr; }
            FDefaultConstructedPropertyElement Scratch(Array), Canonical(Array);
            if (FScriptArrayHelper(Array, Scratch.GetObjAddress()).Num() != 0
                || FScriptArrayHelper(Array, Canonical.GetObjAddress()).Num() != 0) { return nullptr; }
            const TCHAR* End = Array->ImportText_Direct(*Text, Scratch.GetObjAddress(), nullptr, PPF_None);
            const TCHAR* CanonicalEnd = Array->ImportText_Direct(CategoryPreparedArray, Canonical.GetObjAddress(), nullptr, PPF_None);
            if (!End || *End != TCHAR('\0') || !CanonicalEnd || *CanonicalEnd != TCHAR('\0')
                || FScriptArrayHelper(Array, Scratch.GetObjAddress()).Num() != 0
                || FScriptArrayHelper(Array, Canonical.GetObjAddress()).Num() != 0) { return nullptr; }
            const auto Parsed = FJsonObjectConverter::UPropertyToJsonValue(const_cast<FArrayProperty*>(Array), Scratch.GetObjAddress());
            const auto ParsedCanonical = FJsonObjectConverter::UPropertyToJsonValue(const_cast<FArrayProperty*>(Array), Canonical.GetObjAddress());
            if (!Parsed.IsValid() || Parsed->Type != EJson::Array || !Parsed->AsArray().IsEmpty()
                || !ParsedCanonical.IsValid() || ParsedCanonical->Type != EJson::Array || !ParsedCanonical->AsArray().IsEmpty()
                || !Array->Identical(Scratch.GetObjAddress(), Canonical.GetObjAddress(), PPF_None)) { return nullptr; }
            // Stock empty-text and canonical () both prove internal []; public default remains None.
            return MakeShared<FJsonValueNull>();
        }
        const auto String = CastField<FStrProperty>(Property); if (!String || (!Text.IsEmpty() && Text != CategoryPreparedEmpty)) { return nullptr; }
        FDefaultConstructedPropertyElement Scratch(String);
        if (!Text.IsEmpty())
        {
            const TCHAR* End = String->ImportText_Direct(*Text, Scratch.GetObjAddress(), nullptr, PPF_Delimited);
            if (!End || *End != TCHAR('\0')) { return nullptr; }
        }
        const FString Value = String->GetPropertyValue(Scratch.GetObjAddress());
        if (!Value.IsEmpty()) { return nullptr; } return MakeShared<FJsonValueString>(Value);
    }
    class FBlueprintCategorySortingDefaultsConverter final : public UE::ToolsetRegistry::FToolsetJsonConverter
    {
    public:
        virtual FString GetName() const override { return CategorySortingConverterName; }
        virtual bool CanConvertProperty(TNotNull<const FProperty*> Property) override { return MatchCategorySortingProperty(Property) != nullptr; }
        virtual TSharedPtr<FJsonObject> PropertyToJsonSchema(TNotNull<const FProperty*> Property) override
        {
            auto Schema = MakeShared<FJsonObject>(); if (!MatchCategorySortingProperty(Property)) { return Schema; }
            if (CastField<FArrayProperty>(Property))
            {
                Schema->SetArrayField(TEXT("type"), { MakeShared<FJsonValueString>(TEXT("array")), MakeShared<FJsonValueString>(TEXT("null")) });
                auto Items = MakeShared<FJsonObject>(); Items->SetStringField(TEXT("type"), TEXT("string")); Schema->SetObjectField(TEXT("items"), Items);
            }
            else { Schema->SetStringField(TEXT("type"), TEXT("string")); }
            // No TYPE-schema default/JsonNull before stock schema Duplicate; default callback is separate.
            return Schema;
        }
        virtual TSharedPtr<FJsonValue> PropertyToDefault(TNotNull<const FProperty*> Property, const FString& Text) override
        {
            const auto Default = CategorySortingPublicDefault(Property, Text); if (Default.IsValid()) { return Default; }
            UKismetSystemLibrary::RaiseScriptError(TEXT("Exact category sorting parsed empty-array/null or empty-front default invalid; registration must fail."));
            return MakeShared<FJsonValueNull>();
        }
        virtual TSharedPtr<FJsonValue> PropertyToJsonData(TNotNull<FProperty*> Property, const void* Value) override
        { if (!Value || !MatchCategorySortingProperty(Property)) { return nullptr; } return FJsonObjectConverter::UPropertyToJsonValue(Property, Value); }
        virtual bool JsonDataToProperty(const TSharedPtr<FJsonValue>& JsonValue, TNotNull<FProperty*> Property, void* OutValue, UObject* Outer) override
        {
            if (!MatchCategorySortingProperty(Property) || !OutValue || !JsonValue.IsValid()) { return false; }
            if (CastField<FArrayProperty>(Property))
            {
                // Null/omitted function arguments retain the stock empty-array frame default.
                // Direct Array makes outer dispatch pass the full JSON array to this callback.
                if (JsonValue->Type != EJson::Array) { return false; }
                for (const auto& Row : JsonValue->AsArray()) { if (!Row.IsValid() || Row->Type != EJson::String) { return false; } }
            }
            else if (JsonValue->Type != EJson::String) { return false; }
            return FJsonObjectConverter::JsonValueToUProperty(JsonValue, Property, OutValue, 0, 0, false, nullptr, nullptr);
        }
    };
    bool CategorySortingSchemaMatches(const TSharedPtr<FJsonObject>& Schema, bool Order)
    {
        if (!Schema.IsValid()) { return false; }
        if (!Order) { FString Type; return Schema->Values.Num() == 1 && Schema->TryGetStringField(TEXT("type"), Type) && Type == TEXT("string"); }
        const TArray<TSharedPtr<FJsonValue>>* Types = nullptr; const TSharedPtr<FJsonObject>* Items = nullptr; FString ItemType;
        return Schema->Values.Num() == 2 && Schema->TryGetArrayField(TEXT("type"), Types) && Types && Types->Num() == 2
            && (*Types)[0].IsValid() && (*Types)[1].IsValid() && (*Types)[0]->Type == EJson::String && (*Types)[1]->Type == EJson::String
            && (*Types)[0]->AsString() == TEXT("array") && (*Types)[1]->AsString() == TEXT("null")
            && Schema->TryGetObjectField(TEXT("items"), Items) && Items && Items->IsValid() && (*Items)->Values.Num() == 1
            && (*Items)->TryGetStringField(TEXT("type"), ItemType) && ItemType == TEXT("string");
    }
}

bool Cubeless::Registration::VerifyBlueprintCategorySortingToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error)
{
    for (const TCHAR* Name : { TEXT("default_verified"), TEXT("exact_converter_active"), TEXT("nullable_schema_verified"), TEXT("front_default_verified") }) { Report->SetBoolField(Name, false); }
    Report->SetNumberField(TEXT("verified_property_count"), 0); Report->SetNumberField(TEXT("prevalidated_property_count"), 0);
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetBoolField(TEXT("actual_call_packing_verified"), false); Report->SetBoolField(TEXT("asset_mutation_attempted"), false);
    Report->SetBoolField(TEXT("mutation_attempted"), false); Report->SetBoolField(TEXT("changed"), false);
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetBoolField(TEXT("outcome_unknown"), false);
    auto Defaults = MakeShared<FJsonObject>(), RawBefore = MakeShared<FJsonObject>(), RawAfter = MakeShared<FJsonObject>();
    Report->SetObjectField(TEXT("defaults"), Defaults); Report->SetObjectField(TEXT("raw_defaults_before"), RawBefore); Report->SetObjectField(TEXT("raw_defaults"), RawAfter);
    TArray<TSharedPtr<FJsonValue>> BeforeRows, Rows, Changes; Report->SetArrayField(TEXT("metadata_before"), BeforeRows);
    Report->SetArrayField(TEXT("property_observations"), Rows); Report->SetArrayField(TEXT("metadata_changes"), Changes);
    Report->SetStringField(TEXT("metadata_mutation_scope"), TEXT("At most two ordered RAM-only CPP_Default attachments on the exact unregistered canonical function: category_order empty text to (), then category_to_front empty text to quoted-empty. Both targets/defaults/converters are proven before any write; no asset/source/disk/default-value writes."));
    Report->SetStringField(TEXT("internal_reflection_scope"), TEXT("Direct Array(FString) actual empty-text or () default is scratch-proven empty and canonical () is separately parsed before metadata normalization. Internal [] projects original public nullable array/defaultNone; original None/[] both omit category_order. No Optional<Array> packing claim."));
    if (!IsInGameThread() || !ToolsetClass || ToolsetClass->GetPathName() != CategorySortingClass)
    { Error = TEXT("Exact canonical CubelessBlueprintTools on game thread required; no metadata write."); return false; }
    UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("set_blueprint_category_sorting"));
    if (!Function) { Error = TEXT("Canonical category sorting method unavailable; no metadata write."); return false; }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless category sorting defaults verification"));
    if (!Subsystem.HasValue()) { Error = TEXT("ToolsetRegistry subsystem unavailable; no metadata write or retry."); return false; }
    TArray<FProperty*> Properties; TArray<FName> Keys; TArray<TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter>> Converters; bool AllBefore = true;
    for (const TCHAR* Name : { TEXT("category_order"), TEXT("category_to_front") })
    {
        FProperty* Property = FindFProperty<FProperty>(Function, Name); const FName Key(*FString::Printf(TEXT("CPP_Default_%s"), Name));
        const bool Present = Function->HasMetaData(Key); const FString Text = Function->GetMetaData(Key);
        auto Before = MakeShared<FJsonObject>(); Before->SetStringField(TEXT("function"), Function->GetName()); Before->SetStringField(TEXT("property"), Name);
        Before->SetBoolField(TEXT("metadata_present"), Present); Before->SetStringField(TEXT("metadata"), Text);
        BeforeRows.Add(MakeShared<FJsonValueObject>(Before)); Report->SetArrayField(TEXT("metadata_before"), BeforeRows); RawBefore->SetStringField(Name, Text); RawAfter->SetStringField(Name, Text);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("function"), Function->GetName()); Row->SetStringField(TEXT("property"), Name);
        Row->SetBoolField(TEXT("property_present"), Property != nullptr); Row->SetBoolField(TEXT("metadata_present_before"), Present); Row->SetStringField(TEXT("metadata_before"), Text);
        Rows.Add(MakeShared<FJsonValueObject>(Row)); Report->SetArrayField(TEXT("property_observations"), Rows);
        const bool Kind = MatchCategorySortingProperty(Property) != nullptr; const bool Order = FString(Name) == TEXT("category_order");
        Row->SetBoolField(TEXT("exact_internal_kind"), Kind); if (Property) { Row->SetStringField(TEXT("internal_cpp_type"), Property->GetCPPType()); }
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Converter;
        if (Kind) { Converter = Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property); }
        const bool Active = Converter.IsValid() && Converter->GetName() == CategorySortingConverterName; Row->SetBoolField(TEXT("exact_converter_active_before"), Active);
        const auto Schema = Active ? Converter->PropertyToJsonSchema(Property) : TSharedPtr<FJsonObject>();
        const FString PreparedText = Order ? CategoryPreparedArray : CategoryPreparedEmpty;
        const auto ParsedBefore = Kind ? CategorySortingPublicDefault(Property, Text) : TSharedPtr<FJsonValue>();
        const auto ParsedPrepared = Kind ? CategorySortingPublicDefault(Property, PreparedText) : TSharedPtr<FJsonValue>();
        const auto Default = Active && ParsedBefore.IsValid() && ParsedPrepared.IsValid()
            ? Converter->PropertyToDefault(Property, Text) : TSharedPtr<FJsonValue>();
        const bool TypeOK = CategorySortingSchemaMatches(Schema, Order);
        const bool DefaultOK = ParsedBefore.IsValid() && ParsedPrepared.IsValid() && Default.IsValid()
            && (Order ? ParsedBefore->Type == EJson::Null && ParsedPrepared->Type == EJson::Null && Default->Type == EJson::Null
                : ParsedBefore->Type == EJson::String && ParsedBefore->AsString().IsEmpty()
                    && ParsedPrepared->Type == EJson::String && ParsedPrepared->AsString().IsEmpty()
                    && Default->Type == EJson::String && Default->AsString().IsEmpty());
        Row->SetStringField(TEXT("prepared_metadata"), PreparedText);
        Row->SetBoolField(TEXT("actual_empty_default_verified_before"), ParsedBefore.IsValid());
        Row->SetBoolField(TEXT("canonical_empty_default_verified_before"), ParsedPrepared.IsValid());
        if (Schema.IsValid()) { Row->SetObjectField(TEXT("type_schema_before"), Schema); } if (Default.IsValid()) { Row->SetField(TEXT("public_default_before"), Default); }
        Row->SetBoolField(TEXT("schema_verified_before"), TypeOK); Row->SetBoolField(TEXT("default_verified_before"), DefaultOK);
        Properties.Add(Property); Keys.Add(Key); Converters.Add(Converter); AllBefore &= Kind && Active && TypeOK && DefaultOK;
    }
    if (!AllBefore || Properties.Num() != 2) { Error = TEXT("Both exact kinds/defaults/converters/type schemas must pass before metadata write."); return false; }
    Report->SetNumberField(TEXT("prevalidated_property_count"), 2);
    int32 MetadataAttempts = 0, MetadataChanges = 0;
    for (int32 I = 0; I < Properties.Num(); ++I)
    {
        const FString Name = Properties[I]->GetName(); const FString PreparedText = I == 0 ? CategoryPreparedArray : CategoryPreparedEmpty;
        const FString Before = Function->GetMetaData(Keys[I]); const bool Attempted = Before != PreparedText;
        auto Change = MakeShared<FJsonObject>(); Change->SetStringField(TEXT("function"), Function->GetName()); Change->SetStringField(TEXT("property"), Name);
        Change->SetNumberField(TEXT("sequence"), I); Change->SetStringField(TEXT("metadata_before"), Before); Change->SetStringField(TEXT("metadata_target"), PreparedText);
        Change->SetBoolField(TEXT("attempted"), Attempted); Change->SetBoolField(TEXT("returned"), false);
        Change->SetBoolField(TEXT("changed"), false); Changes.Add(MakeShared<FJsonValueObject>(Change)); Report->SetArrayField(TEXT("metadata_changes"), Changes);
        if (Attempted)
        {
            Report->SetBoolField(TEXT("mutation_attempted"), true); Report->SetNumberField(TEXT("metadata_write_attempt_count"), ++MetadataAttempts);
            Function->SetMetaData(Keys[I], *PreparedText); Change->SetBoolField(TEXT("returned"), true);
        }
        const FString After = Function->GetMetaData(Keys[I]); const bool Changed = Before != After;
        Change->SetStringField(TEXT("metadata_after"), After); Change->SetBoolField(TEXT("changed"), Changed);
        if (Changed) { ++MetadataChanges; } Report->SetNumberField(TEXT("metadata_changed_count"), MetadataChanges); Report->SetBoolField(TEXT("changed"), MetadataChanges != 0);
        RawAfter->SetStringField(Name, After); Rows[I]->AsObject()->SetStringField(TEXT("metadata_after"), After);
        const auto Parsed = CategorySortingPublicDefault(Properties[I], After);
        const bool DefaultOK = Parsed.IsValid() && (I == 0 ? Parsed->Type == EJson::Null : Parsed->Type == EJson::String && Parsed->AsString().IsEmpty());
        Change->SetBoolField(TEXT("default_verified_after_return"), DefaultOK);
        if (Parsed.IsValid()) { Change->SetField(TEXT("public_default_after_return"), Parsed); }
        if (After != PreparedText || !DefaultOK)
        { Error = TEXT("Ordered metadata attachment postcondition failed; actual RAM metadata prefix retained, no retry."); return false; }
    }
    int32 Verified = 0;
    for (int32 I = 0; I < Properties.Num(); ++I)
    {
        FProperty* Property = Properties[I]; const bool Order = I == 0; const FString Text = Function->GetMetaData(Keys[I]);
        const auto Row = Rows[I]->AsObject(); const bool Kind = MatchCategorySortingProperty(Property) != nullptr;
        const auto Converter = Kind ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter>();
        const bool Active = Converter.IsValid() && Converter == Converters[I] && Converter->GetName() == CategorySortingConverterName;
        const auto Schema = Active ? Converter->PropertyToJsonSchema(Property) : TSharedPtr<FJsonObject>();
        const auto Default = Active ? Converter->PropertyToDefault(Property, Text) : TSharedPtr<FJsonValue>();
        const bool TypeOK = CategorySortingSchemaMatches(Schema, Order); const bool DefaultOK = Default.IsValid() && (Order ? Default->Type == EJson::Null : Default->Type == EJson::String && Default->AsString().IsEmpty());
        Row->SetStringField(TEXT("metadata_after"), Text); Row->SetBoolField(TEXT("exact_converter_active_after"), Active);
        Row->SetBoolField(TEXT("schema_verified_after"), TypeOK); Row->SetBoolField(TEXT("default_verified_after"), DefaultOK);
        if (Schema.IsValid()) { Row->SetObjectField(TEXT("type_schema"), Schema); }
        if (Default.IsValid()) { Row->SetField(TEXT("public_default"), Default); Defaults->SetField(FString::Printf(TEXT("set_blueprint_category_sorting.%s"), *Property->GetName()), Default); }
        if (!Kind || !Active || !TypeOK || !DefaultOK) { Error = TEXT("Post-attachment actual converter/default/schema proof failed; partial RAM prefix retained."); return false; }
        Report->SetNumberField(TEXT("verified_property_count"), ++Verified);
    }
    Report->SetBoolField(TEXT("default_verified"), true); Report->SetBoolField(TEXT("exact_converter_active"), true);
    Report->SetBoolField(TEXT("nullable_schema_verified"), true); Report->SetBoolField(TEXT("front_default_verified"), true);
    Report->SetStringField(TEXT("converter_name"), CategorySortingConverterName);
    Report->SetStringField(TEXT("verification_scope"), TEXT("Active converter type schemas and scratch-proven actual/canonical empty defaults after at most two ordered array/front RAM metadata attachments. Official schema and null/empty/multi-string call packing remain separate gates; no asset/load/compile/save work."));
    return true;
}
bool Cubeless::Registration::FJsonConverters::Register()
{
    if (MaterialArrays.IsValid() || NiagaraStageElements.IsValid() || AIDiscoveryDefaults.IsValid() || BlueprintDefaults.IsValid() || DataTableDefaults.IsValid() || SequencerFloatDefaults.IsValid() || SequencerMaintenanceDefaults.IsValid() || SequencerCameraDefaults.IsValid() || BlueprintMakeStructDefaults.IsValid() || BlueprintVariableMetadataNullable.IsValid() || BlueprintCategorySortingDefaults.IsValid() || BlueprintCustomEventMetadataDefaults.IsValid() || PawnPropertiesDefaults.IsValid() || SkeletalReadDefaults.IsValid() || SkeletalSocketDefaults.IsValid())
    { return MaterialArrays.IsValid() && NiagaraStageElements.IsValid() && AIDiscoveryDefaults.IsValid() && BlueprintDefaults.IsValid() && DataTableDefaults.IsValid() && SequencerFloatDefaults.IsValid() && SequencerMaintenanceDefaults.IsValid() && SequencerCameraDefaults.IsValid() && BlueprintMakeStructDefaults.IsValid() && BlueprintVariableMetadataNullable.IsValid() && BlueprintCategorySortingDefaults.IsValid() && BlueprintCustomEventMetadataDefaults.IsValid() && PawnPropertiesDefaults.IsValid() && SkeletalReadDefaults.IsValid() && SkeletalSocketDefaults.IsValid() && RegistrySubsystem.IsValid(); }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Material array converter registration"));
    if (!Subsystem.HasValue()) { return false; }
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Candidate = UE58::MakeMaterialArrayConverter();
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> StageCandidate = MakeNiagaraStageElementCountConverter();
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> AICandidate = UE58::MakeAIDiscoveryDefaultsConverter();
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> BPCandidate = UE58::MakeBlueprintDefaultsConverter();
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> DTCandidate = UE58::MakeDataTableDefaultsConverter();
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> FloatCandidate = UE58::MakeSequencerFloatDefaultsConverter();
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MaintenanceCandidate = UE58::MakeSequencerMaintenanceDefaultsConverter();
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> CameraCandidate = UE58::MakeSequencerCameraDefaultsConverter();
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> StructCandidate = UE58::MakeBlueprintMakeStructDefaultsConverter();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(Candidate)) { return false; }
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(StageCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(AICandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(BPCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(DTCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(FloatCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(MaintenanceCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(CameraCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MaintenanceCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(StructCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CameraCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MaintenanceCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MetadataCandidate = MakeShared<FBlueprintVariableMetadataNullableConverter>();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(MetadataCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StructCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CameraCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MaintenanceCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> CategoryCandidate = MakeShared<FBlueprintCategorySortingDefaultsConverter>();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(CategoryCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MetadataCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StructCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CameraCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MaintenanceCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> CustomEventCandidate = MakeShared<FBlueprintCustomEventMetadataDefaultsConverter>();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(CustomEventCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CategoryCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MetadataCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StructCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CameraCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MaintenanceCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> PawnCandidate = MakeShared<FPawnPropertiesDefaultsConverter>();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(PawnCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CustomEventCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CategoryCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MetadataCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StructCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CameraCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MaintenanceCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> SkeletalCandidate = MakeShared<FSkeletalReadDefaultsConverter>();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(SkeletalCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(PawnCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CustomEventCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CategoryCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MetadataCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StructCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CameraCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MaintenanceCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> SocketCandidate = MakeShared<FSkeletalSocketDefaultsConverter>();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(SocketCandidate))
    {
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(SkeletalCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(PawnCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CustomEventCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CategoryCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MetadataCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StructCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(CameraCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(MaintenanceCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(FloatCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(DTCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(BPCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(AICandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(StageCandidate);
        Subsystem.GetValue()->ToolsetRegistry.UnregisterConverter(Candidate);
        return false;
    }
    SkeletalSocketDefaults = SocketCandidate;
    SkeletalReadDefaults = SkeletalCandidate;
    PawnPropertiesDefaults = PawnCandidate;
    BlueprintCustomEventMetadataDefaults = CustomEventCandidate;
    BlueprintCategorySortingDefaults = CategoryCandidate;
    BlueprintVariableMetadataNullable = MetadataCandidate;
    RegistrySubsystem = Subsystem.GetValue().Get();
    MaterialArrays = Candidate;
    NiagaraStageElements = StageCandidate;
    AIDiscoveryDefaults = AICandidate;
    BlueprintDefaults = BPCandidate;
    DataTableDefaults = DTCandidate;
    SequencerFloatDefaults = FloatCandidate;
    SequencerMaintenanceDefaults = MaintenanceCandidate;
    SequencerCameraDefaults = CameraCandidate;
    BlueprintMakeStructDefaults = StructCandidate;
    return true;
}

void Cubeless::Registration::FJsonConverters::Unregister()
{
    Cubeless::Registration::ShutdownStateTreeStrictDryRunToolset();
    Cubeless::Registration::ShutdownContentAuditToolset();
    Cubeless::Registration::ShutdownEditorTraceToolset();
    Cubeless::Registration::ShutdownEnumToolset();
    Cubeless::Registration::ShutdownRigVMTopologyToolset();
    Cubeless::Registration::ShutdownRigVMLinkValidationToolset();
    Cubeless::Registration::ShutdownRigVMPinDefaultValidationToolset();
    Cubeless::Registration::ShutdownRigVMLinkWriterToolset();
    Cubeless::Registration::ShutdownRigVMPinDefaultWriterToolset();
    Cubeless::Registration::ShutdownAnimInstanceRuntimeToolset();
    Cubeless::Registration::ShutdownControlRigSolverIntentsToolset();
    Cubeless::Registration::ShutdownControlRigTransientProbeToolset();
    Cubeless::Registration::ShutdownAnimInstanceRuntimePropertyProbeToolset();
    Cubeless::Registration::ShutdownAnimStateRuntimeResponseToolset();
    Cubeless::Registration::ShutdownPostProcessAnimDemoVariantToolset();
    Cubeless::Registration::ShutdownSkeletalBoneSamplingToolset();
    Cubeless::Registration::ShutdownAnimStateMachineVariantToolset();
    Cubeless::Registration::ShutdownAnimBPLocomotionDriverToolset();
    Cubeless::Registration::ShutdownAnimNodePrePostRuntimePoseToolset();
    Cubeless::Registration::ShutdownBlueprintFunctionExtractionToolset();
    Cubeless::Registration::ShutdownSplineComponentPointsToolset();
    Cubeless::Registration::ShutdownFoliageInstanceToolset();
    Cubeless::Registration::ShutdownLandscapeToolset();
    Cubeless::Registration::ShutdownEditorLevelSessionToolset();
    Cubeless::Registration::ShutdownActorTransformToolset();
    Cubeless::Registration::ShutdownActorPropertyFocusToolset();
    Cubeless::Registration::ShutdownChaosSolverSettingsToolset();
    Cubeless::Registration::ShutdownChaosClothComponentToolset();
    Cubeless::Registration::ShutdownChaosClothAuthoringToolset();
    Cubeless::Registration::ShutdownLegacyChaosClothingToolset();
    Cubeless::Registration::ShutdownChaosCinematicSequenceToolset();
    Cubeless::Registration::ShutdownChaosCacheSessionToolset();
    Cubeless::Registration::ShutdownChaosDestructionPerformanceToolset();
    Cubeless::Registration::ShutdownChaosLearningToolset();
    Cubeless::Registration::ShutdownChaosTraceStrainToolset();
    Cubeless::Registration::ShutdownChaosGeometryAuthoringToolset();
    Cubeless::Registration::ShutdownGASAuthoringToolset();
    Cubeless::Registration::ShutdownContentReconstructionToolset();
    Cubeless::Registration::ShutdownNiagaraRendererToolset();
    Cubeless::Registration::ShutdownNiagaraEmitterConfigurationToolset();
    Cubeless::Registration::ShutdownNiagaraParameterCollectionToolset();
    Cubeless::Registration::ShutdownNiagaraAuthoringInspectionToolset();
    Cubeless::Registration::ShutdownPCGInspectionToolset();
    Cubeless::Registration::ShutdownNiagaraCleanupToolset();
    Cubeless::Registration::ShutdownNiagaraInputParameterToolset();
    Cubeless::Registration::ShutdownNiagaraModuleInputPathToolset();
    Cubeless::Registration::ShutdownNiagaraDataChannelAuthoringToolset();
    Cubeless::Registration::ShutdownNiagaraRuntimeInspectionToolset();
    Cubeless::Registration::ShutdownNiagaraFactoryBootstrapToolset();
    Cubeless::Registration::ShutdownNiagaraResourceBindingToolset();
    Cubeless::Registration::ShutdownGeometryCollectionInspectionToolset();
    Cubeless::Registration::ShutdownChaosClothInspectionToolset();
    Cubeless::Registration::ShutdownViewportCaptureToolset();
    Cubeless::Registration::ShutdownLinkedAnimGraphVariantToolset();
    Cubeless::Registration::ShutdownBlendSpacePoseGridToolset();
    Cubeless::Registration::ShutdownBlendSpaceVariantToolset();
    // Never discover/load another subsystem during teardown or unregister someone else's converter.
    if (UObjectInitialized())
    {
        if (UToolsetRegistrySubsystem* Subsystem = RegistrySubsystem.Get())
        {
            if (SkeletalSocketDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(SkeletalSocketDefaults); }
            if (SkeletalReadDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(SkeletalReadDefaults); }
            if (PawnPropertiesDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(PawnPropertiesDefaults); }
            if (BlueprintCustomEventMetadataDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(BlueprintCustomEventMetadataDefaults); }
            if (BlueprintCategorySortingDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(BlueprintCategorySortingDefaults); }
            if (BlueprintVariableMetadataNullable.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(BlueprintVariableMetadataNullable); }
            if (BlueprintMakeStructDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(BlueprintMakeStructDefaults); }
            if (SequencerCameraDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(SequencerCameraDefaults); }
            if (SequencerMaintenanceDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(SequencerMaintenanceDefaults); }
            if (SequencerFloatDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(SequencerFloatDefaults); }
            if (DataTableDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(DataTableDefaults); }
            if (BlueprintDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(BlueprintDefaults); }
            if (AIDiscoveryDefaults.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(AIDiscoveryDefaults); }
            if (NiagaraStageElements.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(NiagaraStageElements); }
            if (MaterialArrays.IsValid()) { Subsystem->ToolsetRegistry.UnregisterConverter(MaterialArrays); }
        }
    }
    SkeletalSocketDefaults.Reset();
    SkeletalReadDefaults.Reset();
    PawnPropertiesDefaults.Reset();
    BlueprintCustomEventMetadataDefaults.Reset();
    BlueprintCategorySortingDefaults.Reset();
    BlueprintVariableMetadataNullable.Reset();
    MaterialArrays.Reset();
    NiagaraStageElements.Reset();
    AIDiscoveryDefaults.Reset();
    BlueprintDefaults.Reset();
    DataTableDefaults.Reset();
    BlueprintMakeStructDefaults.Reset();
    SequencerCameraDefaults.Reset();
    SequencerMaintenanceDefaults.Reset();
    SequencerFloatDefaults.Reset();
    RegistrySubsystem.Reset();
}
