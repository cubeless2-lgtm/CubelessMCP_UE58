#include "Compatibility/UE58/CubelessAnimationStateEditingApi.h"
#include "Compatibility/UE58/CubelessAnimationProductionApi.h"
#include "Animation/AnimSequence.h"
#include "Compatibility/UE58/CubelessAnimationEditingApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "K2Node_VariableGet.h"
#include "EdGraphSchema_K2.h"
#include "UObject/StructOnScope.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimClassInterface.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateNode.h"
#include "AnimGraphNode_Slot.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimationTransitionSchema.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "HAL/FileManager.h"
#include "Misc/CoreMisc.h"
#include "Misc/PackageName.h"
#include "Internationalization/Text.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::AnimationStateEditing
{
namespace
{
using J=FJsonObject;using Values=TMap<FString,FString>;
bool Writing=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_WillBeLoaded|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Idle(){return IsInGameThread()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!IsEngineExitRequested()&&!GIsImportingT3D&&GEditor&&!GEditor->PlayWorld&&!GEditor->bIsSimulatingInEditor;}
FString Path(const UObject* O){return O?O->GetPathName():FString();}
FString Number(double V){return FString::Printf(TEXT("%.17g"),V);}
FString Json(const TSharedRef<J>& O){bool Loaded=false;if(O->TryGetBoolField(TEXT("input_acquisition_unobserved_load"),Loaded)&&Loaded){O->SetBoolField(TEXT("native_change_state_known"),O->GetBoolField(TEXT("change_state_known")));O->SetBoolField(TEXT("change_state_known"),false);bool Changed=false;O->TryGetBoolField(TEXT("changed"),Changed);if(!Changed)O->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());O->SetStringField(TEXT("retry_risk"),TEXT("inspect_actual_target_after_input_load"));}FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
FString NG(const UEdGraphNode* N){return TEXT("n/")+N->NodeGuid.ToString()+TEXT("/");}
FString GG(const UEdGraph* G){return TEXT("g/")+G->GraphGuid.ToString()+TEXT("/");}
FString PK(const UEdGraphPin* P){return NG(P->GetOwningNode())+TEXT("pin/")+P->PinName.ToString()+TEXT("/")+FString::FromInt(P->Direction)+TEXT("/");}
bool MCP(const UObject* O){const FString S=Path(O);return S.Contains(TEXT("CubelessMCP"),ESearchCase::IgnoreCase)||S.Contains(TEXT("ModelContextProtocol"),ESearchCase::IgnoreCase)||S.Contains(TEXT("ToolsetRegistry"),ESearchCase::IgnoreCase);}
struct Lease
{
 TStrongObjectPtr<UObject> Object,Outer;TStrongObjectPtr<UClass> Class;TWeakObjectPtr<UObject> Serial;FString Name;
 explicit Lease(UObject* O):Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Serial(O),Name(Path(O)){}
 bool Current()const{auto* O=Object.Get();return Serial.IsValid()&&Serial.Get()==O&&Live(O)&&Live(Class.Get())&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&Path(O)==Name&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists);}
};
bool Value(FProperty* P,const void* Data,const FString& Key,Values& Out,UAnimBlueprint* B,int Depth)
{
 if(Depth>12||Out.Num()>131072)return false;
 if(auto* A=CastField<FArrayProperty>(P)){FScriptArrayHelper H(A,Data);if(H.Num()>16384)return false;Out.Add(Key+TEXT(".count"),FString::FromInt(H.Num()));for(int I=0;I<H.Num();++I)if(!Value(A->Inner,H.GetRawPtr(I),Key+FString::Printf(TEXT("[%d]"),I),Out,B,Depth+1))return false;return true;}
 if(auto* S=CastField<FStructProperty>(P))
 {
  bool Authored=false;for(TFieldIterator<FProperty> F(S->Struct);F;++F)if(F->HasAnyPropertyFlags(CPF_Edit)&&!F->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated)){Authored=true;for(int I=0;I<F->ArrayDim;++I)if(!Value(*F,F->ContainerPtrToValuePtr<void>(Data,I),Key+TEXT(".")+F->GetName()+FString::FromInt(I),Out,B,Depth+1))return false;}
  if(Authored)return true;
 }
 FString Text;P->ExportTextItem_Direct(Text,Data,nullptr,nullptr,PPF_None);
 if(auto* N=CastField<FNumericProperty>(P);N&&N->IsFloatingPoint()){double V=N->GetFloatingPointPropertyValue(Data);if(!FMath::IsFinite(V))return false;Text+=TEXT("|exact=")+Number(V);}
 if(auto* O=CastField<FObjectPropertyBase>(P)){UObject* Ref=O->GetObjectPropertyValue(Data);if(Ref){if(!Live(Ref)||MCP(Ref)||MCP(Ref->GetClass()))return false;const auto* C=Cast<UClass>(Ref);if(!C||C->ClassGeneratedBy!=B)Text+=TEXT("|object=")+FString::FromInt(Ref->GetUniqueID());}}
 Out.Add(Key,MoveTemp(Text));return true;
}
bool Props(UObject* O,const FString& Prefix,Values& Out,UAnimBlueprint* B)
{
 for(TFieldIterator<FProperty> P(O->GetClass());P;++P)
 {if(!P->HasAnyPropertyFlags(CPF_Edit)||P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_DuplicateTransient)||P->GetFName()==TEXT("ErrorMsg")||P->GetFName()==TEXT("ErrorType")||P->GetFName()==TEXT("bHasCompilerMessage"))continue;for(int I=0;I<P->ArrayDim;++I)if(!Value(*P,P->ContainerPtrToValuePtr<void>(O,I),Prefix+P->GetName()+FString::FromInt(I),Out,B,0))return false;}
 return true;
}

UObject* BindingObject(UObject* Node){auto* P=FindFProperty<FObjectPropertyBase>(Node->GetClass(),TEXT("Binding"));return P?P->GetObjectPropertyValue_InContainer(Node):nullptr;}
bool BindingState(UObject* Node,const FString& Prefix,Values& Out)
{
 auto* O=BindingObject(Node);if(!O)return true;if(!Live(O)||O->GetOuter()!=Node||MCP(O)||MCP(O->GetClass()))return false;Out.Add(Prefix+TEXT("serial"),FString::FromInt(O->GetUniqueID()));auto* M=FindFProperty<FMapProperty>(O->GetClass(),TEXT("PropertyBindings"));if(!M)return false;auto* S=CastField<FStructProperty>(M->ValueProp);if(!S)return false;FScriptMapHelper H(M,M->ContainerPtrToValuePtr<void>(O));if(H.Num()>4096)return false;Out.Add(Prefix+TEXT("count"),FString::FromInt(H.Num()));
 for(int I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)){FString K;M->KeyProp->ExportTextItem_Direct(K,H.GetKeyPtr(I),nullptr,O,PPF_None);for(TFieldIterator<FProperty> F(S->Struct);F;++F){if(F->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated)||F->GetFName()==TEXT("CompiledContext")||F->GetFName()==TEXT("CompiledContextDesc")||F->GetFName()==TEXT("PathAsText"))continue;FString V;F->ExportTextItem_Direct(V,F->ContainerPtrToValuePtr<void>(H.GetValuePtr(I)),nullptr,O,PPF_None);Out.Add(Prefix+K+TEXT(".")+F->GetName(),V);}}
 return true;
}
bool NewNodeFields(UEdGraphNode* N,FName WritableOutput,Values& Out,UAnimBlueprint* B)
{
 Out.Reset();if(!Live(N)||!Props(N,TEXT("props/"),Out,B)||!BindingState(N,TEXT("binding/"),Out))return false;Out.Add(TEXT("xy"),FString::Printf(TEXT("%d,%d"),N->NodePosX,N->NodePosY));
 for(auto* P:N->Pins){if(!P||P->WasTrashed())return false;FString Key=P->PinName.ToString()+TEXT("/")+FString::FromInt(P->Direction);FString Type;FEdGraphPinType::StaticStruct()->ExportText(Type,&P->PinType,nullptr,nullptr,PPF_None,nullptr);FString Text;FTextStringHelper::WriteToBuffer(Text,P->DefaultTextValue);Out.Add(Key,Type+TEXT("|")+P->DefaultValue+TEXT("|")+Path(P->DefaultObject)+TEXT("|")+Text);if(P->Direction!=EGPD_Output||P->PinName!=WritableOutput){TArray<FString> Links;for(auto* L:P->LinkedTo){if(!L||L->WasTrashed()||!Live(L->GetOwningNode()))return false;Links.Add(PK(L));}Links.Sort();Out.Add(Key+TEXT("/links"),FString::Join(Links,TEXT("|")));}}
 return true;
}

