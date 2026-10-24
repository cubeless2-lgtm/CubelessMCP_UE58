// HD Editor-only PCG component generation control. Native raw contracts only.
#include "Compatibility/UE58/CubelessPCGComponentGenerationApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Selection.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "PCGComponent.h"
#include "PCGVolume.h"
#include "Components/BrushComponent.h"
#include "Model.h"
#include "Engine/Polys.h"
#include "PCGGraph.h"
#include "PCGData.h"
#include "PCGManagedResource.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGSettings.h"
#include "PCGInputOutputSettings.h"
#include "PCGSubsystem.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGBoundsModifier.h"
#include "Elements/PCGUnionElement.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/ObjectKey.h"
#include "Serialization/MemoryWriter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Event.h"
#include "K2Node_EditablePinBase.h"
#include "Kismet2/CompilerResultsLog.h"
#include "UObject/UObjectIterator.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "FileHelpers.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Crc.h"
#include "UObject/StrongObjectPtr.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/SoftObjectPath.h"
#include "CoreGlobals.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformTime.h"
#include "Templates/Atomic.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace Cubeless::UE58::PCGComponentGeneration
{
namespace
{
using Variable::FResult;
constexpr bool ConstructionHealthVerified=false;
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(){return IsInGameThread()&&!IsGarbageCollecting()&&!IsEngineExitRequested()&&!GExitPurge&&!UE::IsSavingPackage()&&Live(GEditor);}
bool Same(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}
bool Forbidden(const FString& S){FString L=FPackageName::ExportTextPathToObjectPath(S).TrimStartAndEnd().ToLower();L.TrimQuotesInline();L.ReplaceInline(TEXT("\\"),TEXT("/"));for(const TCHAR* T:{TEXT("/script/unrealmcp"),TEXT("/unrealmcp"),TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("/script/cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtools"),TEXT("cubelesscontrolrigtoolseditor"),TEXT("cubelessmcp"),TEXT("cubeless_mcp"),TEXT("modelcontextprotocol"),TEXT("toolsetregistry"),TEXT("/engine/pythontypes")})if(L.Contains(T))return true;return false;}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
struct FIdentity
{
 TStrongObjectPtr<const UObject> Hold{nullptr};TWeakObjectPtr<const UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Outermost,Package;FString Path;bool Retired=false;
 explicit FIdentity(const UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Outermost(O->GetOutermost()),Package(O->GetPackage()),Path(O->GetPathName()){}
 bool Exact()const{const auto* O=Object.Get();return Retired||(Live(O)&&Live(Class.Get())&&Live(Outermost.Get())&&Live(Package.Get())&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&O->GetOutermost()==Outermost.Get()&&O->GetPackage()==Package.Get()&&Same(O->GetPathName(),Path)&&!O->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists));}
};
struct FPinValue
{
 UEdGraphPin* Raw=nullptr;UEdGraphNode* Owner=nullptr;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;FString Default,AutoDefault,Text;UObject* DefaultObject=nullptr;UEdGraphPin* Parent=nullptr;TArray<UEdGraphPin*> Sub,Links;bool Orphan=false,NotConnectable=false,ReadOnly=false;
};
struct FNodeValue{UEdGraphNode* Node=nullptr;FGuid Id;int32 X=0,Y=0;TArray<UEdGraphPin*> Pins;FString Comment;bool BubbleVisible=false,BubblePinned=false;};
struct FGraphValue{UEdGraph* Graph=nullptr;FGuid Guid;TArray<UEdGraphNode*> Nodes;TArray<FNodeValue> Slots;TArray<FPinValue> Pins;};
struct FComponentRole{UPCGComponent* Component=nullptr;AActor* Actor=nullptr;UWorld* World=nullptr;UPCGGraphInterface* GraphInstance=nullptr;};
FString DirectPropertyShape(const FProperty* P,int32 Depth=0){if(!P||Depth>8)return TEXT("unsupported_nested_signature");FString S=FString::Printf(TEXT("%s:%s:%llu:%d:%d"),*P->GetName(),*P->GetClass()->GetName(),uint64(P->GetPropertyFlags()),P->ArrayDim,P->GetElementSize());if(const auto* O=CastField<FObjectPropertyBase>(P))S+=TEXT(":")+(O->PropertyClass?O->PropertyClass->GetPathName():FString());if(const auto* E=CastField<FEnumProperty>(P))S+=TEXT(":")+(E->GetEnum()?E->GetEnum()->GetPathName():FString());if(const auto* T=CastField<FStructProperty>(P))S+=TEXT(":")+(T->Struct?T->Struct->GetPathName():FString());if(const auto* A=CastField<FArrayProperty>(P))S+=TEXT("|Array=")+DirectPropertyShape(A->Inner,Depth+1);if(const auto* M=CastField<FMapProperty>(P))S+=TEXT("|Key=")+DirectPropertyShape(M->KeyProp,Depth+1)+TEXT("|Value=")+DirectPropertyShape(M->ValueProp,Depth+1);if(const auto* A=CastField<FSetProperty>(P))S+=TEXT("|Set=")+DirectPropertyShape(A->ElementProp,Depth+1);return S;}
FString FunctionShape(const UFunction* F){FString S=FString::Printf(TEXT("%u|%d|%d"),uint32(F->FunctionFlags),F->NumParms,F->ParmsSize);for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::IncludeSuper);It;++It){const FProperty* P=*It;S+=TEXT("|")+DirectPropertyShape(P)+TEXT("|Meta=")+F->GetMetaData(P->GetFName())+TEXT("|CPP=")+F->GetMetaData(FName(*(FString(TEXT("CPP_Default_"))+P->GetName())));}return S;}
struct FWork
{
 FResult R;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Mutation=MakeShared<FJsonObject>(),Compile=MakeShared<FJsonObject>(),Save=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls,Created,ComponentBefore,ComponentAfter;TArray<TUniquePtr<FIdentity>> Objects;TArray<FComponentRole> Components;
 TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;bool ContextCaptured=false,WorldNull=false,Capturing=false,EditingGraph=false,CompilerWindow=false,CompiledCurrentRequired=false;UBlueprint* Blueprint=nullptr;UClass* Generated=nullptr;UClass* Skeleton=nullptr;UClass* ParentClass=nullptr;TArray<FBPVariableDescription> Variables;USimpleConstructionScript* SCS=nullptr;UEdGraph* Graph=nullptr;const UEdGraphSchema_K2* Schema=nullptr;FGraphValue GraphState;UFunction* Function=nullptr;FString FunctionSignature;FString FailedStage,BudgetObject,BudgetProperty;bool DirtyBefore=false;int64 Slots=0,Text=0;
 FWork(){R.Compiled=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("PCG_construction_health_verified"),ConstructionHealthVerified);}
 bool Fail(const TCHAR* Code,const FString& Error){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=Error;FailedStage=R.Stage;}R.bSuccess=false;return false;}
 bool Budget(int64 N,int64 C=0)
 {
  if(N<0||C<0||N>262144-Slots||C>32ll*1024*1024-Text)
  {
   if(!R.Data->HasField(TEXT("result_budget_first_failure"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("kind"),N<0||C<0?TEXT("invalid_request"):N>262144-Slots?TEXT("slots"):TEXT("text"));J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("object"),BudgetObject);J->SetStringField(TEXT("property"),BudgetProperty);J->SetNumberField(TEXT("requested_slots"),N);J->SetNumberField(TEXT("requested_text"),C);J->SetNumberField(TEXT("slots_before"),Slots);J->SetNumberField(TEXT("text_before"),Text);J->SetNumberField(TEXT("slots_limit"),262144);J->SetNumberField(TEXT("text_limit"),32ll*1024*1024);R.Data->SetObjectField(TEXT("result_budget_first_failure"),J);}
   return Fail(TEXT("pcg_component_result_budget"),TEXT("Bounded capture exceeded; actual prefix retained, no retry"));
  }
  Slots+=N;Text+=C;return true;
 }
 UPCGGraph* ScopedSourceGraph=nullptr;TFunction<bool()> ScopedCurrent;TFunction<bool()> HelperCurrent;TFunction<bool()> FinalRefreshObservation;
 bool GraphExact()const;
 bool Admit(const FString& Stage)
 {
  R.Stage=Stage;if(!R.Error.IsEmpty())return false;
  if(!Safe())return Fail(TEXT("pcg_component_context_unavailable"),TEXT("Game thread, live Editor and no GC/exit/purge/save required; no further live observation"));
  auto* Now=GEditor->GetEditorWorldContext().World();if(Now&&(!Live(Now)||Now->bIsTearingDown))return Fail(TEXT("pcg_component_world_unavailable"),TEXT("Editor world unavailable or tearing down"));
  if(!ContextCaptured){Editor=GEditor;World=Now;WorldNull=Now==nullptr;ContextCaptured=true;auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("editor_path"),GEditor->GetPathName());C->SetStringField(TEXT("editor_class"),GEditor->GetClass()->GetPathName());C->SetStringField(TEXT("world_path"),Now?Now->GetPathName():FString());C->SetStringField(TEXT("world_class"),Now?Now->GetClass()->GetPathName():FString());R.Data->SetObjectField(TEXT("execution_context"),C);}
  else if(Editor.Get()!=GEditor||(!WorldNull&&!Live(World.Get()))||World.Get()!=Now)return Fail(TEXT("pcg_component_context_changed"),TEXT("Exact Editor/world identity changed"));
  if(GEditor->PlayWorld||GEditor->GetPIEWorldContext())return Fail(TEXT("pcg_component_PIE_unsupported"),TEXT("Native Editor PCG commands require no PIE/SIE"));
  for(const auto& I:Objects)if(!I->Exact())return Fail(TEXT("pcg_component_target_identity_changed"),TEXT("Captured object class/outer/outermost/effective package/path changed"));
  if(ScopedCurrent){if(!ScopedCurrent())return false;}else for(const auto& C:Components)if(C.Component->GetOwner()!=C.Actor||C.Component->GetWorld()!=C.World||C.Actor->GetWorld()!=C.World||C.Component->GetGraphInstance()!=C.GraphInstance)return Fail(TEXT("pcg_component_role_changed"),TEXT("Exact component owner/world/graph-instance role changed"));
  if(Blueprint&&(Blueprint->ParentClass!=ParentClass||(!CompilerWindow&&(Blueprint->GeneratedClass!=Generated||Blueprint->SkeletonGeneratedClass!=Skeleton||Blueprint->SimpleConstructionScript!=SCS))))return Fail(TEXT("pcg_blueprint_class_role_changed"),TEXT("Current Blueprint generated/skeleton/SCS role changed"));
  if(Blueprint){if(Blueprint->NewVariables.Num()!=Variables.Num())return Fail(TEXT("pcg_binding_variable_layout_changed"),TEXT("Blueprint local variable layout changed"));for(int32 I=0;I<Variables.Num();++I)if(!FBPVariableDescription::StaticStruct()->CompareScriptStruct(&Blueprint->NewVariables[I],&Variables[I],0))return Fail(TEXT("pcg_binding_variable_layout_changed"),TEXT("Blueprint variable type/default/metadata changed during callback"));}
  if(CompiledCurrentRequired&&Blueprint&&(Blueprint->bBeingCompiled||Blueprint->bQueuedForCompilation||(Blueprint->Status!=BS_UpToDate&&Blueprint->Status!=BS_UpToDateWithWarnings)))return Fail(TEXT("pcg_binding_compiled_status_changed"),TEXT("Current compiled Blueprint became queued/compiling/nonready; no save or retry"));
  if(Function&&!Same(FunctionShape(Function),FunctionSignature))return Fail(TEXT("pcg_binding_function_signature_changed"),TEXT("Exact runtime helper reflected signature/flags changed during callback"));
  if(HelperCurrent&&!HelperCurrent())return false;
  if(Graph&&!Capturing&&!EditingGraph&&!GraphExact())return Fail(TEXT("pcg_binding_graph_changed"),TEXT("Exact graph/node/pin values or reciprocal links changed outside the owned SDK step"));
  // GraphExact may observe schema/text through stock callbacks; close the new pure helper role after those observations.
  return !HelperCurrent||HelperCurrent();
 }
 bool Pin(const UObject* O){if(!Admit(TEXT("pin_object"))||!Live(O))return Fail(TEXT("pcg_component_object_unavailable"),TEXT("Expected live object"));for(const auto& I:Objects)if(!I->Retired&&I->Object.Get()==O)return I->Exact();if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
 bool SDK(const TCHAR* Stage,bool Writer=false){if(!Admit(Stage)||!Budget(1))return false;R.bMutationAttempted|=Writer;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetBoolField(TEXT("entered"),true);J->SetBoolField(TEXT("returned"),false);J->SetBoolField(TEXT("mutation_attempted"),Writer);Calls.Add(MakeShared<FJsonValueObject>(J));return true;}
 bool After(const TCHAR* Stage){if(!Calls.IsEmpty())Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
 template<typename F>bool Write(const TCHAR* Stage,F&& Fn){if(!SDK(Stage,true))return false;Fn();return After(Stage);}
 template<typename F,typename T>bool Read(const TCHAR* Stage,F&& Fn,T& Out){if(!SDK(Stage))return false;Out=Fn();return After(Stage);}
 bool RequireConstruction(const TCHAR* Kind){if(ConstructionHealthVerified)return true;R.Data->SetStringField(TEXT("blocked_PCG_operation"),Kind);R.Data->SetStringField(TEXT("construction_evidence_scope"),TEXT("Historical Python UObject/add-node/GC triggers do not prove every pure C++ operation fails. A fresh scoped pure C++ transient canary is proposed separately; this request does not release the gate."));return Fail(TEXT("pcg_component_construction_health_unverified"),TEXT("PCG generation/construction health gate remains closed; the requested PCG-producing operation was not dispatched"));}
 TSharedPtr<FJsonObject> Error(const FString& E){const FString Message=E.IsEmpty()?TEXT("Dedicated PCG operation failed; exact prefix retained"):E;if(R.Error.IsEmpty())Fail(TEXT("pcg_component_operation_failed"),Message);TSharedPtr<FJsonObject> O;const TSharedPtr<FJsonObject>* Existing=nullptr;if(R.Data->TryGetObjectField(TEXT("legacy_result"),Existing)&&Existing&&Existing->IsValid())O=*Existing;else O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),R.Error);return O;}
};
bool PinHeader(const FPinValue& A,const FPinValue& B){return A.Raw==B.Raw&&A.Owner==B.Owner&&A.Id==B.Id&&A.Persistent==B.Persistent&&A.Name==B.Name&&A.Direction==B.Direction&&A.Type==B.Type&&A.Type.bIsUObjectWrapper==B.Type.bIsUObjectWrapper&&A.Type.bSerializeAsSinglePrecisionFloat==B.Type.bSerializeAsSinglePrecisionFloat&&A.Parent==B.Parent&&A.Sub==B.Sub&&A.Orphan==B.Orphan&&A.NotConnectable==B.NotConnectable&&A.ReadOnly==B.ReadOnly;}
bool PinValues(const FPinValue& A,const FPinValue& B){return PinHeader(A,B)&&Same(A.Default,B.Default)&&Same(A.AutoDefault,B.AutoDefault)&&Same(A.Text,B.Text)&&A.DefaultObject==B.DefaultObject;}
bool CaptureGraph(FWork& W,FGraphValue& Out)
{
 TGuardValue<bool> Guard(W.Capturing,true);if(!W.Admit(TEXT("capture_binding_graph")))return false;auto* G=W.Graph;if(!G||!W.Pin(G)||!W.Pin(W.Schema))return false;Out={};Out.Graph=G;Out.Guid=G->GraphGuid;for(const auto& Entry:G->Nodes)Out.Nodes.Add(Entry.Get());if(!W.Budget(Out.Nodes.Num()))return false;
 TSet<UEdGraphNode*> Seen;TSet<UEdGraphPin*> Known;
 for(auto* N:Out.Nodes){if(!N||Seen.Contains(N)||!W.Pin(N)||N->GetGraph()!=G||N->GetOuter()!=G||N->GetPackage()!=G->GetPackage())return W.Fail(TEXT("pcg_binding_node_owner"),TEXT("Graph must contain unique live exactly owned nodes"));Seen.Add(N);FNodeValue V;V.Node=N;V.Id=N->NodeGuid;V.X=N->NodePosX;V.Y=N->NodePosY;V.Comment=N->NodeComment;V.BubbleVisible=N->bCommentBubbleVisible;V.BubblePinned=N->bCommentBubblePinned;for(auto* P:N->Pins)V.Pins.Add(P);if(!W.Budget(V.Pins.Num()))return false;for(auto* P:V.Pins){if(!P||Known.Contains(P))return W.Fail(TEXT("pcg_binding_pin_membership"),TEXT("Null/duplicate pin in physical node array"));Known.Add(P);}Out.Slots.Add(MoveTemp(V));}
 for(const auto& N:Out.Slots)for(auto* P:N.Pins){if(P->GetOwningNode()!=N.Node)return W.Fail(TEXT("pcg_binding_pin_owner"),TEXT("Pin does not belong to its captured graph node"));FPinValue V;V.Raw=P;V.Owner=N.Node;V.Id=P->PinId;V.Persistent=P->PersistentGuid;V.Name=P->PinName;V.Direction=P->Direction;V.Type=P->PinType;V.Default=P->DefaultValue;V.AutoDefault=P->AutogeneratedDefaultValue;V.DefaultObject=P->DefaultObject;V.Text=P->DefaultTextValue.ToString();V.Parent=P->ParentPin;V.Orphan=P->bOrphanedPin;V.NotConnectable=P->bNotConnectable;V.ReadOnly=P->bDefaultValueIsReadOnly;for(auto* S:P->SubPins)V.Sub.Add(S);for(auto* L:P->LinkedTo)V.Links.Add(L);if(!W.Budget(V.Links.Num()+V.Sub.Num(),V.Default.Len()+V.AutoDefault.Len()+V.Text.Len()))return false;if(V.DefaultObject&&!W.Pin(V.DefaultObject))return false;if(V.Type.PinSubCategoryObject.IsValid()&&!W.Pin(V.Type.PinSubCategoryObject.Get()))return false;if(V.Parent&&!Known.Contains(V.Parent))return W.Fail(TEXT("pcg_binding_foreign_parent_pin"),TEXT("Pin parent outside exact graph"));for(auto* S:V.Sub)if(!Known.Contains(S))return W.Fail(TEXT("pcg_binding_foreign_subpin"),TEXT("Subpin outside exact graph"));for(auto* L:V.Links)if(!Known.Contains(L))return W.Fail(TEXT("pcg_binding_foreign_link"),TEXT("Linked raw pin outside exact captured graph; not dereferenced"));Out.Pins.Add(MoveTemp(V));}
 for(const auto& P:Out.Pins)for(auto* L:P.Links){const auto* Q=Out.Pins.FindByPredicate([&](const FPinValue& X){return X.Raw==L;});if(!Q||!Q->Links.Contains(P.Raw)||Q->Direction==P.Direction)return W.Fail(TEXT("pcg_binding_nonreciprocal_link"),TEXT("Expected reciprocal opposite-direction graph-local links"));}
 return W.Admit(TEXT("captured_binding_graph"));
}
bool FWork::GraphExact()const
{
 if(!GraphState.Graph)return true;auto* G=GraphState.Graph;if(G!=Graph||G->GraphGuid!=GraphState.Guid||G->Schema.Get()!=Schema->GetClass()||G->Nodes.Num()!=GraphState.Nodes.Num())return false;for(int32 I=0;I<GraphState.Nodes.Num();++I)if(G->Nodes[I]!=GraphState.Nodes[I])return false;
 for(const auto& N:GraphState.Slots){if(N.Node->GetGraph()!=G||N.Node->GetOuter()!=G||N.Node->NodeGuid!=N.Id||N.Node->NodePosX!=N.X||N.Node->NodePosY!=N.Y||!Same(N.Node->NodeComment,N.Comment)||N.Node->bCommentBubbleVisible!=N.BubbleVisible||N.Node->bCommentBubblePinned!=N.BubblePinned||N.Node->Pins.Num()!=N.Pins.Num())return false;for(int32 I=0;I<N.Pins.Num();++I)if(N.Node->Pins[I]!=N.Pins[I])return false;}
 for(const auto& P:GraphState.Pins){auto* O=P.Raw;if(O->GetOwningNode()!=P.Owner||O->PinId!=P.Id||O->PersistentGuid!=P.Persistent||O->PinName!=P.Name||O->Direction!=P.Direction||O->PinType!=P.Type||O->PinType.bIsUObjectWrapper!=P.Type.bIsUObjectWrapper||O->PinType.bSerializeAsSinglePrecisionFloat!=P.Type.bSerializeAsSinglePrecisionFloat||!Same(O->DefaultValue,P.Default)||!Same(O->AutogeneratedDefaultValue,P.AutoDefault)||!Same(O->DefaultTextValue.ToString(),P.Text)||O->DefaultObject!=P.DefaultObject||O->ParentPin!=P.Parent||O->bOrphanedPin!=P.Orphan||O->bNotConnectable!=P.NotConnectable||O->bDefaultValueIsReadOnly!=P.ReadOnly||O->SubPins!=P.Sub||O->LinkedTo!=P.Links)return false;}
 return true;
}

bool NoEmbeddedNul(const FString& S){for(int32 I=0;I<S.Len();++I)if(S[I]==0)return false;return true;}
struct FExactKeys:DefaultKeyFuncs<FString>
{static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}};
bool Finite(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Count)
{if(!V.IsValid()||Depth>64||++Count>262144){return false;}if(V->Type==EJson::Number){double N=0;return V->TryGetNumber(N)&&FMath::IsFinite(N);}if(V->Type==EJson::String){return NoEmbeddedNul(V->AsString());}if(V->Type==EJson::Array){for(const auto& X:V->AsArray()){if(!Finite(X,Depth+1,Count)){return false;}}}else if(V->Type==EJson::Object){for(const auto& X:V->AsObject()->Values){if(!NoEmbeddedNul(FString(X.Key))||!Finite(X.Value,Depth+1,Count)){return false;}}}return V->Type!=EJson::None;}
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>1048576||!NoEmbeddedNul(Text)){return false;}
    int32 Depth=0;bool InString=false,Escape=false;for(int32 I=0;I<Text.Len();++I){const TCHAR C=Text[I];if(InString){if(Escape){Escape=false;}else if(C=='\\'){Escape=true;}else if(C=='"'){InString=false;}}else if(C=='"'){InString=true;}else if(C=='{'||C=='['){if(++Depth>64){return false;}}else if(C=='}'||C==']'){if(--Depth<0){return false;}}}if(InString||Depth!=0){return false;}
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));TArray<TSet<FString,FExactKeys>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Tokens=0;
    while(Reader->ReadNext(Token)){if(Token==EJsonNotation::Error||++Tokens>262144){return false;}if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()){const FString K(Reader->GetIdentifier());if(!NoEmbeddedNul(K)||Keys.Last().Contains(K)){return false;}Keys.Last().Add(K);}if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart){Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();}else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd){if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}}
    TArray<TSharedPtr<FJsonValue>> A;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),A)||A.Num()!=1||!A[0].IsValid()||A[0]->Type!=EJson::Object){return false;}int32 Count=0;if(!Finite(A[0],0,Count)){return false;}Out=A[0]->AsObject();return Out.IsValid();
}







FString Str(const TSharedPtr<FJsonObject>& P,const TCHAR* K,const FString& D=FString()){FString S;return P->TryGetStringField(K,S)?S:D;}
bool Bool(const TSharedPtr<FJsonObject>& P,const TCHAR* K,bool D){return P->HasField(K)?P->GetBoolField(K):D;}
bool Integer(FWork& W,const TSharedPtr<FJsonObject>& P,const TCHAR* K,int32 D,int32& O){O=D;if(!P->HasField(K))return true;const double N=P->GetNumberField(K);if(!FMath::IsFinite(N)||N<double(MIN_int32)||N>double(MAX_int32))return W.Fail(TEXT("pcg_component_integer_range"),FString(K)+TEXT(" must fit int32 before original truncation"));O=P->GetIntegerField(K);return true;}
TSharedPtr<FJsonObject> ActorJson(FWork& W,AActor* A){auto O=MakeShared<FJsonObject>();if(!W.Admit(TEXT("actor_summary"))||!W.Pin(A))return O;O->SetStringField(TEXT("label"),A->GetActorLabel());O->SetStringField(TEXT("name"),A->GetName());O->SetStringField(TEXT("path"),A->GetPathName());TArray<TSharedPtr<FJsonValue>> Tags;for(const auto& T:A->Tags){if(!W.Budget(1,T.ToString().Len()))return O;Tags.Add(MakeShared<FJsonValueString>(T.ToString()));}O->SetArrayField(TEXT("tags"),Tags);return O;}
TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& V){return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
TSharedPtr<FJsonObject> BoxJson(const FBox& B){auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("valid"),B.IsValid!=0);if(B.IsValid){O->SetArrayField(TEXT("min"),VectorJson(B.Min));O->SetArrayField(TEXT("max"),VectorJson(B.Max));O->SetArrayField(TEXT("center"),VectorJson(B.GetCenter()));O->SetArrayField(TEXT("extent"),VectorJson(B.GetExtent()));}return O;}
TSharedPtr<FJsonObject> ComponentJson(FWork& W,UPCGComponent* C)
{
 auto O=MakeShared<FJsonObject>();if(!W.Admit(TEXT("component_summary"))||!W.Pin(C))return O;auto* A=C->GetOwner();if(!A||!W.Pin(A))return O;int32 Resources=0;const bool Accessible=C->AreManagedResourcesAccessible();if(Accessible){if(!W.SDK(TEXT("ForEachConstManagedResource")))return O;C->ForEachConstManagedResource([&](const UPCGManagedResource*){++Resources;});O->SetNumberField(TEXT("managed_resource_count"),Resources);if(!W.After(TEXT("ForEachConstManagedResource")))return O;}
 const UPCGGraph* G=W.ScopedSourceGraph;if(!W.ScopedCurrent&&!W.Read(TEXT("PCGComponent_GetGraph"),[&]{return C->GetGraph();},G))return O;if(G&&!W.Pin(G))return O;
 const auto& Output=C->GetGeneratedGraphOutput();const int32 OutputCount=Output.TaggedData.Num();if(!W.Budget(28,C->GetPathName().Len()+A->GetPathName().Len()))return O;
 O->SetStringField(TEXT("name"),C->GetName());O->SetStringField(TEXT("path"),C->GetPathName());O->SetStringField(TEXT("component_path"),C->GetPathName());O->SetStringField(TEXT("component_class"),C->GetClass()->GetPathName());O->SetStringField(TEXT("actor_path"),A->GetPathName());O->SetStringField(TEXT("world_path"),C->GetWorld()?C->GetWorld()->GetPathName():FString());O->SetStringField(TEXT("object_identity"),FString::Printf(TEXT("%p"),C));
 O->SetBoolField(TEXT("active"),C->IsActive());O->SetBoolField(TEXT("pcg_activated"),C->bActivated);O->SetBoolField(TEXT("generated"),C->bGenerated);O->SetBoolField(TEXT("generated_offline"),C->IsGeneratedOffline());O->SetBoolField(TEXT("registered"),C->IsRegistered());O->SetBoolField(TEXT("generating"),C->IsGenerating());O->SetBoolField(TEXT("cleaning_up"),C->IsCleaningUp());O->SetBoolField(TEXT("managed_resources_accessible"),Accessible);O->SetNumberField(TEXT("managed_resource_count"),Resources);O->SetNumberField(TEXT("generated_output_data_count"),OutputCount);O->SetStringField(TEXT("graph"),G?G->GetPathName():FString());O->SetObjectField(TEXT("last_generated_bounds"),BoxJson(C->GetLastGeneratedBounds()));O->SetBoolField(TEXT("refresh_in_progress"),C->IsRefreshInProgress());O->SetBoolField(TEXT("was_generated_this_session"),C->WasGeneratedThisSession());O->SetObjectField(TEXT("actor"),ActorJson(W,A));return O;
}
bool MatchActor(AActor* A,const TSharedPtr<FJsonObject>& P){const FString Label=A->GetActorLabel(),Name=A->GetName(),Path=A->GetPathName();const FString L=Str(P,TEXT("actor_label")),N=Str(P,TEXT("actor_name")),S=Str(P,TEXT("actor_path")),T=Str(P,TEXT("actor_tag")),Prefix=Str(P,TEXT("actor_label_prefix"));return (L.IsEmpty()||Label.Equals(L,ESearchCase::IgnoreCase))&&(N.IsEmpty()||Name.Equals(N,ESearchCase::IgnoreCase))&&(S.IsEmpty()||Path.Equals(S,ESearchCase::IgnoreCase))&&(T.IsEmpty()||A->Tags.Contains(FName(*T)))&&(Prefix.IsEmpty()||Label.StartsWith(Prefix,ESearchCase::IgnoreCase));}
// OI: finite request-local source proof. No task registry, ticker or source mutation.
FString ScopedObjectKey(const UObject* Object)
{
 TArray<uint8> Bytes;FObjectKey Key(Object);FMemoryWriter Writer(Bytes,false);Writer<<Key;return BytesToHex(Bytes.GetData(),Bytes.Num());
}
FString ScopedValueText(const TSharedPtr<FJsonValue>& Value)
{if(!Value)return TEXT("<missing>");FString Text;FJsonSerializer::Serialize(Value,TEXT(""),TJsonWriterFactory<>::Create(&Text));return Text;}
bool ScopedDifference(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,FString& Path,FString& Want,FString& Got,bool StrictText=false)
{
 if(A&&B&&A->Type==B->Type){
  if(A->Type==EJson::Object){TArray<FString> Keys;for(const auto& P:A->AsObject()->Values)Keys.Add(FString(P.Key.ToView()));for(const auto& P:B->AsObject()->Values)Keys.AddUnique(FString(P.Key.ToView()));Keys.Sort();for(const FString& Key:Keys){FString Child=Path+TEXT("/")+Key;if(ScopedDifference(A->AsObject()->TryGetField(Key),B->AsObject()->TryGetField(Key),Child,Want,Got,StrictText)){Path=Child;return true;}}return false;}
  if(A->Type==EJson::Array){const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()){Path+=TEXT("/length");Want=FString::FromInt(X.Num());Got=FString::FromInt(Y.Num());return true;}for(int32 I=0;I<X.Num();++I){FString Child=Path+FString::Printf(TEXT("/%d"),I);if(ScopedDifference(X[I],Y[I],Child,Want,Got,StrictText)){Path=Child;return true;}}return false;}
  if(StrictText&&A->Type==EJson::String){if(A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive))return false;}else if(FJsonValue::CompareEqual(*A,*B))return false;
 }
 Want=ScopedValueText(A);Got=ScopedValueText(B);return true;
}
// AEH: copied logical scalar bags only. No setter, refresh, normalization,
// property-bag type creation or cached-address invariant is introduced.
bool CopyScalarUserParameters(const UPCGGraph* Graph,const UPCGGraphInstance* Instance,TSharedPtr<FJsonObject>& Out,FString& Error)
{
 auto Bad=[&](const FString& What){Error=What;return false;};
 TStrongObjectPtr<const UPCGGraph> GraphHold(Graph);TStrongObjectPtr<const UPCGGraphInstance> InstanceHold(Instance);
 auto RolesCurrent=[&](){return (!Graph||(Live(Graph)&&Graph->GetClass()==UPCGGraph::StaticClass()))&&(!Instance||(Live(Instance)&&Instance->GetClass()==UPCGGraphInstance::StaticClass()&&Instance->Graph.IsResolved()&&Instance->Graph==Graph));};
 if(!RolesCurrent())return Bad(TEXT("Same held direct native graph/instance roles required before scalar read"));
 auto ReadOnce=[&](const FInstancedPropertyBag& Bag,const FString& Role,TSharedPtr<FJsonObject>& Result)->bool
 {
  if(!Safe())return Bad(Role+TEXT(": Editor context changed before const read"));
  const UPropertyBag* Type=Bag.GetPropertyBagStruct();TStrongObjectPtr<const UPropertyBag> HeldType(Type);
  const auto View=Bag.GetValue();
  // Short-lived read coherence only; the pointer is never retained/compared in
  // a future invocation or across compiler reconstruction of the instance.
  if(Type&&(!View.IsValid()||View.GetScriptStruct()!=Type))return Bad(Role+TEXT(": typed view changed while capturing current descriptor"));
  auto CopyDescriptors=[&](const UPropertyBag* BagType,TArray<FPropertyBagPropertyDesc>& Rows)->bool
  {
   if(!BagType)return true;const auto NativeRows=BagType->GetPropertyDescs();if(NativeRows.Num()>64)return Bad(Role+TEXT(": descriptor inventory exceeds64"));
#if WITH_EDITORONLY_DATA
   int32 MetaCharacters=0;for(const auto& Row:NativeRows){if(Row.MetaData.Num()>64)return Bad(Role+TEXT(": metadata inventory exceeds64 per descriptor"));for(const auto& Meta:Row.MetaData){const int32 KeyCharacters=Meta.Key.ToString().Len();if(Meta.Value.Len()>16384||KeyCharacters>16384||Meta.Value.Len()+KeyCharacters>65536-MetaCharacters)return Bad(Role+TEXT(": copied metadata text bound exceeded"));MetaCharacters+=Meta.Value.Len()+KeyCharacters;}}
#endif
   Rows.Append(NativeRows);return true;
  };
  TArray<FPropertyBagPropertyDesc> Descs;if(!CopyDescriptors(Type,Descs))return false;
  if(Descs.Num()>64||Bag.GetNumPropertiesInBag()!=Descs.Num())return Bad(Role+TEXT(": scalar descriptor inventory changed or exceeds64"));
  Descs.Sort([](const FPropertyBagPropertyDesc& A,const FPropertyBagPropertyDesc& B){return A.ID.ToString(EGuidFormats::DigitsWithHyphens)<B.ID.ToString(EGuidFormats::DigitsWithHyphens);});
  TSet<FGuid> IDs;TSet<FName> Names;TArray<TSharedPtr<FJsonValue>> Schema;auto Values=MakeShared<FJsonObject>();int32 TextBudget=0;
  // Coherence belongs to this single read window only. A following window
  // may own a reconstructed type/buffer as long as its logical schema is equal.
  auto Coherent=[&]()->bool
  {
   if(!Safe()||!RolesCurrent())return Bad(Role+TEXT(": graph/instance or Editor role changed during const getter"));
   const UPropertyBag* CurrentType=Bag.GetPropertyBagStruct();TStrongObjectPtr<const UPropertyBag> CurrentTypeHold(CurrentType);const auto CurrentView=Bag.GetValue();
   if(CurrentType!=Type||CurrentView.GetScriptStruct()!=View.GetScriptStruct()||CurrentView.GetMemory()!=View.GetMemory())return Bad(Role+TEXT(": bag type/view changed inside one const read window"));
   TArray<FPropertyBagPropertyDesc> CurrentDescs;if(!CopyDescriptors(Type,CurrentDescs))return false;
   if(CurrentDescs.Num()!=Descs.Num())return Bad(Role+TEXT(": descriptor inventory changed inside one const read window"));
   TMap<FGuid,const FPropertyBagPropertyDesc*> ByID;ByID.Reserve(CurrentDescs.Num());for(const auto& Row:CurrentDescs){if(ByID.Contains(Row.ID))return Bad(Role+TEXT(": duplicate descriptor ID inside one const read window"));ByID.Add(Row.ID,&Row);}
   for(const auto& Before:Descs)
   {
    const auto* Current=ByID.FindRef(Before.ID);
    if(!Current||!Current->Name.IsEqual(Before.Name,ENameCase::CaseSensitive)||Current->ValueType!=Before.ValueType||Current->ContainerTypes!=Before.ContainerTypes||Current->PropertyFlags!=Before.PropertyFlags)return Bad(Role+TEXT(": descriptor logical schema changed inside one const read window: ")+Before.ID.ToString(EGuidFormats::DigitsWithHyphens));
#if WITH_EDITORONLY_DATA
    if(Current->MetaData.Num()!=Before.MetaData.Num())return Bad(Role+TEXT(": descriptor metadata count changed inside one const read window"));
    for(int32 M=0;M<Before.MetaData.Num();++M)if(!Current->MetaData[M].Key.IsEqual(Before.MetaData[M].Key,ENameCase::CaseSensitive)||!Current->MetaData[M].Value.Equals(Before.MetaData[M].Value,ESearchCase::CaseSensitive))return Bad(Role+TEXT(": descriptor authored metadata changed inside one const read window: ")+Before.ID.ToString(EGuidFormats::DigitsWithHyphens));
#endif
   }
   return true;
  };
  if(!Coherent())return false;
  for(const auto& Desc:Descs)
  {
   // Desc is an owned row copy. CachedProperty/CachedIndex and inactive type
   // object caches are not used; all public getters re-find the current Name.
   const FGuid ID=Desc.ID;const FName Name=Desc.Name;const EPropertyBagPropertyType Kind=Desc.ValueType;
   const FString IDText=ID.ToString(EGuidFormats::DigitsWithHyphens),NameText=Name.ToString();
   if(!ID.IsValid()||Name.IsNone()||IDs.Contains(ID)||Names.Contains(Name)||!Desc.ContainerTypes.IsEmpty())return Bad(Role+TEXT(": invalid/duplicate ID or Name, or unsupported container: ")+NameText+TEXT("/")+IDText);
   IDs.Add(ID);Names.Add(Name);TSharedPtr<FJsonValue> Value;FString Label;auto ReadError=[&](){return Bad(Role+TEXT(": current scalar getter failed for ")+NameText+TEXT("/")+IDText+FString::Printf(TEXT(" (type%d)"),int32(Kind)));};
   switch(Kind)
   {
    case EPropertyBagPropertyType::Bool:{auto R=Bag.GetValueBool(Name);if(!Coherent())return false;if(!R.IsValid())return ReadError();Value=MakeShared<FJsonValueBoolean>(R.GetValue());Label=TEXT("Bool");break;}
    case EPropertyBagPropertyType::Byte:{auto R=Bag.GetValueByte(Name);if(!Coherent())return false;if(!R.IsValid())return ReadError();Value=MakeShared<FJsonValueNumber>(R.GetValue());Label=TEXT("Byte");break;}
    case EPropertyBagPropertyType::Int32:{auto R=Bag.GetValueInt32(Name);if(!Coherent())return false;if(!R.IsValid())return ReadError();Value=MakeShared<FJsonValueNumber>(R.GetValue());Label=TEXT("Int32");break;}
    case EPropertyBagPropertyType::Int64:{auto R=Bag.GetValueInt64(Name);if(!Coherent())return false;if(!R.IsValid())return ReadError();Value=MakeShared<FJsonValueString>(LexToString(R.GetValue()));Label=TEXT("Int64");break;}
    case EPropertyBagPropertyType::Float:{auto R=Bag.GetValueFloat(Name);if(!Coherent())return false;if(!R.IsValid())return ReadError();const float V=R.GetValue();if(!FMath::IsFinite(V))return Bad(Role+TEXT(": nonfinite Float pending: ")+NameText);auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("number"),V);J->SetStringField(TEXT("bits"),BytesToHex(reinterpret_cast<const uint8*>(&V),sizeof(V)));Value=MakeShared<FJsonValueObject>(J);Label=TEXT("Float");break;}
    case EPropertyBagPropertyType::Double:{auto R=Bag.GetValueDouble(Name);if(!Coherent())return false;if(!R.IsValid())return ReadError();const double V=R.GetValue();if(!FMath::IsFinite(V))return Bad(Role+TEXT(": nonfinite Double pending: ")+NameText);auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("number"),V);J->SetStringField(TEXT("bits"),BytesToHex(reinterpret_cast<const uint8*>(&V),sizeof(V)));Value=MakeShared<FJsonValueObject>(J);Label=TEXT("Double");break;}
    case EPropertyBagPropertyType::Name:{auto R=Bag.GetValueName(Name);if(!Coherent())return false;if(!R.IsValid())return ReadError();const FName V=R.GetValue();Value=MakeShared<FJsonValueString>(V.ToString());Label=TEXT("Name");break;}
    case EPropertyBagPropertyType::String:{auto R=Bag.GetValueString(Name);if(!Coherent())return false;if(!R.IsValid())return ReadError();const FString V=R.GetValue();TextBudget+=V.Len();if(V.Len()>16384||TextBudget>65536)return Bad(Role+TEXT(": copied scalar string bound exceeded: ")+NameText);Value=MakeShared<FJsonValueString>(V);Label=TEXT("String");break;}
    default:return Bad(Role+TEXT(": enum/struct/reference/container/Text or other scalar variant remains pending: ")+NameText+FString::Printf(TEXT(" (type%d)"),int32(Kind)));
   }
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),IDText);Row->SetStringField(TEXT("name"),NameText);Row->SetStringField(TEXT("type"),Label);Row->SetStringField(TEXT("property_flags"),LexToString(Desc.PropertyFlags));TArray<TSharedPtr<FJsonValue>> MetaRows;
#if WITH_EDITORONLY_DATA
   for(const auto& Meta:Desc.MetaData){const FString Key=Meta.Key.ToString();const FString Text=Meta.Value;if(Text.Len()+Key.Len()>65536-TextBudget)return Bad(Role+TEXT(": total scalar/metadata text bound exceeded"));TextBudget+=Text.Len()+Key.Len();auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("key"),Key);M->SetStringField(TEXT("value"),Text);MetaRows.Add(MakeShared<FJsonValueObject>(M));}
#endif
   Row->SetArrayField(TEXT("metadata"),MetaRows);Schema.Add(MakeShared<FJsonValueObject>(Row));Values->SetField(IDText,Value);
  }
  if(!Coherent())return false;
  Result=MakeShared<FJsonObject>();Result->SetArrayField(TEXT("schema"),Schema);Result->SetObjectField(TEXT("values"),Values);return true;
 };
 auto ReadStable=[&](const FInstancedPropertyBag& Bag,const FString& Role,TSharedPtr<FJsonObject>& Result)->bool
 {
  TSharedPtr<FJsonObject> First,Second;if(!ReadOnce(Bag,Role,First)||!ReadOnce(Bag,Role,Second))return false;FString Path,Want,Got;
  if(ScopedDifference(MakeShared<FJsonValueObject>(First),MakeShared<FJsonValueObject>(Second),Path,Want,Got,true))return Bad(Role+TEXT(": logical schema/value changed during read at ")+Path+TEXT(" expected=")+Want+TEXT(" actual=")+Got);
  Result=First;return true; // Keep the first copied logical state, never adopt the second.
 };
 TSharedPtr<FJsonObject> Defaults,Actual;Out=MakeShared<FJsonObject>();
 if(Graph){if(Graph->GetClass()!=UPCGGraph::StaticClass())return Bad(TEXT("Exact direct native graph required for scalar user parameters"));const FInstancedPropertyBag* Bag=Graph->GetUserParametersStruct();if(!Bag||!ReadStable(*Bag,TEXT("graph_defaults"),Defaults))return false;Out->SetObjectField(TEXT("graph_defaults"),Defaults);}
 else Out->SetField(TEXT("graph_defaults"),MakeShared<FJsonValueNull>());
 if(Instance)
 {
  if(Instance->GetClass()!=UPCGGraphInstance::StaticClass()||!Instance->Graph.IsResolved()||Instance->Graph!=Graph)return Bad(TEXT("Same exact native instance/direct graph slot required for scalar parameters"));
  const FInstancedPropertyBag* Bag=Instance->GetUserParametersStruct();if(!Bag||!ReadStable(*Bag,TEXT("instance_actual"),Actual))return false;
  const TArray<TSharedPtr<FJsonValue>>& Rows=Actual->GetArrayField(TEXT("schema"));if(!Graph&&!Rows.IsEmpty())return Bad(TEXT("Nonnull graph required for a nonempty instance parameter schema"));
  Out->SetObjectField(TEXT("instance_actual"),Actual);TArray<FGuid> Marks=Instance->ParametersOverrides.PropertiesIDsOverridden.Array();Marks.Sort();TArray<TSharedPtr<FJsonValue>> MarkRows;
  // The explicit override set is independent of value equality with defaults.
  // Do not call IsPropertyOverriddenAndNotDefault, Refresh or CopyOverrides.
  if(Marks.Num()>64)return Bad(TEXT("Instance override mark inventory exceeds64"));for(const FGuid& ID:Marks)MarkRows.Add(MakeShared<FJsonValueString>(ID.ToString(EGuidFormats::DigitsWithHyphens)));Out->SetArrayField(TEXT("override_guids"),MarkRows);
 }
 if(!RolesCurrent()||!Safe())return Bad(TEXT("Held graph/instance role or Editor context changed during scalar reads"));
 Out->SetStringField(TEXT("scope"),TEXT("Copied logical scalar ID/Name/type/flags/authored metadata, graph defaults, instance actual values and explicit override GUID set; inactive scalar MetaClass/type-object fields are not observed. No bag type/buffer/CachedProperty address invariant or normalization."));return true;
}

struct FScopedGeneration
{
 FWork& W;bool Captured=false,CleanupMode=false,CompositeMode=false,RefreshMode=false;TArray<UActorComponent*> CleanupComponents;TSharedPtr<FJsonObject> Expected,Baseline;TArray<TSharedPtr<FJsonValue>> ObjectRows;TMap<FString,const UObject*> Bound;
 UPCGComponent* Component=nullptr;AActor* Actor=nullptr;UPCGGraphInstance* Instance=nullptr;UPCGGraph* Source=nullptr;UBlueprint* SourceBlueprint=nullptr;bool NativeHost=false,NativeVolumeHost=false;UStaticMesh* NativeMesh=nullptr;
 struct FFieldRole{UStruct* Owner=nullptr;FName Name;FProperty* Field=nullptr;FFieldClass* Class=nullptr;void* DirectOwner=nullptr;int32 Offset=0,Size=0,Dim=0;uint64 Flags=0;};TArray<FFieldRole> Fields;struct FInnerRole{FArrayProperty* Array=nullptr;FProperty* Inner=nullptr;FFieldClass* Class=nullptr;int32 Offset=0,Size=0,Dim=0;uint64 Flags=0;};TArray<FInnerRole> Inners;
 explicit FScopedGeneration(FWork& Work):W(Work){}
 bool BindingSourceOnly=false;TFunction<bool(const UObject*)> BindingKeep;
 template<typename T>T* Resolved(const TObjectPtr<T>& Handle)
 {
  if(!BindingSourceOnly)return Handle.Get();
  if(!Handle.IsResolved()){Stop(TEXT("pcg_binding_source_unresolved"),TEXT("source_graph"),TEXT("reference"),TEXT("already resolved resident handle"),TEXT("unresolved"));return nullptr;}
  if(Handle==nullptr)return nullptr;
  // Public path observation of an already-resolved handle avoids tracked Get().
  UObject* Value=StaticFindObject(UObject::StaticClass(),nullptr,*Handle.GetPathName());
  TStrongObjectPtr<UObject> Hold(Live(Value)?Value:nullptr);
  if(!Value||Handle!=Value||!BindingKeep||!BindingKeep(Value)){Stop(TEXT("pcg_binding_source_resident_changed"),TEXT("source_graph"),TEXT("resident reference"),TEXT("same resolved held object"),TEXT("unavailable/replaced"));return nullptr;}
  return Cast<T>(Value);
 }
 template<typename T>T* ResolvedSoft(const TSoftObjectPtr<T>& Handle)
 {
  if(!BindingSourceOnly)return Handle.Get();
  if(Handle.IsNull())return nullptr;
  const FString Path=Handle.ToSoftObjectPath().ToString();UObject* Value=StaticFindObject(UObject::StaticClass(),nullptr,*Path);
  TStrongObjectPtr<UObject> Hold(Live(Value)?Value:nullptr);
  if(!Live(Value)||!Same(Value->GetPathName(),Path)||!BindingKeep||!BindingKeep(Value)){Stop(TEXT("pcg_binding_source_asset_not_resident"),TEXT("source_asset"),TEXT("soft_reference"),TEXT("already loaded exact resident soft path; no load or weak-cache normalization"),Path);return nullptr;}
  return Cast<T>(Value);
 }
 bool Stop(const TCHAR* Code,const FString& Object,const FString& Property,const FString& Want,const FString& Got,bool Observed=true)
 {
  const TCHAR* FailureKey=CompositeMode?TEXT("scoped_cleanup_generate_first_failure"):CleanupMode?TEXT("scoped_cleanup_first_failure"):TEXT("scoped_generate_first_failure");if(!W.R.Data->HasField(FailureKey)){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),W.R.Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Want);J->SetStringField(TEXT("actual"),Got);J->SetBoolField(TEXT("expected_actual_observed"),Observed);J->SetBoolField(TEXT("independent_engine_reread"),false);W.R.Data->SetObjectField(FailureKey,J);}
  return W.Fail(Code,CompositeMode?TEXT("Scoped immediate cleanup/generate source/target failed; actual completed cleanup or accepted-task prefix retained, no replay or unsafe follow-up"):CleanupMode?TEXT("Scoped immediate CPU cleanup source/target failed; actual prefix retained, no retry or unsafe follow-up"):TEXT("Scoped CPU generation source/target admission failed; exact call prefix retained, no retry or unsafe follow-up"));
 }
 bool Condition(bool Okay,const FString& Object,const FString& Property,const FString& Want,const FString& Got)
 {return Okay||Stop(CompositeMode?TEXT("pcg_scoped_cleanup_generate_profile_pending"):CleanupMode?TEXT("pcg_scoped_cleanup_profile_pending"):TEXT("pcg_scoped_generate_profile_pending"),Object,Property,Want,Got,false);}
 bool ExpectShape(const TSharedPtr<FJsonObject>& E)
 {
  if(!E||E->Values.Num()!=4||(Str(E,TEXT("profile"))!=TEXT("owned_editor_cpu_createpoints_v1")&&Str(E,TEXT("profile"))!=TEXT("owned_editor_cpu_weighted_ism_v1")&&Str(E,TEXT("profile"))!=TEXT("owned_editor_cpu_createpoints_refresh_v1")&&Str(E,TEXT("profile"))!=TEXT("owned_editor_cpu_point_pipeline_v1")&&Str(E,TEXT("profile"))!=TEXT("owned_editor_cpu_point_pipeline_refresh_v1")))return Stop(TEXT("pcg_scoped_expectation_invalid"),TEXT("expectation"),TEXT("profile/shape"),TEXT("exact finite profile with four keys"),TEXT("invalid"),false);
  RefreshMode=Str(E,TEXT("profile"))==TEXT("owned_editor_cpu_createpoints_refresh_v1")||Str(E,TEXT("profile"))==TEXT("owned_editor_cpu_point_pipeline_refresh_v1");
  const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;const TArray<TSharedPtr<FJsonValue>>* Files=nullptr;const TSharedPtr<FJsonObject>* Authored=nullptr;
  if(!E->TryGetArrayField(TEXT("objects"),Rows)||!Rows||Rows->IsEmpty()||Rows->Num()>128||!E->TryGetObjectField(TEXT("source_authored"),Authored)||!Authored||!(*Authored)->Values.Num()||!E->TryGetArrayField(TEXT("source_files"),Files)||!Files||(Str(E,TEXT("profile"))!=TEXT("owned_editor_cpu_weighted_ism_v1")?Files->Num()!=((*Authored)->HasField(TEXT("native_volume_host"))?1:2):(Files->Num()<3||Files->Num()>66)))return Stop(TEXT("pcg_scoped_expectation_invalid"),TEXT("expectation"),TEXT("objects/source_authored/source_files"),TEXT("bounded objects, nonempty authored object, two files"),TEXT("invalid"),false);
  TSet<FString> Roles;for(const auto& Row:*Rows){if(!Row||Row->Type!=EJson::Object)return false;const auto J=Row->AsObject();if(J->Values.Num()!=6)return false;for(const TCHAR* Key:{TEXT("role"),TEXT("path"),TEXT("class_path"),TEXT("outer_path"),TEXT("package_path"),TEXT("identity")}){FString Value;if(!J->TryGetStringField(Key,Value)||Value.Len()>4096||(Value.IsEmpty()&&FCString::Strcmp(Key,TEXT("outer_path"))!=0))return false;}const FString Role=Str(J,TEXT("role"));if(Roles.Contains(Role))return false;Roles.Add(Role);}
  for(const auto& Value:*Files){if(!Value||Value->Type!=EJson::Object)return false;const auto J=Value->AsObject();double Bytes=0;bool Exists=false;const FString Hash=Str(J,TEXT("sha1"));if(Str(J,TEXT("path")).IsEmpty()||!J->TryGetBoolField(TEXT("exists"),Exists)||!Exists||!J->TryGetNumberField(TEXT("bytes"),Bytes)||Bytes<0||Bytes>67108864||FMath::FloorToDouble(Bytes)!=Bytes||Hash.Len()!=40)return false;for(TCHAR Ch:Hash)if(!FChar::IsHexDigit(Ch))return false;}
  Expected=E;return true;
 }
 bool Object(const FString& Role,const UObject* Value)
 {
  if(Captured){const auto* Old=Bound.Find(Role);return (Old&&*Old==Value)||Stop(TEXT("pcg_scoped_binding_changed"),Role,TEXT("object_role"),Old?FString::Printf(TEXT("%p"),*Old):TEXT("captured role"),FString::Printf(TEXT("%p"),Value));}
  if(!Live(Value))return Stop(TEXT("pcg_scoped_object_unavailable"),Role,TEXT("lifetime"),TEXT("live resolved object"),TEXT("null/unavailable"));
  if(Bound.Contains(Role))return Bound[Role]==Value||Stop(TEXT("pcg_scoped_duplicate_role"),Role,TEXT("object_identity"),ScopedObjectKey(Bound[Role]),ScopedObjectKey(Value));if(ObjectRows.Num()>=128)return Stop(TEXT("pcg_scoped_object_budget"),Role,TEXT("object_count"),TEXT("<=128 roles"),FString::FromInt(ObjectRows.Num()+1));
  const FString Identity=ScopedObjectKey(Value);
  if(Expected){const auto& Rows=Expected->GetArrayField(TEXT("objects"));const TSharedPtr<FJsonValue>* Row=Rows.FindByPredicate([&](const TSharedPtr<FJsonValue>& V){return Str(V->AsObject(),TEXT("role"))==Role;});if(!Row||Str((*Row)->AsObject(),TEXT("identity"))!=Identity)return Stop(TEXT("pcg_scoped_expected_identity_changed"),Role,TEXT("identity"),Row?Str((*Row)->AsObject(),TEXT("identity")):TEXT("captured expected role"),Identity);}
  // FObjectKey comparison precedes any path/class/outer read of a replacement role.
  TStrongObjectPtr<const UObject> Returned(Value);if(BindingSourceOnly){if(!BindingKeep||!BindingKeep(Value))return false;}else if(!W.Pin(Value))return false;if(Forbidden(Value->GetClass()->GetPathName()))return Stop(TEXT("pcg_scoped_dependency_forbidden"),Role,TEXT("class"),TEXT("native engine/game source"),Value->GetClass()->GetPathName());
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("role"),Role);J->SetStringField(TEXT("path"),Value->GetPathName());J->SetStringField(TEXT("class_path"),Value->GetClass()->GetPathName());J->SetStringField(TEXT("outer_path"),Value->GetOuter()?Value->GetOuter()->GetPathName():FString());J->SetStringField(TEXT("package_path"),Value->GetPackage()->GetPathName());J->SetStringField(TEXT("identity"),Identity);ObjectRows.Add(MakeShared<FJsonValueObject>(J));Bound.Add(Role,Value);return true;
 }
 FProperty* Field(UStruct* Owner,const FName Name)
 {
  FProperty* Fresh=FindFProperty<FProperty>(Owner,Name);const auto* Old=Fields.FindByPredicate([&](const FFieldRole& F){return F.Owner==Owner&&F.Name==Name;});
  if(Old){if(Fresh!=Old->Field)return Stop(TEXT("pcg_scoped_field_changed"),Owner->GetPathName(),Name.ToString(),TEXT("captured physical field membership"),TEXT("different/missing field"),false),nullptr;if(Fresh->GetClass()!=Old->Class||Fresh->GetOwnerVariant().GetRawPointer()!=Old->DirectOwner||Fresh->GetOffset_ForInternal()!=Old->Offset||Fresh->GetElementSize()!=Old->Size||Fresh->ArrayDim!=Old->Dim||uint64(Fresh->GetPropertyFlags())!=Old->Flags)return Stop(TEXT("pcg_scoped_field_changed"),Owner->GetPathName(),Name.ToString(),TEXT("captured native field layout/owner/flags"),TEXT("compound layout mismatch"),false),nullptr;return Fresh;}
  if(Captured||!Fresh||!Fresh->GetOwnerStruct()||!Fresh->GetOwnerStruct()->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||Fresh->ArrayDim!=1)return Stop(TEXT("pcg_scoped_field_unsupported"),Owner->GetPathName(),Name.ToString(),TEXT("loaded native single field"),TEXT("missing/non-native/arraydim"),false),nullptr;
  Fields.Add({Owner,Name,Fresh,Fresh->GetClass(),Fresh->GetOwnerVariant().GetRawPointer(),Fresh->GetOffset_ForInternal(),Fresh->GetElementSize(),Fresh->ArrayDim,uint64(Fresh->GetPropertyFlags())});return Fresh;
 }
 UObject* Ref(UObject* Owner,const FName Name)
 {
  auto* P=CastField<FObjectProperty>(Field(Owner->GetClass(),Name));if(!P){if(W.R.Error.IsEmpty())Stop(TEXT("pcg_scoped_object_field_layout"),Owner->GetPathName(),Name.ToString(),TEXT("FObjectProperty"),TEXT("other/missing field"));return nullptr;}auto* Ptr=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(Owner));if(!Ptr->IsResolved())return Stop(TEXT("pcg_scoped_unresolved_binding"),Owner->GetPathName(),Name.ToString(),TEXT("already resolved reference; no loader"),TEXT("unresolved")),nullptr;return Resolved(*Ptr);
 }
 bool Refs(UObject* Owner,const FName Name,TArray<UObject*>& Out)
 {
  auto* A=CastField<FArrayProperty>(Field(Owner->GetClass(),Name));if(!W.R.Error.IsEmpty())return false;if(!A)return Stop(TEXT("pcg_scoped_array_layout"),Owner->GetPathName(),Name.ToString(),TEXT("native array field"),TEXT("other field class"));const auto* Held=Inners.FindByPredicate([&](const FInnerRole& R){return R.Array==A;});if(Held&&A->Inner!=Held->Inner)return Stop(TEXT("pcg_scoped_array_inner_changed"),Owner->GetPathName(),Name.ToString(),TEXT("captured Inner pointer before dereference"),TEXT("different Inner"),false);auto* P=CastField<FObjectProperty>(A->Inner);if(!P||P->GetOwnerVariant().GetRawPointer()!=A)return Stop(TEXT("pcg_scoped_array_layout"),Owner->GetPathName(),Name.ToString(),TEXT("object array with Inner directly owned by array field"),TEXT("different layout"),false);
  if(Held){if(P->GetClass()!=Held->Class||P->GetOffset_ForInternal()!=Held->Offset||P->GetElementSize()!=Held->Size||P->ArrayDim!=Held->Dim||uint64(P->GetPropertyFlags())!=Held->Flags)return Stop(TEXT("pcg_scoped_array_inner_changed"),Owner->GetPathName(),Name.ToString(),TEXT("captured Inner identity/layout before buffer access"),TEXT("compound Inner mismatch"),false);}else if(Captured)return Stop(TEXT("pcg_scoped_array_inner_changed"),Owner->GetPathName(),Name.ToString(),TEXT("previously captured Inner"),TEXT("new array field"));else Inners.Add({A,P,P->GetClass(),P->GetOffset_ForInternal(),P->GetElementSize(),P->ArrayDim,uint64(P->GetPropertyFlags())});
  FScriptArrayHelper H(A,A->ContainerPtrToValuePtr<void>(Owner));if(H.Num()>128)return Stop(TEXT("pcg_scoped_array_budget"),Owner->GetPathName(),Name.ToString(),TEXT("<=128 elements"),FString::FromInt(H.Num()));for(int32 I=0;I<H.Num();++I){auto* Ptr=P->GetObjectPtrPropertyValuePtr(H.GetRawPtr(I));if(!Ptr->IsResolved())return Stop(TEXT("pcg_scoped_unresolved_binding"),Owner->GetPathName(),Name.ToString(),TEXT("resolved array handles"),TEXT("unresolved"));Out.Add(Resolved(*Ptr));if(!W.R.Error.IsEmpty())return false;}return true;
 }
 bool Scalars(UObject* Owner,const TArray<FName>& Names,const TSharedRef<FJsonObject>& Out)
 {
  for(const FName Name:Names){auto* P=Field(Owner->GetClass(),Name);if(!P)return false;if(auto* B=CastField<FBoolProperty>(P))Out->SetBoolField(Name.ToString(),B->GetPropertyValue_InContainer(Owner));else if(auto* I=CastField<FIntProperty>(P))Out->SetNumberField(Name.ToString(),I->GetPropertyValue_InContainer(Owner));else if(auto* D=CastField<FDoubleProperty>(P))Out->SetNumberField(Name.ToString(),D->GetPropertyValue_InContainer(Owner));else return Stop(TEXT("pcg_scoped_scalar_layout"),Owner->GetPathName(),Name.ToString(),TEXT("native bool/int32/double"),P->GetClass()->GetName());}return true;
 }
 TSharedRef<FJsonObject> TextIdentity(const FText& Text)
 {auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("history_identity"),FString::Printf(TEXT("%p"),FTextInspector::GetSharedDataId(Text)));O->SetNumberField(TEXT("flags"),FTextInspector::GetFlags(Text));return O;}
 // TX extends this concrete source lease; it does not change the generation writer.
 bool SpawnerProfile=false,ResourceRetirement=false,GenerationAuthorized=false,InSnapshot=false;
 bool PointPipelineProfile=false;TArray<UObject*> PointPipelineNodes;TMap<FString,FString> PointFieldLayouts;
 struct FPointPath{FProperty* Field=nullptr;FFieldClass* Kind=nullptr;UStruct* Owner=nullptr;FName Root,Member;FString Parent;int32 Step=0;UScriptStruct* StructOwner=nullptr;};
 TMap<FString,FPointPath> PointPaths;int64 PointProjectionValueSlots=0,PointProjectionChargedValueSlots=0,PointRecheckChargedValueSlots=0;TArray<TStrongObjectPtr<const UStruct>> PointTypeHolds;TArray<TFunction<bool()>> PointSeals;

 UPCGNode* PointsNode=nullptr;UPCGNode* SpawnerNode=nullptr;
 struct FAssetRole{const UObject* Asset=nullptr;EObjectFlags Flags=RF_NoFlags,PackageFlags=RF_NoFlags;uint32 PackageBits=0;bool Dirty=false;};
 TMap<FString,FAssetRole> Assets;TArray<const UObject*> AssetFiles;
 bool Asset(const FString& Role,const UObject* Value,bool Mesh,const TSharedRef<FJsonObject>& Out,bool EditorSprite=false)
 {
  if(!Object(Role,Value))return false;
  if((EditorSprite?Value->GetClass()!=UTexture2D::StaticClass():(Mesh?Value->GetClass()!=UStaticMesh::StaticClass():(Value->GetClass()!=UMaterial::StaticClass()&&Value->GetClass()!=UMaterialInstanceConstant::StaticClass())))||!Value->IsAsset()||Value->GetOuter()!=Value->GetPackage()||Value->GetPackage()->IsDirty()||(!EditorSprite&&Value->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly))||Forbidden(Value->GetPathName())||(!Value->GetPathName().StartsWith(TEXT("/Game/"))&&!Value->GetPathName().StartsWith(TEXT("/Engine/"))))return Stop(TEXT("pcg_scoped_spawner_asset_pending"),Role,TEXT("native_saved_asset"),EditorSprite?TEXT("loaded saved-clean exact native Texture2D for editor visualization only; Game/Engine asset"):TEXT("loaded saved-clean native StaticMesh/Material/MaterialInstanceConstant Game/Engine asset"),Value->GetPathName());
  FAssetRole Now{Value,Value->GetFlags(),Value->GetPackage()->GetFlags(),uint32(Value->GetPackage()->GetPackageFlags()),Value->GetPackage()->IsDirty()};
  if(const auto* Old=Assets.Find(Role)){if(Old->Asset!=Value||Old->Flags!=Now.Flags||Old->PackageFlags!=Now.PackageFlags||Old->PackageBits!=Now.PackageBits||Old->Dirty!=Now.Dirty)return Stop(TEXT("pcg_scoped_spawner_asset_changed"),Role,TEXT("identity/flags/package_dirty"),TEXT("captured saved native source role"),TEXT("changed"));}
  else{if(Captured||Assets.Num()>=64)return Stop(TEXT("pcg_scoped_spawner_asset_pending"),Role,TEXT("asset_inventory"),TEXT("captured <=64 native source roles"),TEXT("new/larger inventory"));Assets.Add(Role,Now);AssetFiles.AddUnique(Value);}
  auto State=MakeShared<FJsonObject>();State->SetStringField(TEXT("identity"),ScopedObjectKey(Value));State->SetStringField(TEXT("flags"),LexToString(uint64(Now.Flags)));State->SetStringField(TEXT("package_flags"),LexToString(uint64(Now.PackageFlags)));State->SetNumberField(TEXT("package_bits"),Now.PackageBits);State->SetBoolField(TEXT("dirty"),Now.Dirty);Out->SetObjectField(Role,State);return true;
 }
 bool Spawner(const FString& Role,UPCGStaticMeshSpawnerSettings* S,const TSharedRef<FJsonObject>& Out)
 {
  if(!S->MeshSelectorType.GetGCPtr().IsResolved()||!S->MeshSelectorParameters.IsResolved()||!S->InstanceDataPackerType.GetGCPtr().IsResolved()||!S->InstanceDataPackerParameters.IsResolved())return Stop(TEXT("pcg_scoped_spawner_unresolved"),Role,TEXT("selector/packer"),TEXT("resolved native slots; no loader"),TEXT("unresolved"));
  UClass* SelectorClass=Resolved(S->MeshSelectorType.GetGCPtr());UPCGMeshSelectorBase* Raw=Resolved(S->MeshSelectorParameters);
  if(SelectorClass!=UPCGMeshSelectorWeighted::StaticClass()||!Object(Role+TEXT("_selector_class"),SelectorClass)||!Object(Role+TEXT("_selector"),Raw)||Raw->GetClass()!=SelectorClass||Raw->GetOuter()!=S||Resolved(S->InstanceDataPackerParameters)!=nullptr||!S->TargetActor.IsNull()||!S->PostProcessFunctionNames.IsEmpty()||!S->StaticMeshComponentPropertyOverrides.IsEmpty())return Stop(TEXT("pcg_scoped_spawner_pending"),Role,TEXT("native_weighted_no_callback"),TEXT("own exact Weighted selector, no packer instance/target override/property override/postprocess function"),TEXT("different role/configuration"));
  if(UClass* Packer=Resolved(S->InstanceDataPackerType.GetGCPtr())){if(!Object(Role+TEXT("_packer_type"),Packer))return false;if(!Packer->HasAnyClassFlags(CLASS_Native))return Stop(TEXT("pcg_scoped_spawner_pending"),Role,TEXT("packer_class"),TEXT("native optional packer class without instance"),TEXT("nonnative"));Out->SetStringField(TEXT("packer_type"),ScopedObjectKey(Packer));}else Out->SetStringField(TEXT("packer_type"),TEXT(""));
  auto* Weighted=static_cast<UPCGMeshSelectorWeighted*>(Raw);
  if(Weighted->bUseAttributeMaterialOverrides||!Weighted->MaterialOverrideAttributes.IsEmpty()||Weighted->MeshEntries.IsEmpty()||Weighted->MeshEntries.Num()>32)return Stop(TEXT("pcg_scoped_spawner_pending"),Role,TEXT("weighted_entries"),TEXT("1..32 entries without attribute material overrides"),FString::FromInt(Weighted->MeshEntries.Num()));
  if(!Scalars(S,{TEXT("bAllowDescriptorChanges"),TEXT("bApplyMeshBoundsToPoints"),TEXT("bSynchronousLoad"),TEXT("bAllowMergeDifferentDataInSameInstancedComponents"),TEXT("bSilenceOverrideAttributeNotFoundErrors"),TEXT("bWarnOnIdenticalSpawn"),TEXT("bSetupCullingCells"),TEXT("CullingCellSize")},Out))return false;
  Out->SetStringField(TEXT("out_attribute"),S->OutAttributeName.ToString());TArray<TSharedPtr<FJsonValue>> Entries;
  TArray<FPCGMeshSelectorWeightedEntry> OwnedEntries;if(BindingSourceOnly)OwnedEntries=Weighted->MeshEntries;const auto& ReadEntries=BindingSourceOnly?OwnedEntries:Weighted->MeshEntries;
  for(int32 I=0;I<ReadEntries.Num();++I)
  {
   const auto& Entry=ReadEntries[I];const auto& D=Entry.Descriptor;auto ClassSlotCopy=D.ComponentClass;const FString EntryRole=Role+FString::Printf(TEXT("_entry_%d"),I);
   if(Entry.Weight<=0||!ClassSlotCopy.GetGCPtr().IsResolved()||(Resolved(ClassSlotCopy.GetGCPtr())!=UInstancedStaticMeshComponent::StaticClass()&&Resolved(ClassSlotCopy.GetGCPtr())!=UHierarchicalInstancedStaticMeshComponent::StaticClass())||!D.RuntimeVirtualTextures.IsEmpty())return Stop(TEXT("pcg_scoped_spawner_descriptor_pending"),EntryRole,TEXT("weight/component_class/runtime_virtual_textures"),TEXT("positive weight and resolved exact native ISM/HISM, no runtime virtual textures"),TEXT("unsupported descriptor"));
   if(!Object(EntryRole+TEXT("_component_class"),Resolved(ClassSlotCopy.GetGCPtr())))return false;
   auto* PhysicalMaterialField=CastField<FObjectProperty>(Field(FBodyInstance::StaticStruct(),TEXT("PhysMaterialOverride")));if(!PhysicalMaterialField||!PhysicalMaterialField->PropertyClass.IsResolved())return Stop(TEXT("pcg_scoped_spawner_descriptor_pending"),EntryRole,TEXT("physical_material_field"),TEXT("resolved native object descriptor"),TEXT("other/unresolved"));if(!Object(EntryRole+TEXT("_physical_material_class"),Resolved(PhysicalMaterialField->PropertyClass)))return false;const auto* PhysicalMaterialHandle=PhysicalMaterialField->GetObjectPtrPropertyValuePtr(PhysicalMaterialField->ContainerPtrToValuePtr<void>(&D.BodyInstance));if(!PhysicalMaterialHandle->IsResolved()||Resolved(*PhysicalMaterialHandle)!=nullptr)return Stop(TEXT("pcg_scoped_spawner_descriptor_pending"),EntryRole,TEXT("physical_material_override"),TEXT("resolved null override; nonnull overrides remain incomplete"),TEXT("nonnull/unresolved"));auto J=MakeShared<FJsonObject>();
   const UStaticMesh* Mesh=ResolvedSoft(D.StaticMesh);if(!Asset(EntryRole+TEXT("_mesh"),Mesh,true,J))return false;
   if(D.OverrideMaterials.Num()>32)return Stop(TEXT("pcg_scoped_spawner_descriptor_pending"),EntryRole,TEXT("materials"),TEXT("<=32"),FString::FromInt(D.OverrideMaterials.Num()));
   for(int32 M=0;M<D.OverrideMaterials.Num();++M){const auto& RefMaterial=D.OverrideMaterials[M];if(!RefMaterial.IsNull()&&!Asset(EntryRole+FString::Printf(TEXT("_override_%d"),M),ResolvedSoft(RefMaterial),false,J))return false;}
   if(!D.OverlayMaterial.IsNull()&&!Asset(EntryRole+TEXT("_overlay"),ResolvedSoft(D.OverlayMaterial),false,J))return false;
   const auto& StaticMaterials=Mesh->GetStaticMaterials();if(StaticMaterials.Num()>32)return Stop(TEXT("pcg_scoped_spawner_descriptor_pending"),EntryRole,TEXT("mesh_materials"),TEXT("<=32"),FString::FromInt(StaticMaterials.Num()));TArray<TSharedPtr<FJsonValue>> MeshMaterials;
   for(int32 M=0;M<StaticMaterials.Num();++M){const auto& Material=StaticMaterials[M];if(!Material.MaterialInterface.IsResolved())return Stop(TEXT("pcg_scoped_spawner_unresolved"),EntryRole,TEXT("mesh_material"),TEXT("resolved nullable slot"),TEXT("unresolved"));const auto* MaterialObject=Resolved(Material.MaterialInterface);const FString MaterialRole=EntryRole+FString::Printf(TEXT("_mesh_material_%d"),M);if(MaterialObject&&!Asset(MaterialRole,MaterialObject,false,J))return false;auto MaterialRow=MakeShared<FJsonObject>();MaterialRow->SetStringField(TEXT("reference"),MaterialObject?ScopedObjectKey(MaterialObject):FString());MaterialRow->SetStringField(TEXT("slot"),Material.MaterialSlotName.ToString());MaterialRow->SetStringField(TEXT("imported_slot"),Material.ImportedMaterialSlotName.ToString());MeshMaterials.Add(MakeShared<FJsonValueObject>(MaterialRow));}J->SetArrayField(TEXT("mesh_materials"),MeshMaterials);
   // Exact native descriptor export is copied text; no source soft-reference load or InitComponent occurs here.
   const FPCGMeshSelectorWeightedEntry CopiedEntry=Entry;FString Descriptor;FPCGMeshSelectorWeightedEntry::StaticStruct()->ExportText(Descriptor,&CopiedEntry,nullptr,nullptr,PPF_None,nullptr);if(!W.Admit(TEXT("scoped_spawner_entry_export_return")))return false;J->SetStringField(TEXT("authored_entry"),Descriptor);Entries.Add(MakeShared<FJsonValueObject>(J));
  }Out->SetArrayField(TEXT("weighted_entries"),Entries);return true;
 }

 struct FManagedISMRole
 {
  TStrongObjectPtr<const UPCGManagedResource> Resource{nullptr};TStrongObjectPtr<UInstancedStaticMeshComponent> ISM{nullptr};
  TStrongObjectPtr<UClass> ResourceClass{nullptr},ISMClass{nullptr};TStrongObjectPtr<UObject> ResourceOuter{nullptr};
  TSharedPtr<FJsonObject> Copied;
 };
 TArray<FManagedISMRole> ManagedBefore;TSet<const UObject*> RetiredObjects;TArray<UActorComponent*> PermanentComponents;
 TSharedPtr<FJsonObject> ManagedBaseline;
 bool ManagedRows(TArray<TSharedPtr<FJsonValue>>& Rows,TArray<UActorComponent*>& Generated,bool Capture=false)
 {
  if(!Component->AreManagedResourcesAccessible())return Stop(TEXT("pcg_scoped_resources_inaccessible"),TEXT("component"),TEXT("managed_resources"),TEXT("accessible after completed generation"),TEXT("inaccessible"));
  TArray<TStrongObjectPtr<const UPCGManagedResource>> Fresh;
  // Only immediate strong capture inside the public SDK's resource lock. No nested resource SDK/Current call in the callback.
  Component->ForEachConstManagedResource([&](const UPCGManagedResource* R){Fresh.Emplace(R);});
  if(Fresh.Num()>128)return Stop(TEXT("pcg_scoped_resources_bound"),TEXT("component"),TEXT("managed_resources"),TEXT("<=128"),FString::FromInt(Fresh.Num()));
  TSet<const UObject*> SeenResources,SeenComponents;
  if(Capture)ManagedBefore.Empty();
  for(int32 I=0;I<Fresh.Num();++I)
  {
   const auto* Raw=Fresh[I].Get();
   if(RetiredObjects.Contains(Raw))return Stop(TEXT("pcg_scoped_retired_resource_still_managed"),TEXT("component"),TEXT("resource_membership"),TEXT("retired resource absent; no getter"),FString::Printf(TEXT("%p"),Raw));
   if(!Live(Raw)||Raw->GetClass()!=UPCGManagedISMComponent::StaticClass()||Raw->GetOuter()!=Component||SeenResources.Contains(Raw))return Stop(TEXT("pcg_scoped_resource_pending"),TEXT("component"),TEXT("resource_class/owner"),TEXT("unique live own exact native ManagedISM; other resource types incomplete"),TEXT("different/unavailable"));
   SeenResources.Add(Raw);const auto* Resource=static_cast<const UPCGManagedISMComponent*>(Raw);
   if(Resource->IsMarkedTransientOnLoad()||Resource->IsMarkedUnused())return Stop(TEXT("pcg_scoped_resource_pending"),TEXT("component"),TEXT("resource_state"),TEXT("active non-transient-on-load native resource"),TEXT("unused/transient"));
   // Soft Get is find-only. Keep its result before inspecting the concrete native component.
   UActorComponent* ComponentResult=Resource->GeneratedComponent.Get();TStrongObjectPtr<UActorComponent> Immediate(ComponentResult);
   if(RetiredObjects.Contains(ComponentResult))return Stop(TEXT("pcg_scoped_retired_ISM_still_managed"),TEXT("component"),TEXT("generated_component"),TEXT("retired component absent; no getter"),FString::Printf(TEXT("%p"),ComponentResult));
   if(!Live(ComponentResult)||(ComponentResult->GetClass()!=UInstancedStaticMeshComponent::StaticClass()&&ComponentResult->GetClass()!=UHierarchicalInstancedStaticMeshComponent::StaticClass())||ComponentResult->GetOwner()!=Actor||ComponentResult->GetOuter()!=Actor||ComponentResult->GetWorld()!=W.World.Get()||!ComponentResult->IsRegistered()||SeenComponents.Contains(ComponentResult))return Stop(TEXT("pcg_scoped_ISM_pending"),TEXT("component"),TEXT("ISM_class/owner/world/membership"),TEXT("unique own registered native ISM/HISM"),TEXT("different/unavailable"));
   SeenComponents.Add(ComponentResult);auto* ISM=static_cast<UInstancedStaticMeshComponent*>(ComponentResult);TArray<UActorComponent*> OwnerComponents;Actor->GetComponents(OwnerComponents);
   if(!OwnerComponents.Contains(ISM)||!Actor->GetInstanceComponents().Contains(ISM)||ISM->GetAttachParent()!=Actor->GetRootComponent()||ISM->NumCustomDataFloats!=0||ISM->GetInstanceCount()>4096)return Stop(TEXT("pcg_scoped_ISM_pending"),TEXT("component"),TEXT("ISM_owner_inventory/custom_data/count"),TEXT("own instance attached to root, custom-data0, <=4096 instances"),TEXT("different role/count"));
   const UStaticMesh* Mesh=ISM->GetStaticMesh();if(!Mesh||!AssetFiles.Contains(Mesh))return Stop(TEXT("pcg_scoped_ISM_mesh_changed"),TEXT("component"),TEXT("mesh"),TEXT("captured source mesh"),TEXT("foreign/null mesh"));
   const auto& Descriptor=Resource->GetDescriptor();auto ResourceClassSlotCopy=Descriptor.ComponentClass;if(!ResourceClassSlotCopy.GetGCPtr().IsResolved()||ResourceClassSlotCopy.GetGCPtr().Get()!=ISM->GetClass()||!Descriptor.StaticMesh.IsResolved()||Descriptor.StaticMesh.Get()!=Mesh||!Descriptor.RuntimeVirtualTextures.IsEmpty())return Stop(TEXT("pcg_scoped_resource_descriptor_changed"),TEXT("component"),TEXT("native_descriptor"),TEXT("resolved actual ISM/HISM class and captured mesh, no virtual textures"),TEXT("different role"));
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("resource_identity"),ScopedObjectKey(Raw));Row->SetStringField(TEXT("resource_path"),Raw->GetPathName());Row->SetStringField(TEXT("resource_class"),Raw->GetClass()->GetPathName());Row->SetStringField(TEXT("resource_outer"),ScopedObjectKey(Component));Row->SetStringField(TEXT("component_identity"),ScopedObjectKey(ISM));Row->SetStringField(TEXT("component_path"),ISM->GetPathName());Row->SetStringField(TEXT("component_class"),ISM->GetClass()->GetPathName());Row->SetStringField(TEXT("mesh"),ScopedObjectKey(Mesh));Row->SetBoolField(TEXT("registered"),ISM->IsRegistered());Row->SetNumberField(TEXT("creation_method"),int32(ISM->CreationMethod));Row->SetArrayField(TEXT("relative_location"),VectorJson(ISM->GetRelativeLocation()));Row->SetArrayField(TEXT("relative_scale"),VectorJson(ISM->GetRelativeScale3D()));const FQuat Rotation=ISM->GetRelativeRotation().Quaternion();Row->SetArrayField(TEXT("relative_rotation"),{MakeShared<FJsonValueNumber>(Rotation.X),MakeShared<FJsonValueNumber>(Rotation.Y),MakeShared<FJsonValueNumber>(Rotation.Z),MakeShared<FJsonValueNumber>(Rotation.W)});
   TArray<TSharedPtr<FJsonValue>> Tags,Materials,Transforms;for(FName Tag:ISM->ComponentTags)Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));Row->SetArrayField(TEXT("tags"),Tags);
   for(const auto& Material:ISM->OverrideMaterials){if(!Material.IsResolved())return Stop(TEXT("pcg_scoped_ISM_material_unresolved"),TEXT("component"),TEXT("override_material"),TEXT("resolved nullable captured material"),TEXT("unresolved"));const auto* M=Material.Get();if(M&&!AssetFiles.Contains(M))return Stop(TEXT("pcg_scoped_ISM_material_changed"),TEXT("component"),TEXT("override_material"),TEXT("captured native source material"),TEXT("foreign reference"));Materials.Add(MakeShared<FJsonValueString>(M?ScopedObjectKey(M):FString()));}Row->SetArrayField(TEXT("override_materials"),Materials);
   for(int32 Index=0;Index<ISM->GetInstanceCount();++Index){FTransform Pose;if(!ISM->GetInstanceTransform(Index,Pose,false)||!Pose.IsValid())return Stop(TEXT("pcg_scoped_ISM_transform_invalid"),TEXT("component"),TEXT("instance_transform"),TEXT("finite copied transform"),FString::FromInt(Index));auto T=MakeShared<FJsonObject>();T->SetArrayField(TEXT("location"),VectorJson(Pose.GetLocation()));T->SetArrayField(TEXT("scale"),VectorJson(Pose.GetScale3D()));const FQuat Q=Pose.GetRotation();T->SetArrayField(TEXT("rotation"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});Transforms.Add(MakeShared<FJsonValueObject>(T));}Row->SetArrayField(TEXT("instances"),Transforms);
   Rows.Add(MakeShared<FJsonValueObject>(Row));Generated.Add(ISM);
   if(Capture){FManagedISMRole Held;Held.Resource.Reset(Raw);Held.ISM.Reset(ISM);Held.ResourceClass.Reset(Raw->GetClass());Held.ISMClass.Reset(ISM->GetClass());Held.ResourceOuter.Reset(Raw->GetOuter());Held.Copied=Row;ManagedBefore.Add(MoveTemp(Held));}
  }
  Rows.Sort([](const auto& A,const auto& B){return Str(A->AsObject(),TEXT("resource_identity"))<Str(B->AsObject(),TEXT("resource_identity"));});return true;
 }
 bool SpawnerOwner(const TSharedRef<FJsonObject>& Authored)
 {
  TArray<TSharedPtr<FJsonValue>> Resources;TArray<UActorComponent*> Generated;
  const bool Busy=Component->IsGenerating()||Component->IsCleaningUp();
  if(!Busy){if(!ManagedRows(Resources,Generated))return false;if(ResourceRetirement&&!GenerationAuthorized&&!Resources.IsEmpty())return Stop(TEXT("pcg_scoped_hard_cleanup_resources_remain"),TEXT("component"),TEXT("resource_count"),TEXT("0 after hard cleanup"),FString::FromInt(Resources.Num()));}
  TArray<UActorComponent*> Actual;Actor->GetComponents(Actual);
  if(!Captured)
  {
   if(Actual.Num()>128)return Stop(TEXT("pcg_scoped_cleanup_components_bound"),TEXT("actor"),TEXT("component_count"),TEXT("<=128"),FString::FromInt(Actual.Num()));
   if(Expected)
   {
    const TArray<TSharedPtr<FJsonValue>>* ExpectedSurvivors=nullptr;const auto AuthoredExpected=Expected->GetObjectField(TEXT("source_authored"));
    if(!AuthoredExpected->TryGetArrayField(TEXT("surviving_owner_components"),ExpectedSurvivors)||!ExpectedSurvivors||ExpectedSurvivors->IsEmpty()||ExpectedSurvivors->Num()>128)return Stop(TEXT("pcg_scoped_expectation_invalid"),TEXT("source_authored"),TEXT("surviving_owner_components"),TEXT("copied finite nonempty original survivor roles"),TEXT("missing/invalid"));
    for(const auto& Value:*ExpectedSurvivors){if(!Value||Value->Type!=EJson::Object)return Stop(TEXT("pcg_scoped_expectation_invalid"),TEXT("source_authored"),TEXT("survivor"),TEXT("copied role object"),TEXT("invalid"));const FString Identity=Str(Value->AsObject(),TEXT("identity"));UActorComponent* Match=nullptr;for(auto* Other:Actual)if(Live(Other)&&ScopedObjectKey(Other)==Identity){if(Match)return Stop(TEXT("pcg_scoped_survivor_changed"),TEXT("actor"),TEXT("identity"),TEXT("unique copied survivor"),Identity);Match=Other;}if(!Match||PermanentComponents.Contains(Match))return Stop(TEXT("pcg_scoped_survivor_changed"),TEXT("actor"),TEXT("identity"),TEXT("same copied survivor membership"),Identity);PermanentComponents.Add(Match);}
   }
   else for(auto* Other:Actual)if(!Generated.Contains(Other))PermanentComponents.Add(Other);
  }
  for(const UObject* Retired:RetiredObjects)if(Actual.Contains(Retired))return Stop(TEXT("pcg_scoped_retired_ISM_still_owned"),TEXT("actor"),TEXT("component_membership"),TEXT("retired component absent; no getter"),FString::Printf(TEXT("%p"),Retired));
  for(auto* Old:PermanentComponents)if(!Actual.Contains(Old))return Stop(TEXT("pcg_scoped_survivor_removed"),TEXT("actor"),TEXT("component_membership"),TEXT("original nonselected component retained"),FString::Printf(TEXT("%p"),Old));
  for(auto* Extra:Actual)if(!PermanentComponents.Contains(Extra)&&!Generated.Contains(Extra))
  {
   // An accepted asynchronous generation can temporarily own an ISM before the resource is published.
   // Only that exact native provisional role is admitted; idle observations require the resource association.
   if(!GenerationAuthorized||!Busy||!Live(Extra)||(Extra->GetClass()!=UInstancedStaticMeshComponent::StaticClass()&&Extra->GetClass()!=UHierarchicalInstancedStaticMeshComponent::StaticClass())||Extra->GetOwner()!=Actor||Extra->GetOuter()!=Actor||Extra->GetWorld()!=W.World.Get())return Stop(TEXT("pcg_scoped_unselected_component_added"),TEXT("actor"),TEXT("component_membership"),TEXT("captured survivors plus actual selected managed native ISM/HISM"),TEXT("unexpected component"));
  }
  TArray<TSharedPtr<FJsonValue>> Rows;
  for(int32 I=0;I<PermanentComponents.Num();++I)
  {
   auto* Other=PermanentComponents[I];const UClass* SurvivorClass=Live(Other)?Other->GetClass():nullptr;
   if(!Live(SurvivorClass)||(SurvivorClass!=UPCGComponent::StaticClass()&&SurvivorClass!=USceneComponent::StaticClass()&&SurvivorClass!=UStaticMeshComponent::StaticClass()&&SurvivorClass!=UBoxComponent::StaticClass()&&SurvivorClass!=UBillboardComponent::StaticClass()))return Stop(TEXT("pcg_scoped_survivor_class_pending"),Live(Other)?Other->GetPathName():TEXT("actor"),TEXT("nonselected_component_class"),TEXT("exact native PCG/Scene/StaticMesh/Box and stock editor-visualization Billboard components; custom component observers incomplete"),Live(SurvivorClass)?SurvivorClass->GetPathName():TEXT("unavailable component/class"));const FString Role=FString::Printf(TEXT("spawner_survivor_%d"),I);if(!Object(Role,Other))return false;if(Other->GetOwner()!=Actor||Other->GetWorld()!=W.World.Get())return Stop(TEXT("pcg_scoped_survivor_changed"),Role,TEXT("owner/world"),TEXT("same actor/world"),TEXT("different"));
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("identity"),ScopedObjectKey(Other));Row->SetNumberField(TEXT("creation_method"),int32(Other->CreationMethod));Row->SetBoolField(TEXT("instance_component_member"),Actor->GetInstanceComponents().Contains(Other));if(Other!=Component){Row->SetBoolField(TEXT("registered"),Other->IsRegistered());Row->SetBoolField(TEXT("active"),Other->IsActive());TArray<TSharedPtr<FJsonValue>> Tags;for(FName Tag:Other->ComponentTags)Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));Row->SetArrayField(TEXT("tags"),Tags);if(const auto* Scene=Cast<USceneComponent>(Other)){Row->SetStringField(TEXT("parent"),Scene->GetAttachParent()?ScopedObjectKey(Scene->GetAttachParent()):FString());const auto Pose=Scene->GetRelativeTransform();Row->SetArrayField(TEXT("location"),VectorJson(Pose.GetLocation()));Row->SetArrayField(TEXT("scale"),VectorJson(Pose.GetScale3D()));const auto Q=Pose.GetRotation();Row->SetArrayField(TEXT("rotation"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});}}if(Other->GetClass()==UStaticMeshComponent::StaticClass())
   {
    auto* SM=static_cast<UStaticMeshComponent*>(Other);if(SM->IsSimulatingPhysics())return Stop(TEXT("pcg_scoped_survivor_physics_pending"),Role,TEXT("physics"),TEXT("non-simulating native survivor"),TEXT("simulating"));UObject* RootMesh=Ref(SM,TEXT("StaticMesh"));if(!W.R.Error.IsEmpty())return false;if(RootMesh&&!Asset(Role+TEXT("_mesh"),RootMesh,true,Row))return false;Row->SetStringField(TEXT("static_mesh_identity"),RootMesh?ScopedObjectKey(RootMesh):FString());TArray<UObject*> Materials;if(!Refs(SM,TEXT("OverrideMaterials"),Materials))return false;TArray<TSharedPtr<FJsonValue>> MaterialRows;for(int32 M=0;M<Materials.Num();++M){if(Materials[M]&&!Asset(Role+FString::Printf(TEXT("_material_%d"),M),Materials[M],false,Row))return false;MaterialRows.Add(MakeShared<FJsonValueString>(Materials[M]?ScopedObjectKey(Materials[M]):FString()));}Row->SetArrayField(TEXT("override_materials"),MaterialRows);Row->SetNumberField(TEXT("collision_enabled"),int32(SM->GetCollisionEnabled()));
   }
   if(SurvivorClass==UBoxComponent::StaticClass())Row->SetArrayField(TEXT("box_extent"),VectorJson(static_cast<const UBoxComponent*>(Other)->GetUnscaledBoxExtent()));
   if(SurvivorClass==UBoxComponent::StaticClass()||SurvivorClass==UBillboardComponent::StaticClass())
   {
    // Same public primitive projection for admitted native shape/visualization survivors; no physics cache identity.
    const auto* Primitive=static_cast<const UPrimitiveComponent*>(Other);
    if(Primitive->IsSimulatingPhysics())return Stop(TEXT("pcg_scoped_survivor_physics_pending"),Role,TEXT("physics"),TEXT("non-simulating native survivor"),TEXT("simulating"));
    Row->SetNumberField(TEXT("mobility"),int32(Primitive->Mobility));
    Row->SetBoolField(TEXT("generate_overlap_events"),Primitive->GetGenerateOverlapEvents());
    Row->SetBoolField(TEXT("actor_collision_enabled"),Actor->GetActorEnableCollision());
    Row->SetNumberField(TEXT("collision_enabled_authored"),int32(Primitive->BodyInstance.GetCollisionEnabled(false)));
    Row->SetNumberField(TEXT("collision_enabled_effective"),int32(Primitive->GetCollisionEnabled()));
    Row->SetStringField(TEXT("collision_profile"),Primitive->GetCollisionProfileName().ToString());
    Row->SetNumberField(TEXT("collision_object_type"),int32(Primitive->GetCollisionObjectType()));
    const FCollisionResponseContainer& Responses=Primitive->GetCollisionResponseToChannels();TArray<TSharedPtr<FJsonValue>> Channels;
    for(int32 ChannelIndex=0;ChannelIndex<int32(UE_ARRAY_COUNT(Responses.EnumArray));++ChannelIndex)Channels.Add(MakeShared<FJsonValueNumber>(int32(Responses.GetResponse(static_cast<ECollisionChannel>(ChannelIndex)))));
    Row->SetArrayField(TEXT("collision_response_channels"),Channels);
   }
   if(SurvivorClass==UBillboardComponent::StaticClass())
   {
    auto* Visual=static_cast<UBillboardComponent*>(Other);USceneComponent* VisualParent=Visual->GetAttachParent();
    if(!Visual->IsVisualizationComponent()||!Visual->IsEditorOnly()||!Visual->HasAllFlags(RF_Transient|RF_TextExportTransient)||Visual->GetOuter()!=Actor||!Live(VisualParent)||!PermanentComponents.Contains(VisualParent))return Stop(TEXT("pcg_scoped_visualization_role_pending"),Role,TEXT("editor_visualization/parent"),TEXT("own transient editor visualization with captured nonselected Scene parent"),TEXT("different role/parent"));
    const UClass* ParentClass=VisualParent->GetClass();
    if((ParentClass!=USceneComponent::StaticClass()&&ParentClass!=UBoxComponent::StaticClass()&&ParentClass!=UStaticMeshComponent::StaticClass())||!Object(Role+TEXT("_visual_parent"),VisualParent)||VisualParent->GetOwner()!=Actor||VisualParent->GetWorld()!=W.World.Get()||!VisualParent->bVisualizeComponent||Visual->CreationMethod!=VisualParent->CreationMethod||!Visual->GetAttachChildren().IsEmpty())return Stop(TEXT("pcg_scoped_visualization_role_pending"),Role,TEXT("visualizing_parent"),TEXT("captured own native visualizing parent, same creation method, leaf visualization"),TEXT("different role"));
    int32 Reciprocal=0;for(const auto& Child:VisualParent->GetAttachChildren()){if(!Child.IsResolved())return Stop(TEXT("pcg_scoped_visualization_slot_unresolved"),Role,TEXT("parent_children"),TEXT("resolved attachment membership; no load"),TEXT("unresolved"));if(Child.Get()==Visual)++Reciprocal;}
    if(Reciprocal!=1)return Stop(TEXT("pcg_scoped_visualization_attachment_changed"),Role,TEXT("parent_children"),TEXT("exactly one reciprocal visualization child"),FString::FromInt(Reciprocal));
    if(!Visual->Sprite.IsResolved())return Stop(TEXT("pcg_scoped_visualization_sprite_unresolved"),Role,TEXT("sprite"),TEXT("resolved nullable sprite; no load"),TEXT("unresolved"));
    UTexture2D* Texture=Visual->Sprite.Get();TStrongObjectPtr<UTexture2D> ReturnedTexture(Texture);
    if(Texture&&!Asset(Role+TEXT("_editor_sprite"),Texture,false,Row,true))return false;
    auto Billboard=MakeShared<FJsonObject>();Billboard->SetStringField(TEXT("sprite"),Texture?ScopedObjectKey(Texture):FString());Billboard->SetStringField(TEXT("parent"),ScopedObjectKey(VisualParent));
    Billboard->SetBoolField(TEXT("private_sprite_slot_read"),false);Billboard->SetBoolField(TEXT("editor_only"),Visual->IsEditorOnly());Billboard->SetBoolField(TEXT("visualization"),Visual->IsVisualizationComponent());Billboard->SetStringField(TEXT("object_flags"),LexToString(uint64(Visual->GetFlags())));
    Billboard->SetBoolField(TEXT("absolute_location"),Visual->IsUsingAbsoluteLocation());Billboard->SetBoolField(TEXT("absolute_rotation"),Visual->IsUsingAbsoluteRotation());Billboard->SetBoolField(TEXT("absolute_scale"),Visual->IsUsingAbsoluteScale());Billboard->SetStringField(TEXT("socket"),Visual->GetAttachSocketName().ToString());
    Billboard->SetBoolField(TEXT("visible"),Visual->IsVisible());Billboard->SetBoolField(TEXT("hidden_in_game"),Visual->bHiddenInGame);Billboard->SetBoolField(TEXT("screen_size_scaled"),Visual->bIsScreenSizeScaled);Billboard->SetNumberField(TEXT("screen_size"),Visual->ScreenSize);
    Billboard->SetNumberField(TEXT("U"),Visual->U);Billboard->SetNumberField(TEXT("UL"),Visual->UL);Billboard->SetNumberField(TEXT("V"),Visual->V);Billboard->SetNumberField(TEXT("VL"),Visual->VL);Billboard->SetNumberField(TEXT("opacity_mask"),Visual->OpacityMaskRefVal);
    Billboard->SetBoolField(TEXT("in_editor_scaling"),Visual->bUseInEditorScaling);Billboard->SetBoolField(TEXT("show_locked_location"),Visual->bShowLockedLocation);Billboard->SetStringField(TEXT("category"),Visual->SpriteInfo.Category.ToString());Billboard->SetObjectField(TEXT("category_display_name"),TextIdentity(Visual->SpriteInfo.DisplayName));Billboard->SetObjectField(TEXT("category_description"),TextIdentity(Visual->SpriteInfo.Description));
    Billboard->SetNumberField(TEXT("actor_sprite_scale"),Actor->SpriteScale);Authored->SetBoolField(TEXT("editor_visualization_private_slot_read"),false);Row->SetObjectField(TEXT("billboard"),Billboard);
   }Rows.Add(MakeShared<FJsonValueObject>(Row));
  }Authored->SetArrayField(TEXT("surviving_owner_components"),Rows);TArray<TSharedPtr<FJsonValue>> ActorTags;for(FName Tag:Actor->Tags)ActorTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));Authored->SetArrayField(TEXT("actor_tags"),ActorTags);
  if(CleanupMode&&!ResourceRetirement&&!Busy&&ManagedBaseline){auto Now=MakeShared<FJsonObject>();Now->SetArrayField(TEXT("resources"),Resources);FString Path,Want,Got;if(ScopedDifference(MakeShared<FJsonValueObject>(ManagedBaseline),MakeShared<FJsonValueObject>(Now),Path,Want,Got,true))return Stop(TEXT("pcg_scoped_managed_resource_changed"),TEXT("component"),Path,Want,Got);}
  return true;
 }
 bool RetireManaged()
 {
  // The caller has just completed W.SDK's final Current. No source/retired getter after this point.
  ResourceRetirement=true;TArray<TSharedPtr<FJsonValue>> Rows;
  for(const auto& Held:ManagedBefore){RetiredObjects.Add(Held.Resource.Get());RetiredObjects.Add(Held.ISM.Get());Rows.Add(MakeShared<FJsonValueObject>(Held.Copied));}
  W.R.Data->SetArrayField(TEXT("selected_managed_resources_before_retirement"),Rows);W.R.Data->SetBoolField(TEXT("selected_managed_retirement_registered"),true);return true;
 }

 bool Settings(const FString& Role,UPCGSettings* SettingsObject,const TSharedRef<FJsonObject>& Out)
 {
  if(!Object(Role,SettingsObject))return false;
  if(!Scalars(SettingsObject,{TEXT("bEnabled"),TEXT("bDebug"),TEXT("bBreakDebugger"),TEXT("Seed"),TEXT("bExecutionDependencyRequired"),TEXT("bExposeToLibrary"),TEXT("bExecuteOnGPU"),TEXT("bDumpCookedHLSL"),TEXT("bDumpDataDescriptions"),TEXT("bPrintShaderDebugValues"),TEXT("DebugBufferSize"),TEXT("bTriggerRenderCapture"),TEXT("ProfileKernelIndex")},Out))return false;
  if(!Condition(!SettingsObject->bDebug&&!Out->GetBoolField(TEXT("bExecuteOnGPU")),Role,TEXT("execution_profile"),TEXT("CPU with debug visualization disabled; other variants pending"),TEXT("debug/GPU authored state")))return false;
  Out->SetBoolField(TEXT("marked_manual_editing"),SettingsObject->IsMarkedForManualEditing());Out->SetObjectField(TEXT("category"),TextIdentity(SettingsObject->Category));Out->SetObjectField(TEXT("description"),TextIdentity(SettingsObject->Description));
  const auto& D=SettingsObject->DebugSettings;auto Debug=MakeShared<FJsonObject>();Debug->SetNumberField(TEXT("point_scale"),D.PointScale);Debug->SetNumberField(TEXT("scale_method"),int32(D.ScaleMethod));Debug->SetStringField(TEXT("mesh"),D.PointMesh.ToSoftObjectPath().ToString());Debug->SetStringField(TEXT("material"),D.MaterialOverride.ToSoftObjectPath().ToString());Debug->SetBoolField(TEXT("display_properties"),D.bDisplayProperties);Out->SetObjectField(TEXT("debug_settings"),Debug);
  auto& T=SettingsObject->DeterminismSettings;auto Determinism=MakeShared<FJsonObject>();Determinism->SetBoolField(TEXT("native_tests"),T.bNativeTests);Determinism->SetBoolField(TEXT("blueprint_test_enabled"),T.bUseBlueprintDeterminismTest);const auto& Test=T.DeterminismTestBlueprint.GetGCPtr();if(!Test.IsResolved())return Stop(TEXT("pcg_scoped_unresolved_binding"),Role,TEXT("DeterminismTestBlueprint"),TEXT("resolved optional class handle"),TEXT("unresolved"));const UClass* TestClass=BindingSourceOnly?Resolved(Test):Test.Get();if(TestClass&&!Object(Role+TEXT("_determinism_class"),TestClass))return false;Determinism->SetStringField(TEXT("test_class"),TestClass?ScopedObjectKey(TestClass):FString());Out->SetObjectField(TEXT("determinism"),Determinism);return true;
 }
 // RK is one extra host role in the existing CPU source lease, not a second generation writer.
 bool NativeClass(const FString& Role,UClass* K,const TSharedRef<FJsonObject>& Out)
 {
  if(!Object(Role,K)||K->GetClass()!=UClass::StaticClass()||!K->HasAnyClassFlags(CLASS_Native)||K->HasAnyClassFlags(CLASS_CompiledFromBlueprint|CLASS_NewerVersionExists|CLASS_Deprecated)||!K->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=nullptr||!K->ClassDefaultObject.IsResolved())return Stop(TEXT("pcg_scoped_native_host_class_pending"),Role,TEXT("native_class/source/CDO_slot"),TEXT("resolved exact native compiled-in class and nullable existing CDO"),TEXT("unavailable or different role"),false);
  UObject* D=K->ClassDefaultObject.Get();if(D&&(!Object(Role+TEXT("_cdo"),D)||D->GetClass()!=K||D->GetOuter()!=K->GetOuter()||!D->HasAnyFlags(RF_ClassDefaultObject)))return Stop(TEXT("pcg_scoped_native_host_cdo_changed"),Role,TEXT("CDO"),TEXT("nullable held exact native default role"),TEXT("different default/class/outer"),false);
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("class"),ScopedObjectKey(K));J->SetStringField(TEXT("class_flags"),LexToString(uint64(K->GetClassFlags())));J->SetStringField(TEXT("cdo"),D?ScopedObjectKey(D):FString());J->SetStringField(TEXT("cdo_flags"),D?LexToString(uint64(D->GetFlags())):FString());Out->SetObjectField(Role,J);return true;
 }
 // AEE: the stock PCG volume is a native actor with native PCG/brush default subobjects.
 // Preserve authored brush vertices/relationships. BodySetup, BSP and physics caches
 // may be rebuilt by the engine and are not immutable source identities here.
 bool NativeVolumeSnapshot(const TSharedRef<FJsonObject>& Authored)
 {
  auto Host=MakeShared<FJsonObject>();Host->SetStringField(TEXT("kind"),TEXT("native_pcg_volume"));
  if(SourceBlueprint||Actor->GetClass()!=APCGVolume::StaticClass()||Component->GetClass()!=UPCGComponent::StaticClass()||Component->GetOuter()!=Actor||Component->CreationMethod!=EComponentCreationMethod::Native||Actor->bIsEditorPreviewActor||Ref(Actor,TEXT("PCGComponent"))!=Component)return Stop(TEXT("pcg_scoped_volume_host_pending"),TEXT("native_volume"),TEXT("native_PCGraph_owner"),TEXT("exact stock PCGVolume with its own native PCG defaultsubobject"),TEXT("other owner/class/role"),false);
  if(!NativeClass(TEXT("volume_class"),APCGVolume::StaticClass(),Host)||!NativeClass(TEXT("volume_root_class"),UBrushComponent::StaticClass(),Host)||!NativeClass(TEXT("volume_pcg_class"),UPCGComponent::StaticClass(),Host)||!NativeClass(TEXT("volume_instance_class"),UPCGGraphInstance::StaticClass(),Host))return false;
  UObject* Level=Actor->GetLevel();if(!Object(TEXT("volume_level"),Level)||Ref(Level,TEXT("OwningWorld"))!=W.World.Get())return Stop(TEXT("pcg_scoped_volume_level_changed"),TEXT("native_volume"),TEXT("level/world"),TEXT("current held Editor-world level"),TEXT("changed"),false);
  UObject* RawRoot=Ref(Actor,TEXT("RootComponent"));if(!W.R.Error.IsEmpty()||!Object(TEXT("volume_root"),RawRoot))return false;
  auto* Root=Cast<UBrushComponent>(RawRoot);
  if(!Root||Root->GetClass()!=UBrushComponent::StaticClass()||Root->GetOwner()!=Actor||Root->GetOuter()!=Actor||Root->GetWorld()!=W.World.Get()||!Root->IsRegistered()||Root->CreationMethod!=EComponentCreationMethod::Native||Ref(Actor,TEXT("BrushComponent"))!=Root||Root->Mobility!=EComponentMobility::Static||Root->IsSimulatingPhysics())return Stop(TEXT("pcg_scoped_volume_root_changed"),TEXT("volume_root"),TEXT("class/owner/world/native/registration"),TEXT("same own static nonsimulating registered native BrushComponent root"),TEXT("changed"),false);
  UObject* Parent=Ref(Root,TEXT("AttachParent"));TArray<UObject*> Children;if(!W.R.Error.IsEmpty()||!Refs(Root,TEXT("AttachChildren"),Children))return false;
  if(Parent||!Children.IsEmpty())return Stop(TEXT("pcg_scoped_volume_attachments_pending"),TEXT("volume_root"),TEXT("attachments"),TEXT("unattached native brush root without children"),TEXT("other attachment"),false);
  TArray<UActorComponent*> Components;Actor->GetComponents(Components);
  if(Components.Num()!=2||!Components.Contains(Root)||!Components.Contains(Component)||!Actor->GetInstanceComponents().IsEmpty())return Stop(TEXT("pcg_scoped_volume_membership_changed"),TEXT("native_volume"),TEXT("owned/instance components"),TEXT("only native brush root + selected native PCG, no instance components"),FString::Printf(TEXT("components=%d instances=%d"),Components.Num(),Actor->GetInstanceComponents().Num()));
  UObject* RawModel=Ref(Actor,TEXT("Brush"));if(!W.R.Error.IsEmpty()||!Object(TEXT("volume_model"),RawModel))return false;
  auto* Model=Cast<UModel>(RawModel);
  if(!Model||Model->GetClass()!=UModel::StaticClass()||Model->GetOuter()!=Actor||Model->GetPackage()!=Actor->GetPackage()||Ref(Root,TEXT("Brush"))!=Model)return Stop(TEXT("pcg_scoped_volume_model_changed"),TEXT("volume_model"),TEXT("class/owner/brush references"),TEXT("same actor-owned native model, matching root brush reference"),TEXT("changed"),false);
  UObject* RawPolys=Ref(Model,TEXT("Polys"));if(!W.R.Error.IsEmpty()||!Object(TEXT("volume_polys"),RawPolys))return false;
  auto* Polys=Cast<UPolys>(RawPolys);
  if(!Polys||Polys->GetClass()!=UPolys::StaticClass()||Polys->GetOuter()!=Model||Polys->GetPackage()!=Actor->GetPackage()||Polys->Element.IsEmpty()||Polys->Element.Num()>128)return Stop(TEXT("pcg_scoped_volume_polys_pending"),TEXT("volume_polys"),TEXT("owner/authored polygons"),TEXT("same model-owned native Polys with 1..128 polygons"),TEXT("other/empty/large profile"),false);
  // Copy before any further object admission; never retain a borrowed FPoly or array cell.
  const TArray<FPoly> Copied=Polys->Element;FBox LocalBounds(ForceInit);int32 VertexCount=0;TArray<TSharedPtr<FJsonValue>> Rows;
  for(int32 I=0;I<Copied.Num();++I)
  {
   const auto& Poly=Copied[I];if(Poly.Vertices.Num()<3||Poly.Vertices.Num()>128||VertexCount>4096-Poly.Vertices.Num())return Stop(TEXT("pcg_scoped_volume_vertices_pending"),TEXT("volume_polys"),TEXT("vertex count"),TEXT("3..128 per polygon, <=4096 total"),TEXT("other/large profile"),false);
   VertexCount+=Poly.Vertices.Num();auto Row=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Vertices;
   for(const auto& P:Poly.Vertices){const FVector V(P);if(!FMath::IsFinite(V.X)||!FMath::IsFinite(V.Y)||!FMath::IsFinite(V.Z))return Stop(TEXT("pcg_scoped_volume_nonfinite_vertex"),TEXT("volume_polys"),TEXT("vertex"),TEXT("finite authored XYZ"),TEXT("nonfinite"));LocalBounds+=V;Vertices.Add(MakeShared<FJsonValueArray>(VectorJson(V)));}
   Row->SetArrayField(TEXT("vertices"),Vertices);Row->SetStringField(TEXT("item"),Poly.ItemName.ToString());
   if(!Poly.Material.IsResolved())return Stop(TEXT("pcg_scoped_volume_material_unresolved"),TEXT("volume_polys"),TEXT("material"),TEXT("resolved nullable surface material"),TEXT("unresolved"));
   // Observe the already-resolved resident without tracked Get(), then retain it
   // before Object/W.Pin may admit callback-bearing current checks.
   const auto MaterialHandle=Poly.Material;UMaterialInterface* Material=nullptr;TStrongObjectPtr<UObject> MaterialHold(nullptr);
   if(MaterialHandle!=nullptr)
   {
    UObject* Resident=StaticFindObject(UMaterialInterface::StaticClass(),nullptr,*MaterialHandle.GetPathName());MaterialHold.Reset(Live(Resident)?Resident:nullptr);
    if(!Live(Resident)||MaterialHandle!=Resident)return Stop(TEXT("pcg_scoped_volume_material_changed"),TEXT("volume_polys"),TEXT("resident material"),TEXT("same already-resolved held native material object"),TEXT("unavailable/replaced"));
    Material=static_cast<UMaterialInterface*>(Resident);
   }
   if(Material&&!Object(FString::Printf(TEXT("volume_poly_material_%d"),I),Material))return false;Row->SetStringField(TEXT("material"),Material?ScopedObjectKey(Material):FString());Rows.Add(MakeShared<FJsonValueObject>(Row));
  }
  // One pure closure after all material admissions; do not adopt a changed
  // original geometry snapshot or compare temporary physics/BSP caches.
  if(Polys->Element.Num()!=Copied.Num())return Stop(TEXT("pcg_scoped_volume_geometry_changed"),TEXT("volume_polys"),TEXT("polygon count"),LexToString(Copied.Num()),LexToString(Polys->Element.Num()));
  for(int32 I=0;I<Copied.Num();++I)
  {
   const auto& Now=Polys->Element[I];const auto& Was=Copied[I];
   if(Now.Vertices!=Was.Vertices||Now.ItemName!=Was.ItemName||!Now.Material.IsResolved()||Now.Material.GetHandle()!=Was.Material.GetHandle())return Stop(TEXT("pcg_scoped_volume_geometry_changed"),TEXT("volume_polys"),FString::Printf(TEXT("polygon[%d].vertices/item/material"),I),TEXT("original copied authored values and shallow material handle"),TEXT("changed after object admission"));
  }
  const FBox Bounds=LocalBounds.TransformBy(Root->GetComponentTransform());
  if(!LocalBounds.IsValid||!Bounds.IsValid||!FMath::IsFinite(Bounds.Min.X)||!FMath::IsFinite(Bounds.Min.Y)||!FMath::IsFinite(Bounds.Min.Z)||!FMath::IsFinite(Bounds.Max.X)||!FMath::IsFinite(Bounds.Max.Y)||!FMath::IsFinite(Bounds.Max.Z)||Bounds.Min.X>=Bounds.Max.X||Bounds.Min.Y>=Bounds.Max.Y||Bounds.Min.Z>=Bounds.Max.Z)return Stop(TEXT("pcg_scoped_volume_bounds_pending"),TEXT("volume_root"),TEXT("authored bounds"),TEXT("finite positive brush volume from authored vertices and current root transform"),TEXT("empty/degenerate/nonfinite"),false);
  Host->SetArrayField(TEXT("polygons"),Rows);Host->SetArrayField(TEXT("bounds_min"),VectorJson(Bounds.Min));Host->SetArrayField(TEXT("bounds_max"),VectorJson(Bounds.Max));Host->SetStringField(TEXT("model"),ScopedObjectKey(Model));Host->SetStringField(TEXT("polys"),ScopedObjectKey(Polys));Host->SetStringField(TEXT("level"),ScopedObjectKey(Level));Host->SetStringField(TEXT("actor_label"),Actor->GetActorLabel(false));Host->SetNumberField(TEXT("collision_enabled"),int32(Root->GetCollisionEnabled()));Host->SetNumberField(TEXT("collision_object_type"),int32(Root->GetCollisionObjectType()));
  TArray<TSharedPtr<FJsonValue>> Tags;for(FName Tag:Actor->Tags)Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));Host->SetArrayField(TEXT("actor_tags"),Tags);Tags.Reset();for(FName Tag:Component->ComponentTags)Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));Host->SetArrayField(TEXT("component_tags"),Tags);
  Authored->SetObjectField(TEXT("native_volume_host"),Host);return W.R.Error.IsEmpty();
 }
 bool NativeHostSnapshot(const TSharedRef<FJsonObject>& Authored)
 {
  if(NativeVolumeHost)return NativeVolumeSnapshot(Authored);
  auto Host=MakeShared<FJsonObject>();Host->SetStringField(TEXT("kind"),TEXT("native_staticmesh_actor_instance_pcg"));
  if(SourceBlueprint||Actor->GetClass()!=AStaticMeshActor::StaticClass()||Component->GetClass()!=UPCGComponent::StaticClass()||Component->GetOuter()!=Actor||Component->CreationMethod!=EComponentCreationMethod::Instance||Actor->bIsEditorPreviewActor)return Stop(TEXT("pcg_scoped_native_host_profile_pending"),TEXT("native_host"),TEXT("class/owner/instance/mode"),TEXT("existing exact native StaticMeshActor with one own instance PCGComponent; bounded CPU generation/empty-resource cleanup/query"),TEXT("different host/mode"),false);
  if(!NativeClass(TEXT("native_host_class"),AStaticMeshActor::StaticClass(),Host)||!NativeClass(TEXT("native_root_class"),UStaticMeshComponent::StaticClass(),Host)||!NativeClass(TEXT("native_pcg_class"),UPCGComponent::StaticClass(),Host)||!NativeClass(TEXT("native_instance_class"),UPCGGraphInstance::StaticClass(),Host))return false;
  UObject* Level=Actor->GetLevel();if(!Object(TEXT("native_host_level"),Level)||Ref(Level,TEXT("OwningWorld"))!=W.World.Get())return Stop(TEXT("pcg_scoped_native_level_changed"),TEXT("native_host"),TEXT("level/world"),TEXT("held current Editor-world level"),TEXT("different or unavailable level"),false);
  UObject* RawRoot=Ref(Actor,TEXT("RootComponent"));if(!W.R.Error.IsEmpty()||!Object(TEXT("native_host_root"),RawRoot))return false;
  auto* Root=Cast<UStaticMeshComponent>(RawRoot);if(!Root||Root->GetClass()!=UStaticMeshComponent::StaticClass()||Root->GetOwner()!=Actor||Root->GetOuter()!=Actor||Root->GetWorld()!=W.World.Get()||!Root->IsRegistered()||Root->CreationMethod!=EComponentCreationMethod::Native||Ref(Actor,TEXT("StaticMeshComponent"))!=Root)return Stop(TEXT("pcg_scoped_native_root_changed"),TEXT("native_host_root"),TEXT("class/owner/world/registered/role"),TEXT("own exact registered native StaticMeshComponent and root slot"),TEXT("different role"),false);
  UObject* Parent=Ref(Root,TEXT("AttachParent"));TArray<UObject*> Children;if(!W.R.Error.IsEmpty()||!Refs(Root,TEXT("AttachChildren"),Children))return false;
  if(Parent||!Children.IsEmpty())return Stop(TEXT("pcg_scoped_native_attachment_pending"),TEXT("native_host_root"),TEXT("attachment"),TEXT("unattached root without attached children; wider original hosts incomplete"),TEXT("attached parent/children"),false);
  TArray<UActorComponent*> Components;Actor->GetComponents(Components);const auto& Instances=Actor->GetInstanceComponents();
  if(Components.Num()!=2||!Components.Contains(Root)||!Components.Contains(Component)||Instances.Num()!=1||Instances[0]!=Component)return Stop(TEXT("pcg_scoped_native_membership_changed"),TEXT("native_host"),TEXT("owned_components/instance_components"),TEXT("root + selected PCG, with only selected PCG in instance array"),FString::Printf(TEXT("components=%d instances=%d"),Components.Num(),Instances.Num()));
  UObject* RawMesh=Ref(Root,TEXT("StaticMesh"));if(!W.R.Error.IsEmpty()||!Object(TEXT("native_host_mesh"),RawMesh))return false;
  auto* Mesh=Cast<UStaticMesh>(RawMesh);if(!Mesh||Mesh->GetClass()!=UStaticMesh::StaticClass()||Mesh->GetPackage()->IsDirty()||Forbidden(Mesh->GetPathName())||Mesh->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly)||Mesh->GetOuter()!=Mesh->GetPackage())return Stop(TEXT("pcg_scoped_native_mesh_pending"),TEXT("native_host_mesh"),TEXT("mesh_source"),TEXT("loaded exact top-level saved-clean runtime StaticMesh"),TEXT("different/null/dirty source"),false);
  if(NativeMesh&&NativeMesh!=Mesh)return Stop(TEXT("pcg_scoped_native_mesh_changed"),TEXT("native_host_mesh"),TEXT("identity"),ScopedObjectKey(NativeMesh),ScopedObjectKey(Mesh));NativeMesh=Mesh;
  TArray<UObject*> Materials;if(!Refs(Root,TEXT("OverrideMaterials"),Materials))return false;TArray<TSharedPtr<FJsonValue>> MaterialRows;
  for(int32 I=0;I<Materials.Num();++I){const auto* M=Materials[I];if(M&&!Object(FString::Printf(TEXT("native_host_material_%d"),I),M))return false;MaterialRows.Add(MakeShared<FJsonValueString>(M?ScopedObjectKey(M):FString()));}
  Host->SetArrayField(TEXT("override_materials"),MaterialRows);Host->SetStringField(TEXT("mesh"),ScopedObjectKey(Mesh));Host->SetStringField(TEXT("level"),ScopedObjectKey(Actor->GetLevel()));Host->SetStringField(TEXT("actor_label"),Actor->GetActorLabel(false));Host->SetNumberField(TEXT("mobility"),int32(Root->Mobility));Host->SetNumberField(TEXT("collision_enabled"),int32(Root->GetCollisionEnabled()));
  TArray<TSharedPtr<FJsonValue>> Tags;for(FName Tag:Actor->Tags)Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));Host->SetArrayField(TEXT("actor_tags"),Tags);Tags.Reset();for(FName Tag:Component->ComponentTags)Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));Host->SetArrayField(TEXT("component_tags"),Tags);
  const FBox Bounds=Root->Bounds.GetBox();if(!Bounds.IsValid||!FMath::IsFinite(Bounds.Min.X)||!FMath::IsFinite(Bounds.Min.Y)||!FMath::IsFinite(Bounds.Min.Z)||!FMath::IsFinite(Bounds.Max.X)||!FMath::IsFinite(Bounds.Max.Y)||!FMath::IsFinite(Bounds.Max.Z)||Bounds.Min.X>=Bounds.Max.X||Bounds.Min.Y>=Bounds.Max.Y||Bounds.Min.Z>=Bounds.Max.Z)return Stop(TEXT("pcg_scoped_native_bounds_pending"),TEXT("native_host_root"),TEXT("bounds"),TEXT("finite positive-volume primitive bounds before generation"),TEXT("invalid/empty/nonfinite"),false);
  Host->SetArrayField(TEXT("bounds_min"),VectorJson(Bounds.Min));Host->SetArrayField(TEXT("bounds_max"),VectorJson(Bounds.Max));Host->SetNumberField(TEXT("component_creation_method"),int32(Component->CreationMethod));Authored->SetObjectField(TEXT("native_host"),Host);return W.R.Error.IsEmpty();
 }
 // AED: one bounded resource-free CPU topology group, shared with the saved-source binding lease.
 // This is not a global native-class gate release or a second generation writer.
 bool PointCPUClass(const UClass* C)const
 {return C==UPCGCreatePointsSettings::StaticClass()||C==UPCGTransformPointsSettings::StaticClass()||C==UPCGBoundsModifierSettings::StaticClass()||C==UPCGUnionSettings::StaticClass();}
 bool SelectSourceNodes(const TArray<UObject*>& Nodes)
 {
  if(Nodes.IsEmpty()||Nodes.Num()>8)return Stop(TEXT("pcg_scoped_point_topology_pending"),TEXT("source_graph"),TEXT("node_count"),TEXT("1..8 native point-pipeline nodes or original finite Weighted pair"),LexToString(Nodes.Num()),false);
  int32 PointCount=0,SpawnerCount=0;bool AllPointCPU=true;
  for(UObject* Raw:Nodes)
  {
   if(!Live(Raw)||Raw->GetClass()!=UPCGNode::StaticClass()||Raw->GetOuter()!=Source)return Stop(TEXT("pcg_scoped_graph_pending"),TEXT("source_graph"),TEXT("node"),TEXT("own exact native node"),TEXT("other role"));
   UObject* S=Ref(Raw,TEXT("SettingsInterface"));if(!W.R.Error.IsEmpty())return false;
   if(!Live(S))return Stop(TEXT("pcg_scoped_graph_pending"),TEXT("source_graph"),TEXT("settings"),TEXT("resident native settings"),TEXT("missing"));
   if(S->GetClass()==UPCGCreatePointsSettings::StaticClass()){++PointCount;if(!PointsNode)PointsNode=static_cast<UPCGNode*>(Raw);}
   if(S->GetClass()==UPCGStaticMeshSpawnerSettings::StaticClass()){++SpawnerCount;SpawnerNode=static_cast<UPCGNode*>(Raw);}
   AllPointCPU&=PointCPUClass(S->GetClass());
  }
  SpawnerProfile=Nodes.Num()==2&&PointCount==1&&SpawnerCount==1;
  if(SpawnerProfile)return true; // Original resource/Weighted topology and owner lease stay unchanged.
  if(!AllPointCPU||PointCount==0)return Stop(TEXT("pcg_scoped_point_class_pending"),TEXT("source_graph"),TEXT("settings_classes"),TEXT("CreatePoints roots plus native TransformPoints/BoundsModifier/Union; no subgraph, data-source, actor writer or GPU"),TEXT("unsupported composition"),false);
  PointPipelineProfile=Nodes.Num()!=1||PointCount!=1;
  if(PointPipelineProfile)PointPipelineNodes=Nodes;
  return true;
 }
 template<typename T>T* PointResident(const TObjectPtr<T>& Slot)
 {
  if(!Slot.IsResolved()){Stop(TEXT("pcg_scoped_point_unresolved"),TEXT("source_schema"),TEXT("resident slot"),TEXT("already resolved; no explicit loader"),TEXT("unresolved"));return nullptr;}
  if(Slot==nullptr)return nullptr;
  UObject* Value=StaticFindObject(UObject::StaticClass(),nullptr,*Slot.GetPathName());TStrongObjectPtr<UObject> Immediate(Live(Value)?Value:nullptr);
  if(!Value||Slot!=Value){Stop(TEXT("pcg_scoped_point_resident_changed"),TEXT("source_schema"),TEXT("resident slot"),TEXT("same resolved resident value"),TEXT("missing/replaced"));return nullptr;}return Cast<T>(Value);
 }
 FProperty* PointFresh(const FString& Role)
 {
  const FPointPath* Path=PointPaths.Find(Role);if(!Path)return nullptr;FProperty* Now=nullptr;
  if(!Path->Step){if(!Live(Path->Owner))return nullptr;Now=FindFProperty<FProperty>(Path->Owner,Path->Root);}
  else
  {
   FProperty* Parent=PointFresh(Path->Parent);if(!Parent)return nullptr;
   if(Path->Step==1){auto* A=CastField<FArrayProperty>(Parent);if(A)Now=A->Inner;}
   else if(Path->Step==2){auto* E=CastField<FEnumProperty>(Parent);if(E)Now=E->GetUnderlyingProperty();}
   else if(Path->Step==3){auto* S=CastField<FStructProperty>(Parent);if(S&&S->Struct.IsResolved()&&S->Struct==Path->StructOwner&&Live(Path->StructOwner))Now=FindFProperty<FProperty>(Path->StructOwner,Path->Member);}
  }
  // Compare fresh owner/parent membership before touching any old descriptor.
  return Now==Path->Field&&Now&&Now->GetClass()==Path->Kind?Now:nullptr;
 }
 bool PointFieldShape(const FString& Role,FProperty* P,const FString& Parent=FString(),int32 Step=0,UScriptStruct* Type=nullptr)
 {
  if(PointPaths.Contains(Role)){P=PointFresh(Role);if(!P)return Stop(TEXT("pcg_scoped_point_field_changed"),Role,TEXT("fresh root/child slot"),TEXT("captured native membership before descriptor dereference"),TEXT("changed"));}
  else
  {
   if(Captured||!P)return Stop(TEXT("pcg_scoped_point_field_changed"),Role,TEXT("membership"),TEXT("original field inventory"),TEXT("new/missing"));
   FPointPath Path;Path.Field=P;Path.Kind=P->GetClass();Path.Owner=P->GetOwnerStruct();Path.Root=P->GetFName();Path.Parent=Parent;Path.Step=Step;Path.StructOwner=Type;Path.Member=P->GetFName();PointPaths.Add(Role,Path);
   if(PointFresh(Role)!=P)return Stop(TEXT("pcg_scoped_point_field_changed"),Role,TEXT("initial native membership"),TEXT("current root/parent slot"),TEXT("changed"));
   if(!PointTypeHolds.ContainsByPredicate([&](const auto& Held){return Held.Get()==Path.Owner;}))PointTypeHolds.Emplace(Path.Owner);
  }
  if(P->ArrayDim!=1||P->HasSetterOrGetter()||!P->GetOwnerStruct()||!P->GetOwnerStruct()->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return Stop(TEXT("pcg_scoped_point_field_pending"),Role,TEXT("layout"),TEXT("single native direct field without accessor"),TEXT("unsupported layout"),false);
  FString Shape=FString::Printf(TEXT("%p|%p|%p|%d|%d|%llu"),P,P->GetClass(),P->GetOwnerVariant().GetRawPointer(),P->GetOffset_ForInternal(),P->GetElementSize(),uint64(P->GetPropertyFlags()));
  if(const auto* B=CastField<FBoolProperty>(P))Shape+=FString::Printf(TEXT("|%u|%u"),B->GetByteOffset(),B->GetFieldMask());
  if(const FString* Old=PointFieldLayouts.Find(Role))return Same(*Old,Shape)||Stop(TEXT("pcg_scoped_point_field_changed"),Role,TEXT("layout"),*Old,Shape);
  PointFieldLayouts.Add(Role,Shape);return true;
 }
 bool PointCatalogue(const FString& Role,FProperty* P,const FString& Parent=FString(),int32 Step=0,UScriptStruct* Owner=nullptr,int32 Depth=0)
 {
  if(Depth>12||!PointFieldShape(Role,P,Parent,Step,Owner))return false;P=PointFresh(Role);if(!P)return false;
  if(auto* A=CastField<FArrayProperty>(P))return PointCatalogue(Role+TEXT("/inner"),A->Inner,Role,1,nullptr,Depth+1);
  if(auto* E=CastField<FEnumProperty>(P))return PointCatalogue(Role+TEXT("/enum_storage"),E->GetUnderlyingProperty(),Role,2,nullptr,Depth+1);
  if(auto* S=CastField<FStructProperty>(P))
  {
   const auto Slot=S->Struct;if(!Slot.IsResolved())return Stop(TEXT("pcg_scoped_point_unresolved"),Role,TEXT("struct slot"),TEXT("resident native schema"),TEXT("unresolved"));
   UScriptStruct* Type=PointResident(Slot);TStrongObjectPtr<UScriptStruct> Immediate(Type);P=PointFresh(Role);if(!P||!Type||!Live(Type)||!Type->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||CastField<FStructProperty>(P)->Struct!=Slot)return false;
   if(!PointTypeHolds.ContainsByPredicate([&](const auto& Held){return Held.Get()==Type;}))PointTypeHolds.Emplace(Type);TArray<FName> Names;for(TFieldIterator<FProperty> It(Type,EFieldIteratorFlags::IncludeSuper);It;++It)if(!It->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))Names.Add(It->GetFName());
   for(FName Name:Names){P=PointFresh(Role);if(!P||CastField<FStructProperty>(P)->Struct!=Slot)return false;FProperty* Child=FindFProperty<FProperty>(Type,Name);if(!Child||!PointCatalogue(Role+TEXT("/")+Name.ToString(),Child,Role,3,Type,Depth+1))return false;}
  }return true;
 }
 bool PointValue(const FString& Role,FProperty* P,const void* V,TSharedPtr<FJsonValue>& Out,int32 Depth=0)
 {
  // V belongs to initialized request-owned storage, never a live container/Inner
  // element held across Object/metadata SDK boundaries (same owned-copy principle as ADT).
  if(Depth>12||!PointFieldShape(Role,P))return false;
  // The first captured projection and one concurrent transient recheck have
  // separate footprint allowances within the unchanged global result budget.
  // Repeated rechecks only charge growth of their maximum, not every discarded DTO.
  ++PointProjectionValueSlots;int64& Charged=Captured?PointRecheckChargedValueSlots:PointProjectionChargedValueSlots;
  if(PointProjectionValueSlots>Charged){if(!W.Budget(PointProjectionValueSlots-Charged))return false;Charged=PointProjectionValueSlots;}
  P=PointFresh(Role);if(!P)return false;
  if(const auto* B=CastField<FBoolProperty>(P)){Out=MakeShared<FJsonValueBoolean>(B->GetPropertyValue(V));return true;}
  FNumericProperty* Number=nullptr;UEnum* Enum=nullptr;TStrongObjectPtr<UEnum> EnumHold(nullptr);
  if(CastField<FEnumProperty>(P))
  {
   // Enum's TObjectPtr is private in this SDK. Use its public getter as an
   // explicit SDK read, never private-offset or serialization/loader bypass.
   if(!W.Read(TEXT("point_enum_descriptor_read"),[&](){auto* E=CastField<FEnumProperty>(PointFresh(Role));UEnum* Value=E?E->GetEnum():nullptr;EnumHold.Reset(Live(Value)?Value:nullptr);return Value;},Enum))return false;
   P=PointFresh(Role);if(!P||!PointFieldShape(Role,P))return false;Number=CastField<FEnumProperty>(P)->GetUnderlyingProperty();
   if(PointFresh(Role+TEXT("/enum_storage"))!=Number||!PointFieldShape(Role+TEXT("/enum_storage"),Number))return false;
  }
  else if(auto* B=CastField<FByteProperty>(P))
  {
   const auto Slot=B->Enum;Enum=PointResident(Slot);EnumHold.Reset(Enum);P=PointFresh(Role);if(!P||!W.R.Error.IsEmpty()||CastField<FByteProperty>(P)->Enum!=Slot)return false;Number=CastField<FNumericProperty>(P);
  }
  else Number=CastField<FNumericProperty>(P);
  if(Number)
  {
   if(Enum){if(!Object(FString::Printf(TEXT("point_enum_%p"),Enum),Enum))return false;P=PointFresh(Role);if(!P||!PointFieldShape(Role,P)||!Enum->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return false;Number=CastField<FEnumProperty>(P)?CastField<FEnumProperty>(P)->GetUnderlyingProperty():CastField<FNumericProperty>(P);if(!Number||(CastField<FEnumProperty>(P)&&PointFresh(Role+TEXT("/enum_storage"))!=Number))return false;auto Names=MakeShared<FJsonObject>();for(int32 I=0;I<Enum->NumEnums();++I)Names->SetStringField(Enum->GetNameByIndex(I).ToString(),LexToString(Enum->GetValueByIndex(I)));Out=MakeShared<FJsonValueObject>(Names);Names->SetStringField(TEXT("captured_value"),LexToString(Number->GetSignedIntPropertyValue(V)));return true;}
   if(Number->IsFloatingPoint()){const double N=Number->GetFloatingPointPropertyValue(V);if(!FMath::IsFinite(N))return Stop(TEXT("pcg_scoped_point_nonfinite"),Role,TEXT("value"),TEXT("finite scalar"),TEXT("NaN/Inf"),false);Out=MakeShared<FJsonValueNumber>(N);}
   else Out=MakeShared<FJsonValueString>(LexToString(Number->GetSignedIntPropertyValue(V)));return true;
  }
  if(auto* N=CastField<FNameProperty>(P)){Out=MakeShared<FJsonValueString>(N->GetPropertyValue(V).ToString());return true;}
  if(auto* T=CastField<FStrProperty>(P)){Out=MakeShared<FJsonValueString>(T->GetPropertyValue(V));return true;}
  if(auto* T=CastField<FTextProperty>(P)){Out=MakeShared<FJsonValueObject>(TextIdentity(T->GetPropertyValue(V)));return true;}
  if(auto* A=CastField<FArrayProperty>(P))
  {
   const FProperty* Inner=A->Inner;if(!PointFieldShape(Role+TEXT("/inner"),A->Inner))return false;FScriptArrayHelper Values(A,V);if(Values.Num()>128)return Stop(TEXT("pcg_scoped_point_array_bound"),Role,TEXT("count"),TEXT("<=128 copied elements"),LexToString(Values.Num()),false);
   TArray<TSharedPtr<FJsonValue>> Rows;for(int32 I=0;I<Values.Num();++I){P=PointFresh(Role);if(!P||!PointFieldShape(Role,P))return false;A=CastField<FArrayProperty>(P);if(!A||A->Inner!=Inner||PointFresh(Role+TEXT("/inner"))!=Inner)return Stop(TEXT("pcg_scoped_point_field_changed"),Role,TEXT("Inner"),TEXT("same captured field"),TEXT("changed"));TSharedPtr<FJsonValue> Value;if(!PointValue(Role+TEXT("/inner"),A->Inner,Values.GetRawPtr(I),Value,Depth+1))return false;Rows.Add(Value);}Out=MakeShared<FJsonValueArray>(Rows);return true;
  }
  if(auto* S=CastField<FStructProperty>(P))
  {
   const auto StructSlot=S->Struct;UScriptStruct* Type=PointResident(StructSlot);TStrongObjectPtr<UScriptStruct> Immediate(Type);P=PointFresh(Role);if(!P||!W.R.Error.IsEmpty())return false;S=CastField<FStructProperty>(P);if(!S||S->Struct!=StructSlot)return false;
   if(!Type||!Object(FString::Printf(TEXT("point_struct_%p"),Type),Type))return false;P=PointFresh(Role);if(!P||!PointFieldShape(Role,P))return false;S=CastField<FStructProperty>(P);if(!S||!Type->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||S->Struct!=StructSlot)return Stop(TEXT("pcg_scoped_point_struct_pending"),Role,TEXT("struct"),TEXT("same held compiled native struct"),TEXT("unsupported or changed"));
   auto FieldsJson=MakeShared<FJsonObject>();TArray<FName> Names;for(TFieldIterator<FProperty> It(Type,EFieldIteratorFlags::IncludeSuper);It;++It)if(!It->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))Names.Add(It->GetFName());if(Names.IsEmpty()||Names.Num()>64)return Stop(TEXT("pcg_scoped_point_struct_pending"),Role,TEXT("members"),TEXT("bounded reflected native value members"),TEXT("opaque/excessive"),false);
   for(FName Name:Names){P=PointFresh(Role);if(!P||!PointFieldShape(Role,P)||CastField<FStructProperty>(P)->Struct!=StructSlot)return false;const FString ChildRole=Role+TEXT("/")+Name.ToString();FProperty* Child=PointFresh(ChildRole);if(!Child||!PointFieldShape(ChildRole,Child))return false;TSharedPtr<FJsonValue> Value;if(!PointValue(ChildRole,Child,Child->ContainerPtrToValuePtr<void>(V),Value,Depth+1))return false;FieldsJson->SetField(Name.ToString(),Value);}Out=MakeShared<FJsonValueObject>(FieldsJson);return true;
  }

  if(auto* O=CastField<FObjectProperty>(P))
  {
   const auto* Handle=O->GetObjectPtrPropertyValuePtr(V);if(!Handle->IsResolved())return Stop(TEXT("pcg_scoped_point_unresolved"),Role,TEXT("reference"),TEXT("resident native descriptor; no loader"),TEXT("unresolved"));
   const UObject* ObjectValue=PointResident(*Handle);TStrongObjectPtr<const UObject> Immediate(ObjectValue);P=PointFresh(Role);if(!P||!W.R.Error.IsEmpty())return false;
   if(!ObjectValue){Out=MakeShared<FJsonValueNull>();return W.R.Error.IsEmpty();}
   if(!ObjectValue->IsA<UField>()||!ObjectValue->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||!Object(FString::Printf(TEXT("point_ref_%p"),ObjectValue),ObjectValue))return Stop(TEXT("pcg_scoped_point_reference_pending"),Role,TEXT("reference"),TEXT("held native schema descriptor; assets/custom references incomplete"),TEXT("unsupported reference"));
   Out=MakeShared<FJsonValueString>(ScopedObjectKey(ObjectValue));return true;
  }
  return Stop(TEXT("pcg_scoped_point_field_pending"),Role,TEXT("type"),TEXT("native scalar/struct/array/schema-reference"),P->GetClass()->GetName(),false);
 }
 struct FPointOwnedValue
 {
  void* Value=nullptr;TFunction<void()> Destroy;TFunction<bool(const void*)> SameValue;
  ~FPointOwnedValue(){if(Destroy)Destroy();} // Never dereference an old FProperty during failure cleanup.
  template<typename T,typename Equal>void Copy(const T& In,Equal Compare){T* Owned=new T(In);Value=Owned;Destroy=[Owned]{delete Owned;};SameValue=[Owned,Compare](const void* Current){return Compare(*Owned,*static_cast<const T*>(Current));};}
  void CopyBytes(const void* In,int32 Size){auto* Owned=new TArray<uint8>();Owned->Append(static_cast<const uint8*>(In),Size);Value=Owned->GetData();Destroy=[Owned]{delete Owned;};SameValue=[Owned,Size](const void* Current){return FMemory::Memcmp(Owned->GetData(),Current,Size)==0;};}
 };
 bool PointPinSame(const FPCGPinProperties& A,const FPCGPinProperties& B)
 {
  auto* Connections=CastField<FBoolProperty>(Field(FPCGPinProperties::StaticStruct(),TEXT("bAllowMultipleConnections")));if(!Connections)return false;
  const auto X=A.AllowedTypes.GetIds(),Y=B.AllowedTypes.GetIds();if(X.Num()!=Y.Num()||A.AllowedTypes.CustomSubtype!=B.AllowedTypes.CustomSubtype)return false;
  for(int32 I=0;I<X.Num();++I)if(!(X[I]==Y[I]))return false; // Public BaseId == compares handles; no tracked Get/registry lookup.
  return A.Label.IsEqual(B.Label,ENameCase::CaseSensitive)&&A.Usage==B.Usage&&A.PinStatus==B.PinStatus&&A.bAllowMultipleData==B.bAllowMultipleData&&A.bInvisiblePin==B.bInvisiblePin&&A.bAllowEditMultipleData==B.bAllowEditMultipleData&&A.bAllowEditMultipleConnections==B.bAllowEditMultipleConnections
   &&Connections->GetPropertyValue_InContainer(&A)==Connections->GetPropertyValue_InContainer(&B)
   &&FTextInspector::GetSharedDataId(A.Tooltip)==FTextInspector::GetSharedDataId(B.Tooltip)&&FTextInspector::GetFlags(A.Tooltip)==FTextInspector::GetFlags(B.Tooltip);
 }
 bool PointCopy(const FString& Role,UPCGSettings* S,TSharedPtr<FPointOwnedValue>& Out)
 {
  FProperty* P=PointFresh(Role);if(!P||!PointFieldShape(Role,P))return false;Out=MakeShared<FPointOwnedValue>();const void* V=P->ContainerPtrToValuePtr<void>(S);
  if(auto* B=CastField<FBoolProperty>(P)){if(!B->IsNativeBool())return false;Out->Copy(B->GetPropertyValue(V),[](bool A,bool C){return A==C;});return true;}
  if(CastField<FNumericProperty>(P)||CastField<FEnumProperty>(P)){Out->CopyBytes(V,P->GetElementSize());return true;}
  if(auto* N=CastField<FNameProperty>(P)){Out->Copy(N->GetPropertyValue(V),[](FName A,FName B){return A.IsEqual(B,ENameCase::CaseSensitive);});return true;}
  if(auto* T=CastField<FStrProperty>(P)){Out->Copy(T->GetPropertyValue(V),[](const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);});return true;}
  if(auto* T=CastField<FStructProperty>(P))
  {
   UScriptStruct* Type=PointResident(T->Struct);TStrongObjectPtr<UScriptStruct> Immediate(Type);P=PointFresh(Role);if(!P||!Type||!PointFieldShape(Role,P))return false;
   UScriptStruct* VectorType=TBaseStructure<FVector>::Get();UScriptStruct* RotatorType=TBaseStructure<FRotator>::Get();P=PointFresh(Role);if(!P||CastField<FStructProperty>(P)->Struct!=Type)return false;V=P->ContainerPtrToValuePtr<void>(S);
   if(Type==VectorType){Out->Copy(*static_cast<const FVector*>(V),[](const FVector& A,const FVector& B){return A==B;});return true;}
   if(Type==RotatorType){Out->Copy(*static_cast<const FRotator*>(V),[](const FRotator& A,const FRotator& B){return A.Pitch==B.Pitch&&A.Yaw==B.Yaw&&A.Roll==B.Roll;});return true;}
  }
  if(auto* A=CastField<FArrayProperty>(P))
  {
   FProperty* Inner=PointFresh(Role+TEXT("/inner"));auto* Struct=CastField<FStructProperty>(Inner);if(!Struct||A->Inner!=Inner)return false;UScriptStruct* Type=PointResident(Struct->Struct);TStrongObjectPtr<UScriptStruct> Immediate(Type);
   UScriptStruct* PointsType=FPCGPoint::StaticStruct();UScriptStruct* PinsType=FPCGPinProperties::StaticStruct();P=PointFresh(Role);Inner=PointFresh(Role+TEXT("/inner"));if(!P||!Inner||CastField<FArrayProperty>(P)->Inner!=Inner||!PointFieldShape(Role,P)||!PointFieldShape(Role+TEXT("/inner"),Inner)||CastField<FStructProperty>(Inner)->Struct!=Type)return false;V=P->ContainerPtrToValuePtr<void>(S);
   if(Type==PointsType){Out->Copy(*static_cast<const TArray<FPCGPoint>*>(V),[](const TArray<FPCGPoint>& X,const TArray<FPCGPoint>& Y){if(X.Num()!=Y.Num())return false;for(int32 I=0;I<X.Num();++I){const auto& A=X[I];const auto& B=Y[I];if(A.Transform.GetLocation()!=B.Transform.GetLocation()||A.Transform.GetRotation()!=B.Transform.GetRotation()||A.Transform.GetScale3D()!=B.Transform.GetScale3D()||A.Density!=B.Density||A.BoundsMin!=B.BoundsMin||A.BoundsMax!=B.BoundsMax||A.Color!=B.Color||A.Steepness!=B.Steepness||A.Seed!=B.Seed||A.MetadataEntry!=B.MetadataEntry)return false;}return true;});return true;}
   if(Type==PinsType){Out->Copy(*static_cast<const TArray<FPCGPinProperties>*>(V),[this](const TArray<FPCGPinProperties>& X,const TArray<FPCGPinProperties>& Y){if(X.Num()!=Y.Num())return false;for(int32 I=0;I<X.Num();++I)if(!PointPinSame(X[I],Y[I]))return false;return true;});return true;}
  }
  return Stop(TEXT("pcg_scoped_point_copy_pending"),Role,TEXT("owned native value"),TEXT("closed-group scalar/math/point-array/pin-array copy with typed destruction"),TEXT("unsupported field"),false);
 }
 bool PointSlot(UObject* Owner,const TCHAR* Name,const UObject* ExpectedValue)
 {if(!Live(Owner))return false;auto* P=CastField<FObjectProperty>(Field(Owner->GetClass(),Name));if(!P)return false;const auto* Slot=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(Owner));return Slot->IsResolved()&&*Slot==ExpectedValue;}
 bool PointArray(UObject* Owner,const TCHAR* Name,const TArray<UObject*>& ExpectedValues)
 {
  if(!Live(Owner))return false;auto* A=CastField<FArrayProperty>(Field(Owner->GetClass(),Name));if(!A)return false;const auto* CapturedInner=Inners.FindByPredicate([A](const FInnerRole& R){return R.Array==A;});if(!CapturedInner||A->Inner!=CapturedInner->Inner)return false;auto* P=CastField<FObjectProperty>(A->Inner);if(!P)return false;FScriptArrayHelper Values(A,A->ContainerPtrToValuePtr<void>(Owner));if(Values.Num()!=ExpectedValues.Num())return false;for(int32 I=0;I<Values.Num();++I){const auto* Slot=P->GetObjectPtrPropertyValuePtr(Values.GetRawPtr(I));if(!Slot->IsResolved()||*Slot!=ExpectedValues[I])return false;}return true;
 }
 bool PointCommonSeal(UPCGSettings* S)
 {
  const TArray<FName> Names{TEXT("bEnabled"),TEXT("bDebug"),TEXT("bBreakDebugger"),TEXT("Seed"),TEXT("bExecutionDependencyRequired"),TEXT("bExposeToLibrary"),TEXT("bExecuteOnGPU"),TEXT("bDumpCookedHLSL"),TEXT("bDumpDataDescriptions"),TEXT("bPrintShaderDebugValues"),TEXT("DebugBufferSize"),TEXT("bTriggerRenderCapture"),TEXT("ProfileKernelIndex")};
  auto Before=MakeShared<FJsonObject>();if(!Scalars(S,Names,Before))return false;const UClass* Class=S->GetClass();const FText Category=S->Category,Description=S->Description;const auto Debug=S->DebugSettings;const bool NativeTest=S->DeterminismSettings.bNativeTests,BlueprintTest=S->DeterminismSettings.bUseBlueprintDeterminismTest;const auto Test=S->DeterminismSettings.DeterminismTestBlueprint.GetGCPtr();const bool Manual=S->IsMarkedForManualEditing();
  PointSeals.Add([this,S,Class,Names,Before,Category,Description,Debug,NativeTest,BlueprintTest,Test,Manual]
  {
   if(!Live(S)||S->GetClass()!=Class)return false;auto Now=MakeShared<FJsonObject>();if(!Scalars(S,Names,Now))return false;FString P,E,A;if(ScopedDifference(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(Now),P,E,A,true))return false;
   const auto& D=S->DebugSettings;return FTextInspector::GetSharedDataId(Category)==FTextInspector::GetSharedDataId(S->Category)&&FTextInspector::GetFlags(Category)==FTextInspector::GetFlags(S->Category)&&FTextInspector::GetSharedDataId(Description)==FTextInspector::GetSharedDataId(S->Description)&&FTextInspector::GetFlags(Description)==FTextInspector::GetFlags(S->Description)
    &&Debug.PointScale==D.PointScale&&Debug.ScaleMethod==D.ScaleMethod&&Debug.PointMesh.ToSoftObjectPath()==D.PointMesh.ToSoftObjectPath()&&Debug.MaterialOverride.ToSoftObjectPath()==D.MaterialOverride.ToSoftObjectPath()&&Debug.bDisplayProperties==D.bDisplayProperties&&NativeTest==S->DeterminismSettings.bNativeTests&&BlueprintTest==S->DeterminismSettings.bUseBlueprintDeterminismTest&&Test==S->DeterminismSettings.DeterminismTestBlueprint.GetGCPtr()&&Manual==S->IsMarkedForManualEditing();
  });return true;
 }
 bool PointSettings(const FString& Role,UPCGSettings* S,const TSharedRef<FJsonObject>& Out)
 {
  if(!PointCPUClass(S->GetClass()))return false;auto ClassJson=MakeShared<FJsonObject>();if(!NativeClass(FString::Printf(TEXT("point_settings_class_%p"),S->GetClass()),S->GetClass(),ClassJson))return false;
  TArray<FString> Keys;
  for(UStruct* C=S->GetClass();C&&C!=UPCGSettings::StaticClass();C=C->GetSuperStruct())for(TFieldIterator<FProperty> It(C,EFieldIterationFlags::None);It;++It){FProperty* P=*It;if(P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated)||P->GetName().EndsWith(TEXT("_DEPRECATED")))continue;const FString Key=Role+TEXT("/")+C->GetPathName()+TEXT(":")+P->GetName();if(!PointCatalogue(Key,P))return false;Keys.Add(Key);}
  TArray<TSharedPtr<FPointOwnedValue>> Values;for(int32 I=0;I<Keys.Num();++I){TSharedPtr<FPointOwnedValue> Copy;if(!PointCopy(Keys[I],S,Copy))return false;Values.Add(Copy);}
  auto Members=MakeShared<FJsonObject>();for(int32 I=0;I<Keys.Num();++I)
  {
   const FString Key=Keys[I];const auto Copy=Values[I];FProperty* Fresh=PointFresh(Key);if(!Fresh||!PointFieldShape(Key,Fresh))return false;TSharedPtr<FJsonValue> Value;if(!PointValue(Key,Fresh,Copy->Value,Value))return false;Members->SetField(Key,Value);
   PointSeals.Add([this,S,Key,Copy]{if(!Live(S))return false;FProperty* P=PointFresh(Key);return P&&PointFieldShape(Key,P)&&Copy->SameValue(P->ContainerPtrToValuePtr<void>(S));});
  }
  Out->SetObjectField(TEXT("native_class"),ClassJson);Out->SetObjectField(TEXT("authored_members"),Members);return true;
 }
 bool PointPipelineSnapshot(const TSharedRef<FJsonObject>& Authored)
 {
  PointSeals.Empty();PointProjectionValueSlots=0;
  // Share the held native descriptor role across pins and derived settings.
  // Each pin still records and seals its own ordered AllowedTypes identities.
  UScriptStruct* PinSchema=FPCGPinProperties::StaticStruct();TStrongObjectPtr<UScriptStruct> PinSchemaHold(PinSchema);if(!Object(TEXT("point_pin_properties_struct"),PinSchema)||!Field(PinSchema,TEXT("bAllowMultipleConnections")))return false;
  TArray<UObject*> Ordinary,Embedded;if(!Refs(Source,TEXT("Nodes"),Ordinary)||!Refs(Source,TEXT("EmbeddedSubgraphs"),Embedded))return false;
  if(Ordinary!=PointPipelineNodes||!Embedded.IsEmpty())return Stop(TEXT("pcg_scoped_point_topology_changed"),TEXT("source_graph"),TEXT("nodes/embedded"),TEXT("same captured native point pipeline without subgraph"),TEXT("different membership"));
  UObject* Input=Ref(Source,TEXT("InputNode"));if(!W.R.Error.IsEmpty())return false;UObject* Output=Ref(Source,TEXT("OutputNode"));if(!W.R.Error.IsEmpty())return false;
  TArray<UObject*> Nodes{Input,Output};Nodes.Append(Ordinary);TArray<TSharedPtr<FJsonValue>> NodeRows;
  PointSeals.Add([this,Input,Output,Ordinary,Embedded]{return PointArray(Source,TEXT("Nodes"),Ordinary)&&PointArray(Source,TEXT("EmbeddedSubgraphs"),Embedded)&&PointSlot(Source,TEXT("InputNode"),Input)&&PointSlot(Source,TEXT("OutputNode"),Output);});
  TSet<const UPCGPin*> AllPins;TSet<const UPCGEdge*> Edges;TMap<const UPCGEdge*,TArray<const UPCGPin*>> EdgeMembership;
  TMap<const UPCGPin*,UPCGNode*> PinOwners;TMap<const UPCGPin*,bool> PinDirections;TMap<UPCGNode*,int64> PointCounts;
  for(int32 N=0;N<Nodes.Num();++N)
  {
   const FString Role=N==0?TEXT("node_input"):N==1?TEXT("node_output"):FString::Printf(TEXT("node_pipeline_%d"),N-2);
   if(!Object(Role,Nodes[N]))return false;auto* Node=Cast<UPCGNode>(Nodes[N]);
   if(!Node||Node->GetClass()!=UPCGNode::StaticClass()||Node->GetOuter()!=Source)return Stop(TEXT("pcg_scoped_point_node_changed"),Role,TEXT("owner/class"),TEXT("captured exact native graph-owned node"),TEXT("different role"));
   const FString SettingsRole=Role+TEXT("_settings");UObject* Raw=Ref(Node,TEXT("SettingsInterface"));if(!W.R.Error.IsEmpty()||!Object(SettingsRole,Raw))return false;auto* S=Cast<UPCGSettings>(Raw);
   if(!S||(N<2?S->GetClass()!=UPCGGraphInputOutputSettings::StaticClass():!PointCPUClass(S->GetClass()))||S->GetOuter()!=(N<2?static_cast<UObject*>(Source):static_cast<UObject*>(Node)))return Stop(TEXT("pcg_scoped_point_settings_changed"),Role,TEXT("class/owner"),TEXT("native IO or own bounded point-pipeline settings"),TEXT("different role"));
   const FName Title=Node->NodeTitle;const int32 X=Node->PositionX,Y=Node->PositionY;const FString Comment=Node->NodeComment;const bool Pinned=Node->bCommentBubblePinned,Visible=Node->bCommentBubbleVisible;
   PointSeals.Add([this,Node,S,Title,X,Y,Comment,Pinned,Visible]{return Live(Node)&&Node->GetClass()==UPCGNode::StaticClass()&&Node->GetOuter()==Source&&PointSlot(Node,TEXT("SettingsInterface"),S)&&Node->NodeTitle.IsEqual(Title,ENameCase::CaseSensitive)&&Node->PositionX==X&&Node->PositionY==Y&&Node->NodeComment.Equals(Comment,ESearchCase::CaseSensitive)&&Node->bCommentBubblePinned==Pinned&&Node->bCommentBubbleVisible==Visible;});
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("role"),Role);Row->SetStringField(TEXT("title"),Node->NodeTitle.ToString());Row->SetNumberField(TEXT("position_x"),Node->PositionX);Row->SetNumberField(TEXT("position_y"),Node->PositionY);Row->SetStringField(TEXT("comment"),Node->NodeComment);Row->SetBoolField(TEXT("comment_pinned"),Node->bCommentBubblePinned);Row->SetBoolField(TEXT("comment_visible"),Node->bCommentBubbleVisible);
   auto Values=MakeShared<FJsonObject>();if(!PointCommonSeal(S)||!Settings(SettingsRole,S,Values))return false;
   if(N<2){if(static_cast<UPCGGraphInputOutputSettings*>(S)->IsInput()!=(N==0))return Stop(TEXT("pcg_scoped_point_io_changed"),Role,TEXT("direction"),TEXT("same native IO direction"),TEXT("different"));}
   else
   {
    // No Blueprint determinism hook or custom execution class is admitted by this CPU group.
    if(S->DeterminismSettings.bUseBlueprintDeterminismTest||S->DeterminismSettings.DeterminismTestBlueprint.GetGCPtr()!=nullptr)return Stop(TEXT("pcg_scoped_point_custom_test_pending"),Role,TEXT("determinism"),TEXT("no custom Blueprint test"),TEXT("custom hook"),false);
    if(!PointSettings(SettingsRole,S,Values))return false;
    if(S->GetClass()==UPCGCreatePointsSettings::StaticClass()){const auto Count=static_cast<UPCGCreatePointsSettings*>(S)->PointsToCreate.Num();if(Count>128)return Stop(TEXT("pcg_scoped_point_count_bound"),Role,TEXT("PointsToCreate"),TEXT("<=128 per root"),LexToString(Count),false);PointCounts.Add(Node,Count);}
   }Row->SetObjectField(TEXT("settings"),Values);TArray<TSharedPtr<FJsonValue>> PinRows;
   for(int32 Direction=0;Direction<2;++Direction){TArray<UObject*> Pins;if(!Refs(Node,Direction?TEXT("OutputPins"):TEXT("InputPins"),Pins))return false;PointSeals.Add([this,Node,Pins,Direction]{return PointArray(Node,Direction?TEXT("OutputPins"):TEXT("InputPins"),Pins);});for(int32 I=0;I<Pins.Num();++I){const FString PinRole=Role+FString::Printf(TEXT("_%s_%d"),Direction?TEXT("out"):TEXT("in"),I);if(!Object(PinRole,Pins[I]))return false;auto* Pin=Cast<UPCGPin>(Pins[I]);if(!Pin||Pin->GetClass()!=UPCGPin::StaticClass()||AllPins.Contains(Pin)||Pin->GetOuter()!=Node||Ref(Pin,TEXT("Node"))!=Node)return Stop(TEXT("pcg_scoped_pin_role_changed"),PinRole,TEXT("owner/membership"),TEXT("unique graph-local pin"),TEXT("compound mismatch"),false);AllPins.Add(Pin);PinOwners.Add(Pin,Node);PinDirections.Add(Pin,Direction!=0);if(AllPins.Num()>64)return Stop(TEXT("pcg_scoped_point_pin_bound"),Role,TEXT("pins"),TEXT("<=64 graph-local pins"),LexToString(AllPins.Num()),false);const FPCGPinProperties Properties=Pin->Properties;auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("path"),Pin->GetPathName());P->SetBoolField(TEXT("output"),Direction!=0);P->SetStringField(TEXT("label"),Properties.Label.ToString());P->SetNumberField(TEXT("usage"),int32(Properties.Usage));P->SetNumberField(TEXT("status"),int32(Properties.PinStatus));P->SetBoolField(TEXT("multiple_data"),Properties.bAllowMultipleData);P->SetBoolField(TEXT("multiple_connections"),Properties.AllowsMultipleConnections());P->SetBoolField(TEXT("invisible"),Properties.bInvisiblePin);P->SetNumberField(TEXT("custom_subtype"),Properties.AllowedTypes.CustomSubtype);P->SetObjectField(TEXT("tooltip"),TextIdentity(Properties.Tooltip));P->SetBoolField(TEXT("edit_multiple_data"),Properties.bAllowEditMultipleData);P->SetBoolField(TEXT("edit_multiple_connections"),Properties.bAllowEditMultipleConnections);TArray<TSharedPtr<FJsonValue>> Types;const auto Ids=Properties.AllowedTypes.GetIds();for(int32 T=0;T<Ids.Num();++T){auto* TypeField=CastField<FObjectProperty>(Field(FPCGDataTypeBaseId::StaticStruct(),TEXT("Struct")));if(!TypeField)return false;auto* Ptr=TypeField->GetObjectPtrPropertyValuePtr(TypeField->ContainerPtrToValuePtr<void>(&Ids[T]));if(!Ptr->IsResolved())return Stop(TEXT("pcg_scoped_unresolved_binding"),PinRole,TEXT("AllowedTypes"),TEXT("resolved native type descriptors"),TEXT("unresolved"));const UObject* Type=Resolved(*Ptr);TStrongObjectPtr<const UObject> ImmediateType(Type);if(!Live(Type)||!Type->IsA<UScriptStruct>()||!Type->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return Stop(TEXT("pcg_scoped_point_pin_type_pending"),PinRole,TEXT("AllowedTypes"),TEXT("native compiled-in schema descriptors"),TEXT("custom/unavailable"),false);if(!Object(FString::Printf(TEXT("point_ref_%p"),Type),Type))return false;Types.Add(MakeShared<FJsonValueString>(ScopedObjectKey(Type)));}P->SetArrayField(TEXT("allowed_types"),Types);
    TArray<UObject*> PinEdges;if(!Refs(Pin,TEXT("Edges"),PinEdges))return false;TArray<TSharedPtr<FJsonValue>> LinkRows;for(UObject* RawEdge:PinEdges){const FString EdgeRole=FString::Printf(TEXT("edge_%p"),RawEdge);if(!Object(EdgeRole,RawEdge))return false;auto* Edge=Cast<UPCGEdge>(RawEdge);if(!Edge||Edge->GetClass()!=UPCGEdge::StaticClass())return Stop(TEXT("pcg_scoped_edge_class"),TEXT("edge_0"),TEXT("class"),TEXT("UPCGEdge"),TEXT("other object"));Edges.Add(Edge);EdgeMembership.FindOrAdd(Edge).Add(Pin);LinkRows.Add(MakeShared<FJsonValueString>(ScopedObjectKey(Edge)));}PointSeals.Add([this,Pin,Node,Properties,PinEdges]{return Live(Pin)&&Pin->GetClass()==UPCGPin::StaticClass()&&Pin->GetOuter()==Node&&PointSlot(Pin,TEXT("Node"),Node)&&PointArray(Pin,TEXT("Edges"),PinEdges)&&PointPinSame(Properties,Pin->Properties);});P->SetArrayField(TEXT("edges"),LinkRows);PinRows.Add(MakeShared<FJsonValueObject>(P));
   }}Row->SetArrayField(TEXT("pins"),PinRows);NodeRows.Add(MakeShared<FJsonValueObject>(Row));
  }
  if(Edges.IsEmpty()||Edges.Num()>64)return Stop(TEXT("pcg_scoped_point_edge_bound"),TEXT("source_graph"),TEXT("edges"),TEXT("1..64 graph-local reciprocal edges"),LexToString(Edges.Num()),false);
  TMap<UPCGNode*,TArray<UPCGNode*>> Forward,Reverse;TMap<UPCGNode*,int32> Degree;TMap<UPCGNode*,int64> Cost;TSet<FString> LogicalEdges;
  for(UObject* Raw:Ordinary){auto* N=static_cast<UPCGNode*>(Raw);Degree.Add(N,0);Cost.Add(N,PointCounts.FindRef(N));}auto* Sink=static_cast<UPCGNode*>(Output);Degree.Add(Sink,0);Cost.Add(Sink,0);
  for(const UPCGEdge* Edge:Edges)
  {
   const auto* Members=EdgeMembership.Find(Edge);UObject* FromObject=Ref(const_cast<UPCGEdge*>(Edge),TEXT("InputPin"));if(!W.R.Error.IsEmpty())return false;UObject* ToObject=Ref(const_cast<UPCGEdge*>(Edge),TEXT("OutputPin"));if(!W.R.Error.IsEmpty())return false;
   auto* From=Cast<UPCGPin>(FromObject);auto* To=Cast<UPCGPin>(ToObject);
   if(!From||!To||!Members||Members->Num()!=2||!Members->Contains(From)||!Members->Contains(To)||!PinDirections.Contains(From)||!PinDirections.Contains(To)||!PinDirections[From]||PinDirections[To]||(Edge->GetOuter()!=From&&Edge->GetOuter()!=To))return Stop(TEXT("pcg_scoped_point_edge_changed"),TEXT("source_graph"),TEXT("edge_roles"),TEXT("same endpoint-owned edge with two reciprocal local directional memberships"),TEXT("different or external endpoints"));
   const UObject* EdgeOuter=Edge->GetOuter();PointSeals.Add([this,Edge,From,To,EdgeOuter]{return Live(Edge)&&Edge->GetClass()==UPCGEdge::StaticClass()&&Edge->GetOuter()==EdgeOuter&&PointSlot(const_cast<UPCGEdge*>(Edge),TEXT("InputPin"),From)&&PointSlot(const_cast<UPCGEdge*>(Edge),TEXT("OutputPin"),To);});
   UPCGNode* A=PinOwners[From];UPCGNode* B=PinOwners[To];const FString Logical=ScopedObjectKey(From)+TEXT("->")+ScopedObjectKey(To);
   if(A==Input||A==Sink||B==Input||!Degree.Contains(A)||!Degree.Contains(B)||LogicalEdges.Contains(Logical)||PointCounts.Contains(B))return Stop(TEXT("pcg_scoped_point_topology_pending"),TEXT("source_graph"),TEXT("data_source/edge"),TEXT("context-free CreatePoints roots, native point operations and graph Output; no external input, duplicate edge or root dependency"),TEXT("unsupported edge"),false);
   LogicalEdges.Add(Logical);Forward.FindOrAdd(A).Add(B);Reverse.FindOrAdd(B).Add(A);++Degree[B];
  }
  TArray<UPCGNode*> Ready;for(const auto& Pair:Degree)if(Pair.Value==0){if(!PointCounts.Contains(Pair.Key))return Stop(TEXT("pcg_scoped_point_unrooted"),TEXT("source_graph"),TEXT("root"),TEXT("every root is CreatePoints"),TEXT("unconnected operation"),false);Ready.Add(Pair.Key);}
  int32 Visited=0;for(int32 I=0;I<Ready.Num();++I){UPCGNode* N=Ready[I];++Visited;if(Cost[N]>1024)return Stop(TEXT("pcg_scoped_point_budget"),TEXT("source_graph"),TEXT("conservative_output_bound"),TEXT("<=1024 point contributions through DAG"),LexToString(Cost[N]),false);if(const auto* Next=Forward.Find(N))for(UPCGNode* M:*Next){Cost[M]+=Cost[N];if(--Degree[M]==0)Ready.Add(M);}}
  if(Visited!=Degree.Num())return Stop(TEXT("pcg_scoped_point_cycle"),TEXT("source_graph"),TEXT("DAG"),TEXT("acyclic native point composition"),TEXT("cycle"),false);
  TSet<UPCGNode*> ReachesOutput;TArray<UPCGNode*> Back{Sink};ReachesOutput.Add(Sink);for(int32 I=0;I<Back.Num();++I)if(const auto* Parents=Reverse.Find(Back[I]))for(UPCGNode* N:*Parents)if(!ReachesOutput.Contains(N)){ReachesOutput.Add(N);Back.Add(N);}
  if(ReachesOutput.Num()!=Ordinary.Num()+1)return Stop(TEXT("pcg_scoped_point_unused_branch"),TEXT("source_graph"),TEXT("output_reachability"),TEXT("all original ordinary nodes reach the graph Output"),TEXT("disconnected branch"),false);
  // Last metadata callback has returned. Pure copied/live seals close earlier
  // settings/CPU bits, pin properties and complete original node/edge memberships.
  // No recursive Snapshot, UObject getter, SDK notification or file scan here.
  TArray<FString> CatalogueRoles;PointPaths.GetKeys(CatalogueRoles);
  for(const FString& CatalogueRole:CatalogueRoles)if(!PointFieldShape(CatalogueRole,PointFresh(CatalogueRole)))return false;
  for(const auto& Seal:PointSeals)if(!Seal())return Stop(TEXT("pcg_scoped_point_source_changed"),TEXT("source_graph"),TEXT("post_callback_owned_value_and_membership_seal"),TEXT("all original copied authored values, CPU policy, pins and topology"),TEXT("changed after observation"));
  Authored->SetArrayField(TEXT("nodes"),NodeRows);Authored->SetNumberField(TEXT("point_pipeline_conservative_output_bound"),Cost[Sink]);Authored->SetStringField(TEXT("point_pipeline_scope"),TEXT("Context-free CreatePoints + native TransformPoints/BoundsModifier/Union DAG, no generated resources; copied authored recursive fields and native schema identities, reciprocal graph-local edges. Original finite Weighted path is separate."));return W.R.Error.IsEmpty();
 }
 bool GraphSourceSnapshot(const TSharedRef<FJsonObject>& Authored)
 {
  // Preserve supported logical scalar bags; generated type/buffer addresses are not authored invariants.
  TSharedPtr<FJsonObject> UserParameters;FString ParameterError;if(!CopyScalarUserParameters(Source,Instance,UserParameters,ParameterError))return Stop(TEXT("pcg_scoped_scalar_parameters_pending"),TEXT("source_graph/instance"),TEXT("user_parameters"),TEXT("preserved supported scalar logical schema/default/actual/override set"),ParameterError,false);Authored->SetObjectField(TEXT("user_parameters"),UserParameters);
  auto ParametersCurrent=[&]()->bool
  {
   TSharedPtr<FJsonObject> Now;FString Error;if(!CopyScalarUserParameters(Source,Instance,Now,Error))return Stop(TEXT("pcg_scoped_scalar_parameters_changed"),TEXT("source_graph/instance"),TEXT("user_parameters"),TEXT("same copied logical scalar schema/default/actual/override set"),Error);
   FString Path,Want,Got;if(ScopedDifference(MakeShared<FJsonValueObject>(UserParameters),MakeShared<FJsonValueObject>(Now),Path,Want,Got,true))return Stop(TEXT("pcg_scoped_scalar_parameters_changed"),TEXT("source_graph/instance"),Path,Want,Got);
   if(Instance&&(!Component||!PointSlot(Component,TEXT("GraphInstance"),Instance)||!PointSlot(Instance,TEXT("Graph"),Source)))return Stop(TEXT("pcg_scoped_scalar_parameters_changed"),TEXT("component/graph_instance"),TEXT("post_read_owner_slots"),TEXT("same captured Component.GraphInstance and Instance.Graph"),TEXT("changed during const parameter read"));
   // Const bag reads may cross descriptor access boundaries. Close earlier DAG
   // copies with existing pure seals; do not recurse Snapshot or rebase source.
   if(PointPipelineProfile){TArray<FString> Roles;PointPaths.GetKeys(Roles);for(const FString& Role:Roles)if(!PointFieldShape(Role,PointFresh(Role)))return false;for(const auto& Seal:PointSeals)if(!Seal())return Stop(TEXT("pcg_scoped_point_source_changed"),TEXT("source_graph"),TEXT("post_parameter_read_pure_seal"),TEXT("same original copied CPU/typed values/pins/topology"),TEXT("changed during parameter read"));}
   return W.R.Error.IsEmpty();
  };
  if(Instance){TArray<FGuid> OverrideGuids=Instance->ParametersOverrides.PropertiesIDsOverridden.Array();OverrideGuids.Sort();TArray<TSharedPtr<FJsonValue>> OverrideRows;for(const FGuid& Guid:OverrideGuids)OverrideRows.Add(MakeShared<FJsonValueString>(Guid.ToString(EGuidFormats::DigitsWithHyphens)));Authored->SetArrayField(TEXT("parameter_override_guids"),OverrideRows);}
  auto GraphPolicy=MakeShared<FJsonObject>();if(!Scalars(Source,{TEXT("bUseHierarchicalGeneration"),TEXT("HiGenGridSizeMultiplier"),TEXT("bUse2DGrid"),TEXT("bHasDefaultConstructedInputs")},GraphPolicy))return false;if(GraphPolicy->GetBoolField(TEXT("bUseHierarchicalGeneration")))return Stop(TEXT("pcg_scoped_higen_pending"),TEXT("source_graph"),TEXT("hierarchical_generation"),TEXT("false for this scoped CPU path"),TEXT("true"));Authored->SetObjectField(TEXT("graph_policy"),GraphPolicy);
  if(PointPipelineProfile)return PointPipelineSnapshot(Authored)&&ParametersCurrent();
  TArray<UObject*> Ordinary,Embedded;if(!Refs(Source,TEXT("Nodes"),Ordinary)||!Refs(Source,TEXT("EmbeddedSubgraphs"),Embedded)||!Condition(Ordinary.Num()==(SpawnerProfile?2:1)&&Embedded.IsEmpty(),TEXT("source_graph"),TEXT("regular_nodes/embedded"),TEXT("one CreatePoints node and no embedded subgraphs"),FString::Printf(TEXT("%d/%d"),Ordinary.Num(),Embedded.Num())))return false;
  UObject* Input=Ref(Source,TEXT("InputNode"));if(!W.R.Error.IsEmpty())return false;UObject* Output=Ref(Source,TEXT("OutputNode"));if(!W.R.Error.IsEmpty())return false;TArray<UObject*> Nodes{Input,Output,PointsNode?PointsNode:Ordinary[0]};if(SpawnerProfile)Nodes.Add(SpawnerNode);TArray<TSharedPtr<FJsonValue>> NodeRows;TSet<const UPCGPin*> AllPins;TSet<const UPCGEdge*> Edges;TMap<const UPCGEdge*,TArray<const UPCGPin*>> EdgeMembership;const UPCGPin* PointsOut=nullptr;const UPCGPin* GraphOut=nullptr;const UPCGPin* SpawnerIn=nullptr;const UPCGPin* SpawnerOut=nullptr;
  for(int32 N=0;N<Nodes.Num();++N){const FString Role=N==0?TEXT("node_input"):(N==1?TEXT("node_output"):(N==2?TEXT("node_points"):TEXT("node_spawner")));if(!Object(Role,Nodes[N]))return false;auto* Node=Cast<UPCGNode>(Nodes[N]);if(!Condition(Node&&Node->GetClass()==UPCGNode::StaticClass()&&Node->GetOuter()==Source,Role,TEXT("class/owner"),TEXT("native node owned by source graph"),TEXT("compound mismatch")))return false;
   const FString SettingsRole=Role+TEXT("_settings");UObject* RawSettings=Ref(Node,TEXT("SettingsInterface"));if(!W.R.Error.IsEmpty()||!Object(SettingsRole,RawSettings))return false;auto* SettingsObject=Cast<UPCGSettings>(RawSettings);
   UClass* ExpectedSettingsClass=N==3?UPCGStaticMeshSpawnerSettings::StaticClass():N==2?UPCGCreatePointsSettings::StaticClass():UPCGGraphInputOutputSettings::StaticClass();
   if(!SettingsObject||SettingsObject->GetClass()!=ExpectedSettingsClass)return Stop(TEXT("pcg_scoped_settings_pending"),RawSettings->GetPathName(),TEXT("class"),ExpectedSettingsClass->GetPathName(),RawSettings->GetClass()->GetPathName(),true);
   // Installed PCGGraph constructor/PostLoad owns both default IO settings directly; AddNodeOfType reparents authored settings to the new Node.
   const UObject* ExpectedSettingsOuter=N<2?static_cast<const UObject*>(Source):static_cast<const UObject*>(Node);const UObject* ActualSettingsOuter=SettingsObject->GetOuter();
   if(ActualSettingsOuter!=ExpectedSettingsOuter){const auto* CopiedRole=ObjectRows.FindByPredicate([&](const TSharedPtr<FJsonValue>& V){return Str(V->AsObject(),TEXT("role"))==SettingsRole;});const FString ActualOuter=CopiedRole?Str((*CopiedRole)->AsObject(),TEXT("outer_path")):FString::Printf(TEXT("unheld owner pointer %p"),ActualSettingsOuter);return Stop(TEXT("pcg_scoped_settings_pending"),SettingsObject->GetPathName(),TEXT("Outer"),ExpectedSettingsOuter->GetPathName(),ActualOuter,true);}
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("role"),Role);Row->SetStringField(TEXT("title"),Node->NodeTitle.ToString());Row->SetNumberField(TEXT("position_x"),Node->PositionX);Row->SetNumberField(TEXT("position_y"),Node->PositionY);Row->SetStringField(TEXT("comment"),Node->NodeComment);Row->SetBoolField(TEXT("comment_pinned"),Node->bCommentBubblePinned);Row->SetBoolField(TEXT("comment_visible"),Node->bCommentBubbleVisible);auto Values=MakeShared<FJsonObject>();if(!Settings(Role+TEXT("_settings"),SettingsObject,Values))return false;
   if(N<2){const auto* IO=static_cast<UPCGGraphInputOutputSettings*>(SettingsObject);if(!Condition(IO->IsInput()==(N==0),Role,TEXT("io_direction"),N==0?TEXT("input"):TEXT("output"),IO->IsInput()?TEXT("input"):TEXT("output")))return false;}
   else if(N==3){if(!Spawner(Role,static_cast<UPCGStaticMeshSpawnerSettings*>(SettingsObject),Values))return false;}
   else{const auto* Create=static_cast<UPCGCreatePointsSettings*>(SettingsObject);if(!Condition(Create->PointsToCreate.Num()<=128,Role,TEXT("PointsToCreate.Num"),TEXT("0..128 points independent of node count"),FString::FromInt(Create->PointsToCreate.Num())))return false;Values->SetNumberField(TEXT("coordinate_space"),int32(Create->CoordinateSpace));Values->SetBoolField(TEXT("cull_outside_volume"),Create->bCullPointsOutsideVolume);TArray<TSharedPtr<FJsonValue>> PointsJson;const TArray<FPCGPoint> CopiedPoints=Create->PointsToCreate;for(const auto& Point:CopiedPoints){auto P=MakeShared<FJsonObject>();P->SetArrayField(TEXT("location"),VectorJson(Point.Transform.GetLocation()));const auto Q=Point.Transform.GetRotation();P->SetArrayField(TEXT("rotation_quaternion"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});P->SetArrayField(TEXT("scale"),VectorJson(Point.Transform.GetScale3D()));P->SetNumberField(TEXT("density"),Point.Density);P->SetArrayField(TEXT("bounds_min"),VectorJson(Point.BoundsMin));P->SetArrayField(TEXT("bounds_max"),VectorJson(Point.BoundsMax));P->SetArrayField(TEXT("color"),{MakeShared<FJsonValueNumber>(Point.Color.X),MakeShared<FJsonValueNumber>(Point.Color.Y),MakeShared<FJsonValueNumber>(Point.Color.Z),MakeShared<FJsonValueNumber>(Point.Color.W)});P->SetNumberField(TEXT("steepness"),Point.Steepness);P->SetNumberField(TEXT("seed"),Point.Seed);P->SetStringField(TEXT("metadata_entry"),LexToString(Point.MetadataEntry));PointsJson.Add(MakeShared<FJsonValueObject>(P));}Values->SetArrayField(TEXT("points"),PointsJson);}
   Row->SetObjectField(TEXT("settings"),Values);TArray<TSharedPtr<FJsonValue>> PinRows;
   for(int32 Direction=0;Direction<2;++Direction){TArray<UObject*> Pins;if(!Refs(Node,Direction?TEXT("OutputPins"):TEXT("InputPins"),Pins))return false;for(int32 I=0;I<Pins.Num();++I){const FString PinRole=Role+FString::Printf(TEXT("_%s_%d"),Direction?TEXT("out"):TEXT("in"),I);if(!Object(PinRole,Pins[I]))return false;auto* Pin=Cast<UPCGPin>(Pins[I]);if(!Pin||AllPins.Contains(Pin)||Pin->GetOuter()!=Node||Ref(Pin,TEXT("Node"))!=Node)return Stop(TEXT("pcg_scoped_pin_role_changed"),PinRole,TEXT("owner/membership"),TEXT("unique graph-local pin"),TEXT("compound mismatch"),false);AllPins.Add(Pin);auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("path"),Pin->GetPathName());P->SetBoolField(TEXT("output"),Direction!=0);P->SetStringField(TEXT("label"),Pin->Properties.Label.ToString());P->SetNumberField(TEXT("usage"),int32(Pin->Properties.Usage));P->SetNumberField(TEXT("status"),int32(Pin->Properties.PinStatus));P->SetBoolField(TEXT("multiple_data"),Pin->Properties.bAllowMultipleData);P->SetBoolField(TEXT("multiple_connections"),Pin->Properties.AllowsMultipleConnections());P->SetBoolField(TEXT("invisible"),Pin->Properties.bInvisiblePin);P->SetNumberField(TEXT("custom_subtype"),Pin->Properties.AllowedTypes.CustomSubtype);P->SetObjectField(TEXT("tooltip"),TextIdentity(Pin->Properties.Tooltip));P->SetBoolField(TEXT("edit_multiple_data"),Pin->Properties.bAllowEditMultipleData);P->SetBoolField(TEXT("edit_multiple_connections"),Pin->Properties.bAllowEditMultipleConnections);TArray<TSharedPtr<FJsonValue>> Types;const auto Ids=Pin->Properties.AllowedTypes.GetIds();for(int32 T=0;T<Ids.Num();++T){auto* TypeField=CastField<FObjectProperty>(Field(FPCGDataTypeBaseId::StaticStruct(),TEXT("Struct")));if(!TypeField)return false;auto* Ptr=TypeField->GetObjectPtrPropertyValuePtr(TypeField->ContainerPtrToValuePtr<void>(&Ids[T]));if(!Ptr->IsResolved())return Stop(TEXT("pcg_scoped_unresolved_binding"),PinRole,TEXT("AllowedTypes"),TEXT("resolved native type descriptors"),TEXT("unresolved"));const UObject* Type=Resolved(*Ptr);if(!Object(PinRole+FString::Printf(TEXT("_type_%d"),T),Type))return false;Types.Add(MakeShared<FJsonValueString>(ScopedObjectKey(Type)));}P->SetArrayField(TEXT("allowed_types"),Types);
    TArray<UObject*> PinEdges;if(!Refs(Pin,TEXT("Edges"),PinEdges))return false;TArray<TSharedPtr<FJsonValue>> LinkRows;for(UObject* RawEdge:PinEdges){const FString EdgeRole=SpawnerProfile?FString::Printf(TEXT("edge_%p"),RawEdge):FString(TEXT("edge_0"));if(!Object(EdgeRole,RawEdge))return false;auto* Edge=Cast<UPCGEdge>(RawEdge);if(!Edge)return Stop(TEXT("pcg_scoped_edge_class"),TEXT("edge_0"),TEXT("class"),TEXT("UPCGEdge"),TEXT("other object"));Edges.Add(Edge);EdgeMembership.FindOrAdd(Edge).Add(Pin);LinkRows.Add(MakeShared<FJsonValueString>(ScopedObjectKey(Edge)));}P->SetArrayField(TEXT("edges"),LinkRows);PinRows.Add(MakeShared<FJsonValueObject>(P));if(N==2&&Direction&&Pin->Properties.Label==TEXT("Out"))PointsOut=Pin;if(N==1&&!Direction&&Pin->Properties.Label==TEXT("Out"))GraphOut=Pin;if(N==3&&!Direction&&Pin->Properties.Label==TEXT("In"))SpawnerIn=Pin;if(N==3&&Direction&&Pin->Properties.Label==TEXT("Out"))SpawnerOut=Pin;
   }}Row->SetArrayField(TEXT("pins"),PinRows);NodeRows.Add(MakeShared<FJsonValueObject>(Row));
  }
  if(SpawnerProfile)
  {
   if(!Condition(Edges.Num()==2&&PointsOut&&SpawnerIn&&SpawnerOut&&GraphOut,TEXT("source_graph"),TEXT("weighted_topology"),TEXT("CreatePoints.Out->Spawner.In and Spawner.Out->Output.Out"),FString::FromInt(Edges.Num())))return false;
   bool First=false,Second=false;
   for(const auto* Edge:Edges){const auto* Members=EdgeMembership.Find(Edge);if(!Members||Members->Num()!=2)return Stop(TEXT("pcg_scoped_edge_membership_changed"),TEXT("source_graph"),TEXT("edges"),TEXT("two reciprocal memberships"),TEXT("different"));const auto* From=Ref(const_cast<UPCGEdge*>(Edge),TEXT("InputPin"));const auto* To=Ref(const_cast<UPCGEdge*>(Edge),TEXT("OutputPin"));if(!W.R.Error.IsEmpty())return false;if(From==PointsOut&&To==SpawnerIn&&Members->Contains(PointsOut)&&Members->Contains(SpawnerIn))First=true;else if(From==SpawnerOut&&To==GraphOut&&Members->Contains(SpawnerOut)&&Members->Contains(GraphOut))Second=true;else return Stop(TEXT("pcg_scoped_edge_membership_changed"),TEXT("source_graph"),TEXT("endpoints"),TEXT("two declared graph-local edges"),TEXT("other endpoints"));}if(!First||!Second)return Stop(TEXT("pcg_scoped_edge_membership_changed"),TEXT("source_graph"),TEXT("topology"),TEXT("both declared edges once"),TEXT("missing edge"));
   // Active generation retains its Actor/resource/survivor guard. Binding uses its own
   // unchanged inactive own-SCS/no-live-instance guard, never an unguarded Actor.
   if(!BindingSourceOnly&&!SpawnerOwner(Authored))return false;
  }
  else
  {
  if(!Condition(Edges.Num()==1&&PointsOut&&GraphOut,TEXT("source_graph"),TEXT("edge_count/Out_pins"),TEXT("one CreatePoints.Out to Output.Out edge"),FString::FromInt(Edges.Num())))return false;const UPCGEdge* Edge=*Edges.CreateConstIterator();const auto* Members=EdgeMembership.Find(Edge);if(!Condition(Members&&Members->Num()==2&&Members->Contains(PointsOut)&&Members->Contains(GraphOut)&&Ref(const_cast<UPCGEdge*>(Edge),TEXT("InputPin"))==PointsOut&&Ref(const_cast<UPCGEdge*>(Edge),TEXT("OutputPin"))==GraphOut,TEXT("edge_0"),TEXT("endpoints/reciprocal_membership"),TEXT("CreatePoints output and graph output input with reciprocal edge"),TEXT("compound endpoint mismatch")))return false;
  }
  Authored->SetArrayField(TEXT("nodes"),NodeRows);return W.R.Error.IsEmpty()&&ParametersCurrent();
 }
 bool Snapshot(TSharedPtr<FJsonObject>& Out)
 {
  TGuardValue<bool> SnapshotGuard(InSnapshot,true);auto Authored=MakeShared<FJsonObject>();if(!Object(TEXT("editor"),W.Editor.Get())||!Object(TEXT("world"),W.World.Get())||!Object(TEXT("actor"),Actor)||!Object(TEXT("actor_class"),Actor->GetClass())||!Object(TEXT("component"),Component)||!Object(TEXT("component_class"),Component->GetClass()))return false;
  if(!Condition(Component->GetOwner()==Actor&&Component->GetWorld()==W.World.Get()&&Actor->GetWorld()==W.World.Get(),TEXT("component"),TEXT("owner/world"),TEXT("exact selected Actor and Editor world"),TEXT("compound role check")))return false;
  if(Ref(Component,TEXT("GraphInstance"))!=Instance||!Object(TEXT("graph_instance"),Instance)||Instance->GetOuter()!=Component)return Stop(TEXT("pcg_scoped_graph_instance_changed"),TEXT("component"),TEXT("GraphInstance"),TEXT("captured own native instance"),TEXT("different slot/owner"),false);
  if(Ref(Instance,TEXT("Graph"))!=Source||!Object(TEXT("source_graph"),Source))return Stop(TEXT("pcg_scoped_source_graph_changed"),TEXT("graph_instance"),TEXT("Graph"),TEXT("captured direct graph"),TEXT("different slot"),false);
  if(NativeHost){if(!NativeHostSnapshot(Authored))return false;}else{UClass* Class=Actor->GetClass();if(!Class->ClassGeneratedBy.IsResolved()||Class->ClassGeneratedBy.Get()!=SourceBlueprint||Class->GetSuperClass()!=AActor::StaticClass()||!Object(TEXT("source_blueprint"),SourceBlueprint)||!SourceBlueprint->GeneratedClass.GetGCPtr().IsResolved()||SourceBlueprint->GeneratedClass.Get()!=Class||!SourceBlueprint->ParentClass.GetGCPtr().IsResolved()||SourceBlueprint->ParentClass.GetGCPtr()!=AActor::StaticClass())return Stop(TEXT("pcg_scoped_blueprint_binding_changed"),TEXT("actor_class"),TEXT("source_blueprint"),TEXT("same direct Actor Blueprint and generated class"),TEXT("compound source role mismatch"),false);}
  if(!Condition(!Source->GetPackage()->IsDirty()&&(NativeHost||!SourceBlueprint->GetPackage()->IsDirty()),TEXT("source_packages"),TEXT("dirty"),TEXT("saved clean source graph and Blueprint"),TEXT("dirty source")))return false;
  TArray<UPCGComponent*> OwnerComponents;Actor->GetComponents<UPCGComponent>(OwnerComponents);if(!Condition(OwnerComponents.Num()==1&&OwnerComponents[0]==Component,TEXT("actor"),TEXT("PCG_component_membership"),TEXT("same sole native PCG component"),FString::FromInt(OwnerComponents.Num())))return false;
  auto OwnerInput=MakeShared<FJsonObject>();UObject* RawRoot=Ref(Actor,TEXT("RootComponent"));if(!W.R.Error.IsEmpty())return false;if(RawRoot){if(!Object(TEXT("actor_root"),RawRoot))return false;auto* Root=Cast<USceneComponent>(RawRoot);if(!Root||Root->GetOwner()!=Actor)return Stop(TEXT("pcg_scoped_root_role_changed"),TEXT("actor_root"),TEXT("class/owner"),TEXT("own SceneComponent"),TEXT("compound mismatch"),false);const FTransform Pose=Root->GetComponentTransform();OwnerInput->SetArrayField(TEXT("location"),VectorJson(Pose.GetLocation()));const FQuat Rotation=Pose.GetRotation();OwnerInput->SetArrayField(TEXT("quaternion"),{MakeShared<FJsonValueNumber>(Rotation.X),MakeShared<FJsonValueNumber>(Rotation.Y),MakeShared<FJsonValueNumber>(Rotation.Z),MakeShared<FJsonValueNumber>(Rotation.W)});OwnerInput->SetArrayField(TEXT("scale"),VectorJson(Pose.GetScale3D()));OwnerInput->SetStringField(TEXT("root"),ScopedObjectKey(Root));}else OwnerInput->SetStringField(TEXT("root"),TEXT(""));Authored->SetObjectField(TEXT("owner_input"),OwnerInput);
  auto Policy=MakeShared<FJsonObject>();Policy->SetNumberField(TEXT("seed"),Component->Seed);Policy->SetBoolField(TEXT("activated"),Component->bActivated);Policy->SetBoolField(TEXT("partitioned"),Component->bIsComponentPartitioned);Policy->SetNumberField(TEXT("generation_trigger"),int32(Component->GenerationTrigger));Policy->SetBoolField(TEXT("regenerate_in_editor"),Component->bRegenerateInEditor);Policy->SetBoolField(TEXT("generate_on_drop"),Component->bGenerateOnDropWhenTriggerOnDemand);Policy->SetBoolField(TEXT("preview"),Component->IsInPreviewMode());Authored->SetObjectField(TEXT("component_policy"),Policy);
  if(!Condition(Component->bActivated&&!Component->bIsComponentPartitioned&&!Component->IsManagedByRuntimeGenSystem()&&!Component->IsInPreviewMode()&&Component->IsRegistered(),TEXT("component"),TEXT("generation_policy"),TEXT("activated registered nonpartitioned nonruntime nonpreview"),TEXT("compound policy mismatch")))return false;
  // Actual scalar instance/default/override state is copied by the common graph observer below.
  if(!GraphSourceSnapshot(Authored))return false;
  if(CleanupMode&&!SpawnerProfile)
  {
   TArray<UActorComponent*> CurrentComponents;Actor->GetComponents(CurrentComponents);
   if(Captured){if(CurrentComponents.Num()!=CleanupComponents.Num())return Stop(TEXT("pcg_scoped_cleanup_components_changed"),TEXT("actor"),TEXT("component_count"),FString::FromInt(CleanupComponents.Num()),FString::FromInt(CurrentComponents.Num()));for(auto* Candidate:CurrentComponents)if(!CleanupComponents.Contains(Candidate))return Stop(TEXT("pcg_scoped_cleanup_components_changed"),TEXT("actor"),TEXT("component_membership"),TEXT("captured complete component set"),TEXT("different member"),false);}
   else{if(CurrentComponents.Num()>128)return Stop(TEXT("pcg_scoped_cleanup_components_bound"),TEXT("actor"),TEXT("component_count"),TEXT("<=128"),FString::FromInt(CurrentComponents.Num()));CleanupComponents=CurrentComponents;}
   TArray<TSharedPtr<FJsonValue>> ComponentRows;
   for(int32 ComponentIndex=0;ComponentIndex<CleanupComponents.Num();++ComponentIndex)
   {
    auto* Other=CleanupComponents[ComponentIndex];const FString Role=FString::Printf(TEXT("cleanup_owner_component_%d"),ComponentIndex);if(!Object(Role,Other))return false;
    if(Other->GetOwner()!=Actor||Other->GetWorld()!=W.World.Get())return Stop(TEXT("pcg_scoped_cleanup_component_role_changed"),Role,TEXT("owner/world"),TEXT("same Actor and Editor world"),TEXT("compound role mismatch"),false);
    auto ComponentRow=MakeShared<FJsonObject>();ComponentRow->SetStringField(TEXT("identity"),ScopedObjectKey(Other));ComponentRow->SetBoolField(TEXT("selected"),Other==Component);
    if(Other!=Component){ComponentRow->SetBoolField(TEXT("registered"),Other->IsRegistered());ComponentRow->SetBoolField(TEXT("active"),Other->IsActive());TArray<TSharedPtr<FJsonValue>> Tags;for(const FName Tag:Other->ComponentTags)Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));ComponentRow->SetArrayField(TEXT("tags"),Tags);
     if(const auto* Scene=Cast<USceneComponent>(Other)){const FTransform Pose=Scene->GetRelativeTransform();const FQuat Rotation=Pose.GetRotation();ComponentRow->SetArrayField(TEXT("relative_location"),VectorJson(Pose.GetLocation()));ComponentRow->SetArrayField(TEXT("relative_rotation"),{MakeShared<FJsonValueNumber>(Rotation.X),MakeShared<FJsonValueNumber>(Rotation.Y),MakeShared<FJsonValueNumber>(Rotation.Z),MakeShared<FJsonValueNumber>(Rotation.W)});ComponentRow->SetArrayField(TEXT("relative_scale"),VectorJson(Pose.GetScale3D()));}}
    ComponentRows.Add(MakeShared<FJsonValueObject>(ComponentRow));
   }Authored->SetArrayField(TEXT("cleanup_owner_components"),ComponentRows);
  }
  Out=MakeShared<FJsonObject>();Out->SetStringField(TEXT("profile"),PointPipelineProfile?(RefreshMode?TEXT("owned_editor_cpu_point_pipeline_refresh_v1"):TEXT("owned_editor_cpu_point_pipeline_v1")):RefreshMode?TEXT("owned_editor_cpu_createpoints_refresh_v1"):SpawnerProfile?TEXT("owned_editor_cpu_weighted_ism_v1"):TEXT("owned_editor_cpu_createpoints_v1"));Out->SetArrayField(TEXT("objects"),ObjectRows);Out->SetObjectField(TEXT("source_authored"),Authored);int32 JsonCount=0;if(!Finite(MakeShared<FJsonValueObject>(Out),0,JsonCount))return Stop(TEXT("pcg_scoped_nonfinite_source"),TEXT("source_authored"),TEXT("finite_snapshot"),TEXT("bounded finite authored values"),TEXT("nonfinite/excessive"),false);return W.R.Error.IsEmpty();
 }
 bool Current()
 {
  if(InSnapshot)return true;TSharedPtr<FJsonObject> Actual;if(!Snapshot(Actual))return false;FString Path,Want,Got;if(ScopedDifference(MakeShared<FJsonValueObject>(Baseline),MakeShared<FJsonValueObject>(Actual),Path,Want,Got,CleanupMode||SpawnerProfile))return Stop(TEXT("pcg_scoped_authored_changed"),TEXT("scoped_generate_source"),Path,Want,Got);return true;
 }
 bool Files(TArray<TSharedPtr<FJsonValue>>& Out)
 {
  TArray<const UObject*> FileSources{static_cast<const UObject*>(Source)};if(!NativeVolumeHost)FileSources.Add(NativeHost?static_cast<const UObject*>(NativeMesh):static_cast<const UObject*>(SourceBlueprint));if(SpawnerProfile)for(const UObject* AssetObject:AssetFiles)FileSources.AddUnique(AssetObject);for(const UObject* SourceObject:FileSources){if(!SourceObject||!Live(SourceObject))return Stop(TEXT("pcg_scoped_source_file_role_lost"),TEXT("source_files"),TEXT("live_source"),TEXT("actual current graph plus applicable Blueprint/native mesh source; volume uses its real graph only"),TEXT("unavailable"));if(!W.SDK(TEXT("scoped_source_file_read")))return false;const FString Filename=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(SourceObject->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()));const int64 Size=IFileManager::Get().FileSize(*Filename);TArray<uint8> Bytes;const bool Loaded=Size>=0&&Size<=67108864&&FFileHelper::LoadFileToArray(Bytes,*Filename);if(!W.After(TEXT("scoped_source_file_read")))return false;if(!Loaded||Bytes.Num()!=Size)return Stop(TEXT("pcg_scoped_source_file_unavailable"),Filename,TEXT("saved_file"),TEXT("stable saved source <=64MiB"),TEXT("missing/unreadable/size changed"));uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Filename);J->SetBoolField(TEXT("exists"),true);J->SetNumberField(TEXT("bytes"),Size);J->SetStringField(TEXT("sha1"),BytesToHex(Digest,FSHA1::DigestSize).ToLower());Out.Add(MakeShared<FJsonValueObject>(J));}return true;
 }
 bool Prepare(UPCGComponent* C,AActor* A,const TSharedPtr<FJsonObject>& Expectation)
 {
  Component=C;Actor=A;if(Expectation&&!ExpectShape(Expectation))return Stop(TEXT("pcg_scoped_expectation_invalid"),TEXT("expectation"),TEXT("shape"),TEXT("exact scoped source proof"),TEXT("invalid"),false);
  if(!Object(TEXT("editor"),W.Editor.Get())||!Object(TEXT("world"),W.World.Get())||!Object(TEXT("actor"),Actor)||!Object(TEXT("actor_class"),Actor->GetClass())||!Object(TEXT("component"),C)||!Object(TEXT("component_class"),C->GetClass()))return false;
  NativeVolumeHost=Actor->GetClass()==APCGVolume::StaticClass();NativeHost=NativeVolumeHost||Actor->GetClass()==AStaticMeshActor::StaticClass();if(NativeHost){if(!Condition(C->GetClass()==UPCGComponent::StaticClass()&&C->GetOuter()==Actor&&C->CreationMethod==(NativeVolumeHost?EComponentCreationMethod::Native:EComponentCreationMethod::Instance),TEXT("target"),TEXT("native_host"),TEXT("exact own native PCG on stock Volume or own instance PCG on native StaticMeshActor"),TEXT("other host/owner/creation role")))return false;}else{if(!Condition(C->GetClass()==UPCGComponent::StaticClass()&&Actor->GetClass()->GetClass()==UBlueprintGeneratedClass::StaticClass()&&Actor->GetClass()->GetSuperClass()==AActor::StaticClass(),TEXT("target"),TEXT("native_component/direct_Actor_BP"),TEXT("exact native PCG component on direct Actor Blueprint"),TEXT("other class profile")))return false;}
  UObject* RawInstance=Ref(C,TEXT("GraphInstance"));if(!W.R.Error.IsEmpty()||!Object(TEXT("graph_instance"),RawInstance))return false;Instance=Cast<UPCGGraphInstance>(RawInstance);if(!Instance||Instance->GetClass()!=UPCGGraphInstance::StaticClass())return Stop(TEXT("pcg_scoped_instance_pending"),TEXT("component"),TEXT("GraphInstance"),TEXT("own exact native instance"),TEXT("other/null instance"),false);
  UObject* RawSource=Ref(Instance,TEXT("Graph"));if(!W.R.Error.IsEmpty()||!Object(TEXT("source_graph"),RawSource))return false;Source=Cast<UPCGGraph>(RawSource);if(!Source||Source->GetClass()!=UPCGGraph::StaticClass())return Stop(TEXT("pcg_scoped_graph_pending"),TEXT("graph_instance"),TEXT("Graph"),TEXT("direct exact UPCGGraph"),TEXT("other/null graph"),false);
  if(!NativeHost){UClass* Class=Actor->GetClass();if(!Class->ClassGeneratedBy.IsResolved())return Stop(TEXT("pcg_scoped_unresolved_binding"),TEXT("actor_class"),TEXT("ClassGeneratedBy"),TEXT("resolved source Blueprint"),TEXT("unresolved"));UObject* RawBlueprint=Class->ClassGeneratedBy.Get();if(!Object(TEXT("source_blueprint"),RawBlueprint))return false;SourceBlueprint=Cast<UBlueprint>(RawBlueprint);if(!SourceBlueprint)return Stop(TEXT("pcg_scoped_blueprint_class"),TEXT("source_blueprint"),TEXT("class"),TEXT("UBlueprint"),TEXT("other object"));}
  for(const UObject* SourceObject:{static_cast<const UObject*>(Source),static_cast<const UObject*>(SourceBlueprint)}){if(NativeHost&&!SourceObject)continue;const FString Package=SourceObject->GetPackage()->GetName();if(!Condition(Package.StartsWith(TEXT("/Game/"))&&!Forbidden(Package),Package,TEXT("owned_source_scope"),TEXT("saved clean /Game source held read-only; exact authored/identity/file preservation required"),Package))return false;}
  TArray<UObject*> OrdinaryNodes;if(!Refs(Source,TEXT("Nodes"),OrdinaryNodes)||!SelectSourceNodes(OrdinaryNodes))return false;
  if(SpawnerProfile&&NativeHost)return Stop(TEXT("pcg_scoped_graph_pending"),TEXT("source_graph"),TEXT("weighted_host"),TEXT("existing direct Actor Blueprint resource host"),TEXT("native host"));
  if(RefreshMode&&SpawnerProfile)return Stop(TEXT("pcg_scoped_refresh_profile_pending"),TEXT("source_graph"),TEXT("settings"),TEXT("CPU CreatePoints only; managed-resource retirement remains incomplete"),TEXT("Weighted Spawner"),false);
  if(!Snapshot(Baseline))return false;Captured=true;if(NativeHost){W.R.Data->SetBoolField(TEXT("scoped_native_instance_host"),true);W.R.Data->SetStringField(TEXT("scoped_host_source_files"),NativeVolumeHost?TEXT("actual saved CPU graph; native volume brush is held authored map input, no fake mesh/SourceBlueprint file"):TEXT("actual saved CPU graph plus native host StaticMesh; no SourceBlueprint is synthesized"));}return true;
 }
 bool CleanupState(TSharedPtr<FJsonObject>& Out)
 {
  if(!W.Admit(TEXT("scoped_cleanup_state")))return false;Out=MakeShared<FJsonObject>();
  const bool Accessible=Component->AreManagedResourcesAccessible();int32 ResourceCount=0;
  if(Accessible){if(!W.SDK(TEXT("scoped_cleanup_managed_resources")))return false;Component->ForEachConstManagedResource([&](const UPCGManagedResource*){++ResourceCount;});if(!W.After(TEXT("scoped_cleanup_managed_resources")))return false;}
  Out->SetBoolField(TEXT("generated"),Component->bGenerated);Out->SetBoolField(TEXT("was_generated_this_session"),Component->WasGeneratedThisSession());Out->SetBoolField(TEXT("generating"),Component->IsGenerating());Out->SetBoolField(TEXT("cleaning_up"),Component->IsCleaningUp());Out->SetBoolField(TEXT("refresh_in_progress"),Component->IsRefreshInProgress());Out->SetBoolField(TEXT("managed_resources_accessible"),Accessible);Out->SetBoolField(TEXT("managed_resource_count_observed"),Accessible);Out->SetNumberField(TEXT("managed_resource_count"),ResourceCount);Out->SetBoolField(TEXT("component_package_dirty"),Component->GetPackage()->IsDirty());Out->SetBoolField(TEXT("actor_package_dirty"),Actor->GetPackage()->IsDirty());
  const auto& Output=Component->GetGeneratedGraphOutput();if(Output.TaggedData.Num()>128)return Stop(TEXT("pcg_scoped_cleanup_output_bound"),TEXT("component"),TEXT("output_count"),TEXT("<=128 tagged data"),FString::FromInt(Output.TaggedData.Num()));TArray<TSharedPtr<FJsonValue>> Rows;
  // No FPCGDataPtrWrapper copy or strong pin: the SDK may normally Rename/reset this output.
  for(const auto& Tagged:Output.TaggedData)
  {
   const auto Handle=Tagged.Data.GetObjectPtr();if(!Handle.IsResolved())return Stop(TEXT("pcg_scoped_cleanup_output_unresolved"),TEXT("component"),TEXT("output_handle"),TEXT("already resolved; no load"),TEXT("unresolved"));const UPCGData* Data=Handle.Get();if(Data&&!Live(Data))return Stop(TEXT("pcg_scoped_cleanup_output_unavailable"),TEXT("component"),TEXT("output_lifetime"),TEXT("live or null current output"),TEXT("unavailable"));
   auto Row=MakeShared<FJsonObject>();Row->SetBoolField(TEXT("is_null"),Data==nullptr);Row->SetStringField(TEXT("identity"),Data?ScopedObjectKey(Data):FString());Row->SetStringField(TEXT("path"),Data?Data->GetPathName():FString());Row->SetStringField(TEXT("class"),Data?Data->GetClass()->GetPathName():FString());Row->SetStringField(TEXT("outer"),Data&&Data->GetOuter()?Data->GetOuter()->GetPathName():FString());Row->SetStringField(TEXT("pin"),Tagged.Pin.ToString());TArray<FString> SortedTags=Tagged.Tags.Array();SortedTags.Sort();TArray<TSharedPtr<FJsonValue>> Tags;for(const FString& Tag:SortedTags)Tags.Add(MakeShared<FJsonValueString>(Tag));Row->SetArrayField(TEXT("tags"),Tags);Rows.Add(MakeShared<FJsonValueObject>(Row));
  }Out->SetNumberField(TEXT("generated_output_data_count"),Rows.Num());Out->SetArrayField(TEXT("output_identities"),Rows);return W.Admit(TEXT("scoped_cleanup_state_copied"));
 }
 bool CleanupInitialReady(TSharedPtr<FJsonObject>& Before)
 {
  if(!CleanupState(Before))return false;
  const bool Fresh=CompositeMode&&!Before->GetBoolField(TEXT("generated"))&&!Before->GetBoolField(TEXT("was_generated_this_session"));
   // A saved generated state can be restored without this session's generation.
    // Keep its actual observed history; source/owner/resource leases still apply.
    const bool PreviouslyGenerated=Before->GetBoolField(TEXT("generated"));
    const bool SessionHistory=Before->GetBoolField(TEXT("was_generated_this_session"));
   if(!Condition(Before->GetBoolField(TEXT("managed_resources_accessible"))&&(PreviouslyGenerated||Fresh)&&!Before->GetBoolField(TEXT("generating"))&&!Before->GetBoolField(TEXT("cleaning_up"))&&!Before->GetBoolField(TEXT("refresh_in_progress")),TEXT("component"),TEXT("cleanup_origin"),TEXT("idle existing generated state with captured session history, or never-generated empty bounded CPU composite"),TEXT("compound generation state")))return false;
   if(Fresh&&!Condition(Before->GetIntegerField(TEXT("managed_resource_count"))==0&&Before->GetIntegerField(TEXT("generated_output_data_count"))==0,TEXT("component"),TEXT("never_generated_empty"),TEXT("resource0/output0 before actual cleanup call"),TEXT("prior resource/output traces")))return false;
   W.R.Data->SetBoolField(TEXT("scoped_cleanup_started_never_generated"),Fresh);
    W.R.Data->SetStringField(TEXT("scoped_cleanup_origin"),Fresh?TEXT("never_generated_empty"):SessionHistory?TEXT("generated_this_session"):TEXT("existing_generated_no_session_history"));
   if(SpawnerProfile)
   {
    TArray<TSharedPtr<FJsonValue>> Rows;TArray<UActorComponent*> Generated;if(!ManagedRows(Rows,Generated,true))return false;
    if(Fresh)
    {
     // Empty selected-resource retirement does not authorize retiring survivors.
     // Prepare's full Weighted source/asset/owner lease is already captured; every
     // actor component must remain in the original survivor inventory until the
     // later accepted generation creates and publishes its own managed ISM.
     if(!Condition(Rows.IsEmpty()&&Generated.IsEmpty()&&ManagedBefore.IsEmpty()&&!PermanentComponents.IsEmpty(),TEXT("component"),TEXT("fresh_weighted_empty_capture"),TEXT("managed rows0/generated components0/held resources0 and captured nonempty survivors"),FString::Printf(TEXT("rows=%d generated=%d held=%d survivors=%d"),Rows.Num(),Generated.Num(),ManagedBefore.Num(),PermanentComponents.Num())))return false;
     Before->SetNumberField(TEXT("captured_managed_row_count"),Rows.Num());Before->SetNumberField(TEXT("captured_generated_component_count"),Generated.Num());Before->SetNumberField(TEXT("captured_held_managed_count"),ManagedBefore.Num());Before->SetNumberField(TEXT("captured_survivor_component_count"),PermanentComponents.Num());
    }
    ManagedBaseline=MakeShared<FJsonObject>();ManagedBaseline->SetArrayField(TEXT("resources"),Rows);Before->SetArrayField(TEXT("managed_resource_details"),Rows);return true;
   }
   const int32 Resources=Before->GetIntegerField(TEXT("managed_resource_count"));return Resources==0||Stop(TEXT("pcg_scoped_cleanup_resource_profile_pending"),TEXT("component"),TEXT("managed_resource_count"),TEXT("0 for CPU CreatePoints; constructed resource cleanup incomplete"),FString::FromInt(Resources));
 }
 bool CleanupPostcondition(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,bool Remove)
 {
  if(!Before)return Stop(TEXT("pcg_scoped_cleanup_history_unavailable"),TEXT("component"),TEXT("session_history"),TEXT("captured pre-cleanup state"),TEXT("missing"));
  const bool ExpectedSession=Before->GetBoolField(TEXT("was_generated_this_session"));
  if(After->GetBoolField(TEXT("was_generated_this_session"))!=ExpectedSession)return Stop(TEXT("pcg_scoped_cleanup_session_changed"),TEXT("component"),TEXT("was_generated_this_session"),ExpectedSession?TEXT("true"):TEXT("false"),After->GetBoolField(TEXT("was_generated_this_session"))?TEXT("true"):TEXT("false"));
  if(!After->GetBoolField(TEXT("managed_resources_accessible"))||After->GetBoolField(TEXT("generating"))||After->GetBoolField(TEXT("cleaning_up"))||After->GetBoolField(TEXT("refresh_in_progress"))||After->GetIntegerField(TEXT("managed_resource_count"))!=0)return Stop(TEXT("pcg_scoped_cleanup_not_complete"),TEXT("component"),TEXT("idle/resources/session"),TEXT("idle accessible resource0; original captured session history retained"),TEXT("compound cleanup state"),false);
  if(After->GetBoolField(TEXT("generated"))!=!Remove)return Stop(TEXT("pcg_scoped_cleanup_generated_mismatch"),TEXT("component"),TEXT("bGenerated"),Remove?TEXT("false"):TEXT("true"),After->GetBoolField(TEXT("generated"))?TEXT("true"):TEXT("false"));
  if(Remove)return After->GetIntegerField(TEXT("generated_output_data_count"))==0||Stop(TEXT("pcg_scoped_cleanup_output_not_cleared"),TEXT("component"),TEXT("generated_output_data_count"),TEXT("0"),FString::FromInt(After->GetIntegerField(TEXT("generated_output_data_count"))));
  if(Before){FString Path,Want,Got;if(ScopedDifference(Before->TryGetField(TEXT("output_identities")),After->TryGetField(TEXT("output_identities")),Path,Want,Got,true))return Stop(TEXT("pcg_scoped_cleanup_shallow_output_changed"),TEXT("component"),Path,Want,Got);}
  return true;
 }
 bool InitialReady(bool AfterCleanup=false,bool ExpectedCleanupSession=true)
 {
  if(!Condition(!Component->bGenerated&&(Component->WasGeneratedThisSession()==(AfterCleanup?ExpectedCleanupSession:false))&&!Component->IsGenerating()&&!Component->IsCleaningUp()&&!Component->IsRefreshInProgress()&&Component->GetGeneratedGraphOutput().TaggedData.IsEmpty()&&Component->AreManagedResourcesAccessible(),TEXT("component"),AfterCleanup?TEXT("after_completed_cleanup"):TEXT("first_generation"),AfterCleanup?TEXT("completed own cleanup, idle, no output, resources accessible; captured true/false session history retained"):TEXT("never generated, idle, no output, resources accessible"),TEXT("compound generation precondition")))return false;
  int32 Resources=0;if(!W.SDK(TEXT("scoped_initial_managed_resources")))return false;Component->ForEachConstManagedResource([&](const UPCGManagedResource*){++Resources;});if(!W.After(TEXT("scoped_initial_managed_resources")))return false;if(!Condition(Resources==0,TEXT("component"),TEXT("managed_resources"),TEXT("0 before first generation"),FString::FromInt(Resources)))return false;
  if(!W.SDK(TEXT("scoped_GetSubsystem")))return false;UPCGSubsystem* Subsystem=Component->GetSubsystem();TStrongObjectPtr<UPCGSubsystem> Returned(Subsystem);if(!W.After(TEXT("scoped_GetSubsystem")))return false;return Subsystem?W.Pin(Subsystem):Stop(TEXT("pcg_scoped_subsystem_unavailable"),TEXT("component"),TEXT("subsystem"),TEXT("live Editor world PCG subsystem"),TEXT("null"));
 }
};

// AEO: request-local telemetry only. No engine task is cancelled by this registry.
// Native generated broadcasts after CurrentGenerationTask is cleared; retain the
// task observed by OnStart. The public SDK has no refresh-task ID accessor.
struct FRefreshOperation
{
 FString Token,Identity,Status=TEXT("pending"),Reason,GenerationTask;
 TWeakObjectPtr<UPCGComponent> Component;TWeakObjectPtr<AActor> Owner;TWeakObjectPtr<UWorld> World;
 TSharedPtr<FJsonObject> Proof;FDelegateHandle StartHandle,GeneratedHandle,AbortHandle;
 TAtomic<bool> OffThread{false};double Expires=0;bool Committed=false,Started=false,Generated=false,Aborted=false,Verified=false;int32 Starts=0;
 void Unknown(const TCHAR* Why){Status=TEXT("unknown");Reason=Why;}
 bool Roles(UPCGComponent* C)const{return Live(GEditor)&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext()&&GEditor->GetEditorWorldContext().World()==World.Get()&&Live(C)&&Component.Get()==C&&Live(Owner.Get())&&Live(World.Get())&&C->GetOwner()==Owner.Get()&&C->GetWorld()==World.Get()&&Owner->GetWorld()==World.Get()&&!World->bIsTearingDown;}
 void Event(UPCGComponent* C,int32 Kind)
 {
  // Unexpected worker-thread signals carry no UObject reads or GT registry access.
  if(!IsInGameThread()){OffThread.Store(true);return;}
  if(!Safe()||!Roles(C)){Unknown(TEXT("event_context_or_target_lost"));return;}
  if(Verified){Unknown(TEXT("event_after_verified_terminal"));return;}
  if(Status==TEXT("unknown"))return;
  if(!Committed){Unknown(TEXT("event_before_refresh_dispatch_return"));return;}
  if(Kind==0)
  {
   ++Starts;if(Starts!=1||Generated||Aborted||C->IsRefreshInProgress()||C->GetGenerationTaskId()==InvalidPCGTaskId){Unknown(TEXT("ambiguous_or_reentrant_generation_start"));return;}
   Started=true;GenerationTask=LexToString(C->GetGenerationTaskId());
  }
  else if(Kind==1)
  {
   Generated=true;if(!Started||Starts!=1||Aborted){Unknown(TEXT("unmatched_or_conflicting_generated_event"));return;}Status=TEXT("succeeded");Reason=TEXT("same_observed_generation_start_and_generated_event");
  }
  else
  {
   Aborted=true;if(!Started||Starts!=1||Generated){Unknown(TEXT("unmatched_or_conflicting_abort_event"));return;}Status=TEXT("aborted");Reason=TEXT("native_cancelled_or_aborted_event_after_start");
  }
 }
 void Detach()
 {
  check(IsInGameThread());if(auto* C=Component.Get()){C->OnPCGGraphStartGeneratingDelegate.Remove(StartHandle);C->OnPCGGraphGeneratedDelegate.Remove(GeneratedHandle);C->OnPCGGraphCancelledDelegate.Remove(AbortHandle);}StartHandle.Reset();GeneratedHandle.Reset();AbortHandle.Reset();
 }
 void ObserveIdle(UPCGComponent* C)
 {
  if(!IsInGameThread()||!Safe()){Unknown(TEXT("query_context_lost"));return;}
  if(OffThread.Load())Unknown(TEXT("off_thread_lifecycle_event"));
  if(!Roles(C)){Unknown(TEXT("query_target_lost"));return;}
  if(!C->IsGenerating()&&!C->IsRefreshInProgress()&&!C->IsCleaningUp()&&Status==TEXT("pending"))Unknown(TEXT("idle_without_request_terminal_event"));
  if((Status==TEXT("succeeded")||Status==TEXT("aborted"))&&(C->IsGenerating()||C->IsRefreshInProgress()||C->IsCleaningUp()))Unknown(TEXT("new_or_remaining_task_after_terminal_event"));
 }
 TSharedRef<FJsonObject> Json(bool Query)const
 {
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("token"),Token);J->SetStringField(TEXT("status"),OffThread.Load()?TEXT("unknown"):*Status);J->SetStringField(TEXT("component_identity"),Identity);J->SetBoolField(TEXT("query_only"),Query);J->SetStringField(TEXT("refresh_task_id"),TEXT(""));J->SetBoolField(TEXT("refresh_task_id_available"),false);J->SetStringField(TEXT("generation_task_id"),GenerationTask);J->SetBoolField(TEXT("started_observed"),Started);J->SetBoolField(TEXT("generated_event_observed"),Generated);J->SetBoolField(TEXT("cancelled_or_aborted_event_observed"),Aborted);J->SetStringField(TEXT("reason"),OffThread.Load()?TEXT("off_thread_lifecycle_event"):*Reason);J->SetBoolField(TEXT("subscriptions_active"),StartHandle.IsValid()||GeneratedHandle.IsValid()||AbortHandle.IsValid());J->SetBoolField(TEXT("terminal_observation_verified"),Verified);return J;
 }
};
using FRefreshPtr=TSharedPtr<FRefreshOperation,ESPMode::ThreadSafe>;
TMap<FString,FRefreshPtr> RefreshOperations;
FTSTicker::FDelegateHandle RefreshTrackingTicker;FDelegateHandle RefreshTrackingPreExit;
void StopRefreshTracking()
{
 if(!IsInGameThread())return;FTSTicker::RemoveTicker(RefreshTrackingTicker);RefreshTrackingTicker.Reset();FCoreDelegates::OnEnginePreExit.Remove(RefreshTrackingPreExit);RefreshTrackingPreExit.Reset();for(auto& Pair:RefreshOperations)Pair.Value->Detach();RefreshOperations.Empty();
}
void PruneRefreshTracking()
{
 check(IsInGameThread());if(!Safe())return;const double Now=FPlatformTime::Seconds();TArray<FString> Remove;
 for(auto& Pair:RefreshOperations){auto& Op=*Pair.Value;if(Op.Expires<=Now){Op.Detach();Remove.Add(Pair.Key);}else if(Op.OffThread.Load()||!Op.Roles(Op.Component.Get())){Op.Unknown(TEXT("tracking_context_or_target_lost"));Op.Detach();}}
 for(const auto& Token:Remove)RefreshOperations.Remove(Token);
}
void PublishRefreshTracking(FWork& W,const FRefreshPtr& Op,bool Query)
{
 auto J=Op->Json(Query);J->SetNumberField(TEXT("registry_operation_count"),RefreshOperations.Num());int32 Pending=0;for(const auto& Pair:RefreshOperations)if(!Pair.Value->Verified&&Pair.Value->Status==TEXT("pending"))++Pending;J->SetNumberField(TEXT("registry_pending_count"),Pending);W.R.Data->SetObjectField(TEXT("scoped_refresh_operation"),J);
}
void EnsureRefreshTracking()
{
 if(!RefreshTrackingTicker.IsValid())RefreshTrackingTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){if(IsInGameThread())PruneRefreshTracking();return true;}),1.0f);
 if(!RefreshTrackingPreExit.IsValid())RefreshTrackingPreExit=FCoreDelegates::OnEnginePreExit.AddStatic(&StopRefreshTracking);
}
FRefreshPtr BeginRefreshTracking(FWork& W,UPCGComponent* C,const TSharedPtr<FJsonObject>& Proof)
{
 PruneRefreshTracking();
 for(const auto& Pair:RefreshOperations)if(Pair.Value->Component.Get()==C&&!Pair.Value->Verified){W.Fail(TEXT("pcg_scoped_refresh_tracking_active"),TEXT("A prior request-local refresh observation remains active; no new refresh dispatched"));return {};}
 if(RefreshOperations.Num()>=64){TArray<FString> Evict;for(const auto& Pair:RefreshOperations)if(Pair.Value->Verified)Evict.Add(Pair.Key);for(const auto& Key:Evict){RefreshOperations[Key]->Detach();RefreshOperations.Remove(Key);if(RefreshOperations.Num()<64)break;}}
 if(RefreshOperations.Num()>=64){W.Fail(TEXT("pcg_scoped_refresh_tracking_budget"),TEXT("Request-local refresh telemetry capacity reached; no writer dispatched"));return {};}
 auto Op=MakeShared<FRefreshOperation,ESPMode::ThreadSafe>();Op->Token=FGuid::NewGuid().ToString(EGuidFormats::Digits);Op->Identity=ScopedObjectKey(C);Op->Component=C;Op->Owner=C->GetOwner();Op->World=C->GetWorld();Op->Proof=Proof;Op->Expires=FPlatformTime::Seconds()+600.0;
 const TWeakPtr<FRefreshOperation,ESPMode::ThreadSafe> Weak=Op;
 Op->StartHandle=C->OnPCGGraphStartGeneratingDelegate.AddLambda([Weak](UPCGComponent* Target){if(auto Held=Weak.Pin())Held->Event(Target,0);});
 Op->GeneratedHandle=C->OnPCGGraphGeneratedDelegate.AddLambda([Weak](UPCGComponent* Target){if(auto Held=Weak.Pin())Held->Event(Target,1);});
 Op->AbortHandle=C->OnPCGGraphCancelledDelegate.AddLambda([Weak](UPCGComponent* Target){if(auto Held=Weak.Pin())Held->Event(Target,2);});
 RefreshOperations.Add(Op->Token,Op);EnsureRefreshTracking();return Op;
}
FRefreshPtr FindRefreshTracking(FWork& W,const TSharedPtr<FJsonObject>& P,UPCGComponent* C,const TSharedPtr<FJsonObject>& Proof)
{
 PruneRefreshTracking();FString Token;FGuid Parsed;
 if(!P->TryGetStringField(TEXT("_scoped_refresh_operation"),Token)||Token.Len()!=32||!FGuid::ParseExact(Token,EGuidFormats::Digits,Parsed)){W.Fail(TEXT("pcg_scoped_refresh_outcome_unknown"),TEXT("Refresh continuation requires its exact request-local operation token; task history alone is not completion"));return {};}
 const auto* Found=RefreshOperations.Find(Token);if(!Found||!(*Found)->Roles(C)||(*Found)->Identity!=ScopedObjectKey(C)){W.Fail(TEXT("pcg_scoped_refresh_outcome_unknown"),TEXT("Refresh telemetry expired, target changed or token not observed; no success/cancellation inferred"));return {};}
 FString Path,Want,Got;if(ScopedDifference(MakeShared<FJsonValueObject>((*Found)->Proof),MakeShared<FJsonValueObject>(Proof),Path,Want,Got,true)){W.Fail(TEXT("pcg_scoped_refresh_outcome_unknown"),TEXT("Request-local refresh source proof changed: ")+Path);return {};}
 return *Found;
}
struct FRefreshReceipt
{
 FWork& W;FRefreshPtr Op;bool Query=false,Completed=false;
 ~FRefreshReceipt(){if(Op){if(!Completed&&!Query)Op->Unknown(TEXT("refresh_dispatch_or_postcondition_failed"));PublishRefreshTracking(W,Op,Query);}}
};

TSharedPtr<FJsonObject> HandleRefresh(FWork& W,const TSharedPtr<FJsonObject>& P)
{
 FRefreshReceipt RefreshReceipt{W};
 if(!W.World.IsValid())return W.Error(TEXT("Failed to get editor world"));
 const bool Selected=Bool(P,TEXT("selected_only"),false),Cleanup=Bool(P,TEXT("cleanup"),false),Immediate=Bool(P,TEXT("immediate_cleanup"),true),Remove=Bool(P,TEXT("remove_components"),true),Notify=Bool(P,TEXT("notify_properties_changed"),false),Refresh=Bool(P,TEXT("refresh"),false),Cancel=Bool(P,TEXT("cancel_existing_refresh"),true),Generate=Bool(P,TEXT("generate"),true),Force=Bool(P,TEXT("force"),true),Wait=Bool(P,TEXT("wait_until_complete"),false);
 const bool HasGenerateExpectation=P->HasField(TEXT("_scoped_generate_expectation")),HasCleanupExpectation=P->HasField(TEXT("_scoped_cleanup_expectation")),HasCompositeExpectation=P->HasField(TEXT("_scoped_cleanup_generate_expectation"));
 const bool HasExpectation=HasGenerateExpectation||HasCleanupExpectation||HasCompositeExpectation;if(int32(HasGenerateExpectation)+int32(HasCleanupExpectation)+int32(HasCompositeExpectation)>1)return W.Error(TEXT("Only one scoped continuation proof is permitted"));TSharedPtr<FJsonObject> Expectation;bool CleanupRemove=Remove;
 if(HasCleanupExpectation&&!P->TryGetBoolField(TEXT("_scoped_cleanup_remove_components"),CleanupRemove))return W.Error(TEXT("Scoped cleanup continuation requires an exact bool remove-components mode"));
 bool CleanupExpectedSession=true;if(HasCleanupExpectation&&P->HasField(TEXT("_scoped_cleanup_session_history"))&&!P->TryGetBoolField(TEXT("_scoped_cleanup_session_history"),CleanupExpectedSession))return W.Error(TEXT("Scoped cleanup continuation session history must be an exact bool"));
 if(HasExpectation){const TSharedPtr<FJsonObject>* E=nullptr;if(!P->TryGetObjectField(HasCompositeExpectation?TEXT("_scoped_cleanup_generate_expectation"):HasCleanupExpectation?TEXT("_scoped_cleanup_expectation"):TEXT("_scoped_generate_expectation"),E)||!E||!E->IsValid()||Cleanup||Generate||Refresh||Force||Notify)return W.Error(TEXT("Internal scoped continuation requires a proof object and cleanup/generate/refresh/force/notify all false"));Expectation=*E;}
 const bool ScopedRefresh=(Notify!=Refresh)&&!Cleanup&&!Generate;
 const bool ScopedInitial=Generate&&!Cleanup&&!Notify&&!Refresh;const bool ScopedCleanup=Cleanup&&Immediate&&!Generate&&!Notify&&!Refresh;const bool ScopedComposite=Cleanup&&Immediate&&Generate&&!Notify&&!Refresh;const bool CleanupScope=ScopedCleanup||HasCleanupExpectation,CompositeScope=ScopedComposite||HasCompositeExpectation;const bool ScopedRequest=ScopedInitial||ScopedRefresh||CleanupScope||CompositeScope||HasExpectation;
 TSharedPtr<FScopedGeneration> Scope;if(ScopedRequest){Scope=MakeShared<FScopedGeneration>(W);Scope->CleanupMode=CleanupScope||CompositeScope;Scope->CompositeMode=CompositeScope;Scope->RefreshMode=ScopedRefresh;}if(Scope&&Expectation&&!Scope->ExpectShape(Expectation)){Scope->Stop(TEXT("pcg_scoped_expectation_invalid"),TEXT("expectation"),TEXT("shape"),TEXT("finite scoped source proof"),TEXT("invalid"),false);return W.Error(W.R.Error);}
 double Timeout=10.0,Poll=.05;if(P->HasField(TEXT("timeout_seconds")))Timeout=P->GetNumberField(TEXT("timeout_seconds"));if(P->HasField(TEXT("poll_interval_seconds")))Poll=P->GetNumberField(TEXT("poll_interval_seconds"));Timeout=FMath::Clamp(Timeout,0.,300.);Poll=FMath::Clamp(Poll,.005,1.);int32 MinOutput=-1,MinResource=-1,Max=1000;if(!Integer(W,P,TEXT("min_generated_output_data_count"),-1,MinOutput)||!Integer(W,P,TEXT("min_managed_resource_count"),-1,MinResource)||!Integer(W,P,TEXT("max_components"),1000,Max))return W.Error(W.R.Error);Max=FMath::Max(1,Max);
 TArray<AActor*> Actors;auto Add=[&](AActor* A){if(Live(A)&&MatchActor(A,P))Actors.AddUnique(A);};if(Selected){USelection* Selection=GEditor->GetSelectedActors();if(Selection){if(!W.Pin(Selection))return W.Error(W.R.Error);for(FSelectionIterator It(*Selection);It;++It){if(!W.Budget(1))return W.Error(W.R.Error);Add(Cast<AActor>(*It));}}}else{for(TActorIterator<AActor> It(W.World.Get());It;++It){if(!W.Budget(1))return W.Error(W.R.Error);Add(*It);}}
 if(!W.Budget(Actors.Num()))return W.Error(W.R.Error);TArray<AActor*> IncludedActors;TArray<UPCGComponent*> Targets;TArray<TSharedPtr<FJsonValue>> InitialRows;bool Limit=false;const FString CName=Str(P,TEXT("component_name")),CTag=Str(P,TEXT("component_tag"));
 // Capture the entire selected target set before any callback/writer. Do not iterate live actor/component arrays across SDK dispatch.
 for(auto* A:Actors){if(!W.Pin(A)||A->GetWorld()!=W.World.Get())return W.Error(TEXT("Selected actor not in exact Editor world"));TArray<UPCGComponent*> Cs;A->GetComponents<UPCGComponent>(Cs);if(!W.Budget(Cs.Num()))return W.Error(W.R.Error);for(auto* C:Cs){if(!Live(C))continue;if(!CName.IsEmpty()&&!C->GetName().Equals(CName,ESearchCase::IgnoreCase))continue;if(!CTag.IsEmpty()&&!C->ComponentTags.Contains(FName(*CTag)))continue;if(Targets.Num()>=Max){Limit=true;break;}if(!W.Pin(C)||C->GetOwner()!=A||C->GetWorld()!=W.World.Get())return W.Error(TEXT("PCG component ownership/world unavailable"));auto* Instance=ScopedRequest?Cast<UPCGGraphInterface>(Scope->Ref(C,TEXT("GraphInstance"))):C->GetGraphInstance();TStrongObjectPtr<UPCGGraphInterface> ReturnedInstance(Instance);if(!W.R.Error.IsEmpty()||(Instance&&!W.Pin(Instance)))return W.Error(W.R.Error);W.Components.Add({C,A,W.World.Get(),Instance});Targets.Add(C);IncludedActors.AddUnique(A);}if(Limit)break;}
 auto O=MakeShared<FJsonObject>();W.R.Data->SetObjectField(TEXT("legacy_result"),O);O->SetNumberField(TEXT("matched_actor_count"),Actors.Num());O->SetNumberField(TEXT("pcg_actor_count"),IncludedActors.Num());O->SetNumberField(TEXT("component_count"),Targets.Num());O->SetNumberField(TEXT("cleanup_count"),0);O->SetNumberField(TEXT("notify_properties_changed_count"),0);O->SetNumberField(TEXT("refresh_count"),0);O->SetNumberField(TEXT("generate_count"),0);O->SetBoolField(TEXT("component_limit_hit"),Limit);
 const bool ScopedActive=HasExpectation||((ScopedInitial||ScopedCleanup||ScopedComposite||ScopedRefresh)&&!Targets.IsEmpty());TArray<TSharedPtr<FJsonValue>> SourceFiles;TSharedPtr<FJsonObject> CleanupBefore,CleanupAfter;
 if(ScopedActive){auto Admission=MakeShared<FJsonObject>();Admission->SetNumberField(TEXT("matched_actor_count"),Actors.Num());Admission->SetNumberField(TEXT("included_actor_count"),IncludedActors.Num());Admission->SetNumberField(TEXT("component_count"),Targets.Num());Admission->SetBoolField(TEXT("component_limit_hit"),Limit);W.R.Data->SetObjectField(CompositeScope?TEXT("scoped_cleanup_generate_target_admission"):CleanupScope?TEXT("scoped_cleanup_target_admission"):TEXT("scoped_generate_target_admission"),Admission);
  // ADY: original filters are conjunctive, not mutually exclusive. Keep the
   // existing prefix path (including an exact-label intersection), and admit a
   // separate exact-label-only path with the same whole-match/ownership proof.
   const FString ExactLabel=Str(P,TEXT("actor_label")),LabelPrefix=Str(P,TEXT("actor_label_prefix"));
   const bool PrefixSelection=!LabelPrefix.IsEmpty(),ExactLabelOnly=!ExactLabel.IsEmpty()&&!PrefixSelection;
   Admission->SetStringField(TEXT("selector_mode"),PrefixSelection?(ExactLabel.IsEmpty()?TEXT("label_prefix"):TEXT("label_exact_and_prefix_intersection")):ExactLabelOnly?TEXT("label_exact"):TEXT("unsupported_scoped_selector"));
   Admission->SetStringField(TEXT("actor_label"),ExactLabel);Admission->SetStringField(TEXT("actor_label_prefix"),LabelPrefix);Admission->SetBoolField(TEXT("selected_only"),Selected);
   if(!Scope->Condition(!Selected&&(PrefixSelection||ExactLabelOnly)&&Actors.Num()==1&&IncludedActors.Num()==1&&Targets.Num()==1&&!Limit,TEXT("target_selection"),TEXT("unique_whole_match"),TEXT("nonempty exact label or prefix matches one Actor and exactly one PCG component, selected_only=false and no truncation"),FString::Printf(TEXT("actors=%d included=%d components=%d limit=%d selected=%d exact=%d prefix=%d"),Actors.Num(),IncludedActors.Num(),Targets.Num(),Limit,Selected,!ExactLabel.IsEmpty(),PrefixSelection)))return W.Error(W.R.Error);
  if(ScopedComposite&&!Scope->Condition(Remove,TEXT("request"),TEXT("cleanup_generate_modes"),TEXT("remove_components=true before any cleanup/generate writer; requested force is forwarded only after observed hard cleanup"),FString::Printf(TEXT("remove=%d force=%d"),Remove,Force)))return W.Error(W.R.Error);
  Scope->GenerationAuthorized=HasGenerateExpectation||HasCompositeExpectation;if(!Scope->Prepare(Targets[0],IncludedActors[0],Expectation))return W.Error(W.R.Error);if(Scope->RefreshMode&&!Scope->Condition(!Scope->SpawnerProfile&&Targets[0]->bRegenerateInEditor,TEXT("component"),TEXT("refresh_profile"),TEXT("CPU CreatePoints only, editor regeneration enabled; resource retirement/custom cancellation remains incomplete"),TEXT("profile/property")))return W.Error(W.R.Error);if((ScopedCleanup||ScopedComposite)&&Scope->SpawnerProfile&&!Remove){Scope->Stop(TEXT("pcg_scoped_soft_resource_cleanup_pending"),TEXT("component"),TEXT("remove_components"),TEXT("true for native ISM resource cleanup; soft cleanup remains valid incomplete"),TEXT("false"),false);return W.Error(W.R.Error);}W.ScopedSourceGraph=Scope->Source;W.ScopedCurrent=[Scope]{return Scope->Current();};
  if(!W.Admit(TEXT("scoped_source_captured"))||!Scope->Files(SourceFiles))return W.Error(W.R.Error);
  auto Proof=MakeShared<FJsonObject>(*Scope->Baseline);Proof->SetArrayField(TEXT("source_files"),SourceFiles);
  if(Expectation){FString Path,Want,Got;if(ScopedDifference(MakeShared<FJsonValueObject>(Expectation),MakeShared<FJsonValueObject>(Proof),Path,Want,Got,CleanupScope||CompositeScope||Scope->SpawnerProfile)){Scope->Stop(TEXT("pcg_scoped_expectation_changed"),TEXT("scoped_generate_source"),Path,Want,Got);return W.Error(W.R.Error);}}
  if(CompositeScope){W.R.Data->SetBoolField(TEXT("scoped_cleanup_generate"),true);W.R.Data->SetBoolField(TEXT("scoped_cleanup_generate_query_only"),HasCompositeExpectation);W.R.Data->SetObjectField(TEXT("scoped_cleanup_generate_source_before"),Proof);W.R.Data->SetBoolField(TEXT("PCG_cleanup_completed"),false);W.R.Data->SetBoolField(TEXT("PCG_generation_completed"),false);W.R.Data->SetStringField(TEXT("scoped_cleanup_generate_scope"),TEXT("Own bounded native CPU point-pipeline or finite stock Weighted Spawner, Actor1/component1: completed hard immediate cleanup from prior generation or an empty never-generated bounded CPU CreatePoints/Weighted target, then requested force=true/false task enqueue only after generated=false/output0/resource0/idle is observed. Selected managed native ISM/HISM roles retire before cleanup; source and nonselected owners remain guarded. Named queries are readonly; enqueue is not completion. Other resources, soft/non-immediate cleanup, custom callbacks, partition/runtime/GPU, save and replay remain incomplete."));}
  else if(CleanupScope){W.R.Data->SetBoolField(TEXT("scoped_immediate_cleanup_only"),true);W.R.Data->SetBoolField(TEXT("scoped_cleanup_query_only"),HasCleanupExpectation);W.R.Data->SetObjectField(TEXT("scoped_cleanup_source_before"),Proof);W.R.Data->SetBoolField(TEXT("PCG_generation_completed"),false);}
  else{W.R.Data->SetBoolField(Scope->RefreshMode?TEXT("scoped_refresh_only"):TEXT("scoped_generate_only"),true);W.R.Data->SetObjectField(TEXT("scoped_generate_source"),Proof);W.R.Data->SetBoolField(TEXT("scoped_generate_query_only"),HasExpectation);W.R.Data->SetStringField(TEXT("scoped_generation_proof_scope"),TEXT("Current request + copied Named continuation identity/authored/native graph roles and captured saved source files only. Does not prove global callback purity, runtime/partitioned/GPU/cleanup/refresh/regeneration, every Blueprint template field, or cold-reload identity."));}
  if((ScopedCleanup||ScopedComposite)&&!Scope->CleanupInitialReady(CleanupBefore))return W.Error(W.R.Error);if(CleanupBefore)W.R.Data->SetObjectField(TEXT("scoped_cleanup_before"),CleanupBefore);
  if(ScopedInitial&&!Scope->InitialReady())return W.Error(W.R.Error);
  if(P->HasField(TEXT("_scoped_refresh_operation"))&&!(HasExpectation&&Scope->RefreshMode))return W.Error(TEXT("Refresh operation token is permitted only on a readonly refresh proof continuation"));
  if(HasExpectation&&Scope->RefreshMode){RefreshReceipt.Query=true;RefreshReceipt.Op=FindRefreshTracking(W,P,Targets[0],Proof);if(!RefreshReceipt.Op)return W.Error(W.R.Error);}
   if(ScopedRefresh)
   {
    TSharedPtr<FJsonObject> BeforeRefresh;if(!Scope->CleanupInitialReady(BeforeRefresh))return W.Error(W.R.Error);W.R.Data->SetObjectField(TEXT("scoped_refresh_before"),BeforeRefresh);
    W.R.Data->SetStringField(TEXT("scoped_refresh_scope"),TEXT("One NotifyPropertiesChangedFromBlueprint OR one DirtyGenerated+Refresh on an idle existing-generated CPU CreatePoints target (captured current session history may be false after reload) with zero managed resources; read-only source proof and future observations reused. No combined writer sequence, partition/runtime/GPU/multi-target or in-flight cancellation support."));
    RefreshReceipt.Op=BeginRefreshTracking(W,Targets[0],Proof);if(!RefreshReceipt.Op)return W.Error(W.R.Error);
   }
 }
 for(auto* C:Targets){auto BeforeJson=ComponentJson(W,C);W.ComponentBefore.Add(MakeShared<FJsonValueObject>(BeforeJson));if(!W.Admit(TEXT("refresh_before_projection")))return W.Error(W.R.Error);}W.R.Data->SetArrayField(TEXT("before"),W.ComponentBefore);W.R.Data->SetNumberField(TEXT("selected_component_count"),Targets.Num());

 // Existing PCG GC/construction gate is specific to requested PCG effects, not to pure observation or K2-only authoring.
 if(!ScopedActive&&!Targets.IsEmpty()&&(Cleanup||Notify||Refresh||Generate)&&!W.RequireConstruction(TEXT("cleanup_notify_refresh_generate")))return W.Error(W.R.Error);
 int32 Cleanups=0,Notifies=0,Refreshes=0,Generates=0;
 for(auto* C:Targets)
 {
  W.Mutation->SetStringField(TEXT("component_path"),C->GetPathName());W.Mutation->SetStringField(TEXT("actor_path"),C->GetOwner()->GetPathName());
  if(Cleanup){if(ScopedCleanup||ScopedComposite){auto Dispatch=MakeShared<FJsonObject>();Dispatch->SetStringField(TEXT("api"),TEXT("CleanupLocalImmediate(bool,bool)"));Dispatch->SetBoolField(TEXT("remove_components"),Remove);Dispatch->SetBoolField(TEXT("cleanup_local_components"),true);Dispatch->SetBoolField(TEXT("entered"),false);Dispatch->SetBoolField(TEXT("returned"),false);Dispatch->SetBoolField(TEXT("completion_proven"),false);W.R.Data->SetObjectField(TEXT("scoped_cleanup_dispatch"),Dispatch);
   // Fresh cleanup still runs its original callback-bearing SDK, never a skipped no-op.
    // Recheck first-generation readiness before entry and direct task flags after
    // the last admission callback, so this slice cannot cancel an existing task.
    const bool FreshCleanup=W.R.Data->GetBoolField(TEXT("scoped_cleanup_started_never_generated"));
    if(FreshCleanup&&!Scope->InitialReady())return W.Error(W.R.Error);
    if(!W.SDK(TEXT("CleanupLocalImmediate"),true))return W.Error(W.R.Error);
    if(FreshCleanup&&!Scope->Condition(!C->bGenerated&&!C->WasGeneratedThisSession()&&!C->IsGenerating()&&!C->IsCleaningUp()&&!C->IsRefreshInProgress()&&C->AreManagedResourcesAccessible()&&C->GetGeneratedGraphOutput().TaggedData.IsEmpty(),C->GetPathName(),TEXT("fresh_cleanup_dispatch_state"),TEXT("same never-generated idle empty target after final admission callback; no existing task cancellation"),TEXT("changed live state")))return W.Error(W.R.Error);
    Dispatch->SetBoolField(TEXT("entered"),true);if(Scope->SpawnerProfile)Scope->RetireManaged();C->CleanupLocalImmediate(Remove,true);Dispatch->SetBoolField(TEXT("returned"),true);++Cleanups;O->SetNumberField(TEXT("cleanup_count"),Cleanups);if(!W.After(TEXT("CleanupLocalImmediate")))return W.Error(W.R.Error);
   if(!Scope->CleanupState(CleanupAfter))return W.Error(W.R.Error);W.R.Data->SetObjectField(TEXT("scoped_cleanup_after"),CleanupAfter);// Count only the actual state returned by CleanupState on both sides.
    // Captured proof/count/details added to Before remain in the raw receipt,
    // but their absence from After is not an engine mutation.
    auto BeforeState=MakeShared<FJsonObject>();auto AfterState=MakeShared<FJsonObject>();
    for(const TCHAR* Key:{TEXT("generated"),TEXT("was_generated_this_session"),TEXT("generating"),TEXT("cleaning_up"),TEXT("refresh_in_progress"),TEXT("managed_resources_accessible"),TEXT("managed_resource_count_observed"),TEXT("managed_resource_count"),TEXT("component_package_dirty"),TEXT("actor_package_dirty"),TEXT("generated_output_data_count"),TEXT("output_identities")})
    {
     const auto BeforeValue=CleanupBefore->TryGetField(Key),AfterValue=CleanupAfter->TryGetField(Key);
     if(!BeforeValue||!AfterValue){Scope->Stop(TEXT("pcg_scoped_cleanup_state_missing"),C->GetPathName(),Key,TEXT("actual common CleanupState field on both sides"),TEXT("missing"));return W.Error(W.R.Error);}
     BeforeState->SetField(Key,BeforeValue);AfterState->SetField(Key,AfterValue);
    }
    FString DifferencePath,DifferenceWant,DifferenceGot;W.R.bChanged|=ScopedDifference(MakeShared<FJsonValueObject>(BeforeState),MakeShared<FJsonValueObject>(AfterState),DifferencePath,DifferenceWant,DifferenceGot,true);
   if(!Scope->CleanupPostcondition(CleanupBefore,CleanupAfter,Remove))return W.Error(W.R.Error);Dispatch->SetBoolField(TEXT("completion_proven"),true);W.R.Data->SetBoolField(TEXT("PCG_cleanup_completed"),true);W.R.Data->SetBoolField(TEXT("scoped_cleanup_state_ready"),true);W.R.Data->SetBoolField(TEXT("cleanup_selected_output_cleared_verified"),Remove);W.R.Data->SetBoolField(TEXT("cleanup_prior_output_reread_performed"),false);}
   else if(!W.Write(Immediate?TEXT("CleanupLocalImmediate"):TEXT("CleanupLocal"),[&]{if(Immediate)C->CleanupLocalImmediate(Remove,true);else C->CleanupLocal(Remove);++Cleanups;O->SetNumberField(TEXT("cleanup_count"),Cleanups);}))return W.Error(W.R.Error);}
  if(Notify){if(!W.Write(TEXT("NotifyPropertiesChangedFromBlueprint"),[&]{C->NotifyPropertiesChangedFromBlueprint();++Notifies;O->SetNumberField(TEXT("notify_properties_changed_count"),Notifies);}))return W.Error(W.R.Error);}
  if(Refresh){if(!W.Write(TEXT("DirtyGenerated"),[&]{C->DirtyGenerated();})||!W.Write(TEXT("Refresh"),[&]{C->Refresh(EPCGChangeType::Input,Cancel);++Refreshes;O->SetNumberField(TEXT("refresh_count"),Refreshes);}))return W.Error(W.R.Error);}
  if(Generate){if(ScopedInitial||ScopedComposite){auto Dispatch=MakeShared<FJsonObject>();Dispatch->SetStringField(TEXT("api"),TEXT("GenerateLocalGetTaskId(bool)"));Dispatch->SetBoolField(TEXT("requested_force"),Force);Dispatch->SetBoolField(TEXT("entered"),false);Dispatch->SetBoolField(TEXT("returned"),false);Dispatch->SetBoolField(TEXT("task_accepted"),false);Dispatch->SetBoolField(TEXT("completion_proven"),false);W.R.Data->SetObjectField(TEXT("scoped_generate_dispatch"),Dispatch);
   // ADW: only observed completed hard cleanup authorizes the second stage,
    // regardless of requested force. GenerateInternal/ShouldGenerate accept the
    // OnDemand request from bGenerated=false; CreateGenerateTask has no active
    // force-specific writer in the installed source. Keep every source/current
    // and post-cleanup idle/output/resource check; forward the original bool.
   // Keep the same immutable source baseline and all owner/GI roles; no old output is reread.
   if(ScopedComposite){if(!W.R.Data->GetBoolField(TEXT("PCG_cleanup_completed"))||!CleanupBefore||!Scope->InitialReady(true,CleanupBefore->GetBoolField(TEXT("was_generated_this_session"))))return W.Error(W.R.Error);}
   if(!W.SDK(TEXT("GenerateLocalGetTaskId"),true))return W.Error(W.R.Error);Dispatch->SetBoolField(TEXT("entered"),true);const FPCGTaskId Task=C->GenerateLocalGetTaskId(Force);Scope->GenerationAuthorized=Task!=InvalidPCGTaskId;Dispatch->SetBoolField(TEXT("returned"),true);Dispatch->SetStringField(TEXT("task_id"),LexToString(Task));Dispatch->SetBoolField(TEXT("task_accepted"),Task!=InvalidPCGTaskId);if(Task!=InvalidPCGTaskId)W.R.bChanged=true;++Generates;O->SetNumberField(TEXT("generate_count"),Generates);if(!W.After(TEXT("GenerateLocalGetTaskId")))return W.Error(W.R.Error);if(Task==InvalidPCGTaskId){Scope->Stop(TEXT("pcg_scoped_enqueue_rejected"),C->GetPathName(),TEXT("task_id"),TEXT("valid enqueue task; not generation completion"),LexToString(Task));return W.Error(W.R.Error);}}
   else if(!W.Write(TEXT("GenerateLocal"),[&]{C->GenerateLocal(Force);++Generates;O->SetNumberField(TEXT("generate_count"),Generates);}))return W.Error(W.R.Error);}
  if(ScopedRefresh)
  {
   const bool Pending=C->IsRefreshInProgress()||C->IsGenerating();W.R.Data->SetBoolField(TEXT("scoped_refresh_dispatch_returned"),true);W.R.Data->SetBoolField(TEXT("scoped_refresh_task_pending_observed"),Pending);W.R.Data->SetBoolField(TEXT("scoped_refresh_enqueue_is_completion"),false);
   if(!Pending){Scope->Stop(TEXT("pcg_scoped_refresh_not_scheduled"),C->GetPathName(),TEXT("refresh/generation_pending"),TEXT("actual pending task after exactly one refresh API return"),TEXT("false"));return W.Error(W.R.Error);}
   if(RefreshReceipt.Op)RefreshReceipt.Op->Committed=true;
   W.R.bChanged=true;W.R.Data->SetStringField(TEXT("scoped_refresh_change_scope"),TEXT("Observed runtime refresh/generation task enqueue; not authored source edits, saved changes or completed generation."));
  }
  InitialRows.Add(MakeShared<FJsonValueObject>(ComponentJson(W,C)));if(!W.Admit(TEXT("component_after_owned_effects")))return W.Error(W.R.Error);
 }
 if(HasCleanupExpectation){if(!Scope->CleanupState(CleanupAfter))return W.Error(W.R.Error);W.R.Data->SetObjectField(TEXT("scoped_cleanup_after"),CleanupAfter);const bool StateReady=CleanupAfter->GetBoolField(TEXT("managed_resources_accessible"))&&(CleanupAfter->GetBoolField(TEXT("was_generated_this_session"))==CleanupExpectedSession)&&!CleanupAfter->GetBoolField(TEXT("generating"))&&!CleanupAfter->GetBoolField(TEXT("cleaning_up"))&&!CleanupAfter->GetBoolField(TEXT("refresh_in_progress"))&&CleanupAfter->GetIntegerField(TEXT("managed_resource_count"))==0&&CleanupAfter->GetBoolField(TEXT("generated"))==!CleanupRemove&&(!CleanupRemove||CleanupAfter->GetIntegerField(TEXT("generated_output_data_count"))==0);W.R.Data->SetBoolField(TEXT("PCG_cleanup_completed"),false);W.R.Data->SetBoolField(TEXT("scoped_cleanup_state_ready"),StateReady);W.R.Data->SetBoolField(TEXT("scoped_cleanup_expected_session_history"),CleanupExpectedSession);}
 const bool RequiredCleanupSession=HasCleanupExpectation?CleanupExpectedSession:(CleanupBefore?CleanupBefore->GetBoolField(TEXT("was_generated_this_session")):true);
 bool Ready=!Wait;TArray<TSharedPtr<FJsonValue>> ActorRows;for(auto* A:IncludedActors){ActorRows.Add(MakeShared<FJsonValueObject>(ActorJson(W,A)));if(!W.Admit(TEXT("refresh_actor_readback")))return W.Error(W.R.Error);}
 if(Wait&&!Targets.IsEmpty())Ready=true;
 for(auto* C:Targets){auto Row=ComponentJson(W,C);W.ComponentAfter.Add(MakeShared<FJsonValueObject>(Row));if(!W.Admit(TEXT("refresh_component_readback")))return W.Error(W.R.Error);if(Wait){const bool Busy=Row->GetBoolField(TEXT("generating"))||Row->GetBoolField(TEXT("cleaning_up"))||Row->GetBoolField(TEXT("refresh_in_progress"));const bool Minima=(MinOutput<0||Row->GetIntegerField(TEXT("generated_output_data_count"))>=MinOutput)&&(MinResource<0||Row->GetIntegerField(TEXT("managed_resource_count"))>=MinResource);Ready&=!Busy&&Minima&&(!ScopedActive||((MinResource<0||Row->GetBoolField(TEXT("managed_resources_accessible")))&&(CleanupScope?(C->bGenerated==!CleanupRemove&&(C->WasGeneratedThisSession()==RequiredCleanupSession)&&(!CleanupRemove||Row->GetIntegerField(TEXT("generated_output_data_count"))==0)):(C->bGenerated&&C->WasGeneratedThisSession()))));}}
 if(CleanupScope&&ScopedActive)Ready&=W.R.Data->GetBoolField(TEXT("scoped_cleanup_state_ready"));
 // PC initial wait=false proves enqueue only; generation completion requires the readonly wait=true observation.
 if(CompositeScope&&ScopedActive&&!Wait)Ready=false;
 if(ScopedActive&&Scope->RefreshMode){if(RefreshReceipt.Op){RefreshReceipt.Op->ObserveIdle(Targets[0]);Ready&=RefreshReceipt.Op->Status==TEXT("succeeded")&&!RefreshReceipt.Op->OffThread.Load();}else Ready=false;if(!Wait)Ready=false;W.R.Data->SetBoolField(TEXT("scoped_refresh_query_only"),HasExpectation);W.R.Data->SetBoolField(TEXT("scoped_refresh_completed"),Wait&&Ready);}
 if(ScopedActive){TArray<TSharedPtr<FJsonValue>> ActualFiles;if(!Scope->Files(ActualFiles))return W.Error(W.R.Error);auto A=MakeShared<FJsonObject>();auto B=MakeShared<FJsonObject>();A->SetArrayField(TEXT("source_files"),SourceFiles);B->SetArrayField(TEXT("source_files"),ActualFiles);FString Path,Want,Got;if(ScopedDifference(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B),Path,Want,Got,CleanupScope||CompositeScope)){Scope->Stop(TEXT("pcg_scoped_source_disk_changed"),TEXT("source_files"),Path,Want,Got);return W.Error(W.R.Error);}W.R.Data->SetBoolField(TEXT("scoped_source_postcondition_passed"),true);W.R.Data->SetBoolField(TEXT("scoped_generation_enqueue_is_completion"),false);if(CompositeScope){auto Proof=MakeShared<FJsonObject>(*Scope->Baseline);Proof->SetArrayField(TEXT("source_files"),ActualFiles);W.R.Data->SetObjectField(TEXT("scoped_cleanup_generate_source_after"),Proof);W.R.Data->SetBoolField(TEXT("scoped_cleanup_generate_source_preserved"),true);}else if(CleanupScope){auto Proof=MakeShared<FJsonObject>(*Scope->Baseline);Proof->SetArrayField(TEXT("source_files"),ActualFiles);W.R.Data->SetObjectField(TEXT("scoped_cleanup_source_after"),Proof);W.R.Data->SetBoolField(TEXT("scoped_cleanup_source_preserved"),true);}}
 O->SetBoolField(TEXT("wait_requested"),Wait);O->SetBoolField(TEXT("wait_supported"),true);O->SetStringField(TEXT("wait_mode"),TEXT("single_frame_readback"));O->SetStringField(TEXT("wait_note"),TEXT("Native bridge commands run on the game thread, so this command does not sleep-poll inside Unreal. External MCP callers should poll with generate=false until wait_completed is true."));O->SetBoolField(TEXT("wait_completed"),Ready);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_elapsed_seconds"),0);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetNumberField(TEXT("min_generated_output_data_count"),MinOutput);O->SetNumberField(TEXT("min_managed_resource_count"),MinResource);O->SetArrayField(TEXT("actors"),ActorRows);if(Wait)O->SetArrayField(TEXT("initial_components"),InitialRows);O->SetArrayField(TEXT("components"),Wait?W.ComponentAfter:InitialRows);W.R.Data->SetArrayField(TEXT("after"),W.ComponentAfter);W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);W.R.Data->SetBoolField(TEXT("PCG_readiness_observed"),Wait);W.R.Data->SetBoolField(TEXT("PCG_generation_completed"),!CleanupScope&&Wait&&Ready);W.R.Data->SetStringField(TEXT("PCG_completion_scope"),TEXT("Current selected target single-frame busy/minimum observation only; enqueue return and global/GPU completion are not proven"));
 if(RefreshReceipt.Op)
 {
  RefreshReceipt.Completed=true;auto& Op=*RefreshReceipt.Op;
  // Files/current observers may enqueue work after the earlier component readback.
  Op.ObserveIdle(Targets[0]);
  if(HasExpectation&&(Op.Status==TEXT("aborted")||Op.Status==TEXT("unknown")||Op.OffThread.Load()))
  {
   if(Op.Status==TEXT("aborted")&&!Op.OffThread.Load()){Op.Verified=true;Op.Detach();W.R.Data->SetBoolField(TEXT("scoped_refresh_terminal_observation_verified"),true);W.R.bChangeStateKnown=true;W.Fail(TEXT("pcg_scoped_refresh_aborted"),TEXT("This refresh generation start was followed by the native cancelled/aborted notification; current target/source postconditions verified, no successful completion claimed"));}
   else W.Fail(TEXT("pcg_scoped_refresh_outcome_unknown"),TEXT("No coherent request-local refresh terminal observation; actual state retained, no success/cancellation inferred"));
   return W.Error(W.R.Error);
  }
  // Finish still performs its original final admission, which may invoke SDK
  // observers. Keep listeners armed through that boundary and close telemetry last.
  const FRefreshPtr Held=RefreshReceipt.Op;const FString ReturnedStatus=Op.Status;const bool Query=HasExpectation;
  W.FinalRefreshObservation=[Held,ReturnedStatus,Query,Ready,&W,O]()
  {
   Held->ObserveIdle(Held->Component.Get());
   if(Held->OffThread.Load()||Held->Status!=ReturnedStatus||!Held->Roles(Held->Component.Get()))
   {
    Held->Unknown(TEXT("lifecycle_changed_during_final_admission"));W.R.Data->SetBoolField(TEXT("scoped_refresh_completed"),false);W.R.Data->SetBoolField(TEXT("PCG_generation_completed"),false);O->SetBoolField(TEXT("wait_completed"),false);PublishRefreshTracking(W,Held,Query);return W.Fail(TEXT("pcg_scoped_refresh_outcome_unknown"),TEXT("Refresh lifecycle changed after the typed readback; no terminal outcome inferred"));
   }
   if(Query&&Ready&&Held->Status==TEXT("succeeded")){Held->Verified=true;Held->Detach();W.R.Data->SetBoolField(TEXT("scoped_refresh_terminal_observation_verified"),true);}
   PublishRefreshTracking(W,Held,Query);return true;
  };
 }
 return O;
}

FString NormalizeObject(const FString& S){FString P=FPackageName::ExportTextPathToObjectPath(S).TrimStartAndEnd();P.TrimQuotesInline();if((P.StartsWith(TEXT("/Game/"))||P.StartsWith(TEXT("/Engine/")))&&!P.Contains(TEXT(".")))P+=TEXT(".")+FPackageName::GetShortName(P);return P;}
UBlueprint* LoadBlueprint(FWork& W,const FString& Name)
{
 auto Try=[&](const FString& P)->UBlueprint*{UBlueprint* B=nullptr;if(!W.Read(TEXT("LoadObject_Blueprint"),[&]{return LoadObject<UBlueprint>(nullptr,*P);},B))return nullptr;return B;};
 auto Path=NormalizeObject(Name);UBlueprint* B=Try(Path);if(!W.Admit(TEXT("after_direct_blueprint_load")))return nullptr;if(!B&&!Name.Contains(TEXT("/"))&&!Name.Contains(TEXT("."))){B=Try(FString::Printf(TEXT("/Game/Blueprints/%s.%s"),*Name,*Name));if(!W.Admit(TEXT("after_legacy_blueprint_load")))return nullptr;}
 if(!B){TArray<FAssetData> Assets;if(!W.SDK(TEXT("registry_Blueprint_candidates")))return nullptr;auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();Registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(),Assets,true);if(!W.After(TEXT("registry_Blueprint_candidates"))||!W.Budget(Assets.Num()))return nullptr;TArray<FString> Candidates;const FString Short=FPackageName::GetShortName(Path);for(const auto& A:Assets)if(A.AssetName.ToString().Equals(Name,ESearchCase::IgnoreCase)||A.AssetName.ToString().Equals(Short,ESearchCase::IgnoreCase)||A.PackageName.ToString().Equals(Name,ESearchCase::IgnoreCase)||A.GetObjectPathString().Equals(Path,ESearchCase::IgnoreCase))Candidates.Add(A.GetObjectPathString());if(Candidates.Num()==1)B=Try(Candidates[0]);else if(Candidates.Num()>1)W.Fail(TEXT("pcg_binding_blueprint_ambiguous"),TEXT("Use a full exact Blueprint asset path"));}
 if(!B||!W.Pin(B)||Forbidden(B->GetPathName()))return nullptr;W.Blueprint=B;W.Generated=B->GeneratedClass;W.Skeleton=B->SkeletonGeneratedClass;W.SCS=B->SimpleConstructionScript;W.ParentClass=B->ParentClass;W.Variables=B->NewVariables;for(UObject* O:{static_cast<UObject*>(W.Generated),static_cast<UObject*>(W.Skeleton),static_cast<UObject*>(W.SCS),static_cast<UObject*>(B->ParentClass)})if(O&&!W.Pin(O))return nullptr;W.DirtyBefore=B->GetPackage()->IsDirty();return B;
}
bool HasMember(FWork& W,const FString& S){if(!W.Admit(TEXT("member_variable_lookup")))return false;auto* B=W.Blueprint;const FName Name(*S);if(S.IsEmpty())return false;if((B->GeneratedClass&&FindFProperty<FProperty>(B->GeneratedClass,Name))||(B->SkeletonGeneratedClass&&FindFProperty<FProperty>(B->SkeletonGeneratedClass,Name))||(B->ParentClass&&FindFProperty<FProperty>(B->ParentClass,Name)))return true;for(const auto& V:B->NewVariables)if(V.VarName==Name)return true;if(B->SimpleConstructionScript){const auto Nodes=B->SimpleConstructionScript->GetAllNodes();for(auto* N:Nodes){if(!N||!W.Pin(N))return false;if(N->GetVariableName()==Name)return true;}}return false;}
FString GraphType(UBlueprint* B,UEdGraph* G){if(B->FunctionGraphs.Contains(G))return TEXT("function");if(B->UbergraphPages.Contains(G))return TEXT("event");if(B->MacroGraphs.Contains(G))return TEXT("macro");if(B->DelegateSignatureGraphs.Contains(G))return TEXT("delegate");return TEXT("other");}
FString NormalizeGraphType(FString T){T=T.ToLower();T.ReplaceInline(TEXT(" "),TEXT(""));T.ReplaceInline(TEXT("-"),TEXT("_"));if(T==TEXT("eventgraph")||T==TEXT("event_graph")||T==TEXT("ubergraph")||T==TEXT("uber_graph"))return TEXT("event");if(T==TEXT("functiongraph")||T==TEXT("function_graph"))return TEXT("function");if(T==TEXT("macrograph")||T==TEXT("macro_graph"))return TEXT("macro");return T;}
UEdGraph* SelectGraph(FWork& W,const TSharedPtr<FJsonObject>& P){const FString Name=Str(P,TEXT("graph_name"),TEXT("UserConstructionScript")),Id=Str(P,TEXT("graph_id")),Type=NormalizeGraphType(Str(P,TEXT("graph_type"),TEXT("function")));TArray<UEdGraph*> Graphs;W.Blueprint->GetAllGraphs(Graphs);if(!W.Budget(Graphs.Num()))return nullptr;for(auto* G:Graphs){if(!G||!W.Pin(G))return nullptr;FGuid Parsed;const bool IdMatch=Id.IsEmpty()||(FGuid::Parse(Id,Parsed)?G->GraphGuid==Parsed:Same(G->GraphGuid.ToString(),Id));if(IdMatch&&(Name.IsEmpty()||G->GetName().Equals(Name,ESearchCase::IgnoreCase))&&(Type.IsEmpty()||Type==TEXT("any")||GraphType(W.Blueprint,G)==Type))return G;}return nullptr;}
UClass* LoadFunctionClass(FWork& W,const FString& Input){const FString Path=NormalizeObject(Input);UClass* C=nullptr;if(!W.Read(TEXT("LoadObject_function_class"),[&]{return LoadObject<UClass>(nullptr,*Path);},C))return nullptr;if(!C){if(!W.Read(TEXT("FindFirstObject_function_class"),[&]{return FindFirstObject<UClass>(*Input);},C))return nullptr;}if(!C){if(!W.Read(TEXT("LoadObject_Engine_function_class"),[&]{return LoadObject<UClass>(nullptr,*(FString(TEXT("/Script/Engine."))+Input));},C))return nullptr;}if(C&&(!W.Pin(C)||Forbidden(C->GetPathName())||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)))return nullptr;return C;}
UFunction* FindFunction(FWork& W,UClass* C,const FString& Name){if(!W.Admit(TEXT("function_lookup")))return nullptr;if(auto* F=C->FindFunctionByName(FName(*Name)))return W.Pin(F)?F:nullptr;TArray<UFunction*> Matches;for(TFieldIterator<UFunction> It(C,EFieldIteratorFlags::IncludeSuper);It;++It){auto* F=*It;if(F->GetName().Equals(Name,ESearchCase::IgnoreCase)||F->GetDisplayNameText().ToString().Equals(Name,ESearchCase::IgnoreCase))Matches.AddUnique(F);}return Matches.Num()==1&&W.Pin(Matches[0])?Matches[0]:nullptr;}
bool Callable(FWork& W,UFunction* F){if(!F->HasAnyFunctionFlags(FUNC_BlueprintCallable|FUNC_BlueprintPure))return W.Fail(TEXT("pcg_binding_function_not_callable"),TEXT("Runtime helper must be BlueprintCallable/BlueprintPure"));if(F->HasAnyFunctionFlags(FUNC_EditorOnly)||F->HasMetaData(TEXT("Latent"))||F->HasMetaData(TEXT("BlueprintInternalUseOnly"))||F->HasMetaData(TEXT("BlueprintInternalUseOnlyHierarchical")))return W.Fail(TEXT("pcg_binding_runtime_helper_unavailable"),TEXT("Editor-only, latent or internal helper is not the required existing runtime dependency"));return true;}

bool DirectRuntimeReference(FWork& W,UObject* O)
{
 if(!O)return true;if(!W.Pin(O))return false;if(Forbidden(O->GetPathName())||Forbidden(O->GetClass()->GetPathName())||O->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly)||O->GetClass()->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly))return W.Fail(TEXT("pcg_binding_runtime_parameter_dependency"),TEXT("A directly serialized helper pin type/default refers to MCP/Python/Editor-only implementation"));bool EditorOnly=false;if(!W.Read(TEXT("Object_IsEditorOnly_dependency"),[&]{return O->IsEditorOnly();},EditorOnly))return false;return !EditorOnly||W.Fail(TEXT("pcg_binding_runtime_default_editor_only"),TEXT("Direct helper default/type is explicitly Editor-only"));
}
bool TypeDependency(FWork& W,const FEdGraphPinType& Type)
{
 // Map value and delegate member parent are separate serialized object references. No recursive struct/asset dependency walk.
 for(UObject* O:{Type.PinSubCategoryObject.Get(),Type.PinValueType.TerminalSubCategoryObject.Get(),Type.PinSubCategoryMemberReference.MemberParent.Get()})if(!DirectRuntimeReference(W,O))return false;return true;
}
bool HelperAllParameters(FWork& W)
{
 if(!W.Admit(TEXT("preflight_all_helper_parameters")))return false;TArray<FName> Names;for(TFieldIterator<FProperty> It(W.Function);It&&It->HasAnyPropertyFlags(CPF_Parm);++It)Names.Add(It->GetFName());if(!W.Budget(Names.Num()))return false;TArray<TSharedPtr<FJsonValue>> Rows;
 for(FName Name:Names)
 {
  if(!W.Admit(TEXT("preflight_next_helper_parameter")))return false;FProperty* P=FindFProperty<FProperty>(W.Function,Name);if(!P||!P->HasAnyPropertyFlags(CPF_Parm))return W.Fail(TEXT("pcg_binding_helper_parameter_changed"),TEXT("Captured helper parameter no longer exists"));const bool Return=P->HasAnyPropertyFlags(CPF_ReturnParm);FEdGraphPinType Type;bool Converted=false;if(!W.SDK(TEXT("Schema_ConvertAllHelperParameterToPinType")))return false;Converted=W.Schema->ConvertPropertyToPinType(P,Type);if(!W.After(TEXT("Schema_ConvertAllHelperParameterToPinType")))return false;if(!Converted)return W.Fail(TEXT("pcg_binding_helper_type_unavailable"),TEXT("Additional/return parameter lacks a valid public K2 pin type"));if(!TypeDependency(W,Type))return false;
  // Re-find the FProperty after the virtual conversion callback. No old FProperty pointer crosses the next SDK boundary.
  P=FindFProperty<FProperty>(W.Function,Name);if(!P||!P->HasAnyPropertyFlags(CPF_Parm))return W.Fail(TEXT("pcg_binding_helper_parameter_changed"),TEXT("Helper parameter changed during public conversion"));FString Default;bool HasDefault=false;if(!W.SDK(TEXT("Schema_FindFunctionParameterDefaultValue")))return false;HasDefault=UEdGraphSchema_K2::FindFunctionParameterDefaultValue(W.Function,P,Default);if(!W.After(TEXT("Schema_FindFunctionParameterDefaultValue"))||!W.Budget(1,Default.Len()))return false;
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("parameter"),Name.ToString());Row->SetBoolField(TEXT("return_parameter"),Return);Row->SetBoolField(TEXT("has_explicit_default"),HasDefault);Row->SetStringField(TEXT("pin_category"),Type.PinCategory.ToString());Row->SetStringField(TEXT("default_reference_check"),TEXT("not_an_object_reference_category"));
  const bool Reference=Type.PinCategory==UEdGraphSchema_K2::PC_Object||Type.PinCategory==UEdGraphSchema_K2::PC_Class||Type.PinCategory==UEdGraphSchema_K2::PC_Interface||Type.PinCategory==UEdGraphSchema_K2::PC_SoftObject||Type.PinCategory==UEdGraphSchema_K2::PC_SoftClass;
  if(HasDefault&&Reference)
  {
   if(Forbidden(Default))return W.Fail(TEXT("pcg_binding_helper_default_dependency"),TEXT("Direct reference-bearing helper default contains an MCP/Python implementation reference"));FString Path=FPackageName::ExportTextPathToObjectPath(Default).TrimStartAndEnd();Path.TrimQuotesInline();Row->SetStringField(TEXT("default_reference_path"),Path);
   if(FPackageName::IsValidObjectPath(Path))
   {
    UPackage* Package=FindPackage(nullptr,*FPackageName::ObjectPathToPackageName(Path));if(Package&&Package->HasAnyPackageFlags(PKG_EditorOnly))return W.Fail(TEXT("pcg_binding_helper_default_dependency"),TEXT("Default points directly into an already loaded Editor-only package"));UObject* Object=nullptr;const FSoftObjectPath ReferencePath(Path);const bool Hard=Type.PinCategory==UEdGraphSchema_K2::PC_Object||Type.PinCategory==UEdGraphSchema_K2::PC_Class||Type.PinCategory==UEdGraphSchema_K2::PC_Interface;
    // Match the stock hard-default resolver: TryLoad outside compilation, ResolveObject during compilation. Soft defaults are never eagerly loaded.
    if(!W.Read(TEXT("Resolve_direct_helper_default"),[&]{return Hard&&!GCompilingBlueprint?ReferencePath.TryLoad():ReferencePath.ResolveObject();},Object))return false;Row->SetBoolField(TEXT("default_object_resolved"),Object!=nullptr);Row->SetStringField(TEXT("default_reference_check"),Object?TEXT("direct_object_and_type_verified"):TEXT("unresolved_original_default_preserved"));if(!DirectRuntimeReference(W,Object))return false;
   }
   else Row->SetStringField(TEXT("default_reference_check"),TEXT("relative_or_None_original_default_preserved"));
  }
  Rows.Add(MakeShared<FJsonValueObject>(Row));W.R.Data->SetArrayField(TEXT("helper_direct_parameter_checks"),Rows);
 }
 return W.Admit(TEXT("after_all_helper_parameter_dependencies"));
}
bool VariableType(FWork& W,const FString& Name,FEdGraphPinType& Out)
{
 if(!W.Admit(TEXT("preflight_variable_type")))return false;FProperty* P=nullptr;for(auto* C:{W.Skeleton,W.Generated,W.ParentClass})if(C&&(P=FindFProperty<FProperty>(C,FName(*Name))))break;
 if(P){bool Converted=false;if(!W.SDK(TEXT("Schema_ConvertVariablePropertyToPinType")))return false;Converted=W.Schema->ConvertPropertyToPinType(P,Out);if(!W.After(TEXT("Schema_ConvertVariablePropertyToPinType")))return false;if(!Converted)return W.Fail(TEXT("pcg_binding_variable_type_unavailable"),TEXT("Reflected variable has no public K2 pin type"));return TypeDependency(W,Out);}
 for(const auto& V:W.Blueprint->NewVariables)if(V.VarName==FName(*Name)){Out=V.VarType;return TypeDependency(W,Out);}if(W.SCS){const auto Nodes=W.SCS->GetAllNodes();for(auto* N:Nodes){if(!N||!W.Pin(N))return false;if(N->GetVariableName()==FName(*Name)&&N->ComponentClass){if(!W.Pin(N->ComponentClass))return false;Out.PinCategory=UEdGraphSchema_K2::PC_Object;Out.PinSubCategoryObject=N->ComponentClass;return TypeDependency(W,Out);}}}return W.Fail(TEXT("pcg_binding_variable_type_unavailable"),TEXT("Exact variable/component type cannot be obtained before writer"));
}
bool HelperInputType(FWork& W,const TArray<FName>& Names,FEdGraphPinType& Out,bool& Found)
{
 Found=false;if(!W.Admit(TEXT("preflight_helper_input_type")))return false;FProperty* P=nullptr;for(FName Name:Names){auto* Candidate=FindFProperty<FProperty>(W.Function,Name);if(Candidate&&Candidate->HasAnyPropertyFlags(CPF_Parm)&&!Candidate->HasAnyPropertyFlags(CPF_ReturnParm)&&(!Candidate->HasAnyPropertyFlags(CPF_OutParm)||Candidate->HasAnyPropertyFlags(CPF_ReferenceParm))){P=Candidate;break;}}if(!P)return true;Found=true;bool Converted=false;if(!W.SDK(TEXT("Schema_ConvertHelperPropertyToPinType")))return false;Converted=W.Schema->ConvertPropertyToPinType(P,Out);if(!W.After(TEXT("Schema_ConvertHelperPropertyToPinType")))return false;if(!Converted)return W.Fail(TEXT("pcg_binding_helper_type_unavailable"),TEXT("Reflected runtime helper parameter does not have a public K2 input type"));return TypeDependency(W,Out);
}
bool HelperPreflight(FWork& W,UClass* C,const FString& Component,const FString& Trigger,const FString& Regenerate,const FString& Drop)
{
 if(!HelperAllParameters(W))return false;
 if(W.Function->HasAnyFunctionFlags(FUNC_BlueprintPure))return W.Fail(TEXT("pcg_binding_helper_exec_unavailable"),TEXT("Pure helper has no required exec pair; rejected before graph writer"));if(!W.Function->HasAnyFunctionFlags(FUNC_Static)&&!(W.Generated&&W.Generated->IsChildOf(C))&&!(W.Skeleton&&W.Skeleton->IsChildOf(C)))return W.Fail(TEXT("pcg_binding_helper_self_unavailable"),TEXT("Nonstatic helper requires this Blueprint to supply its actual self class; no runtime target is synthesized"));
 auto Pair=[&](const FString& Variable,const TArray<FName>& Names){FEdGraphPinType From,To;bool Found=false;if(!VariableType(W,Variable,From)||!HelperInputType(W,Names,To,Found))return false;if(!Found)return W.Fail(TEXT("pcg_binding_helper_required_input_missing"),TEXT("Runtime helper is missing a required typed input"));bool Compatible=false;if(!W.Read(TEXT("Schema_ArePinTypesCompatible_preflight"),[&]{return W.Schema->ArePinTypesCompatible(From,To,W.Skeleton?W.Skeleton:W.Generated);},Compatible))return false;return Compatible||W.Fail(TEXT("pcg_binding_helper_input_incompatible"),TEXT("Original getter and helper input types are incompatible before writer"));};
 if(!Pair(Component,{TEXT("PCGComponent")})||!Pair(Trigger,{TEXT("GenerationTrigger")})||!Pair(Regenerate,{TEXT("bRegenerateInEditor"),TEXT("RegenerateInEditor")}))return false;
 if(!Drop.IsEmpty()){if(!Pair(Drop,{TEXT("bGenerateOnDropWhenTriggerOnDemand"),TEXT("GenerateOnDropWhenTriggerOnDemand")}))return false;W.R.Data->SetBoolField(TEXT("helper_prewriter_types_verified"),true);return true;}FEdGraphPinType Optional;bool Present=false;if(!HelperInputType(W,{TEXT("bGenerateOnDropWhenTriggerOnDemand"),TEXT("GenerateOnDropWhenTriggerOnDemand")},Optional,Present))return false;W.R.Data->SetBoolField(TEXT("helper_prewriter_types_verified"),true);return true;
}

const FPinValue* PinSlot(const FGraphValue& G,const UEdGraphPin* P){return G.Pins.FindByPredicate([&](const FPinValue& X){return X.Raw==P;});}
// ADS: UK2Node::GetBlueprintClassFromNode uses SKEL for self calls (K2Node.cpp:2222).
// SetFromFunction/GetTargetFunction use that scope (K2Node_CallFunction.cpp:1772/1377).
// Keep both exact held roles, not cross-class raw compiler metadata equality. No compile is dispatched here.
struct FOwnHelperParameter
{
 FName Name;FFieldClass* Kind=nullptr;uint64 Flags=0;int32 Dimension=0;TArray<FObjectHandle> TypeHandles;
 bool operator==(const FOwnHelperParameter& O)const{return Name==O.Name&&Kind==O.Kind&&Flags==O.Flags&&Dimension==O.Dimension&&TypeHandles==O.TypeHandles;}
};
struct FOwnHelperSignature
{
 uint32 Flags=0;TArray<FOwnHelperParameter> Parameters;
 bool operator==(const FOwnHelperSignature& O)const{return Flags==O.Flags&&Parameters==O.Parameters;}
};
bool OwnHelperSignature(UFunction* F,FOwnHelperSignature& Out)
{
 // This normal-use profile needs scalar primitives, byte enums and plain object references.
 // Complex own-helper declarations remain explicit pending; external helper admission is unchanged.
 Out=FOwnHelperSignature();Out.Flags=uint32(F->FunctionFlags)&uint32(FUNC_Public|FUNC_Protected|FUNC_Private|FUNC_BlueprintCallable|FUNC_BlueprintPure|FUNC_Static|FUNC_Const|FUNC_Event|FUNC_BlueprintEvent|FUNC_EditorOnly|FUNC_NetFuncFlags);
 for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It&&It->HasAnyPropertyFlags(CPF_Parm);++It)
 {
  const FProperty* P=*It;if(Out.Parameters.Num()>=64||P->ArrayDim!=1)return false;
  FOwnHelperParameter Row;Row.Name=P->GetFName();Row.Kind=P->GetClass();Row.Flags=uint64(P->GetPropertyFlags()&(CPF_Parm|CPF_OutParm|CPF_ReturnParm|CPF_ReferenceParm|CPF_ConstParm|CPF_UObjectWrapper));Row.Dimension=P->ArrayDim;
  if(Row.Kind==FByteProperty::StaticClass()){const auto* Byte=static_cast<const FByteProperty*>(P);if(!Byte->Enum.IsResolved())return false;Row.TypeHandles.Add(Byte->Enum.GetHandle());}
  else if(Row.Kind==FObjectProperty::StaticClass()){const auto* Object=static_cast<const FObjectProperty*>(P);if(!Object->PropertyClass.IsResolved())return false;Row.TypeHandles.Add(Object->PropertyClass.GetHandle());}
  else if(Row.Kind!=FBoolProperty::StaticClass()&&Row.Kind!=FIntProperty::StaticClass()&&Row.Kind!=FInt64Property::StaticClass()&&Row.Kind!=FFloatProperty::StaticClass()&&Row.Kind!=FDoubleProperty::StaticClass()&&Row.Kind!=FNameProperty::StaticClass()&&Row.Kind!=FStrProperty::StaticClass()&&Row.Kind!=FTextProperty::StaticClass())return false;
  Out.Parameters.Add(MoveTemp(Row));
 }
 // Named call inputs are logical, not a borrowed physical property order.
 Out.Parameters.Sort([](const FOwnHelperParameter& A,const FOwnHelperParameter& B){return A.Name.LexicalLess(B.Name);});return true;
}
struct FOwnHelperResolution
{
 FWork& W;UFunction* Primary=nullptr;UFunction* Alternate=nullptr;UClass* PrimaryOwner=nullptr;UClass* AlternateOwner=nullptr;FName Name;FGuid Guid;FOwnHelperSignature PrimaryBefore,AlternateBefore;bool Active=false;UK2Node_CallFunction* AuthoredCall=nullptr;UScriptStruct* MemberStruct=nullptr;FObjectProperty* MemberParentField=nullptr;UPackage* MetadataPackage=nullptr;FMetaData* Metadata=nullptr;
 explicit FOwnHelperResolution(FWork& In):W(In){}
 bool RuntimeCallable(UFunction* Role)const
 {
  // UField::HasMetaData itself may call UPackage::GetMetaData/legacy preload.
  // This held package's inline FMetaData is acquired once at an SDK boundary; FindValue is a map-only read (MetaData.cpp:379).
  return Metadata&&Role->HasAnyFunctionFlags(FUNC_BlueprintCallable|FUNC_BlueprintPure)&&!Role->HasAnyFunctionFlags(FUNC_EditorOnly)
   &&!Metadata->FindValue(Role,FName(TEXT("Latent")))&&!Metadata->FindValue(Role,FName(TEXT("BlueprintInternalUseOnly")))&&!Metadata->FindValue(Role,FName(TEXT("BlueprintInternalUseOnlyHierarchical")));
 }
 bool Diagnostic(const TCHAR* Reason,UFunction* Actual=nullptr,UK2Node_CallFunction* Call=nullptr)
 {
  if(!W.R.Data->HasField(TEXT("first_helper_mismatch")))
  {
   auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("reason"),Reason);J->SetStringField(TEXT("expected_function"),Primary?Primary->GetPathName():FString());J->SetStringField(TEXT("expected_owner"),PrimaryOwner?PrimaryOwner->GetPathName():FString());J->SetStringField(TEXT("allowed_alternate_function"),Alternate?Alternate->GetPathName():FString());J->SetStringField(TEXT("allowed_alternate_owner"),AlternateOwner?AlternateOwner->GetPathName():FString());J->SetStringField(TEXT("actual_function"),Actual&&Live(Actual)?Actual->GetPathName():FString());J->SetStringField(TEXT("actual_owner"),Actual&&Live(Actual)&&Actual->GetOuter()?Actual->GetOuter()->GetPathName():FString());if(Active)J->SetBoolField(TEXT("expected_self"),true);else J->SetStringField(TEXT("expected_self_policy"),TEXT("Unchanged external-helper exact-pointer policy; no self-context substitution admitted"));if(!Call)J->SetField(TEXT("actual_self"),MakeShared<FJsonValueNull>());if(Call){J->SetBoolField(TEXT("actual_self"),Call->FunctionReference.IsSelfContext());J->SetStringField(TEXT("expected_name"),Name.ToString());J->SetStringField(TEXT("actual_name"),Call->FunctionReference.GetMemberName().ToString());J->SetStringField(TEXT("expected_guid"),Guid.ToString());J->SetStringField(TEXT("actual_guid"),Call->FunctionReference.GetMemberGuid().ToString());}W.R.Data->SetObjectField(TEXT("first_helper_mismatch"),J);
  }
  return W.Fail(TEXT("pcg_binding_helper_resolution_changed"),TEXT("Runtime helper resolution differs from the captured exact external function or own Generated/SKEL semantic pair; actual mutation/save prefix retained."));
 }
 bool Current()
 {
  if(!Active)return true;
  FOwnHelperSignature A,B;
  if(!Live(Primary)||!Live(Alternate)||!((PrimaryOwner==W.Generated&&AlternateOwner==W.Skeleton)||(PrimaryOwner==W.Skeleton&&AlternateOwner==W.Generated))||Primary->GetOuter()!=PrimaryOwner||Alternate->GetOuter()!=AlternateOwner||Primary->GetFName()!=Name||Alternate->GetFName()!=Name||!PrimaryOwner->ClassGeneratedBy.IsResolved()||PrimaryOwner->ClassGeneratedBy.GetHandle()!=TObjectPtr<UObject>(W.Blueprint).GetHandle()||!AlternateOwner->ClassGeneratedBy.IsResolved()||AlternateOwner->ClassGeneratedBy.GetHandle()!=TObjectPtr<UObject>(W.Blueprint).GetHandle()||!OwnHelperSignature(Primary,A)||!OwnHelperSignature(Alternate,B)||!(A==PrimaryBefore)||!(B==AlternateBefore))return Diagnostic(TEXT("held_own_helper_semantic_role"),Primary,AuthoredCall);
  if(!MetadataPackage||!Live(MetadataPackage)||W.Blueprint->GetPackage()!=MetadataPackage||!RuntimeCallable(Primary)||!RuntimeCallable(Alternate))return Diagnostic(TEXT("own_helper_runtime_callability"),Alternate,AuthoredCall);
  if(AuthoredCall)
  {
   // MemberReference.h:76 stores MemberParent as a public reflected TObjectPtr; self references require its resolved-null handle.
   if(!Live(AuthoredCall)||!MemberStruct||!MemberParentField||MemberParentField->GetOwnerStruct()!=MemberStruct||MemberParentField->ArrayDim!=1||MemberParentField->ElementSize!=sizeof(TObjectPtr<UObject>))return Diagnostic(TEXT("own_helper_member_parent_descriptor"),nullptr,AuthoredCall);
   const auto* Slot=MemberParentField->GetObjectPtrPropertyValuePtr(MemberParentField->ContainerPtrToValuePtr<void>(&AuthoredCall->FunctionReference));const TObjectPtr<UObject> NullParent;
   if(!Slot->IsResolved()||Slot->GetHandle()!=NullParent.GetHandle()||!AuthoredCall->FunctionReference.IsSelfContext()||AuthoredCall->FunctionReference.IsLocalScope()||AuthoredCall->FunctionReference.GetMemberName()!=Name||AuthoredCall->FunctionReference.GetMemberGuid()!=Guid)return Diagnostic(TEXT("own_helper_authored_reference_changed"),nullptr,AuthoredCall);
  }
  return true;
 }
 bool Bind(UFunction* F)
 {
  Primary=F;PrimaryOwner=F->GetOwnerClass();Name=F->GetFName();
  if(PrimaryOwner!=W.Generated&&PrimaryOwner!=W.Skeleton)return true;
  AlternateOwner=PrimaryOwner==W.Generated?W.Skeleton:W.Generated;
  if(!AlternateOwner||!W.Pin(AlternateOwner))return false;
  TStrongObjectPtr<UFunction> Returned(nullptr);if(!W.Read(TEXT("Find_own_helper_other_class"),[&]{auto* Value=AlternateOwner->FindFunctionByName(Name,EIncludeSuperFlag::ExcludeSuper);if(Live(Value))Returned.Reset(Value);return Value;},Alternate))return false;
  
  if(!Alternate||!W.Pin(Alternate)||Alternate->GetOuter()!=AlternateOwner||!PrimaryOwner->ClassGeneratedBy.IsResolved()||PrimaryOwner->ClassGeneratedBy.GetHandle()!=TObjectPtr<UObject>(W.Blueprint).GetHandle()||!AlternateOwner->ClassGeneratedBy.IsResolved()||AlternateOwner->ClassGeneratedBy.GetHandle()!=TObjectPtr<UObject>(W.Blueprint).GetHandle())return Diagnostic(TEXT("own_helper_other_class_role"),Alternate);
  if(!OwnHelperSignature(Primary,PrimaryBefore)||!OwnHelperSignature(Alternate,AlternateBefore))return W.Fail(TEXT("pcg_binding_own_helper_signature_pending"),TEXT("Own Generated/SKEL pairing currently admits scalar primitives, byte enums and plain object references; other own signatures remain unverified before graph writing."));
  if(!(PrimaryBefore==AlternateBefore))return Diagnostic(TEXT("own_helper_parameter_semantics"),Alternate);
  MetadataPackage=W.Blueprint->GetPackage();if(!W.Pin(MetadataPackage))return false;
  if(!W.Read(TEXT("Own_helper_package_metadata"),[&]{return &MetadataPackage->GetMetaData();},Metadata))return false;
  FOwnHelperSignature AfterMetadataA,AfterMetadataB;if(!OwnHelperSignature(Primary,AfterMetadataA)||!OwnHelperSignature(Alternate,AfterMetadataB)||!(AfterMetadataA==PrimaryBefore)||!(AfterMetadataB==AlternateBefore)||!RuntimeCallable(Primary)||!RuntimeCallable(Alternate))return Diagnostic(TEXT("own_helper_runtime_callability"),Alternate);
  Active=true;
  MemberStruct=FMemberReference::StaticStruct();if(!W.Pin(MemberStruct))return false;MemberParentField=FindFProperty<FObjectProperty>(MemberStruct,TEXT("MemberParent"));if(!MemberParentField||MemberParentField->GetOwnerStruct()!=MemberStruct||MemberParentField->ArrayDim!=1||MemberParentField->ElementSize!=sizeof(TObjectPtr<UObject>))return Diagnostic(TEXT("own_helper_member_parent_descriptor"));
  // Hold every actual K2 type reference before any later SDK callback. No borrowed FProperty survives a conversion.
  for(UFunction* Role:{Primary,Alternate})
  {
   TArray<FName> Names;for(TFieldIterator<FProperty> It(Role,EFieldIteratorFlags::ExcludeSuper);It&&It->HasAnyPropertyFlags(CPF_Parm);++It)Names.Add(It->GetFName());
   for(FName Parameter:Names)
   {
    if(!W.Admit(TEXT("own_helper_parameter_type")))return false;FProperty* P=FindFProperty<FProperty>(Role,Parameter);if(!P||!P->HasAnyPropertyFlags(CPF_Parm))return Diagnostic(TEXT("own_helper_parameter_missing"),Role);
    FEdGraphPinType Type;bool Converted=false;if(!W.Read(TEXT("Schema_ConvertOwnHelperParameterToPinType"),[&]{return W.Schema->ConvertPropertyToPinType(P,Type);},Converted)||!Converted||!TypeDependency(W,Type))return false;
    FOwnHelperSignature A,B;if(!OwnHelperSignature(Primary,A)||!OwnHelperSignature(Alternate,B)||!(A==PrimaryBefore)||!(B==AlternateBefore))return Diagnostic(TEXT("own_helper_changed_during_conversion"),Role);
   }
  }
  bool Found=false;if(!W.Read(TEXT("Own_helper_guid"),[&]{return UBlueprint::GetGuidFromClassByFieldName<UFunction>(PrimaryOwner,Name,Guid);},Found))return false;
  FGuid OtherGuid;bool OtherFound=false;if(!W.Read(TEXT("Own_helper_other_guid"),[&]{return UBlueprint::GetGuidFromClassByFieldName<UFunction>(AlternateOwner,Name,OtherGuid);},OtherFound))return false;
  if(Found!=OtherFound||Guid!=OtherGuid)return Diagnostic(TEXT("own_helper_guid_pair"),Alternate);
  Active=true;if(!Current())return false;W.R.Data->SetBoolField(TEXT("own_generated_skeleton_helper_pair"),true);W.R.Data->SetStringField(TEXT("helper_pair_scope"),TEXT("Exact same Blueprint Generated/SKEL scalar logical parameter roles. No class/function replacement, compile, raw compiler-metadata equality, or external-helper aliasing."));return true;
 }
 bool Matches(UFunction* F)const{return F==Primary||(Active&&F==Alternate);}
 bool VerifyReference(UK2Node_CallFunction* Call,UFunction* Actual,bool Remember=false)
 {
  if(!W.Admit(TEXT("helper_reference_readback")))return false;
  if(!Active)return Actual==Primary||Diagnostic(TEXT("external_helper_pointer"),Actual,Call);
  if(!Current())return false;UClass* Parent=nullptr;if(!W.Read(TEXT("Own_helper_member_parent"),[&]{return Call->FunctionReference.GetMemberParentClass();},Parent))return false;
  if(!Current()||!Matches(Actual)||!Call->FunctionReference.IsSelfContext()||Call->FunctionReference.IsLocalScope()||Parent!=nullptr||Call->FunctionReference.GetMemberName()!=Name||Call->FunctionReference.GetMemberGuid()!=Guid)return Diagnostic(TEXT("own_helper_member_reference"),Actual,Call);
  if(Remember)AuthoredCall=Call;if(!Current())return false;W.R.Data->SetStringField(TEXT("resolved_helper_function"),Actual->GetPathName());W.R.Data->SetStringField(TEXT("resolved_helper_owner"),Actual->GetOuter()->GetPathName());W.R.Data->SetBoolField(TEXT("resolved_helper_self"),true);return true;
 }
};
UEdGraphPin* FindPin(FWork& W,UEdGraphNode* Node,EEdGraphPinDirection Direction,const TArray<FName>& Names={},bool Exec=false,bool Data=false){if(!W.Admit(TEXT("find_owned_pin")))return nullptr;const auto* N=W.GraphState.Slots.FindByPredicate([&](const FNodeValue& X){return X.Node==Node;});if(!N){W.Fail(TEXT("pcg_binding_node_membership"),TEXT("Node outside exact physical graph"));return nullptr;}for(FName Name:Names)for(auto* P:N->Pins){const auto* V=PinSlot(W.GraphState,P);if(V&&V->Direction==Direction&&V->Name==Name)return P;}if(!Names.IsEmpty())return nullptr;for(auto* P:N->Pins){const auto* V=PinSlot(W.GraphState,P);if(V&&V->Direction==Direction&&(!Exec||V->Type.PinCategory==UEdGraphSchema_K2::PC_Exec)&&(!Data||V->Type.PinCategory!=UEdGraphSchema_K2::PC_Exec))return P;}return nullptr;}
// Each intended graph mutation is verified against plain snapshots. Old raw pins are never read after a callback until physical membership and header are independently recaptured.
template<typename F>bool GraphStep(FWork& W,const TCHAR* Stage,F&& Fn,UEdGraphNode* Mutable=nullptr,UEdGraphNode* Append=nullptr,UEdGraphNode* Remove=nullptr,const TMap<UEdGraphPin*,TArray<UEdGraphPin*>>* ExpectedLinks=nullptr,UEdGraphPin* DefaultPin=nullptr,const FString& ExpectedDefault=FString())
{
 if(!W.Admit(Stage))return false;const FGraphValue Before=W.GraphState;TArray<UEdGraphNode*> Expected=Before.Nodes;if(Append)Expected.Add(Append);if(Remove)Expected.Remove(Remove);
 TGuardValue<bool> Editing(W.EditingGraph,true);if(!W.SDK(Stage,true))return false;Fn();if(!W.After(Stage))return false;FGraphValue After;if(!CaptureGraph(W,After))return false;if(After.Guid!=Before.Guid||After.Nodes!=Expected)return W.Fail(TEXT("pcg_binding_unexpected_node_transition"),TEXT("Graph node membership/order differs from the exact intended append/removal"));
 for(const auto& N:Before.Slots){if(N.Node==Remove)continue;const auto* A=After.Slots.FindByPredicate([&](const FNodeValue& X){return X.Node==N.Node;});if(!A||A->Id!=N.Id||A->X!=N.X||A->Y!=N.Y||!Same(A->Comment,N.Comment)||A->BubbleVisible!=N.BubbleVisible||A->BubblePinned!=N.BubblePinned||(N.Node!=Mutable&&A->Pins!=N.Pins))return W.Fail(TEXT("pcg_binding_unrelated_node_changed"),TEXT("Another node identity/GUID/position/pin membership changed"));}
 for(const auto& P:Before.Pins){if(P.Owner==Remove)continue;if(P.Owner==Mutable){if(!P.Links.IsEmpty())return W.Fail(TEXT("pcg_binding_linked_reconstruction_unsupported"),TEXT("Only a newly owned unlinked node may reconstruct pins"));continue;}const auto* A=PinSlot(After,P.Raw);if(!A||!PinHeader(P,*A))return W.Fail(TEXT("pcg_binding_pin_header_changed"),TEXT("Captured retained pin identity/type/header changed"));if(P.Raw==DefaultPin){if(!Same(A->Default,ExpectedDefault)||!Same(A->AutoDefault,P.AutoDefault)||!Same(A->Text,P.Text)||A->DefaultObject!=P.DefaultObject)return W.Fail(TEXT("pcg_binding_default_readback_failed"),TEXT("Requested exact bool default did not survive schema callback"));}else if(!PinValues(P,*A))return W.Fail(TEXT("pcg_binding_unrelated_pin_value_changed"),TEXT("Unrelated retained pin value changed"));auto Links=P.Links;if(Remove)Links.RemoveAll([&](UEdGraphPin* L){const auto* S=PinSlot(Before,L);return S&&S->Owner==Remove;});if(ExpectedLinks)if(const auto* V=ExpectedLinks->Find(P.Raw))Links=*V;if(A->Links!=Links)return W.Fail(TEXT("pcg_binding_links_changed"),TEXT("Ordered reciprocal links differ from the expected edit"));}
 if(Append){const auto* N=After.Slots.FindByPredicate([&](const FNodeValue& X){return X.Node==Append;});if(!N)return W.Fail(TEXT("pcg_binding_new_node_missing"),TEXT("Exact created node missing"));for(auto* P:N->Pins){const auto* V=PinSlot(After,P);if(!V||!V->Links.IsEmpty())return W.Fail(TEXT("pcg_binding_new_node_linked"),TEXT("New node unexpectedly acquired links during placement"));}}
 if(Mutable)for(const auto& P:After.Pins)if(P.Owner==Mutable&&!P.Links.IsEmpty())return W.Fail(TEXT("pcg_binding_reconstructed_pin_linked"),TEXT("Default allocation/reconstruction must keep the new node unlinked"));
 W.GraphState=MoveTemp(After);W.R.bChanged=true;return true;
}
template<typename T,typename F>T* NewNode(FWork& W,int32 X,int32 Y,F&& Setup)
{
 if(!W.Admit(TEXT("before_NewObject_K2Node")))return nullptr;const FString ExpectedGraphPath=W.Graph->GetPathName();T* N=nullptr;if(!W.SDK(TEXT("NewObject_K2Node"),true))return nullptr;N=NewObject<T>(W.Graph);auto Row=MakeShared<FJsonObject>();Row->SetBoolField(TEXT("creation_returned_nonnull"),N!=nullptr);Row->SetStringField(TEXT("expected_graph"),ExpectedGraphPath);W.Created.Add(MakeShared<FJsonValueObject>(Row));if(!W.After(TEXT("NewObject_K2Node")))return nullptr;if(!N||!W.Pin(N)||N->GetOuter()!=W.Graph||N->GetGraph()!=W.Graph||N->GetPackage()!=W.Blueprint->GetPackage())return nullptr;Row->SetStringField(TEXT("node_path"),N->GetPathName());Row->SetStringField(TEXT("node_class"),N->GetClass()->GetPathName());
 if(!W.Write(TEXT("configure_new_K2Node"),[&]{Setup(N);N->NodePosX=X;N->NodePosY=Y;N->CreateNewGuid();}))return nullptr;
 if(!GraphStep(W,TEXT("Graph_AddNode"),[&]{W.Graph->AddNode(N,true);},nullptr,N))return nullptr;
 if(!GraphStep(W,TEXT("Node_PostPlacedNewNode"),[&]{N->PostPlacedNewNode();},N)||!GraphStep(W,TEXT("Node_AllocateDefaultPins"),[&]{N->AllocateDefaultPins();},N)||!GraphStep(W,TEXT("Node_ReconstructNode"),[&]{N->ReconstructNode();},N))return nullptr;
 return N;
}
bool Link(FWork& W,UEdGraphPin* A,UEdGraphPin* B){if(!A||!B||!W.Admit(TEXT("before_TryCreateConnection")))return W.Fail(TEXT("pcg_binding_required_pin_missing"),TEXT("Required data/exec pin missing"));const auto* AS=PinSlot(W.GraphState,A);const auto* BS=PinSlot(W.GraphState,B);if(!AS||!BS)return W.Fail(TEXT("pcg_binding_pin_membership"),TEXT("Connection endpoints must be exact captured pins"));if(AS->Links.Contains(B)&&BS->Links.Contains(A))return true;FPinConnectionResponse Response(CONNECT_RESPONSE_DISALLOW,FText::GetEmpty());if(!W.Read(TEXT("Schema_CanCreateConnection"),[&]{return W.Schema->CanCreateConnection(A,B);},Response))return false;if(Response.Response!=CONNECT_RESPONSE_MAKE)return W.Fail(TEXT("pcg_binding_connection_requires_conversion"),TEXT("A safe exact connection must not replace other links or insert implicit conversion nodes"));TMap<UEdGraphPin*,TArray<UEdGraphPin*>> Expected;Expected.Add(A,AS->Links);Expected.Add(B,BS->Links);Expected[A].Add(B);Expected[B].Add(A);bool Returned=false;if(!GraphStep(W,TEXT("Schema_TryCreateConnection"),[&]{Returned=W.Schema->TryCreateConnection(A,B);W.Mutation->SetBoolField(TEXT("last_connection_returned"),Returned);},nullptr,nullptr,nullptr,&Expected))return false;return Returned||W.Fail(TEXT("pcg_binding_connection_failed"),TEXT("Schema refused requested connection"));}
bool Break(FWork& W,UEdGraphPin* P){if(!W.Admit(TEXT("before_BreakAllPinLinks")))return false;const auto* V=PinSlot(W.GraphState,P);if(!V)return W.Fail(TEXT("pcg_binding_pin_membership"),TEXT("Break endpoint outside captured graph"));TMap<UEdGraphPin*,TArray<UEdGraphPin*>> Expected;Expected.Add(P,{});for(auto* L:V->Links){const auto* Other=PinSlot(W.GraphState,L);if(!Other)return false;auto Links=Other->Links;Links.Remove(P);Expected.Add(L,Links);}return GraphStep(W,TEXT("Pin_BreakAllPinLinks"),[&]{P->BreakAllPinLinks();},nullptr,nullptr,nullptr,&Expected);}
bool RemoveNode(FWork& W,UEdGraphNode* N){if(!W.Admit(TEXT("before_DestroyNode")))return false;const auto* S=W.GraphState.Slots.FindByPredicate([&](const FNodeValue& V){return V.Node==N;});if(!S)return W.Fail(TEXT("pcg_binding_remove_foreign_node"),TEXT("Removal requires exact current node"));const auto Pins=S->Pins;for(auto* P:Pins)if(!Break(W,P))return false;return GraphStep(W,TEXT("Node_DestroyNode"),[&]{for(auto& I:W.Objects){auto* O=I->Object.Get();if(O&&(O==N||O->IsIn(N)))I->Retired=true;}W.Mutation->SetStringField(TEXT("intentional_retired_node"),N->GetPathName());N->DestroyNode();},nullptr,nullptr,N);}
TSharedPtr<FJsonObject> GraphJson(FWork& W){auto O=MakeShared<FJsonObject>();if(!W.Admit(TEXT("graph_result")))return O;O->SetStringField(TEXT("graph_name"),W.Graph->GetName());O->SetStringField(TEXT("schema"),W.Schema->GetClass()->GetName());O->SetStringField(TEXT("graph_id"),W.Graph->GraphGuid.ToString());O->SetStringField(TEXT("graph_type"),GraphType(W.Blueprint,W.Graph));O->SetNumberField(TEXT("node_count"),W.GraphState.Nodes.Num());O->SetStringField(TEXT("object_path"),W.Graph->GetPathName());return O;}
TSharedPtr<FJsonObject> NodeJson(FWork& W,UEdGraphNode* N)
{
 auto O=MakeShared<FJsonObject>();if(!W.Admit(TEXT("apply_node_result")))return O;FText Title;if(!W.Read(TEXT("Node_GetNodeTitle"),[&]{return N->GetNodeTitle(ENodeTitleType::FullTitle);},Title))return O;const auto* Node=W.GraphState.Slots.FindByPredicate([&](const FNodeValue& V){return V.Node==N;});if(!Node)return O;O->SetStringField(TEXT("node_id"),Node->Id.ToString());O->SetStringField(TEXT("name"),N->GetName());O->SetStringField(TEXT("class"),N->GetClass()->GetName());O->SetStringField(TEXT("title"),Title.ToString());O->SetNumberField(TEXT("x"),Node->X);O->SetNumberField(TEXT("y"),Node->Y);O->SetStringField(TEXT("node_comment"),Node->Comment);O->SetBoolField(TEXT("comment_bubble_visible"),Node->BubbleVisible);O->SetBoolField(TEXT("comment_bubble_pinned"),Node->BubblePinned);TArray<TSharedPtr<FJsonValue>> Pins;
 for(auto* P:Node->Pins){const auto* V=PinSlot(W.GraphState,P);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),V->Name.ToString());J->SetStringField(TEXT("direction"),V->Direction==EGPD_Input?TEXT("input"):TEXT("output"));J->SetStringField(TEXT("category"),V->Type.PinCategory.ToString());J->SetStringField(TEXT("subcategory"),V->Type.PinSubCategory.ToString());J->SetBoolField(TEXT("is_array"),V->Type.IsArray());auto T=MakeShared<FJsonObject>();T->SetStringField(TEXT("category"),V->Type.PinCategory.ToString());T->SetStringField(TEXT("subcategory"),V->Type.PinSubCategory.ToString());T->SetBoolField(TEXT("is_array"),V->Type.IsArray());if(V->Type.PinSubCategoryObject.IsValid()){T->SetStringField(TEXT("subcategory_object"),V->Type.PinSubCategoryObject->GetPathName());J->SetStringField(TEXT("subcategory_object"),V->Type.PinSubCategoryObject->GetPathName());}J->SetObjectField(TEXT("pin_type"),T);J->SetStringField(TEXT("default_value"),V->Default);if(V->DefaultObject)J->SetStringField(TEXT("default_object"),V->DefaultObject->GetPathName());TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:V->Links){const auto* Q=PinSlot(W.GraphState,L);const auto* Owner=Q?W.GraphState.Slots.FindByPredicate([&](const FNodeValue& X){return X.Node==Q->Owner;}):nullptr;if(!Q||!Owner){W.Fail(TEXT("pcg_binding_result_link_missing"),TEXT("Result link outside captured graph"));return O;}auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("node_id"),Owner->Id.ToString());R->SetStringField(TEXT("node_name"),Owner->Node->GetName());R->SetStringField(TEXT("pin_name"),Q->Name.ToString());Links.Add(MakeShared<FJsonValueObject>(R));}J->SetArrayField(TEXT("linked_to"),Links);Pins.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("pins"),Pins);return O;
}
bool PackageOwnership(FWork& W)
{
 if(!W.Admit(TEXT("before_package_ownership_audit")))return false;auto* B=W.Blueprint;auto* Package=B->GetPackage();if(!Package||Package!=B->GetOutermost()||Forbidden(Package->GetName())||(Package->HasAnyPackageFlags(PKG_CompiledIn)||Package==GetTransientPackage()||Package->HasAnyFlags(RF_Transient)))return W.Fail(TEXT("pcg_binding_package_unavailable"),TEXT("Exact nontransient target package required"));
 UClass* C=B->GeneratedClass;UClass* S=B->SkeletonGeneratedClass;UObject* CDO=C?C->GetDefaultObject(false):nullptr;UObject* SDO=S?S->GetDefaultObject(false):nullptr;
 for(auto* D:{CDO,SDO})if(D&&!W.Pin(D))return false;
 if((CDO&&(CDO->GetClass()!=C||!CDO->HasAnyFlags(RF_ClassDefaultObject)||CDO->GetPackage()!=Package||CDO->GetOutermost()!=Package))||(SDO&&(SDO->GetClass()!=S||!SDO->HasAnyFlags(RF_ClassDefaultObject)||SDO->GetPackage()!=Package||SDO->GetOutermost()!=Package)))return W.Fail(TEXT("pcg_binding_CDO_ownership"),TEXT("Current class default object role/package differs"));
 if((C&&(C->ClassGeneratedBy!=B||C->GetPackage()!=Package||C->GetOutermost()!=Package))||(S&&(S->ClassGeneratedBy!=B||S->GetPackage()!=Package||S->GetOutermost()!=Package)))return W.Fail(TEXT("pcg_binding_class_ownership"),TEXT("Exact generated/skeleton owner and package required"));
 TArray<UObject*> Exports;GetObjectsWithPackage(Package,Exports,EGetObjectsFlags::IncludeNestedObjects);if(!W.Budget(Exports.Num()))return false;for(auto* O:Exports){if(!Live(O)||!W.Pin(O))return W.Fail(TEXT("pcg_binding_package_export_unavailable"),TEXT("Package contains pending/unavailable exports"));const bool Owned=O==B||O->IsIn(B)||(C&&(O==C||O->IsIn(C)))||(S&&(O==S||O->IsIn(S)))||(CDO&&(O==CDO||O->IsIn(CDO)))||(SDO&&(O==SDO||O->IsIn(SDO)));if(!Owned||O->GetPackage()!=Package||O->GetOutermost()!=Package)return W.Fail(TEXT("pcg_binding_package_foreign_export"),TEXT("Package-wide Save refuses unrelated export or external package"));}
 return W.Admit(TEXT("after_package_ownership_audit"));
}
bool SaveExact(FWork& W)
{
 if(!PackageOwnership(W)||!W.Admit(TEXT("before_SavePackages")))return false;auto* Package=W.Blueprint->GetPackage();const FString Path=Package->GetPathName();const bool Dirty=Package->IsDirty();W.Save->SetStringField(TEXT("package_before"),Path);W.Save->SetBoolField(TEXT("dirty_before"),Dirty);W.Save->SetBoolField(TEXT("SDK_save_returned"),false);
 if(!W.SDK(TEXT("SavePackages"),true))return false;W.R.bSaveAttempted=true;TArray<UPackage*> Packages{Package};const bool Saved=UEditorLoadingAndSavingUtils::SavePackages(Packages,false);W.R.bSaved|=Saved;W.Save->SetBoolField(TEXT("SDK_save_returned"),true);W.Save->SetBoolField(TEXT("SDK_save_success"),Saved);if(!W.After(TEXT("SavePackages")))return false;W.Save->SetStringField(TEXT("package_after"),Package->GetPathName());W.Save->SetBoolField(TEXT("dirty_after"),Package->IsDirty());if(!Saved||Package->IsDirty())return W.Fail(TEXT("pcg_binding_save_failed"),TEXT("Exact package save failed or is dirty after callback; true SDK history retained"));return PackageOwnership(W);
}
bool VerifyAuthoredCallDependencies(FWork& W,UEdGraphNode* Node)
{
 if(!W.Admit(TEXT("verify_authored_call_direct_dependencies")))return false;const auto* Slot=W.GraphState.Slots.FindByPredicate([&](const FNodeValue& V){return V.Node==Node;});if(!Slot)return W.Fail(TEXT("pcg_binding_apply_node_missing"),TEXT("Exact authored call node is absent"));const auto Pins=Slot->Pins;for(auto* P:Pins){if(!W.Admit(TEXT("verify_next_authored_call_pin")))return false;const auto* V=PinSlot(W.GraphState,P);if(!V)return W.Fail(TEXT("pcg_binding_apply_pin_missing"),TEXT("Authored call pin absent from current snapshot"));const FEdGraphPinType Type=V->Type;UObject* Object=V->DefaultObject;if(!TypeDependency(W,Type)||!DirectRuntimeReference(W,Object))return false;}return W.Admit(TEXT("verified_authored_call_direct_dependencies"));
}

// ACY: original template sync only. PCG's null-property override calls Super; it is
// not a purity claim. Global property delegates/reregister seams retain before/
// after admission. No PCG Refresh/Generate or Blueprint compile is enabled here.
// AEA: feature-local, read-only saved source lease. No graph writer or actor admission.
struct FBindingGraphSource
{
 FWork& W;FScopedGeneration Observer;TArray<TUniquePtr<FIdentity>> Held;
 TSharedPtr<FJsonObject> Before,FileBefore;bool Captured=false,Observing=false;TArray<UObject*> SourceNodes;
 explicit FBindingGraphSource(FWork& In):W(In),Observer(In)
 {Observer.BindingSourceOnly=true;Observer.BindingKeep=[this](const UObject* O){return Keep(O);};}
 bool Stop(const FString& Property,const FString& Want,const FString& Got)
 {return Observer.Stop(TEXT("pcg_binding_source_changed"),TEXT("saved_graph_source"),Property,Want,Got);}
 bool Keep(const UObject* O)
 {
  if(!Live(O)||Forbidden(O->GetPathName())||Forbidden(O->GetClass()->GetPathName()))return Stop(TEXT("lifetime/dependency"),TEXT("live Engine/game source without tooling dependency"),TEXT("unavailable/forbidden"));
  for(const UObject* Role:{O,static_cast<const UObject*>(O->GetClass()),static_cast<const UObject*>(O->GetPackage())})
  {bool Found=false;for(const auto& H:Held)if(H->Object.Get()==Role){Found=true;if(!H->Exact())return Stop(TEXT("identity"),H->Path,TEXT("changed"));break;}if(!Found){if(Held.Num()>=256)return Stop(TEXT("source roles"),TEXT("<=256"),LexToString(Held.Num()));Held.Add(MakeUnique<FIdentity>(Role));}}
  return true;
 }
 bool Current()
 {
  if(!Safe()||W.Editor.Get()!=GEditor||GEditor->PlayWorld||GEditor->GetPIEWorldContext()||GEditor->GetEditorWorldContext().World()!=W.World.Get())return Stop(TEXT("Editor/world"),TEXT("same idle Editor world"),TEXT("changed"));
  for(const auto& H:Held)if(!H->Exact())return Stop(TEXT("identity"),H->Path,TEXT("changed"));
  if(!Captured||Observing)return true;
  TGuardValue<bool> Busy(Observing,true);TSharedPtr<FJsonObject> Now;if(!Projection(Now))return false;
  FString Path,Want,Got;if(ScopedDifference(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(Now),Path,Want,Got,true))return Stop(Path,Want,Got);
  return true;
 }
 bool Projection(TSharedPtr<FJsonObject>& Out)
 {
  auto* G=Observer.Source;
  if(!Live(G)||G->GetClass()!=UPCGGraph::StaticClass()||G->GetOuter()!=G->GetPackage()||G->GetPackage()==W.Blueprint->GetPackage()||!G->GetPackage()->GetName().StartsWith(TEXT("/Game/"))||G->GetPackage()->IsDirty()||G->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject))return Stop(TEXT("graph source"),TEXT("same exact native saved-clean external /Game graph"),TEXT("other role/dirty"));
  // Common GraphSourceSnapshot copies supported logical defaults; no dynamic bag-type identity is frozen.
  TArray<UObject*> NodesNow;if(!Observer.Refs(G,TEXT("Nodes"),NodesNow)||NodesNow!=SourceNodes)return Stop(TEXT("source node membership"),TEXT("original exact physical source membership"),TEXT("changed"));
  Out=MakeShared<FJsonObject>();Out->SetStringField(TEXT("source_graph"),ScopedObjectKey(G));
  if(!Observer.GraphSourceSnapshot(Out.ToSharedRef()))return false;
  int32 Count=0;return Finite(MakeShared<FJsonValueObject>(Out),0,Count)||Stop(TEXT("projection"),TEXT("bounded finite authored values"),TEXT("nonfinite/excessive"));
 }
 bool File(bool Capture)
 {
  if(!Current())return false;
  TArray<const UObject*> Sources{Observer.Source};for(const UObject* Asset:Observer.AssetFiles)Sources.AddUnique(Asset);
  auto Files=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
  for(const UObject* O:Sources)
  {
   if(!Live(O)||!Keep(O))return Stop(TEXT("saved asset role"),TEXT("same held graph/mesh/material source"),TEXT("unavailable"));
   const FString Name=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(O->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()));
   TSharedPtr<FJsonObject> Now;
   if(!W.Read(TEXT("Binding_saved_graph_file_boundary"),[&]{auto J=MakeShared<FJsonObject>();const int64 Size=IFileManager::Get().FileSize(*Name);TArray<uint8> Bytes;if(Size<=0||Size>67108864||!FFileHelper::LoadFileToArray(Bytes,*Name)||Bytes.Num()!=Size)return TSharedPtr<FJsonObject>();uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);J->SetStringField(TEXT("path"),Name);J->SetStringField(TEXT("source_object"),ScopedObjectKey(O));J->SetNumberField(TEXT("bytes"),Size);J->SetStringField(TEXT("sha1"),BytesToHex(Digest,FSHA1::DigestSize));return TSharedPtr<FJsonObject>(J);},Now)||!Now)return Stop(TEXT("saved file"),TEXT("saved bounded nonempty graph/mesh/material source file"),TEXT("unavailable"));
   Rows.Add(MakeShared<FJsonValueObject>(Now));
  }
  Files->SetArrayField(TEXT("files"),Rows);
  if(Capture)FileBefore=Files;else {FString Path,Want,Got;if(!FileBefore||ScopedDifference(MakeShared<FJsonValueObject>(FileBefore),MakeShared<FJsonValueObject>(Files),Path,Want,Got,true))return Stop(FString(TEXT("source_file."))+Path,Want,Got);}
  return Current();
 }
 bool Capture(UPCGGraph* G)
 {
  Observer.Source=G;if(!Keep(G))return false;
  if(!Observer.Refs(G,TEXT("Nodes"),SourceNodes)||!Observer.SelectSourceNodes(SourceNodes))return Stop(TEXT("source topology"),TEXT("original finite source or bounded native CPU point pipeline"),TEXT("unsupported source graph"));
  // Resident lookup and local strong holds finish before any SDK callback.
  if(!Projection(Before))return false;Observer.Captured=true;Captured=true;
  for(int32 I=0;I<Held.Num();++I){const UObject* O=Held[I]->Object.Get();if(!Current()||!W.Pin(O)||!Current())return false;}
  if(!File(true))return false;
  W.R.Data->SetObjectField(TEXT("binding_readonly_graph_source"),Before);W.R.Data->SetObjectField(TEXT("binding_readonly_graph_source_file"),FileBefore);
  return true;
 }
};

struct FBindingTemplate
{
 FWork& W;USCS_Node* Node=nullptr;UPCGComponent* Target=nullptr;UPCGGraphInstance* Instance=nullptr;UObject* CDO=nullptr;
 FName ComponentName,TriggerName,RegenerateName,DropName;bool DropFallback=false;
 TSharedPtr<FJsonObject> Inputs,Expected,Receipt=MakeShared<FJsonObject>();TSharedPtr<FBindingGraphSource> GraphSource;bool GraphSourceKnown=false;
 TMap<FName,FProperty*> Fields;TMap<FName,FString> Shapes;TArray<USCS_Node*> Nodes;FGuid NodeGuid;EObjectFlags TargetFlags=RF_NoFlags,InstanceFlags=RF_NoFlags,CDOFlags=RF_NoFlags;
 explicit FBindingTemplate(FWork& In):W(In){}
 bool Stop(const FString& Object,const FString& Property,const FString& Want,const FString& Got,bool Pending=false)
 {
  if(!W.R.Data->HasField(TEXT("template_sync_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),W.R.Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Want);J->SetStringField(TEXT("actual"),Got);W.R.Data->SetObjectField(TEXT("template_sync_first_mismatch"),J);}
  return W.Fail(Pending?TEXT("pcg_binding_template_profile_pending"):TEXT("pcg_binding_template_current_changed"),TEXT("Exact inactive own-SCS template/source guard failed; actual graph/template prefix retained, no retry"));
 }
 bool Check(bool Okay,const TCHAR* Field,const FString& Want,const FString& Got,bool Pending=false){return Okay||Stop(Target?Target->GetPathName():TEXT("PCG_template"),Field,Want,Got,Pending);}
 bool ObjectFlagsCurrent(UObject* Object,EObjectFlags Before,const TCHAR* Role)
 {
  const EObjectFlags Now=Object->GetFlags();if(Now==Before)return true;
  // SavePackage2.cpp marks serialized exports loaded. Preserve exact authored
  // and lifetime flags; only this successful, audited target-package save may
  // add the exact pair of load-state bits. No mask, clearing or normalization.
  bool SDKSaveSuccess=false;FString SavedPackage;
  W.Save->TryGetBoolField(TEXT("SDK_save_success"),SDKSaveSuccess);W.Save->TryGetStringField(TEXT("package_before"),SavedPackage);
  const bool Allowed=W.R.bSaveAttempted&&W.R.bSaved&&SDKSaveSuccess&&Object->GetPackage()==W.Blueprint->GetPackage()&&SavedPackage==Object->GetPackage()->GetPathName()&&!EnumHasAnyFlags(Before,RF_Transient)&&Now==(Before|RF_WasLoaded|RF_LoadCompleted);
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("object"),Object->GetPathName());Row->SetStringField(TEXT("role"),Role);Row->SetStringField(TEXT("expected_flags"),LexToString(uint32(Before)));Row->SetStringField(TEXT("actual_flags"),LexToString(uint32(Now)));Row->SetStringField(TEXT("added_bits"),LexToString(uint32(Now)&~uint32(Before)));Row->SetBoolField(TEXT("successful_owned_save_load_bits_only"),Allowed);
  TSharedPtr<FJsonObject> Transitions;const TSharedPtr<FJsonObject>* Existing=nullptr;if(W.R.Data->TryGetObjectField(TEXT("template_object_save_flag_transitions"),Existing))Transitions=*Existing;else{Transitions=MakeShared<FJsonObject>();W.R.Data->SetObjectField(TEXT("template_object_save_flag_transitions"),Transitions);}Transitions->SetObjectField(Role,Row);
  return Allowed||Stop(Object->GetPathName(),FString(Role)+TEXT(".object_flags"),LexToString(uint32(Before))+TEXT(" or exact successful owned-save addition RF_WasLoaded|RF_LoadCompleted"),LexToString(uint32(Now)));
 }
 FProperty* Field(UClass* Class,FName Name)
 {
  FProperty* P=FindFProperty<FProperty>(Class,Name);if(!P)return nullptr;
  // These keys refer only to the exact native template class; CDO input descriptors
  // are re-found on the held generated class, never retained across SDK callbacks.
  if(Class==UPCGComponent::StaticClass()){if(auto* Was=Fields.Find(Name)){if(*Was!=P||Shapes[Name]!=DirectPropertyShape(P)){Stop(Class->GetPathName(),Name.ToString(),TEXT("same exact reflected descriptor/layout"),TEXT("changed"));return nullptr;}}else {Fields.Add(Name,P);Shapes.Add(Name,DirectPropertyShape(P));}}
  return P;
 }
 bool ReadBool(UObject* O,FName Name,bool& Value)
 {auto* P=CastField<FBoolProperty>(Field(O->GetClass(),Name));if(!P||P->ArrayDim!=1)return Stop(O->GetPathName(),Name.ToString(),TEXT("scalar bool descriptor"),TEXT("missing/changed"));Value=P->GetPropertyValue_InContainer(O);return true;}
 bool EmptyUserData(const TCHAR* Name)
 {auto* P=CastField<FArrayProperty>(Field(UPCGComponent::StaticClass(),FName(Name)));if(!P||P->ArrayDim!=1)return Check(false,Name,TEXT("native array"),TEXT("missing"));FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Target));return Check(A.Num()==0,Name,TEXT("empty (no user-data PostEditChangeOwner callback)"),FString::FromInt(A.Num()),true);}
 TSharedPtr<FJsonObject> Source()
 {
  auto J=MakeShared<FJsonObject>();if(!W.Generated||!W.Generated->ClassDefaultObject.IsResolved()||W.Generated->ClassDefaultObject.Get()!=CDO)return nullptr;
  auto ReadSourceBool=[&](FName Name,bool Default){auto* P=FindFProperty<FBoolProperty>(W.Generated,Name);return P&&P->ArrayDim==1?P->GetPropertyValue_InContainer(CDO):Default;};
  EPCGComponentGenerationTrigger Trigger=EPCGComponentGenerationTrigger::GenerateOnLoad;FProperty* P=FindFProperty<FProperty>(W.Generated,TriggerName);
  if(P&&P->ArrayDim==1){if(auto* E=CastField<FEnumProperty>(P)){if(!E->GetUnderlyingProperty())return nullptr;Trigger=static_cast<EPCGComponentGenerationTrigger>(E->GetUnderlyingProperty()->GetSignedIntPropertyValue(E->ContainerPtrToValuePtr<void>(CDO)));}else if(auto* B=CastField<FByteProperty>(P))Trigger=static_cast<EPCGComponentGenerationTrigger>(B->GetPropertyValue_InContainer(CDO));}
  J->SetNumberField(TEXT("generation_trigger"),uint8(Trigger));J->SetBoolField(TEXT("regenerate"),ReadSourceBool(RegenerateName,true));J->SetBoolField(TEXT("drop"),DropName.IsNone()?DropFallback:ReadSourceBool(DropName,false));
  for(FName Name:{TriggerName,RegenerateName,DropName})if(!Name.IsNone()){auto* Current=FindFProperty<FProperty>(W.Generated,Name);J->SetStringField(Name.ToString()+TEXT("_descriptor"),Current?DirectPropertyShape(Current):TEXT("missing_original_fallback"));}
  return J;
 }
 TSharedPtr<FJsonObject> Observe()
 {
  if(!Check(Target&&Target->GetClass()==UPCGComponent::StaticClass()&&Target->HasAnyFlags(RF_ArchetypeObject)&&!Target->HasAnyFlags(RF_ClassDefaultObject|RF_Transient),TEXT("class/template"),TEXT("exact native non-CDO own-SCS archetype"),TEXT("other profile"),true))return nullptr;
  if(!Check(W.SCS&&W.SCS->GetAllNodes()==Nodes&&Nodes.Contains(Node)&&Node->VariableGuid==NodeGuid&&Node->GetVariableName()==ComponentName&&Node->ComponentTemplate.IsResolved()&&Node->ComponentTemplate.Get()==Target&&Node->ComponentClass.IsResolved()&&Node->ComponentClass.Get()==UPCGComponent::StaticClass()&&Node->IsIn(W.SCS)&&Target->GetOuter()==W.Generated&&Target->GetPackage()==W.Blueprint->GetPackage(),TEXT("SCS membership"),TEXT("captured own node/template/class"),TEXT("changed")))return nullptr;
  if(!Check(!Target->GetOwner()&&!Target->GetWorld()&&!Target->IsRegistered()&&!Target->IsActive()&&!Target->bGenerated&&!Target->IsGenerating()&&!Target->IsCleaningUp()&&!Target->IsRefreshInProgress()&&Target->GetGeneratedGraphOutput().TaggedData.IsEmpty(),TEXT("runtime activity"),TEXT("owner/world null, unregistered inactive and no generated/tasks"),TEXT("active/changed"),true)||!EmptyUserData(TEXT("AssetUserData"))||!EmptyUserData(TEXT("AssetUserDataEditorOnly")))return nullptr;
  auto* InstanceProperty=CastField<FObjectProperty>(Field(UPCGComponent::StaticClass(),TEXT("GraphInstance")));if(!InstanceProperty)return Check(false,TEXT("GraphInstance descriptor"),TEXT("native object field"),TEXT("missing/changed")),nullptr;auto* InstanceSlot=InstanceProperty->GetObjectPtrPropertyValuePtr(InstanceProperty->ContainerPtrToValuePtr<void>(Target));
  if(!Check(InstanceSlot->IsResolved()&&InstanceSlot->Get()==Instance&&Instance&&Instance->GetClass()==UPCGGraphInstance::StaticClass()&&Instance->GetOuter()==Target&&Instance->Graph.IsResolved()&&GraphSourceKnown&&Instance->Graph==(GraphSource?GraphSource->Observer.Source:nullptr),TEXT("graph instance"),TEXT("same owned native instance and immutable direct graph source; scalar parameters observed logically"),TEXT("source or parameters changed"),true))return nullptr;
  if(!ObjectFlagsCurrent(Target,TargetFlags,TEXT("template"))||!ObjectFlagsCurrent(Instance,InstanceFlags,TEXT("graph_instance"))||!ObjectFlagsCurrent(CDO,CDOFlags,TEXT("CDO")))return nullptr;
  if(GraphSource&&!GraphSource->Current())return nullptr;
  bool Auto=false;if(!ReadBool(Target,TEXT("bAutoActivate"),Auto))return nullptr;
  TSharedPtr<FJsonObject> UserParameters;FString ParameterError;if(!CopyScalarUserParameters(GraphSource?GraphSource->Observer.Source:nullptr,Instance,UserParameters,ParameterError))return Stop(Target->GetPathName(),TEXT("scalar user_parameters"),TEXT("preserved logical schema/default/actual/override state"),ParameterError,true),nullptr;
  // Re-find the native field after the last descriptor read; do not reuse a
  // pre-getter slot or accept a replacement GI as a new baseline.
  InstanceProperty=CastField<FObjectProperty>(Field(UPCGComponent::StaticClass(),TEXT("GraphInstance")));if(!InstanceProperty)return nullptr;InstanceSlot=InstanceProperty->GetObjectPtrPropertyValuePtr(InstanceProperty->ContainerPtrToValuePtr<void>(Target));
  if(!Check(InstanceSlot->IsResolved()&&*InstanceSlot==Instance&&Instance->Graph.IsResolved()&&Instance->Graph==(GraphSource?GraphSource->Observer.Source:nullptr)&&Node->ComponentTemplate.IsResolved()&&Node->ComponentTemplate==Target&&Target->GetOuter()==W.Generated,TEXT("post_parameter_read_owner_slots"),TEXT("same captured SCS template, target GI and GI graph"),TEXT("changed during const parameter read")))return nullptr;
  auto J=MakeShared<FJsonObject>();J->SetObjectField(TEXT("user_parameters"),UserParameters);J->SetNumberField(TEXT("generation_trigger"),uint8(Target->GenerationTrigger));J->SetBoolField(TEXT("regenerate"),Target->bRegenerateInEditor);J->SetBoolField(TEXT("drop"),Target->bGenerateOnDropWhenTriggerOnDemand);J->SetBoolField(TEXT("auto_activate"),Auto);return J;
 }
 bool Current()
 {
  auto SourceNow=Source();if(!SourceNow)return Stop(TEXT("Blueprint_CDO"),TEXT("default object"),TEXT("same held already-created CDO"),TEXT("changed"));FString Path,Want,Got;
  if(ScopedDifference(MakeShared<FJsonValueObject>(Inputs),MakeShared<FJsonValueObject>(SourceNow),Path,Want,Got,true))return Stop(CDO->GetPathName(),Path,Want,Got);
  auto Now=Observe();if(!Now)return false;Path.Reset();if(ScopedDifference(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Now),Path,Want,Got,true))return Stop(Target->GetPathName(),Path,Want,Got);return true;
 }
 bool Capture(USCS_Node* InNode,UPCGComponent* InTarget,const FString& Trigger,const FString& Regenerate,const FString& Drop,bool InDropFallback,const TSharedPtr<FBindingGraphSource>& ImmutableSource=nullptr,bool Reacquiring=false)
 {
  Node=InNode;Target=InTarget;ComponentName=Node->GetVariableName();TriggerName=FName(*Trigger);RegenerateName=FName(*Regenerate);DropName=FName(*Drop);DropFallback=InDropFallback;
  if(!W.Generated||!W.Read(TEXT("GetDefaultObject_existing_template_sync"),[&]{return W.Generated->GetDefaultObject(false);},CDO)||!CDO||!W.Pin(CDO))return W.Fail(TEXT("pcg_binding_template_CDO_pending"),TEXT("Original may create a CDO; this bounded template profile requires the already-created generated CDO"));
  if(!W.Read(TEXT("GetGraphInstance_template_sync"),[&]{return Target->GetGraphInstance();},Instance)||!Instance||!W.Pin(Instance))return false;
  if(!Instance->Graph.IsResolved())return Stop(Instance->GetPathName(),TEXT("Graph"),TEXT("already resolved source or null"),TEXT("unresolved"),true);
  if(Reacquiring){GraphSource=ImmutableSource;GraphSourceKnown=true;if(Instance->Graph!=(GraphSource?GraphSource->Observer.Source:nullptr)||(GraphSource&&!GraphSource->Current()))return Stop(Instance->GetPathName(),TEXT("compiled source reference"),TEXT("original held immutable external source or original null"),TEXT("changed"));}
  else if(Instance->Graph!=nullptr)
  {
   const auto Copied=Instance->Graph;UObject* Resident=StaticFindObject(UObject::StaticClass(),nullptr,*Copied.GetPathName());TStrongObjectPtr<UObject> Hold(Live(Resident)?Resident:nullptr);
   if(!Resident||Copied!=Resident||Instance->Graph!=Copied||Resident->GetClass()!=UPCGGraph::StaticClass())return Stop(Instance->GetPathName(),TEXT("Graph"),TEXT("same resolved resident exact native graph"),TEXT("changed/other"),true);
   GraphSource=MakeShared<FBindingGraphSource>(W);if(!GraphSource->Capture(static_cast<UPCGGraph*>(Resident)))return false;GraphSourceKnown=true;
  }
  else GraphSourceKnown=true;
  Nodes=W.SCS->GetAllNodes();NodeGuid=Node->VariableGuid;TargetFlags=Target->GetFlags();InstanceFlags=Instance->GetFlags();CDOFlags=CDO->GetFlags();Inputs=Source();Expected=Observe();if(!Inputs||!Expected)return false;
  Receipt->SetStringField(TEXT("template"),Target->GetPathName());Receipt->SetStringField(TEXT("CDO"),CDO->GetPathName());Receipt->SetObjectField(TEXT("source_defaults"),Inputs);auto Before=MakeShared<FJsonObject>();Before->Values=Expected->Values;Receipt->SetObjectField(TEXT("before"),Before);Receipt->SetBoolField(TEXT("PCG_refresh_generate_called"),false);Receipt->SetBoolField(TEXT("full_authored_template_or_global_callback_purity_proven"),false);Receipt->SetStringField(TEXT("scope"),TEXT("Exact native own-SCS inactive archetype, empty or preserved saved native CPU CreatePoints/point-pipeline/finite Weighted source with supported scalar parameter bags, empty user data; selected four fields and original CDO default inputs/current roles. Null-property Super/global delegates still run. Other PCG sources and enum/struct/ref/container parameters, arbitrary subclass/inherited template, full unrelated authored/cache proof, CDO creation and broader Blueprint compiler propagation remain pending."));
  bool SyncRequested=true;W.R.Data->TryGetBoolField(TEXT("template_defaults_sync_requested"),SyncRequested);Receipt->SetBoolField(TEXT("requested"),SyncRequested);Receipt->SetBoolField(TEXT("readonly_preservation_only"),!SyncRequested);
  W.Mutation->SetObjectField(SyncRequested?TEXT("template_sync"):TEXT("template_compile_preservation"),Receipt);return Current();
 }
 template<typename F>bool Assign(const TCHAR* Stage,const TCHAR* Key,const TSharedPtr<FJsonValue>& Desired,F&& Fn)
 {
  // Exact native direct assignments and ActorComponent::SetAutoActivate on this
  // unregistered profile have no callback; only Modify/PostEditChange use Write.
  if(!W.SDK(Stage,true))return false;Fn();auto Now=Observe();if(!Now)return false;const auto Actual=Now->TryGetField(Key);Receipt->SetObjectField(TEXT("last_observed"),Now);Receipt->SetStringField(TEXT("last_assignment"),Stage);
  FString Path,Want,Got;if(ScopedDifference(Expected->TryGetField(Key),Actual,Path,Want,Got,true))W.R.bChanged=true;
  Expected->SetField(Key,Desired);if(!W.After(Stage))return false;return true;
 }
 bool Apply(const TSharedPtr<FJsonObject>& Legacy)
 {
  if(!W.Write(TEXT("PCG_template_Modify"),[&]{Target->Modify();}))return false;
  const auto Trigger=static_cast<EPCGComponentGenerationTrigger>(uint8(Inputs->GetNumberField(TEXT("generation_trigger"))));const bool Regenerate=Inputs->GetBoolField(TEXT("regenerate")),Drop=Trigger==EPCGComponentGenerationTrigger::GenerateOnDemand&&Inputs->GetBoolField(TEXT("drop"));
  if(!Assign(TEXT("PCG_template_GenerationTrigger"),TEXT("generation_trigger"),MakeShared<FJsonValueNumber>(uint8(Trigger)),[&]{Target->GenerationTrigger=Trigger;})||!Assign(TEXT("PCG_template_GenerateOnDrop"),TEXT("drop"),MakeShared<FJsonValueBoolean>(Drop),[&]{Target->bGenerateOnDropWhenTriggerOnDemand=Drop;})||!Assign(TEXT("PCG_template_SetAutoActivate"),TEXT("auto_activate"),MakeShared<FJsonValueBoolean>(true),[&]{Target->SetAutoActivate(true);})||!Assign(TEXT("PCG_template_RegenerateInEditor"),TEXT("regenerate"),MakeShared<FJsonValueBoolean>(Regenerate),[&]{Target->bRegenerateInEditor=Regenerate;}))return false;
  if(!W.Write(TEXT("PCG_template_PostEditChange_null_property"),[&]{Receipt->SetBoolField(TEXT("PostEditChange_entered"),true);Target->PostEditChange();Receipt->SetBoolField(TEXT("PostEditChange_returned"),true);}))return false;
  Legacy->SetBoolField(TEXT("template_defaults_synced"),true);const UEnum* Enum=StaticEnum<EPCGComponentGenerationTrigger>();Legacy->SetStringField(TEXT("template_generation_trigger"),Enum?Enum->GetNameStringByValue(int64(Trigger)):FString::FromInt(int32(Trigger)));Legacy->SetBoolField(TEXT("template_regenerate_in_editor"),Regenerate);Legacy->SetBoolField(TEXT("template_generate_on_drop_when_on_demand"),Inputs->GetBoolField(TEXT("drop")));Receipt->SetObjectField(TEXT("after"),Expected);Receipt->SetBoolField(TEXT("completed"),true);return true;
 }
};

// ADV: feature-local compiler seam, not a global construction-health release.
// All old strong holds survive; only target compiler children may retire.
struct FBindingCompiler
{
 FWork& W;FName ComponentName;FGuid ComponentGuid,SelectedGuid,ApplyGuid;FString SelectedName;
 TSharedPtr<FJsonObject> Before,Expected;TMap<FString,TSharedPtr<FJsonObject>> FileBefore;TMap<FString,UPackage*> FilePackages;
 UClass* OldGenerated=nullptr;UClass* OldSkeleton=nullptr;UClass* SkeletonParent=nullptr;UObject* OldCDO=nullptr;UObject* OldSKELCDO=nullptr;
 TArray<TStrongObjectPtr<UObject>> CompilerChildren;TSharedPtr<FBindingTemplate> OldTemplate;
 explicit FBindingCompiler(FWork& In):W(In){}
 bool Stop(const FString& Object,const FString& Property,const FString& Want,const FString& Got,const TCHAR* Code=TEXT("pcg_binding_compile_profile_pending"))
 {
  if(!W.R.Data->HasField(TEXT("first_compile_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),W.R.Stage);J->SetStringField(TEXT("object"),Object);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Want);J->SetStringField(TEXT("actual"),Got);W.R.Data->SetObjectField(TEXT("first_compile_mismatch"),J);}
  return W.Fail(Code,TEXT("Scoped compiler admission/authored/current proof failed; actual prefix retained, no save or automatic retry."));
 }
 bool Equal(const TSharedPtr<FJsonObject>& A,const TSharedPtr<FJsonObject>& B,const FString& Object)
 {if(!A||!B)return false;FString P,E,V;return !ScopedDifference(MakeShared<FJsonValueObject>(A),MakeShared<FJsonValueObject>(B),P,E,V,true)||Stop(Object,P,E,V,TEXT("pcg_binding_compile_authored_changed"));}
 FString Role(const UObject* O)const
 {if(!O)return TEXT("<null>");if(O==W.Generated)return TEXT("$Generated");if(O==W.Skeleton)return TEXT("$Skeleton");if(W.Generated&&O==W.Generated->ClassDefaultObject.Get())return TEXT("$CDO");if(W.Skeleton&&O==W.Skeleton->ClassDefaultObject.Get())return TEXT("$SKELCDO");return O->GetPathName();}
 bool Type(const FEdGraphPinType& Source,TSharedPtr<FJsonObject>& J,const FString& Object=TEXT("pin"),FString NodeName={},FString PinName={},int32 Direction=-1)
 {
  // FEdGraphPinType and its member/terminal descriptors are copied before any
  // tracked object admission; never retain a reference into live node storage.
  const FEdGraphPinType T=Source;const TObjectPtr<UObject> Null;const bool ParentPresent=T.PinSubCategoryMemberReference.MemberParent.GetHandle()!=Null.GetHandle();const bool MemberPresent=ParentPresent||!T.PinSubCategoryMemberReference.MemberName.IsNone()||T.PinSubCategoryMemberReference.MemberGuid.IsValid();
  const bool InactiveTerminal=!T.PinValueType.TerminalCategory.IsNone()||!T.PinValueType.TerminalSubCategory.IsNone()||T.PinValueType.TerminalSubCategoryObject.IsValid()||T.PinValueType.bTerminalIsConst||T.PinValueType.bTerminalIsWeakPointer||T.PinValueType.bTerminalIsUObjectWrapper;
  auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("object"),Object);D->SetStringField(TEXT("node_name"),NodeName);D->SetStringField(TEXT("pin_name"),PinName);D->SetNumberField(TEXT("pin_direction"),Direction);D->SetStringField(TEXT("category"),T.PinCategory.ToString());D->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());D->SetNumberField(TEXT("container"),uint8(T.ContainerType));D->SetBoolField(TEXT("object_present"),T.PinSubCategoryObject.IsValid());D->SetBoolField(TEXT("reference"),T.bIsReference);D->SetBoolField(TEXT("const"),T.bIsConst);D->SetBoolField(TEXT("weak"),T.bIsWeakPointer);D->SetBoolField(TEXT("wrapper"),T.bIsUObjectWrapper);D->SetBoolField(TEXT("single"),T.bSerializeAsSinglePrecisionFloat);D->SetStringField(TEXT("terminal_category"),T.PinValueType.TerminalCategory.ToString());D->SetStringField(TEXT("terminal_subcategory"),T.PinValueType.TerminalSubCategory.ToString());D->SetBoolField(TEXT("terminal_object_present"),T.PinValueType.TerminalSubCategoryObject.IsValid());D->SetBoolField(TEXT("terminal_const"),T.PinValueType.bTerminalIsConst);D->SetBoolField(TEXT("terminal_weak"),T.PinValueType.bTerminalIsWeakPointer);D->SetBoolField(TEXT("terminal_wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);D->SetBoolField(TEXT("member_parent_present"),ParentPresent);D->SetBoolField(TEXT("member_parent_resolved"),T.PinSubCategoryMemberReference.MemberParent.IsResolved());D->SetStringField(TEXT("member_name"),T.PinSubCategoryMemberReference.MemberName.ToString());D->SetStringField(TEXT("member_guid"),T.PinSubCategoryMemberReference.MemberGuid.ToString());D->SetBoolField(TEXT("map_value_type_active"),T.IsMap());
  auto Reject=[&](const TCHAR* Reason){D->SetStringField(TEXT("rejection_reason"),Reason);if(!W.R.Data->HasField(TEXT("first_compile_type_details")))W.R.Data->SetObjectField(TEXT("first_compile_type_details"),D);return Stop(Object,TEXT("type"),TEXT("scalar or stock delegate logical type; containers remain pending"),Json(D));};
  if(T.ContainerType!=EPinContainerType::None)return Reject(TEXT("container_type_pending"));
  const bool Delegate=T.PinCategory==UEdGraphSchema_K2::PC_Delegate||T.PinCategory==UEdGraphSchema_K2::PC_MCDelegate;
  if(MemberPresent&&!Delegate)return Reject(TEXT("member_role_on_non_delegate_pending"));
  if(ParentPresent&&!T.PinSubCategoryMemberReference.MemberParent.IsResolved())return Reject(TEXT("delegate_member_parent_unresolved"));
  // Engine serializes PinValueType only for maps (EdGraphPin.cpp:215-220),
  // and K2 compatibility/TypeToText read it under IsMap. This scalar slice does
  // not turn inactive terminal residue into a new authored invariant.
  UObject* O=T.PinSubCategoryObject.Get();TStrongObjectPtr<UObject> Hold(Live(O)?O:nullptr);UObject* Parent=ParentPresent?T.PinSubCategoryMemberReference.MemberParent.Get():nullptr;TStrongObjectPtr<UObject> ParentHold(Live(Parent)?Parent:nullptr);
  if((O&&!W.Pin(O))||(Parent&&!W.Pin(Parent)))return false;if(Parent&&!Live(Parent))return Reject(TEXT("delegate_member_parent_unavailable"));
  J=MakeShared<FJsonObject>();J->SetStringField(TEXT("category"),T.PinCategory.ToString());J->SetStringField(TEXT("subcategory"),T.PinSubCategory.ToString());J->SetStringField(TEXT("object"),Role(O));J->SetBoolField(TEXT("reference"),T.bIsReference);J->SetBoolField(TEXT("const"),T.bIsConst);J->SetBoolField(TEXT("weak"),T.bIsWeakPointer);J->SetBoolField(TEXT("wrapper"),T.bIsUObjectWrapper);J->SetBoolField(TEXT("single"),T.bSerializeAsSinglePrecisionFloat);
  if(Delegate){auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("name"),T.PinSubCategoryMemberReference.MemberName.ToString());M->SetStringField(TEXT("guid"),T.PinSubCategoryMemberReference.MemberGuid.ToString());M->SetStringField(TEXT("parent"),Role(Parent));J->SetObjectField(TEXT("delegate_member"),M);}
  if((MemberPresent||InactiveTerminal)&&!W.R.Data->HasField(TEXT("compile_type_profile_observation"))){D->SetStringField(TEXT("profile_action"),Delegate?TEXT("delegate_member_preserved_in_logical_type"):TEXT("nonmap_terminal_inactive"));W.R.Data->SetObjectField(TEXT("compile_type_profile_observation"),D);}return true;
 }
 bool Member(const FMemberReference& R,TSharedPtr<FJsonObject>& J)
 {
  const FMemberReference Owned=R;UClass* P=nullptr;TStrongObjectPtr<UClass> Hold(nullptr);if(!W.Read(TEXT("Compile_authored_member"),[&]{auto* V=Owned.GetMemberParentClass();if(Live(V))Hold.Reset(V);return V;},P)||(P&&!W.Pin(P)))return false;
  J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),Owned.GetMemberName().ToString());J->SetStringField(TEXT("guid"),Owned.GetMemberGuid().ToString());J->SetStringField(TEXT("parent"),Role(P));J->SetBoolField(TEXT("self"),Owned.IsSelfContext());J->SetBoolField(TEXT("local"),Owned.IsLocalScope());J->SetStringField(TEXT("scope"),Owned.GetMemberScopeName());return true;
 }
 bool Variables(const TArray<FBPVariableDescription>& Vars,TArray<TSharedPtr<FJsonValue>>& Out)
 {
  const TArray<FBPVariableDescription> Owned=Vars;for(const auto& V:Owned){auto J=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> T;if(!Type(V.VarType,T,TEXT("member/")+V.VarName.ToString()))return false;J->SetStringField(TEXT("name"),V.VarName.ToString());J->SetStringField(TEXT("guid"),V.VarGuid.ToString());J->SetObjectField(TEXT("type"),T);J->SetStringField(TEXT("friendly"),V.FriendlyName);const FText Category=V.Category;const FString CategoryText=Category.ToString();if(!W.Admit(TEXT("compile_owned_variable_text")))return false;J->SetStringField(TEXT("category"),CategoryText);J->SetStringField(TEXT("flags"),LexToString(V.PropertyFlags));J->SetStringField(TEXT("notify"),V.RepNotifyFunc.ToString());J->SetNumberField(TEXT("replication"),uint8(V.ReplicationCondition));J->SetStringField(TEXT("default"),V.DefaultValue);auto M=MakeShared<FJsonObject>();for(const auto& E:V.MetaDataArray){const FString Key=E.DataKey.ToString();if(M->HasField(Key))return Stop(V.VarName.ToString(),TEXT("metadata"),TEXT("unique keys"),TEXT("duplicate"));M->SetStringField(Key,E.DataValue);}J->SetObjectField(TEXT("metadata"),M);Out.Add(MakeShared<FJsonValueObject>(J));}return true;
 }
 bool Graphs(TSharedPtr<FJsonObject>& Out);
 bool Membership(const FGraphValue& V)
 {
  // Close callbacks before touching any borrowed pin again. Text conversion is
  // deliberately absent here; all text observers below operate on owned copies.
  if(!W.Admit(TEXT("compile_observer_graph_current")))return false;auto* G=V.Graph;
  if(!Live(G)||G->GraphGuid!=V.Guid||G->Nodes.Num()!=V.Nodes.Num())return Stop(TEXT("graph"),TEXT("observer membership"),TEXT("same graph/node/pin slots after callback"),TEXT("reconstructed"));
  for(int32 I=0;I<V.Nodes.Num();++I)if(G->Nodes[I]!=V.Nodes[I])return Stop(G->GetPathName(),TEXT("node slot"),TEXT("same held node"),TEXT("replaced"));
  for(const auto& N:V.Slots){if(!Live(N.Node)||N.Node->GetGraph()!=G||N.Node->GetOuter()!=G||N.Node->NodeGuid!=N.Id||N.Node->Pins.Num()!=N.Pins.Num())return Stop(G->GetPathName(),TEXT("node/pin role"),TEXT("same held node and raw pin membership"),TEXT("changed"));for(int32 I=0;I<N.Pins.Num();++I)if(N.Node->Pins[I]!=N.Pins[I])return Stop(G->GetPathName(),TEXT("pin slot"),TEXT("same raw pin before dereference"),TEXT("replaced"));}
  for(const auto& P:V.Pins){const auto* A=P.Raw;if(A->GetOwningNode()!=P.Owner||A->PinId!=P.Id||A->PinName!=P.Name||A->Direction!=P.Direction||A->ParentPin!=P.Parent||A->SubPins!=P.Sub||A->LinkedTo!=P.Links||A->PinType!=P.Type||A->PinType.bIsUObjectWrapper!=P.Type.bIsUObjectWrapper||A->PinType.bSerializeAsSinglePrecisionFloat!=P.Type.bSerializeAsSinglePrecisionFloat||A->PinType.PinValueType.bTerminalIsUObjectWrapper!=P.Type.PinValueType.bTerminalIsUObjectWrapper||!Same(A->DefaultValue,P.Default)||A->DefaultObject!=P.DefaultObject)return Stop(G->GetPathName(),TEXT("pin header/value/links"),TEXT("same captured pins at observer boundary"),TEXT("changed"));}return true;
 }
 bool Values(UObject* Object,TSharedPtr<FJsonObject>& Out,bool IsCDO=false);
 bool State(TSharedPtr<FJsonObject>& Out);
 bool Unselected(const TSharedPtr<FJsonObject>& A,const TSharedPtr<FJsonObject>& B)
 {
  auto Project=[&](const TSharedPtr<FJsonObject>& Source){auto J=MakeShared<FJsonObject>();J->SetField(TEXT("variables"),Source->TryGetField(TEXT("variables")));J->SetField(TEXT("defaults"),Source->TryGetField(TEXT("defaults")));auto G=MakeShared<FJsonObject>();G->Values=Source->GetObjectField(TEXT("graphs"))->Values;G->RemoveField(SelectedGuid.ToString());J->SetObjectField(TEXT("graphs"),G);auto C=MakeShared<FJsonObject>();for(const auto& P:Source->GetObjectField(TEXT("components"))->Values){auto Row=MakeShared<FJsonObject>();Row->Values=P.Value->AsObject()->Values;if(Same(FString(P.Key.ToView()),ComponentGuid.ToString())){auto V=MakeShared<FJsonObject>();V->Values=Row->GetObjectField(TEXT("values"))->Values;for(const TCHAR* K:{TEXT("GenerationTrigger"),TEXT("bRegenerateInEditor"),TEXT("bGenerateOnDropWhenTriggerOnDemand"),TEXT("bAutoActivate")})V->RemoveField(K);Row->SetObjectField(TEXT("values"),V);}C->SetObjectField(P.Key,Row);}J->SetObjectField(TEXT("components"),C);return TSharedPtr<FJsonObject>(J);};return Equal(Project(A),Project(B),TEXT("unselected authored source"));
 }
 bool NoInstances();bool Files(bool Capture);
 bool Prepare(const TSharedPtr<FBindingTemplate>& Template);
 bool Execute(UK2Node_CallFunction*& Apply,TSharedPtr<FOwnHelperResolution>& Helper,TSharedPtr<FBindingTemplate>& Template,const TSharedPtr<FJsonObject>& Legacy);
};

bool FBindingCompiler::Graphs(TSharedPtr<FJsonObject>& Out)
{
 TArray<UEdGraph*> All;TArray<TStrongObjectPtr<UEdGraph>> Holds;bool Found=false;
 if(!W.Read(TEXT("Compile_GetAllGraphs"),[&]{W.Blueprint->GetAllGraphs(All);for(auto* G:All)if(Live(G))Holds.Emplace(G);return true;},Found))return false;
 if(All.Num()>64)return Stop(TEXT("Blueprint"),TEXT("graphs"),TEXT("at most 64 owned K2 graphs"),TEXT("larger graph set pending"));
 Out=MakeShared<FJsonObject>();TGuardValue<bool> Capture(W.Capturing,true);TGuardValue<UEdGraph*> RestoreGraph(W.Graph,W.Graph);TGuardValue<const UEdGraphSchema_K2*> RestoreSchema(W.Schema,W.Schema);
 for(auto* G:All)
 {
  if(!Live(G)||!G->IsIn(W.Blueprint)||G->GetPackage()!=W.Blueprint->GetPackage()||!G->GraphGuid.IsValid()||!G->SubGraphs.IsEmpty())return Stop(TEXT("graph"),TEXT("ownership/GUID/subgraphs"),TEXT("flat owned graphs with valid GUID"),TEXT("nested or unavailable profile"));
  W.Graph=G;TStrongObjectPtr<const UEdGraphSchema_K2> SH(nullptr);if(!W.Read(TEXT("Compile_graph_schema"),[&]{auto* S=Cast<const UEdGraphSchema_K2>(G->GetSchema());if(Live(S))SH.Reset(S);return S;},W.Schema)||!W.Schema||W.Schema->GetClass()!=UEdGraphSchema_K2::StaticClass())return Stop(G->GetPathName(),TEXT("schema"),TEXT("native K2"),TEXT("custom schema pending"));
  FGraphValue V;if(!CaptureGraph(W,V))return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),G->GetName());auto Nodes=MakeShared<FJsonObject>();TMap<UEdGraphPin*,FString> Keys;
  for(const auto& P:V.Pins){if(P.Parent||!P.Sub.IsEmpty()||P.Orphan)return Stop(G->GetPathName(),TEXT("pins"),TEXT("unsplit nonorphan logical pins"),TEXT("split/orphan pending"));const FString Key=P.Owner->NodeGuid.ToString()+TEXT("/")+LexToString(int32(P.Direction))+TEXT("/")+P.Name.ToString();for(const auto& E:Keys)if(Same(E.Value,Key))return Stop(G->GetPathName(),TEXT("logical pin key"),TEXT("unique direction/name per node"),Key);Keys.Add(P.Raw,Key);}
  for(const auto& N:V.Slots)
  {
   if(!Membership(V))return false;auto* Node=N.Node;const UClass* C=Node->GetClass();if(!N.Id.IsValid()||Nodes->HasField(N.Id.ToString()))return Stop(G->GetPathName(),TEXT("node GUID"),TEXT("unique valid GUIDs"),TEXT("invalid/duplicate"));
   if(C!=UK2Node_FunctionEntry::StaticClass()&&C!=UK2Node_FunctionResult::StaticClass()&&C!=UK2Node_CallFunction::StaticClass()&&C!=UK2Node_VariableGet::StaticClass()&&C!=UK2Node_VariableSet::StaticClass()&&C!=UK2Node_Event::StaticClass())return Stop(Node->GetPathName(),TEXT("authored node class"),TEXT("stock Entry/Result/Call/Get/Set/Event"),C->GetPathName());
   auto NJ=MakeShared<FJsonObject>();NJ->SetStringField(TEXT("class"),C->GetPathName());NJ->SetNumberField(TEXT("x"),N.X);NJ->SetNumberField(TEXT("y"),N.Y);NJ->SetStringField(TEXT("comment"),N.Comment);NJ->SetBoolField(TEXT("bubble_visible"),N.BubbleVisible);NJ->SetBoolField(TEXT("bubble_pinned"),N.BubblePinned);
   TSharedPtr<FJsonObject> M;if(auto* Call=Cast<UK2Node_CallFunction>(Node)){if(!Member(Call->FunctionReference,M)||!Membership(V))return false;NJ->SetObjectField(TEXT("function"),M);}else if(auto* Var=Cast<UK2Node_Variable>(Node)){if(!Member(Var->VariableReference,M)||!Membership(V))return false;NJ->SetObjectField(TEXT("variable"),M);}else if(auto* Term=Cast<UK2Node_FunctionTerminator>(Node)){if(!Member(Term->FunctionReference,M)||!Membership(V))return false;NJ->SetObjectField(TEXT("function"),M);}
   if(auto* Entry=Cast<UK2Node_FunctionEntry>(Node))
   {
    const FName CustomName=Entry->CustomGeneratedFunctionName;const int32 ExtraFlags=Entry->GetExtraFlags();const bool EnforceConst=Entry->bEnforceConstCorrectness;const FKismetUserDeclaredFunctionMetadata D=Entry->MetaData;const TArray<FBPVariableDescription> LocalVariables=Entry->LocalVariables;NJ->SetStringField(TEXT("custom_function"),CustomName.ToString());NJ->SetNumberField(TEXT("extra_flags"),ExtraFlags);NJ->SetBoolField(TEXT("enforce_const"),EnforceConst);auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("tooltip"),D.ToolTip.ToString());Meta->SetStringField(TEXT("category"),D.Category.ToString());Meta->SetStringField(TEXT("keywords"),D.Keywords.ToString());Meta->SetStringField(TEXT("compact"),D.CompactNodeTitle.ToString());Meta->SetStringField(TEXT("title_color"),D.InstanceTitleColor.ToString());Meta->SetStringField(TEXT("deprecation"),D.DeprecationMessage);Meta->SetBoolField(TEXT("deprecated"),D.bIsDeprecated);Meta->SetBoolField(TEXT("call_in_editor"),D.bCallInEditor);Meta->SetBoolField(TEXT("thread_safe"),D.bThreadSafe);Meta->SetBoolField(TEXT("unsafe_construction"),D.bIsUnsafeDuringActorConstruction);NJ->SetObjectField(TEXT("metadata"),Meta);TArray<TSharedPtr<FJsonValue>> L;if(!Variables(LocalVariables,L)||!Membership(V))return false;NJ->SetArrayField(TEXT("local_variables"),L);
   }
   if(!Membership(V))return false;if(auto* Event=Cast<UK2Node_Event>(Node)){const FMemberReference Ref=Event->EventReference;const bool Override=Event->bOverrideFunction,Internal=Event->bInternalEvent;const FName Custom=Event->CustomFunctionName;const uint32 Flags=Event->FunctionFlags;if(!Member(Ref,M)||!Membership(V))return false;NJ->SetObjectField(TEXT("event"),M);NJ->SetBoolField(TEXT("override"),Override);NJ->SetBoolField(TEXT("internal"),Internal);NJ->SetStringField(TEXT("custom_event"),Custom.ToString());NJ->SetNumberField(TEXT("function_flags"),Flags);}
   if(auto* Editable=Cast<UK2Node_EditablePinBase>(Node)){TArray<FUserPinInfo> Definitions;for(const auto& P:Editable->UserDefinedPins){if(!P)return Stop(Node->GetPathName(),TEXT("user pins"),TEXT("nonnull authored definition"),TEXT("null"));Definitions.Add(*P);}TArray<TSharedPtr<FJsonValue>> User;for(const auto& P:Definitions){auto U=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> T;if(!Type(P.PinType,T)||!Membership(V))return false;U->SetStringField(TEXT("name"),P.PinName.ToString());U->SetObjectField(TEXT("type"),T);U->SetNumberField(TEXT("direction"),P.DesiredPinDirection);U->SetStringField(TEXT("default"),P.PinDefaultValue);User.Add(MakeShared<FJsonValueObject>(U));}NJ->SetArrayField(TEXT("user_pins"),User);}
   auto Pins=MakeShared<FJsonObject>();for(const auto& P:V.Pins)if(P.Owner==Node){auto PJ=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> T;if(!Type(P.Type,T,G->GetPathName()+TEXT("/")+Keys.FindChecked(P.Raw),N.Node->GetName(),P.Name.ToString(),int32(P.Direction))||!Membership(V))return false;PJ->SetObjectField(TEXT("type"),T);if(P.Direction==EGPD_Input){PJ->SetStringField(TEXT("default"),P.Default);PJ->SetStringField(TEXT("text"),P.Text);PJ->SetStringField(TEXT("object"),Role(P.DefaultObject));}TArray<FString> L;for(auto* Link:P.Links)L.Add(Keys.FindChecked(Link));L.Sort();TArray<TSharedPtr<FJsonValue>> Links;for(const auto& Key:L)Links.Add(MakeShared<FJsonValueString>(Key));PJ->SetArrayField(TEXT("links"),Links);Pins->SetObjectField(Keys.FindChecked(P.Raw),PJ);}NJ->SetObjectField(TEXT("pins"),Pins);Nodes->SetObjectField(N.Id.ToString(),NJ);
  }
  J->SetObjectField(TEXT("nodes"),Nodes);if(Out->HasField(G->GraphGuid.ToString()))return Stop(G->GetPathName(),TEXT("graph GUID"),TEXT("unique"),TEXT("duplicate"));Out->SetObjectField(G->GraphGuid.ToString(),J);
 }
 return W.Admit(TEXT("compiler_authored_graphs_captured"));
}

bool FBindingCompiler::Values(UObject* Object,TSharedPtr<FJsonObject>& Out,bool IsCDO)
{
 if(!Live(Object)||!W.Pin(Object)||!W.Pin(Object->GetClass()))return false;Out=MakeShared<FJsonObject>();TArray<FName> Names;
 for(TFieldIterator<FProperty> It(Object->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It){auto* P=*It;const bool Local=IsCDO&&W.Variables.ContainsByPredicate([&](const FBPVariableDescription& V){return V.VarName==P->GetFName();});if((Local||P->HasAnyPropertyFlags(CPF_Edit))&&!P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_Deprecated))Names.Add(P->GetFName());}
 Names.Sort(FNameLexicalLess());for(FName Name:Names)
 {
  TGuardValue<FString> ObjectContext(W.BudgetObject,Object->GetPathName());TGuardValue<FString> PropertyContext(W.BudgetProperty,Name.ToString());
  // These instanced relationships have separate current/empty-instance proof.
  // They are never compared using obsolete Generated/CDO paths.
  if(Name==TEXT("GraphInstance")||Name==TEXT("RootComponent")||Name==ComponentName)continue;
  FProperty* P=FindFProperty<FProperty>(Object->GetClass(),Name);if(!P||P->ArrayDim!=1)return Stop(Object->GetPathName(),Name.ToString(),TEXT("current scalar/native authored field"),TEXT("descriptor pending"));
  TArray<const FStructProperty*> Structs;const bool HasObject=P->ContainsObjectReference(Structs)||P->ContainsWeakObjectReference();
  if(HasObject)
  {
   // A generated CDO has no independent native same-class default. Only empty
   // reference containers/null object slots are admitted there, never self-comparison.
   if(IsCDO){bool Empty=false;if(auto* O=CastField<FObjectProperty>(P)){const auto* Slot=O->GetObjectPtrPropertyValuePtr(O->ContainerPtrToValuePtr<void>(Object));const TObjectPtr<UObject> Null;Empty=Slot->IsResolved()&&Slot->GetHandle()==Null.GetHandle();}else if(auto* A=CastField<FArrayProperty>(P))Empty=FScriptArrayHelper(A,A->ContainerPtrToValuePtr<void>(Object)).Num()==0;else if(auto* M=CastField<FMapProperty>(P))Empty=FScriptMapHelper(M,M->ContainerPtrToValuePtr<void>(Object)).Num()==0;else if(auto* S=CastField<FSetProperty>(P))Empty=FScriptSetHelper(S,S->ContainerPtrToValuePtr<void>(Object)).Num()==0;if(!Empty)return Stop(Object->GetPathName(),Name.ToString(),TEXT("empty/null CDO reference default"),TEXT("custom object default pending"));Out->SetStringField(Name.ToString(),TEXT("$empty_reference_default"));continue;}
   UObject* NativeDefault=Object->GetClass()->GetDefaultObject(false);TStrongObjectPtr<UObject> Hold(Live(NativeDefault)?NativeDefault:nullptr);if(!NativeDefault||NativeDefault==Object||!P->Identical_InContainer(Object,NativeDefault))return Stop(Object->GetPathName(),Name.ToString(),TEXT("unchanged native reference default"),TEXT("custom object default pending"));Out->SetStringField(Name.ToString(),TEXT("$native_reference_default"));continue;
  }
  FString Text;if(!W.Read(TEXT("Compile_export_authored_value"),[&]{FString V;P->ExportTextItem_Direct(V,P->ContainerPtrToValuePtr<void>(Object),nullptr,Object,PPF_None);return V;},Text))return false;
  if(FindFProperty<FProperty>(Object->GetClass(),Name)!=P)return Stop(Object->GetPathName(),Name.ToString(),TEXT("same descriptor at export boundary"),TEXT("replaced"));Out->SetStringField(Name.ToString(),Text);
 }
 return W.Admit(TEXT("compiler_authored_defaults_captured"));
}

bool FBindingCompiler::State(TSharedPtr<FJsonObject>& Out)
{
 if(!W.Admit(TEXT("compiler_source_state")))return false;Out=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> G;if(!Graphs(G))return false;Out->SetObjectField(TEXT("graphs"),G);TArray<TSharedPtr<FJsonValue>> Vars;if(!Variables(W.Blueprint->NewVariables,Vars))return false;Out->SetArrayField(TEXT("variables"),Vars);
 UObject* CDO=W.Generated->ClassDefaultObject.Get();TStrongObjectPtr<UObject> CDOHold(Live(CDO)?CDO:nullptr);if(!CDO||!W.Pin(CDO)||CDO->GetClass()!=W.Generated)return Stop(TEXT("CDO"),TEXT("current"),TEXT("already-created target CDO"),TEXT("missing/foreign"));TSharedPtr<FJsonObject> Defaults;if(!Values(CDO,Defaults,true))return false;Out->SetObjectField(TEXT("defaults"),Defaults);
 auto Components=MakeShared<FJsonObject>();const auto Nodes=W.SCS->GetAllNodes();if(Nodes.Num()<1||Nodes.Num()>2)return Stop(TEXT("SCS"),TEXT("nodes"),TEXT("own PCG and optional native SceneRoot"),TEXT("other component scope pending"));
 for(auto* N:Nodes)
 {
  if(!Live(N)||!W.Pin(N)||!N->IsIn(W.SCS)||!N->VariableGuid.IsValid()||!N->ComponentClass.IsResolved()||!N->ComponentTemplate.IsResolved())return Stop(TEXT("SCS"),TEXT("node role"),TEXT("resolved owned node/template/class"),TEXT("invalid"));auto* T=N->ComponentTemplate.Get();TStrongObjectPtr<UActorComponent> Hold(Live(T)?T:nullptr);if(!T||!W.Pin(T)||T->GetOuter()!=W.Generated)return Stop(N->GetPathName(),TEXT("template outer"),TEXT("current target Generated"),TEXT("foreign"));
  if((N->GetVariableName()==ComponentName&&T->GetClass()!=UPCGComponent::StaticClass())||(N->GetVariableName()!=ComponentName&&T->GetClass()!=USceneComponent::StaticClass())||N->ComponentClass.Get()!=T->GetClass())return Stop(N->GetPathName(),TEXT("component profile"),TEXT("selected exact PCG / optional exact SceneComponent"),T->GetClass()->GetPathName());
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),N->GetVariableName().ToString());J->SetStringField(TEXT("class"),T->GetClass()->GetPathName());J->SetStringField(TEXT("parent"),N->ParentComponentOrVariableName.ToString());J->SetStringField(TEXT("parent_owner"),N->ParentComponentOwnerClassName.ToString());J->SetBoolField(TEXT("native_parent"),N->bIsParentComponentNative);TArray<TSharedPtr<FJsonValue>> Children;for(auto* Child:N->GetChildNodes()){if(!Child||!Nodes.Contains(Child))return Stop(N->GetPathName(),TEXT("child"),TEXT("owned node"),TEXT("foreign"));Children.Add(MakeShared<FJsonValueString>(Child->VariableGuid.ToString()));}J->SetArrayField(TEXT("children"),Children);auto Meta=MakeShared<FJsonObject>();for(const auto& M:N->MetaDataArray)Meta->SetStringField(M.DataKey.ToString(),M.DataValue);J->SetObjectField(TEXT("metadata"),Meta);TSharedPtr<FJsonObject> V;if(!Values(T,V))return false;J->SetObjectField(TEXT("values"),V);Components->SetObjectField(N->VariableGuid.ToString(),J);
 }
 Out->SetObjectField(TEXT("components"),Components);return W.Admit(TEXT("compiler_state_complete"));
}

bool FBindingCompiler::NoInstances()
{
 if(!W.Admit(TEXT("compiler_no_live_instances")))return false;
 for(TObjectIterator<UBlueprintGeneratedClass> It;It;++It)if(Live(*It)&&*It!=W.Generated&&*It!=W.Skeleton&&(It->IsChildOf(W.Generated)||It->IsChildOf(W.Skeleton)))return Stop(It->GetPathName(),TEXT("derived Blueprint"),TEXT("no loaded child classes"),TEXT("propagation scope pending"));
 for(TObjectIterator<AActor> It;It;++It)if(Live(*It)&&!It->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)&&(It->IsA(W.Generated)||It->IsA(W.Skeleton)))return Stop(It->GetPathName(),TEXT("live target actor"),TEXT("no live instances / construction-script execution"),TEXT("live instance support pending"));return true;
}

bool FBindingCompiler::Files(bool Capture)
{
 if(Capture){for(const auto& I:W.Objects){const UObject* O=I->Object.Get();if(O&&!I->Retired&&O->GetPackage()->GetName().StartsWith(TEXT("/Game/")))FilePackages.Add(O->GetPackage()->GetName(),O->GetPackage());}FilePackages.Add(W.Blueprint->GetPackage()->GetName(),W.Blueprint->GetPackage());
  if(OldTemplate&&OldTemplate->GraphSource)for(const UObject* Asset:OldTemplate->GraphSource->Observer.AssetFiles)if(Live(Asset))FilePackages.Add(Asset->GetPackage()->GetName(),Asset->GetPackage());}
 for(const auto& Pair:FilePackages)
 {
  UPackage* Package=Pair.Value;if(!Live(Package)||!W.Pin(Package))return false;const FString Filename=FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()));TSharedPtr<FJsonObject> J;
  if(!W.Read(TEXT("Compile_source_file_boundary"),[&]{const int64 Size=IFileManager::Get().FileSize(*Filename);auto V=MakeShared<FJsonObject>();V->SetBoolField(TEXT("exists"),Size>=0);V->SetNumberField(TEXT("bytes"),Size);if(Size>=0){TArray<uint8> B;if(Size>67108864||!FFileHelper::LoadFileToArray(B,*Filename)||B.Num()!=Size)return TSharedPtr<FJsonObject>();uint8 D[FSHA1::DigestSize];FSHA1::HashBuffer(B.GetData(),B.Num(),D);V->SetStringField(TEXT("sha1"),BytesToHex(D,FSHA1::DigestSize));}if(Package!=W.Blueprint->GetPackage())V->SetBoolField(TEXT("dirty"),Package->IsDirty());return TSharedPtr<FJsonObject>(V);},J)||!J)return Stop(Filename,TEXT("source file"),TEXT("bounded observable bytes"),TEXT("unavailable"));
  if(Capture)FileBefore.Add(Pair.Key,J);else if(!Equal(FileBefore.FindChecked(Pair.Key),J,Filename))return false;
 }
 return true;
}

bool FBindingCompiler::Prepare(const TSharedPtr<FBindingTemplate>& Template)
{
 OldTemplate=Template;
 if(!Template||!W.SCS||W.Blueprint->ParentClass!=AActor::StaticClass()||W.Blueprint->BlueprintType!=BPTYPE_Normal||!W.Blueprint->ImplementedInterfaces.IsEmpty()||!W.Blueprint->Timelines.IsEmpty()||!W.Blueprint->MacroGraphs.IsEmpty()||!W.Blueprint->DelegateSignatureGraphs.IsEmpty()||W.Blueprint->bBeingCompiled||W.Blueprint->bQueuedForCompilation)return Stop(W.Blueprint->GetPathName(),TEXT("compile profile"),TEXT("direct Actor normal BP, own native PCG sync, no interface/timeline/macro/delegate or active compile"),TEXT("remaining valid scope pending"));
 ComponentName=Template->ComponentName;ComponentGuid=Template->NodeGuid;SelectedGuid=W.Graph->GraphGuid;SelectedName=W.Graph->GetName();OldGenerated=W.Generated;OldSkeleton=W.Skeleton;OldCDO=W.Generated->ClassDefaultObject.Get();OldSKELCDO=W.Skeleton?W.Skeleton->ClassDefaultObject.Get():nullptr;SkeletonParent=W.Skeleton?W.Skeleton->GetSuperClass():nullptr;
 for(auto* O:{static_cast<UObject*>(OldGenerated),static_cast<UObject*>(OldSkeleton),OldCDO,OldSKELCDO,static_cast<UObject*>(W.SCS),static_cast<UObject*>(SkeletonParent)})if(Live(O))CompilerChildren.Emplace(O);
 if(!Live(OldGenerated)||!Live(OldSkeleton)||!Live(SkeletonParent)||!W.Pin(SkeletonParent)||!NoInstances())return false;if(!State(Before)||!Files(true))return false;
 W.R.Data->SetStringField(TEXT("compile_profile"),TEXT("owned_direct_actor_native_pcg_no_live_instances_v1"));W.R.Data->SetBoolField(TEXT("global_construction_gate_changed"),false);return true;
}

bool FBindingCompiler::Execute(UK2Node_CallFunction*& Apply,TSharedPtr<FOwnHelperResolution>& Helper,TSharedPtr<FBindingTemplate>& Template,const TSharedPtr<FJsonObject>& Legacy)
{
 if(!NoInstances()||!State(Expected)||!Unselected(Before,Expected)||!Files(false))return false;ApplyGuid=Apply->NodeGuid;const FOwnHelperSignature Signature=Helper->PrimaryBefore;const FGuid HelperGuid=Helper->Guid;const FName HelperName=Helper->Name;const bool Own=Helper->Active;UFunction* External=Own?nullptr:Helper->Primary;
 if(!W.SDK(TEXT("CompileBlueprint_scoped_binding"),true))return false;const int32 CompileCallIndex=W.Calls.Num()-1;W.Compile->SetBoolField(TEXT("attempted"),true);W.Compile->SetBoolField(TEXT("returned"),false);W.R.bCompileAttempted=true;
 for(auto& I:W.Objects){const UObject* O=I->Object.Get();if(O&&(O==OldGenerated||O==OldSkeleton||O==OldCDO||O==OldSKELCDO||O==W.SCS||O->IsIn(OldGenerated)||O->IsIn(OldSkeleton)||(OldCDO&&O->IsIn(OldCDO))||(OldSKELCDO&&O->IsIn(OldSKELCDO))||O->IsIn(W.SCS)||(O->IsIn(W.Blueprint)&&(O->IsA<UEdGraph>()||O->IsA<UEdGraphNode>()))))I->Retired=true;}
 W.CompilerWindow=true;W.Graph=nullptr;W.GraphState=FGraphValue();W.Function=Own?nullptr:External;W.ScopedCurrent=nullptr;W.HelperCurrent=nullptr;
 // Target compiler children may retire; the external source never does. Keep
 // its original immutable guard through every callback-bearing reacquisition SDK.
 if(OldTemplate->GraphSource){const auto HeldSource=OldTemplate->GraphSource;W.ScopedCurrent=[HeldSource]{return HeldSource->Current();};}
 FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(W.Blueprint,EBlueprintCompileOptions::None,&Log);
 // Never dereference the old Apply, raw pins, helper children or CDO again.
 auto* Gen=W.Blueprint->GeneratedClass.Get();TStrongObjectPtr<UClass> GH(Live(Gen)?Gen:nullptr);auto* Skel=W.Blueprint->SkeletonGeneratedClass.Get();TStrongObjectPtr<UClass> KH(Live(Skel)?Skel:nullptr);auto* SCS=W.Blueprint->SimpleConstructionScript.Get();TStrongObjectPtr<USimpleConstructionScript> SH(Live(SCS)?SCS:nullptr);UObject* CDO=Live(Gen)?Gen->ClassDefaultObject.Get():nullptr;TStrongObjectPtr<UObject> CH(Live(CDO)?CDO:nullptr);UObject* SKELCDO=Live(Skel)?Skel->ClassDefaultObject.Get():nullptr;TStrongObjectPtr<UObject> KCH(Live(SKELCDO)?SKELCDO:nullptr);
 const EBlueprintStatus Status=W.Blueprint->Status.GetValue();
 W.Calls[CompileCallIndex]->AsObject()->SetBoolField(TEXT("returned"),true);W.Compile->SetBoolField(TEXT("returned"),true);W.Compile->SetNumberField(TEXT("error_count"),Log.NumErrors);W.Compile->SetNumberField(TEXT("warning_count"),Log.NumWarnings);
 TArray<TSharedPtr<FJsonValue>> Messages;for(const auto& M:Log.Messages){if(Messages.Num()>=128)break;Messages.Add(MakeShared<FJsonValueString>(M->ToText().ToString()));}W.Compile->SetArrayField(TEXT("messages"),Messages);
 FString StatusName;switch(Status){case BS_Unknown:StatusName=TEXT("unknown");break;case BS_Dirty:StatusName=TEXT("dirty");break;case BS_Error:StatusName=TEXT("error");break;case BS_UpToDate:StatusName=TEXT("up_to_date");break;case BS_BeingCreated:StatusName=TEXT("being_created");break;case BS_UpToDateWithWarnings:StatusName=TEXT("up_to_date_with_warnings");break;default:StatusName=TEXT("unknown");}
 Legacy->SetStringField(TEXT("compile_status"),StatusName);Legacy->SetBoolField(TEXT("compiled"),Status!=BS_Error);W.Compile->SetBoolField(TEXT("legacy_status_not_error"),Status!=BS_Error);
 if(!W.Admit(TEXT("compiler_returned_target_asset")))return false;
 auto RoleFailure=[&](const TCHAR* Field,const FString& ExpectedRole,const FString& ActualRole){return Stop(W.Blueprint->GetPathName(),Field,ExpectedRole,ActualRole,TEXT("pcg_binding_compile_reacquire_failed"));};
 auto PathOf=[](const UObject* O){return Live(O)?O->GetPathName():FString(TEXT("missing or invalid"));};
 if(!Live(Gen))return RoleFailure(TEXT("GeneratedClass"),TEXT("live current GeneratedClass"),PathOf(Gen));
 if(!Live(Skel))return RoleFailure(TEXT("SkeletonGeneratedClass"),TEXT("live current SkeletonGeneratedClass"),PathOf(Skel));
 if(!Live(SCS))return RoleFailure(TEXT("SimpleConstructionScript"),TEXT("live current SimpleConstructionScript"),PathOf(SCS));
 if(!Live(CDO))return RoleFailure(TEXT("ClassDefaultObject"),TEXT("live current GeneratedClass CDO"),PathOf(CDO));
 if(Gen->GetSuperClass()!=W.ParentClass)return RoleFailure(TEXT("GeneratedClass.SuperClass"),PathOf(W.ParentClass),PathOf(Gen->GetSuperClass()));
 if(Skel->GetSuperClass()!=SkeletonParent)return RoleFailure(TEXT("SkeletonGeneratedClass.SuperClass"),PathOf(SkeletonParent),PathOf(Skel->GetSuperClass()));
 if(!Gen->ClassGeneratedBy.IsResolved()||Gen->ClassGeneratedBy.Get()!=W.Blueprint)return RoleFailure(TEXT("GeneratedClass.ClassGeneratedBy"),W.Blueprint->GetPathName(),Gen->ClassGeneratedBy.IsResolved()?PathOf(Gen->ClassGeneratedBy.Get()):TEXT("unresolved"));
 if(!Skel->ClassGeneratedBy.IsResolved()||Skel->ClassGeneratedBy.Get()!=W.Blueprint)return RoleFailure(TEXT("SkeletonGeneratedClass.ClassGeneratedBy"),W.Blueprint->GetPathName(),Skel->ClassGeneratedBy.IsResolved()?PathOf(Skel->ClassGeneratedBy.Get()):TEXT("unresolved"));
 if(CDO->GetClass()!=Gen)return RoleFailure(TEXT("CDO.Class"),Gen->GetPathName(),PathOf(CDO->GetClass()));
 // Kismet2.cpp creates SCS under GeneratedClass, not under the Blueprint asset.
 // Require both current owner slots and exact Outer/package; do not admit arbitrary ownership.
 auto* BPG=Cast<UBlueprintGeneratedClass>(Gen);
 if(!BPG||BPG->SimpleConstructionScript!=SCS)return RoleFailure(TEXT("GeneratedClass.SimpleConstructionScript"),SCS->GetPathName(),BPG?PathOf(BPG->SimpleConstructionScript.Get()):TEXT("not BlueprintGeneratedClass"));
 if(SCS->GetOuter()!=Gen)return RoleFailure(TEXT("SimpleConstructionScript.Outer"),Gen->GetPathName(),PathOf(SCS->GetOuter()));
 if(SCS->GetPackage()!=W.Blueprint->GetPackage())return RoleFailure(TEXT("SimpleConstructionScript.Package"),W.Blueprint->GetPackage()->GetName(),SCS->GetPackage()->GetName());
 W.Generated=Gen;W.Skeleton=Skel;W.SCS=SCS;for(auto* O:{static_cast<UObject*>(Gen),static_cast<UObject*>(Skel),CDO,SKELCDO,static_cast<UObject*>(SCS)})if(O&&!W.Pin(O))return false;
 if(W.Blueprint->GeneratedClass!=Gen||W.Blueprint->SkeletonGeneratedClass!=Skel||W.Blueprint->SimpleConstructionScript!=SCS)return Stop(TEXT("Blueprint"),TEXT("new role callback"),TEXT("same held current roles"),TEXT("replaced"));W.CompilerWindow=false;
 TSharedPtr<FJsonObject> Actual;if(!State(Actual)||!Equal(Expected,Actual,W.Blueprint->GetPathName())||!Files(false)||!NoInstances())return false;
 TArray<UEdGraph*> GraphsNow;TArray<TStrongObjectPtr<UEdGraph>> GraphHolds;bool Found=false;if(!W.Read(TEXT("Compile_reacquire_graphs"),[&]{W.Blueprint->GetAllGraphs(GraphsNow);for(auto* G:GraphsNow)if(Live(G))GraphHolds.Emplace(G);return true;},Found))return false;UEdGraph* Selected=nullptr;
 for(auto* G:GraphsNow)if(G&&G->GraphGuid==SelectedGuid&&Same(G->GetName(),SelectedName)){if(Selected)return Stop(TEXT("graph"),TEXT("selection"),TEXT("one logical graph"),TEXT("duplicate"));Selected=G;}
 if(!Selected||!W.Pin(Selected))return Stop(TEXT("graph"),TEXT("selection"),TEXT("same logical graph"),TEXT("missing"));W.Graph=Selected;TStrongObjectPtr<const UEdGraphSchema_K2> SchemaHold(nullptr);if(!W.Read(TEXT("Compile_reacquire_schema"),[&]{auto* S=Cast<const UEdGraphSchema_K2>(Selected->GetSchema());if(Live(S))SchemaHold.Reset(S);return S;},W.Schema)||!CaptureGraph(W,W.GraphState))return false;
 Apply=nullptr;for(auto* N:W.GraphState.Nodes)if(N->NodeGuid==ApplyGuid){Apply=Cast<UK2Node_CallFunction>(N);break;}if(!Apply)return Stop(TEXT("apply node"),TEXT("GUID/class"),TEXT("same authored call"),TEXT("missing"));
 UFunction* Resolved=nullptr;TStrongObjectPtr<UFunction> RH(nullptr);if(!W.Read(TEXT("Compile_reacquire_helper"),[&]{auto* F=Own?Gen->FindFunctionByName(HelperName,EIncludeSuperFlag::ExcludeSuper):External;if(Live(F))RH.Reset(F);return F;},Resolved)||!Resolved||!W.Pin(Resolved))return false;
 W.Function=Resolved;if(Own)W.FunctionSignature=FunctionShape(Resolved);else if(!Same(FunctionShape(Resolved),W.FunctionSignature))return Stop(TEXT("external helper"),TEXT("original signature"),W.FunctionSignature,TEXT("changed"));auto NewHelper=MakeShared<FOwnHelperResolution>(W);if(!NewHelper->Bind(Resolved)||(Own&&(!(NewHelper->PrimaryBefore==Signature)||NewHelper->Guid!=HelperGuid)))return Stop(TEXT("helper"),TEXT("authored signature/GUID"),TEXT("same logical callable pair"),TEXT("changed"));Helper=NewHelper;W.HelperCurrent=[NewHelper]{return NewHelper->Current();};
 USCS_Node* Node=nullptr;for(auto* N:W.SCS->GetAllNodes())if(N&&N->VariableGuid==ComponentGuid&&N->GetVariableName()==ComponentName){if(Node)return Stop(TEXT("template"),TEXT("SCS GUID/name"),TEXT("unique"),TEXT("duplicate"));Node=N;}
 UPCGComponent* Target=Node&&Node->ComponentTemplate.IsResolved()?Cast<UPCGComponent>(Node->ComponentTemplate.Get()):nullptr;TStrongObjectPtr<UPCGComponent> TH(Live(Target)?Target:nullptr);UPCGGraphInstance* Instance=Target?Target->GetGraphInstance():nullptr;TStrongObjectPtr<UPCGGraphInstance> IH(Live(Instance)?Instance:nullptr);if(!Node||!Target||!Instance||!W.Pin(Node)||!W.Pin(Target)||!W.Pin(Instance))return Stop(TEXT("template"),TEXT("current template/GI"),TEXT("new held own native roles"),TEXT("missing"));
 auto NewTemplate=MakeShared<FBindingTemplate>(W);if(!NewTemplate->Capture(Node,Target,OldTemplate->TriggerName.ToString(),OldTemplate->RegenerateName.ToString(),OldTemplate->DropName.IsNone()?FString():OldTemplate->DropName.ToString(),OldTemplate->DropFallback,OldTemplate->GraphSource,true)||!Equal(OldTemplate->Expected,NewTemplate->Expected,TEXT("PCG template")))return false;
 for(const TCHAR* Key:{TEXT("generation_trigger"),TEXT("regenerate"),TEXT("drop")}){FString P,E,V;if(ScopedDifference(OldTemplate->Inputs->TryGetField(Key),NewTemplate->Inputs->TryGetField(Key),P,E,V,true))return Stop(TEXT("CDO"),Key,E,V,TEXT("pcg_binding_compile_authored_changed"));}
 Template=NewTemplate;W.ScopedCurrent=[NewTemplate]{return NewTemplate->Current();};UFunction* CallTarget=nullptr;TStrongObjectPtr<UFunction> FH(nullptr);if(!W.Read(TEXT("Compile_apply_resolution"),[&]{auto* F=Apply->GetTargetFunction();if(Live(F))FH.Reset(F);return F;},CallTarget)||!Helper->VerifyReference(Apply,CallTarget,true))return false;
 TSharedPtr<FJsonObject> FinalAuthored;if(!State(FinalAuthored)||!Equal(Expected,FinalAuthored,W.Blueprint->GetPathName())||!Files(false))return false;
const EBlueprintStatus CurrentStatus=W.Blueprint->Status.GetValue();const bool Ready=(Status==BS_UpToDate||Status==BS_UpToDateWithWarnings)&&!W.Blueprint->bBeingCompiled&&!W.Blueprint->bQueuedForCompilation&&(CurrentStatus==BS_UpToDate||CurrentStatus==BS_UpToDateWithWarnings)&&Log.NumErrors==0;W.Compile->SetNumberField(TEXT("current_status"),uint8(CurrentStatus));W.R.Compiled=Ready;W.Compile->SetBoolField(TEXT("postcompile_reacquire_passed"),true);W.Compile->SetBoolField(TEXT("authored_state_preserved"),true);W.Compile->SetBoolField(TEXT("success"),Ready);
 if(!Ready)return W.Fail(TEXT("pcg_binding_compile_failed"),TEXT("One explicit compile returned errors/nonready status; actual prefix retained and save suppressed"));W.CompiledCurrentRequired=true;return W.Admit(TEXT("compiled_binding_current"));
}

TSharedPtr<FJsonObject> HandleBind(FWork& W,const TSharedPtr<FJsonObject>& P)
{
 FString Name;if(!P->TryGetStringField(TEXT("blueprint_name"),Name))return W.Error(TEXT("Missing 'blueprint_name' parameter"));const FString Component=Str(P,TEXT("component_variable"),TEXT("ForestPCG")),Trigger=Str(P,TEXT("generation_trigger_variable"),TEXT("PCGGenerationTrigger")),Regenerate=Str(P,TEXT("regenerate_in_editor_variable"),TEXT("PCGRegenerateInEditor")),Drop=Str(P,TEXT("generate_on_drop_variable")),FunctionClass=Str(P,TEXT("function_class"),Str(P,TEXT("function_class_path"))),FunctionName=Str(P,TEXT("function_name"),TEXT("ApplyPCGGenerationSettings"));const bool DropDefault=Bool(P,TEXT("generate_on_drop_default"),false),Replace=Bool(P,TEXT("replace_existing"),true),Sync=Bool(P,TEXT("sync_component_template_defaults"),true),Compile=Bool(P,TEXT("compile"),false),Save=Bool(P,TEXT("save"),false);W.R.bCompileRequested=Compile;W.R.bSaveRequested=Save;if(Compile)W.R.Compiled.Reset();W.Compile->SetBoolField(TEXT("requested"),Compile);W.Compile->SetBoolField(TEXT("attempted"),false);W.Save->SetBoolField(TEXT("requested"),Save);
 if(FunctionClass.IsEmpty())return W.Error(TEXT("Missing 'function_class' parameter. Provide a runtime BlueprintFunctionLibrary class that owns ApplyPCGGenerationSettings."));if(Forbidden(FunctionClass))return W.Error(TEXT("Runtime function_class must not reference MCP/Editor plugin implementation"));
 if(Forbidden(Name))return W.Error(TEXT("Blueprint target contains an MCP-only dependency marker"));auto* B=LoadBlueprint(W,Name);if(!B)return W.Error(TEXT("Blueprint not found: ")+Name);for(const auto& V:{Component,Trigger,Regenerate})if(!HasMember(W,V))return W.Error(TEXT("Blueprint member/component not found: ")+V);if(!Drop.IsEmpty()&&!HasMember(W,Drop))return W.Error(TEXT("Blueprint generate-on-drop variable not found: ")+Drop);
 W.Graph=SelectGraph(W,P);if(!W.Graph)return W.Error(TEXT("Blueprint graph selector did not resolve an existing graph"));if(!W.Pin(W.Graph)||!W.Graph->IsIn(B)||W.Graph->GetPackage()!=B->GetPackage())return W.Error(TEXT("Selected graph is not contained in exact target Blueprint/package"));if(!W.Read(TEXT("Graph_GetSchema"),[&]{return Cast<const UEdGraphSchema_K2>(W.Graph->GetSchema());},W.Schema)||!W.Schema)return W.Error(TEXT("Failed to get K2 schema"));if(!CaptureGraph(W,W.GraphState))return W.Error(W.R.Error);UK2Node_FunctionEntry* Entry=nullptr;for(auto* N:W.GraphState.Nodes)if((Entry=Cast<UK2Node_FunctionEntry>(N)))break;if(!Entry)return W.Error(TEXT("Function entry node not found in target Blueprint graph"));
 auto* C=LoadFunctionClass(W,FunctionClass);if(!C)return W.Error(TEXT("Apply function class not found: ")+FunctionClass);auto* F=FindFunction(W,C,FunctionName);if(!F)return W.Error(TEXT("ApplyPCGGenerationSettings function not found"));if(!Callable(W,F))return W.Error(W.R.Error);W.Function=F;W.FunctionSignature=FunctionShape(F);if(C->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly)||F->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly)||Forbidden(F->GetPathName()))return W.Error(TEXT("Existing helper belongs to an Editor/MCP package; runtime binding unavailable"));if(!HelperPreflight(W,C,Component,Trigger,Regenerate,Drop))return W.Error(W.R.Error);
 TSharedPtr<FOwnHelperResolution> Helper=MakeShared<FOwnHelperResolution>(W);W.HelperCurrent=[Helper]{return Helper->Current();};if(!Helper->Bind(F))return W.Error(W.R.Error);
 auto O=MakeShared<FJsonObject>();W.R.Data->SetObjectField(TEXT("legacy_result"),O);O->SetStringField(TEXT("blueprint_name"),Name);O->SetStringField(TEXT("component_variable"),Component);O->SetStringField(TEXT("generation_trigger_variable"),Trigger);O->SetStringField(TEXT("regenerate_in_editor_variable"),Regenerate);O->SetStringField(TEXT("generate_on_drop_variable"),Drop);O->SetStringField(TEXT("function_class"),C->GetPathName());O->SetStringField(TEXT("function_name"),F->GetName());O->SetBoolField(TEXT("generate_on_drop_connected"),false);O->SetNumberField(TEXT("removed_existing_node_count"),0);O->SetNumberField(TEXT("reconnected_exec_target_count"),0);O->SetBoolField(TEXT("template_defaults_synced"),false);O->SetStringField(TEXT("template_generation_trigger"),TEXT(""));O->SetBoolField(TEXT("template_regenerate_in_editor"),true);O->SetBoolField(TEXT("template_generate_on_drop_when_on_demand"),false);O->SetBoolField(TEXT("compiled"),false);O->SetStringField(TEXT("compile_status"),TEXT("not_requested"));O->SetBoolField(TEXT("saved"),false);
 W.Mutation->SetStringField(TEXT("blueprint_path"),B->GetPathName());W.Mutation->SetStringField(TEXT("graph_path"),W.Graph->GetPathName());W.Mutation->SetStringField(TEXT("runtime_helper_class"),C->GetPathName());W.Mutation->SetStringField(TEXT("runtime_helper_function"),F->GetPathName());W.R.Data->SetBoolField(TEXT("external_runtime_helper_required"),true);W.R.Data->SetBoolField(TEXT("Cubeless_game_runtime_class_added"),false);W.R.Data->SetBoolField(TEXT("runtime_dependency_packaging_proven"),false);
 // Scoped own native template sync/compile retain request-local guards; global gate stays closed.
 // Compile needs the same readonly template/source preservation lease even
 // when the independently requested template-default sync is disabled.
 W.R.Data->SetBoolField(TEXT("template_defaults_sync_requested"),Sync);W.R.Data->SetBoolField(TEXT("template_compile_preservation_only"),Compile&&!Sync);
 UPCGComponent* Template=nullptr;USCS_Node* TemplateNode=nullptr;if((Sync||Compile)&&B->SimpleConstructionScript){const auto Nodes=B->SimpleConstructionScript->GetAllNodes();for(auto* N:Nodes){if(!N||!W.Pin(N))return W.Error(W.R.Error);if(N->GetVariableName()==FName(*Component)){Template=Cast<UPCGComponent>(N->ComponentTemplate);TemplateNode=N;if(Template&&!W.Pin(Template))return W.Error(W.R.Error);break;}}}
 TSharedPtr<FBindingTemplate> TemplateScope;if(Template){TemplateScope=MakeShared<FBindingTemplate>(W);if(!TemplateScope->Capture(TemplateNode,Template,Trigger,Regenerate,Drop,DropDefault))return W.Error(W.R.Error);W.ScopedCurrent=[TemplateScope]{return TemplateScope->Current();};}
 TSharedPtr<FBindingCompiler> Compiler;if(Compile){Compiler=MakeShared<FBindingCompiler>(W);if(!Compiler->Prepare(TemplateScope))return W.Error(W.R.Error);}
 if(Save&&!PackageOwnership(W))return W.Error(W.R.Error);
 UEdGraphPin* EntryThen=FindPin(W,Entry,EGPD_Output,{},true);if(!EntryThen)return W.Error(TEXT("Function entry exec output pin not found"));const auto* EntryState=W.GraphState.Slots.FindByPredicate([&](const FNodeValue& V){return V.Node==Entry;});const int32 EX=EntryState->X,EY=EntryState->Y;if(EX>MAX_int32-300||EY>MAX_int32-560)return W.Error(TEXT("Node placement exceeds int32"));
 TArray<UK2Node_CallFunction*> OldCalls;if(Replace){const auto BeforeNodes=W.GraphState.Nodes;for(auto* N:BeforeNodes)if(auto* Call=Cast<UK2Node_CallFunction>(N)){UFunction* Target=nullptr;if(!W.Read(TEXT("CallFunction_GetTargetFunction"),[&]{return Call->GetTargetFunction();},Target))return W.Error(W.R.Error);if(Helper->Matches(Target)){if(!Helper->VerifyReference(Call,Target))return W.Error(W.R.Error);OldCalls.Add(Call);}}}
 if(!W.Write(TEXT("Graph_Modify"),[&]{W.Graph->Modify();})||!W.Write(TEXT("Blueprint_Modify"),[&]{B->Modify();}))return W.Error(W.R.Error);
 auto* Apply=NewNode<UK2Node_CallFunction>(W,EX+300,EY,[&](UK2Node_CallFunction* N){N->SetFromFunction(F);});if(!Apply)return W.Error(W.R.Error.IsEmpty()?TEXT("Failed to create apply function node"):W.R.Error);if(!VerifyAuthoredCallDependencies(W,Apply))return W.Error(W.R.Error);UFunction* InitialResolved=nullptr;TStrongObjectPtr<UFunction> InitialResolvedHold(nullptr);if(!W.Read(TEXT("ApplyNode_GetTargetFunction_initial"),[&]{auto* Value=Apply->GetTargetFunction();if(Live(Value))InitialResolvedHold.Reset(Value);return Value;},InitialResolved)||!Helper->VerifyReference(Apply,InitialResolved,true))return W.Error(W.R.Error);
 auto Getter=[&](const FString& Variable,int32 Y){return NewNode<UK2Node_VariableGet>(W,EX+40,EY+Y,[&](UK2Node_VariableGet* N){N->VariableReference.SetSelfMember(FName(*Variable));});};
 auto* ComponentGet=Getter(Component,160);if(!ComponentGet)return W.Error(W.R.Error);auto* TriggerGet=Getter(Trigger,300);if(!TriggerGet)return W.Error(W.R.Error);auto* RegenerateGet=Getter(Regenerate,430);if(!RegenerateGet)return W.Error(W.R.Error);auto* DropGet=Drop.IsEmpty()?nullptr:Getter(Drop,560);if(!Drop.IsEmpty()&&!DropGet)return W.Error(W.R.Error);
 auto* ApplyIn=FindPin(W,Apply,EGPD_Input,{},true);auto* ApplyOut=FindPin(W,Apply,EGPD_Output,{},true);auto* ComponentOut=FindPin(W,ComponentGet,EGPD_Output,{},false,true);auto* TriggerOut=FindPin(W,TriggerGet,EGPD_Output,{},false,true);auto* RegenerateOut=FindPin(W,RegenerateGet,EGPD_Output,{},false,true);auto* ComponentIn=FindPin(W,Apply,EGPD_Input,{TEXT("PCGComponent")});auto* TriggerIn=FindPin(W,Apply,EGPD_Input,{TEXT("GenerationTrigger")});auto* RegenerateIn=FindPin(W,Apply,EGPD_Input,{TEXT("bRegenerateInEditor"),TEXT("RegenerateInEditor")});auto* DropIn=FindPin(W,Apply,EGPD_Input,{TEXT("bGenerateOnDropWhenTriggerOnDemand"),TEXT("GenerateOnDropWhenTriggerOnDemand")});
 if(!Link(W,ComponentOut,ComponentIn)||!Link(W,TriggerOut,TriggerIn)||!Link(W,RegenerateOut,RegenerateIn))return W.Error(W.R.Error);
 if(DropGet){auto* DropOut=FindPin(W,DropGet,EGPD_Output,{},false,true);if(!Link(W,DropOut,DropIn))return W.Error(W.R.Error);O->SetBoolField(TEXT("generate_on_drop_connected"),true);}else if(DropIn){const FString Value=DropDefault?TEXT("true"):TEXT("false");if(!GraphStep(W,TEXT("Schema_TrySetDefaultValue"),[&]{W.Schema->TrySetDefaultValue(*DropIn,Value);},nullptr,nullptr,nullptr,nullptr,DropIn,Value))return W.Error(W.R.Error);}
 if(!ApplyIn||!ApplyOut)return W.Error(TEXT("ApplyPCGGenerationSettings node is missing exec pins"));
 int32 Removed=0;for(auto* Old:OldCalls){auto* In=FindPin(W,Old,EGPD_Input,{},true);auto* Out=FindPin(W,Old,EGPD_Output,{},true);const auto* InState=In?PinSlot(W.GraphState,In):nullptr;const auto* OutState=Out?PinSlot(W.GraphState,Out):nullptr;const TArray<UEdGraphPin*> Sources=InState?InState->Links:TArray<UEdGraphPin*>(),Targets=OutState?OutState->Links:TArray<UEdGraphPin*>();const auto* Node=W.GraphState.Slots.FindByPredicate([&](const FNodeValue& V){return V.Node==Old;});const auto Pins=Node->Pins;for(auto* Pin:Pins)if(!Break(W,Pin))return W.Error(W.R.Error);for(auto* Source:Sources)for(auto* Target:Targets)if(!Link(W,Source,Target))return W.Error(W.R.Error);if(!RemoveNode(W,Old))return W.Error(W.R.Error);O->SetNumberField(TEXT("removed_existing_node_count"),++Removed);}
 const auto* Previous=PinSlot(W.GraphState,EntryThen);if(!Previous)return W.Error(TEXT("Entry exec pin membership lost"));const auto Targets=Previous->Links;if(!Break(W,EntryThen)||!Link(W,EntryThen,ApplyIn))return W.Error(W.R.Error);int32 Reconnected=0;for(auto* Target:Targets){if(!Link(W,ApplyOut,Target))return W.Error(W.R.Error);O->SetNumberField(TEXT("reconnected_exec_target_count"),++Reconnected);}
 if(Sync&&TemplateScope&&!TemplateScope->Apply(O))return W.Error(W.R.Error);
 if(!W.Write(TEXT("Graph_NotifyGraphChanged"),[&]{W.Graph->NotifyGraphChanged();})||!W.Write(TEXT("MarkBlueprintAsModified"),[&]{FBlueprintEditorUtils::MarkBlueprintAsModified(B);})||!W.Write(TEXT("Blueprint_MarkPackageDirty"),[&]{B->MarkPackageDirty();}))return W.Error(W.R.Error);
 if(Compiler&&!Compiler->Execute(Apply,Helper,TemplateScope,O))return W.Error(W.R.Error);
 if(!VerifyAuthoredCallDependencies(W,Apply))return W.Error(W.R.Error);if(Save&&!SaveExact(W))return W.Error(W.R.Error);O->SetBoolField(TEXT("saved"),W.R.bSaved);if(!W.Admit(TEXT("binding_final_readback")))return W.Error(W.R.Error);UFunction* ActualFunction=nullptr;TStrongObjectPtr<UFunction> FinalResolvedHold(nullptr);if(!W.Read(TEXT("ApplyNode_GetTargetFunction_readback"),[&]{auto* Value=Apply->GetTargetFunction();if(Live(Value))FinalResolvedHold.Reset(Value);return Value;},ActualFunction)||!Helper->VerifyReference(Apply,ActualFunction))return W.Error(W.R.Error);
 if(TemplateScope&&TemplateScope->GraphSource&&!TemplateScope->GraphSource->File(false))return W.Error(W.R.Error);O->SetObjectField(TEXT("apply_node"),NodeJson(W,Apply));if(!W.Admit(TEXT("after_apply_node_result")))return W.Error(W.R.Error);O->SetObjectField(TEXT("graph"),GraphJson(W));if(!W.Admit(TEXT("after_graph_result")))return W.Error(W.R.Error);W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);W.R.Data->SetBoolField(TEXT("package_dirty_before"),W.DirtyBefore);W.R.Data->SetBoolField(TEXT("package_dirty_after"),B->GetPackage()->IsDirty());return O;
}
FString Finish(FWork& W,TSharedPtr<FJsonObject> Legacy,const TCHAR* Operation)
{
 bool Typed=false;W.R.Data->TryGetBoolField(TEXT("typed_target_postcondition_passed"),Typed);bool Complete=Typed&&W.R.Error.IsEmpty()&&W.Admit(TEXT("final_exact_scoped_admission"));if(Complete&&W.FinalRefreshObservation)Complete=W.FinalRefreshObservation();W.R.bSuccess=Complete;W.R.bPostconditionPassed=Complete;if(Complete)W.R.bChangeStateKnown=true;else if(W.R.bMutationAttempted)W.R.bChangeStateKnown=false;if(!Complete&&W.R.Error.IsEmpty())W.Fail(TEXT("pcg_component_postcondition_incomplete"),TEXT("Requested supported operation did not complete typed postconditions"));
 if(!Legacy)Legacy=MakeShared<FJsonObject>();if(W.R.bSaved)Legacy->SetBoolField(TEXT("saved"),true);W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);W.R.Data->SetObjectField(TEXT("native_call_counts"),W.Counts);W.R.Data->SetArrayField(TEXT("actual_call_prefix"),W.Calls);W.R.Data->SetObjectField(TEXT("actual_mutation_prefix"),W.Mutation);W.R.Data->SetObjectField(TEXT("actual_compile_prefix"),W.Compile);W.R.Data->SetObjectField(TEXT("actual_save_prefix"),W.Save);W.R.Data->SetArrayField(TEXT("created_nodes"),W.Created);auto Partial=MakeShared<FJsonObject>();Partial->SetBoolField(TEXT("mutation_attempted"),W.R.bMutationAttempted);Partial->SetBoolField(TEXT("change_state_known"),W.R.bChangeStateKnown);Partial->SetBoolField(TEXT("saved"),W.R.bSaved);Partial->SetBoolField(TEXT("post_failure_live_observation_skipped"),!W.R.Error.IsEmpty());Partial->SetBoolField(TEXT("automatic_cleanup_completed"),false);Partial->SetStringField(TEXT("recovery_scope"),TEXT("No force deletion, global Undo reset, automatic retry, semantic restore or atomic rollback. Created graph nodes/PCG fanout may remain after failure; use preserved exact paths/actual prefix."));W.R.Data->SetObjectField(TEXT("actual_partial_state"),Partial);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("supported_variant_complete"),Complete);W.R.Data->SetStringField(TEXT("dependency_layer"),TEXT("Direct installed Editor/PCG public exported C++ SDK. No Toolset/RPC, transport, Python executor, engine modification or Cubeless game runtime class."));W.R.Data->SetStringField(TEXT("remaining_scope"),TEXT("Finite owned Editor CPU CreatePoints and a bounded resource-free native TransformPoints/BoundsModifier/Union DAG use request-local guards; global ConstructionHealthVerified remains false. Scoped immediate cleanup reuses the captured CPU source proof after completed session generation: original CreatePoints resource0 or the bounded WeightedSpawner native ManagedISM/ISM/HISM hard-removal slice; selected output and managed component retirement is separate from retained source and nonselected owners. A separate same-source remove=true, requested force=true/false immediate cleanup-then-generation slice preserves actual stage prefixes; neither enqueue nor cleanup proves generation completion. General resource/nonimmediate cleanup, notify/refresh and other regeneration remain guarded; a finite exact native inactive own-SCS empty or preserved supported-scalar saved-native CreatePoints/point-pipeline/finite Weighted-source template sync uses selected-role/current proof around original Modify/assign/SetAutoActivate/null-property PostEditChange; arbitrary/inherited/other-source or nonscalar parameterized template sync and broader compiler propagation remain pending. The direct Actor/native own-PCG/no-live-instance compile slice calls the public compiler once and reacquires logical authored roles without changing the global construction gate. Native wait is original single_frame_readback, not a loop or async completion; Named polling/fallback is Python-owned. No cook/packaged/runtime dependency independence proof."));W.R.Stage=Complete?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString Run(const FString& Text,const TCHAR* Operation,FHandler Handler){FWork W;TSharedPtr<FJsonObject> P;if(!Parse(Text,P)){W.Fail(TEXT("pcg_component_invalid_params_json"),TEXT("Finite bounded JSON object, exact unique keys, depth64/262144 tokens and no embedded NUL required"));return Finish(W,W.Error(W.R.Error),Operation);}W.R.Data->SetObjectField(TEXT("requested_params"),P);if(!IsInGameThread()){W.Fail(TEXT("pcg_component_game_thread_required"),TEXT("Editor game thread required"));return Finish(W,W.Error(W.R.Error),Operation);}if(Running){W.Fail(TEXT("pcg_component_reentrant_request"),TEXT("An HD request is active; reentrant request refused without dispatch"));return Finish(W,W.Error(W.R.Error),Operation);}TGuardValue<bool> Lease(Running,true);if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);auto Legacy=Handler(W,P);return Finish(W,Legacy,Operation);}
}
void ShutdownRefreshTracking(){StopRefreshTracking();}
FString RefreshComponents(const FString& ParamsJson){return Run(ParamsJson,TEXT("refresh_pcg_components"),&HandleRefresh);}
FString BindGenerationSettings(const FString& ParamsJson){return Run(ParamsJson,TEXT("bind_pcg_generation_settings_to_blueprint"),&HandleBind);}
bool ReadScalarUserParameters(const UPCGGraph* Graph,const UPCGGraphInstance* Instance,TSharedPtr<FJsonObject>& Out,FString& Error)
{return CopyScalarUserParameters(Graph,Instance,Out,Error);}
bool SameScalarUserParameters(const TSharedPtr<FJsonObject>& Expected,const TSharedPtr<FJsonObject>& Actual,FString& Error)
{
 if(!Expected||!Actual){Error=TEXT("missing logical parameter observation");return false;}FString Path,Want,Got;
 if(ScopedDifference(MakeShared<FJsonValueObject>(Expected),MakeShared<FJsonValueObject>(Actual),Path,Want,Got,true)){Error=Path+TEXT(" expected=")+Want+TEXT(" actual=")+Got;return false;}return true;
}
}