bool FreshBinding(UObject* Node)
{
 auto* O=BindingObject(Node);if(!Live(O)||O->GetOuter()!=Node||Path(O->GetClass())!=TEXT("/Script/AnimGraph.AnimGraphNodeBinding_Base"))return false;auto* M=FindFProperty<FMapProperty>(O->GetClass(),TEXT("PropertyBindings"));return M&&FScriptMapHelper(M,M->ContainerPtrToValuePtr<void>(O)).Num()==0;
}

struct Snapshot
{
 Values Data;TSet<UEdGraph*> Graphs;TSet<UEdGraphNode*> Nodes;
};
bool Capture(UAnimBlueprint* B,Snapshot& S)
{
 S=Snapshot();if(!Live(B)||!Live(B->TargetSkeleton))return false;
 if(!Props(B,TEXT("blueprint/"),S.Data,B)||!Props(B->TargetSkeleton,TEXT("skeleton/"),S.Data,B))return false;
 for(const TCHAR* Field:{TEXT("SlotGroups"),TEXT("BoneTree"),TEXT("VirtualBones")})if(auto* P=FindFProperty<FProperty>(B->TargetSkeleton->GetClass(),Field))if(!Value(P,P->ContainerPtrToValuePtr<void>(B->TargetSkeleton),FString(TEXT("skeleton/authored/"))+Field,S.Data,B,0))return false;
 for(const TCHAR* Name:{TEXT("NewVariables"),TEXT("ImplementedInterfaces")})if(auto* P=FindFProperty<FProperty>(B->GetClass(),Name))if(!Value(P,P->ContainerPtrToValuePtr<void>(B),FString(TEXT("blueprint/"))+Name,S.Data,B,0))return false;
 S.Data.Add(TEXT("skeleton/path"),Path(B->TargetSkeleton));S.Data.Add(TEXT("parent"),Path(B->ParentClass));
 const auto& Ref=B->TargetSkeleton->GetReferenceSkeleton();if(Ref.GetRawBoneNum()>4096)return false;S.Data.Add(TEXT("skeleton/bones"),FString::FromInt(Ref.GetRawBoneNum()));
 for(int I=0;I<Ref.GetRawBoneNum();++I){const auto& T=Ref.GetRawRefBonePose()[I];const auto P=T.GetTranslation(),Scale=T.GetScale3D();const auto Q=T.GetRotation();FString V=Ref.GetBoneName(I).ToString()+TEXT("|")+FString::FromInt(Ref.GetParentIndex(I));for(double F:{P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,Scale.X,Scale.Y,Scale.Z})V+=TEXT("|")+Number(F);S.Data.Add(TEXT("skeleton/bone/")+FString::FromInt(I),V);}
 TArray<UEdGraph*> All;B->GetAllGraphs(All);if(All.Num()>256)return false;TSet<FGuid> GraphIds,NodeIds;int Pins=0;
 for(auto* G:All)
 {
  if(!Live(G)||S.Graphs.Contains(G)||!G->GraphGuid.IsValid()||GraphIds.Contains(G->GraphGuid)||!Live(G->GetSchema())||MCP(G->GetClass()))return false;S.Graphs.Add(G);GraphIds.Add(G->GraphGuid);
  S.Data.Add(GG(G)+TEXT("identity"),Path(G)+TEXT("|")+Path(G->GetClass())+TEXT("|")+Path(G->GetOuter())+TEXT("|")+Path(G->GetSchema()->GetClass()));
  FString Children;for(UEdGraph* Child:G->SubGraphs){if(!Live(Child))return false;Children+=Child->GraphGuid.ToString()+TEXT("|");}S.Data.Add(GG(G)+TEXT("children"),Children);
  if(G->Nodes.Num()>4096)return false;
  for(UEdGraphNode* N:G->Nodes)
  {
   if(!Live(N)||S.Nodes.Contains(N)||!N->NodeGuid.IsValid()||NodeIds.Contains(N->NodeGuid)||N->GetGraph()!=G||MCP(N->GetClass())||S.Nodes.Num()>=4096)return false;NodeIds.Add(N->NodeGuid);S.Nodes.Add(N);
   S.Data.Add(NG(N)+TEXT("identity"),Path(N)+TEXT("|")+Path(N->GetClass())+TEXT("|")+G->GraphGuid.ToString());S.Data.Add(NG(N)+TEXT("xy"),FString::Printf(TEXT("%d,%d"),N->NodePosX,N->NodePosY));
   if(!Props(N,NG(N)+TEXT("prop/"),S.Data,B)||!BindingState(N,NG(N)+TEXT("binding/"),S.Data))return false;
   if(auto* State=Cast<UAnimStateNodeBase>(N))S.Data.Add(NG(N)+TEXT("bound"),Path(State->GetBoundGraph()));
   for(const TCHAR* Field:{TEXT("StateType"),TEXT("SaveCachedPoseNode"),TEXT("NameOfCache"),TEXT("InterfaceGuid"),TEXT("FunctionReference"),TEXT("AlwaysDynamicProperties"),TEXT("VariableReference")})if(auto* P=FindFProperty<FProperty>(N->GetClass(),Field)){FString V;P->ExportTextItem_Direct(V,P->ContainerPtrToValuePtr<void>(N),nullptr,N,PPF_None);S.Data.Add(NG(N)+TEXT("authored/")+Field,V);}
   TSet<FString> PinKeys;
   for(auto* P:N->Pins)
   {
    if(!P||P->WasTrashed()||P->GetOwningNode()!=N||++Pins>16384)return false;const FString Key=PK(P);if(PinKeys.Contains(Key))return false;PinKeys.Add(Key);
    FString Type;FEdGraphPinType::StaticStruct()->ExportText(Type,&P->PinType,nullptr,nullptr,PPF_None,nullptr);S.Data.Add(Key+TEXT("type"),Type);
    FString Text;FTextStringHelper::WriteToBuffer(Text,P->DefaultTextValue);S.Data.Add(Key+TEXT("default"),P->DefaultValue+TEXT("|")+Path(P->DefaultObject)+TEXT("|")+Text);
    TArray<FString> Links;for(auto* L:P->LinkedTo){if(!L||L->WasTrashed()||!Live(L->GetOwningNode())||L->GetOwningNode()->GetGraph()!=G||!L->LinkedTo.Contains(P))return false;Links.Add(PK(L));}Links.Sort();S.Data.Add(Key+TEXT("links"),FString::Join(Links,TEXT("|")));
   }
  }
 }
 int64 Bytes=0;for(const auto& E:S.Data)Bytes+=(E.Key.Len()+E.Value.Len())*2;return Bytes<=8*1024*1024;
}
struct Plan
{
 TSet<FString> Keys,Prefixes;TSet<UObject*> RetiredObjects;TSet<UEdGraphNode*> RetireNodes,AddNodes;TSet<UEdGraph*> RetireGraphs,AddGraphs;
 bool Allowed(const FString& Key)const{if(Keys.Contains(Key))return true;for(const auto& Prefix:Prefixes)if(Key.StartsWith(Prefix))return true;return false;}
};
bool Same(const Snapshot& A,const Snapshot& B,const Plan& P,TSharedRef<J> R)
{
 for(auto* G:A.Graphs)if(!P.RetireGraphs.Contains(G)&&!B.Graphs.Contains(G))return false;for(auto* G:B.Graphs)if(!A.Graphs.Contains(G)&&!P.AddGraphs.Contains(G))return false;
 for(auto* N:A.Nodes)if(!P.RetireNodes.Contains(N)&&!B.Nodes.Contains(N))return false;for(auto* N:B.Nodes)if(!A.Nodes.Contains(N)&&!P.AddNodes.Contains(N))return false;
 for(auto* G:P.RetireGraphs)if(B.Graphs.Contains(G))return false;for(auto* G:P.AddGraphs)if(!B.Graphs.Contains(G))return false;for(auto* N:P.RetireNodes)if(B.Nodes.Contains(N))return false;for(auto* N:P.AddNodes)if(!B.Nodes.Contains(N))return false;
 TArray<FString> Keys;A.Data.GetKeys(Keys);for(const auto& E:B.Data)Keys.AddUnique(E.Key);Keys.Sort();
 for(const auto& K:Keys){if(P.Allowed(K))continue;auto* E=A.Data.Find(K);auto* V=B.Data.Find(K);if(!E||!V||*E!=*V){R->SetStringField(TEXT("first_mismatch_property"),K);R->SetStringField(TEXT("expected"),E?E->Left(2048):TEXT("<absent>"));R->SetStringField(TEXT("actual"),V?V->Left(2048):TEXT("<absent>"));return false;}}
 return true;
}
struct Work
{
 TSharedRef<J> R;UAnimBlueprint* B=nullptr;UAnimationStateMachineGraph* G=nullptr;UEditorEngine* Editor=nullptr;TArray<Lease> Holds;TMap<UObject*,Values> SourceValues;TArray<UEdGraph*> OldChildren;Snapshot Before,Expected;Plan Changes;FString ScriptError;bool Started=false,Settled=false;
 explicit Work(const FString& Action):R(MakeShared<J>())
 {R->SetStringField(TEXT("operation"),Action);R->SetStringField(TEXT("stage"),TEXT("preflight"));for(const TCHAR* K:{TEXT("success"),TEXT("mutation_attempted"),TEXT("changed"),TEXT("compile_attempted"),TEXT("save_attempted"),TEXT("saved"),TEXT("atomic_rollback"),TEXT("automatic_retry")})R->SetBoolField(K,false);R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("save_state_known"),true);R->SetStringField(TEXT("compile_scope"),TEXT("Explicit full compile is separate from engine structural/skeleton regeneration inside schema actions and graph deletion."));}
 bool Init(UObject* O,const FString& Name)
 {
  B=Cast<UAnimBlueprint>(O);if(!Idle()||!Live(B)||B->GetClass()!=UAnimBlueprint::StaticClass()||B->IsTemplate()||B->BlueprintType!=BPTYPE_Normal||B->GetOuter()!=B->GetOutermost()||!B->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))||B->GetOutermost()->HasAnyPackageFlags(PKG_ContainsMap)||!Live(B->TargetSkeleton)||MCP(B->ParentClass)||MCP(B->TargetSkeleton)||Name.IsEmpty()||Name.Len()>256)return false;
  if(!Live(B->SkeletonGeneratedClass)||!IAnimClassInterface::GetFromClass(B->SkeletonGeneratedClass))return false;
  Editor=GEditor;TArray<UEdGraph*> All;B->GetAllGraphs(All);for(auto* Item:All)if(Live(Item)&&Item->GetName()==Name){if(G)return false;G=Cast<UAnimationStateMachineGraph>(Item);if(!G)return false;}
  if(!G||!Live(G->GetSchema())||G->GetSchema()->GetClass()!=UAnimationStateMachineSchema::StaticClass()||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=B||!Live(G->EntryNode)||!G->Nodes.Contains(G->EntryNode))return false;
  if(!Capture(B,Before))return false;Expected=Before;for(UEdGraph* Child:G->SubGraphs)OldChildren.Add(Child);Holds.Emplace(B);Holds.Emplace(B->TargetSkeleton);for(auto* Graph:Before.Graphs)Holds.Emplace(Graph);for(auto* Node:Before.Nodes)Holds.Emplace(Node);
  R->SetStringField(TEXT("asset"),Path(B));R->SetStringField(TEXT("graph"),Path(G));R->SetBoolField(TEXT("dirty_before"),B->GetOutermost()->IsDirty());return true;
 }
 bool Anchors(bool Permitted)
 {
  if(!Idle()||GEditor!=Editor||!ScriptError.IsEmpty()){R->SetBoolField(TEXT("change_state_known"),false);return false;}
  for(const auto& H:Holds){if((Permitted||Settled)&&Changes.RetiredObjects.Contains(H.Object.Get()))continue;if(!H.Current()){R->SetStringField(TEXT("first_mismatch_property"),TEXT("object.identity_owner_lifetime:" )+H.Name);R->SetBoolField(TEXT("change_state_known"),false);return false;}}return true;
 }
 bool Check(const Snapshot& Wanted,bool Permitted)
 {
  if(!Anchors(Permitted))return false;
  for(const auto& Source:SourceValues){Values Current;FString Error;if(!AnimationEditing::CaptureReadOnlySource(Source.Key,Current,Error)){R->SetBoolField(TEXT("change_state_known"),false);R->SetStringField(TEXT("first_mismatch_property"),TEXT("input_authored_snapshot:" )+Path(Source.Key));R->SetStringField(TEXT("input_observation_error"),Error);return false;}if(!Current.OrderIndependentCompareEqual(Source.Value)){R->SetBoolField(TEXT("change_state_known"),false);TArray<FString> Keys;Source.Value.GetKeys(Keys);for(const auto& Item:Current)Keys.AddUnique(Item.Key);Keys.Sort();for(const auto& K:Keys){const auto* E=Source.Value.Find(K);const auto* A=Current.Find(K);if(!E||!A||*E!=*A){R->SetStringField(TEXT("first_mismatch_property"),Path(Source.Key)+TEXT(".")+K);R->SetStringField(TEXT("expected"),E?E->Left(2048):TEXT("<absent>"));R->SetStringField(TEXT("actual"),A?A->Left(2048):TEXT("<absent>"));break;}}return false;}}
  Snapshot Now;if(!Capture(B,Now)||!Same(Wanted,Now,Permitted?Changes:Plan(),R)){R->SetBoolField(TEXT("change_state_known"),false);return false;}return true;
 }
 bool Establish()
 {TSet<UEdGraph*> Children;for(auto* ChildGraph:OldChildren)if(!Changes.RetireGraphs.Contains(ChildGraph))Children.Add(ChildGraph);for(auto* ChildGraph:Changes.AddGraphs)Children.Add(ChildGraph);if(Children.Num()!=G->SubGraphs.Num())return false;for(UEdGraph* Child:G->SubGraphs)if(!Children.Contains(Child))return false;if(!Check(Before,true)||!Capture(B,Expected))return false;for(auto* N:Changes.AddNodes)Holds.Emplace(N);for(auto* ChildGraph:Changes.AddGraphs)Holds.Emplace(ChildGraph);Settled=true;R->SetBoolField(TEXT("changed"),!Same(Before,Expected,Plan(),MakeShared<J>()));R->SetBoolField(TEXT("change_state_known"),true);return true;}
 FString Fail(const FString& Message)
 {R->SetBoolField(TEXT("success"),false);R->SetStringField(TEXT("error"),Message);if(!ScriptError.IsEmpty())R->SetStringField(TEXT("script_exception"),ScriptError);if(!R->GetBoolField(TEXT("change_state_known")))R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());if(Live(B))R->SetBoolField(TEXT("dirty_after"),B->GetOutermost()->IsDirty());R->SetStringField(TEXT("retry_risk"),!R->GetBoolField(TEXT("change_state_known"))?TEXT("inspect_actual_target"):Started?TEXT("mutation_attempted"):TEXT("none_observed"));return Json(R);}
};
UEdGraphPin* Pin(UEdGraphNode* N,EEdGraphPinDirection Direction)
{if(!Live(N))return nullptr;UEdGraphPin* Found=nullptr;for(auto* P:N->Pins)if(P&&!P->WasTrashed()&&P->Direction==Direction){if(Found)return nullptr;Found=P;}return Found;}
UAnimStateNodeBase* Endpoint(UAnimStateTransitionNode* T,EEdGraphPinDirection Direction)
{auto* P=Pin(T,Direction);return P&&P->LinkedTo.Num()==1&&P->LinkedTo[0]&&!P->LinkedTo[0]->WasTrashed()?Cast<UAnimStateNodeBase>(P->LinkedTo[0]->GetOwningNode()):nullptr;}
UEdGraphNode* Node(UAnimationStateMachineGraph* G,const TSharedPtr<J>& P,const TCHAR* Key)
{FString Id;FGuid Guid;if(!P->TryGetStringField(Key,Id)||!FGuid::Parse(Id,Guid)||!Guid.IsValid())return nullptr;for(UEdGraphNode* N:G->Nodes)if(Live(N)&&N->NodeGuid==Guid)return N;return nullptr;}
bool Bool(const TSharedPtr<J>& P,const TCHAR* Key,bool& V){return P->TryGetBoolField(Key,V);}
bool Int(const TSharedPtr<J>& P,const TCHAR* Key,int32& V,int32 Min,int32 Max)
{double D=0;if(!P->TryGetNumberField(Key,D)||!FMath::IsFinite(D)||D<Min||D>Max||FMath::TruncToDouble(D)!=D)return false;V=int32(D);return true;}
bool TextName(const FString& N){if(N.IsEmpty()||N.Len()>128||N.Equals(TEXT("None"),ESearchCase::IgnoreCase))return false;for(auto C:N)if(C<32||C=='/'||C=='\\'||C=='.'||C==':'||C=='\''||C=='"')return false;return true;}
bool SaveAdmitted(UAnimBlueprint* B,bool Save,bool AllowDirty)
{if(!Save)return true;FString File;return (!B->GetOutermost()->IsDirty()||AllowDirty)&&FPackageName::TryConvertLongPackageNameToFilename(B->GetOutermost()->GetName(),File,FPackageName::GetAssetPackageExtension())&&!IFileManager::Get().IsReadOnly(*File);}
int Owners(const Snapshot& S,UEdGraph* G)
{int Count=0;for(auto* N:S.Nodes)if(auto* State=Cast<UAnimStateNodeBase>(N);State&&State->GetBoundGraph()==G)++Count;return Count;}
bool Retire(Work& W,UAnimStateNodeBase* N)
{
 if(!Live(N)||!N->CanUserDeleteNode())return false;W.Changes.RetireNodes.Add(N);W.Changes.RetiredObjects.Add(N);W.Changes.Prefixes.Add(NG(N));
 for(auto* Root:{N->GetBoundGraph(),Cast<UAnimStateTransitionNode>(N)?CastChecked<UAnimStateTransitionNode>(N)->CustomTransitionGraph.Get():nullptr})if(Root)
 {
  if(!Live(Root)||Owners(W.Before,Root)>1)return false;
  for(auto* G:W.Before.Graphs)if(G==Root||G->IsIn(Root)){W.Changes.RetireGraphs.Add(G);W.Changes.RetiredObjects.Add(G);W.Changes.Prefixes.Add(GG(G));for(UEdGraphNode* Child:G->Nodes){W.Changes.RetireNodes.Add(Child);W.Changes.RetiredObjects.Add(Child);W.Changes.Prefixes.Add(NG(Child));}}
 }
 W.Changes.Keys.Add(GG(W.G)+TEXT("children"));return true;
}
void LinkKeys(Plan& P,UEdGraphPin* A,UEdGraphPin* B){if(A)P.Keys.Add(PK(A)+TEXT("links"));if(B)P.Keys.Add(PK(B)+TEXT("links"));}
bool NewGraph(Work& W,UEdGraphNode* N,UEdGraph* Bound,UClass* ResultClass)
{
 if(!Live(N)||!Live(Bound)||Bound->GetOuter()!=N||!W.G->Nodes.Contains(N)||!W.G->SubGraphs.Contains(Bound)||Bound->Nodes.Num()!=1||!Live(Bound->Nodes[0])||Bound->Nodes[0]->GetClass()!=ResultClass)return false;
 if(Bound->GetClass()!=(ResultClass==UAnimGraphNode_StateResult::StaticClass()?UAnimationStateGraph::StaticClass():UAnimationTransitionGraph::StaticClass()))return false;
 for(UEdGraphNode* Created:{N,Bound->Nodes[0].Get()})
 {auto* CDO=Created->GetClass()->GetDefaultObject(false);if(!Live(CDO))return false;Values Defaults,Actual;if(!Props(CDO,TEXT("default/"),Defaults,W.B)||!Props(Created,TEXT("default/"),Actual,W.B))return false;if(Cast<UAnimStateTransitionNode>(Created))Defaults.Add(TEXT("default/bAllowInertializationForSelfTransitions0"),TEXT("True"));
  // AllocateDefaultPins builds ShowPinForProperties from an empty CDO
  // array. Validate its authored visibility semantics, not CDO cache equality.
  if(auto* Anim=Cast<UAnimGraphNode_Base>(Created)){
   if(!FreshBinding(Created))return false;
   Defaults.Remove(TEXT("default/Binding0"));Actual.Remove(TEXT("default/Binding0"));
   const FName ExpectedPin=ResultClass==UAnimGraphNode_StateResult::StaticClass()?FName(TEXT("Result")):FName(TEXT("bCanEnterTransition"));
   if(Anim->ShowPinForProperties.Num()!=1||Anim->ShowPinForProperties[0].PropertyName!=ExpectedPin||!Anim->ShowPinForProperties[0].bShowPin||Anim->ShowPinForProperties[0].bCanToggleVisibility!=(ResultClass==UAnimGraphNode_TransitionResult::StaticClass()))return false;
   for(auto It=Defaults.CreateIterator();It;++It)if(It.Key().StartsWith(TEXT("default/ShowPinForProperties0")))It.RemoveCurrent();for(auto It=Actual.CreateIterator();It;++It)if(It.Key().StartsWith(TEXT("default/ShowPinForProperties0")))It.RemoveCurrent();
  }
  for(const auto& E:Defaults){auto* A=Actual.Find(E.Key);if(!A||(*A!=E.Value&&!(E.Key==TEXT("default/bAllowInertializationForSelfTransitions0")&&(*A==TEXT("True")||*A==TEXT("true"))))){W.R->SetStringField(TEXT("first_mismatch_property"),Path(Created)+TEXT(".")+E.Key);W.R->SetStringField(TEXT("expected"),E.Value.Left(2048));W.R->SetStringField(TEXT("actual"),A?A->Left(2048):TEXT("<absent>"));return false;}}if(Actual.Num()!=Defaults.Num())return false;}
 if(Cast<UAnimStateNode>(N)){if(N->Pins.Num()!=2)return false;for(auto* P:N->Pins)if(!P||P->WasTrashed()||!P->LinkedTo.IsEmpty())return false;}
 auto* Sink=Bound->Nodes[0].Get();if(Sink->Pins.Num()!=1||!Sink->Pins[0]||Sink->Pins[0]->Direction!=EGPD_Input||!Sink->Pins[0]->LinkedTo.IsEmpty())return false;
 W.Changes.AddNodes.Add(N);W.Changes.AddGraphs.Add(Bound);W.Changes.AddNodes.Add(Sink);W.Changes.Prefixes.Add(NG(N));W.Changes.Prefixes.Add(GG(Bound));W.Changes.Prefixes.Add(NG(Sink));W.Changes.Keys.Add(GG(W.G)+TEXT("children"));return true;
}
}

bool CaptureNodeBindings(UObject* Node,const FString& Prefix,TMap<FString,FString>& Values){return BindingState(Node,Prefix,Values);}

FString Inspect(UObject* Blueprint,const FString& GraphName)
{
 Work W(TEXT("inspect_animation_state_machine"));if(!W.Init(Blueprint,GraphName))return W.Fail(TEXT("Idle exact project AnimBlueprint, unique StateMachine graph and bounded valid graph topology required"));
 TArray<TSharedPtr<FJsonValue>> Rows;for(UEdGraphNode* N:W.G->Nodes)
 {auto Row=MakeShared<J>();Row->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());Row->SetStringField(TEXT("class"),Path(N->GetClass()));Row->SetStringField(TEXT("path"),Path(N));
  if(auto* S=Cast<UAnimStateNodeBase>(N)){Row->SetStringField(TEXT("bound_graph"),Path(S->GetBoundGraph()));if(auto* G=S->GetBoundGraph())Row->SetStringField(TEXT("bound_graph_name"),G->GetName());}
  if(auto* T=Cast<UAnimStateTransitionNode>(N)){auto* A=Endpoint(T,EGPD_Input);auto* B=Endpoint(T,EGPD_Output);Row->SetStringField(TEXT("from_id"),A?A->NodeGuid.ToString():TEXT(""));Row->SetStringField(TEXT("to_id"),B?B->NodeGuid.ToString():TEXT(""));Row->SetNumberField(TEXT("crossfade_duration"),T->CrossfadeDuration);Row->SetNumberField(TEXT("priority_order"),T->PriorityOrder);Row->SetBoolField(TEXT("shared_rules"),Owners(W.Before,T->BoundGraph)>1);}
  Rows.Add(MakeShared<FJsonValueObject>(Row));}
 W.R->SetArrayField(TEXT("nodes"),Rows);if(!W.Check(W.Before,false))return W.Fail(TEXT("Inspection context or graph changed"));W.R->SetBoolField(TEXT("success"),true);W.R->SetStringField(TEXT("stage"),TEXT("complete"));return Json(W.R);
}

FString Edit(UObject* Blueprint,const FString& Action,const FString& GraphName,const FString& Arguments,bool Execute,bool Compile,bool Save,bool AllowDirty,bool AllowDestructive)
{
 Work W(Action);if(Writing||!W.Init(Blueprint,GraphName)||!SaveAdmitted(W.B,Save,AllowDirty))return W.Fail(TEXT("Idle exact project AnimBlueprint, unique StateMachine graph and explicit dirty-package save approval required"));TGuardValue<bool> Lock(Writing,true);
 // Schema creation/removal can compile implicitly. Slot compilation registers
 // names on the shared Skeleton, so do not enter a writer with unapproved slots.
 TArray<UEdGraph*> CompileGraphs;W.B->GetAllGraphs(CompileGraphs);for(auto* Graph:CompileGraphs)for(UEdGraphNode* Other:Graph->Nodes)if(auto* Slot=Cast<UAnimGraphNode_Slot>(Other);Slot&&!W.B->TargetSkeleton->ContainsSlotName(Slot->Node.SlotName))return W.Fail(TEXT("Structural/compile work would register an unapproved Skeleton slot; register it explicitly first"));
 FScopedScriptExceptionHandler Errors([&](ELogVerbosity::Type V,const TCHAR* M,const TCHAR* S){if(V==ELogVerbosity::Error&&W.ScriptError.IsEmpty())W.ScriptError=FString(M).Left(2048);FScriptExceptionHandler::LoggingExceptionHandler(V,M,S);});
 TSharedPtr<J> P;if(Arguments.Len()>65536||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Arguments),P)||!P)return W.Fail(TEXT("Bounded JSON argument object required"));
 UEdGraphNode* Selected=nullptr;UAnimStateNode* State=nullptr,*FromState=nullptr,*ToState=nullptr;UAnimStateTransitionNode* Transition=nullptr;UEdGraphPin* FromPin=nullptr,*ToPin=nullptr;UAnimGraphNode_TransitionResult* Result=nullptr;UEdGraphPin* RulePin=nullptr;FString Name;int32 X=0,Y=0;bool ConstantRuleValue=false;TArray<UAnimStateTransitionNode*> Cascade;UAnimSequence* Sequence=nullptr;UEdGraphNode* PoseSink=nullptr;UEdGraphPin* PosePin=nullptr;FString VariableName;FGuid VariableGuid;TStrongObjectPtr<UAnimStateNode> StateTemplate;
 int32 Priority=0;float Crossfade=0;bool Automatic=false,Bidirectional=false;float Trigger=-1;
 if((Action==TEXT("create_state")||Action==TEXT("create_transition")||Action==TEXT("set_state_sequence"))&&W.B->GetDefaultBindingClass()&&Path(W.B->GetDefaultBindingClass())!=TEXT("/Script/AnimGraph.AnimGraphNodeBinding_Base"))return W.Fail(TEXT("Creation requires the standard AnimGraph binding class; custom binding constructors need a separate authored profile"));
 if(Action==TEXT("create_state"))
 {if(!P->TryGetStringField(TEXT("name"),Name)||!TextName(Name)||!Int(P,TEXT("x"),X,-1000000,1000000)||!Int(P,TEXT("y"),Y,-1000000,1000000))return W.Fail(TEXT("name and integral x/y within +/-1000000 required"));StateTemplate.Reset(NewObject<UAnimStateNode>(W.G,NAME_None,RF_Transient));if(!W.Check(W.Before,false)||!Live(StateTemplate.Get()))return W.Fail(TEXT("Scratch state template allocation changed the target"));auto Validator=static_cast<UEdGraphNode*>(StateTemplate.Get())->MakeNameValidator();if(!Validator.IsValid()||Validator->IsValid(Name)!=EValidatorResult::Ok)return W.Fail(TEXT("State name conflicts with an existing state, conduit or Animation Layer"));}
 else if(Action==TEXT("create_transition"))
 {FromState=Cast<UAnimStateNode>(Node(W.G,P,TEXT("from_id")));ToState=Cast<UAnimStateNode>(Node(W.G,P,TEXT("to_id")));if(!FromState||!ToState||FromState==ToState)return W.Fail(TEXT("Two distinct exact State GUIDs required; self transitions are outside this finite API"));for(UEdGraphNode* N:W.G->Nodes)if(auto* T=Cast<UAnimStateTransitionNode>(N);T&&Endpoint(T,EGPD_Input)==FromState&&Endpoint(T,EGPD_Output)==ToState)return W.Fail(TEXT("Transition already exists"));FromPin=Pin(FromState,EGPD_Output);ToPin=Pin(ToState,EGPD_Input);if(!FromPin||!ToPin||W.G->GetSchema()->CanCreateConnection(FromPin,ToPin).Response!=CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE)return W.Fail(TEXT("Machine schema did not admit a new transition"));LinkKeys(W.Changes,FromPin,ToPin);}
 else
 {
  Selected=Node(W.G,P,TEXT("node_id"));State=Cast<UAnimStateNode>(Selected);Transition=Cast<UAnimStateTransitionNode>(Selected);
  if(Action==TEXT("set_entry_state"))
  {if(!State)return W.Fail(TEXT("Exact State node_id required"));FromPin=Pin(W.G->EntryNode,EGPD_Output);ToPin=Pin(State,EGPD_Input);if(!FromPin||!ToPin)return W.Fail(TEXT("Entry/State pins invalid"));if(!FromPin->LinkedTo.IsEmpty()&&!FromPin->LinkedTo.Contains(ToPin)&&!AllowDestructive)return W.Fail(TEXT("Replacing Entry target requires allow_destructive=true"));LinkKeys(W.Changes,FromPin,ToPin);for(auto* L:FromPin->LinkedTo)LinkKeys(W.Changes,L,nullptr);}
  else if(Action==TEXT("set_transition_rule"))
  {auto* Graph=Transition?Cast<UAnimationTransitionGraph>(Transition->BoundGraph):nullptr;if(!Graph||Owners(W.Before,Graph)!=1||!Bool(P,TEXT("value"),ConstantRuleValue))return W.Fail(TEXT("Exact Transition with nonshared rule graph and bool value required"));Result=Graph->GetResultNode();if(!Live(Result)||!Graph->Nodes.Contains(Result))return W.Fail(TEXT("Rule Result invalid"));for(auto* Pin:Result->Pins)if(Pin&&Pin->Direction==EGPD_Input&&Pin->PinName==TEXT("bCanEnterTransition"))RulePin=Pin;if(!RulePin||RulePin->PinType.PinCategory!=TEXT("bool"))return W.Fail(TEXT("Bool rule Result pin unavailable"));if(!RulePin->LinkedTo.IsEmpty()&&!AllowDestructive)return W.Fail(TEXT("Replacing an existing rule input link requires allow_destructive=true"));W.Changes.Keys.Add(PK(RulePin)+TEXT("default"));W.Changes.Keys.Add(NG(Result)+TEXT("prop/Node0.bCanEnterTransition0"));LinkKeys(W.Changes,RulePin,nullptr);for(auto* L:RulePin->LinkedTo)LinkKeys(W.Changes,L,nullptr);}
  else if(Action==TEXT("set_state_sequence"))
  {
   if(!State||!Live(State->BoundGraph))return W.Fail(TEXT("Exact State with an existing pose graph required"));PoseSink=State->GetResultNodeInsideState();PosePin=Live(PoseSink)?PoseSink->FindPin(TEXT("Result"),EGPD_Input):nullptr;
   if(!PosePin||(!PosePin->LinkedTo.IsEmpty()&&!AllowDestructive))return W.Fail(TEXT("Existing State Result link replacement requires allow_destructive=true"));FString Source;if(!P->TryGetStringField(TEXT("animation_path"),Source)||Source.Len()>900||!Source.StartsWith(TEXT("/Game/"))||Source.Contains(TEXT(":"))||Source.Contains(TEXT("'"))||Source.Contains(TEXT("\"")))return W.Fail(TEXT("Bounded explicit project AnimSequence path required"));if(!Source.Contains(TEXT(".")))Source+=TEXT(".")+FPackageName::GetLongPackageAssetName(Source);if(Source.Len()>1023||!FPackageName::IsValidObjectPath(Source))return W.Fail(TEXT("Canonical bounded top-level animation path required"));
   W.R->SetBoolField(TEXT("input_load_attempted"),false);Sequence=FindObject<UAnimSequence>(nullptr,*Source);
   if(!Sequence){const auto Data=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetByObjectPath(FSoftObjectPath(Source));if(!Data.IsValid()||Data.AssetClassPath!=UAnimSequence::StaticClass()->GetClassPathName())return W.Fail(TEXT("Canonical registered ordinary AnimSequence required; resolve redirectors separately"));const FString Token=FGuid::NewGuid().ToString(EGuidFormats::Digits);TSharedPtr<J> Begin;const FString BeginText=AnimationEditing::BeginAcquisition(Source,Token);if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BeginText),Begin)||!Begin||!Begin->GetBoolField(TEXT("success")))return W.Fail(TEXT("Source acquisition admission failed"));W.R->SetBoolField(TEXT("input_load_attempted"),true);Sequence=LoadObject<UAnimSequence>(nullptr,*Source);const FString EndText=AnimationEditing::EndAcquisition(Token,true);W.R->SetStringField(TEXT("input_acquisition_observation"),EndText);TSharedPtr<J> End;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(EndText),End)||!End||!End->GetBoolField(TEXT("success"))){W.R->SetBoolField(TEXT("change_state_known"),false);return W.Fail(TEXT("Source acquisition observation/cleanup failed; no retry"));}}
   if(!W.Check(W.Before,false))return W.Fail(TEXT("Animation resolution changed protected state"));if(!Live(Sequence)||Sequence->GetClass()!=UAnimSequence::StaticClass()||Sequence->IsTemplate()||Sequence->GetOuter()!=Sequence->GetOutermost()||Sequence->GetSkeleton()!=W.B->TargetSkeleton)return W.Fail(TEXT("Ordinary AnimSequence with exactly matching Skeleton required"));W.Holds.Emplace(Sequence);Values Input;FString SourceError;if(!AnimationEditing::CaptureReadOnlySource(Sequence,Input,SourceError))return W.Fail(TEXT("Animation input authored snapshot failed"));W.SourceValues.Add(Sequence,Input);FString Error;if(!AnimationProduction::AuditRuntimeInput(Sequence,Error,W.R)||!W.Check(W.Before,false)||!SaveAdmitted(W.B,Save,AllowDirty))return W.Fail(Error.IsEmpty()?TEXT("Animation resolution/audit/save admission failed"):Error);
   if(State->GetResultNodeInsideState()!=PoseSink||!Live(PoseSink))return W.Fail(TEXT("State Result identity changed during source acquisition"));PosePin=PoseSink->FindPin(TEXT("Result"),EGPD_Input);if(!PosePin||PosePin->WasTrashed()||(!PosePin->LinkedTo.IsEmpty()&&!AllowDestructive))return W.Fail(TEXT("State Result/replacement admission changed during source acquisition"));LinkKeys(W.Changes,PosePin,nullptr);for(auto* L:PosePin->LinkedTo)LinkKeys(W.Changes,L,nullptr);
  }
  else if(Action==TEXT("set_transition_bool_variable"))
  {
   auto* Graph=Transition?Cast<UAnimationTransitionGraph>(Transition->BoundGraph):nullptr;if(!Graph||Owners(W.Before,Graph)!=1||!P->TryGetStringField(TEXT("variable_name"),VariableName)||!TextName(VariableName))return W.Fail(TEXT("Exact unshared transition and existing bool variable required"));auto* Property=FindFProperty<FBoolProperty>(W.B->SkeletonGeneratedClass,*VariableName);if(!Property||!Property->HasAnyPropertyFlags(CPF_BlueprintVisible)||Property->HasAnyPropertyFlags(CPF_EditorOnly))return W.Fail(TEXT("Existing runtime Blueprint-visible bool variable required"));VariableGuid=FBlueprintEditorUtils::FindMemberVariableGuidByName(W.B,FName(*VariableName));Result=Graph->GetResultNode();RulePin=Live(Result)?Result->FindPin(TEXT("bCanEnterTransition"),EGPD_Input):nullptr;if(!RulePin||(!RulePin->LinkedTo.IsEmpty()&&!AllowDestructive))return W.Fail(TEXT("Replacing an existing rule input requires allow_destructive=true"));LinkKeys(W.Changes,RulePin,nullptr);for(auto* L:RulePin->LinkedTo)LinkKeys(W.Changes,L,nullptr);
  }
  else if(Action==TEXT("set_transition_settings"))
  {if(!Transition||Transition->bSharedCrossfade)return W.Fail(TEXT("Exact unshared-crossfade Transition required"));Priority=Transition->PriorityOrder;Crossfade=Transition->CrossfadeDuration;Automatic=Transition->bAutomaticRuleBasedOnSequencePlayerInState;Bidirectional=Transition->Bidirectional;Trigger=Transition->AutomaticRuleTriggerTime;int Fields=0;for(const auto& Pair:P->Values){const auto& K=Pair.Key;if(K==TEXT("node_id"))continue;++Fields;double D=0;if(K==TEXT("priority_order")){if(!Int(P,*K,Priority,0,MAX_int32))return W.Fail(TEXT("priority_order must be nonnegative int32"));}else if(K==TEXT("crossfade_duration")||K==TEXT("automatic_rule_trigger_time")){if(!Pair.Value->TryGetNumber(D)||!FMath::IsFinite(D)||D>(K==TEXT("crossfade_duration")?60.:60.)||D<(K==TEXT("crossfade_duration")?0.:-1.))return W.Fail(TEXT("Finite crossfade0..60 or trigger-1..60 required"));if(K==TEXT("crossfade_duration"))Crossfade=float(D);else Trigger=float(D);}else if(K==TEXT("automatic_rule")){if(!Bool(P,*K,Automatic))return W.Fail(TEXT("automatic_rule must be bool"));}else if(K==TEXT("bidirectional")){if(!Bool(P,*K,Bidirectional))return W.Fail(TEXT("bidirectional must be bool"));}else return W.Fail(TEXT("Unsupported transition setting"));}if(!Fields)return W.Fail(TEXT("Supply at least one transition setting"));for(const TCHAR* K:{TEXT("PriorityOrder0"),TEXT("CrossfadeDuration0"),TEXT("bAutomaticRuleBasedOnSequencePlayerInState0"),TEXT("AutomaticRuleTriggerTime0"),TEXT("Bidirectional0")})W.Changes.Keys.Add(NG(Transition)+TEXT("prop/")+K);}
  else if(Action==TEXT("delete_state")||Action==TEXT("delete_transition"))
  {if(!AllowDestructive||(!State&&!Transition)||(Action==TEXT("delete_state"))!=(State!=nullptr))return W.Fail(TEXT("Exact requested State/Transition and allow_destructive=true required"));if(State){int Count=0;for(UEdGraphNode* N:W.G->Nodes)if(Cast<UAnimStateNode>(N))++Count;if(Count<=1)return W.Fail(TEXT("Last State deletion refused"));auto* Entry=Pin(W.G->EntryNode,EGPD_Output);if(!Entry)return W.Fail(TEXT("Entry pin invalid"));for(auto* L:Entry->LinkedTo)if(L&&L->GetOwningNode()==State)return W.Fail(TEXT("Set Entry to another State explicitly before deleting its current target"));for(UEdGraphNode* N:W.G->Nodes)if(auto* T=Cast<UAnimStateTransitionNode>(N);T&&(Endpoint(T,EGPD_Input)==State||Endpoint(T,EGPD_Output)==State))Cascade.Add(T);bool Approved=false;if(!Cascade.IsEmpty()&&(!Bool(P,TEXT("cascade"),Approved)||!Approved))return W.Fail(TEXT("Connected State deletion requires cascade=true"));}if(!Retire(W,CastChecked<UAnimStateNodeBase>(Selected)))return W.Fail(TEXT("Shared/invalid owned graph deletion is not admitted"));for(auto* T:Cascade)if(!Retire(W,T))return W.Fail(TEXT("Cascade contains shared/invalid transition graphs"));for(auto* N:W.Changes.RetireNodes)for(auto* Pin:N->Pins)for(auto* L:Pin->LinkedTo)if(!W.Changes.RetireNodes.Contains(L->GetOwningNode()))LinkKeys(W.Changes,L,nullptr);W.R->SetNumberField(TEXT("planned_cascade_transitions"),Cascade.Num());W.R->SetBoolField(TEXT("engine_structural_compile_possible"),true);}
  else return W.Fail(TEXT("Unknown fixed state action; rename_state is not implemented"));
 }
 W.R->SetBoolField(TEXT("explicit_full_compile_requested"),Compile);W.R->SetBoolField(TEXT("full_compile_attempted"),false);if(!W.Check(W.Before,false))return W.Fail(TEXT("Preflight context changed"));if(!Execute){W.R->SetBoolField(TEXT("success"),true);W.R->SetStringField(TEXT("stage"),TEXT("validated"));return Json(W.R);}
 W.Started=true;W.R->SetBoolField(TEXT("mutation_attempted"),true);W.R->SetBoolField(TEXT("change_state_known"),false);W.R->SetStringField(TEXT("stage"),TEXT("modify"));W.B->Modify(false);if(!W.Check(W.Before,false))return W.Fail(TEXT("Blueprint Modify callback changed protected state"));W.G->Modify(false);if(!W.Check(W.Before,false))return W.Fail(TEXT("Graph Modify callback changed protected state"));if(Selected){Selected->Modify(false);if(!W.Check(W.Before,false))return W.Fail(TEXT("Node Modify callback changed protected state"));}
 W.R->SetStringField(TEXT("stage"),TEXT("engine_write"));
 // Modify callbacks may reconstruct semantic-equivalent pins. Never retain a
 // physical UEdGraphPin pointer across that callback boundary.
 if(Action==TEXT("create_transition")){FromPin=Pin(FromState,EGPD_Output);ToPin=Pin(ToState,EGPD_Input);if(!FromPin||!ToPin||W.G->GetSchema()->CanCreateConnection(FromPin,ToPin).Response!=CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE)return W.Fail(TEXT("Transition pins changed before engine writer"));}
 else if(Action==TEXT("set_entry_state")){FromPin=Pin(W.G->EntryNode,EGPD_Output);ToPin=Pin(State,EGPD_Input);if(!FromPin||!ToPin||(!FromPin->LinkedTo.IsEmpty()&&!FromPin->LinkedTo.Contains(ToPin)&&!AllowDestructive))return W.Fail(TEXT("Entry pins/replacement admission changed"));}
 else if(Action==TEXT("set_state_sequence")){PoseSink=State?State->GetResultNodeInsideState():nullptr;PosePin=Live(PoseSink)?PoseSink->FindPin(TEXT("Result"),EGPD_Input):nullptr;if(!PosePin||PosePin->WasTrashed()||(!PosePin->LinkedTo.IsEmpty()&&!AllowDestructive))return W.Fail(TEXT("State Result changed before writer"));}
 else if(Action==TEXT("set_transition_rule")||Action==TEXT("set_transition_bool_variable")){auto* RuleGraph=Transition?Cast<UAnimationTransitionGraph>(Transition->BoundGraph):nullptr;auto* CurrentResult=RuleGraph?RuleGraph->GetResultNode():nullptr;if(CurrentResult!=Result||!Live(Result))return W.Fail(TEXT("Rule Result changed before writer"));RulePin=Result->FindPin(TEXT("bCanEnterTransition"),EGPD_Input);if(!RulePin||RulePin->WasTrashed()||RulePin->PinType.PinCategory!=TEXT("bool")||(!RulePin->LinkedTo.IsEmpty()&&!AllowDestructive))return W.Fail(TEXT("Rule pin/replacement admission changed"));}
 if(Action==TEXT("create_state"))
 {FEdGraphSchemaAction_NewStateNode Spawn;StateTemplate->ClearFlags(RF_Transient);Spawn.NodeTemplate=StateTemplate.Get();auto* Created=Cast<UAnimStateNode>(Spawn.PerformAction(W.G,nullptr,FVector2f(float(X),float(Y)),false));State=Created;if(!W.Anchors(false)||!Live(State)||!NewGraph(W,State,State->BoundGraph,UAnimGraphNode_StateResult::StaticClass())||!W.Check(W.Before,true))return W.Fail(TEXT("State creation incomplete; inspect actual graph"));const FIntPoint SpawnedPosition(State->NodePosX,State->NodePosY);Lease StateAnchor(State);TStrongObjectPtr<UEdGraph> BoundAnchor(State->BoundGraph);TWeakObjectPtr<UEdGraph> BoundSerial(State->BoundGraph);auto* Sink=State->BoundGraph->Nodes[0].Get();TStrongObjectPtr<UEdGraphNode> SinkAnchor(Sink);TWeakObjectPtr<UEdGraphNode> SinkSerial(Sink);const FGuid BoundId=State->BoundGraph->GraphGuid,SinkId=Sink->NodeGuid;const FName SinkName=Sink->GetFName();const FString Default=Sink->Pins[0]->DefaultValue;const FName DefaultPinName=Sink->Pins[0]->PinName;const FText DefaultText=Sink->Pins[0]->DefaultTextValue;const FEdGraphPinType Type=Sink->Pins[0]->PinType;UObject* DefaultObject=Sink->Pins[0]->DefaultObject;
 static_cast<UEdGraphNode*>(State)->OnRenameNode(Name);
 if(!W.Anchors(false)||!StateAnchor.Current()||!BoundSerial.IsValid()||!SinkSerial.IsValid()||!Live(BoundAnchor.Get())||!Live(Sink)||State->BoundGraph!=BoundAnchor.Get()||BoundAnchor->GraphGuid!=BoundId||Sink->NodeGuid!=SinkId||Sink->GetFName()!=SinkName||Sink->GetOuter()!=BoundAnchor.Get()||BoundAnchor->GetName()!=Name||State->NodePosX!=SpawnedPosition.X||State->NodePosY!=SpawnedPosition.Y||!NewGraph(W,State,BoundAnchor.Get(),UAnimGraphNode_StateResult::StaticClass())||Sink->Pins[0]->PinName!=DefaultPinName||!Sink->Pins[0]->DefaultTextValue.IdenticalTo(DefaultText)||Sink->Pins[0]->DefaultValue!=Default||Sink->Pins[0]->DefaultObject!=DefaultObject||Sink->Pins[0]->PinType!=Type)return W.Fail(TEXT("State rename callback changed new State/Result contract"));W.R->SetNumberField(TEXT("actual_x"),SpawnedPosition.X);W.R->SetNumberField(TEXT("actual_y"),SpawnedPosition.Y);W.R->SetStringField(TEXT("node_id"),State->NodeGuid.ToString());W.R->SetStringField(TEXT("bound_graph"),Path(State->BoundGraph));W.R->SetBoolField(TEXT("engine_structural_compile_possible"),true);}
 else if(Action==TEXT("create_transition"))
 {const TSet<UEdGraphNode*> Old=W.Before.Nodes;const bool Connected=W.G->GetSchema()->TryCreateConnection(FromPin,ToPin);if(!W.Anchors(false))return W.Fail(TEXT("Transition creation callback changed a protected object"));for(UEdGraphNode* N:W.G->Nodes)if(!Old.Contains(N)){if(Transition)return W.Fail(TEXT("More than one transition created; inspect graph"));Transition=Cast<UAnimStateTransitionNode>(N);}if(!Connected||!Live(Transition)||!NewGraph(W,Transition,Transition->BoundGraph,UAnimGraphNode_TransitionResult::StaticClass())||Endpoint(Transition,EGPD_Input)!=FromState||Endpoint(Transition,EGPD_Output)!=ToState)return W.Fail(TEXT("Transition creation incomplete or endpoints changed"));W.R->SetStringField(TEXT("node_id"),Transition->NodeGuid.ToString());W.R->SetStringField(TEXT("bound_graph"),Path(Transition->BoundGraph));W.R->SetBoolField(TEXT("engine_structural_compile_possible"),true);}
 else if(Action==TEXT("set_entry_state")){const bool Connected=W.G->GetSchema()->TryCreateConnection(FromPin,ToPin);if(!W.Anchors(false))return W.Fail(TEXT("Entry callback changed a protected object"));FromPin=Pin(W.G->EntryNode,EGPD_Output);ToPin=Pin(State,EGPD_Input);if(!Connected||!FromPin||!ToPin||FromPin->LinkedTo.Num()!=1||FromPin->LinkedTo[0]!=ToPin)return W.Fail(TEXT("Entry connection failed; inspect actual graph"));}
 else if(Action==TEXT("set_transition_rule"))
 {auto* Schema=RulePin->GetOwningNode()->GetSchema();Schema->BreakPinLinks(*RulePin,true);if(!W.Check(W.Before,true)||!Live(Result))return W.Fail(TEXT("Rule link replacement changed protected state"));RulePin=Result->FindPin(TEXT("bCanEnterTransition"),EGPD_Input);if(!RulePin||RulePin->WasTrashed())return W.Fail(TEXT("Rule pin lifetime changed during link replacement"));Result->Node.bCanEnterTransition=ConstantRuleValue;Schema->TrySetDefaultValue(*RulePin,ConstantRuleValue?TEXT("true"):TEXT("false"));if(!W.Anchors(false)||!Live(Result))return W.Fail(TEXT("Rule setter changed a protected object"));RulePin=Result->FindPin(TEXT("bCanEnterTransition"),EGPD_Input);if(!RulePin||RulePin->WasTrashed()||!RulePin->LinkedTo.IsEmpty()||Result->Node.bCanEnterTransition!=ConstantRuleValue||RulePin->DefaultValue!=(ConstantRuleValue?TEXT("true"):TEXT("false")))return W.Fail(TEXT("Rule setter did not preserve requested constant"));}
 else if(Action==TEXT("set_state_sequence"))
 {
  auto* Graph=State->BoundGraph.Get();FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*Graph);auto* Player=Creator.CreateNode(false);W.Holds.Emplace(Player);static_cast<UAnimGraphNode_AssetPlayerBase*>(Player)->SetAnimationAsset(Sequence);Player->NodePosX=-240;Player->NodePosY=0;Creator.Finalize();if(!W.Anchors(false)||!Live(Player)||Player->GetGraph()!=Graph||!FreshBinding(Player)||static_cast<UAnimGraphNode_Base*>(Player)->GetAnimationAsset()!=Sequence)return W.Fail(TEXT("Sequence player creation/reference failed"));W.Changes.AddNodes.Add(Player);W.Changes.Prefixes.Add(NG(Player));if(!W.Check(W.Before,true))return W.Fail(TEXT("Sequence player callback changed unrelated graph"));
  // Constructor defaults plus the explicit Sequence are the authored contract.
  Values Defaults,Actual;auto* CDO=Player->GetClass()->GetDefaultObject(false);if(!Live(CDO)||!Props(CDO,TEXT("player/"),Defaults,W.B)||!Props(Player,TEXT("player/"),Actual,W.B))return W.Fail(TEXT("Sequence player default snapshot failed"));for(auto* Map:{&Defaults,&Actual}){Map->Remove(TEXT("player/Binding0"));for(auto It=Map->CreateIterator();It;++It)if(It.Key().StartsWith(TEXT("player/ShowPinForProperties0")))It.RemoveCurrent();}auto* Runtime=FindFProperty<FStructProperty>(Player->GetClass(),TEXT("Node"));if(!Runtime)return W.Fail(TEXT("Sequence player Node unavailable"));FStructOnScope Wanted(Runtime->Struct);Runtime->CopyCompleteValue(Wanted.GetStructMemory(),Runtime->ContainerPtrToValuePtr<void>(CDO));auto* AssetField=FindFProperty<FObjectPropertyBase>(Runtime->Struct,TEXT("Sequence"));if(!AssetField)return W.Fail(TEXT("Sequence property unavailable"));AssetField->SetObjectPropertyValue_InContainer(Wanted.GetStructMemory(),Sequence);if(!Value(Runtime,Wanted.GetStructMemory(),TEXT("player/Node0"),Defaults,W.B,0)||!Defaults.OrderIndependentCompareEqual(Actual))return W.Fail(TEXT("Sequence player authored defaults differ from its requested profile"));
  Values FrozenPlayer;if(!NewNodeFields(Player,FName(TEXT("Pose")),FrozenPlayer,W.B))return W.Fail(TEXT("New player snapshot failed"));auto PlayerValid=[&]{Values Current;return W.Anchors(false)&&NewNodeFields(Player,FName(TEXT("Pose")),Current,W.B)&&Current.OrderIndependentCompareEqual(FrozenPlayer);};
  const UEdGraphSchema* Schema=Graph->GetSchema();PosePin=PoseSink->FindPin(TEXT("Result"),EGPD_Input);Schema->BreakPinLinks(*PosePin,true);if(!W.Check(W.Before,true)||!PlayerValid())return W.Fail(TEXT("Pose-link replacement changed authored graph/player"));PosePin=PoseSink->FindPin(TEXT("Result"),EGPD_Input);auto* Output=Pin(Player,EGPD_Output);if(!PosePin||!Output||!Schema->TryCreateConnection(Output,PosePin)||!W.Anchors(false))return W.Fail(TEXT("Sequence pose connection failed"));PosePin=PoseSink->FindPin(TEXT("Result"),EGPD_Input);Output=Pin(Player,EGPD_Output);if(!PlayerValid()||!PosePin||!Output||PosePin->LinkedTo.Num()!=1||PosePin->LinkedTo[0]!=Output)return W.Fail(TEXT("Sequence Result link/player settings were not preserved"));W.R->SetStringField(TEXT("created_node_id"),Player->NodeGuid.ToString());W.R->SetStringField(TEXT("animation"),Path(Sequence));
 }
 else if(Action==TEXT("set_transition_bool_variable"))
 {
  auto* Graph=Transition->BoundGraph.Get();FMemberReference PlannedVariable;PlannedVariable.SetSelfMember(FName(*VariableName),VariableGuid);if(!PlannedVariable.ResolveMember<FProperty>(W.B->SkeletonGeneratedClass)||!W.Check(W.Before,false))return W.Fail(TEXT("Bool variable member resolution/context failed before node creation"));FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);auto* Getter=Creator.CreateNode(false);W.Holds.Emplace(Getter);Getter->VariableReference=PlannedVariable;Getter->NodePosX=-240;Getter->NodePosY=0;Creator.Finalize();if(!W.Anchors(false))return W.Fail(TEXT("Variable getter callback changed identity"));W.Changes.AddNodes.Add(Getter);W.Changes.Prefixes.Add(NG(Getter));if(!W.Check(W.Before,true))return W.Fail(TEXT("Variable getter callback changed unrelated authored graph"));auto* Output=Getter->FindPin(FName(*VariableName),EGPD_Output);if(!Output||Output->PinType.PinCategory!=UEdGraphSchema_K2::PC_Boolean||Getter->VariableReference.GetMemberName()!=FName(*VariableName)||!Getter->VariableReference.IsSelfContext())return W.Fail(TEXT("Getter does not match requested bool variable"));
  for(auto* Pin:Getter->Pins)if(!Pin||Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)return W.Fail(TEXT("Pure bool getter required"));Values FrozenGetter;if(!NewNodeFields(Getter,FName(*VariableName),FrozenGetter,W.B))return W.Fail(TEXT("Getter snapshot failed"));auto GetterValid=[&]{Values Current;return W.Anchors(false)&&FMemberReference::StaticStruct()->CompareScriptStruct(&PlannedVariable,&Getter->VariableReference,PPF_None)&&NewNodeFields(Getter,FName(*VariableName),Current,W.B)&&Current.OrderIndependentCompareEqual(FrozenGetter);};
  const UEdGraphSchema* Schema=Graph->GetSchema();RulePin=Result->FindPin(TEXT("bCanEnterTransition"),EGPD_Input);Schema->BreakPinLinks(*RulePin,true);if(!W.Check(W.Before,true)||!GetterValid())return W.Fail(TEXT("Rule replacement changed unrelated authored graph/getter"));RulePin=Result->FindPin(TEXT("bCanEnterTransition"),EGPD_Input);Output=Getter->FindPin(FName(*VariableName),EGPD_Output);if(!RulePin||!Output||!Schema->TryCreateConnection(Output,RulePin)||!W.Anchors(false))return W.Fail(TEXT("Bool rule connection failed"));RulePin=Result->FindPin(TEXT("bCanEnterTransition"),EGPD_Input);Output=Getter->FindPin(FName(*VariableName),EGPD_Output);if(!GetterValid()||!RulePin||!Output||RulePin->LinkedTo.Num()!=1||RulePin->LinkedTo[0]!=Output||Getter->VariableReference.GetMemberName()!=FName(*VariableName))return W.Fail(TEXT("Bool rule binding changed"));W.R->SetStringField(TEXT("created_node_id"),Getter->NodeGuid.ToString());W.R->SetStringField(TEXT("variable_name"),VariableName);
 }
 else if(Action==TEXT("set_transition_settings")){Transition->PriorityOrder=Priority;Transition->CrossfadeDuration=Crossfade;Transition->bAutomaticRuleBasedOnSequencePlayerInState=Automatic;Transition->AutomaticRuleTriggerTime=Trigger;Transition->Bidirectional=Bidirectional;}
 else
 {FBlueprintEditorUtils::RemoveNode(W.B,Selected,true);if(!W.Anchors(true))return W.Fail(TEXT("Deletion callback changed a surviving object"));Snapshot Now;if(!Capture(W.B,Now))return W.Fail(TEXT("Post-delete graph inventory unavailable"));for(auto* N:W.Changes.RetireNodes)if(Now.Nodes.Contains(N))return W.Fail(TEXT("Deletion incomplete; inspect cascade and implicit compiler state"));for(auto* G:W.Changes.RetireGraphs)if(Now.Graphs.Contains(G))return W.Fail(TEXT("Owned graph deletion incomplete"));}
 if(!W.Establish())return W.Fail(TEXT("Protected authored graph/source changed during engine operation"));
 W.R->SetStringField(TEXT("stage"),TEXT("mark_structural"));W.R->SetBoolField(TEXT("engine_structural_mark_called"),true);W.R->SetBoolField(TEXT("engine_structural_compile_possible"),true);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(W.B);if(!W.Check(W.Expected,false))return W.Fail(TEXT("Structural callback changed protected graph"));
 if(Compile){W.R->SetStringField(TEXT("stage"),TEXT("compile"));W.R->SetBoolField(TEXT("compile_attempted"),true);W.R->SetBoolField(TEXT("full_compile_attempted"),true);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(W.B,EBlueprintCompileOptions::None,&Log);W.R->SetNumberField(TEXT("compile_errors"),Log.NumErrors);const bool Compiled=Log.NumErrors==0&&Live(W.B->GeneratedClass);W.R->SetBoolField(TEXT("compiled"),Compiled);if(!W.Check(W.Expected,false)||!Compiled)return W.Fail(TEXT("Full compile/preservation failed; no subsequent save"));}
 W.R->SetStringField(TEXT("stage"),TEXT("mark_dirty"));W.B->MarkPackageDirty();if(!W.Check(W.Expected,false))return W.Fail(TEXT("Dirty callback changed protected graph"));
 if(Save){W.R->SetStringField(TEXT("stage"),TEXT("save"));W.R->SetBoolField(TEXT("save_attempted"),true);FString File;FPackageName::TryConvertLongPackageNameToFilename(W.B->GetOutermost()->GetName(),File,FPackageName::GetAssetPackageExtension());FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;const bool Saved=UPackage::SavePackage(W.B->GetOutermost(),W.B,*File,Args);W.R->SetBoolField(TEXT("saved"),Saved);if(!W.Check(W.Expected,false)||!Saved)return W.Fail(TEXT("Save/post-save failed; actual saved flag retained"));}
 W.R->SetBoolField(TEXT("success"),true);W.R->SetBoolField(TEXT("dirty_after"),W.B->GetOutermost()->IsDirty());W.R->SetStringField(TEXT("stage"),TEXT("complete"));return Json(W.R);
}
}
