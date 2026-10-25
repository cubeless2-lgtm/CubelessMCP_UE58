// ADL source-only candidate: pcg_tools.py3153/3226 -> add_pcg_node/set_pcg_node_setting
// -> PCGCommands.cpp1524/1532 -> HandleAddPCGNode1859/HandleSetPCGNodeSetting1996.
// Generic original property writer768-931; existing AddNodeOfType/remove-on-failure,
// Modify/raw setter/Structural notification, partial/unknown reporting and compile/save0 unchanged.
// SDK UnionElement40/43, InnerIntersection37/41, OuterIntersection53/58/62; all7 enum/bool fields runtime-editable.
// Public native FName inputs retained. Numeric enum/bool coercion limits remain the shared finite profile.
// Scoped dynamic declarations preserved; dynamic-pin writer, nested child spatial profiles,
// graph-spec spatial creation, generation and actual use/save/reload remain unverified/incomplete.
// GY dedicated PCG graph basic authoring. Editor-only source candidate; production construction GC gate remains closed.
#include "Compatibility/UE58/CubelessPCGGraphAuthoringApi.h"
#include "Compatibility/UE58/CubelessPCGApi.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/Factory.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectIterator.h"
#include "Misc/SecureHash.h"
#include "UObject/SavePackage.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Operations/CubelessInspectionOperations.h"
#include "Misc/ScopeExit.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "PCGSettings.h"
#include "Elements/PCGProjectionElement.h"
#include "Elements/PCGCopyAttributes.h"
#include "Elements/Metadata/PCGMetadataRenameElement.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "PCGSubgraph.h"
#include "PCGPoint.h"
#include "PCGInputOutputSettings.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGSurfaceSampler.h"
#include "Elements/PCGDataFromActor.h"
#include "Elements/PCGTypedGetter.h"
#include "Elements/PCGDifferenceElement.h"
#include "Compatibility/UE58/CubelessPCGContractAuthoringApi.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "MeshSelectors/PCGMeshSelectorByAttribute.h"
#include "Data/Registry/PCGDataTypeIdentifier.h"
#include "Data/Registry/PCGDataType.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyAccessUtil.h"
#include "UObject/UObjectHash.h"
#include "Misc/PackageName.h"
#include "Misc/Crc.h"
#include "Misc/StringOutputDevice.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "ISourceControlModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
namespace Cubeless::UE58::PCGGraphAuthoring
{
namespace
{
using Variable::FResult;
bool Running=false;
FString FreshSha256Bytes(const void*,uint32);
// This source does not fix the preserved AI3/AJ2/AW2 GC health failure. No public override.
constexpr bool ConstructionHealthVerified=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(){return IsInGameThread()&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage&&Live(GEditor);}
bool Same(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}
bool Forbidden(const FString& S){auto L=S.ToLower();return L.Contains(TEXT("unrealmcp"))||L.Contains(TEXT("cubelessmcp"))||L.Contains(TEXT("toolsetregistry"))||L.Contains(TEXT("modelcontextprotocol"));}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
struct FObjectIdentity{TStrongObjectPtr<const UObject> Hold{nullptr};TWeakObjectPtr<const UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path;bool Retired=false;explicit FObjectIdentity(const UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()){}bool Exact()const{auto* O=Object.Get();return Retired||(Live(O)&&Live(Class.Get())&&Live(Package.Get())&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&Same(O->GetPathName(),Path));}};
struct FNodeSlot{UPCGNode* Node=nullptr;UPCGSettingsInterface* Interface=nullptr;UPCGSettings* Settings=nullptr;UPCGSettings* InstanceTarget=nullptr;TArray<UPCGPin*> Input,Output;FName Title;FString Name,Path,SettingsPath,SettingsClass;int32 X=0,Y=0;};
struct FPinSlot{UPCGPin* Pin=nullptr;UPCGNode* Node=nullptr;FPCGPinProperties Properties{NAME_None};bool Output=false;TArray<UPCGEdge*> Edges;};
struct FEdgeSlot{UPCGEdge* Edge=nullptr;UPCGPin* Input=nullptr;UPCGPin* Output=nullptr;UPCGNode* From=nullptr;UPCGNode* To=nullptr;FName FromLabel,ToLabel;FString Key;};
struct FGraphSnapshot{UPCGGraph* Graph=nullptr;UPCGNode* Input=nullptr;UPCGNode* Output=nullptr;TArray<UPCGNode*> Nodes;TArray<FNodeSlot> NodeSlots;TArray<FPinSlot> Pins;TArray<FEdgeSlot> Edges;};
FString EdgeKey(const UPCGNode* A,FName AL,const UPCGNode* B,FName BL){return FString::Printf(TEXT("%p|%s|%p|%s"),A,*AL.ToString(),B,*BL.ToString());}
struct FStandaloneRequest;
struct FWork
{
 TSharedPtr<FStandaloneRequest> Standalone;int32 StandaloneDirtyCall=INDEX_NONE;bool StandalonePackageCleanBeforeCall=false;
 FResult R;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls,Transitions,PropertyRows;TArray<TUniquePtr<FObjectIdentity>> Objects;FGraphSnapshot Snapshot;bool Capturing=false,AllowFailureObservation=false,WorldCaptured=false,WorldWasNull=false;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UEditorEngine> Editor;UPCGGraph* Graph=nullptr;UPCGNode* CreatedNode=nullptr;FString FailedStage;int64 Slots=0,Text=0;bool DirtyBefore=false;
 FWork(){R.Compiled=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("construction_available"),ConstructionHealthVerified);R.Data->SetBoolField(TEXT("construction_GC_health_resolved"),false);}
 TFunction<bool()> SubgraphFinalCheck;TFunction<bool()> FreshCurrent,FreshSnapshot;TFunction<bool(UPCGGraph*)> FreshCapture;TFunction<void()> FreshCaptureDone;TFunction<TArray<UPCGPin*>(UPCGNode*,bool)> FreshPins;TFunction<TArray<UPCGEdge*>(UPCGPin*)> FreshEdges;TFunction<UPCGPin*(UPCGEdge*,bool)> FreshEdgePin;TSet<UPCGGraph*> FreshGraphs;
 bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=Message;FailedStage=R.Stage;}R.bSuccess=false;return false;}
 TSharedPtr<FJsonObject> Error(const FString& M){if(R.Error.IsEmpty())Fail(TEXT("pcg_graph_authoring_failed"),M);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),M);return O;}
 bool CaptureBudget(int64 N,int64 C=0){if(N<0||C<0||N>262144-Slots||C>32ll*1024*1024-Text)return Fail(TEXT("pcg_graph_authoring_budget"),TEXT("Request capture/JSON text budget exceeded; actual prefix retained"));Slots+=N;Text+=C;return true;}
 bool OutputBudget(int64 C=0){return CaptureBudget(1,C);}
 bool Admit(const FString& Stage)
 {R.Stage=Stage;if(!R.Error.IsEmpty()&&!AllowFailureObservation)return false;if(!Safe()||(WorldCaptured&&Editor.Get()!=GEditor))return Fail(TEXT("pcg_graph_context_unavailable"),TEXT("Unsafe Editor/GC/exit/save context; no further live observation"));auto* Now=GEditor->GetEditorWorldContext().World();if(FreshCurrent&&(!Safe()||(WorldCaptured&&Editor.Get()!=GEditor)||Now!=GWorld||!FreshCurrent()))return Fail(TEXT("pcg_fresh_current_changed"),TEXT("Fresh request source/owner current role changed"));if(FreshCurrent&&WorldCaptured&&World.Get()!=Now)return Fail(TEXT("pcg_graph_world_changed"),TEXT("Exact current world role changed before object access"));if(Now&&(!Live(Now)||Now->bIsTearingDown))return Fail(TEXT("pcg_graph_world_unavailable"),TEXT("Editor world unavailable/tearing down"));if(!WorldCaptured){World=Now;WorldWasNull=Now==nullptr;Editor=GEditor;WorldCaptured=true;}else if((!WorldWasNull&&!Live(World.Get()))||World.Get()!=Now)return Fail(TEXT("pcg_graph_world_changed"),TEXT("Exact Editor world changed; no replay"));if(GEditor->PlayWorld||GEditor->GetPIEWorldContext())return Fail(TEXT("pcg_graph_PIE_unsupported"),TEXT("Stop PIE/SIE before PCG graph authoring"));for(const auto& O:Objects)if(!O->Exact())return Fail(TEXT("pcg_graph_identity_changed"),TEXT("Captured class/outer/outermost/effective package/path changed"));if(Capturing||!Snapshot.Graph)return true;if(FreshCurrent)return FreshSnapshot&&FreshSnapshot();auto* G=Snapshot.Graph;if(G!=Graph||G->GetInputNode()!=Snapshot.Input||G->GetOutputNode()!=Snapshot.Output||G->GetNodes().Num()!=Snapshot.Nodes.Num())return Fail(TEXT("pcg_graph_topology_changed"),TEXT("Graph node membership/count changed outside an owned transition"));for(int32 I=0;I<Snapshot.Nodes.Num();++I)if(G->GetNodes()[I]!=Snapshot.Nodes[I])return Fail(TEXT("pcg_graph_topology_changed"),TEXT("Graph physical node order changed"));for(const auto& N:Snapshot.NodeSlots){auto* O=N.Node;if(O->GetOuter()!=G||O->GetGraph()!=G||O->GetSettingsInterface()!=N.Interface||O->GetAuthoredTitleName()!=N.Title)return Fail(TEXT("pcg_graph_node_role_changed"),TEXT("Node graph/settings/title role changed"));int32 X=0,Y=0;O->GetNodePosition(X,Y);if(X!=N.X||Y!=N.Y)return Fail(TEXT("pcg_graph_position_changed"),TEXT("Node position changed outside owned transition"));auto Check=[](const auto& A,const auto& B){if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(A[I]!=B[I])return false;return true;};if(!Check(O->GetInputPins(),N.Input)||!Check(O->GetOutputPins(),N.Output))return Fail(TEXT("pcg_graph_pin_membership_changed"),TEXT("Physical pin membership/order changed"));if(auto* Instance=Cast<UPCGSettingsInstance>(N.Interface))if(Instance->Settings!=N.InstanceTarget)return Fail(TEXT("pcg_graph_shared_settings_changed"),TEXT("Settings instance target changed"));}for(const auto& P:Snapshot.Pins){if(P.Pin->Node!=P.Node||!FPCGPinProperties::StaticStruct()->CompareScriptStruct(&P.Pin->Properties,&P.Properties,0)||P.Pin->Edges.Num()!=P.Edges.Num())return Fail(TEXT("pcg_graph_pin_state_changed"),TEXT("Pin properties/owner/edges changed"));for(int32 I=0;I<P.Edges.Num();++I)if(P.Pin->Edges[I]!=P.Edges[I])return Fail(TEXT("pcg_graph_edge_order_changed"),TEXT("Pin edge order changed"));}for(const auto& E:Snapshot.Edges)if(E.Edge->InputPin!=E.Input||E.Edge->OutputPin!=E.Output)return Fail(TEXT("pcg_graph_edge_endpoints_changed"),TEXT("Captured edge endpoints changed"));return true;}
 bool Pin(const UObject* O){if(!Admit(TEXT("pin_object"))||!Live(O))return Fail(TEXT("pcg_graph_object_unavailable"),TEXT("Expected live loaded object"));for(const auto& I:Objects)if(I->Object.Get()==O&&!I->Retired)return I->Exact();if(!CaptureBudget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FObjectIdentity>(O));return true;}
 bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage)||!CaptureBudget(1))return false;StandaloneDirtyCall=INDEX_NONE;StandalonePackageCleanBeforeCall=false;if(Standalone&&Mutation&&Graph&&(Same(Stage,TEXT("Graph_AddNodeOfType"))||Same(Stage,TEXT("Graph_AddEdge"))||Same(Stage,TEXT("Settings_Modify"))||Same(Stage,TEXT("Graph_MarkPackageDirty")))){StandaloneDirtyCall=Calls.Num();StandalonePackageCleanBeforeCall=!Graph->GetPackage()->IsDirty();}R.bMutationAttempted|=Mutation;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetBoolField(TEXT("entered"),true);Row->SetBoolField(TEXT("returned"),false);Row->SetBoolField(TEXT("mutation_attempted"),Mutation);Calls.Add(MakeShared<FJsonValueObject>(Row));return true;}
 bool After(const TCHAR* Stage){if(!Calls.IsEmpty())Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
 template<typename F,typename T>bool Read(const TCHAR* Stage,F&& Fn,T& Out){if(!SDK(Stage))return false;Out=Fn();return After(Stage);}
 template<typename F>bool Write(const TCHAR* Stage,F&& Fn){if(!SDK(Stage,true))return false;Fn();return After(Stage);}
 bool StandaloneConstruction(const TCHAR* Operation);
 bool RequireConstruction(const TCHAR* Operation){if(Standalone)return StandaloneConstruction(Operation);if(FreshCurrent&&FreshGraphs.Contains(Graph)&&FreshCurrent())return true;if(ConstructionHealthVerified)return true;R.Data->SetStringField(TEXT("construction_blocked_operation"),Operation);return Fail(TEXT("pcg_construction_blocked_python_gc"),TEXT("Preserved AI3/AJ2/AW2 construction/automatic Python GC failure unresolved. C++ source is not a GC fix; no construction, fallback or replay dispatched."));}
 bool Node(UPCGNode* N){if(!N||!Admit(TEXT("node_member")))return false;return Snapshot.NodeSlots.ContainsByPredicate([&](const FNodeSlot& S){return S.Node==N;})||Fail(TEXT("pcg_graph_foreign_node"),TEXT("Node is not an exact current graph member"));}
 bool PinNode(const UPCGPin* P){return P&&Admit(TEXT("pin_member"))&&(Snapshot.Pins.ContainsByPredicate([&](const FPinSlot& S){return S.Pin==P;})||Fail(TEXT("pcg_graph_foreign_pin"),TEXT("Pin is not captured in exact physical graph membership")));}
 void RetireMutablePins(UPCGNode* N){for(auto& I:Objects){auto* O=I->Object.Get();if(!O)continue;const bool PinOf=Snapshot.Pins.ContainsByPredicate([&](const FPinSlot& P){return P.Pin==O&&P.Node==N;});const bool EdgeOf=Snapshot.Edges.ContainsByPredicate([&](const FEdgeSlot& E){return E.Edge==O&&(E.From==N||E.To==N);});if((PinOf||EdgeOf)&&O->GetPackage()==Graph->GetPackage())I->Retired=true;}}
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("read_continuation"));}

bool NoEmbeddedNul(const FString& S){for(int32 I=0;I<S.Len();++I)if(S[I]==0)return false;return true;}
struct FExactKeys:DefaultKeyFuncs<FString>
{static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}};
bool Finite(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Count)
{if(!V.IsValid()||Depth>64||++Count>262144){return false;}if(V->Type==EJson::Number){double N=0;return V->TryGetNumber(N)&&FMath::IsFinite(N);}if(V->Type==EJson::String){return NoEmbeddedNul(V->AsString());}if(V->Type==EJson::Array){for(const auto& X:V->AsArray()){if(!Finite(X,Depth+1,Count)){return false;}}}else if(V->Type==EJson::Object){for(const auto& X:V->AsObject()->Values){if(!NoEmbeddedNul(FString(X.Key))||!Finite(X.Value,Depth+1,Count)){return false;}}}return V->Type!=EJson::None;}
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Out,int32 MaxLength=1048576)
{
    if(Text.IsEmpty()||Text.Len()>MaxLength||!NoEmbeddedNul(Text)){return false;}
    int32 Depth=0;bool InString=false,Escape=false;for(int32 I=0;I<Text.Len();++I){const TCHAR C=Text[I];if(InString){if(Escape){Escape=false;}else if(C=='\\'){Escape=true;}else if(C=='"'){InString=false;}}else if(C=='"'){InString=true;}else if(C=='{'||C=='['){if(++Depth>64){return false;}}else if(C=='}'||C==']'){if(--Depth<0){return false;}}}if(InString||Depth!=0){return false;}
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));TArray<TSet<FString,FExactKeys>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Tokens=0;
    while(Reader->ReadNext(Token)){if(Token==EJsonNotation::Error||++Tokens>262144){return false;}if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()){const FString K(Reader->GetIdentifier());if(!NoEmbeddedNul(K)||Keys.Last().Contains(K)){return false;}Keys.Last().Add(K);}if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart){Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();}else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd){if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}}
    TArray<TSharedPtr<FJsonValue>> A;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),A)||A.Num()!=1||!A[0].IsValid()||A[0]->Type!=EJson::Object){return false;}int32 Count=0;if(!Finite(A[0],0,Count)){return false;}Out=A[0]->AsObject();return Out.IsValid();
}





bool QuickNodes(UPCGGraph* G,const FGraphSnapshot& S){if(G->GetInputNode()!=S.Input||G->GetOutputNode()!=S.Output||G->GetNodes().Num()!=S.Nodes.Num())return false;for(int32 I=0;I<S.Nodes.Num();++I)if(G->GetNodes()[I]!=S.Nodes[I])return false;return true;}
bool CaptureGraph(FWork& W,UPCGGraph* G,FGraphSnapshot& Out)
{
 TGuardValue<bool> CaptureScope(W.Capturing,true);ON_SCOPE_EXIT{if(W.FreshCaptureDone)W.FreshCaptureDone();};if(W.FreshCapture&&!W.FreshCapture(G))return false;if(!W.Pin(G))return false;Out={};Out.Graph=G;Out.Input=G->GetInputNode();Out.Output=G->GetOutputNode();for(UPCGNode* N:G->GetNodes())Out.Nodes.Add(N);if(!W.CaptureBudget(Out.Nodes.Num()+2))return false;TArray<UPCGNode*> Ordered{Out.Input,Out.Output};Ordered.Append(Out.Nodes);TSet<UPCGNode*> Seen;for(auto* N:Out.Nodes)if(!N)return W.Fail(TEXT("pcg_graph_null_node"),TEXT("Null ordinary node is malformed"));for(auto* N:Ordered)if(N&&(!W.Pin(N)||N->GetOuter()!=G||N->GetGraph()!=G||N->GetPackage()!=G->GetPackage()))return W.Fail(TEXT("pcg_graph_foreign_node"),TEXT("Capture pins every exact node before any virtual settings callback"));
 for(auto* N:Ordered){if(!N){if(N==Out.Input||N==Out.Output)continue;return W.Fail(TEXT("pcg_graph_null_node"),TEXT("Null ordinary node is malformed"));}if(Seen.Contains(N))return W.Fail(TEXT("pcg_graph_duplicate_node"),TEXT("Duplicate physical Node identity"));Seen.Add(N);if(!W.Pin(N)||N->GetOuter()!=G||N->GetGraph()!=G||N->GetPackage()!=G->GetPackage())return W.Fail(TEXT("pcg_graph_foreign_node"),TEXT("Node is not owned by exact graph/package"));FNodeSlot S;S.Node=N;S.Interface=N->GetSettingsInterface();if(!S.Interface||!W.Pin(S.Interface))return W.Fail(TEXT("pcg_graph_settings_missing"),TEXT("Node settings interface unavailable"));if(auto* I=Cast<UPCGSettingsInstance>(S.Interface)){S.InstanceTarget=I->Settings;if(S.InstanceTarget&&!W.Pin(S.InstanceTarget))return false;}if(!QuickNodes(G,Out))return W.Fail(TEXT("pcg_graph_topology_during_capture"),TEXT("Graph changed while capturing node interfaces"));UPCGSettings* Settings=nullptr;if(!W.Read(TEXT("Node_GetSettings"),[&]{return N->GetSettings();},Settings))return false;if(!QuickNodes(G,Out)||N->GetSettingsInterface()!=S.Interface)return W.Fail(TEXT("pcg_graph_settings_callback_changed_owner"),TEXT("Virtual settings getter changed graph/interface"));S.Settings=Settings;if(!Settings||!W.Pin(Settings))return W.Fail(TEXT("pcg_graph_settings_unavailable"),TEXT("Virtual settings getter returned no live settings"));if(auto* I=Cast<UPCGSettingsInstance>(S.Interface))if(I->Settings!=S.InstanceTarget||Settings!=S.InstanceTarget)return W.Fail(TEXT("pcg_graph_settings_instance_changed"),TEXT("Shared settings instance changed during getter"));S.Title=N->GetAuthoredTitleName();N->GetNodePosition(S.X,S.Y);S.Name=N->GetName();S.Path=N->GetPathName();S.SettingsPath=Settings->GetPathName();S.SettingsClass=Settings->GetClass()->GetPathName();if(W.FreshPins){S.Input=W.FreshPins(N,false);S.Output=W.FreshPins(N,true);if(!W.R.Error.IsEmpty())return false;}else{for(UPCGPin* P:N->GetInputPins())S.Input.Add(P);for(UPCGPin* P:N->GetOutputPins())S.Output.Add(P);}for(auto* P:S.Input)if(!P||!W.Pin(P)||P->Node!=N||P->GetOuter()!=N)return W.Fail(TEXT("pcg_graph_pin_owner_invalid"),TEXT("Capture pins input identity before next virtual getter"));for(auto* P:S.Output)if(!P||!W.Pin(P)||P->Node!=N||P->GetOuter()!=N)return W.Fail(TEXT("pcg_graph_pin_owner_invalid"),TEXT("Capture pins output identity before next virtual getter"));if(!W.CaptureBudget(S.Input.Num()+S.Output.Num(),S.Path.Len()+S.SettingsPath.Len()))return false;Out.NodeSlots.Add(MoveTemp(S));}
 TSet<UPCGPin*> Known;for(const auto& N:Out.NodeSlots){for(int32 Direction=0;Direction<2;++Direction){const auto& Pins=Direction?N.Output:N.Input;for(auto* P:Pins){if(!P||Known.Contains(P)||!W.Pin(P)||P->Node!=N.Node||P->GetOuter()!=N.Node||P->GetPackage()!=G->GetPackage())return W.Fail(TEXT("pcg_graph_pin_owner_invalid"),TEXT("Pin must be unique/non-null in exactly one physical owner array with same package"));Known.Add(P);FPinSlot S;S.Pin=P;S.Node=N.Node;S.Output=Direction!=0;S.Properties=P->Properties;if(W.FreshEdges){S.Edges=W.FreshEdges(P);if(!W.R.Error.IsEmpty())return false;}else{for(UPCGEdge* E:P->Edges)S.Edges.Add(E);}if(!W.CaptureBudget(S.Edges.Num(),S.Properties.Label.ToString().Len()))return false;Out.Pins.Add(MoveTemp(S));}}}
 TSet<UPCGEdge*> EdgeSeen;for(const auto& P:Out.Pins){for(auto* E:P.Edges){if(!E||!W.Pin(E))return W.Fail(TEXT("pcg_graph_null_edge"),TEXT("Null/retired physical edge unsupported"));auto* A=W.FreshEdgePin?W.FreshEdgePin(E,true):E->InputPin.Get();auto* B=W.FreshEdgePin?W.FreshEdgePin(E,false):E->OutputPin.Get();if(!Known.Contains(A)||!Known.Contains(B)||(A!=P.Pin&&B!=P.Pin)||!A->Edges.Contains(E)||!B->Edges.Contains(E)||A->Node==B->Node||E->GetPackage()!=G->GetPackage()||(E->GetOuter()!=A&&E->GetOuter()!=B))return W.Fail(TEXT("pcg_graph_edge_membership_invalid"),TEXT("Edge is foreign/non-reciprocal or has unresolved endpoints"));const auto* AS=Out.Pins.FindByPredicate([&](const FPinSlot& S){return S.Pin==A;});const auto* BS=Out.Pins.FindByPredicate([&](const FPinSlot& S){return S.Pin==B;});if(!AS||!BS||!AS->Output||BS->Output)return W.Fail(TEXT("pcg_graph_edge_direction_invalid"),TEXT("PCG Edge.InputPin must be upstream output and Edge.OutputPin downstream input"));if(EdgeSeen.Contains(E))continue;EdgeSeen.Add(E);FEdgeSlot S;S.Edge=E;S.Input=A;S.Output=B;S.From=AS->Node;S.To=BS->Node;S.FromLabel=AS->Properties.Label;S.ToLabel=BS->Properties.Label;S.Key=EdgeKey(S.From,S.FromLabel,S.To,S.ToLabel);if(Out.Edges.ContainsByPredicate([&](const FEdgeSlot& X){return X.Key==S.Key;}))return W.Fail(TEXT("pcg_graph_duplicate_edge"),TEXT("Duplicate physical endpoint edge"));Out.Edges.Add(MoveTemp(S));}}
 return W.Admit(TEXT("captured_current_graph"))&&QuickNodes(G,Out);
}
TSharedPtr<FJsonObject> SnapshotJson(const FGraphSnapshot& S){auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Nodes,Edges;for(const auto& N:S.NodeSlots){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("node_id"),N.Name);J->SetStringField(TEXT("object_path"),N.Path);J->SetStringField(TEXT("settings_path"),N.SettingsPath);J->SetStringField(TEXT("settings_class"),N.SettingsClass);J->SetStringField(TEXT("authored_title"),N.Title.ToString());J->SetNumberField(TEXT("x"),N.X);J->SetNumberField(TEXT("y"),N.Y);J->SetNumberField(TEXT("input_pin_count"),N.Input.Num());J->SetNumberField(TEXT("output_pin_count"),N.Output.Num());Nodes.Add(MakeShared<FJsonValueObject>(J));}for(const auto& E:S.Edges){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("key"),E.Key);J->SetStringField(TEXT("from_pin"),E.FromLabel.ToString());J->SetStringField(TEXT("to_pin"),E.ToLabel.ToString());Edges.Add(MakeShared<FJsonValueObject>(J));}TArray<TSharedPtr<FJsonValue>> Pins;for(const auto& P:S.Pins){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("pin_identity"),FString::Printf(TEXT("%p"),P.Pin));J->SetStringField(TEXT("owner_identity"),FString::Printf(TEXT("%p"),P.Node));J->SetStringField(TEXT("label"),P.Properties.Label.ToString());J->SetBoolField(TEXT("output"),P.Output);J->SetStringField(TEXT("allowed_types"),P.Properties.AllowedTypes.ToString());J->SetBoolField(TEXT("multiple_data"),P.Properties.bAllowMultipleData);J->SetNumberField(TEXT("physical_edge_count"),P.Edges.Num());Pins.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("nodes"),Nodes);O->SetArrayField(TEXT("pins"),Pins);O->SetArrayField(TEXT("edges"),Edges);return O;}
bool SameNodeRole(const FNodeSlot& A,const FNodeSlot& B,bool Pins=true,bool Metadata=true){if(A.Node!=B.Node||A.Interface!=B.Interface||A.Settings!=B.Settings||A.InstanceTarget!=B.InstanceTarget)return false;if(Metadata&&(A.Title!=B.Title||A.X!=B.X||A.Y!=B.Y))return false;return !Pins||(A.Input==B.Input&&A.Output==B.Output);}
bool Transition(FWork& W,const FGraphSnapshot& Before,const TCHAR* Kind,UPCGNode* Mutable=nullptr,UPCGNode* NewNode=nullptr,const FString& ExpectedEdge=FString(),bool ReplaceInput=false,UPCGPin* TargetPin=nullptr,const TSet<FString>* ExpectedOverride=nullptr)
{
 FGraphSnapshot After;if(!CaptureGraph(W,W.Graph,After))return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("kind"),Kind);J->SetObjectField(TEXT("before"),SnapshotJson(Before));J->SetObjectField(TEXT("after"),SnapshotJson(After));W.Transitions.Add(MakeShared<FJsonValueObject>(J));if(After.Input!=Before.Input||After.Output!=Before.Output)return W.Fail(TEXT("pcg_graph_factory_node_replaced"),TEXT("Input/Output identities changed"));TArray<UPCGNode*> ExpectedNodes=Before.Nodes;if(FString(Kind)==TEXT("add"))ExpectedNodes.Add(NewNode);if(FString(Kind)==TEXT("remove"))ExpectedNodes.Remove(Mutable);if(After.Nodes!=ExpectedNodes)return W.Fail(TEXT("pcg_graph_transition_nodes"),TEXT("SDK changed node membership outside exact expected transition"));for(const auto& N:Before.NodeSlots){if(FString(Kind)==TEXT("remove")&&N.Node==Mutable)continue;const auto* A=After.NodeSlots.FindByPredicate([&](const FNodeSlot& S){return S.Node==N.Node;});const bool MutablePins=FString(Kind)==TEXT("notify")&&N.Node==Mutable;if(!A||!SameNodeRole(N,*A,!MutablePins,FString(Kind)!=TEXT("metadata")||N.Node!=Mutable))return W.Fail(TEXT("pcg_graph_transition_node_role"),TEXT("SDK changed another node/settings/pin/metadata role"));}
 TSet<FString> Expected;for(const auto& E:Before.Edges){if(FString(Kind)==TEXT("remove")&&(E.From==Mutable||E.To==Mutable))continue;if(FString(Kind)==TEXT("connect")&&ReplaceInput&&E.Output==TargetPin&&E.Key!=ExpectedEdge)continue;Expected.Add(E.Key);}if(FString(Kind)==TEXT("connect"))Expected.Add(ExpectedEdge);
 if(ExpectedOverride)Expected=*ExpectedOverride;
 for(const auto& P:Before.Pins){if(FString(Kind)==TEXT("notify")&&P.Node==Mutable)continue;if(FString(Kind)==TEXT("remove")&&P.Node==Mutable)continue;const auto* A=After.Pins.FindByPredicate([&](const FPinSlot& S){return S.Pin==P.Pin;});if(!A||A->Node!=P.Node||A->Output!=P.Output||!FPCGPinProperties::StaticStruct()->CompareScriptStruct(&A->Properties,&P.Properties,0))return W.Fail(TEXT("pcg_graph_unrelated_pin_changed"),TEXT("SDK changed unrelated pin header"));}
 TSet<FString> Actual;for(const auto& E:After.Edges){Actual.Add(E.Key);if(!Expected.Contains(E.Key))return W.Fail(TEXT("pcg_graph_transition_unexpected_edge"),TEXT("SDK created an unexpected edge"));}if(FString(Kind)!=TEXT("notify")){if(Actual.Num()!=Expected.Num())return W.Fail(TEXT("pcg_graph_requested_edge_missing"),TEXT("Requested edge transition was not retained; AddEdge To return is not success"));for(const auto& E:Expected)if(!Actual.Contains(E))return W.Fail(TEXT("pcg_graph_edge_lost"),TEXT("Unrelated edge was lost"));}else{for(const auto& E:Before.Edges)if(E.From!=Mutable&&E.To!=Mutable&&!Actual.Contains(E.Key))return W.Fail(TEXT("pcg_graph_unrelated_edge_removed"),TEXT("Settings notification removed unrelated edge"));}
 for(const auto& E:Before.Edges){if(!Expected.Contains(E.Key)||!Actual.Contains(E.Key))continue;const auto* A=After.Edges.FindByPredicate([&](const FEdgeSlot& X){return X.Key==E.Key;});if(!A||A->Edge!=E.Edge)return W.Fail(TEXT("pcg_graph_retained_edge_replaced"),TEXT("SDK replaced an unrelated retained physical edge"));const bool Notify=FString(Kind)==TEXT("notify");if((!Notify||E.From!=Mutable)&&A->Input!=E.Input)return W.Fail(TEXT("pcg_graph_retained_edge_retargeted"),TEXT("Retained upstream endpoint changed"));if((!Notify||E.To!=Mutable)&&A->Output!=E.Output)return W.Fail(TEXT("pcg_graph_retained_edge_retargeted"),TEXT("Retained downstream endpoint changed"));}
 // Do not re-target old raw pins after callback. Current members are independently captured above.
 bool Changed=Before.Nodes!=After.Nodes||Before.Edges.Num()!=After.Edges.Num();for(const auto& N:Before.NodeSlots){const auto* A=After.NodeSlots.FindByPredicate([&](const FNodeSlot& X){return X.Node==N.Node;});Changed|=!A||(A&&(A->Title!=N.Title||A->X!=N.X||A->Y!=N.Y||A->Input!=N.Input||A->Output!=N.Output));}for(const auto& P:Before.Pins){const auto* A=After.Pins.FindByPredicate([&](const FPinSlot& X){return X.Pin==P.Pin;});Changed|=!A||(A&&!FPCGPinProperties::StaticStruct()->CompareScriptStruct(&A->Properties,&P.Properties,0));}W.R.bChanged|=Changed;J->SetBoolField(TEXT("observed_graph_projection_changed"),Changed);W.Snapshot=MoveTemp(After);return W.Admit(TEXT("owned_transition_verified"));
}


FString NormalizePCGObjectPathForLoad(const FString&);
UPCGGraph* LoadGraph(const FString& Path){UPCGGraph* G=nullptr;if(!Current->Read(TEXT("LoadObject_graph"),[&]{return LoadObject<UPCGGraph>(nullptr,*Path);},G))return nullptr;if(G&&(!Current->Pin(G)||Forbidden(G->GetClass()->GetPathName())))return nullptr;return G;}
FString NormalizePCGObjectPathForLoad(const FString& ObjectPath)
{
    FString NormalizedPath = FPackageName::ExportTextPathToObjectPath(ObjectPath).TrimStartAndEnd();
    NormalizedPath.TrimQuotesInline();

    if ((NormalizedPath.StartsWith(TEXT("/Game/")) || NormalizedPath.StartsWith(TEXT("/Engine/"))) && !NormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(NormalizedPath);
        NormalizedPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    return NormalizedPath;
}
TArray<FString> FindPCGGraphAssetPaths(const FString& GraphPathOrName)
{
    TArray<FString> CandidatePaths;
    const FString Query = NormalizePCGObjectPathForLoad(GraphPathOrName);

    if (Query.StartsWith(TEXT("/Game/")) || Query.StartsWith(TEXT("/Engine/")))
    {
        if (LoadGraph(Query))
        {
            if(!Current->CaptureBudget(1,Query.Len()))return CandidatePaths;
            CandidatePaths.Add(Query);
            return CandidatePaths;
        }
    }

    if(!Current->SDK(TEXT("registry_GetAssetsByClass")))return CandidatePaths;
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssetsByClass(UPCGGraph::StaticClass()->GetClassPathName(), Assets, true);

    Current->Prefix->SetNumberField(TEXT("registry_asset_count"),Assets.Num());
    if(!Current->After(TEXT("registry_GetAssetsByClass")))return CandidatePaths;
    if(Assets.Num()>1000000){Current->Fail(TEXT("pcg_inspection_registry_bound"),TEXT("Registry candidates exceed observation bound."));return CandidatePaths;}
    const FString ShortQuery = FPackageName::GetShortName(Query);
    for (const FAssetData& AssetData : Assets)
    {
        const FString AssetName = AssetData.AssetName.ToString();
        const FString PackageName = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();

        if (AssetName.Equals(GraphPathOrName, ESearchCase::IgnoreCase) ||
            AssetName.Equals(ShortQuery, ESearchCase::IgnoreCase) ||
            PackageName.Equals(GraphPathOrName, ESearchCase::IgnoreCase) ||
            ObjectPath.Equals(Query, ESearchCase::IgnoreCase))
        {
            if(!Current->CaptureBudget(1,ObjectPath.Len()))break;
            CandidatePaths.Add(ObjectPath);
            Current->Prefix->SetNumberField(TEXT("candidate_paths_observed"),CandidatePaths.Num());
            Current->Prefix->SetStringField(TEXT("last_candidate_path"),ObjectPath);
        }
    }

    return CandidatePaths;
}
UPCGGraph* FindPCGGraph(const FString& GraphPathOrName)
{
    const FString Query = NormalizePCGObjectPathForLoad(GraphPathOrName);
    if (UPCGGraph* Graph = LoadGraph(Query))
    {
        return Graph;
    }

    if(!ContinueRead())return nullptr;
    const TArray<FString> CandidatePaths = FindPCGGraphAssetPaths(GraphPathOrName);
    if (CandidatePaths.Num() == 1)
    {
        return LoadGraph(CandidatePaths[0]);
    }

    return nullptr;
}
bool EnrollStandalone(FWork& W);
bool PrepareStandaloneAdd(FWork& W,UClass* Class,const TSharedPtr<FJsonObject>& P);
bool KeepStandaloneReturns(FWork& W,UPCGNode* N,UPCGSettings* S);
bool CaptureStandaloneValues(FWork& W,UPCGSettings* S);
bool SetStandaloneSetting(FWork& W,UPCGNode* N,UPCGSettings* S,const FString& Name,const TSharedPtr<FJsonValue>& V,FString& Expected);
bool BindGraph(FWork& W,const FString& Path){if(W.FreshCurrent){return W.Graph&&(W.Standalone.IsValid()||W.FreshGraphs.Contains(W.Graph))&&Same(W.Graph->GetPathName(),Path)&&W.Admit(TEXT("fresh_bound_graph"));}auto* G=FindPCGGraph(Path);if(!G)return W.Fail(TEXT("pcg_graph_not_found"),TEXT("PCG graph not found: ")+Path);W.Graph=G;W.DirtyBefore=G->GetPackage()->IsDirty();FGraphSnapshot S;if(!CaptureGraph(W,G,S))return false;W.Snapshot=MoveTemp(S);W.R.Data->SetObjectField(TEXT("before"),SnapshotJson(W.Snapshot));W.R.Data->SetStringField(TEXT("resolved_graph"),G->GetPathName());return W.Admit(TEXT("bound_graph"))&&EnrollStandalone(W);}
UPCGNode* FindPCGNodeById(UPCGGraph* G,const FString& Id){if(!Current->Admit(TEXT("node_selector")))return nullptr;if(Current->FreshCurrent){for(const auto& Slot:Current->Snapshot.NodeSlots)if(Slot.Name.Equals(Id,ESearchCase::IgnoreCase)||Slot.Path.Equals(Id,ESearchCase::IgnoreCase)||Slot.Title.ToString().Equals(Id,ESearchCase::IgnoreCase))return Slot.Node;return nullptr;}TArray<UPCGNode*> Ordered{G->GetInputNode(),G->GetOutputNode()};for(UPCGNode* N:G->GetNodes())Ordered.Add(N);for(auto* N:Ordered){if(!N)continue;if(N->GetName().Equals(Id,ESearchCase::IgnoreCase)||N->GetPathName().Equals(Id,ESearchCase::IgnoreCase)||N->GetAuthoredTitleName().ToString().Equals(Id,ESearchCase::IgnoreCase))return N;}return nullptr;}
UClass* LoadClassGuarded(const FString& Path){UClass* C=nullptr;if(!Current->Read(TEXT("LoadObject_class"),[&]{return LoadObject<UClass>(nullptr,*Path);},C))return nullptr;if(C&&!Current->Pin(C))return nullptr;return C;}
UClass* FindClassGuarded(const FString& Name){UClass* C=nullptr;if(!Current->Read(TEXT("FindFirstObject_class"),[&]{return FindFirstObject<UClass>(*Name);},C))return nullptr;if(C&&!Current->Pin(C))return nullptr;return C;}
FString StripUClassPrefix(const FString& S){return S.StartsWith(TEXT("U"))?S.RightChop(1):S;}
UClass* FindSettingsClass(const FString& Name){if(Forbidden(Name))return nullptr;auto Accept=[](UClass* C){return C&&C->IsChildOf(UPCGSettings::StaticClass())&&!C->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)&&!Forbidden(C->GetPathName());};if(auto* C=LoadClassGuarded(NormalizePCGObjectPathForLoad(Name)))return Accept(C)?C:nullptr;if(!ContinueRead())return nullptr;TArray<FString> Candidates{Name,StripUClassPrefix(Name),TEXT("/Script/PCG.")+Name,TEXT("/Script/PCG.")+StripUClassPrefix(Name)};for(const auto& Candidate:Candidates){auto* C=FindClassGuarded(Candidate);if(!ContinueRead())return nullptr;if(Accept(C))return C;C=LoadClassGuarded(Candidate);if(!ContinueRead())return nullptr;if(Accept(C))return C;}return nullptr;}
bool WriterSettings(FWork& W,UPCGNode* N,UPCGSettings*& S){if(!W.Node(N))return false;const auto* Slot=W.Snapshot.NodeSlots.FindByPredicate([&](const FNodeSlot& X){return X.Node==N;});if(!Slot)return false;S=Slot->Settings;if(!S||N->GetSettingsInterface()!=S||S->GetOuter()!=N||S->GetPackage()!=W.Graph->GetPackage()||Forbidden(S->GetClass()->GetPathName()))return W.Fail(TEXT("pcg_shared_or_external_settings_write_unsupported"),TEXT("Writer requires exact local node-owned settings; shared/external settings source is preserved"));return W.Pin(S);}

TArray<UPCGNode*> GraphNodes(UPCGGraph* G,bool IncludeIO)
{TArray<UPCGNode*> Out;if(!G||!ContinueRead())return Out;if(IncludeIO){Out.Add(G->GetInputNode());Out.Add(G->GetOutputNode());}for(UPCGNode* N:G->GetNodes())Out.Add(N);return Out;}

TArray<const UPCGPin*> NodePins(UPCGNode* N,bool Output)
{TArray<const UPCGPin*> Out;if(!N||!ContinueRead())return Out;if(Output){for(const UPCGPin* P:N->GetOutputPins())Out.Add(P);}else{for(const UPCGPin* P:N->GetInputPins())Out.Add(P);}return Out;}

bool ExactPinOwner(const UPCGPin* P)
{
    if(!P)return true;if(!ContinueRead())return false;UPCGNode* N=P->Node.Get();
    if(!N||!Current->Node(N))return Current->Fail(TEXT("pcg_inspection_pin_owner_invalid"),TEXT("Nonnull pin has no live exact owning node."));
    const bool In=N->GetInputPins().Contains(P),Out=N->GetOutputPins().Contains(P);
    if(In==Out)return Current->Fail(TEXT("pcg_inspection_pin_membership_invalid"),TEXT("Pin must belong to exactly one physical input/output array of its owner."));
    return true;
}

EPCGDataType LegacyType(const FPCGDataTypeIdentifier& Identifier)
{uint64 Mask=0;for(const auto& Id:Identifier.GetIds())Mask|=static_cast<uint64>(Id.AsLegacyType());return static_cast<EPCGDataType>(Mask);}

TSharedPtr<FJsonObject> PCGPinToJson(const UPCGPin* P)
{
    auto O=MakeShared<FJsonObject>();if(!Current->OutputBudget()||!P)return O;
    if(!ExactPinOwner(P)||!Current->PinNode(P))return O;
    if(!Current->CaptureBudget(0,P->Properties.Label.ToString().Len()))return O;
    O->SetStringField(TEXT("label"),P->Properties.Label.ToString());
    O->SetStringField(TEXT("direction"),P->IsOutputPin()?TEXT("output"):TEXT("input"));
    const FPCGDataTypeIdentifier Allowed=P->Properties.AllowedTypes;const EPCGDataType Mask=LegacyType(Allowed);
    O->SetStringField(TEXT("allowed_types"),StaticEnum<EPCGDataType>()?StaticEnum<EPCGDataType>()->GetNameStringByValue(static_cast<int64>(Mask)):LexToString(static_cast<uint64>(Mask)));
    O->SetStringField(TEXT("allowed_types_identifier"),Allowed.ToString());
    O->SetBoolField(TEXT("allowed_types_legacy_mapping_complete"),Allowed.IsIdentical(FPCGDataTypeIdentifier(Mask)));
    FPCGDataTypeIdentifier Types;if(!Current->Read(TEXT("pin_GetCurrentTypesID"),[&]{return P->GetCurrentTypesID();},Types))return O;
    O->SetStringField(TEXT("current_types"),Types.ToString());bool MultiData=false,MultiConnections=false,Connected=false;
    if(!Current->Read(TEXT("pin_AllowsMultipleData"),[&]{return P->AllowsMultipleData();},MultiData))return O;
    if(!Current->Read(TEXT("pin_AllowsMultipleConnections"),[&]{return P->AllowsMultipleConnections();},MultiConnections))return O;
    if(!Current->Read(TEXT("pin_IsConnected"),[&]{return P->IsConnected();},Connected))return O;
    O->SetBoolField(TEXT("allows_multiple_data"),MultiData);O->SetBoolField(TEXT("allows_multiple_connections"),MultiConnections);O->SetBoolField(TEXT("connected"),Connected);
    TArray<const UPCGEdge*> Captured;for(const UPCGEdge* E:P->Edges)Captured.Add(E);
    TArray<TSharedPtr<FJsonValue>> Values;
    for(const UPCGEdge* E:Captured)
    {
        if(!ContinueRead())break;if(!E||!E->IsValid())continue;const UPCGPin* Other=E->GetOtherPin(P);const UPCGNode* Owner=Other?Other->Node.Get():nullptr;if(!Other||!Owner)continue;
        if(!ExactPinOwner(Other)||!Current->Pin(Owner)||!Current->PinNode(Other))break;
        if(!Current->OutputBudget(Owner->GetPathName().Len()+Other->Properties.Label.ToString().Len()))break;auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("node_id"),Owner->GetName());V->SetStringField(TEXT("node_path"),Owner->GetPathName());V->SetStringField(TEXT("pin_label"),Other->Properties.Label.ToString());V->SetStringField(TEXT("pin_direction"),Other->IsOutputPin()?TEXT("output"):TEXT("input"));Values.Add(MakeShared<FJsonValueObject>(V));
    }
    O->SetArrayField(TEXT("edges"),Values);return O;
}

UPCGSettings* SettingsFor(UPCGNode* O){if(!O||!Current||!Current->Node(O)){return nullptr;}UPCGSettings* S=nullptr;if(!Current->Read(TEXT("node_GetSettings"),[&]{return O->GetSettings();},S)){return nullptr;}const auto* Bound=Current->Snapshot.NodeSlots.FindByPredicate([&](const FNodeSlot& N){return N.Node==O;});if(!Bound||S!=Bound->Settings){Current->Fail(TEXT("pcg_graph_settings_result_changed"),TEXT("Virtual settings return changed from captured role"));return nullptr;}if(S&&!Current->Pin(S)){return nullptr;}return S;}

FString NodeTitle(UPCGNode* O){FString S;if(!Current||!O||!Current->Node(O)||!Current->Read(TEXT("node_GetNodeTitle"),[&]{return O->GetNodeTitle(EPCGNodeTitleType::ListView).ToString();},S)){return FString();}if(!Current->CaptureBudget(0,S.Len()))return FString();return S;}

TSharedPtr<FJsonObject> PCGNodeToJson(UPCGNode* N,bool Pins)
{
    auto O=MakeShared<FJsonObject>();if(!Current->OutputBudget()||!N||!ContinueRead()||!Current->Node(N))return O;
    if(!Current->CaptureBudget(0,N->GetPathName().Len()+N->GetName().Len()))return O;
    int32 X=0,Y=0;N->GetNodePosition(X,Y);UPCGSettings* S=SettingsFor(N);if(!ContinueRead())return O;const FString Title=NodeTitle(N);if(!ContinueRead())return O;
    O->SetStringField(TEXT("node_id"),N->GetName());O->SetStringField(TEXT("object_path"),N->GetPathName());O->SetStringField(TEXT("title"),Title);O->SetStringField(TEXT("authored_title"),N->GetAuthoredTitleName().ToString());O->SetStringField(TEXT("settings_class"),S?S->GetClass()->GetName():FString());O->SetStringField(TEXT("settings_path"),S?S->GetPathName():FString());O->SetNumberField(TEXT("x"),X);O->SetNumberField(TEXT("y"),Y);
    if(Pins){TArray<TSharedPtr<FJsonValue>> In,Out;const auto Input=NodePins(N,false),Output=NodePins(N,true);for(const auto* P:Input){if(!ContinueRead())break;In.Add(MakeShared<FJsonValueObject>(PCGPinToJson(P)));}if(ContinueRead()){for(const auto* P:Output){if(!ContinueRead())break;Out.Add(MakeShared<FJsonValueObject>(PCGPinToJson(P)));}}O->SetArrayField(TEXT("input_pins"),In);O->SetArrayField(TEXT("output_pins"),Out);}return O;
}

bool BoundedPropertyValue(FWork& W,FProperty* P,const void* Address,int32 Depth=0){if(!P||!Address||Depth>64||!W.CaptureBudget(1))return W.Fail(TEXT("pcg_property_value_budget"),TEXT("Property nesting/slot budget exceeded"));if(auto* Num=CastField<FNumericProperty>(P)){if(Num->IsFloatingPoint()&&!FMath::IsFinite(Num->GetFloatingPointPropertyValue(Address)))return W.Fail(TEXT("pcg_property_nonfinite_stored_value"),TEXT("Actual imported numeric storage is nonfinite; partial state retained"));return true;}if(auto* S=CastField<FStrProperty>(P)){const FString& V=S->GetPropertyValue(Address);return W.CaptureBudget(0,V.Len())&&NoEmbeddedNul(V);}if(auto* S=CastField<FStructProperty>(P)){for(TFieldIterator<FProperty> It(S->Struct);It;++It){auto* Child=*It;for(int32 I=0;I<Child->ArrayDim;++I)if(!BoundedPropertyValue(W,Child,Child->ContainerPtrToValuePtr<void>(Address,I),Depth+1))return false;}return true;}if(auto* A=CastField<FArrayProperty>(P)){FScriptArrayHelper H(A,Address);if(H.Num()>262144)return W.Fail(TEXT("pcg_property_array_budget"),TEXT("Actual array exceeds bounded observation"));for(int32 I=0;I<H.Num();++I)if(!BoundedPropertyValue(W,A->Inner,H.GetRawPtr(I),Depth+1))return false;return true;}if(auto* M=CastField<FMapProperty>(P)){FScriptMapHelper H(M,Address);if(H.GetMaxIndex()>262144)return W.Fail(TEXT("pcg_property_map_budget"),TEXT("Actual map exceeds bounded observation"));for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)&&(!BoundedPropertyValue(W,M->KeyProp,H.GetKeyPtr(I),Depth+1)||!BoundedPropertyValue(W,M->ValueProp,H.GetValuePtr(I),Depth+1)))return false;return true;}if(auto* S=CastField<FSetProperty>(P)){FScriptSetHelper H(S,Address);if(H.GetMaxIndex()>262144)return W.Fail(TEXT("pcg_property_set_budget"),TEXT("Actual set exceeds bounded observation"));for(int32 I=0;I<H.GetMaxIndex();++I)if(H.IsValidIndex(I)&&!BoundedPropertyValue(W,S->ElementProp,H.GetElementPtr(I),Depth+1))return false;return true;}return true;}


template<typename F>bool MutateGraph(FWork& W,const TCHAR* Stage,const TCHAR* Kind,F&& Fn,UPCGNode* Mutable=nullptr,UPCGNode* Added=nullptr,const FString& Key=FString(),bool Replace=false,UPCGPin* TargetPin=nullptr,const TSet<FString>* ExpectedOverride=nullptr)
{const FGraphSnapshot Before=W.Snapshot;if(!W.SDK(Stage,true))return false;if(FString(Kind)==TEXT("notify")||FString(Kind)==TEXT("remove"))W.RetireMutablePins(Mutable);if(FString(Kind)==TEXT("remove"))for(auto& I:W.Objects){auto* O=I->Object.Get();if(O&&(O==Mutable||O->IsIn(Mutable))&&O->GetPackage()==W.Graph->GetPackage())I->Retired=true;}TGuardValue<bool> TransitionScope(W.Capturing,true);Fn();if(!W.After(Stage))return false;if(!Transition(W,Before,Kind,Mutable,Added,Key,Replace,TargetPin,ExpectedOverride))return false;return true;}
bool ExportProperty(FWork& W,UPCGSettings* S,const FString& Name,FString& Out){if(!W.Admit(TEXT("before_property_export")))return false;FProperty* P=FindFProperty<FProperty>(S->GetClass(),*Name);if(!P)return W.Fail(TEXT("pcg_property_missing"),TEXT("Property not found: ")+Name);if(!BoundedPropertyValue(W,P,P->ContainerPtrToValuePtr<void>(S)))return W.Fail(TEXT("pcg_property_value_unavailable"),TEXT("Property value cannot be safely observed"));if(!W.SDK(TEXT("Property_ExportText")))return false;Out.Reset();P->ExportText_InContainer(0,Out,S,S,S,PPF_None);if(!W.After(TEXT("Property_ExportText")))return false;return W.CaptureBudget(1,Out.Len());}
bool VerifyProperty(FWork& W,UPCGSettings* S,const FString& Name,const FString& Expected){FString Actual;if(!ExportProperty(W,S,Name,Actual))return false;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property"),Name);Row->SetStringField(TEXT("expected_canonical_text"),Expected);Row->SetStringField(TEXT("actual_canonical_text"),Actual);const bool Pass=Same(Actual,Expected);Row->SetBoolField(TEXT("exact_readback"),Pass);W.PropertyRows.Add(MakeShared<FJsonValueObject>(Row));return Pass||W.Fail(TEXT("pcg_property_readback_changed"),TEXT("Property changed across notification/callback: ")+Name);}
bool Unsigned(FNumericProperty* P){return CastField<FByteProperty>(P)||CastField<FUInt16Property>(P)||CastField<FUInt32Property>(P)||CastField<FUInt64Property>(P);}
bool PutInteger(FWork& W,FNumericProperty* P,void* A,int64 V){if((Unsigned(P)&&V<0)||!P->CanHoldValue(V))return W.Fail(TEXT("pcg_property_numeric_range"),TEXT("Enum value exceeds destination integer storage"));P->SetIntPropertyValue(A,V);return true;}
bool NumericFits(FNumericProperty* P,double V){if(!FMath::IsFinite(V))return false;if(!P->IsInteger())return P->GetElementSize()!=sizeof(float)||FMath::Abs(V)<=static_cast<double>(MAX_flt);const int32 Bits=P->GetElementSize()*8;if(Unsigned(P)){const double MaxExclusive=FMath::Pow(2.0,static_cast<double>(Bits));return V>=0&&V<MaxExclusive;}const double Bound=FMath::Pow(2.0,static_cast<double>(Bits-1));return V>=-Bound&&V<Bound;}
bool PutNumeric(FWork& W,FNumericProperty* P,void* Address,double V){if(!NumericFits(P,V))return W.Fail(TEXT("pcg_property_numeric_range"),TEXT("Requested value exceeds finite destination numeric storage before cast"));if(P->IsInteger()){if(Unsigned(P))P->SetIntPropertyValue(Address,static_cast<uint64>(V));else P->SetIntPropertyValue(Address,static_cast<int64>(V));}else P->SetFloatingPointPropertyValue(Address,V);return true;}
UObject* LoadObjectValue(FWork& W,const FString& Path,UClass* Type){UObject* O=nullptr;if(Path.IsEmpty())return nullptr;if(!W.Read(TEXT("LoadObject_property_value"),[&]{return StaticLoadObject(Type,nullptr,*NormalizePCGObjectPathForLoad(Path));},O))return nullptr;if(O&&!W.Pin(O))return nullptr;return O;}
bool SetSettingCore(FWork& W,UPCGNode* N,UPCGSettings* S,const FString& Name,const TSharedPtr<FJsonValue>& V,FString& Expected)
{
 if(!W.RequireConstruction(TEXT("Settings_Modify_and_structural_UpdatePins")))return false;
 // Preserve original always-Modify even when a same-value request or later conversion fails.
 if(!W.Write(TEXT("Settings_Modify"),[&]{S->Modify();}))return false;FProperty* P=FindFProperty<FProperty>(S->GetClass(),*Name);if(!P)return W.Fail(TEXT("pcg_property_missing"),TEXT("Property not found: ")+Name);const auto* PropertyClass=P->GetClass();const int32 Size=P->GetSize();const int32 Dimension=P->ArrayDim;const FName FieldName=P->GetFName();auto Refind=[&](){FProperty* Q=FindFProperty<FProperty>(S->GetClass(),FieldName);return Q&&Q==P&&Q->GetClass()==PropertyClass&&Q->GetSize()==Size&&Q->ArrayDim==Dimension;};FString Before;if(!ExportProperty(W,S,Name,Before))return false;W.R.Data->SetStringField(TEXT("last_property_before_text"),Before);if(!Refind())return W.Fail(TEXT("pcg_property_layout_changed"),TEXT("Property layout changed before write"));
 UObject* Loaded=nullptr;if(auto* C=CastField<FClassProperty>(P)){if(V->Type!=EJson::String)return W.Fail(TEXT("pcg_property_class_value"),TEXT("Class property requires class string"));const auto Path=V->AsString();if(Forbidden(Path))return W.Fail(TEXT("pcg_property_dependency_forbidden"),TEXT("Class tooling dependency forbidden"));Loaded=LoadObjectValue(W,Path,UClass::StaticClass());if(!W.Admit(TEXT("after_property_class_load")))return false;if(!Refind())return W.Fail(TEXT("pcg_property_layout_changed"),TEXT("Property replaced across load callback"));if(!Path.IsEmpty()&&!Loaded){Loaded=LoadClassGuarded(NormalizePCGObjectPathForLoad(Path));if(!W.Admit(TEXT("after_fallback_class_load")))return false;if(!Refind())return W.Fail(TEXT("pcg_property_layout_changed"),TEXT("Property replaced across load callback"));if(!Loaded)Loaded=FindClassGuarded(Path);if(!W.Admit(TEXT("after_fallback_class_find")))return false;if(!Refind())return W.Fail(TEXT("pcg_property_layout_changed"),TEXT("Property replaced across load callback"));if(!Loaded)return W.Fail(TEXT("pcg_property_class_not_found"),TEXT("Class not found: ")+Path);}auto* LC=Cast<UClass>(Loaded);if(Loaded&&(!LC||(C->MetaClass&&!LC->IsChildOf(C->MetaClass))))return W.Fail(TEXT("pcg_property_class_mismatch"),TEXT("Loaded class is not compatible with property metaclass"));}
 else if(auto* O=CastField<FObjectPropertyBase>(P)){if(V->Type!=EJson::String)return W.Fail(TEXT("pcg_property_object_value"),TEXT("Object property requires path string"));const auto Path=V->AsString();if(Forbidden(Path))return W.Fail(TEXT("pcg_property_dependency_forbidden"),TEXT("Object tooling dependency forbidden"));Loaded=LoadObjectValue(W,Path,O->PropertyClass);if(!W.Admit(TEXT("after_property_object_load")))return false;if(!Refind())return W.Fail(TEXT("pcg_property_layout_changed"),TEXT("Property replaced across load callback"));if(!Path.IsEmpty()&&!Loaded){Loaded=LoadObjectValue(W,Path,UObject::StaticClass());if(!W.Admit(TEXT("after_fallback_object_load")))return false;if(!Refind())return W.Fail(TEXT("pcg_property_layout_changed"),TEXT("Property replaced across load callback"));if(!Loaded)return W.Fail(TEXT("pcg_property_object_not_found"),TEXT("Object not found: ")+Path);}if(Loaded&&!Loaded->IsA(O->PropertyClass))return W.Fail(TEXT("pcg_property_object_mismatch"),TEXT("Loaded object is not compatible with property class"));}
 if(!Refind())return W.Fail(TEXT("pcg_property_layout_changed"),TEXT("Property layout changed after load"));if(!W.SDK(TEXT("Property_SetValue"),true))return false;void* Address=P->ContainerPtrToValuePtr<void>(S);bool Accepted=false;
 if(auto* B=CastField<FBoolProperty>(P)){B->SetPropertyValue(Address,V->AsBool());Accepted=true;}
 else if(auto* I=CastField<FIntProperty>(P))Accepted=PutNumeric(W,I,Address,V->AsNumber());
 else if(auto* F=CastField<FFloatProperty>(P))Accepted=PutNumeric(W,F,Address,V->AsNumber());
 else if(auto* String=CastField<FStrProperty>(P)){String->SetPropertyValue(Address,V->AsString());Accepted=true;}
 else if(auto* Struct=CastField<FStructProperty>(P);Struct&&Struct->Struct==TBaseStructure<FLinearColor>::Get()&&V->Type==EJson::Array&&V->AsArray().Num()==4){const auto& A=V->AsArray();double X[4];Accepted=true;for(int32 ColorIndex=0;ColorIndex<4;++ColorIndex){X[ColorIndex]=A[ColorIndex]->AsNumber();Accepted&=FMath::IsFinite(X[ColorIndex])&&FMath::Abs(X[ColorIndex])<=static_cast<double>(MAX_flt);}if(Accepted){FLinearColor Color(X[0],X[1],X[2],X[3]);Struct->CopySingleValue(Address,&Color);}else W.Fail(TEXT("pcg_property_numeric_range"),TEXT("LinearColor exceeds float32 storage"));}
 else if(auto* C=CastField<FClassProperty>(P)){C->SetObjectPropertyValue(Address,Loaded);Accepted=true;}
 else if(auto* O=CastField<FObjectPropertyBase>(P)){O->SetObjectPropertyValue(Address,Loaded);Accepted=true;}
 else if(auto* Enum=CastField<FEnumProperty>(P)){auto* U=Enum->GetUnderlyingProperty();int64 E=INDEX_NONE;if(V->Type==EJson::Number)Accepted=PutNumeric(W,U,Address,V->AsNumber());else if(V->Type==EJson::String){FString T=V->AsString();if(T.IsNumeric()){if(LexTryParseString(E,*T))Accepted=PutInteger(W,U,Address,E);}else{FString Short=T;if(Short.Contains(TEXT("::")))Short.Split(TEXT("::"),nullptr,&Short);E=Enum->GetEnum()->GetValueByNameString(Short);if(E==INDEX_NONE)E=Enum->GetEnum()->GetValueByNameString(T);if(E!=INDEX_NONE)Accepted=PutInteger(W,U,Address,E);}}}
 else if(auto* Byte=CastField<FByteProperty>(P)){UEnum* ByteEnum=Byte->GetIntPropertyEnum();double Number=0;bool Have=false;if(V->Type==EJson::Number){Number=V->AsNumber();Have=true;}else if(V->Type==EJson::String&&ByteEnum){auto T=V->AsString();int64 E=INDEX_NONE;if(T.IsNumeric()){Have=LexTryParseString(E,*T);}else{auto Short=T;if(Short.Contains(TEXT("::")))Short.Split(TEXT("::"),nullptr,&Short);E=ByteEnum->GetValueByNameString(Short);if(E==INDEX_NONE)E=ByteEnum->GetValueByNameString(T);Have=E!=INDEX_NONE;}Number=static_cast<double>(E);}else if(!ByteEnum){Number=V->AsNumber();Have=true;}if(Have)Accepted=PutNumeric(W,Byte,Address,Number);}
 // Original PCG fallback after Common converter failure.
 if(!Accepted&&W.R.Error.IsEmpty()){if(auto* NameProp=CastField<FNameProperty>(P)){NameProp->SetPropertyValue(Address,FName(*V->AsString()));Accepted=true;}else if(auto* TextProp=CastField<FTextProperty>(P)){TextProp->SetPropertyValue(Address,FText::FromString(V->AsString()));Accepted=true;}else if(auto* Num=CastField<FNumericProperty>(P)){if(V->Type!=EJson::Number)W.Fail(TEXT("pcg_property_numeric_value_required"),TEXT("Numeric PCG fallback requires JSON number"));else Accepted=PutNumeric(W,Num,Address,V->AsNumber());}}
 if(!Accepted&&W.R.Error.IsEmpty()&&V->Type==EJson::String){const auto Import=V->AsString();if(Forbidden(Import))W.Fail(TEXT("pcg_property_dependency_forbidden"),TEXT("Import tooling dependency forbidden"));else Accepted=P->ImportText_Direct(*Import,Address,S,PPF_None)!=nullptr;}
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property"),Name);Row->SetBoolField(TEXT("SDK_set_or_import_accepted"),Accepted);W.PropertyRows.Add(MakeShared<FJsonValueObject>(Row));if(!W.After(TEXT("Property_SetValue")))return false;if(!Accepted)return W.Fail(TEXT("pcg_property_conversion_failed"),TEXT("Unsupported/invalid property conversion: ")+Name+TEXT("; ImportText may have partially written, no retry"));if(!Refind())return W.Fail(TEXT("pcg_property_layout_changed"),TEXT("Property layout changed after setter/import"));if(!ExportProperty(W,S,Name,Expected))return false;W.R.bChanged|=!Same(Before,Expected);
 if(!MutateGraph(W,TEXT("Settings_Structural_Notify"),TEXT("notify"),[&]{S->OnSettingsChangedDelegate.Broadcast(S,EPCGChangeType::Settings|EPCGChangeType::Structural);},N))return false;if(!W.Admit(TEXT("after_settings_notify")))return false;return VerifyProperty(W,S,Name,Expected);
}
bool SetSetting(FWork& W,UPCGNode* N,UPCGSettings* S,const FString& Name,const TSharedPtr<FJsonValue>& V,FString& Expected)
{return W.Standalone.IsValid()?SetStandaloneSetting(W,N,S,Name,V,Expected):SetSettingCore(W,N,S,Name,V,Expected);}



bool Required(const TSharedPtr<FJsonObject>& P,const TCHAR* K,FString& S){return P->TryGetStringField(K,S)||Current->Fail(TEXT("pcg_graph_missing_parameter"),FString(TEXT("Missing '"))+K+TEXT("' parameter"));}
bool ParsePosition(FWork& W,const TSharedPtr<FJsonObject>& P,int32& X,int32& Y){double A=0,B=0;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(P->TryGetArrayField(TEXT("node_position"),Values)&&Values->Num()>=2){A=(*Values)[0]->AsNumber();B=(*Values)[1]->AsNumber();}else{FString T;if(P->TryGetStringField(TEXT("node_position"),T)){T.TrimStartAndEndInline();T.RemoveFromStart(TEXT("["));T.RemoveFromEnd(TEXT("]"));T.RemoveFromStart(TEXT("("));T.RemoveFromEnd(TEXT(")"));TArray<FString> Parts;T.ParseIntoArray(Parts,TEXT(","),true);if(Parts.Num()>=2){A=FCString::Atof(*Parts[0].TrimStartAndEnd());B=FCString::Atof(*Parts[1].TrimStartAndEnd());}}}if(!FMath::IsFinite(A)||!FMath::IsFinite(B)||FMath::Abs(A)>static_cast<double>(MAX_flt)||FMath::Abs(B)>static_cast<double>(MAX_flt))return W.Fail(TEXT("pcg_node_position_range"),TEXT("Node position exceeds float32 storage before original quantization"));const float FX=static_cast<float>(A),FY=static_cast<float>(B);if(static_cast<double>(FX)<MIN_int32||static_cast<double>(FX)>=2147483648.0||static_cast<double>(FY)<MIN_int32||static_cast<double>(FY)>=2147483648.0)return W.Fail(TEXT("pcg_node_position_range"),TEXT("Quantized node position exceeds int32 cast range"));X=static_cast<int32>(FX);Y=static_cast<int32>(FY);return true;}
void TryCleanupCreated(FWork& W){if(!W.CreatedNode)return;TGuardValue<bool> CleanupScope(W.AllowFailureObservation,true);if(!W.Admit(TEXT("cleanup_new_node_admission"))){W.R.Data->SetBoolField(TEXT("owned_cleanup_skipped_unsafe"),true);return;}auto* N=W.CreatedNode;if(!W.Graph||N->GetOuter()!=W.Graph||N->GetGraph()!=W.Graph||N->GetPackage()!=W.Graph->GetPackage()||!W.Graph->GetNodes().Contains(N)){W.R.Data->SetBoolField(TEXT("owned_cleanup_skipped_identity"),true);return;}W.R.Data->SetBoolField(TEXT("owned_cleanup_attempted"),true);const bool Removed=MutateGraph(W,TEXT("Graph_RemoveOwnedNewNode"),TEXT("remove"),[&]{W.Graph->RemoveNode(N);W.R.Data->SetBoolField(TEXT("owned_cleanup_SDK_returned"),true);},N);W.R.Data->SetBoolField(TEXT("owned_node_absence_verified"),Removed);W.R.Data->SetBoolField(TEXT("cleanup_is_atomic_rollback"),false);if(!Removed)W.R.Data->SetBoolField(TEXT("owned_cleanup_residual_unknown"),true);}
TSharedPtr<FJsonObject> HandleAdd(const TSharedPtr<FJsonObject>& P){auto& W=*Current;FString Path,ClassName;if(!Required(P,TEXT("graph_path"),Path)||!Required(P,TEXT("settings_class"),ClassName))return W.Error(W.R.Error);if(Forbidden(ClassName))return W.Error(TEXT("Settings class tooling dependency forbidden"));if(!BindGraph(W,Path))return W.Error(W.R.Error);UClass* Class=FindSettingsClass(ClassName);if(!ContinueRead())return W.Error(W.R.Error);if(!Class)return W.Error(TEXT("PCG settings class not found: ")+ClassName);if(W.Standalone&&!PrepareStandaloneAdd(W,Class,P))return W.Error(W.R.Error);if(!W.RequireConstruction(TEXT("Graph_AddNodeOfType")))return W.Error(W.R.Error);const FGraphSnapshot Before=W.Snapshot;UPCGSettings* Settings=nullptr;UPCGNode* N=nullptr;if(!W.SDK(TEXT("Graph_AddNodeOfType"),true))return W.Error(W.R.Error);{TGuardValue<bool> TransitionScope(W.Capturing,true);N=W.Graph->AddNodeOfType(Class,Settings);W.R.Data->SetBoolField(TEXT("AddNode_returned_nonnull"),N!=nullptr);W.R.Data->SetBoolField(TEXT("AddSettings_returned_nonnull"),Settings!=nullptr);if(W.Standalone){if(!W.Calls.IsEmpty())W.Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);if(!KeepStandaloneReturns(W,N,Settings))return W.Error(W.R.Error);}if(!W.After(TEXT("Graph_AddNodeOfType")))return W.Error(W.R.Error);if(!N||!Settings||!W.Pin(N)||!W.Pin(Settings))return W.Error(TEXT("Failed to add PCG node; partial graph state may remain"));if(N->GetOuter()!=W.Graph||Settings->GetOuter()!=N||Settings->GetClass()!=Class||N->GetSettingsInterface()!=Settings||N->GetPackage()!=W.Graph->GetPackage()||Settings->GetPackage()!=W.Graph->GetPackage())return W.Error(TEXT("Created exact node/settings owner/class mismatch"));W.CreatedNode=N;W.R.bChanged=true;W.R.Data->SetStringField(TEXT("created_node_path"),N->GetPathName());W.R.Data->SetStringField(TEXT("created_settings_path"),Settings->GetPathName());if(W.Standalone&&!CaptureStandaloneValues(W,Settings))return W.Error(W.R.Error);if(!Transition(W,Before,TEXT("add"),nullptr,N))return W.Error(W.R.Error);}
 if(!W.Admit(TEXT("after_add_transition")))return W.Error(W.R.Error);if(P->HasField(TEXT("node_position"))){int32 X=0,Y=0;if(!ParsePosition(W,P,X,Y)){TryCleanupCreated(W);return W.Error(W.R.Error);}if(!MutateGraph(W,TEXT("Node_SetPosition"),TEXT("metadata"),[&]{N->SetNodePosition(X,Y);},N))return W.Error(W.R.Error);int32 AX=0,AY=0;if(!W.Admit(TEXT("position_readback")))return W.Error(W.R.Error);N->GetNodePosition(AX,AY);if(AX!=X||AY!=Y)return W.Error(TEXT("Actual node position differs from first2 quantized request"));}
 FString Title;if(P->TryGetStringField(TEXT("node_title"),Title)&&!Title.IsEmpty()){const FName Name(*Title);if(!MutateGraph(W,TEXT("Node_SetAuthoredTitle"),TEXT("metadata"),[&]{N->NodeTitle=Name;},N))return W.Error(W.R.Error);if(!W.Admit(TEXT("title_readback"))||N->NodeTitle!=Name)return W.Error(TEXT("Actual node title differs"));}
 const TSharedPtr<FJsonObject>* SettingsObject=nullptr;TMap<FName,FString> Expected;if(P->TryGetObjectField(TEXT("settings"),SettingsObject)){TArray<TPair<FString,TSharedPtr<FJsonValue>>> Entries;for(const auto& Pair:(*SettingsObject)->Values)Entries.Emplace(FString(Pair.Key),Pair.Value);for(const auto& Pair:Entries){FString Value;if(!SetSetting(W,N,Settings,Pair.Key,Pair.Value,Value)){TryCleanupCreated(W);return W.Error(W.R.Error);}Expected.Add(FName(*Pair.Key),Value);}}
 auto Result=PCGNodeToJson(N,true);if(!ContinueRead())return W.Error(W.R.Error);for(const auto& Pair:Expected)if(!VerifyProperty(W,Settings,Pair.Key.ToString(),Pair.Value))return W.Error(W.R.Error);Result->SetStringField(TEXT("graph"),W.Graph->GetPathName());W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;}
TSharedPtr<FJsonObject> HandleConnect(const TSharedPtr<FJsonObject>& P){auto& W=*Current;FString Path,FromId,ToId,FromLabel,ToLabel;if(!Required(P,TEXT("graph_path"),Path)||!Required(P,TEXT("from_node_id"),FromId)||!Required(P,TEXT("to_node_id"),ToId)||!Required(P,TEXT("from_pin"),FromLabel)||!Required(P,TEXT("to_pin"),ToLabel))return W.Error(W.R.Error);if(!BindGraph(W,Path))return W.Error(W.R.Error);auto* From=FindPCGNodeById(W.Graph,FromId);auto* To=FindPCGNodeById(W.Graph,ToId);if(!From||!To)return W.Error(TEXT("Source or target PCG node not found"));auto* A=From->GetOutputPin(FName(*FromLabel));if(W.FreshCurrent&&!W.Admit(TEXT("fresh_after_source_pin_lookup")))return W.Error(W.R.Error);auto* B=To->GetInputPin(FName(*ToLabel));if(!A)return W.Error(TEXT("Source output pin not found: ")+FromLabel);if(!B)return W.Error(TEXT("Target input pin not found: ")+ToLabel);if(!W.PinNode(A)||!W.PinNode(B))return W.Error(W.R.Error);const FString Requested=EdgeKey(From,A->Properties.Label,To,B->Properties.Label);const bool Existing=W.Snapshot.Edges.ContainsByPredicate([&](const FEdgeSlot& E){return E.Input==A&&E.Output==B;});bool Multiple=false;if(!W.Read(TEXT("Pin_AllowsMultipleConnections"),[&]{return B->AllowsMultipleConnections();},Multiple))return W.Error(W.R.Error);
 TSet<FString> Expected;for(const auto& E:W.Snapshot.Edges)Expected.Add(E.Key);TArray<TPair<FString,UPCGPin*>> TargetOrder;if(W.FreshCurrent){const auto* PinSlot=W.Snapshot.Pins.FindByPredicate([&](const FPinSlot& P){return P.Pin==B;});if(!PinSlot)return W.Error(TEXT("Target pin current role missing"));for(UPCGEdge* E:PinSlot->Edges){const auto* S=W.Snapshot.Edges.FindByPredicate([&](const FEdgeSlot& X){return X.Edge==E;});if(!S)return W.Error(TEXT("Target edge missing from captured graph"));TargetOrder.Emplace(S->Key,S->Input);}}else{for(UPCGEdge* E:B->Edges){const auto* S=W.Snapshot.Edges.FindByPredicate([&](const FEdgeSlot& X){return X.Edge==E;});if(!S)return W.Error(TEXT("Target edge missing from captured graph"));TargetOrder.Emplace(S->Key,S->Input);}}if(!Existing)TargetOrder.Emplace(Requested,A);Expected.Add(Requested);if(!Multiple){bool HaveKept=false;for(int32 I=TargetOrder.Num()-1;I>=0;--I){EPCGDataTypeCompatibilityResult Compatibility{};auto* Other=TargetOrder[I].Value;if(!W.Read(TEXT("Pin_GetCompatibilityWithOtherPin"),[&]{return B->GetCompatibilityWithOtherPin(Other);},Compatibility))return W.Error(W.R.Error);const bool Compatible=PCGDataTypeCompatibilityResult::IsValid(Compatibility)||Compatibility==EPCGDataTypeCompatibilityResult::TypeCompatibleSubtypeNotCompatible;const bool Remove=HaveKept||!Compatible;if(Remove)Expected.Remove(TargetOrder[I].Key);else HaveKept=true;}}
 W.R.Data->SetBoolField(TEXT("requested_edge_already_existed"),Existing);W.R.Data->SetBoolField(TEXT("input_allows_multiple_connections"),Multiple);if(!Existing&&!W.RequireConstruction(TEXT("Graph_AddEdge_allocates_UPCGEdge")))return W.Error(W.R.Error);
 // Existing endpoint AddEdgeTo is non-allocating; the SDK may still clean incompatible edges and notify.
 UPCGNode* Returned=nullptr;if(!MutateGraph(W,TEXT("Graph_AddEdge"),TEXT("connect"),[&]{Returned=W.Graph->AddEdge(From,FName(*FromLabel),To,FName(*ToLabel));W.R.Data->SetBoolField(TEXT("AddEdge_returned_exact_to"),Returned==To);},nullptr,nullptr,Requested,!Multiple,B,&Expected))return W.Error(W.R.Error);if(!W.Admit(TEXT("connect_readback")))return W.Error(W.R.Error);const bool Present=W.Snapshot.Edges.ContainsByPredicate([&](const FEdgeSlot& E){return E.Input==A&&E.Output==B;});if(!Present)return W.Error(TEXT("SDK did not retain requested exact reciprocal edge; To return is not success"));if(!W.Write(TEXT("Graph_MarkPackageDirty"),[&]{W.Graph->MarkPackageDirty();}))return W.Error(W.R.Error);W.R.bChanged|=!Existing||W.Graph->GetPackage()->IsDirty()!=W.DirtyBefore;auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("graph"),W.Graph->GetPathName());O->SetStringField(TEXT("from_node_id"),From->GetName());O->SetStringField(TEXT("from_pin"),FromLabel);O->SetStringField(TEXT("to_node_id"),To->GetName());O->SetStringField(TEXT("to_pin"),ToLabel);W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return O;}
TSharedPtr<FJsonObject> HandleSetting(const TSharedPtr<FJsonObject>& P){auto& W=*Current;FString Path,Id,Name;if(!Required(P,TEXT("graph_path"),Path)||!Required(P,TEXT("node_id"),Id)||!Required(P,TEXT("property_name"),Name))return W.Error(W.R.Error);auto V=P->TryGetField(TEXT("value"));if(!V)return W.Error(TEXT("Missing 'value' parameter"));if(!BindGraph(W,Path))return W.Error(W.R.Error);auto* N=FindPCGNodeById(W.Graph,Id);if(!N)return W.Error(TEXT("PCG node not found: ")+Id);UPCGSettings* S=nullptr;if(!WriterSettings(W,N,S))return W.Error(W.R.Error);FString Expected;if(!SetSetting(W,N,S,Name,V,Expected))return W.Error(W.R.Error);if(!W.Write(TEXT("Graph_MarkPackageDirty"),[&]{W.Graph->MarkPackageDirty();}))return W.Error(W.R.Error);auto O=PCGNodeToJson(N,true);if(!ContinueRead()||!VerifyProperty(W,S,Name,Expected))return W.Error(W.R.Error);O->SetStringField(TEXT("updated_property"),Name);W.R.bChanged|=W.Graph->GetPackage()->IsDirty()!=W.DirtyBefore;W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return O;}


FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation){bool LeafSuccess=Legacy.IsValid();if(Legacy&&Legacy->HasField(TEXT("success"))){bool B=false;LeafSuccess=Legacy->TryGetBoolField(TEXT("success"),B)&&B;}bool Typed=false;W.R.Data->TryGetBoolField(TEXT("typed_target_postcondition_passed"),Typed);if(!LeafSuccess&&W.R.Error.IsEmpty()){FString E;if(Legacy)Legacy->TryGetStringField(TEXT("error"),E);W.Fail(TEXT("pcg_graph_authoring_failed"),E.IsEmpty()?TEXT("Dedicated PCG operation failed; actual prefix retained"):E);}const bool Complete=LeafSuccess&&Typed&&W.R.Error.IsEmpty()&&W.Admit(TEXT("final_exact_scoped_admission"));W.R.bSuccess=Complete;W.R.bPostconditionPassed=Complete;if(Complete)W.R.bChangeStateKnown=true;else if(W.R.bMutationAttempted)W.R.bChangeStateKnown=false;if(!Complete&&W.R.Error.IsEmpty())W.Fail(TEXT("pcg_graph_postcondition_incomplete"),TEXT("Supported typed graph/property transition not complete"));W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("supported_variant_complete"),Complete);W.R.Data->SetObjectField(TEXT("native_call_counts"),W.Counts);W.R.Data->SetObjectField(TEXT("actual_observation_prefix"),W.Prefix);W.R.Data->SetArrayField(TEXT("actual_call_prefix"),W.Calls);W.R.Data->SetArrayField(TEXT("graph_transition_rows"),W.Transitions);W.R.Data->SetArrayField(TEXT("property_rows"),W.PropertyRows);W.R.Data->SetBoolField(TEXT("explicit_compile_requested"),false);W.R.Data->SetBoolField(TEXT("explicit_compile_attempted"),false);W.R.Data->SetBoolField(TEXT("explicit_save_requested"),false);W.R.Data->SetBoolField(TEXT("explicit_save_attempted"),false);W.R.Data->SetBoolField(TEXT("PCG_generation_completed"),false);W.R.Data->SetBoolField(TEXT("PCG_cache_recompile_may_occur_inside_SDK"),true);W.R.Data->SetBoolField(TEXT("game_runtime_Cubeless_dependency_added"),false);W.R.Data->SetBoolField(TEXT("post_failure_live_projection_skipped"),!W.R.Error.IsEmpty());W.R.Data->SetStringField(TEXT("scope"),TEXT("Three source-implemented public PCG graph operations, with preserved construction/GC health gate. New node/edge and structural pin reconstruction are not production-enabled or Engine-validated. Existing exact-edge SDK path may still notify/clean incompatible edges; actual reciprocal topology is authoritative. Any setting conversion uses public reflection; no private PCGEditor, Python nested pin/array traversal, generic server/transport, save, explicit recompile, generation, sleep/flush/pump or automatic replay/rollback."));if(W.Standalone)W.R.Data->SetStringField(TEXT("scope"),TEXT("Request-local existing owned native PCG graph with exact Grid/DensityRemap/Distance/RandomChoice/DensityFilter/AttributeNoise/AttributeFiltering/CreatePoints/GetActorProperty settings, exact PointExtentsModifier/BoundsModifier authored fields and bounded stock Spawner fields, with held source/current/whole physical topology; SurfaceSampler8/Difference4 and Union2/InnerIntersection2/OuterIntersection3 own fields are also supported by standalone Add/Connect/Set; the new spatial composition profile is implemented/usage validation pending and preserves captured dynamic input declarations without adding dynamic-pin writers. Sampler debug bKeepZeroDensityPoints is transient editor RAM state, not persistence proof; no runtime sampling/difference or child-source expansion claimed. Global construction GC health remains unresolved. Other settings/field shapes, generation, compile/save and original full PCG pipeline remain outside this standalone slice; actual partial/unknown prefix is authoritative."));W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);}
using FHandler=TSharedPtr<FJsonObject>(*)(const TSharedPtr<FJsonObject>&);
FString Run(const FString& Text,const TCHAR* Operation,FHandler Handler){FWork W;TSharedPtr<FJsonObject> P;if(!Parse(Text,P)){W.Fail(TEXT("pcg_graph_invalid_params_json"),TEXT("Finite bounded JSON object without duplicate keys/embedded NUL required"));return Finish(W,W.Error(W.R.Error),Operation);}W.R.Data->SetObjectField(TEXT("requested_params"),P);if(!IsInGameThread()){W.Fail(TEXT("pcg_graph_game_thread_required"),TEXT("Editor game thread required"));return Finish(W,W.Error(W.R.Error),Operation);}if(Running){W.Fail(TEXT("pcg_graph_reentrant_request"),TEXT("Dedicated PCG request active; nested dispatch refused"));return Finish(W,W.Error(W.R.Error),Operation);}TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);auto Legacy=Handler(P);if(W.R.Error.IsEmpty()&&W.SubgraphFinalCheck&&!W.SubgraphFinalCheck())return Finish(W,W.Error(W.R.Error),Operation);if(W.R.Error.IsEmpty()&&W.Graph&&W.Admit(TEXT("safe_after_projection"))){W.R.Data->SetObjectField(TEXT("after"),SnapshotJson(W.Snapshot));W.R.Data->SetBoolField(TEXT("package_dirty_before"),W.DirtyBefore);W.R.Data->SetBoolField(TEXT("package_dirty_after"),W.Graph->GetPackage()->IsDirty());}return Finish(W,Legacy,Operation);}
}
FString AddNode(const FString& ParamsJson){return Run(ParamsJson,TEXT("add_pcg_node"),&HandleAdd);}
FString ConnectNodes(const FString& ParamsJson){return Run(ParamsJson,TEXT("connect_pcg_nodes"),&HandleConnect);}
FString SetNodeSetting(const FString& ParamsJson){return Run(ParamsJson,TEXT("set_pcg_node_setting"),&HandleSetting);}
}

// LS: request-owned finite construction. No public construction gate is changed.
namespace Cubeless::UE58::PCGGraphAuthoring
{
namespace
{
FProperty* FreshNativeField(UClass* C,FName Name)
{for(auto Super=C->GetSuperStructIterator();Super;++Super)for(TFieldIterator<FProperty> It(*Super,EFieldIterationFlags::None);It;++It)if(It->GetFName()==Name)return *It;return nullptr;}
struct FFreshReference
{
 UObject* Owner=nullptr;FProperty* Property=nullptr;FObjectProperty* Object=nullptr;FArrayProperty* Array=nullptr;TArray<TObjectPtr<UObject>> Handles;
 bool Read(TArray<TObjectPtr<UObject>>& Out)const
 {
  Out.Reset();if(FreshNativeField(Owner->GetClass(),Property->GetFName())!=Property)return false;
  if(Array){FScriptArrayHelper H(Array,Array->ContainerPtrToValuePtr<void>(Owner));if(H.Num()>262144)return false;for(int32 I=0;I<H.Num();++I){const auto& Handle=*Object->GetObjectPtrPropertyValuePtr(H.GetRawPtr(I));if(!Handle.IsResolved())return false;Out.Add(Handle);}}
  else{const auto& Handle=*Object->GetObjectPtrPropertyValuePtr(Property->ContainerPtrToValuePtr<void>(Owner));if(!Handle.IsResolved())return false;Out.Add(Handle);}return true;
 }
 bool Exact()const{TArray<TObjectPtr<UObject>> Now;return Read(Now)&&Now==Handles;}
};
bool FreshObjectEquals(UObject* O,const TCHAR* Name,const UObject* Expected)
{auto* P=CastField<FObjectProperty>(FreshNativeField(O->GetClass(),Name));if(!P||P->ArrayDim!=1)return false;auto* H=P->GetObjectPtrPropertyValuePtr(P->ContainerPtrToValuePtr<void>(O));return H->IsResolved()&&*H==Expected;}
template<typename T>bool FreshArrayEquals(UObject* O,const TCHAR* Name,const TArray<T*>& Expected)
{auto* A=CastField<FArrayProperty>(FreshNativeField(O->GetClass(),Name));auto* P=A?CastField<FObjectProperty>(A->Inner):nullptr;if(!P)return false;FScriptArrayHelper H(A,A->ContainerPtrToValuePtr<void>(O));if(H.Num()!=Expected.Num())return false;for(int32 I=0;I<H.Num();++I){auto* Handle=P->GetObjectPtrPropertyValuePtr(H.GetRawPtr(I));if(!Handle->IsResolved()||*Handle!=Expected[I])return false;}return true;}
bool FreshSnapshotCurrent(const FGraphSnapshot& S)
{
 if(!FreshObjectEquals(S.Graph,TEXT("InputNode"),S.Input)||!FreshObjectEquals(S.Graph,TEXT("OutputNode"),S.Output)||!FreshArrayEquals(S.Graph,TEXT("Nodes"),S.Nodes))return false;
 for(const auto& N:S.NodeSlots)if(!Live(N.Node)||N.Node->GetOuter()!=S.Graph||!FreshObjectEquals(N.Node,TEXT("SettingsInterface"),N.Interface)||!FreshArrayEquals(N.Node,TEXT("InputPins"),N.Input)||!FreshArrayEquals(N.Node,TEXT("OutputPins"),N.Output)||N.Node->PositionX!=N.X||N.Node->PositionY!=N.Y||N.Node->NodeTitle!=N.Title)return false;
 for(const auto& P:S.Pins)if(!Live(P.Pin)||!P.Pin->Node.IsResolved()||P.Pin->Node!=P.Node||!FPCGPinProperties::StaticStruct()->CompareScriptStruct(&P.Pin->Properties,&P.Properties,0)||!FreshArrayEquals(P.Pin,TEXT("Edges"),P.Edges))return false;
 for(const auto& E:S.Edges)if(!Live(E.Edge)||!E.Edge->InputPin.IsResolved()||!E.Edge->OutputPin.IsResolved()||E.Edge->InputPin!=E.Input||E.Edge->OutputPin!=E.Output)return false;return true;
}
struct FFreshField
{
 FProperty* P=nullptr;FFieldClass* Kind=nullptr;UStruct* Owner=nullptr;FName Name;uint64 Flags=0;int32 Offset=0,Size=0,Dim=0;
 explicit FFreshField(FProperty* Q):P(Q),Kind(Q->GetClass()),Owner(Q->GetOwnerStruct()),Name(Q->GetFName()),Flags(Q->GetPropertyFlags()),Offset(Q->GetOffset_ForInternal()),Size(Q->GetSize()),Dim(Q->ArrayDim){}
 bool Exact()const{return P->GetClass()==Kind&&P->GetOwnerStruct()==Owner&&P->GetFName()==Name&&P->GetPropertyFlags()==Flags&&P->GetOffset_ForInternal()==Offset&&P->GetSize()==Size&&P->ArrayDim==Dim;}
};
struct FFreshClass
{
 UClass* Class=nullptr;UObject* CDO=nullptr;UPackage* Package=nullptr;uint32 PackageFlags=0;EObjectFlags CDOFlags=RF_NoFlags;EClassFlags Flags=CLASS_None;int32 Size=0;TArray<FProperty*> Inventory;TArray<FFreshField> Fields;TObjectPtr<UObject> CDOHandle;TArray<UStruct*> Supers;
 bool Exact()const
 {
  if(!Live(Class)||Class->GetPackage()!=Package||!Live(Package)||Package->IsDirty()||Package->GetPackageFlags()!=PackageFlags||Class->GetClassFlags()!=Flags||Class->GetPropertiesSize()!=Size||!Class->ClassDefaultObject.IsResolved()||Class->ClassDefaultObject!=CDOHandle||Class->ClassDefaultObject!=CDO||(CDO&&(!Live(CDO)||CDO->GetFlags()!=CDOFlags)))return false;
  TArray<UStruct*> CurrentSupers;for(auto It=Class->GetSuperStructIterator();It;++It)CurrentSupers.Add(*It);if(CurrentSupers!=Supers)return false;
  TArray<FProperty*> CurrentFields;for(UStruct* Super:Supers)for(TFieldIterator<FProperty> It(Super,EFieldIterationFlags::None);It;++It)CurrentFields.Add(*It);if(CurrentFields!=Inventory)return false;
  for(const auto& Field:Fields)if(!Field.Exact())return false;return true;
 }
};
struct FFreshNodeSpec
{
 FString Id,Title,ClassName;UClass* Class=nullptr;bool GeneralNative=false;int32 X=0,Y=0;TSharedPtr<FJsonObject> Settings,Plans,Inline,Aliases;
  bool Spawner=false,AttributePhase=false,AttributeNameReady=false,MaterialsReady=false,WeightedPhase=false;FString AttributeText,AttributeError,WeightedError;FName Attribute;TArray<FName> Materials;TArray<FPCGMeshSelectorWeightedEntry> MeshEntries;
};
struct FFreshRequest
{
 FWork& W;FString PackagePath,ObjectPath,AssetName,Folder,File;bool DryRun=false,Save=true,Overwrite=false,AllowOverwrite=false,HaveTarget=false,GeneralNative=false;
 TArray<FFreshNodeSpec> Nodes;TArray<TSharedPtr<FJsonValue>> Edges;TArray<FString> ForbiddenPrefixes;TArray<FFreshClass> Sources;
 TStrongObjectPtr<UFactory> Factory{nullptr};UClass* FactoryClass=nullptr;FObjectProperty* TemplateProperty=nullptr;TStrongObjectPtr<UPCGGraph> Scratch{nullptr},Target{nullptr};
 IAssetRegistry* Registry=nullptr;IAssetTools* AssetTools=nullptr;ISourceControlModule* SourceControl=nullptr;TSet<FString> DirtyBefore;TArray<FGraphSnapshot> Completed;FBoolProperty* CreateProperty=nullptr;void* Undo=GUndo;
 FStandaloneRequest* DataSourceWork=nullptr;bool DataSourceProfile=false;
 bool CapturingReferences=false;TArray<FFreshReference> CaptureReferences;TArray<UObject*> CaptureObjects;
  UPCGNode* ActiveSettingsNode=nullptr;UPCGSettings* ActiveSettings=nullptr;
  bool InlineSpawnerProfile=false;UPCGStaticMeshSpawnerSettings* SpawnerMutableSettings=nullptr;FName SpawnerMutableField;TArray<TFunction<bool()>> SpawnerChecks,ExternalAssetChecks;TArray<TFunction<bool(bool)>> SpawnerSaveChecks;
   bool SamplingDifferenceProfile=false;bool TransformPointsProfile=false;bool PointBoundsProfile=false;UPCGSettings* PointBoundsMutableSettings=nullptr;FProperty* PointBoundsMutableField=nullptr;TArray<TFunction<bool()>> PointBoundsValueChecks;
  TArray<TFunction<bool()>> GeneralScalarChecks;UPCGSettings* GeneralMutableSettings=nullptr;FProperty* GeneralMutableField=nullptr;
  bool CaptureBegin(UPCGGraph* G)
 {
  CapturingReferences=false;CaptureReferences.Reset();CaptureObjects.Reset();GetObjectsWithOuter(G,CaptureObjects,true);CaptureObjects.Insert(G,0);
  if(CaptureObjects.Num()>262144)return Fail(TEXT("pcg_fresh_capture_budget"),TEXT("Owned object tree exceeds bounded capture"));
  for(UObject* O:CaptureObjects)
  {
   if(!Live(O))continue;if(!KeepReturned(O,TEXT("capture_owned_object")))return false;TArray<FName> Names;
   if(O==G)Names={TEXT("InputNode"),TEXT("OutputNode"),TEXT("Nodes")};else if(O->GetClass()==UPCGNode::StaticClass()||O->GetClass()==UPCGSubgraphNode::StaticClass())Names={TEXT("SettingsInterface"),TEXT("InputPins"),TEXT("OutputPins")};else if(O->GetClass()==UPCGPin::StaticClass())Names={TEXT("Node"),TEXT("Edges")};else if(O->GetClass()==UPCGEdge::StaticClass())Names={TEXT("InputPin"),TEXT("OutputPin")};
   for(FName Name:Names){FFreshReference R;R.Owner=O;R.Property=FreshNativeField(O->GetClass(),Name);if(!R.Property)return false;R.Array=CastField<FArrayProperty>(R.Property);R.Object=CastField<FObjectProperty>(R.Array?R.Array->Inner:R.Property);if(!R.Object||!R.Read(R.Handles))return false;CaptureReferences.Add(MoveTemp(R));}
  }
  CapturingReferences=true;return true;
 }
 TArray<UObject*> CaptureValues(UObject* Owner,FName Name)
 {
  TArray<UObject*> Out;const auto* R=CaptureReferences.FindByPredicate([&](const FFreshReference& X){return X.Owner==Owner&&X.Property->GetFName()==Name;});if(!R||!R->Exact()){Fail(TEXT("pcg_fresh_capture_changed"),TEXT("Current reflected reference inventory changed"));return Out;}
  for(const auto& H:R->Handles){UObject* Found=nullptr;for(UObject* O:CaptureObjects)if(H==O){Found=O;break;}if(!Found){Fail(TEXT("pcg_fresh_reference_foreign"),TEXT("Reference is not in the exact strong owned graph tree"));return {};}Out.Add(Found);}return Out;
 }
 UPackage* TargetPackage=nullptr;EObjectFlags TargetFlags=RF_NoFlags,PackageObjectFlags=RF_NoFlags;uint32 PackageFlags=0;bool SavedAccepted=false,StandalonePackageTransactionalExpected=false;
 explicit FFreshRequest(FWork& Work):W(Work){}
 bool Fail(const TCHAR* C,const FString& M){return W.Fail(C,M);}
 bool Current()
 {
  if(GIsImportingT3D)return Fail(TEXT("pcg_fresh_import_unsupported"),TEXT("T3D import is active; no fresh lookup or writer is admitted"));
  if(GUndo!=Undo)return Fail(TEXT("pcg_fresh_transaction_changed"),TEXT("Active transaction role changed across an SDK boundary"));
  for(const auto& Source:Sources)if(!Source.Exact())return Fail(TEXT("pcg_fresh_source_changed"),TEXT("Held native class/CDO/layout changed"));
   for(const auto& Check:SpawnerChecks)if(!Check())return Fail(TEXT("pcg_fresh_spawner_changed"),TEXT("Exact stock Spawner authored/selector expected role changed; no replacement baseline"));
   for(const auto& Check:ExternalAssetChecks)if(!Check())return Fail(TEXT("pcg_fresh_inline_asset_changed"),TEXT("Held loaded mesh/material identity or observed flags/dirty changed"));
    for(const auto& Check:GeneralScalarChecks)if(!Check())return Fail(TEXT("pcg_general_scalar_changed"),TEXT("Held general native scalar authored expectation changed outside its exact selected setter"));
     for(const auto& Check:PointBoundsValueChecks)if(!Check())return Fail(TEXT("pcg_fresh_point_bounds_changed"),TEXT("Captured exact finite authored field/source or a nonselected expected value changed"));
  if(CapturingReferences)for(const auto& Ref:CaptureReferences)if(!Ref.Exact())return Fail(TEXT("pcg_fresh_capture_changed"),TEXT("Public resolved reference inventory changed during capture callback"));
  if(ActiveSettingsNode&&(!W.Graph||!FreshArrayEquals(W.Graph,TEXT("Nodes"),W.Snapshot.Nodes)||!FreshObjectEquals(W.Graph,TEXT("InputNode"),W.Snapshot.Input)||!FreshObjectEquals(W.Graph,TEXT("OutputNode"),W.Snapshot.Output)||ActiveSettingsNode->GetOuter()!=W.Graph||!FreshObjectEquals(ActiveSettingsNode,TEXT("SettingsInterface"),ActiveSettings)||!Live(ActiveSettings)||ActiveSettings->GetOuter()!=ActiveSettingsNode))return Fail(TEXT("pcg_fresh_setting_owner_changed"),TEXT("Active setting callback changed the exact graph/node/settings ownership role"));
  if(FactoryClass&&TemplateProperty&&CreateProperty)
  {
   const auto* Source=Sources.FindByPredicate([&](const FFreshClass& S){return S.Class==FactoryClass;});if(!Source||!Source->CDO)return Fail(TEXT("pcg_fresh_factory_cdo_changed"),TEXT("Held exact factory CDO is unavailable"));
   auto* CDO=static_cast<UFactory*>(Source->CDO);auto Supported=CDO->SupportedClass;auto* Template=TemplateProperty->GetObjectPtrPropertyValuePtr(TemplateProperty->ContainerPtrToValuePtr<void>(CDO));
   if(!Supported.GetGCPtr().IsResolved()||Supported.GetGCPtr()!=UPCGGraph::StaticClass()||!Template->IsResolved()||*Template!=nullptr||!CreateProperty->GetPropertyValue_InContainer(CDO))return Fail(TEXT("pcg_fresh_factory_cdo_changed"),TEXT("Factory source policy fields changed"));
  }
  if(SourceControl&&SourceControl->IsEnabled())return Fail(TEXT("pcg_source_control_policy_unsupported"),TEXT("Source control must remain observably disabled"));
  if(Factory)
  {
   auto Supported=Factory->SupportedClass;
   if(Factory->GetClass()!=FactoryClass||!Supported.GetGCPtr().IsResolved()||Supported.GetGCPtr()!=UPCGGraph::StaticClass()||!CreateProperty||!CreateProperty->GetPropertyValue_InContainer(Factory.Get())||
      !TemplateProperty||FreshNativeField(FactoryClass,TEXT("TemplateGraph"))!=TemplateProperty||
      !TemplateProperty->GetObjectPtrPropertyValuePtr(TemplateProperty->ContainerPtrToValuePtr<void>(Factory.Get()))->IsResolved()||
      *TemplateProperty->GetObjectPtrPropertyValuePtr(TemplateProperty->ContainerPtrToValuePtr<void>(Factory.Get()))!=nullptr)
       return Fail(TEXT("pcg_fresh_factory_changed"),TEXT("Exact factory SupportedClass/TemplateGraph null role changed"));
  }
  for(const auto& S:Completed)if(!FreshSnapshotCurrent(S))return Fail(TEXT("pcg_fresh_completed_graph_changed"),TEXT("Earlier request-owned resolved physical inventory changed"));
  if(Target)
  {
   if(Target->GetOutermost()!=TargetPackage||Target->GetOuter()!=TargetPackage||TargetPackage->GetOuter()!=nullptr||!Same(Target->GetPathName(),ObjectPath)||Target->GetClass()!=UPCGGraph::StaticClass())return Fail(TEXT("pcg_fresh_target_changed"),TEXT("Fresh selected persistent graph/package role changed"));
   const EObjectFlags Actual=Target->GetFlags(),ActualPackage=TargetPackage->GetFlags();const uint32 ActualPackageFlags=TargetPackage->GetPackageFlags();
   EObjectFlags ExpectedPackageObjectFlags=StandalonePackageTransactionalExpected?static_cast<EObjectFlags>(PackageObjectFlags|RF_Transactional):PackageObjectFlags;
   if(!StandalonePackageTransactionalExpected&&ActualPackage!=PackageObjectFlags&&W.Standalone&&Target.Get()==W.Graph&&TargetPackage==W.Graph->GetPackage()&&!W.DirtyBefore&&Undo&&GUndo==Undo&&Actual==TargetFlags&&ActualPackageFlags==PackageFlags&&(PackageObjectFlags&RF_Transactional)==0&&ActualPackage==static_cast<EObjectFlags>(PackageObjectFlags|RF_Transactional)&&!TargetPackage->HasAnyPackageFlags(PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn)&&W.StandalonePackageCleanBeforeCall&&TargetPackage->IsDirty()&&W.StandaloneDirtyCall!=INDEX_NONE&&W.StandaloneDirtyCall==W.Calls.Num()-1)
   {
    const auto Row=W.Calls.Last()->AsObject();FString ReturnedStage;bool Entered=false,Returned=false,Mutation=false;
    const bool ActualModifyReturn=Row.IsValid()&&Row->TryGetStringField(TEXT("stage"),ReturnedStage)&&Row->TryGetBoolField(TEXT("entered"),Entered)&&Entered&&Row->TryGetBoolField(TEXT("returned"),Returned)&&Returned&&Row->TryGetBoolField(TEXT("mutation_attempted"),Mutation)&&Mutation&&Same(W.R.Stage,FString(TEXT("after_"))+ReturnedStage)&&(Same(ReturnedStage,TEXT("Graph_AddNodeOfType"))||Same(ReturnedStage,TEXT("Graph_AddEdge"))||Same(ReturnedStage,TEXT("Settings_Modify"))||Same(ReturnedStage,TEXT("Graph_MarkPackageDirty")));
    if(ActualModifyReturn)
    {
     StandalonePackageTransactionalExpected=true;ExpectedPackageObjectFlags=static_cast<EObjectFlags>(PackageObjectFlags|RF_Transactional);
     auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),W.R.Stage);J->SetStringField(TEXT("object"),TargetPackage->GetPathName());J->SetStringField(TEXT("property"),TEXT("UPackage EObjectFlags"));J->SetStringField(TEXT("captured_before"),FString::Printf(TEXT("%u"),static_cast<uint32>(PackageObjectFlags)));J->SetStringField(TEXT("expected_after"),FString::Printf(TEXT("%u"),static_cast<uint32>(ExpectedPackageObjectFlags)));J->SetStringField(TEXT("actual"),FString::Printf(TEXT("%u"),static_cast<uint32>(ActualPackage)));J->SetStringField(TEXT("returned_SDK_stage"),ReturnedStage);J->SetNumberField(TEXT("returned_call_index"),W.StandaloneDirtyCall);J->SetBoolField(TEXT("same_owned_target_package"),true);J->SetBoolField(TEXT("same_nonnull_transaction"),true);J->SetBoolField(TEXT("clean_before_call"),true);J->SetBoolField(TEXT("dirty_after_return"),true);J->SetBoolField(TEXT("only_RF_Transactional_added"),true);J->SetBoolField(TEXT("non_PIE_script_compiled_in_package"),true);J->SetBoolField(TEXT("target_EObjectFlags_unchanged"),true);J->SetBoolField(TEXT("EPackageFlags_unchanged"),true);J->SetBoolField(TEXT("captured_baseline_changed"),false);J->SetBoolField(TEXT("remaining_source_and_topology_guards_continue"),true);W.R.Data->SetObjectField(TEXT("pcg_standalone_package_transactional_transition"),J);
    }
   }
   if(Actual!=TargetFlags||ActualPackage!=ExpectedPackageObjectFlags||ActualPackageFlags!=PackageFlags)
   {
    if(!W.R.Data->HasField(TEXT("pcg_target_flags_first_failure")))
    {
     const bool TargetMismatch=Actual!=TargetFlags,PackageObjectMismatch=ActualPackage!=ExpectedPackageObjectFlags;
     auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),W.R.Stage);J->SetStringField(TEXT("object"),TargetMismatch?ObjectPath:TargetPackage->GetPathName());J->SetStringField(TEXT("property"),TargetMismatch?TEXT("UPCGGraph EObjectFlags"):(PackageObjectMismatch?TEXT("UPackage EObjectFlags"):TEXT("UPackage PackageFlags")));
     const uint32 Expected=TargetMismatch?static_cast<uint32>(TargetFlags):(PackageObjectMismatch?static_cast<uint32>(ExpectedPackageObjectFlags):PackageFlags);const uint32 Observed=TargetMismatch?static_cast<uint32>(Actual):(PackageObjectMismatch?static_cast<uint32>(ActualPackage):ActualPackageFlags);
     J->SetStringField(TEXT("expected"),FString::Printf(TEXT("%u"),Expected));J->SetStringField(TEXT("actual"),FString::Printf(TEXT("%u"),Observed));J->SetBoolField(TEXT("target_flags_equal"),!TargetMismatch);J->SetBoolField(TEXT("package_object_flags_equal"),!PackageObjectMismatch);J->SetBoolField(TEXT("package_flags_equal"),ActualPackageFlags==PackageFlags);W.R.Data->SetObjectField(TEXT("pcg_target_flags_first_failure"),J);
    }
    return Fail(TEXT("pcg_fresh_target_flags_changed"),TEXT("Target/package flags changed outside captured factory/save boundary"));
   }
  }
  return true;
 }
 bool KeepReturned(const UObject* O,const TCHAR* Key)
 {
  W.R.Data->SetBoolField(FString(Key)+TEXT("_returned_nonnull"),O!=nullptr);W.R.Data->SetBoolField(FString(Key)+TEXT("_identity_observed"),false);
  if(!O||!IsInGameThread()||IsGarbageCollecting()||GIsSavingPackage||GExitPurge||IsEngineExitRequested()||!Live(O))return false;
  auto Identity=MakeUnique<FObjectIdentity>(O);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object_path"),Identity->Path);J->SetStringField(TEXT("class_path"),O->GetClass()->GetPathName());J->SetStringField(TEXT("outer_path"),O->GetOuter()?O->GetOuter()->GetPathName():FString());J->SetStringField(TEXT("package_path"),O->GetOutermost()->GetPathName());J->SetStringField(TEXT("flags_string"),FString::Printf(TEXT("%u"),static_cast<uint32>(O->GetFlags())));
  if(!W.Objects.ContainsByPredicate([&](const TUniquePtr<FObjectIdentity>& I){return !I->Retired&&I->Object.Get()==O;}))W.Objects.Add(MoveTemp(Identity));W.R.Data->SetObjectField(FString(Key)+TEXT("_identity"),J);W.R.Data->SetBoolField(FString(Key)+TEXT("_identity_observed"),true);return true;
 }
 bool ClassSource(UClass* C)
 {
  if(Sources.ContainsByPredicate([&](const FFreshClass& S){return S.Class==C;}))return true;
  if(!W.Pin(C)||!C->HasAllClassFlags(CLASS_Native)||C->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||!C->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||C->GetPackage()->IsDirty()||!C->ClassDefaultObject.IsResolved())return Fail(TEXT("pcg_fresh_source_unsupported"),TEXT("Exact loaded native source class/package/resolved CDO required"));
  FFreshClass S;S.Class=C;S.Package=C->GetPackage();S.PackageFlags=S.Package->GetPackageFlags();if(!W.Pin(S.Package))return false;S.Flags=C->GetClassFlags();S.Size=C->GetPropertiesSize();S.CDOHandle=C->ClassDefaultObject;
  for(auto It=C->GetSuperStructIterator();It;++It){UStruct* Super=*It;if(!W.Pin(Super))return false;S.Supers.Add(Super);}
  for(UStruct* Super:S.Supers)for(TFieldIterator<FProperty> It(Super,EFieldIterationFlags::None);It;++It){S.Inventory.Add(*It);S.Fields.Emplace(*It);}
  if(S.CDOHandle!=nullptr)
  {
   UObject* Existing=StaticFindObject(nullptr,C->GetPackage(),*(TEXT("Default__")+C->GetName()),false);
   if(!W.Admit(TEXT("existing_source_cdo_lookup"))||!Live(Existing)||C->ClassDefaultObject!=S.CDOHandle||S.CDOHandle!=Existing||Existing->GetClass()!=C||!Existing->HasAllFlags(RF_ClassDefaultObject)||!W.Pin(Existing))return Fail(TEXT("pcg_fresh_source_cdo_changed"),TEXT("Existing exact source CDO lookup role unavailable; CDO creation not requested"));S.CDO=Existing;S.CDOFlags=Existing->GetFlags();
  }
  if(!S.CDO)return Fail(TEXT("pcg_fresh_source_cdo_unavailable"),TEXT("Constructor source CDO must already exist; this path does not adopt lazy source creation"));Sources.Add(MoveTemp(S));return W.Admit(TEXT("captured_fresh_native_source"));
 }
 TSet<FString> Dirty()const{TSet<FString> Out;for(TObjectIterator<UPackage> It;It;++It)if(It->IsDirty()&&!It->HasAnyPackageFlags(PKG_CompiledIn)&&It->GetName()!=TEXT("/Engine/Transient"))Out.Add(It->GetName());return Out;}
 bool PreservedDirty()
 {
  auto After=Dirty();After.Remove(PackagePath);auto Before=DirtyBefore;Before.Remove(PackagePath);
  if(After.Num()!=Before.Num())return Fail(TEXT("pcg_unrelated_dirty_state_changed"),TEXT("Unrelated dirty package inventory changed"));for(const auto& Name:Before)if(!After.Contains(Name))return Fail(TEXT("pcg_unrelated_dirty_state_changed"),TEXT("An unrelated dirty package changed"));W.R.Data->SetBoolField(TEXT("unrelated_dirty_state_preserved"),true);return true;
 }
 bool Absent(const TCHAR* Stage,bool FailIfPresent=true)
 {
  if(!W.Admit(Stage)||!Registry)return false;const FAssetData Asset=Registry->GetAssetByObjectPath(FSoftObjectPath(ObjectPath));if(!W.Admit(Stage))return false;
  const bool RegistryPresent=Asset.IsValid();const bool FilePresent=IFileManager::Get().FileExists(*File);const bool CachedDirectory=Registry->PathExists(PackagePath);if(!W.Admit(Stage))return false;const bool Directory=CachedDirectory||IFileManager::Get().DirectoryExists(*FPaths::GetBaseFilename(File,false));
  const bool Loaded=FindPackage(nullptr,*PackagePath)!=nullptr||StaticFindObject(nullptr,nullptr,*ObjectPath,false)!=nullptr;
  auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("asset_registered"),RegistryPresent);J->SetBoolField(TEXT("loaded_package_or_object"),Loaded);J->SetBoolField(TEXT("directory_exists"),Directory);J->SetBoolField(TEXT("file_exists"),FilePresent);J->SetStringField(TEXT("file_path"),File);W.R.Data->SetObjectField(Stage,J);
  return (!RegistryPresent&&!FilePresent&&!Directory&&!Loaded)||(!FailIfPresent?false:Fail(Overwrite?TEXT("pcg_existing_target_overwrite_unsupported"):TEXT("pcg_asset_exists"),TEXT("Exact registry/loaded/file/directory target must be fresh; existing data is preserved")));
 }
};

// NE: a request owns this admission, never the global construction-health bit.
// Existing-target current/capture helpers are shared with LS; factory creation,
// absence policy, empty FreshBind and FreshGraphs are not used as approval.
struct FStandaloneField { const TCHAR* Class;const TCHAR* Native;const TCHAR* Alias; };
const FStandaloneField StandaloneFields[]={
 // XZ: data-source factory scalars; selector/sampling aggregates stay incomplete.
 {TEXT("PCGDataFromActorSettings"),TEXT("ActorSelector"),TEXT("actorselector")},
 {TEXT("PCGDataFromActorSettings"),TEXT("ComponentSelector"),TEXT("componentselector")},
 {TEXT("PCGDataFromActorSettings"),TEXT("ExpectedPins"),TEXT("expectedpins")},
 {TEXT("PCGDataFromActorSettings"),TEXT("Mode"),TEXT("mode")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bIgnorePCGGeneratedComponents"),TEXT("ignorepcggeneratedcomponents")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bAlsoOutputSinglePointData"),TEXT("alsooutputsinglepointdata")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bComponentsMustOverlapSelf"),TEXT("componentsmustoverlapself")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bGetDataOnAllGrids"),TEXT("getdataonallgrids")},
 {TEXT("PCGDataFromActorSettings"),TEXT("AllowedGrids"),TEXT("allowedgrids")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bMergeSinglePointData"),TEXT("mergesinglepointdata")},
 {TEXT("PCGDataFromActorSettings"),TEXT("PropertyName"),TEXT("propertyname")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bAlwaysRequeryActors"),TEXT("alwaysrequeryactors")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bSilenceSanitizedAttributeNameWarnings"),TEXT("silencesanitizedattributenamewarnings")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bSilenceReservedAttributeNameWarnings"),TEXT("silencereservedattributenamewarnings")},
 {TEXT("PCGDataFromActorSettings"),TEXT("bTrackActorsOnlyWithinBounds"),TEXT("trackactorsonlywithinbounds")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("ActorSelector"),TEXT("actorselector")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("ComponentSelector"),TEXT("componentselector")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("ExpectedPins"),TEXT("expectedpins")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("Mode"),TEXT("mode")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bIgnorePCGGeneratedComponents"),TEXT("ignorepcggeneratedcomponents")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bAlsoOutputSinglePointData"),TEXT("alsooutputsinglepointdata")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bComponentsMustOverlapSelf"),TEXT("componentsmustoverlapself")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bGetDataOnAllGrids"),TEXT("getdataonallgrids")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("AllowedGrids"),TEXT("allowedgrids")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bMergeSinglePointData"),TEXT("mergesinglepointdata")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("PropertyName"),TEXT("propertyname")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bAlwaysRequeryActors"),TEXT("alwaysrequeryactors")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bSilenceSanitizedAttributeNameWarnings"),TEXT("silencesanitizedattributenamewarnings")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bSilenceReservedAttributeNameWarnings"),TEXT("silencereservedattributenamewarnings")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bTrackActorsOnlyWithinBounds"),TEXT("trackactorsonlywithinbounds")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("bUnbounded"),TEXT("unbounded")},
 {TEXT("PCGGetLandscapeSettings"),TEXT("SamplingProperties"),TEXT("samplingproperties")},
 // QR: own public native fields, with transient editor debug state separate.
 {TEXT("PCGSurfaceSamplerSettings"),TEXT("PointsPerSquaredMeter"),TEXT("pointspersquaredmeter")},
 {TEXT("PCGSurfaceSamplerSettings"),TEXT("PointExtents"),TEXT("pointextents")},
 {TEXT("PCGSurfaceSamplerSettings"),TEXT("Looseness"),TEXT("looseness")},
 {TEXT("PCGSurfaceSamplerSettings"),TEXT("bUnbounded"),TEXT("unbounded")},
 {TEXT("PCGSurfaceSamplerSettings"),TEXT("bApplyDensityToPoints"),TEXT("applydensitytopoints")},
 {TEXT("PCGSurfaceSamplerSettings"),TEXT("PointSteepness"),TEXT("pointsteepness")},
 {TEXT("PCGSurfaceSamplerSettings"),TEXT("bKeepZeroDensityPoints"),TEXT("keepzerodensitypoints")},
 {TEXT("PCGSurfaceSamplerSettings"),TEXT("bUseLegacyGridCreationMethod"),TEXT("uselegacygridcreationmethod")},
 // ADP: graph-spec typed value retention only; raw Add/Set/Connect enrollment unchanged.
 {TEXT("PCGProjectionSettings"),TEXT("ProjectionParams"),TEXT("projectionparams")},{TEXT("PCGProjectionSettings"),TEXT("bForceCollapseToPoint"),TEXT("forcecollapsetopoint")},{TEXT("PCGProjectionSettings"),TEXT("bKeepZeroDensityPoints"),TEXT("keepzerodensitypoints")},
 // ADL: original Add/Set/Connect own spatial-composition fields.
 {TEXT("PCGUnionSettings"),TEXT("Type"),TEXT("type")},{TEXT("PCGUnionSettings"),TEXT("DensityFunction"),TEXT("densityfunction")},
 {TEXT("PCGInnerIntersectionSettings"),TEXT("DensityFunction"),TEXT("densityfunction")},{TEXT("PCGInnerIntersectionSettings"),TEXT("bKeepZeroDensityPoints"),TEXT("keepzerodensitypoints")},
 {TEXT("PCGOuterIntersectionSettings"),TEXT("DensityFunction"),TEXT("densityfunction")},{TEXT("PCGOuterIntersectionSettings"),TEXT("bIgnorePinsWithNoInput"),TEXT("ignorepinswithnoinput")},{TEXT("PCGOuterIntersectionSettings"),TEXT("bKeepZeroDensityPoints"),TEXT("keepzerodensitypoints")},
 {TEXT("PCGDifferenceSettings"),TEXT("DensityFunction"),TEXT("densityfunction")},
 {TEXT("PCGDifferenceSettings"),TEXT("Mode"),TEXT("mode")},
 {TEXT("PCGDifferenceSettings"),TEXT("bDiffMetadata"),TEXT("diffmetadata")},
 {TEXT("PCGDifferenceSettings"),TEXT("bKeepZeroDensityPoints"),TEXT("keepzerodensitypoints")},
 // PA: exact point-bounds authored fields; existing native FName lookup is retained.
 {TEXT("PCGTransformPointsSettings"),TEXT("bApplyToAttribute"),TEXT("applytoattribute")},{TEXT("PCGTransformPointsSettings"),TEXT("AttributeName"),TEXT("attributename")},
 {TEXT("PCGTransformPointsSettings"),TEXT("OffsetMin"),TEXT("offsetmin")},{TEXT("PCGTransformPointsSettings"),TEXT("OffsetMax"),TEXT("offsetmax")},{TEXT("PCGTransformPointsSettings"),TEXT("bAbsoluteOffset"),TEXT("absoluteoffset")},
 {TEXT("PCGTransformPointsSettings"),TEXT("RotationMin"),TEXT("rotationmin")},{TEXT("PCGTransformPointsSettings"),TEXT("RotationMax"),TEXT("rotationmax")},{TEXT("PCGTransformPointsSettings"),TEXT("bAbsoluteRotation"),TEXT("absoluterotation")},
 {TEXT("PCGTransformPointsSettings"),TEXT("ScaleMin"),TEXT("scalemin")},{TEXT("PCGTransformPointsSettings"),TEXT("ScaleMax"),TEXT("scalemax")},{TEXT("PCGTransformPointsSettings"),TEXT("bAbsoluteScale"),TEXT("absolutescale")},
 {TEXT("PCGTransformPointsSettings"),TEXT("bUniformScale"),TEXT("uniformscale")},{TEXT("PCGTransformPointsSettings"),TEXT("bRecomputeSeed"),TEXT("recomputeseed")},
 {TEXT("PCGPointExtentsModifierSettings"),TEXT("Extents"),TEXT("extents")},{TEXT("PCGPointExtentsModifierSettings"),TEXT("Mode"),TEXT("mode")},
 {TEXT("PCGBoundsModifierSettings"),TEXT("Mode"),TEXT("mode")},{TEXT("PCGBoundsModifierSettings"),TEXT("BoundsMin"),TEXT("boundsmin")},
 {TEXT("PCGBoundsModifierSettings"),TEXT("BoundsMax"),TEXT("boundsmax")},{TEXT("PCGBoundsModifierSettings"),TEXT("bAffectSteepness"),TEXT("affectsteepness")},
 {TEXT("PCGBoundsModifierSettings"),TEXT("Steepness"),TEXT("steepness")},
 {TEXT("PCGCreatePointsGridSettings"),TEXT("GridExtents"),TEXT("gridextents")},{TEXT("PCGCreatePointsGridSettings"),TEXT("CellSize"),TEXT("cellsize")},
 {TEXT("PCGCreatePointsGridSettings"),TEXT("CoordinateSpace"),TEXT("coordinatespace")},{TEXT("PCGCreatePointsGridSettings"),TEXT("PointPosition"),TEXT("pointposition")},
 {TEXT("PCGCreatePointsGridSettings"),TEXT("PointSteepness"),TEXT("pointsteepness")},{TEXT("PCGCreatePointsGridSettings"),TEXT("bSetPointsBounds"),TEXT("setpointsbounds")},
 {TEXT("PCGCreatePointsGridSettings"),TEXT("bCullPointsOutsideVolume"),TEXT("cullpointsoutsidevolume")},
 {TEXT("PCGDensityRemapSettings"),TEXT("InRangeMin"),TEXT("rangemin")},{TEXT("PCGDensityRemapSettings"),TEXT("InRangeMax"),TEXT("rangemax")},
 {TEXT("PCGDensityRemapSettings"),TEXT("OutRangeMin"),TEXT("outrangemin")},{TEXT("PCGDensityRemapSettings"),TEXT("OutRangeMax"),TEXT("outrangemax")},
 {TEXT("PCGDensityRemapSettings"),TEXT("bExcludeValuesOutsideInputRange"),TEXT("excludevaluesoutsideinputrange")},
 {TEXT("PCGDistanceSettings"),TEXT("bSetDensity"),TEXT("setdensity")},{TEXT("PCGDistanceSettings"),TEXT("MaximumDistance"),TEXT("maximumdistance")},
 {TEXT("PCGDistanceSettings"),TEXT("bOutputToAttribute"),TEXT("outputtoattribute")},{TEXT("PCGDistanceSettings"),TEXT("bOutputDistanceVector"),TEXT("outputdistancevector")},
 {TEXT("PCGDistanceSettings"),TEXT("SourceShape"),TEXT("sourceshape")},{TEXT("PCGDistanceSettings"),TEXT("TargetShape"),TEXT("targetshape")},
 {TEXT("PCGDistanceSettings"),TEXT("bCheckSourceAgainstRespectiveTarget"),TEXT("checksourceagainstrespectivetarget")},
 {TEXT("PCGRandomChoiceSettings"),TEXT("bFixedMode"),TEXT("fixedmode")},{TEXT("PCGRandomChoiceSettings"),TEXT("FixedNumber"),TEXT("fixednumber")},
 {TEXT("PCGRandomChoiceSettings"),TEXT("bOutputDiscardedEntries"),TEXT("outputdiscardedentries")},{TEXT("PCGRandomChoiceSettings"),TEXT("Seed"),TEXT("seed")},
 {TEXT("PCGDensityFilterSettings"),TEXT("LowerBound"),TEXT("lowerbound")},{TEXT("PCGDensityFilterSettings"),TEXT("UpperBound"),TEXT("upperbound")},
 {TEXT("PCGDensityFilterSettings"),TEXT("bInvertFilter"),TEXT("invertfilter")},{TEXT("PCGDensityFilterSettings"),TEXT("bKeepZeroDensityPoints"),TEXT("keepzerodensitypoints")},
 {TEXT("PCGAttributeNoiseSettings"),TEXT("Mode"),TEXT("mode")},{TEXT("PCGAttributeNoiseSettings"),TEXT("NoiseMin"),TEXT("noisemin")},
 {TEXT("PCGAttributeNoiseSettings"),TEXT("NoiseMax"),TEXT("noisemax")},{TEXT("PCGAttributeNoiseSettings"),TEXT("bClampResult"),TEXT("clampresult")},
 {TEXT("PCGAttributeNoiseSettings"),TEXT("bInvertSource"),TEXT("invertsource")},{TEXT("PCGAttributeNoiseSettings"),TEXT("bHasCustomSeedSource"),TEXT("hascustomseedsource")},
 {TEXT("PCGAttributeFilteringSettings"),TEXT("Operator"),TEXT("operator")},{TEXT("PCGAttributeFilteringSettings"),TEXT("bUseConstantThreshold"),TEXT("useconstantthreshold")},
 {TEXT("PCGAttributeFilteringSettings"),TEXT("bUseSpatialQuery"),TEXT("usespatialquery")},{TEXT("PCGAttributeFilteringSettings"),TEXT("bWarnOnDataMissingAttribute"),TEXT("warnondatamissingattribute")},
 {TEXT("PCGCreatePointsSettings"),TEXT("CoordinateSpace"),TEXT("coordinatespace")},{TEXT("PCGCreatePointsSettings"),TEXT("PointsToCreate"),TEXT("pointstocreate")},{TEXT("PCGCreatePointsSettings"),TEXT("bCullPointsOutsideVolume"),TEXT("cullpointsoutsidevolume")},
 {TEXT("PCGGetActorPropertySettings"),TEXT("PropertyName"),TEXT("propertyname")},{TEXT("PCGGetActorPropertySettings"),TEXT("bAlwaysRequeryActors"),TEXT("alwaysrequeryactors")}};
bool StandaloneSpawnerClass(UClass* C){return C&&C==UPCGStaticMeshSpawnerSettings::StaticClass();}
// OH: exact native fields only; legacy native property FName matching is kept.
// Python editor_property snake names in old practical scripts are source mapping,
// not a newly invented loose-alias contract on add_pcg_node/set_pcg_node_setting.
const FStandaloneField StandaloneSpawnerFields[]={
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("Seed"),TEXT("seed")},
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("bEnabled"),TEXT("enabled")},
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("bAllowDescriptorChanges"),TEXT("allowdescriptorchanges")},
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("bApplyMeshBoundsToPoints"),TEXT("applymeshboundstopoints")},
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("bSynchronousLoad"),TEXT("synchronousload")},
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("bAllowMergeDifferentDataInSameInstancedComponents"),TEXT("allowmergedifferentdatainsameinstancedcomponents")},
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("bSilenceOverrideAttributeNotFoundErrors"),TEXT("silenceoverrideattributenotfounderrors")},
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("bWarnOnIdenticalSpawn"),TEXT("warnonidenticalspawn")},
 {TEXT("PCGStaticMeshSpawnerSettings"),TEXT("OutAttributeName"),TEXT("outattributename")}};
const FStandaloneField* StandaloneSpawnerField(FName Name)
{for(const auto& Field:StandaloneSpawnerFields)if(Name==Field.Native)return &Field;return nullptr;}
bool StandaloneSpawnerProperty(FProperty* Property)
{
 if(!Property||!StandaloneSpawnerField(Property->GetFName())||Property->ArrayDim!=1||Property->HasSetterOrGetter()||!Property->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||Property->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient))return false;
 const FName Name=Property->GetFName();
 if(Name==TEXT("Seed"))return Property->GetOwnerStruct()==UPCGSettings::StaticClass()&&Property->GetClass()==FIntProperty::StaticClass()&&Property->GetSize()==sizeof(int32);
 if(Name==TEXT("bEnabled")){const auto* Bool=CastField<FBoolProperty>(Property);return Property->GetOwnerStruct()==UPCGSettingsInterface::StaticClass()&&Bool&&Bool->IsNativeBool()&&Property->GetSize()==sizeof(bool);}
 if(Property->GetOwnerStruct()!=UPCGStaticMeshSpawnerSettings::StaticClass())return false;
 if(Name==TEXT("OutAttributeName"))return Property->GetClass()==FNameProperty::StaticClass()&&Property->GetSize()==sizeof(FName);
 const auto* Bool=CastField<FBoolProperty>(Property);return Bool&&Bool->IsNativeBool()&&Property->GetSize()==sizeof(bool);
}
// XZ: exact native actor/landscape data-source factories. No actor query or PCG execution.
bool FreshDataSourceClass(UClass* C)
{return C&&(C==UPCGDataFromActorSettings::StaticClass()||C==UPCGGetLandscapeSettings::StaticClass());}
bool FreshDataSourceProperty(UClass* C,FProperty* P)
{
 if(!FreshDataSourceClass(C)||!P||P->ArrayDim!=1||P->HasSetterOrGetter()||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient))return false;
 const FName N=P->GetFName();
 if(C==UPCGGetLandscapeSettings::StaticClass()&&N==TEXT("bUnbounded")){const auto* B=CastField<FBoolProperty>(P);return P->GetOwnerStruct()==C&&B&&B->IsNativeBool()&&P->GetSize()==sizeof(bool);}
 if(C==UPCGGetLandscapeSettings::StaticClass()&&N==TEXT("SamplingProperties")){const auto* S=CastField<FStructProperty>(P);return P->GetOwnerStruct()==C&&S&&S->Struct.IsResolved()&&S->Struct==FPCGLandscapeDataProps::StaticStruct()&&P->GetSize()==sizeof(FPCGLandscapeDataProps);}
 if(P->GetOwnerStruct()!=UPCGDataFromActorSettings::StaticClass())return false;
 if(N==TEXT("ActorSelector")||N==TEXT("ComponentSelector")){const auto* S=CastField<FStructProperty>(P);UScriptStruct* T=N==TEXT("ActorSelector")?FPCGActorSelectorSettings::StaticStruct():FPCGComponentSelectorSettings::StaticStruct();return S&&S->Struct.IsResolved()&&S->Struct==T&&P->GetSize()==(N==TEXT("ActorSelector")?sizeof(FPCGActorSelectorSettings):sizeof(FPCGComponentSelectorSettings));}
 if(N==TEXT("ExpectedPins")){const auto* A=CastField<FArrayProperty>(P);return A&&A->Inner&&A->Inner->GetClass()==FNameProperty::StaticClass()&&A->Inner->GetOwner<FProperty>()==P&&A->Inner->ArrayDim==1&&A->Inner->GetSize()==sizeof(FName)&&!A->Inner->HasSetterOrGetter()&&P->GetSize()==sizeof(TArray<FName>);}
 if(N==TEXT("AllowedGrids"))return P->GetClass()==FIntProperty::StaticClass()&&P->GetSize()==sizeof(int32);
 if(N==TEXT("PropertyName"))return P->GetClass()==FNameProperty::StaticClass()&&P->GetSize()==sizeof(FName);
 if(N==TEXT("Mode")){const auto* E=CastField<FEnumProperty>(P);auto* U=E?E->GetUnderlyingProperty():nullptr;return E&&U&&U->GetOwner<FProperty>()==P&&U->GetClass()==FByteProperty::StaticClass()&&U->ArrayDim==1&&U->GetSize()==sizeof(uint8)&&P->GetSize()==sizeof(uint8);}
 const auto* B=CastField<FBoolProperty>(P);return B&&B->IsNativeBool()&&P->GetSize()==sizeof(bool);
}
bool FreshTransformPointsClass(UClass* C){return C&&Same(C->GetPathName(),TEXT("/Script/PCG.PCGTransformPointsSettings"));}
bool StandalonePointBoundsClass(UClass* C)
{return C&&(Same(C->GetPathName(),TEXT("/Script/PCG.PCGPointExtentsModifierSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGBoundsModifierSettings")));}
// ADL SDK PCGUnionElement.h/InnerIntersectionElement.h/OuterIntersectionElement.h.
// Exact native classes only; generic source ownership/layout/current remain shared.
bool StandaloneSpatialCompositionClass(UClass* C)
{return C&&(Same(C->GetPathName(),TEXT("/Script/PCG.PCGUnionSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGInnerIntersectionSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGOuterIntersectionSettings")));}
bool FreshProjectionClass(UClass* C)
{return C&&Same(C->GetPathName(),TEXT("/Script/PCG.PCGProjectionSettings"));}
bool SameProjectionAuthored(const FPCGProjectionParams& A,const FPCGProjectionParams& B)
{return A.bProjectPositions==B.bProjectPositions&&A.bProjectRotations==B.bProjectRotations&&A.bProjectScales==B.bProjectScales&&A.ColorBlendMode==B.ColorBlendMode&&A.AttributeList.Equals(B.AttributeList,ESearchCase::CaseSensitive)&&A.AttributeMode==B.AttributeMode&&A.AttributeMergeOperation==B.AttributeMergeOperation&&A.TagMergeOperation==B.TagMergeOperation;}
bool StandaloneSpatialDynamicClass(UClass* C)
{return StandaloneSpatialCompositionClass(C)&&C->GetFName()!=TEXT("PCGInnerIntersectionSettings");}
bool StandaloneSpatialProperty(UClass* C,FProperty* P,UEnum* Enum,FNumericProperty* Storage)
{
 if(!StandaloneSpatialCompositionClass(C)||!P||P->GetOwnerStruct()!=C||P->ArrayDim!=1||P->HasSetterOrGetter()||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient)||!P->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible))return false;
 const FName N=P->GetFName();const bool Union=C->GetFName()==TEXT("PCGUnionSettings"),Outer=C->GetFName()==TEXT("PCGOuterIntersectionSettings");
 if((!Union&&N==TEXT("bKeepZeroDensityPoints"))||(Outer&&N==TEXT("bIgnorePinsWithNoInput"))){const auto* B=CastField<FBoolProperty>(P);return B&&B->IsNativeBool()&&P->GetSize()==sizeof(bool);}
 const bool IsType=Union&&N==TEXT("Type");const auto* E=CastField<FEnumProperty>(P);
 const TCHAR* EnumPath=IsType?TEXT("/Script/PCG.EPCGUnionType"):Union?TEXT("/Script/PCG.EPCGUnionDensityFunction"):TEXT("/Script/PCG.EPCGIntersectionDensityFunction");
 return (IsType||N==TEXT("DensityFunction"))&&E&&E->GetUnderlyingProperty()==Storage&&Enum&&Same(Enum->GetPathName(),EnumPath)&&Storage&&Storage->GetOwner<FProperty>()==P&&Storage->GetClass()==FByteProperty::StaticClass()&&Storage->ArrayDim==1&&Storage->GetSize()==sizeof(uint8)&&P->GetSize()==sizeof(uint8);
}
bool StandaloneSamplingDifferenceClass(UClass* C)
{return C&&(C==UPCGSurfaceSamplerSettings::StaticClass()||C==UPCGDifferenceSettings::StaticClass());}
bool StandaloneSamplingTransient(UClass* C,FProperty* P)
{return C==UPCGSurfaceSamplerSettings::StaticClass()&&P&&P->GetOwnerStruct()==C&&P->GetFName()==TEXT("bKeepZeroDensityPoints");}
bool StandaloneSamplingProperty(UClass* C,FProperty* P,UScriptStruct* Vector,UEnum* Enum,FNumericProperty* Storage)
{
 if(!StandaloneSamplingDifferenceClass(C)||!P||P->GetOwnerStruct()!=C||P->ArrayDim!=1||P->HasSetterOrGetter()||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated)||P->HasAnyPropertyFlags(CPF_Transient)!=StandaloneSamplingTransient(C,P))return false;
 const FName N=P->GetFName();const bool Sampler=C==UPCGSurfaceSamplerSettings::StaticClass();
 if(Sampler&&(N==TEXT("PointsPerSquaredMeter")||N==TEXT("Looseness")||N==TEXT("PointSteepness")))return P->GetClass()==FFloatProperty::StaticClass()&&P->GetSize()==sizeof(float);
 if(Sampler&&N==TEXT("PointExtents")){const auto* V=CastField<FStructProperty>(P);return V&&V->Struct.IsResolved()&&V->Struct==Vector&&Vector==TBaseStructure<FVector>::Get()&&Vector->GetStructureSize()==sizeof(FVector)&&P->GetSize()==sizeof(FVector);}
 if((Sampler&&(N==TEXT("bUnbounded")||N==TEXT("bApplyDensityToPoints")||N==TEXT("bKeepZeroDensityPoints")||N==TEXT("bUseLegacyGridCreationMethod")))||(!Sampler&&(N==TEXT("bDiffMetadata")||N==TEXT("bKeepZeroDensityPoints")))){const auto* B=CastField<FBoolProperty>(P);return B&&B->IsNativeBool()&&P->GetSize()==sizeof(bool);}
 const auto* E=CastField<FEnumProperty>(P);return !Sampler&&(N==TEXT("DensityFunction")||N==TEXT("Mode"))&&E&&E->GetUnderlyingProperty()==Storage&&Enum&&Same(Enum->GetPathName(),N==TEXT("DensityFunction")?TEXT("/Script/PCG.EPCGDifferenceDensityFunction"):TEXT("/Script/PCG.EPCGDifferenceMode"))&&Storage&&Storage->GetOwner<FProperty>()==P&&Storage->GetClass()==FByteProperty::StaticClass()&&Storage->ArrayDim==1&&Storage->GetSize()==sizeof(uint8)&&P->GetSize()==sizeof(uint8);
}
// QV: exact CopyAttributes/Rename semantics, no POD comparison of selector FString/TArray storage.
bool StandaloneMetadataClass(UClass* C){return C==UPCGCopyAttributesSettings::StaticClass()||C==UPCGMetadataRenameSettings::StaticClass();}
struct FStandaloneMetadataCell
{
 FProperty* Property=nullptr;FName Name;FPCGAttributePropertySelector Selector;FName NameValue;bool Bool=false;int64 Integer=0;bool IsSelector=false;TMap<FName,FName> Domains;
};
// AG back half: raw Add/Set retain the original native Name/Name map ImportText contract.
// Compare authored keys/values without depending on map bucket or insertion order.
bool MetadataDomainsSame(const TMap<FName,FName>& A,const TMap<FName,FName>& B)
{
 if(A.Num()!=B.Num())return false;
 for(const auto& E:A){bool Found=false;for(const auto& V:B)if(E.Key.IsEqual(V.Key,ENameCase::CaseSensitive)&&E.Value.IsEqual(V.Value,ENameCase::CaseSensitive)){Found=true;break;}if(!Found)return false;}return true;
}
// FNameProperty imports delimited map tokens by constructing FName directly.
// Bound raw token spans before that constructor; the native parser still owns syntax/escaping.
bool MetadataDomainsTextBounded(const FString& Text)
{
 if(Text.Len()>16384||!NoEmbeddedNul(Text)||Forbidden(Text))return false;
 int32 Span=0;bool Quoted=false,Escaped=false;
 for(TCHAR C:Text){if(Quoted){if(++Span>=NAME_SIZE)return false;if(Escaped)Escaped=false;else if(C==TEXT('\\'))Escaped=true;else if(C==TEXT('"')){Quoted=false;Span=0;}}else if(C==TEXT('"')){Quoted=true;Span=0;}else if(C==TEXT('(')||C==TEXT(')')||C==TEXT(',')||FChar::IsWhitespace(C))Span=0;else if(++Span>=NAME_SIZE)return false;}
 return !Quoted&&!Escaped;
}
TSharedPtr<FJsonValue> MetadataDomainsJson(const TMap<FName,FName>& V)
{
 TArray<FName> Keys;V.GetKeys(Keys);Keys.Sort([](FName A,FName B){return A.ToString().Compare(B.ToString(),ESearchCase::CaseSensitive)<0;});TArray<TSharedPtr<FJsonValue>> Rows;
 for(FName K:Keys){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("key"),K.ToString());Row->SetStringField(TEXT("value"),V.FindChecked(K).ToString());Rows.Add(MakeShared<FJsonValueObject>(Row));}return MakeShared<FJsonValueArray>(Rows);
}
bool MetadataSelectorSame(const FPCGAttributePropertySelector& A,const FPCGAttributePropertySelector& B)
{
 if(A.GetSelection()!=B.GetSelection()||!A.GetDomainName().IsEqual(B.GetDomainName(),ENameCase::CaseSensitive)||!A.GetAttributeName().IsEqual(B.GetAttributeName(),ENameCase::CaseSensitive)||!A.GetPropertyName().IsEqual(B.GetPropertyName(),ENameCase::CaseSensitive)||A.GetExtraProperty()!=B.GetExtraProperty()||A.GetExtraNames().Num()!=B.GetExtraNames().Num())return false;
 for(int32 I=0;I<A.GetExtraNames().Num();++I)if(!Same(A.GetExtraNames()[I],B.GetExtraNames()[I]))return false;return true;
}
TSharedRef<FJsonObject> MetadataSelectorJson(const FPCGAttributePropertySelector& S)
{
 auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("selection"),static_cast<int32>(S.GetSelection()));J->SetStringField(TEXT("domain_name"),S.GetDomainName().ToString());J->SetStringField(TEXT("attribute_name"),S.GetAttributeName().ToString());J->SetStringField(TEXT("property_name"),S.GetPropertyName().ToString());J->SetNumberField(TEXT("extra_property"),static_cast<int32>(S.GetExtraProperty()));TArray<TSharedPtr<FJsonValue>> Extra;for(const auto& E:S.GetExtraNames())Extra.Add(MakeShared<FJsonValueString>(E));J->SetArrayField(TEXT("extra_names"),Extra);return J;
}
struct FStandaloneSelectorSource
{
 UScriptStruct* Struct=nullptr;UStruct* Super=nullptr;EStructFlags Flags=STRUCT_NoFlags;int32 Size=0,Align=0;TArray<FProperty*> Inventory;TArray<FFreshField> Fields;
 struct FArray{FArrayProperty* Property=nullptr;FProperty* Inner=nullptr;FFreshField Field;FArray(FArrayProperty* P):Property(P),Inner(P->Inner),Field(P->Inner){}};TArray<FArray> Arrays;
 struct FEnum{FEnumProperty* Property=nullptr;FNumericProperty* Storage=nullptr;FFreshField Field;TStrongObjectPtr<UEnum> Enum;EObjectFlags Flags=RF_NoFlags;TArray<TPair<FName,int64>> Names;FEnum(FEnumProperty* P,FNumericProperty* S,UEnum* E):Property(P),Storage(S),Field(S),Enum(E),Flags(E?E->GetFlags():RF_NoFlags){if(E)for(int32 I=0;I<E->NumEnums();++I)Names.Emplace(E->GetNameByIndex(I),E->GetValueByIndex(I));}};TArray<FEnum> Enums;
 bool Current()const
 {
  if(!Live(Struct)||Struct->GetSuperStruct()!=Super||Struct->StructFlags!=Flags||Struct->GetStructureSize()!=Size||Struct->GetMinAlignment()!=Align)return false;TArray<FProperty*> Actual;for(TFieldIterator<FProperty> It(Struct,EFieldIterationFlags::IncludeDeprecated);It;++It)Actual.Add(*It);if(Actual!=Inventory)return false;
  for(const auto& F:Fields)if(!F.Exact()||F.P->HasSetterOrGetter())return false;
  for(const auto& A:Arrays){if(A.Property->Inner!=A.Inner)return false;if(!A.Field.Exact()||A.Inner->GetOwner<FProperty>()!=A.Property)return false;}
  for(const auto& E:Enums){if(E.Property->GetUnderlyingProperty()!=E.Storage||!E.Field.Exact()||E.Storage->GetOwner<FProperty>()!=E.Property)return false;if(!E.Enum.IsValid())continue;if(!Live(E.Enum.Get())||E.Enum->GetFlags()!=E.Flags||E.Enum->NumEnums()!=E.Names.Num())return false;for(int32 I=0;I<E.Names.Num();++I)if(E.Enum->GetNameByIndex(I)!=E.Names[I].Key||E.Enum->GetValueByIndex(I)!=E.Names[I].Value)return false;}return true;
 }
};
struct FStandaloneMetadata
{
 UPCGSettings* Settings=nullptr;bool Copy=false;TArray<FFreshField> Fields;FPCGAttributePropertySelector Input,Output,Rename;FName NewName;EPCGCopyAttributesOperation Operation=EPCGCopyAttributesOperation::CopyEachSourceToEachTargetRespectively;bool All=false,AllDomains=false;TMap<FName,FName> Domains;
 bool Enabled=true,Debug=false;int32 Seed=0;UEnum* OperationEnum=nullptr;FNumericProperty* OperationStorage=nullptr;FMapProperty* Map=nullptr;FProperty* MapKey=nullptr;FProperty* MapValue=nullptr;TArray<FFreshField> MapFields;
 bool Read(FStandaloneMetadataCell& C)const
 {
  if(Copy){auto* S=static_cast<UPCGCopyAttributesSettings*>(Settings);if(C.Name==TEXT("InputSource")){C.IsSelector=true;C.Selector=S->InputSource;}else if(C.Name==TEXT("OutputTarget")){C.IsSelector=true;C.Selector=S->OutputTarget;}else if(C.Name==TEXT("Operation"))C.Integer=static_cast<int32>(S->Operation);else if(C.Name==TEXT("bCopyAllAttributes"))C.Bool=S->bCopyAllAttributes;else if(C.Name==TEXT("bCopyAllDomains"))C.Bool=S->bCopyAllDomains;else if(C.Name==TEXT("MetadataDomainsMapping"))C.Domains=S->MetadataDomainsMapping;else return false;}
  else{auto* S=static_cast<UPCGMetadataRenameSettings*>(Settings);if(C.Name==TEXT("AttributeToRename")){C.IsSelector=true;C.Selector=S->AttributeToRename;}else if(C.Name==TEXT("NewAttributeName"))C.NameValue=S->NewAttributeName;else return false;}return true;
 }
 bool Stored(FStandaloneMetadataCell& C)const
 {
  if(Copy){if(C.Name==TEXT("InputSource")){C.IsSelector=true;C.Selector=Input;}else if(C.Name==TEXT("OutputTarget")){C.IsSelector=true;C.Selector=Output;}else if(C.Name==TEXT("Operation"))C.Integer=static_cast<int32>(Operation);else if(C.Name==TEXT("bCopyAllAttributes"))C.Bool=All;else if(C.Name==TEXT("bCopyAllDomains"))C.Bool=AllDomains;else if(C.Name==TEXT("MetadataDomainsMapping"))C.Domains=Domains;else return false;}
  else{if(C.Name==TEXT("AttributeToRename")){C.IsSelector=true;C.Selector=Rename;}else if(C.Name==TEXT("NewAttributeName"))C.NameValue=NewName;else return false;}return true;
 }
 bool SameValue(FName Mutable=NAME_None)const
 {
  if(Settings->bEnabled!=Enabled||Settings->bDebug!=Debug||Settings->Seed!=Seed)return false;
  if(Copy){const auto* S=static_cast<const UPCGCopyAttributesSettings*>(Settings);if((Mutable!=TEXT("InputSource")&&!MetadataSelectorSame(S->InputSource,Input))||(Mutable!=TEXT("OutputTarget")&&!MetadataSelectorSame(S->OutputTarget,Output))||(Mutable!=TEXT("Operation")&&S->Operation!=Operation)||(Mutable!=TEXT("bCopyAllAttributes")&&S->bCopyAllAttributes!=All)||(Mutable!=TEXT("bCopyAllDomains")&&S->bCopyAllDomains!=AllDomains)||(Mutable!=TEXT("MetadataDomainsMapping")&&!MetadataDomainsSame(S->MetadataDomainsMapping,Domains)))return false;}
  else{const auto* S=static_cast<const UPCGMetadataRenameSettings*>(Settings);if((Mutable!=TEXT("AttributeToRename")&&!MetadataSelectorSame(S->AttributeToRename,Rename))||(Mutable!=TEXT("NewAttributeName")&&!S->NewAttributeName.IsEqual(NewName,ENameCase::CaseSensitive)))return false;}return true;
 }
 void Capture()
 {
  Enabled=Settings->bEnabled;Debug=Settings->bDebug;Seed=Settings->Seed;
  if(Copy){auto* S=static_cast<UPCGCopyAttributesSettings*>(Settings);Input=S->InputSource;Output=S->OutputTarget;Operation=S->Operation;All=S->bCopyAllAttributes;AllDomains=S->bCopyAllDomains;Domains=S->MetadataDomainsMapping;}else{auto* S=static_cast<UPCGMetadataRenameSettings*>(Settings);Rename=S->AttributeToRename;NewName=S->NewAttributeName;}
 }
 bool Accept(const FStandaloneMetadataCell& C)
 {
  if(Copy){if(C.Name==TEXT("InputSource"))Input=C.Selector;else if(C.Name==TEXT("OutputTarget"))Output=C.Selector;else if(C.Name==TEXT("Operation"))Operation=static_cast<EPCGCopyAttributesOperation>(C.Integer);else if(C.Name==TEXT("bCopyAllAttributes"))All=C.Bool;else if(C.Name==TEXT("bCopyAllDomains"))AllDomains=C.Bool;else if(C.Name==TEXT("MetadataDomainsMapping"))Domains=C.Domains;else return false;}
  else{if(C.Name==TEXT("AttributeToRename"))Rename=C.Selector;else if(C.Name==TEXT("NewAttributeName"))NewName=C.NameValue;else return false;}return true;
 }
};

bool MetadataCellSame(const FStandaloneMetadataCell& A,const FStandaloneMetadataCell& B)
{if(A.Name!=B.Name||A.IsSelector!=B.IsSelector)return false;if(A.Name==TEXT("MetadataDomainsMapping"))return MetadataDomainsSame(A.Domains,B.Domains);if(A.IsSelector)return MetadataSelectorSame(A.Selector,B.Selector);if(A.Name==TEXT("NewAttributeName"))return A.NameValue.IsEqual(B.NameValue,ENameCase::CaseSensitive);if(A.Name==TEXT("Operation"))return A.Integer==B.Integer;return A.Bool==B.Bool;}
TSharedPtr<FJsonValue> MetadataCellJson(const FStandaloneMetadataCell& C)
{if(C.Name==TEXT("MetadataDomainsMapping"))return MetadataDomainsJson(C.Domains);if(C.IsSelector)return MakeShared<FJsonValueObject>(MetadataSelectorJson(C.Selector));if(C.Name==TEXT("NewAttributeName"))return MakeShared<FJsonValueString>(C.NameValue.ToString());if(C.Name==TEXT("Operation"))return MakeShared<FJsonValueNumber>(C.Integer);return MakeShared<FJsonValueBoolean>(C.Bool);}

bool StandaloneClass(UClass* C)
{return C&&(Same(C->GetPathName(),TEXT("/Script/PCG.PCGCreatePointsGridSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGDensityRemapSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGDistanceSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGRandomChoiceSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGDensityFilterSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGAttributeNoiseSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGAttributeFilteringSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGCreatePointsSettings"))||Same(C->GetPathName(),TEXT("/Script/PCG.PCGGetActorPropertySettings"))||StandaloneSpawnerClass(C)||StandalonePointBoundsClass(C)||StandaloneSamplingDifferenceClass(C)||StandaloneSpatialCompositionClass(C)||FreshDataSourceClass(C)||StandaloneMetadataClass(C)||C==UPCGSubgraphSettings::StaticClass());}
const FStandaloneField* StandaloneField(UClass* C,FName Name)
{if(StandaloneSpawnerClass(C))return StandaloneSpawnerField(Name);for(const auto& F:StandaloneFields)if(C->GetFName()==F.Class&&Name==F.Native)return &F;return nullptr;}
// Point values use native components, never array headers or struct padding.
struct FStandalonePointLayout
{
 UScriptStruct* Struct=nullptr;UPackage* Package=nullptr;FString Path;EObjectFlags ObjectFlags=RF_NoFlags;EStructFlags Flags=STRUCT_NoFlags;uint32 PackageFlags=0;int32 Size=0,Alignment=0;
 TArray<FProperty*> Inventory;TArray<FFreshField> Fields;TArray<TPair<FStructProperty*,TObjectPtr<UScriptStruct>>> Nested;
 bool Exact()const
 {
  if(!Live(Struct)||Struct->GetPackage()!=Package||!Live(Package)||Package->IsDirty()||Package->GetPackageFlags()!=PackageFlags||Struct->GetFlags()!=ObjectFlags||Struct->StructFlags!=Flags||Struct->GetSuperStruct()!=nullptr||Struct->GetStructureSize()!=Size||Struct->GetMinAlignment()!=Alignment||!Same(Struct->GetPathName(),Path))return false;
  TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(Struct);It;++It)Now.Add(*It);if(Now!=Inventory)return false;
  for(const auto& F:Fields)if(F.P->HasSetterOrGetter()||!F.Exact())return false;for(const auto& N:Nested)if(!N.Key->Struct.IsResolved()||N.Key->Struct!=N.Value)return false;return true;
 }
};
// Capture the complete existing native descriptor/enum roles before graph construction.
// Projection has no selector/array Inner. The deprecated color member is layout
// evidence only; its value is not part of the eight authored preservation fields.
bool FreshProjectionSource(FFreshRequest& F)
{
 auto& W=F.W;auto* T=Cast<UScriptStruct>(StaticFindObject(nullptr,nullptr,TEXT("/Script/PCG.PCGProjectionParams"),false));
 TStrongObjectPtr<UScriptStruct> LocalStruct(Safe()&&Live(T)?T:nullptr);
 if(!LocalStruct.IsValid()||!W.Admit(TEXT("projection_struct_lookup_return"))||!Live(T)||!W.Pin(T)||!W.Pin(T->GetPackage())||T!=FPCGProjectionParams::StaticStruct()||T->GetStructureSize()!=sizeof(FPCGProjectionParams)||T->GetMinAlignment()!=alignof(FPCGProjectionParams)||!(T->StructFlags&STRUCT_Native)||T->GetSuperStruct()!=nullptr||T->GetPackage()->IsDirty()||!T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return F.Fail(TEXT("pcg_projection_source_pending"),TEXT("Already-existing exact native ProjectionParams source/layout required; no source construction."));
 auto Source=MakeShared<FStandaloneSelectorSource>();Source->Struct=T;Source->Super=T->GetSuperStruct();Source->Flags=T->StructFlags;Source->Size=T->GetStructureSize();Source->Align=T->GetMinAlignment();
 for(TFieldIterator<FProperty> It(T,EFieldIterationFlags::IncludeDeprecated);It;++It){Source->Inventory.Add(*It);Source->Fields.Emplace(*It);if(It->HasSetterOrGetter())return false;}
 const TCHAR* Names[]={TEXT("bProjectPositions"),TEXT("bProjectRotations"),TEXT("bProjectScales"),TEXT("AttributeList"),TEXT("ColorBlendMode"),TEXT("AttributeMode"),TEXT("AttributeMergeOperation"),TEXT("TagMergeOperation")};
 for(int32 I=0;I<8;++I)
 {
  FProperty* P=FindFProperty<FProperty>(T,Names[I]);if(!P||P->GetOwnerStruct()!=T||P->ArrayDim!=1||!P->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Transient|CPF_Deprecated))return false;
  if(I<3){auto* B=CastField<FBoolProperty>(P);if(!B||!B->IsNativeBool()||P->GetSize()!=sizeof(bool))return false;continue;}
  if(I==3){if(P->GetClass()!=FStrProperty::StaticClass()||P->GetSize()!=sizeof(FString))return false;continue;}
  auto* E=CastField<FEnumProperty>(P);auto* U=E?E->GetUnderlyingProperty():nullptr;if(!E||!U||U->GetOwner<FProperty>()!=P||U->GetClass()!=FByteProperty::StaticClass()||U->ArrayDim!=1||U->GetSize()!=sizeof(uint8)||P->GetSize()!=sizeof(uint8))return false;
  const TCHAR* Path=I==4?TEXT("/Script/PCG.EPCGProjectionColorBlendMode"):I==5?TEXT("/Script/PCG.EPCGMetadataFilterMode"):I==6?TEXT("/Script/PCG.EPCGMetadataOp"):TEXT("/Script/PCG.EPCGProjectionTagMergeMode");
  auto* Enum=Cast<UEnum>(StaticFindObject(nullptr,nullptr,Path,false));TStrongObjectPtr<UEnum> LocalEnum(Safe()&&Live(Enum)?Enum:nullptr);if(!LocalEnum.IsValid()||!W.Admit(TEXT("projection_enum_lookup_return"))||!Source->Current()||!Live(Enum)||!W.Pin(Enum)||!W.Pin(Enum->GetPackage())||!Same(Enum->GetPathName(),Path))return false;
  Source->Enums.Emplace(E,U,Enum);
 }
 const auto Package=T->GetPackage();const auto PackageFlags=Package->GetPackageFlags();const auto ObjectFlags=T->GetFlags();
 F.ExternalAssetChecks.Add([Source,Package,PackageFlags,ObjectFlags]()
 {
  if(!Source->Current()||!Live(Package)||Source->Struct->GetPackage()!=Package||Package->IsDirty()||Package->GetPackageFlags()!=PackageFlags||Source->Struct->GetFlags()!=ObjectFlags)return false;
  for(const auto& E:Source->Enums){UEnum* Actual=E.Property->GetEnum();if(!Source->Current()||Actual!=E.Enum.Get())return false;}
  return Source->Current();
 });
 return W.Admit(TEXT("projection_source_bound_before_factory"));
}
bool SamePointValue(const FPCGPoint& A,const FPCGPoint& B)
{
 const auto AT=A.Transform.GetTranslation(),BT=B.Transform.GetTranslation(),AS=A.Transform.GetScale3D(),BS=B.Transform.GetScale3D();const auto AQ=A.Transform.GetRotation(),BQ=B.Transform.GetRotation();
 return AT.X==BT.X&&AT.Y==BT.Y&&AT.Z==BT.Z&&AS.X==BS.X&&AS.Y==BS.Y&&AS.Z==BS.Z&&AQ.X==BQ.X&&AQ.Y==BQ.Y&&AQ.Z==BQ.Z&&AQ.W==BQ.W&&A.Density==B.Density&&A.BoundsMin.X==B.BoundsMin.X&&A.BoundsMin.Y==B.BoundsMin.Y&&A.BoundsMin.Z==B.BoundsMin.Z&&A.BoundsMax.X==B.BoundsMax.X&&A.BoundsMax.Y==B.BoundsMax.Y&&A.BoundsMax.Z==B.BoundsMax.Z&&A.Color.X==B.Color.X&&A.Color.Y==B.Color.Y&&A.Color.Z==B.Color.Z&&A.Color.W==B.Color.W&&A.Steepness==B.Steepness&&A.Seed==B.Seed&&A.MetadataEntry==B.MetadataEntry;
}
// YV: public Add/Set retain the original native-text aggregate contract.
// Reflected authored fields are compared semantically; mutable wildcard/prepared
// caches and non-UPROPERTY ComponentList are deliberately not captured as values.
bool DataSourceAggregateName(FName N)
{return N==TEXT("ActorSelector")||N==TEXT("ComponentSelector")||N==TEXT("ExpectedPins");}
const FStandaloneField* GraphSpecDataSourceAggregateField(UClass* Class,const FString& Name)
{if(!FreshDataSourceClass(Class)||Name.Len()>=NAME_SIZE||!NoEmbeddedNul(Name))return nullptr;const auto* Field=StandaloneField(Class,FName(*Name.Replace(TEXT("_"),TEXT(""))));return Field&&DataSourceAggregateName(FName(Field->Native))?Field:nullptr;}
bool DataSourceNamesSame(const TArray<FName>& A,const TArray<FName>& B)
{if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(!A[I].IsEqual(B[I],ENameCase::CaseSensitive))return false;return true;}
struct FDataSourceAggregateValue
{
 FName Field;FPCGActorSelectorSettings Actor;FPCGComponentSelectorSettings Component;TArray<FName> Pins;TObjectPtr<UClass> FilterClass;
 bool Read(UPCGSettings* S,FProperty* P)
 {
  if(!FreshDataSourceClass(S->GetClass())||!FreshDataSourceProperty(S->GetClass(),P)||!DataSourceAggregateName(P->GetFName()))return false;Field=P->GetFName();
  if(Field==TEXT("ActorSelector")){
   const auto& A=*P->ContainerPtrToValuePtr<FPCGActorSelectorSettings>(S);if(A.ActorReferenceSelector.GetExtraNames().Num()>4096)return false;
   Actor.ActorFilter=A.ActorFilter;Actor.bMustOverlapSelf=A.bMustOverlapSelf;Actor.bIncludeChildren=A.bIncludeChildren;Actor.IncludeIsolatedActors=A.IncludeIsolatedActors;Actor.bDisableFilter=A.bDisableFilter;Actor.ActorSelection=A.ActorSelection;Actor.ActorSelectionTag=A.ActorSelectionTag;Actor.ActorSelectionClass=A.ActorSelectionClass;Actor.ActorReferenceSelector=A.ActorReferenceSelector;Actor.bSelectMultiple=A.bSelectMultiple;Actor.bIgnoreSelfAndChildren=A.bIgnoreSelfAndChildren;Actor.bShowActorFilter=A.bShowActorFilter;Actor.bShowIncludeChildren=A.bShowIncludeChildren;Actor.bShowActorSelection=A.bShowActorSelection;Actor.bShowActorSelectionClass=A.bShowActorSelectionClass;Actor.bShowSelectMultiple=A.bShowSelectMultiple;Actor.bShowIgnoreSelfAndChildren=A.bShowIgnoreSelfAndChildren;
  }else if(Field==TEXT("ComponentSelector")){
   const auto& C=*P->ContainerPtrToValuePtr<FPCGComponentSelectorSettings>(S);Component.ComponentSelection=C.ComponentSelection;Component.ComponentSelectionTag=C.ComponentSelectionTag;Component.ComponentSelectionClass=C.ComponentSelectionClass;Component.bShowComponentSelection=C.bShowComponentSelection;Component.bShowComponentSelectionClass=C.bShowComponentSelectionClass;
  }else{const auto& A=*P->ContainerPtrToValuePtr<TArray<FName>>(S);if(A.Num()>4096)return false;Pins=A;}RefreshClassReference();return true;
 }
 void RefreshClassReference(){FilterClass=Field==TEXT("ActorSelector")?Actor.ActorSelectionClass.GetGCPtr():Field==TEXT("ComponentSelector")?Component.ComponentSelectionClass.GetGCPtr():TObjectPtr<UClass>();}
 void* Address(){return Field==TEXT("ActorSelector")?static_cast<void*>(&Actor):Field==TEXT("ComponentSelector")?static_cast<void*>(&Component):static_cast<void*>(&Pins);}
 TObjectPtr<UClass> ClassReference()const
 {return FilterClass;}
 bool SameValue(const FDataSourceAggregateValue& B)const
 {
  if(Field!=B.Field)return false;if(Field==TEXT("ExpectedPins"))return DataSourceNamesSame(Pins,B.Pins);
  const auto AClass=ClassReference(),BClass=B.ClassReference();if(!AClass.IsResolved()||!BClass.IsResolved()||AClass!=BClass)return false;
  if(Field==TEXT("ComponentSelector")){return Component.ComponentSelection==B.Component.ComponentSelection&&Component.ComponentSelectionTag.IsEqual(B.Component.ComponentSelectionTag,ENameCase::CaseSensitive)&&Component.bShowComponentSelection==B.Component.bShowComponentSelection&&Component.bShowComponentSelectionClass==B.Component.bShowComponentSelectionClass;}
  const auto& A=Actor;const auto& C=B.Actor;
  return A.ActorFilter==C.ActorFilter&&A.bMustOverlapSelf==C.bMustOverlapSelf&&A.bIncludeChildren==C.bIncludeChildren&&A.IncludeIsolatedActors==C.IncludeIsolatedActors&&A.bDisableFilter==C.bDisableFilter&&A.ActorSelection==C.ActorSelection&&A.ActorSelectionTag.IsEqual(C.ActorSelectionTag,ENameCase::CaseSensitive)&&MetadataSelectorSame(A.ActorReferenceSelector,C.ActorReferenceSelector)&&A.bSelectMultiple==C.bSelectMultiple&&A.bIgnoreSelfAndChildren==C.bIgnoreSelfAndChildren&&A.bShowActorFilter==C.bShowActorFilter&&A.bShowIncludeChildren==C.bShowIncludeChildren&&A.bShowActorSelection==C.bShowActorSelection&&A.bShowActorSelectionClass==C.bShowActorSelectionClass&&A.bShowSelectMultiple==C.bShowSelectMultiple&&A.bShowIgnoreSelfAndChildren==C.bShowIgnoreSelfAndChildren;
 }
 TSharedRef<FJsonObject> JsonValue()const
 {
  auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("native_property"),Field.ToString());
  if(Field==TEXT("ExpectedPins")){TArray<TSharedPtr<FJsonValue>> A;for(FName N:Pins)A.Add(MakeShared<FJsonValueString>(N.ToString()));J->SetArrayField(TEXT("names"),A);return J;}
  const auto Ref=ClassReference();J->SetBoolField(TEXT("class_reference_resolved"),Ref.IsResolved());J->SetStringField(TEXT("class_reference"),Ref.IsResolved()?Ref.GetPathName():TEXT("<unresolved>"));
  if(Field==TEXT("ComponentSelector")){J->SetNumberField(TEXT("ComponentSelection"),static_cast<int32>(Component.ComponentSelection));J->SetStringField(TEXT("ComponentSelectionTag"),Component.ComponentSelectionTag.ToString());J->SetBoolField(TEXT("bShowComponentSelection"),Component.bShowComponentSelection);J->SetBoolField(TEXT("bShowComponentSelectionClass"),Component.bShowComponentSelectionClass);return J;}
  const auto& A=Actor;J->SetNumberField(TEXT("ActorFilter"),static_cast<int32>(A.ActorFilter));J->SetNumberField(TEXT("ActorSelection"),static_cast<int32>(A.ActorSelection));J->SetNumberField(TEXT("IncludeIsolatedActors"),static_cast<int32>(A.IncludeIsolatedActors));J->SetStringField(TEXT("ActorSelectionTag"),A.ActorSelectionTag.ToString());J->SetObjectField(TEXT("ActorReferenceSelector"),MetadataSelectorJson(A.ActorReferenceSelector));
  J->SetBoolField(TEXT("bMustOverlapSelf"),A.bMustOverlapSelf);J->SetBoolField(TEXT("bIncludeChildren"),A.bIncludeChildren);J->SetBoolField(TEXT("bDisableFilter"),A.bDisableFilter);J->SetBoolField(TEXT("bSelectMultiple"),A.bSelectMultiple);J->SetBoolField(TEXT("bIgnoreSelfAndChildren"),A.bIgnoreSelfAndChildren);J->SetBoolField(TEXT("bShowActorFilter"),A.bShowActorFilter);J->SetBoolField(TEXT("bShowIncludeChildren"),A.bShowIncludeChildren);J->SetBoolField(TEXT("bShowActorSelection"),A.bShowActorSelection);J->SetBoolField(TEXT("bShowActorSelectionClass"),A.bShowActorSelectionClass);J->SetBoolField(TEXT("bShowSelectMultiple"),A.bShowSelectMultiple);J->SetBoolField(TEXT("bShowIgnoreSelfAndChildren"),A.bShowIgnoreSelfAndChildren);return J;
 }
};
// Only split top-level native assignments. ImportText remains the value parser;
// quoted commas/escapes and nested PCGBegin(...)PCGEnd selectors stay intact.
bool DataSourceNativeTextBounded(const FString& S)
{
 if(S.Len()>65536||!NoEmbeddedNul(S))return false;TCHAR Quote=0;bool Escape=false;int32 Token=0,Depth=0;
 for(TCHAR C:S){if(Quote){if(++Token>=NAME_SIZE)return false;if(Escape){Escape=false;continue;}if(C==TEXT('\\')){Escape=true;continue;}if(C==Quote){Quote=0;Token=0;}continue;}if(C==TEXT('"')||C==TEXT('\'')){Quote=C;Token=0;continue;}if(C==TEXT('(')){if(++Depth>64)return false;Token=0;}else if(C==TEXT(')')){if(--Depth<0)return false;Token=0;}else if(C==TEXT(',')||C==TEXT('=')||FChar::IsWhitespace(C)){Token=0;}else if(++Token>=NAME_SIZE)return false;}
 return Quote==0&&Depth==0;
}
bool DataSourceTextMembers(const FString& Text,TArray<TPair<FName,FString>>& Out,TArray<FString>& MemberTails)
{
 FString S=Text.TrimStartAndEnd();if(S.Len()>65536||!NoEmbeddedNul(S)||!S.StartsWith(TEXT("("))||!S.EndsWith(TEXT(")")))return false;S=S.Mid(1,S.Len()-2);int32 Start=0,Depth=0;TCHAR Quote=0;bool Escape=false;TSet<FName> Seen;
 for(int32 I=0;I<=S.Len();++I){const TCHAR C=I<S.Len()?S[I]:TEXT(',');if(Quote){if(Escape)Escape=false;else if(C==TEXT('\\'))Escape=true;else if(C==Quote)Quote=0;continue;}if(C==TEXT('"')||C==TEXT('\'')){Quote=C;continue;}if(C==TEXT('(')){++Depth;continue;}if(C==TEXT(')')){if(--Depth<0)return false;continue;}if(C==TEXT(',')&&Depth==0){FString Part=S.Mid(Start,I-Start).TrimStartAndEnd();const FString MemberTail=S.Mid(Start).TrimStart()+TEXT(")");Start=I+1;if(Part.IsEmpty()){if(S.TrimStartAndEnd().IsEmpty())continue;return false;}FString K,V;if(!Part.Split(TEXT("="),&K,&V))return false;K.TrimStartAndEndInline();V.TrimStartAndEndInline();if(K.IsEmpty()||K.Len()>=NAME_SIZE||V.IsEmpty()||Out.Num()>=64)return false;for(TCHAR N:K)if(!FChar::IsAlnum(N)&&N!=TEXT('_'))return false;const FName Name(*K);if(Seen.Contains(Name))return false;Seen.Add(Name);Out.Emplace(Name,V);MemberTails.Add(MemberTail);}}
 return Quote==0&&Depth==0;
}
// AH: raw Add/Set native struct text is planned on owned copied storage.
// No dict coercion, generic struct writer, class construction or new generation scope.
const TCHAR* const LandscapeSamplingNames[]={TEXT("bGetHeightOnly"),TEXT("bGetLayerWeights"),TEXT("bGetActorReference"),TEXT("bGetPhysicalMaterial"),TEXT("bGetComponentCoordinates"),TEXT("bSampleVirtualTextures"),TEXT("bSampleVirtualTextureNormals")};
bool LandscapeSamplingSame(const FPCGLandscapeDataProps& A,const FPCGLandscapeDataProps& B)
{return A.bGetHeightOnly==B.bGetHeightOnly&&A.bGetLayerWeights==B.bGetLayerWeights&&A.bGetActorReference==B.bGetActorReference&&A.bGetPhysicalMaterial==B.bGetPhysicalMaterial&&A.bGetComponentCoordinates==B.bGetComponentCoordinates&&A.bSampleVirtualTextures==B.bSampleVirtualTextures&&A.bSampleVirtualTextureNormals==B.bSampleVirtualTextureNormals;}
bool RawSamplingProperty(UClass* C,FProperty* P)
{return C==UPCGGetLandscapeSettings::StaticClass()&&P&&P->GetFName()==TEXT("SamplingProperties")&&FreshDataSourceProperty(C,P);}
bool RawVectorProperty(UClass* C,FProperty* P)
{
 const auto* S=CastField<FStructProperty>(P);return C&&StandaloneField(C,P?P->GetFName():NAME_None)&&S&&P->ArrayDim==1&&!P->HasSetterOrGetter()&&S->Struct.IsResolved()&&S->Struct==TBaseStructure<FVector>::Get()&&P->GetSize()==sizeof(FVector)&&P->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible)&&!P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient);
}
struct FRawNativeStructValue
{
 bool Sampling=false;FVector Vector=FVector::ZeroVector;FPCGLandscapeDataProps Landscape;
 void* Address(){return Sampling?static_cast<void*>(&Landscape):static_cast<void*>(&Vector);}
 bool Read(UPCGSettings* S,FProperty* P)
 {Sampling=RawSamplingProperty(S->GetClass(),P);if(Sampling)Landscape=*P->ContainerPtrToValuePtr<FPCGLandscapeDataProps>(S);else if(RawVectorProperty(S->GetClass(),P))Vector=*P->ContainerPtrToValuePtr<FVector>(S);else return false;return Valid();}
 bool Valid()const{return Sampling||(FMath::IsFinite(Vector.X)&&FMath::IsFinite(Vector.Y)&&FMath::IsFinite(Vector.Z));}
 bool SameValue(const FRawNativeStructValue& B)const{return Sampling==B.Sampling&&Valid()&&B.Valid()&&(Sampling?LandscapeSamplingSame(Landscape,B.Landscape):(Vector.X==B.Vector.X&&Vector.Y==B.Vector.Y&&Vector.Z==B.Vector.Z));}
 TSharedRef<FJsonObject> JsonValue()const
 {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("native_struct"),Sampling?TEXT("PCGLandscapeDataProps"):TEXT("Vector"));if(Sampling){
  J->SetBoolField(TEXT("bGetHeightOnly"),Landscape.bGetHeightOnly);J->SetBoolField(TEXT("bGetLayerWeights"),Landscape.bGetLayerWeights);J->SetBoolField(TEXT("bGetActorReference"),Landscape.bGetActorReference);J->SetBoolField(TEXT("bGetPhysicalMaterial"),Landscape.bGetPhysicalMaterial);J->SetBoolField(TEXT("bGetComponentCoordinates"),Landscape.bGetComponentCoordinates);J->SetBoolField(TEXT("bSampleVirtualTextures"),Landscape.bSampleVirtualTextures);J->SetBoolField(TEXT("bSampleVirtualTextureNormals"),Landscape.bSampleVirtualTextureNormals);
 }else{J->SetNumberField(TEXT("X"),Vector.X);J->SetNumberField(TEXT("Y"),Vector.Y);J->SetNumberField(TEXT("Z"),Vector.Z);}return J;}
};
struct FStandaloneValue
{
 UPCGSettings* Settings=nullptr;FProperty* Property=nullptr;FString Path;UScriptStruct* Struct=nullptr;UEnum* Enum=nullptr;FNumericProperty* EnumStorage=nullptr;TArray<TPair<FName,int64>> EnumNames;EObjectFlags EnumFlags=RF_NoFlags;TArray<uint8> Bytes;FArrayProperty* PointArray=nullptr;FStructProperty* PointInner=nullptr;uint64 PointInnerFlags=0;int32 PointInnerOffset=0;TArray<FStandalonePointLayout> PointLayouts;TArray<FPCGPoint> Points;FName NameValue;TSharedPtr<FDataSourceAggregateValue> Aggregate;TSharedPtr<FFreshField> AggregateInner;TSharedPtr<FPCGProjectionParams> Projection;TSharedPtr<FRawNativeStructValue> Sampling;
 bool SameType()const
 {
  if(Sampling)return Sampling->Sampling&&RawSamplingProperty(Settings->GetClass(),Property);
  if(Enum){const auto* E=CastField<FEnumProperty>(Property);if(!E||E->GetEnum()!=Enum)return false;}
  if(FreshProjectionClass(Settings->GetClass()))
  {
   if(Property->GetOwnerStruct()!=Settings->GetClass()||Property->ArrayDim!=1)return false;
   if(Property->GetFName()==TEXT("ProjectionParams")){auto* S=CastField<FStructProperty>(Property);return Projection&&S&&S->Struct.IsResolved()&&S->Struct==FPCGProjectionParams::StaticStruct()&&S->Struct==Struct&&Property->GetSize()==sizeof(FPCGProjectionParams);}
   auto* B=CastField<FBoolProperty>(Property);return B&&B->IsNativeBool()&&Property->GetSize()==sizeof(bool)&&(Property->GetFName()==TEXT("bForceCollapseToPoint")||Property->GetFName()==TEXT("bKeepZeroDensityPoints"));
  }
   if(StandaloneSpawnerClass(Settings->GetClass()))return StandaloneSpawnerProperty(Property);
  if(StandaloneSpatialCompositionClass(Settings->GetClass()))return StandaloneSpatialProperty(Settings->GetClass(),Property,Enum,EnumStorage);
  if(StandaloneSamplingDifferenceClass(Settings->GetClass()))return StandaloneSamplingProperty(Settings->GetClass(),Property,Struct,Enum,EnumStorage);
   if(FreshDataSourceClass(Settings->GetClass()))
   {
    if(!FreshDataSourceProperty(Settings->GetClass(),Property))return false;
    if(DataSourceAggregateName(Property->GetFName())){if(!Aggregate)return false;if(Property->GetFName()==TEXT("ExpectedPins")){const auto* A=static_cast<FArrayProperty*>(Property);return AggregateInner&&A->Inner==AggregateInner->P&&AggregateInner->Exact();}const auto* S=static_cast<FStructProperty*>(Property);return S->Struct.IsResolved()&&S->Struct==(Property->GetFName()==TEXT("ActorSelector")?FPCGActorSelectorSettings::StaticStruct():FPCGComponentSelectorSettings::StaticStruct());}
    if(Property->GetFName()==TEXT("Mode")){const auto* E=CastFieldChecked<FEnumProperty>(Property);return Enum&&Same(Enum->GetPathName(),TEXT("/Script/PCG.EPCGGetDataFromActorMode"))&&E->GetUnderlyingProperty()==EnumStorage;}
    return true;
   }
  if(FreshTransformPointsClass(Settings->GetClass()))
  {
   if(Property->GetOwnerStruct()!=Settings->GetClass())return false;const FName Name=Property->GetFName();
   if(Name==TEXT("AttributeName"))return Property->GetClass()==FNameProperty::StaticClass()&&Property->GetSize()==sizeof(FName);
   if(Name==TEXT("OffsetMin")||Name==TEXT("OffsetMax")||Name==TEXT("ScaleMin")||Name==TEXT("ScaleMax")||Name==TEXT("RotationMin")||Name==TEXT("RotationMax"))
   {const auto* S=CastField<FStructProperty>(Property);const bool Rotation=Name==TEXT("RotationMin")||Name==TEXT("RotationMax");return S&&S->Struct.IsResolved()&&S->Struct==Struct&&Struct==(Rotation?TBaseStructure<FRotator>::Get():TBaseStructure<FVector>::Get())&&Struct->GetStructureSize()==(Rotation?sizeof(FRotator):sizeof(FVector))&&Property->GetSize()==Struct->GetStructureSize();}
   const bool BoolRole=Name==TEXT("bApplyToAttribute")||Name==TEXT("bAbsoluteOffset")||Name==TEXT("bAbsoluteRotation")||Name==TEXT("bAbsoluteScale")||Name==TEXT("bUniformScale")||Name==TEXT("bRecomputeSeed");const auto* B=CastField<FBoolProperty>(Property);return BoolRole&&B&&B->IsNativeBool()&&Property->GetSize()==sizeof(bool);
  }
   if(StandalonePointBoundsClass(Settings->GetClass()))
  {
   if(Property->GetOwnerStruct()!=Settings->GetClass())return false;const FName Name=Property->GetFName();const bool Extents=Settings->GetClass()->GetFName()==TEXT("PCGPointExtentsModifierSettings");
   if((Extents&&Name==TEXT("Extents"))||(!Extents&&(Name==TEXT("BoundsMin")||Name==TEXT("BoundsMax")))){const auto* Vector=CastField<FStructProperty>(Property);return Vector&&Vector->Struct.IsResolved()&&Vector->Struct==Struct&&Struct==TBaseStructure<FVector>::Get()&&Struct->GetStructureSize()==sizeof(FVector)&&Property->GetSize()==sizeof(FVector);}
   if(!Extents&&Name==TEXT("bAffectSteepness")){const auto* Bool=CastField<FBoolProperty>(Property);return Bool&&Bool->IsNativeBool()&&Property->GetSize()==sizeof(bool);}
   if(!Extents&&Name==TEXT("Steepness"))return Property->GetClass()==FFloatProperty::StaticClass()&&Property->GetSize()==sizeof(float);
   const auto* Mode=CastField<FEnumProperty>(Property);return Name==TEXT("Mode")&&Mode&&Mode->GetUnderlyingProperty()==EnumStorage&&Enum&&Same(Enum->GetPathName(),Extents?TEXT("/Script/PCG.EPCGPointExtentsModifierMode"):TEXT("/Script/PCG.EPCGBoundsModifierMode"))&&EnumStorage&&EnumStorage->GetOwner<FProperty>()==Property&&EnumStorage->GetClass()==FByteProperty::StaticClass()&&EnumStorage->ArrayDim==1&&EnumStorage->GetSize()==sizeof(uint8)&&Property->GetSize()==sizeof(uint8);
  }
  const bool Create=Settings->GetClass()->GetFName()==TEXT("PCGCreatePointsSettings"),Actor=Settings->GetClass()->GetFName()==TEXT("PCGGetActorPropertySettings");
  if(Create||Actor)
  {
   if(Property->GetOwnerStruct()!=Settings->GetClass())return false;const FName N=Property->GetFName();
   if(Create&&N==TEXT("PointsToCreate"))
   {
    if(Property!=PointArray||!PointInner||PointArray->Inner!=PointInner||PointInner->GetOwner<FProperty>()!=Property||PointInner->GetClass()!=FStructProperty::StaticClass()||PointInner->ArrayDim!=1||PointInner->GetSize()!=sizeof(FPCGPoint)||PointInner->GetPropertyFlags()!=PointInnerFlags||PointInner->GetOffset_ForInternal()!=PointInnerOffset||!PointInner->Struct.IsResolved()||PointInner->Struct!=Struct||!Struct||!Same(Struct->GetPathName(),TEXT("/Script/PCG.PCGPoint"))||Struct->GetStructureSize()!=sizeof(FPCGPoint)||PointLayouts.IsEmpty())return false;
    for(const auto& L:PointLayouts)if(!L.Exact())return false;return true;
   }
   if(Actor&&N==TEXT("PropertyName"))return Property->GetClass()==FNameProperty::StaticClass()&&Property->GetSize()==sizeof(FName);
   if((Create&&N==TEXT("bCullPointsOutsideVolume"))||(Actor&&N==TEXT("bAlwaysRequeryActors"))){const auto* B=CastField<FBoolProperty>(Property);return B&&B->IsNativeBool()&&Property->GetSize()==sizeof(bool);}
   const auto* E=CastField<FEnumProperty>(Property);return Create&&N==TEXT("CoordinateSpace")&&E&&E->GetUnderlyingProperty()==EnumStorage&&Enum&&Same(Enum->GetPathName(),TEXT("/Script/PCG.EPCGCoordinateSpace"))&&EnumStorage&&EnumStorage->GetOwner<FProperty>()==Property&&EnumStorage->GetClass()==FByteProperty::StaticClass()&&EnumStorage->ArrayDim==1&&EnumStorage->GetSize()==sizeof(uint8)&&Property->GetSize()==sizeof(uint8);
  }
  const bool Noise=Settings->GetClass()->GetFName()==TEXT("PCGAttributeNoiseSettings"),Filtering=Settings->GetClass()->GetFName()==TEXT("PCGAttributeFilteringSettings");
  if(Noise||Filtering)
  {
   if(Property->GetOwnerStruct()!=Settings->GetClass())return false;const FName Name=Property->GetFName();
   if(Noise&&(Name==TEXT("NoiseMin")||Name==TEXT("NoiseMax")))return Property->GetClass()==FFloatProperty::StaticClass()&&Property->GetSize()==sizeof(float);
   const bool BoolRole=(Noise&&(Name==TEXT("bClampResult")||Name==TEXT("bInvertSource")||Name==TEXT("bHasCustomSeedSource")))||(Filtering&&(Name==TEXT("bUseConstantThreshold")||Name==TEXT("bUseSpatialQuery")||Name==TEXT("bWarnOnDataMissingAttribute")));
   if(BoolRole){const auto* Bool=CastField<FBoolProperty>(Property);return Bool&&Bool->IsNativeBool()&&Property->GetSize()==sizeof(bool);}
   const bool EnumRole=Noise?Name==TEXT("Mode"):Name==TEXT("Operator");const auto* E=CastField<FEnumProperty>(Property);
   return EnumRole&&E&&E->GetUnderlyingProperty()==EnumStorage&&Enum&&Same(Enum->GetPathName(),Noise?TEXT("/Script/PCG.EPCGAttributeNoiseMode"):TEXT("/Script/PCG.EPCGAttributeFilterOperator"))&&EnumStorage&&EnumStorage->GetClass()==FByteProperty::StaticClass()&&EnumStorage->GetOwner<FProperty>()==Property&&EnumStorage->ArrayDim==1&&EnumStorage->GetSize()==sizeof(uint8)&&Property->GetSize()==sizeof(uint8);
  }
  if(auto* S=CastField<FStructProperty>(Property))return S->Struct.IsResolved()&&S->Struct==Struct&&Struct&&Struct->GetStructureSize()==sizeof(FVector)&&Property->GetSize()==sizeof(FVector);
  if(auto* E=CastField<FEnumProperty>(Property))
  {
   if(E->GetUnderlyingProperty()!=EnumStorage||!Enum||!EnumStorage||EnumStorage->GetOwner<FProperty>()!=Property)return false;
   if(Settings->GetClass()->GetFName()==TEXT("PCGDistanceSettings"))return Property->GetOwnerStruct()==Settings->GetClass()&&(Property->GetFName()==TEXT("SourceShape")||Property->GetFName()==TEXT("TargetShape"))&&Same(Enum->GetPathName(),TEXT("/Script/PCG.PCGDistanceShape"))&&EnumStorage->GetClass()==FIntProperty::StaticClass()&&EnumStorage->ArrayDim==1&&EnumStorage->GetSize()==sizeof(int32)&&Property->GetSize()==sizeof(int32);
   return EnumStorage->GetClass()==FByteProperty::StaticClass()&&Property->GetSize()==1;
  }
  if(auto* B=CastField<FBoolProperty>(Property))return B->IsNativeBool()&&Property->GetSize()==sizeof(bool);
  if(Property->GetClass()==FIntProperty::StaticClass())return Settings->GetClass()->GetFName()==TEXT("PCGRandomChoiceSettings")&&Property->GetSize()==sizeof(int32)&&((Property->GetFName()==TEXT("FixedNumber")&&Property->GetOwnerStruct()==Settings->GetClass())||(Property->GetFName()==TEXT("Seed")&&Same(Property->GetOwnerStruct()->GetPathName(),TEXT("/Script/PCG.PCGSettings"))));
  if(Property->GetClass()==FDoubleProperty::StaticClass())return Settings->GetClass()->GetFName()==TEXT("PCGDistanceSettings")&&Property->GetFName()==TEXT("MaximumDistance")&&Property->GetOwnerStruct()==Settings->GetClass()&&Property->GetSize()==sizeof(double);
  return Property->GetClass()==FFloatProperty::StaticClass()&&Property->GetSize()==sizeof(float);
 }
 bool EnumCurrent()const{if(!Enum)return true;if(Enum->GetFlags()!=EnumFlags||Enum->NumEnums()!=EnumNames.Num())return false;for(int32 I=0;I<EnumNames.Num();++I)if(Enum->GetNameByIndex(I)!=EnumNames[I].Key||Enum->GetValueByIndex(I)!=EnumNames[I].Value)return false;return true;}
 bool SameValue()const
 {
  if(Sampling){FRawNativeStructValue Now;return Now.Read(Settings,Property)&&Sampling->SameValue(Now);}
  if(Projection)return SameProjectionAuthored(*Projection,*Property->ContainerPtrToValuePtr<FPCGProjectionParams>(Settings));
  if(Aggregate){FDataSourceAggregateValue Now;return Now.Read(Settings,Property)&&Aggregate->SameValue(Now);}
  if(PointArray){FScriptArrayHelper H(PointArray,Property->ContainerPtrToValuePtr<void>(Settings));if(H.Num()!=Points.Num())return false;for(int32 I=0;I<H.Num();++I)if(!SamePointValue(*reinterpret_cast<const FPCGPoint*>(H.GetRawPtr(I)),Points[I]))return false;return true;}
  if(const auto* N=CastField<FNameProperty>(Property))return N->GetPropertyValue(Property->ContainerPtrToValuePtr<void>(Settings)).IsEqual(NameValue,ENameCase::CaseSensitive);
  return Bytes.Num()==Property->GetSize()&&FMemory::Memcmp(Bytes.GetData(),Property->ContainerPtrToValuePtr<void>(Settings),Bytes.Num())==0;
 }
 bool Capture()
 {
  if(Sampling)return Sampling->Read(Settings,Property);
  if(Projection){*Projection=*Property->ContainerPtrToValuePtr<FPCGProjectionParams>(Settings);return true;}
  if(Aggregate)return Aggregate->Read(Settings,Property);
  if(PointArray){FScriptArrayHelper H(PointArray,Property->ContainerPtrToValuePtr<void>(Settings));if(H.Num()>128)return false;Points.Reset(H.Num());for(int32 I=0;I<H.Num();++I)Points.Add(*reinterpret_cast<const FPCGPoint*>(H.GetRawPtr(I)));return true;}
  if(const auto* N=CastField<FNameProperty>(Property)){NameValue=N->GetPropertyValue(Property->ContainerPtrToValuePtr<void>(Settings));return true;}
  Bytes.SetNumUninitialized(Property->GetSize());FMemory::Memcpy(Bytes.GetData(),Property->ContainerPtrToValuePtr<void>(Settings),Bytes.Num());return true;
 }
};
// OG: graph-owned stock Spawner instances have a constructor-created selector,
// unlike the Spawner CDO (PCGStaticMeshSpawner.cpp129-138). No selector write,
// source-CDO construction, custom selector/packer or graph_spec expansion occurs.
struct FStandaloneSpawner
{
 UPCGStaticMeshSpawnerSettings* Settings=nullptr;UPCGMeshSelectorBase* Selector=nullptr;UClass* Type=nullptr;
 TObjectPtr<UClass> TypeHandle;TObjectPtr<UClass> PackerType;
 TArray<FPCGMeshSelectorWeightedEntry> Entries;FName Attribute;TArray<FName> Materials;bool UseMaterials=false;FPCGSoftISMComponentDescriptor Template;
 bool AllowDescriptorChanges=false,ApplyBounds=false,Synchronous=false,AllowMerge=false,Silence=false,Warn=false,SetupCulling=false;int32 CullSize=0;
 FBoolProperty* SetupField=nullptr;FBoolProperty* GPUField=nullptr;FIntProperty* CullField=nullptr;FName OutAttribute;
 bool Enabled=false,Debug=false,BreakDebugger=false,MarkedForManualEditing=false,ExecutionDependency=false,ExposeToLibrary=false,ExecuteOnGPU=false,DumpCookedHLSL=false,DumpDataDescriptions=false,PrintShaderDebugValues=false,TriggerRenderCapture=false;
 int32 Seed=0,DebugBufferSize=0,ProfileKernelIndex=0;FPCGDebugVisualizationSettings DebugSettings;FPCGDeterminismSettings Determinism;FText Category,Description;
 TSoftObjectPtr<AActor> TargetActor;TArray<FName> PostProcess;TArray<FPCGObjectPropertyOverrideDescription> Overrides;
 FString Path,SelectorPath;UObject* SettingsOuter=nullptr;UPackage* Package=nullptr;EObjectFlags SettingsFlags=RF_NoFlags,SelectorFlags=RF_NoFlags;
 static bool NamesSame(const TArray<FName>& A,const TArray<FName>& B){if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(!A[I].IsEqual(B[I],ENameCase::CaseSensitive))return false;return true;}
 bool Role(bool SelectorTransition=false,bool SavedExport=false)const
 {
  const EObjectFlags SaveBits=SavedExport?(RF_WasLoaded|RF_LoadCompleted):RF_NoFlags;
  if(!Live(Settings)||Settings->GetClass()!=UPCGStaticMeshSpawnerSettings::StaticClass()||Settings->GetOuter()!=SettingsOuter||Settings->GetPackage()!=Package||Settings->GetOutermost()!=Package||Settings->GetFlags()!=(SettingsFlags|SaveBits)||!Settings->MeshSelectorParameters.IsResolved()||!Settings->MeshSelectorType.GetGCPtr().IsResolved())return false;
  if(!SelectorTransition&&(Settings->MeshSelectorType.GetGCPtr()!=TypeHandle||Settings->MeshSelectorParameters!=Selector||!Live(Selector)||Selector->GetFlags()!=(SelectorFlags|SaveBits)||Selector->GetClass()!=Type||Selector->GetOuter()!=Settings||Selector->GetPackage()!=Settings->GetPackage()||Selector->GetOutermost()!=Settings->GetOutermost()))return false;
  if((Type!=UPCGMeshSelectorWeighted::StaticClass()&&Type!=UPCGMeshSelectorByAttribute::StaticClass())||!Settings->InstanceDataPackerParameters.IsResolved()||Settings->InstanceDataPackerParameters!=nullptr||!Settings->InstanceDataPackerType.GetGCPtr().IsResolved()||Settings->InstanceDataPackerType.GetGCPtr()!=PackerType)return false;
  return FreshNativeField(Settings->GetClass(),TEXT("bSetupCullingCells"))==SetupField&&FreshNativeField(Settings->GetClass(),TEXT("CullingCellSize"))==CullField&&FreshNativeField(Settings->GetClass(),TEXT("bExecuteOnGPU"))==GPUField;
 }
 bool SameValue(FName Selected=NAME_None,bool SelectorTransition=false,bool SavedExport=false)const
 {
  if(!Role(SelectorTransition,SavedExport))return false;
  if((Selected!=TEXT("bEnabled")&&Settings->bEnabled!=Enabled)||Settings->bDebug!=Debug||Settings->bBreakDebugger!=BreakDebugger||Settings->IsMarkedForManualEditing()!=MarkedForManualEditing||(Selected!=TEXT("Seed")&&Settings->Seed!=Seed)||Settings->bExecutionDependencyRequired!=ExecutionDependency||Settings->bExposeToLibrary!=ExposeToLibrary||GPUField->GetPropertyValue_InContainer(Settings)!=ExecuteOnGPU||Settings->bDumpCookedHLSL!=DumpCookedHLSL||Settings->bDumpDataDescriptions!=DumpDataDescriptions||Settings->bPrintShaderDebugValues!=PrintShaderDebugValues||Settings->DebugBufferSize!=DebugBufferSize||Settings->bTriggerRenderCapture!=TriggerRenderCapture||Settings->ProfileKernelIndex!=ProfileKernelIndex)return false;
  // Compare copied FText identity/history+flags without ToString/Rebuild/loading.
  if(FTextInspector::GetSharedDataId(Settings->Category)!=FTextInspector::GetSharedDataId(Category)||FTextInspector::GetFlags(Settings->Category)!=FTextInspector::GetFlags(Category)||FTextInspector::GetSharedDataId(Settings->Description)!=FTextInspector::GetSharedDataId(Description)||FTextInspector::GetFlags(Settings->Description)!=FTextInspector::GetFlags(Description))return false;
  const auto& ActualDebug=Settings->DebugSettings;
  if(ActualDebug.PointScale!=DebugSettings.PointScale||ActualDebug.ScaleMethod!=DebugSettings.ScaleMethod||ActualDebug.PointMesh!=DebugSettings.PointMesh||ActualDebug.MaterialOverride!=DebugSettings.MaterialOverride||ActualDebug.bDisplayProperties!=DebugSettings.bDisplayProperties)return false;
  auto ActualTest=Settings->DeterminismSettings.DeterminismTestBlueprint;auto ExpectedTest=Determinism.DeterminismTestBlueprint;
  if(Settings->DeterminismSettings.bNativeTests!=Determinism.bNativeTests||Settings->DeterminismSettings.bUseBlueprintDeterminismTest!=Determinism.bUseBlueprintDeterminismTest||ActualTest.GetGCPtr().IsResolved()!=ExpectedTest.GetGCPtr().IsResolved()||ActualTest.GetGCPtr().GetHandle()!=ExpectedTest.GetGCPtr().GetHandle())return false;
  if((Selected!=TEXT("bAllowDescriptorChanges")&&Settings->bAllowDescriptorChanges!=AllowDescriptorChanges)||(Selected!=TEXT("bApplyMeshBoundsToPoints")&&Settings->bApplyMeshBoundsToPoints!=ApplyBounds)||(Selected!=TEXT("bSynchronousLoad")&&Settings->bSynchronousLoad!=Synchronous)||(Selected!=TEXT("bAllowMergeDifferentDataInSameInstancedComponents")&&Settings->bAllowMergeDifferentDataInSameInstancedComponents!=AllowMerge)||(Selected!=TEXT("bSilenceOverrideAttributeNotFoundErrors")&&Settings->bSilenceOverrideAttributeNotFoundErrors!=Silence)||(Selected!=TEXT("bWarnOnIdenticalSpawn")&&Settings->bWarnOnIdenticalSpawn!=Warn)||(Selected!=TEXT("OutAttributeName")&&!Settings->OutAttributeName.IsEqual(OutAttribute,ENameCase::CaseSensitive))||Settings->TargetActor!=TargetActor||!NamesSame(Settings->PostProcessFunctionNames,PostProcess)||SetupField->GetPropertyValue_InContainer(Settings)!=SetupCulling||CullField->GetPropertyValue_InContainer(Settings)!=CullSize||Settings->StaticMeshComponentPropertyOverrides.Num()!=Overrides.Num())return false;
  for(int32 I=0;I<Overrides.Num();++I)if(!FPCGObjectPropertyOverrideDescription::StaticStruct()->CompareScriptStruct(&Settings->StaticMeshComponentPropertyOverrides[I],&Overrides[I],0))return false;
  if(SelectorTransition)return true;
  if(Type==UPCGMeshSelectorWeighted::StaticClass()){
   const auto* Weighted=static_cast<const UPCGMeshSelectorWeighted*>(Selector);if(Weighted->MeshEntries.Num()!=Entries.Num()||Weighted->bUseAttributeMaterialOverrides!=UseMaterials||!NamesSame(Weighted->MaterialOverrideAttributes,Materials))return false;
   for(int32 I=0;I<Entries.Num();++I)if(!FPCGMeshSelectorWeightedEntry::StaticStruct()->CompareScriptStruct(&Weighted->MeshEntries[I],&Entries[I],0))return false;
  }else{
   const auto* ByAttribute=static_cast<const UPCGMeshSelectorByAttribute*>(Selector);if(!ByAttribute->AttributeName.IsEqual(Attribute,ENameCase::CaseSensitive)||ByAttribute->bUseAttributeMaterialOverrides!=UseMaterials||!NamesSame(ByAttribute->MaterialOverrideAttributes,Materials)||!FPCGSoftISMComponentDescriptor::StaticStruct()->CompareScriptStruct(&ByAttribute->TemplateDescriptor,&Template,0))return false;
  }
  return true;
 }
 bool AcceptSelected(FName Name,const TSharedPtr<FJsonValue>& Expected)
 {
  if(Name==TEXT("Seed")){Seed=static_cast<int32>(Expected->AsNumber());return true;}
  if(Name==TEXT("bEnabled")){Enabled=Expected->AsBool();return true;}
  if(Name==TEXT("bAllowDescriptorChanges")){AllowDescriptorChanges=Expected->AsBool();return true;}
  if(Name==TEXT("bApplyMeshBoundsToPoints")){ApplyBounds=Expected->AsBool();return true;}
  if(Name==TEXT("bSynchronousLoad")){Synchronous=Expected->AsBool();return true;}
  if(Name==TEXT("bAllowMergeDifferentDataInSameInstancedComponents")){AllowMerge=Expected->AsBool();return true;}
  if(Name==TEXT("bSilenceOverrideAttributeNotFoundErrors")){Silence=Expected->AsBool();return true;}
  if(Name==TEXT("bWarnOnIdenticalSpawn")){Warn=Expected->AsBool();return true;}
  if(Name==TEXT("OutAttributeName")){OutAttribute=FName(*Expected->AsString());return true;}
  return false;
 }
 void CaptureSelectorValues()
 {
  if(Type==UPCGMeshSelectorWeighted::StaticClass()){const auto* Weighted=static_cast<const UPCGMeshSelectorWeighted*>(Selector);Entries=Weighted->MeshEntries;Materials=Weighted->MaterialOverrideAttributes;UseMaterials=Weighted->bUseAttributeMaterialOverrides;}
  else{const auto* ByAttribute=static_cast<const UPCGMeshSelectorByAttribute*>(Selector);Attribute=ByAttribute->AttributeName;Materials=ByAttribute->MaterialOverrideAttributes;UseMaterials=ByAttribute->bUseAttributeMaterialOverrides;Template=ByAttribute->TemplateDescriptor;}
 }
 bool Capture()
 {
  SetupField=CastField<FBoolProperty>(FreshNativeField(Settings->GetClass(),TEXT("bSetupCullingCells")));GPUField=CastField<FBoolProperty>(FreshNativeField(Settings->GetClass(),TEXT("bExecuteOnGPU")));CullField=CastField<FIntProperty>(FreshNativeField(Settings->GetClass(),TEXT("CullingCellSize")));
  if(!GPUField||!GPUField->IsNativeBool()||GPUField->ArrayDim!=1||GPUField->GetSize()!=sizeof(bool)||GPUField->HasSetterOrGetter()||!SetupField||!SetupField->IsNativeBool()||SetupField->ArrayDim!=1||SetupField->GetSize()!=sizeof(bool)||!CullField||CullField->ArrayDim!=1||CullField->GetSize()!=sizeof(int32)||SetupField->HasSetterOrGetter()||CullField->HasSetterOrGetter())return false;
  TypeHandle=Settings->MeshSelectorType.GetGCPtr();PackerType=Settings->InstanceDataPackerType.GetGCPtr();SettingsOuter=Settings->GetOuter();Package=Settings->GetPackage();SettingsFlags=Settings->GetFlags();SelectorFlags=Selector->GetFlags();if(!Role())return false;
  Enabled=Settings->bEnabled;Debug=Settings->bDebug;BreakDebugger=Settings->bBreakDebugger;MarkedForManualEditing=Settings->IsMarkedForManualEditing();Seed=Settings->Seed;ExecutionDependency=Settings->bExecutionDependencyRequired;ExposeToLibrary=Settings->bExposeToLibrary;ExecuteOnGPU=GPUField->GetPropertyValue_InContainer(Settings);DumpCookedHLSL=Settings->bDumpCookedHLSL;DumpDataDescriptions=Settings->bDumpDataDescriptions;PrintShaderDebugValues=Settings->bPrintShaderDebugValues;DebugBufferSize=Settings->DebugBufferSize;TriggerRenderCapture=Settings->bTriggerRenderCapture;ProfileKernelIndex=Settings->ProfileKernelIndex;DebugSettings=Settings->DebugSettings;Determinism=Settings->DeterminismSettings;Category=Settings->Category;Description=Settings->Description;
  AllowDescriptorChanges=Settings->bAllowDescriptorChanges;ApplyBounds=Settings->bApplyMeshBoundsToPoints;Synchronous=Settings->bSynchronousLoad;AllowMerge=Settings->bAllowMergeDifferentDataInSameInstancedComponents;Silence=Settings->bSilenceOverrideAttributeNotFoundErrors;Warn=Settings->bWarnOnIdenticalSpawn;OutAttribute=Settings->OutAttributeName;TargetActor=Settings->TargetActor;PostProcess=Settings->PostProcessFunctionNames;Overrides=Settings->StaticMeshComponentPropertyOverrides;SetupCulling=SetupField->GetPropertyValue_InContainer(Settings);CullSize=CullField->GetPropertyValue_InContainer(Settings);
  CaptureSelectorValues();
  Path=Settings->GetPathName();SelectorPath=Selector->GetPathName();return SameValue();
 }
};
// QO/TU concrete saved-source/owned-instance roles. No global construction gate change.
bool StandaloneSubgraphClass(UClass* C){return C==UPCGSubgraphSettings::StaticClass();}
struct FStandaloneSubgraph
{
 UPCGGraph* OwnerGraph=nullptr;UPCGSubgraphNode* Node=nullptr;UPCGSubgraphSettings* Settings=nullptr;UPCGGraphInstance* Instance=nullptr;UPCGGraphInterface* Override=nullptr;UPCGGraphInterface* InstanceGraph=nullptr;
 TArray<FFreshField> Fields;FObjectProperty* OverrideField=nullptr;bool Mutable=false;UPCGGraphInterface* Planned=nullptr;int32 Seed=0;bool Enabled=true,Debug=false;
};
struct FStandaloneSubgraphIO
{
 UPCGGraphInputOutputSettings* Settings=nullptr;FArrayProperty* Field=nullptr;FStructProperty* Inner=nullptr;TArray<FFreshField> Fields;TArray<FPCGPinProperties> Pins;bool Input=false;
};
struct FStandaloneSubgraphSource
{
 TStrongObjectPtr<UPCGGraph> Graph;TStrongObjectPtr<UPackage> Package;uint32 PackageFlags=0;FString Path,File,Sha256;FGraphSnapshot Snapshot;TArray<FStandaloneSubgraphIO> IOs;bool Ready=false;
};

// DynamicInputPinProperties is authored (SDK PCGSettingsWithDynamicInputs.h59),
// unlike derived physical pins or compiler caches. No Add/RemoveDynamicInputPin call.
struct FStandaloneSpatialPins
{
 UPCGSettings* Settings=nullptr;FArrayProperty* Field=nullptr;FStructProperty* Inner=nullptr;TArray<FFreshField> Fields;TArray<FPCGPinProperties> Pins;
};
struct FStandaloneRequest
{
 bool SubgraphActive=false,SubgraphStopped=false;TStrongObjectPtr<UPCGSubgraphSettings> SubgraphDefaults;TStrongObjectPtr<UPCGGraphInstance> SubgraphDefaultInstance;TArray<FStandaloneSubgraph> Subgraphs;TArray<FStandaloneSubgraphSource> SubgraphSources;TSet<UPCGGraph*> ActiveSubgraphSources;int32 SubgraphSharedReuseCount=0,SubgraphMaxDepth=0;
 TArray<FStandaloneSelectorSource> MetadataSources;TArray<FStandaloneSelectorSource::FEnum> MetadataEnums;TArray<FStandaloneMetadata> Metadata;TMap<FProperty*,FStandaloneMetadataCell> MetadataPlans;bool MetadataSourcesReady=false,DataSourceSourcesReady=false;TSet<UClass*> DataSourceReferenceClasses;TSet<UPackage*> DataSourceReferenceAudits;
 FWork& W;FFreshRequest Held;TArray<FStandaloneSpatialPins> SpatialPins;TArray<FStandaloneValue> Values;TArray<FStandaloneSpawner> Spawners;UPCGSettings* MutableSettings=nullptr;FProperty* MutableField=nullptr;UClass* PreparedAdd=nullptr;
 explicit FStandaloneRequest(FWork& Work):W(Work),Held(Work){}
 bool Bad(const TCHAR* Code,const FString& Why,const FString& Object=FString(),const FString& Expected=TEXT("combined guarded condition=true"),const FString& Actual=TEXT("combined guarded condition=false"))
 {if(!W.R.Data->HasField(TEXT("standalone_first_failure"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),W.R.Stage);J->SetStringField(TEXT("condition"),Code);J->SetStringField(TEXT("object"),Object.IsEmpty()?Held.ObjectPath:Object);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);J->SetBoolField(TEXT("independent_engine_reread"),false);J->SetBoolField(TEXT("expected_actual_observed"),false);W.R.Data->SetObjectField(TEXT("standalone_first_failure"),J);}return W.Fail(Code,Why);}

 // ADL: copied native declarations and callback-free text-data observations,
 // scoped to this SDK editing boundary, never a general FText persistence rule.
 bool SpatialPinsCurrent()
 {
  for(const auto& E:SpatialPins)
  {
   const bool Retired=W.Objects.ContainsByPredicate([&](const TUniquePtr<FObjectIdentity>& I){return I->Retired&&I->Object.Get()==E.Settings;});if(Retired)continue;
   if(!Live(E.Settings)||!StandaloneSpatialDynamicClass(E.Settings->GetClass())||FreshNativeField(E.Settings->GetClass(),TEXT("DynamicInputPinProperties"))!=E.Field||E.Fields.Num()!=2||!E.Fields[0].Exact()||E.Field->Inner!=E.Inner||!E.Fields[1].Exact()||!E.Inner->Struct.IsResolved()||E.Inner->Struct.GetHandle()!=TObjectPtr<UScriptStruct>(FPCGPinProperties::StaticStruct()).GetHandle())return Bad(TEXT("pcg_spatial_dynamic_pin_layout_changed"),TEXT("Held native dynamic pin declaration descriptor/Inner changed"));
   const auto& Pins=*E.Field->ContainerPtrToValuePtr<TArray<FPCGPinProperties>>(E.Settings);if(Pins.Num()!=E.Pins.Num())return Bad(TEXT("pcg_spatial_dynamic_pin_count_changed"),TEXT("Authored dynamic input count changed across scalar authoring"));
   for(int32 Index=0;Index<Pins.Num();++Index){const auto& A=Pins[Index];const auto& B=E.Pins[Index];if(!A.Label.IsEqual(B.Label,ENameCase::CaseSensitive)||!(A==B)||FTextInspector::GetSharedDataId(A.Tooltip)!=FTextInspector::GetSharedDataId(B.Tooltip)||FTextInspector::GetFlags(A.Tooltip)!=FTextInspector::GetFlags(B.Tooltip))return Bad(TEXT("pcg_spatial_dynamic_pin_declaration_changed"),TEXT("Authored dynamic input declaration changed across scalar authoring"),E.Settings->GetPathName(),FString::Printf(TEXT("captured declaration[%d]"),Index),TEXT("current declaration differs"));}
  }
  return true;
 }
 bool CaptureSpatialPins(UPCGSettings* S)
 {
  if(!StandaloneSpatialDynamicClass(S->GetClass())||SpatialPins.ContainsByPredicate([&](const FStandaloneSpatialPins& E){return E.Settings==S;}))return true;
  auto* A=CastField<FArrayProperty>(FreshNativeField(S->GetClass(),TEXT("DynamicInputPinProperties")));auto* I=A?CastField<FStructProperty>(A->Inner):nullptr;
  if(!A||!I||!Same(A->GetOwnerStruct()->GetPathName(),TEXT("/Script/PCG.PCGSettingsWithDynamicInputs"))||A->ArrayDim!=1||A->GetSize()!=sizeof(TArray<FPCGPinProperties>)||A->HasSetterOrGetter()||I->GetOwner<FProperty>()!=A||I->ArrayDim!=1||I->GetSize()!=sizeof(FPCGPinProperties)||I->HasSetterOrGetter()||!I->Struct.IsResolved()||I->Struct.GetHandle()!=TObjectPtr<UScriptStruct>(FPCGPinProperties::StaticStruct()).GetHandle())return Bad(TEXT("pcg_spatial_dynamic_pin_layout_pending"),TEXT("Exact native authored dynamic pin declaration array/Inner required"));
  const auto& Pins=*A->ContainerPtrToValuePtr<TArray<FPCGPinProperties>>(S);if(Pins.Num()>128||!W.CaptureBudget(Pins.Num()))return Bad(TEXT("pcg_spatial_dynamic_pin_budget"),TEXT("Existing 128-pin bounded declaration profile required"));
  FStandaloneSpatialPins E;E.Settings=S;E.Field=A;E.Inner=I;E.Fields.Emplace(A);E.Fields.Emplace(I);E.Pins=Pins;SpatialPins.Add(MoveTemp(E));
  return W.Pin(FPCGPinProperties::StaticStruct())&&W.Pin(FPCGPinProperties::StaticStruct()->GetPackage())&&SpatialPinsCurrent();
 }
 bool MetadataSourcesPlain()
 {
  for(const auto& Source:MetadataSources)if(!Source.Current())return Bad(TEXT("pcg_metadata_selector_source_changed"),TEXT("Native selector root/nested descriptor source changed"));
  for(const auto& Source:MetadataSources)if(Source.Struct==FPCGActorSelectorSettings::StaticStruct()||Source.Struct==FPCGComponentSelectorSettings::StaticStruct()){for(const auto& F:Source.Fields){if(auto* C=CastField<FClassProperty>(F.P)){UClass* Base=Source.Struct==FPCGActorSelectorSettings::StaticStruct()?AActor::StaticClass():UActorComponent::StaticClass();if(!C->MetaClass.IsResolved()||C->MetaClass!=Base||!C->PropertyClass.IsResolved()||C->PropertyClass!=UClass::StaticClass())return Bad(TEXT("pcg_data_source_filter_descriptor_changed"),TEXT("Native selector class-reference metaclass binding changed"));}else if(auto* Child=CastField<FStructProperty>(F.P)){if(!Child->Struct.IsResolved()||Child->Struct!=FPCGAttributePropertyInputSelector::StaticStruct())return Bad(TEXT("pcg_data_source_actor_reference_descriptor_changed"),TEXT("Native ActorReferenceSelector type binding changed"));}}}
  for(const auto& E:MetadataEnums){if(E.Property->GetUnderlyingProperty()!=E.Storage||!E.Field.Exact()||E.Storage->GetOwner<FProperty>()!=E.Property)return Bad(TEXT("pcg_metadata_enum_storage_changed"),TEXT("Native Operation storage binding/layout changed"));if(E.Enum.IsValid()){if(!Live(E.Enum.Get())||E.Enum->GetFlags()!=E.Flags||E.Enum->NumEnums()!=E.Names.Num())return false;for(int32 I=0;I<E.Names.Num();++I)if(E.Enum->GetNameByIndex(I)!=E.Names[I].Key||E.Enum->GetValueByIndex(I)!=E.Names[I].Value)return false;}}
  return true;
 }
 bool MetadataPlain()
 {
  if(!MetadataSourcesPlain())return false;
  for(const auto& E:Metadata){const bool Retired=W.Objects.ContainsByPredicate([&](const TUniquePtr<FObjectIdentity>& I){return I->Retired&&I->Object.Get()==E.Settings;});if(Retired)continue;if(!Live(E.Settings)||!StandaloneMetadataClass(E.Settings->GetClass()))return Bad(TEXT("pcg_metadata_settings_role_changed"),TEXT("Held own exact native metadata settings unavailable"));
   for(const auto& F:E.Fields){if(FreshNativeField(E.Settings->GetClass(),F.Name)!=F.P||!F.Exact()||F.P->HasSetterOrGetter())return Bad(TEXT("pcg_metadata_field_changed"),TEXT("Direct native metadata root field binding/layout changed"),E.Settings->GetPathName()+TEXT(":")+F.Name.ToString());if(F.Kind==FStructProperty::StaticClass()){const auto* P=static_cast<const FStructProperty*>(F.P);UScriptStruct* Expected=(F.Name==TEXT("OutputTarget"))?FPCGAttributePropertyOutputSelector::StaticStruct():FPCGAttributePropertyInputSelector::StaticStruct();if(!P->Struct.IsResolved()||P->Struct.Get()!=Expected)return Bad(TEXT("pcg_metadata_selector_struct_changed"),TEXT("Exact selector type binding changed"));}}
   if(E.Map){if(E.Map->KeyProp!=E.MapKey||E.Map->ValueProp!=E.MapValue)return Bad(TEXT("pcg_metadata_domains_map_binding_changed"),TEXT("Domains mapping Key/Value descriptor changed"));for(const auto& F:E.MapFields)if(!F.Exact()||F.P->GetOwner<FProperty>()!=E.Map)return Bad(TEXT("pcg_metadata_domains_map_layout_changed"),TEXT("Domains mapping nested FName layout changed"));}
   const FName Selected=(E.Settings==MutableSettings&&MutableField)?MutableField->GetFName():NAME_None;if(!Selected.IsNone()){const auto* Planned=MetadataPlans.Find(MutableField);FStandaloneMetadataCell Actual,Before;Actual.Name=Before.Name=Selected;if(!Planned||!E.Read(Actual)||!E.Stored(Before)||(!MetadataCellSame(Actual,Before)&&!MetadataCellSame(Actual,*Planned)))return Bad(TEXT("pcg_metadata_selected_value_changed"),TEXT("Selected raw cell differs from both captured and preplanned value"),E.Settings->GetPathName()+TEXT(":")+Selected.ToString(),Planned?Json(MetadataSelectorJson(Planned->Selector)):TEXT("missing plan"),TEXT("current copied selected cell differs"));}if(!E.SameValue(Selected))return Bad(TEXT("pcg_metadata_unselected_value_changed"),TEXT("Captured selector/name/bool/enum/map or common authored values changed outside selected write"),E.Settings->GetPathName(),TEXT("captured case-sensitive semantic values"),TEXT("current semantic value differs"));
  }return true;
 }
 bool MetadataEnumHandles()
 {
  // Public Enum getters can resolve handles; close the pure source/value projection after every return.
  for(auto& Source:MetadataSources)for(auto& E:Source.Enums){UEnum* Returned=E.Property->GetEnum();TStrongObjectPtr<UEnum> Immediate(Returned);if(!PlainCurrent())return false;if(E.Enum.IsValid()&&Immediate.Get()!=E.Enum.Get())return Bad(TEXT("pcg_metadata_selector_enum_changed"),TEXT("Captured selector enum binding changed"));}
  for(auto& E:MetadataEnums){UEnum* Returned=E.Property->GetEnum();TStrongObjectPtr<UEnum> Immediate(Returned);if(!PlainCurrent())return false;if(E.Enum.IsValid()&&Immediate.Get()!=E.Enum.Get())return Bad(TEXT("pcg_metadata_operation_enum_changed"),TEXT("Captured native Operation enum binding changed"));}return true;
 }
 bool CaptureMetadataSources()
 {
  if(MetadataSourcesReady)return true;
  if(MetadataSources.IsEmpty())
  {
   for(UScriptStruct* T:{FPCGAttributePropertySelector::StaticStruct(),FPCGAttributePropertyInputSelector::StaticStruct(),FPCGAttributePropertyOutputNoSourceSelector::StaticStruct(),FPCGAttributePropertyOutputSelector::StaticStruct()})
   {
    if(!Live(T)||!W.Pin(T)||!W.Pin(T->GetPackage())||!T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||T->GetPackage()->IsDirty()||!(T->StructFlags&STRUCT_Native))return Bad(TEXT("pcg_metadata_selector_source_unavailable"),TEXT("Already-loaded native selector struct source required"));
    FStandaloneSelectorSource S;S.Struct=T;S.Super=T->GetSuperStruct();S.Flags=T->StructFlags;S.Size=T->GetStructureSize();S.Align=T->GetMinAlignment();
    const int32 Expected=T==FPCGAttributePropertySelector::StaticStruct()?sizeof(FPCGAttributePropertySelector):T==FPCGAttributePropertyInputSelector::StaticStruct()?sizeof(FPCGAttributePropertyInputSelector):T==FPCGAttributePropertyOutputNoSourceSelector::StaticStruct()?sizeof(FPCGAttributePropertyOutputNoSourceSelector):sizeof(FPCGAttributePropertyOutputSelector);if(S.Size!=Expected)return Bad(TEXT("pcg_metadata_selector_source_size"),TEXT("Compiled native selector sizeof differs from reflection"));
    for(TFieldIterator<FProperty> It(T,EFieldIterationFlags::IncludeDeprecated);It;++It){auto* P=*It;if(P->HasSetterOrGetter()||P->ArrayDim!=1)return Bad(TEXT("pcg_metadata_selector_source_layout"),TEXT("Direct scalar/native selector fields required"));S.Inventory.Add(P);S.Fields.Emplace(P);if(auto* A=CastField<FArrayProperty>(P)){if(A->GetFName()!=TEXT("ExtraNames")||!A->Inner||A->Inner->GetClass()!=FStrProperty::StaticClass()||A->Inner->GetOwner<FProperty>()!=A||A->Inner->GetSize()!=sizeof(FString)||A->Inner->ArrayDim!=1)return Bad(TEXT("pcg_metadata_selector_extra_names_layout"),TEXT("Native owned FString ExtraNames Inner required"));S.Arrays.Emplace(A);}if(auto* E=CastField<FEnumProperty>(P)){auto* Storage=E->GetUnderlyingProperty();if(!Storage||Storage->GetOwner<FProperty>()!=E||Storage->ArrayDim!=1)return Bad(TEXT("pcg_metadata_selector_enum_layout"),TEXT("Native owned selector enum storage required"));S.Enums.Emplace(E,Storage,nullptr);}}
    MetadataSources.Add(MoveTemp(S));
   }
  }
  // Complete source inventory is registered before any resolving enum observer.
  for(auto& Source:MetadataSources)for(auto& E:Source.Enums)if(!E.Enum.IsValid())
  {UEnum* Returned=E.Property->GetEnum();TStrongObjectPtr<UEnum> Hold(Returned);if(!W.Admit(TEXT("metadata_selector_enum_return")))return false;if(!Live(Hold.Get())||Hold->GetClass()!=UEnum::StaticClass()||!W.Pin(Hold.Get())||!W.Pin(Hold->GetPackage())||!Hold->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return Bad(TEXT("pcg_metadata_selector_enum_unavailable"),TEXT("Existing native selector enum source required"));E.Enum=MoveTemp(Hold);E.Flags=E.Enum->GetFlags();for(int32 I=0;I<E.Enum->NumEnums();++I)E.Names.Emplace(E.Enum->GetNameByIndex(I),E.Enum->GetValueByIndex(I));}
  MetadataSourcesReady=true;return W.Admit(TEXT("metadata_selector_sources_captured"));
 }
 bool CaptureDataSourceSources()
 {
  if(DataSourceSourcesReady)return true;if(!CaptureMetadataSources())return false;
  for(UScriptStruct* T:{FPCGActorSelectorSettings::StaticStruct(),FPCGComponentSelectorSettings::StaticStruct()})
  {
   if(!Live(T)||!W.Pin(T)||!W.Pin(T->GetPackage())||!T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||T->GetPackage()->IsDirty()||!(T->StructFlags&STRUCT_Native))return Bad(TEXT("pcg_data_source_selector_layout_pending"),TEXT("Loaded native actor/component selector source required"));
   if(MetadataSources.ContainsByPredicate([&](const FStandaloneSelectorSource& E){return E.Struct==T;}))continue;
   FStandaloneSelectorSource E;E.Struct=T;E.Super=T->GetSuperStruct();E.Flags=T->StructFlags;E.Size=T->GetStructureSize();E.Align=T->GetMinAlignment();if(E.Super||E.Size!=(T==FPCGActorSelectorSettings::StaticStruct()?sizeof(FPCGActorSelectorSettings):sizeof(FPCGComponentSelectorSettings)))return Bad(TEXT("pcg_data_source_selector_layout_pending"),TEXT("Compiled/reflected exact native selector size required"));
   for(TFieldIterator<FProperty> It(T);It;++It){auto* P=*It;if(P->ArrayDim!=1||P->HasSetterOrGetter())return false;E.Inventory.Add(P);E.Fields.Emplace(P);if(auto* C=CastField<FClassProperty>(P)){UClass* Base=T==FPCGActorSelectorSettings::StaticStruct()?AActor::StaticClass():UActorComponent::StaticClass();if(!C->MetaClass.IsResolved()||C->MetaClass!=Base||!C->PropertyClass.IsResolved()||C->PropertyClass!=UClass::StaticClass()||P->GetSize()!=sizeof(TSubclassOf<UObject>))return false;}
    else if(auto* Child=CastField<FStructProperty>(P)){if(T!=FPCGActorSelectorSettings::StaticStruct()||P->GetFName()!=TEXT("ActorReferenceSelector")||!Child->Struct.IsResolved()||Child->Struct!=FPCGAttributePropertyInputSelector::StaticStruct())return false;}
    else if(auto* Enum=CastField<FEnumProperty>(P)){auto* U=Enum->GetUnderlyingProperty();if(!U||U->GetOwner<FProperty>()!=P||U->ArrayDim!=1)return false;E.Enums.Emplace(Enum,U,nullptr);}
    else if(P->GetClass()!=FBoolProperty::StaticClass()&&P->GetClass()!=FNameProperty::StaticClass())return false;
   }MetadataSources.Add(MoveTemp(E));
  }
  // Register descriptors before the existing enum observer can dispatch callbacks.
  for(auto& E:MetadataSources)for(auto& V:E.Enums)if(!V.Enum.IsValid()){UEnum* Returned=V.Property->GetEnum();TStrongObjectPtr<UEnum> Hold(Returned);if(!W.Admit(TEXT("data_source_selector_enum_return"))||!Live(Hold.Get())||!W.Pin(Hold.Get())||!W.Pin(Hold->GetPackage())||!Hold->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return false;V.Enum=MoveTemp(Hold);V.Flags=V.Enum->GetFlags();for(int32 I=0;I<V.Enum->NumEnums();++I)V.Names.Emplace(V.Enum->GetNameByIndex(I),V.Enum->GetValueByIndex(I));}
  DataSourceSourcesReady=true;return W.Admit(TEXT("data_source_selector_sources_captured"));
 }
 bool HoldDataSourceFilterClass(UClass* First,UClass* Base)
 {
  if(!First)return true;if(!Live(First)||!Live(Base)||!W.Pin(First)||!W.Pin(Base))return false;
  // Filters may intentionally name abstract classes; no CDO is created and no
  // constructor-only nonabstract admission is reused for these references.
  bool IsChild=false;if(!W.Read(TEXT("data_source_filter_ancestry"),[&]{return First->IsChildOf(Base);},IsChild))return false;if(!IsChild)return Bad(TEXT("pcg_data_source_filter_class_mismatch"),TEXT("Requested filter class must derive from the actual Actor/ActorComponent metaclass"));
  TSet<UClass*> Chain;TArray<UPackage*> AuditPackages;
  for(UClass* C=First;C;)
  {
   if(Chain.Num()>=32||Chain.Contains(C)||!Live(C)||!W.Pin(C)||C->IsEditorOnly()||C->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists|CLASS_Interface)||Forbidden(C->GetPathName())||!C->ClassGeneratedBy.IsResolved()||!C->ClassDefaultObject.IsResolved())return Bad(TEXT("pcg_data_source_filter_source_pending"),TEXT("Loaded bounded current runtime class chain required; no class load/default creation"));Chain.Add(C);
   auto* Package=C->GetPackage();if(!Live(Package)||!W.Pin(Package)||Package->IsDirty()||Package->HasAnyPackageFlags(PKG_EditorOnly|PKG_Developer|PKG_UncookedOnly|PKG_PlayInEditor)||C->GetOuter()!=Package)return Bad(TEXT("pcg_data_source_filter_package_pending"),TEXT("Runtime native or saved-clean generated class package required"));
   const auto GeneratedBy=C->ClassGeneratedBy;const bool NativeClass=C->HasAnyClassFlags(CLASS_Native);
   UBlueprint* BP=nullptr;TObjectPtr<UClass> Generated,Skeleton,Parent;
   if(NativeClass){if(C->GetClass()!=UClass::StaticClass()||!Package->HasAnyPackageFlags(PKG_CompiledIn)||!C->GetPathName().StartsWith(TEXT("/Script/"))||GeneratedBy!=nullptr)return Bad(TEXT("pcg_data_source_filter_native_pending"),TEXT("Current exact native runtime class required"));}
   else{
    if(C->GetClass()!=UBlueprintGeneratedClass::StaticClass()||!C->HasAnyClassFlags(CLASS_CompiledFromBlueprint)||!C->GetPathName().StartsWith(TEXT("/Game/"))||GeneratedBy==nullptr)return Bad(TEXT("pcg_data_source_filter_generated_pending"),TEXT("Current saved Game generated Actor/Component filter class required"));
    const FString SourcePath=GeneratedBy.GetPathName();UObject* Source=nullptr;if(!W.Read(TEXT("data_source_filter_blueprint_lookup"),[&]{return StaticFindObject(nullptr,nullptr,*SourcePath);},Source)||!Live(Source)||GeneratedBy!=Source)return false;BP=Cast<UBlueprint>(Source);
    if(!Live(BP)||!W.Pin(BP)||BP->GetClass()!=UBlueprint::StaticClass()||BP->GetPackage()!=Package||BP->BlueprintType!=BPTYPE_Normal||(BP->Status!=BS_UpToDate&&BP->Status!=BS_UpToDateWithWarnings)||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bIsRegeneratingOnLoad||!BP->GeneratedClass.GetGCPtr().IsResolved()||!BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!BP->ParentClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr()!=C)return Bad(TEXT("pcg_data_source_filter_generated_pending"),TEXT("Existing saved-clean idle normal Blueprint source with its current generated class required"));Generated=BP->GeneratedClass.GetGCPtr();Skeleton=BP->SkeletonGeneratedClass.GetGCPtr();Parent=BP->ParentClass.GetGCPtr();AuditPackages.AddUnique(Package);
   }
   if(!DataSourceReferenceClasses.Contains(C))
   {
    // Reuse the established source inventory/hold checks, but permit abstract
    // filters and an actual nullable existing CDO instead of constructing one.
    FFreshClass Source;Source.Class=C;Source.Package=Package;Source.PackageFlags=Package->GetPackageFlags();Source.Flags=C->GetClassFlags();Source.Size=C->GetPropertiesSize();Source.CDOHandle=C->ClassDefaultObject;
    for(auto It=C->GetSuperStructIterator();It;++It){UStruct* Super=*It;if(!W.Pin(Super))return false;Source.Supers.Add(Super);}for(UStruct* Super:Source.Supers)for(TFieldIterator<FProperty> It(Super,EFieldIterationFlags::None);It;++It){Source.Inventory.Add(*It);Source.Fields.Emplace(*It);}
    if(Source.CDOHandle!=nullptr){const FString CDOPath=Source.CDOHandle.GetPathName();UObject* D=nullptr;if(!W.Read(TEXT("data_source_filter_existing_cdo_lookup"),[&]{return StaticFindObject(nullptr,nullptr,*CDOPath);},D)||!Live(D)||Source.CDOHandle!=D||D->GetClass()!=C||!D->HasAnyFlags(RF_ClassDefaultObject)||!W.Pin(D))return false;Source.CDO=D;Source.CDOFlags=D->GetFlags();}
    if(!Source.Exact())return Bad(TEXT("pcg_data_source_filter_source_changed"),TEXT("Filter source changed during its read-only enrollment"));
    if(!Held.Sources.ContainsByPredicate([&](const FFreshClass& E){return E.Class==C;}))Held.Sources.Add(MoveTemp(Source));
    const EBlueprintStatus Status=BP?BP->Status.GetValue():BS_Unknown;const EBlueprintType Type=BP?BP->BlueprintType.GetValue():BPTYPE_Normal;
    Held.ExternalAssetChecks.Add([C,GeneratedBy,BP,Generated,Skeleton,Parent,Status,Type](){if(!Live(C)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=GeneratedBy)return false;if(!BP)return GeneratedBy==nullptr;return Live(BP)&&!BP->bBeingCompiled&&!BP->bQueuedForCompilation&&!BP->bIsRegeneratingOnLoad&&BP->Status==Status&&BP->BlueprintType==Type&&BP->GeneratedClass.GetGCPtr().IsResolved()&&BP->SkeletonGeneratedClass.GetGCPtr().IsResolved()&&BP->ParentClass.GetGCPtr().IsResolved()&&BP->GeneratedClass.GetGCPtr()==Generated&&BP->SkeletonGeneratedClass.GetGCPtr()==Skeleton&&BP->ParentClass.GetGCPtr()==Parent;});DataSourceReferenceClasses.Add(C);
   }
   UClass* Super=nullptr;if(!W.Read(TEXT("data_source_filter_superclass"),[&]{return C->GetSuperClass();},Super))return false;if(BP&&Parent!=Super)return Bad(TEXT("pcg_data_source_filter_generated_super_changed"),TEXT("Generated filter class and Blueprint parent roles differ"));C=Super;
  }
  for(auto* Package:AuditPackages)if(!DataSourceReferenceAudits.Contains(Package))
  {
   if(!W.SDK(TEXT("data_source_filter_saved_dependencies")))return false;const FString Raw=::Cubeless::Operations::AuditPackageDependencies({Package->GetName()},true,4096);W.R.Data->SetStringField(TEXT("data_source_filter_dependency_audit_raw"),Raw);if(!W.After(TEXT("data_source_filter_saved_dependencies")))return false;TSharedPtr<FJsonObject> A;bool Success=false,Passed=false;if(!Parse(Raw,A,8*1024*1024)||!A->TryGetBoolField(TEXT("success"),Success)||!Success||!A->TryGetBoolField(TEXT("passed"),Passed)||!Passed)return Bad(TEXT("pcg_data_source_filter_dependency_pending"),TEXT("Saved generated filter dependencies must be completely known and independent of MCP/editor tooling"));DataSourceReferenceAudits.Add(Package);W.R.Data->SetStringField(TEXT("data_source_filter_dependency_audit_raw"),Raw);
  }
  return W.Admit(TEXT("data_source_filter_class_enrolled"));
 }
 bool HoldDataSourceFilterHandle(const TObjectPtr<UClass>& H,UClass* Base)
 {
  if(!H.IsResolved())return Bad(TEXT("pcg_data_source_filter_unresolved"),TEXT("Existing class filter must already be resolved; no resolver/load is added"));if(H==nullptr)return true;const FString Path=H.GetPathName();UClass* C=nullptr;if(!W.Read(TEXT("data_source_existing_filter_lookup"),[&]{return FindObject<UClass>(nullptr,*Path);},C)||!Live(C)||H!=C)return false;return HoldDataSourceFilterClass(C,Base);
 }
 // The original graph-spec uses recursive editor Python dict/list setters, not the raw Add/Set ImportText contract.
 bool PrepareGraphSpecDataSourceAggregate(UClass* Class,FProperty* P,const TSharedPtr<FJsonValue>& Input,FDataSourceAggregateValue& Scratch,TSharedPtr<FJsonObject>& Plan)
 {
  if(!FreshDataSourceClass(Class)||!FreshDataSourceProperty(Class,P)||!Input||!CaptureDataSourceSources())return Bad(TEXT("pcg_graph_spec_data_source_layout"),TEXT("Held native aggregate source/field and original dict/list input required"));
  Scratch.Field=P->GetFName();Plan=MakeShared<FJsonObject>();Plan->SetStringField(TEXT("native_property"),P->GetName());Plan->SetBoolField(TEXT("original_recursive_editor_setter_contract"),true);Plan->SetBoolField(TEXT("raw_import_text_used"),false);
  auto Payload=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Names;
  if(Scratch.Field==TEXT("ExpectedPins"))
  {
   const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;if(!Input->TryGetArray(Items)||!Items||Items->Num()>4096)return Bad(TEXT("pcg_graph_spec_expected_pins_list_required"),TEXT("Original ordered bounded Name list required; non-string element conversion remains incomplete"));
   Scratch.Pins.Reset();for(const auto& Item:*Items){FString Name;if(!Item||Item->Type!=EJson::String||!Item->TryGetString(Name)||Name.Len()>=NAME_SIZE||!NoEmbeddedNul(Name))return Bad(TEXT("pcg_graph_spec_expected_pin_name_pending"),TEXT("Bounded string Name elements required before native Name construction"));Scratch.Pins.Add(FName(*Name));Names.Add(MakeShared<FJsonValueString>(Name));}Plan->SetArrayField(TEXT("canonical_name_list"),Names);
  }
  else
  {
   if(!DataSourceAggregateName(Scratch.Field)||Input->Type!=EJson::Object||Input->AsObject()->Values.Num()>64)return Bad(TEXT("pcg_graph_spec_selector_dict_required"),TEXT("Original bounded recursive selector dict required; raw-text Add/Set input is a different API contract"));
   auto* Root=Scratch.Field==TEXT("ActorSelector")?FPCGActorSelectorSettings::StaticStruct():FPCGComponentSelectorSettings::StaticStruct();TSet<FProperty*> Selected;
   for(const auto& Pair:Input->AsObject()->Values)
   {
    const FString Requested(Pair.Key.ToView());if(Requested.Len()>=NAME_SIZE||!NoEmbeddedNul(Requested))return Bad(TEXT("pcg_graph_spec_selector_member_name"),TEXT("Bounded native/Python selector member name required"));FString Token=Requested.Replace(TEXT("_"),TEXT("")).ToLower();FProperty* Q=nullptr;
    for(TFieldIterator<FProperty> It(Root);It;++It){FString Native=It->GetName().ToLower();FString Alias=Native;if(CastField<FBoolProperty>(*It)&&Alias.StartsWith(TEXT("b")))Alias.RightChopInline(1);if(Token==Native||Token==Alias){Q=*It;break;}}
    if(!Q||Q->GetOwnerStruct()!=Root||Selected.Contains(Q))return Bad(TEXT("pcg_graph_spec_selector_member_pending"),TEXT("Existing direct selector member once per native field required; repeated aliases and wider fields remain incomplete"),Requested);Selected.Add(Q);
    if(PropertyAccessUtil::CanSetPropertyValue(Q,PropertyAccessUtil::EditorReadOnlyFlags,false)!=EPropertyAccessResultFlags::Success)return Bad(TEXT("pcg_graph_spec_selector_member_permission_denied"),TEXT("Original editor setter permission rejects this field; transient-only show flags are not silently skipped or raw-written"),Q->GetPathName());
    const auto& Value=Pair.Value;if(!Value)return false;void* Address=Q->ContainerPtrToValuePtr<void>(Scratch.Address());
    if(auto* Bool=CastField<FBoolProperty>(Q)){if(!Bool->IsNativeBool()||(Value->Type!=EJson::Boolean&&Value->Type!=EJson::Null))return Bad(TEXT("pcg_graph_spec_selector_bool_pending"),TEXT("Original bool/null values supported; integer Python-bool coercions remain incomplete"),Requested);const bool B=Value->Type==EJson::Boolean&&Value->AsBool();Bool->SetPropertyValue(Address,B);Payload->SetBoolField(Q->GetName(),B);}
    else if(auto* Name=CastField<FNameProperty>(Q)){FString Text;if(Value->Type==EJson::String)Text=Value->AsString();else if(Value->Type==EJson::Null)Text=TEXT("None");else if(Value->Type==EJson::Boolean)Text=Value->AsBool()?TEXT("True"):TEXT("False");else return Bad(TEXT("pcg_graph_spec_selector_name_coercion_pending"),TEXT("String/null/bool Name coercions supported; numeric Python str variants remain incomplete"),Requested);if(Text.Len()>=NAME_SIZE||!NoEmbeddedNul(Text))return false;Name->SetPropertyValue(Address,FName(*Text));Payload->SetStringField(Q->GetName(),Text);}
    else if(auto* Ref=CastField<FClassProperty>(Q)){if(Value->Type!=EJson::Null)return Bad(TEXT("pcg_graph_spec_selector_class_value_invalid"),TEXT("Original JSON graph-spec can clear a class with null; Python NativizeClass does not load path strings. Existing class references remain held and preserved."),Requested);Ref->SetObjectPropertyValue(Address,nullptr);Payload->SetField(Q->GetName(),MakeShared<FJsonValueNull>());}
    else if(auto* Enum=CastField<FEnumProperty>(Q))
    {
     if(Value->Type!=EJson::String)return Bad(TEXT("pcg_graph_spec_selector_enum_pending"),TEXT("Original coerce_value PCG enum string required; numeric enum conversion remains incomplete"),Requested);FString TokenValue=Value->AsString();int32 Dot=INDEX_NONE;if(TokenValue.FindLastChar(TEXT('.'),Dot))TokenValue=TokenValue.Mid(Dot+1);const FString Upper=TokenValue.ToUpper();
     UEnum* E=nullptr;if(!W.Read(TEXT("graph_spec_selector_enum_source"),[&]{return Enum->GetEnum();},E)||!MetadataSourcesPlain()||!E||Enum->GetUnderlyingProperty()==nullptr)return false;int64 Number=0;bool Found=false;
     for(int32 I=0;I<E->NumEnums();++I){const FString Native=E->GetNameStringByIndex(I);if(Native.EndsWith(TEXT("_MAX"))||E->HasMetaData(TEXT("Hidden"),I)||E->HasMetaData(TEXT("ScriptName"),I))continue;FString PythonName;for(int32 J=0;J<Native.Len();++J){if(J&&FChar::IsUpper(Native[J])&&(FChar::IsLower(Native[J-1])||(J+1<Native.Len()&&FChar::IsUpper(Native[J-1])&&FChar::IsLower(Native[J+1]))))PythonName+=TEXT('_');PythonName+=FChar::ToUpper(Native[J]);}if(TokenValue==PythonName||Upper==PythonName){if(Found)return false;Found=true;Number=E->GetValueByIndex(I);}}
     if(!Found)return Bad(TEXT("pcg_graph_spec_selector_enum_value_invalid"),TEXT("Original generated Python enum token required; hidden/ScriptName aliases remain incomplete"),Requested);Enum->GetUnderlyingProperty()->SetIntPropertyValue(Address,Number);Payload->SetStringField(Q->GetName(),Value->AsString());
    }
    else if(auto* Nested=CastField<FStructProperty>(Q)){if(Q->GetFName()!=TEXT("ActorReferenceSelector")||Value->Type!=EJson::Object||!Value->AsObject()->Values.IsEmpty())return Bad(TEXT("pcg_graph_spec_actor_reference_nested_pending"),TEXT("Empty nested selector dict preserves the actual selector; nonempty nested attribute/property authoring remains incomplete"),Requested);Payload->SetObjectField(Q->GetName(),MakeShared<FJsonObject>());}
    else return Bad(TEXT("pcg_graph_spec_selector_member_type_pending"),TEXT("Original first-level enum/bool/Name/null-class and empty nested dict supported"),Requested);
   }
   Plan->SetObjectField(TEXT("canonical_members"),Payload);
  }
  Scratch.RefreshClassReference();if(!W.Admit(TEXT("graph_spec_aggregate_scratch_prepared")))return false;
  if(Scratch.Field!=TEXT("ExpectedPins")&&!HoldDataSourceFilterHandle(Scratch.ClassReference(),Scratch.Field==TEXT("ActorSelector")?AActor::StaticClass():UActorComponent::StaticClass()))return false;
  Plan->SetObjectField(TEXT("expected_from_actual_or_parser_scratch"),Scratch.JsonValue());Plan->SetBoolField(TEXT("no_target_write"),true);return true;
 }
 bool PrepareDataSourceAggregate(UClass* Class,FProperty* P,const TSharedPtr<FJsonValue>& Input,FDataSourceAggregateValue& Scratch,TSharedPtr<FJsonObject>& Plan)
 {
  FString Text;if(!FreshDataSourceClass(Class)||!FreshDataSourceProperty(Class,P)||!DataSourceAggregateName(P->GetFName())||!Input||Input->Type!=EJson::String||!Input->TryGetString(Text)||!DataSourceNativeTextBounded(Text)||Forbidden(Text)||!CaptureDataSourceSources())return Bad(TEXT("pcg_data_source_aggregate_text_required"),TEXT("Original bounded native ImportText string required for ActorSelector/ComponentSelector/ExpectedPins"));
  FString ExpectedTransientSkipDiagnostics;TArray<TSharedPtr<FJsonValue>> TransientSkippedMembers;
  Scratch.Field=P->GetFName();if(Scratch.Field!=TEXT("ExpectedPins")){
   TArray<TPair<FName,FString>> Members;TArray<FString> MemberTails;if(!DataSourceTextMembers(Text,Members,MemberTails))return Bad(TEXT("pcg_data_source_aggregate_text_shape"),TEXT("Bounded native struct assignments with quoted/nested values required; repeated fields and trailing text remain incomplete"));auto* Root=Scratch.Field==TEXT("ActorSelector")?FPCGActorSelectorSettings::StaticStruct():FPCGComponentSelectorSettings::StaticStruct();
   for(int32 MemberIndex=0;MemberIndex<Members.Num();++MemberIndex){const auto& M=Members[MemberIndex];auto* Q=FindFProperty<FProperty>(Root,M.Key);if(!Q)return Bad(TEXT("pcg_data_source_aggregate_member_missing"),TEXT("Native reflected selector member not found"),M.Key.ToString());
    // PPF_None/Delimited intentionally skips these native transient UI flags (Property.cpp ShouldPort/ImportSingleProperty).
    // Match the complete SDK output, including its original input suffix; any extra diagnostic still rejects the import.
    const bool NativeShowFlag=Scratch.Field==TEXT("ActorSelector")?(M.Key==TEXT("bShowActorFilter")||M.Key==TEXT("bShowIncludeChildren")||M.Key==TEXT("bShowActorSelection")||M.Key==TEXT("bShowActorSelectionClass")||M.Key==TEXT("bShowSelectMultiple")||M.Key==TEXT("bShowIgnoreSelfAndChildren")):(M.Key==TEXT("bShowComponentSelection")||M.Key==TEXT("bShowComponentSelectionClass"));
    const auto* ShowBool=CastField<FBoolProperty>(Q);if(NativeShowFlag&&ShowBool&&ShowBool->IsNativeBool()&&Q->HasAnyPropertyFlags(CPF_Transient)&&!Q->ShouldPort(PPF_Delimited)){ExpectedTransientSkipDiagnostics+=FString::Printf(TEXT("Cannot perform text import on property '%s' here: %s"),*Q->GetName(),*MemberTails[MemberIndex]);TransientSkippedMembers.Add(MakeShared<FJsonValueString>(Q->GetName()));}
    if(auto* Ref=CastField<FClassProperty>(Q)){FString ClassText=M.Value.TrimStartAndEnd();ClassText.TrimQuotesInline();FString Path=NormalizePCGObjectPathForLoad(ClassText);if(Path.IsEmpty()||Path.Equals(TEXT("None"),ESearchCase::IgnoreCase))continue;if(!Path.StartsWith(TEXT("/Script/"))&&!Path.StartsWith(TEXT("/Game/")))return Bad(TEXT("pcg_data_source_filter_path_pending"),TEXT("Exact loaded native/Game generated class reference required; unloaded aliases remain incomplete"),Path);UClass* Wanted=nullptr;if(!W.Read(TEXT("data_source_requested_filter_lookup"),[&]{return FindObject<UClass>(nullptr,*Path);},Wanted)||!Live(Wanted)||Wanted->GetPathName()!=Path||!HoldDataSourceFilterClass(Wanted,Scratch.Field==TEXT("ActorSelector")?AActor::StaticClass():UActorComponent::StaticClass()))return Bad(TEXT("pcg_data_source_filter_unavailable"),TEXT("Requested exact loaded runtime class filter could not be enrolled"),Path);}
    else if(auto* Nested=CastField<FStructProperty>(Q)){if(Scratch.Field!=TEXT("ActorSelector")||Q->GetFName()!=TEXT("ActorReferenceSelector")||!Nested->Struct.IsResolved()||Nested->Struct!=FPCGAttributePropertyInputSelector::StaticStruct())return false;}
   }
  }
  if(!W.SDK(TEXT("data_source_aggregate_scratch_import")))return false;FStringOutputDevice Errors;const TCHAR* End=P->ImportText_Direct(*Text,Scratch.Address(),nullptr,PPF_None,&Errors);const bool Returned=End!=nullptr;const FString RawTail=End?FString(End):FString();FString Tail=RawTail;Tail.TrimStartAndEndInline();
  const bool OnlyExpectedTransientSkips=!ExpectedTransientSkipDiagnostics.IsEmpty()&&Errors.Equals(ExpectedTransientSkipDiagnostics,ESearchCase::CaseSensitive);const bool DiagnosticsAccepted=Errors.IsEmpty()||OnlyExpectedTransientSkips;
  const bool NameSlotsBounded=(Scratch.Field!=TEXT("ExpectedPins")||Scratch.Pins.Num()<=4096)&&(Scratch.Field!=TEXT("ActorSelector")||Scratch.Actor.ActorReferenceSelector.GetExtraNames().Num()<=4096);
  auto ImportObservation=MakeShared<FJsonObject>();ImportObservation->SetStringField(TEXT("field"),Scratch.Field.ToString());ImportObservation->SetBoolField(TEXT("returned_non_null"),Returned);ImportObservation->SetNumberField(TEXT("end_offset"),End?static_cast<int64>(End-*Text):-1);ImportObservation->SetStringField(TEXT("unconsumed_tail"),RawTail);ImportObservation->SetBoolField(TEXT("fully_consumed_except_whitespace"),Returned&&Tail.IsEmpty());ImportObservation->SetStringField(TEXT("sdk_output_device_text"),Errors);ImportObservation->SetStringField(TEXT("expected_transient_skip_diagnostics"),ExpectedTransientSkipDiagnostics);ImportObservation->SetArrayField(TEXT("transient_members_skipped_by_PPF_None"),TransientSkippedMembers);ImportObservation->SetBoolField(TEXT("only_expected_transient_skip_diagnostics"),OnlyExpectedTransientSkips);ImportObservation->SetBoolField(TEXT("diagnostics_accepted"),DiagnosticsAccepted);ImportObservation->SetBoolField(TEXT("name_slots_bounded"),NameSlotsBounded);ImportObservation->SetBoolField(TEXT("scratch_only_no_target_writer"),true);
  TArray<TSharedPtr<FJsonValue>> ImportObservations;const TArray<TSharedPtr<FJsonValue>>* PreviousImports=nullptr;if(W.R.Data->TryGetArrayField(TEXT("data_source_aggregate_scratch_imports"),PreviousImports)&&PreviousImports)ImportObservations=*PreviousImports;ImportObservations.Add(MakeShared<FJsonValueObject>(ImportObservation));W.R.Data->SetArrayField(TEXT("data_source_aggregate_scratch_imports"),ImportObservations);
  if(!W.After(TEXT("data_source_aggregate_scratch_import")))return false;
  if(!Returned||!Tail.IsEmpty()||!DiagnosticsAccepted||!NameSlotsBounded)return Bad(TEXT("pcg_data_source_aggregate_import_failed"),TEXT("Scratch native import failed/full consumption or current bounded slots could not be verified; target aggregate was not written"),P->GetPathName());
  Scratch.RefreshClassReference();
  if(Scratch.Field!=TEXT("ExpectedPins")&&!HoldDataSourceFilterHandle(Scratch.ClassReference(),Scratch.Field==TEXT("ActorSelector")?AActor::StaticClass():UActorComponent::StaticClass()))return false;
  Plan=MakeShared<FJsonObject>();Plan->SetStringField(TEXT("native_property"),P->GetName());Plan->SetStringField(TEXT("native_import_text"),Text);Plan->SetObjectField(TEXT("expected_from_scratch"),Scratch.JsonValue());Plan->SetBoolField(TEXT("partial_native_struct_input"),Scratch.Field!=TEXT("ExpectedPins"));Plan->SetBoolField(TEXT("scratch_only_no_target_writer"),true);return W.Admit(TEXT("data_source_aggregate_prepared"));
 }
 bool CaptureMetadata(UPCGSettings* S)
 {
  if(Metadata.ContainsByPredicate([&](const FStandaloneMetadata& E){return E.Settings==S;}))return Current();
  if(!Held.ClassSource(S->GetClass())||!CaptureMetadataSources())return false;
  if(!S->HasAnyFlags(RF_ClassDefaultObject)){const auto* Source=Held.Sources.FindByPredicate([&](const FFreshClass& C){return C.Class==S->GetClass();});if(!Source||!Source->CDO||!Live(Source->CDO)||!CaptureMetadata(static_cast<UPCGSettings*>(Source->CDO)))return false;}
  FStandaloneMetadata E;E.Settings=S;E.Copy=S->GetClass()==UPCGCopyAttributesSettings::StaticClass();TArray<FName> Names=E.Copy?TArray<FName>{TEXT("Operation"),TEXT("bCopyAllAttributes"),TEXT("bCopyAllDomains"),TEXT("MetadataDomainsMapping"),TEXT("InputSource"),TEXT("OutputTarget")}:TArray<FName>{TEXT("AttributeToRename"),TEXT("NewAttributeName")};
  for(FName Name:Names){auto* P=FreshNativeField(S->GetClass(),Name);if(!P||P->GetOwnerStruct()!=S->GetClass()||P->ArrayDim!=1||P->HasSetterOrGetter()||!P->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient))return Bad(TEXT("pcg_metadata_root_field_layout"),TEXT("Exact own public native authored root fields required"));E.Fields.Emplace(P);
   if(auto* Struct=CastField<FStructProperty>(P)){auto* Expected=Name==TEXT("OutputTarget")?FPCGAttributePropertyOutputSelector::StaticStruct():FPCGAttributePropertyInputSelector::StaticStruct();if(!Struct->Struct.IsResolved()||Struct->Struct.Get()!=Expected||Struct->GetSize()!=Expected->GetStructureSize())return Bad(TEXT("pcg_metadata_selector_property_type"),TEXT("Exact native Input/Output selector property required"));}
   else if(auto* B=CastField<FBoolProperty>(P)){if(!B->IsNativeBool()||P->GetSize()!=sizeof(bool))return Bad(TEXT("pcg_metadata_bool_type"),TEXT("Native bool storage required"));}
   else if(auto* Enum=CastField<FEnumProperty>(P)){auto* Storage=Enum->GetUnderlyingProperty();if(Name!=TEXT("Operation")||!Storage||Storage->GetClass()!=FIntProperty::StaticClass()||Storage->GetOwner<FProperty>()!=P||P->GetSize()!=sizeof(int32))return Bad(TEXT("pcg_metadata_operation_type"),TEXT("Exact native int32 Operation enum storage required"));E.OperationStorage=Storage;MetadataEnums.Emplace(Enum,Storage,nullptr);}
   else if(auto* Map=CastField<FMapProperty>(P)){if(Name!=TEXT("MetadataDomainsMapping")||!Map->KeyProp||!Map->ValueProp||Map->KeyProp->GetClass()!=FNameProperty::StaticClass()||Map->ValueProp->GetClass()!=FNameProperty::StaticClass()||Map->KeyProp->GetOwner<FProperty>()!=P||Map->ValueProp->GetOwner<FProperty>()!=P)return Bad(TEXT("pcg_metadata_domains_map_type"),TEXT("Exact native owned Name/Name map fields required"));E.Map=Map;E.MapKey=Map->KeyProp;E.MapValue=Map->ValueProp;E.MapFields.Emplace(Map->KeyProp);E.MapFields.Emplace(Map->ValueProp);}
   else if(Name!=TEXT("NewAttributeName")||P->GetClass()!=FNameProperty::StaticClass()||P->GetSize()!=sizeof(FName))return Bad(TEXT("pcg_metadata_name_type"),TEXT("Native Rename FName storage required"));
  }
  const auto SelectorBound=[](const FPCGAttributePropertySelector& V){if(V.GetExtraNames().Num()>64)return false;for(const auto& X:V.GetExtraNames())if(X.Len()>=1024||!NoEmbeddedNul(X))return false;return true;};
  if(E.Copy){auto* V=static_cast<UPCGCopyAttributesSettings*>(S);if(V->MetadataDomainsMapping.Num()>64||!SelectorBound(V->InputSource)||!SelectorBound(V->OutputTarget))return Bad(TEXT("pcg_metadata_value_budget"),TEXT("Bounded actual selector/map values required"));}else if(!SelectorBound(static_cast<UPCGMetadataRenameSettings*>(S)->AttributeToRename))return Bad(TEXT("pcg_metadata_value_budget"),TEXT("Bounded actual selector values required"));
  E.Capture();Metadata.Add(MoveTemp(E));
  for(auto& Source:MetadataEnums)if(!Source.Enum.IsValid()){UEnum* Returned=Source.Property->GetEnum();TStrongObjectPtr<UEnum> Hold(Returned);if(!W.Admit(TEXT("metadata_operation_enum_return")))return false;if(!Live(Hold.Get())||!Same(Hold->GetPathName(),TEXT("/Script/PCG.EPCGCopyAttributesOperation"))||!W.Pin(Hold.Get())||!W.Pin(Hold->GetPackage()))return Bad(TEXT("pcg_metadata_operation_enum_unavailable"),TEXT("Exact loaded Operation enum required"));Source.Enum=MoveTemp(Hold);Source.Flags=Source.Enum->GetFlags();for(int32 I=0;I<Source.Enum->NumEnums();++I)Source.Names.Emplace(Source.Enum->GetNameByIndex(I),Source.Enum->GetValueByIndex(I));}
  W.R.Data->SetBoolField(TEXT("standalone_metadata_copy_rename_profile"),true);W.R.Data->SetBoolField(TEXT("metadata_CDO_instance_values_assumed_equal"),false);return Current();
 }
 bool PrepareMetadata(UClass* Class,const TSharedPtr<FJsonObject>& Input,TSharedPtr<FJsonObject>& Plans,TSharedPtr<FJsonObject>& Aliases,UPCGSettings* ActualSettings=nullptr)
 {
  const auto* Source=Held.Sources.FindByPredicate([&](const FFreshClass& C){return C.Class==Class;});if(!Source||!Source->CDO||!CaptureMetadata(static_cast<UPCGSettings*>(Source->CDO)))return false;
  if(ActualSettings&&!CaptureMetadata(ActualSettings))return false;UPCGSettings* PlanSettings=ActualSettings?ActualSettings:static_cast<UPCGSettings*>(Source->CDO);Plans=MakeShared<FJsonObject>();Aliases=MakeShared<FJsonObject>();MetadataPlans.Reset();
  for(const auto& Pair:Input->Values){const FString Requested(Pair.Key.ToView());if(Requested.Len()>=NAME_SIZE||!NoEmbeddedNul(Requested))return Bad(TEXT("pcg_metadata_property_name_invalid"),TEXT("Bounded native property name required"));const FName Name(*Requested);const auto* Row=Metadata.FindByPredicate([&](const FStandaloneMetadata& E){return E.Settings==PlanSettings;});if(!Row)return false;const auto* Root=Row->Fields.FindByPredicate([&](const FFreshField& F){return F.Name==Name;});if(!Root)return Bad(TEXT("pcg_metadata_property_pending"),TEXT("Copy selectors/Operation/bools/Name-domain map and Rename selector/name supported; inherited fields remain incomplete"),Requested);
   FStandaloneMetadataCell Cell;Cell.Property=Root->P;Cell.Name=Name;auto Plan=MakeShared<FJsonObject>();const auto& Value=Pair.Value;TSharedPtr<FJsonValue> Expected;
   if(Root->Kind==FMapProperty::StaticClass())
   {
    FString Text;if(Name!=TEXT("MetadataDomainsMapping")||!Value||Value->Type!=EJson::String||!Value->TryGetString(Text)||!MetadataDomainsTextBounded(Text))return Bad(TEXT("pcg_metadata_domains_text_required"),TEXT("Original bounded native Name/Name map ImportText string required"),Requested);
    // Seed scratch from the actual cell, matching ImportText behavior; never parse into the target.
    Cell.Domains=static_cast<UPCGCopyAttributesSettings*>(PlanSettings)->MetadataDomainsMapping;
    if(!W.SDK(TEXT("MetadataDomains_Preflight_ImportText")))return false;FStringOutputDevice Errors;const TCHAR* End=Cell.Property->ImportText_Direct(*Text,&Cell.Domains,nullptr,PPF_None,&Errors);const FString Tail=End?FString(End).TrimStartAndEnd():FString();
    Plan->SetBoolField(TEXT("import_returned_non_null"),End!=nullptr);Plan->SetStringField(TEXT("import_diagnostics"),Errors);Plan->SetStringField(TEXT("unconsumed_tail"),Tail);Plan->SetNumberField(TEXT("scratch_pair_count"),Cell.Domains.Num());Plan->SetBoolField(TEXT("target_writer_entered"),false);W.R.Data->SetObjectField(TEXT("metadata_domains_last_scratch_import"),Plan);if(!W.After(TEXT("MetadataDomains_Preflight_ImportText")))return false;
    if(!End||!Tail.IsEmpty()||!Errors.IsEmpty()||Cell.Domains.Num()>64)return Bad(TEXT("pcg_metadata_domains_import_invalid"),TEXT("Native map import must fully consume input without diagnostics and contain at most64 pairs; target was not written"),Requested);
    Expected=MetadataDomainsJson(Cell.Domains);Plan->SetStringField(TEXT("kind"),TEXT("native_name_map_import"));Plan->SetBoolField(TEXT("scratch_only_no_target_writer"),true);
   }
   else if(Root->Kind==FStructProperty::StaticClass())
   {FString Text;if(!Value||Value->Type!=EJson::String||!Value->TryGetString(Text)||Text.Len()>=1024||!NoEmbeddedNul(Text)||Forbidden(Text))return Bad(TEXT("pcg_metadata_selector_import_required"),TEXT("Bounded original native selector ImportText string required"),Requested);Cell.IsSelector=true;
    FPCGAttributePropertyInputSelector In;FPCGAttributePropertyOutputSelector Out;if(Class==UPCGCopyAttributesSettings::StaticClass()){auto* Actual=static_cast<UPCGCopyAttributesSettings*>(PlanSettings);In=Actual->InputSource;Out=Actual->OutputTarget;}else In=static_cast<UPCGMetadataRenameSettings*>(PlanSettings)->AttributeToRename;void* Address=Name==TEXT("OutputTarget")?static_cast<void*>(&Out):static_cast<void*>(&In);if(!W.SDK(TEXT("Selector_Preflight_ImportText")))return false;const TCHAR* Imported=Cell.Property->ImportText_Direct(*Text,Address,nullptr,PPF_None);if(!W.After(TEXT("Selector_Preflight_ImportText")))return false;if(!Imported)return Bad(TEXT("pcg_metadata_selector_import_invalid"),TEXT("Installed custom selector import rejected text"),Requested);Cell.Selector=Name==TEXT("OutputTarget")?static_cast<const FPCGAttributePropertySelector&>(Out):static_cast<const FPCGAttributePropertySelector&>(In);Expected=MakeShared<FJsonValueObject>(MetadataSelectorJson(Cell.Selector));Plan->SetStringField(TEXT("kind"),TEXT("native_selector_import"));}
   else if(Root->Kind==FBoolProperty::StaticClass()){if(!Value||Value->Type!=EJson::Boolean)return Bad(TEXT("pcg_metadata_boolean_required"),TEXT("JSON boolean required"),Requested);Cell.Bool=Value->AsBool();Expected=MakeShared<FJsonValueBoolean>(Cell.Bool);Plan->SetStringField(TEXT("kind"),TEXT("bool"));}
   else if(Root->Kind==FNameProperty::StaticClass()){FString Text;if(!Value||Value->Type!=EJson::String||!Value->TryGetString(Text)||Text.Len()>=NAME_SIZE||!NoEmbeddedNul(Text))return Bad(TEXT("pcg_metadata_name_required"),TEXT("Bounded native FName string required"),Requested);Cell.NameValue=FName(*Text);Expected=MakeShared<FJsonValueString>(Cell.NameValue.ToString());Plan->SetStringField(TEXT("kind"),TEXT("name"));}
   else{const auto* Enum=MetadataEnums.FindByPredicate([&](const FStandaloneSelectorSource::FEnum& E){return E.Property==Cell.Property;});if(!Enum||!Enum->Enum.IsValid())return false;int64 Native=MIN_int64;double Number=0;FString Token;if(Value&&Value->Type==EJson::Number&&Value->TryGetNumber(Number)&&FMath::IsFinite(Number)&&Number>=MIN_int32&&Number<2147483648.0)Native=static_cast<int32>(Number);else if(Value&&Value->Type==EJson::String&&Value->TryGetString(Token)&&Token.Len()<NAME_SIZE&&NoEmbeddedNul(Token)){if(Token.IsNumeric()){if(!LexTryParseString(Native,*Token))Native=MIN_int64;}else for(const auto& E:Enum->Names){const FString Full=E.Key.ToString();FString Short=Full;if(Short.Contains(TEXT("::")))Short.Split(TEXT("::"),nullptr,&Short);if(Same(Token,Full)||Same(Token,Short)){Native=E.Value;break;}}}if(Native<MIN_int32||Native>MAX_int32)return Bad(TEXT("pcg_metadata_operation_invalid"),TEXT("Native enum token or safe int32 storage required; display-name aliases incomplete"),Requested);Cell.Integer=Native;Expected=MakeShared<FJsonValueNumber>(Native);Plan->SetStringField(TEXT("kind"),TEXT("enum_int32"));}
   Plan->SetStringField(TEXT("native_property"),Root->Name.ToString());Plan->SetField(TEXT("payload"),Value);Plan->SetField(TEXT("expected"),Expected);Plans->SetObjectField(Root->Name.ToString(),Plan);Aliases->SetField(Root->Name.ToString(),Value);MetadataPlans.Add(Cell.Property,MoveTemp(Cell));
  }
  W.R.Data->SetStringField(TEXT("metadata_selector_plan_basis"),ActualSettings?TEXT("actual target copied selector including inactive stored names"):TEXT("CDO syntax preflight only; instance actual cell is replanned before each raw setter"));W.R.Data->SetObjectField(TEXT("metadata_copy_rename_field_plan"),Plans);W.R.Data->SetBoolField(TEXT("metadata_runtime_execution_verified"),false);W.R.Data->SetBoolField(TEXT("QO_child_metadata_classes_admitted"),false);return W.Admit(TEXT("metadata_preflight_current"));
 }

 bool SubgraphRoleCurrent(FStandaloneSubgraph& E)
 {
  const bool Retired=W.Objects.ContainsByPredicate([&](const TUniquePtr<FObjectIdentity>& I){return I->Retired&&I->Object.Get()==E.Node;});if(Retired)return true;
  if(!Live(E.OwnerGraph)||E.OwnerGraph->GetClass()!=UPCGGraph::StaticClass()||!Live(E.Node)||E.Node->GetClass()!=UPCGSubgraphNode::StaticClass()||E.Node->GetOuter()!=E.OwnerGraph||!Live(E.Settings)||E.Settings->GetClass()!=UPCGSubgraphSettings::StaticClass()||E.Settings->GetOuter()!=E.Node||!FreshObjectEquals(E.Node,TEXT("SettingsInterface"),E.Settings))return Bad(TEXT("pcg_subgraph_node_role_changed"),TEXT("Exact own SubgraphNode/settings role changed"));
  for(const auto& F:E.Fields){if(FreshNativeField(static_cast<UClass*>(F.Owner),F.Name)!=F.P||!F.Exact()||F.P->HasSetterOrGetter())return Bad(TEXT("pcg_subgraph_field_binding_changed"),TEXT("Captured native reference descriptor changed"),E.Settings->GetPathName(),F.Name.ToString(),TEXT("descriptor/layout changed"));}
  if(!E.Settings->SubgraphInstance.IsResolved()||E.Settings->SubgraphInstance.Get()!=E.Instance||!E.Settings->SubgraphOverride.IsResolved()||(!E.Mutable&&E.Settings->SubgraphOverride.Get()!=E.Override)||(E.Mutable&&E.Settings->SubgraphOverride.Get()!=E.Override&&E.Settings->SubgraphOverride.Get()!=E.Planned)||!Live(E.Instance)||E.Instance->GetClass()!=UPCGGraphInstance::StaticClass()||E.Instance->GetOuter()!=E.Settings||E.Instance->GetPackage()!=E.OwnerGraph->GetPackage()||!E.Instance->Graph.IsResolved()||E.Instance->Graph.Get()!=E.InstanceGraph||E.Settings->Seed!=E.Seed||E.Settings->bEnabled!=E.Enabled||E.Settings->bDebug!=E.Debug)return Bad(TEXT("pcg_subgraph_reference_current_changed"),TEXT("Owned Instance, selected override or captured common authored cells changed"),E.Settings->GetPathName(),TEXT("captured Instance/Graph/override/common values"),TEXT("current reference/value differs"));
  const auto* Parameters=E.Instance->GetUserParametersStruct();if(!Parameters||Parameters->GetNumPropertiesInBag()!=0)return Bad(TEXT("pcg_subgraph_instance_parameters_pending"),TEXT("First owned Instance profile keeps empty parameter overrides"));return true;
 }
 bool SubgraphIOCurrent(const FStandaloneSubgraphIO& E)
 {
  if(!Live(E.Settings)||E.Settings->GetClass()!=UPCGGraphInputOutputSettings::StaticClass()||FreshNativeField(UPCGGraphInputOutputSettings::StaticClass(),TEXT("Pins"))!=E.Field)return Bad(TEXT("pcg_subgraph_child_io_binding_changed"),TEXT("Child authored IO declaration binding changed"));
  if(E.Fields.Num()!=2||!E.Fields[0].Exact()||E.Field->Inner!=E.Inner)return Bad(TEXT("pcg_subgraph_child_io_layout_changed"),TEXT("Captured native IO array layout or current Inner membership changed"));
  if(!E.Fields[1].Exact()||!E.Inner->Struct.IsResolved()||E.Inner->Struct.Get()!=FPCGPinProperties::StaticStruct()||E.Settings->IsInput()!=E.Input)return Bad(TEXT("pcg_subgraph_child_io_role_changed"),TEXT("Child IO direction or native pin declaration element changed"));
  const auto& Pins=*E.Field->ContainerPtrToValuePtr<TArray<FPCGPinProperties>>(E.Settings);if(Pins.Num()!=E.Pins.Num())return Bad(TEXT("pcg_subgraph_child_io_count_changed"),TEXT("Child authored IO declaration count changed"));
  for(int32 Index=0;Index<Pins.Num();++Index){const auto& A=Pins[Index];const auto& B=E.Pins[Index];if(!(A==B)||FTextInspector::GetSharedDataId(A.Tooltip)!=FTextInspector::GetSharedDataId(B.Tooltip)||FTextInspector::GetFlags(A.Tooltip)!=FTextInspector::GetFlags(B.Tooltip))return Bad(TEXT("pcg_subgraph_child_io_declaration_changed"),TEXT("Child authored IO declaration value changed"),E.Settings->GetPathName(),FString::Printf(TEXT("captured declaration[%d]"),Index),TEXT("current declaration differs"));}return true;
 }
 bool SubgraphsCurrent()
 {
  if(SubgraphStopped)return false;
  if(SubgraphDefaults.IsValid())
  {
   auto* C=UPCGSubgraphSettings::StaticClass();auto* D=SubgraphDefaults.Get();auto* I=SubgraphDefaultInstance.Get();
   if(!C->ClassDefaultObject.IsResolved()||C->ClassDefaultObject.Get()!=D||!Live(D)||D->GetClass()!=C||!D->SubgraphInstance.IsResolved()||D->SubgraphInstance.Get()!=I||!D->SubgraphOverride.IsResolved()||D->SubgraphOverride.Get()!=nullptr||!Live(I)||I->GetClass()!=UPCGGraphInstance::StaticClass()||I->GetOuter()!=D||!I->Graph.IsResolved()||I->Graph.Get()!=nullptr)return Bad(TEXT("pcg_subgraph_native_defaults_changed"),TEXT("Captured constructor CDO/owned default Instance/reference cells changed"));
  }
  for(auto& E:Subgraphs)if(!SubgraphRoleCurrent(E))return false;
  for(const auto& S:SubgraphSources)
  {
   if(!Live(S.Graph.Get())||S.Graph->GetClass()!=UPCGGraph::StaticClass()||S.Graph->GetOuter()!=S.Package.Get()||!Live(S.Package.Get())||S.Package->IsDirty()||S.Package->GetPackageFlags()!=S.PackageFlags)return Bad(TEXT("pcg_subgraph_child_role_changed"),TEXT("Detached and currently referenced source child roles remain protected"),S.Path);
   const auto* Params=S.Graph->GetUserParametersStruct();if(!Params||Params->GetNumPropertiesInBag()!=0)return Bad(TEXT("pcg_subgraph_child_parameters_changed"),TEXT("Captured child has no user parameter bag fields"),S.Path);
   for(const auto& IO:S.IOs)if(!SubgraphIOCurrent(IO))return false;
   if(S.Snapshot.Graph&&!FreshSnapshotCurrent(S.Snapshot))return Bad(TEXT("pcg_subgraph_child_topology_changed"),TEXT("Child captured physical graph topology/pin values changed"),S.Path);
   if(!Held.Current())return false;for(auto& E:Subgraphs)if(!SubgraphRoleCurrent(E))return false;
  }return true;
 }
 bool CaptureSubgraphDefaults()
 {
  if(SubgraphDefaults.IsValid())return Current();auto* C=UPCGSubgraphSettings::StaticClass();if(!C->ClassDefaultObject.IsResolved())return Bad(TEXT("pcg_subgraph_default_slot_pending"),TEXT("Native default slot must already be resolved"));
  auto* D=Cast<UPCGSubgraphSettings>(C->ClassDefaultObject.Get());if(!Held.KeepReturned(D,TEXT("subgraph_constructor_default"))||!Live(D)||D->GetClass()!=C||!D->HasAnyFlags(RF_ClassDefaultObject)||!D->SubgraphInstance.IsResolved()||!D->SubgraphOverride.IsResolved()||D->SubgraphOverride.Get()!=nullptr)return Bad(TEXT("pcg_subgraph_native_defaults_pending"),TEXT("Existing exact native constructor CDO with null override required; no CDO creation"));
  auto* I=D->SubgraphInstance.Get();if(!Held.KeepReturned(I,TEXT("subgraph_default_instance"))||I->GetClass()!=UPCGGraphInstance::StaticClass()||I->GetOuter()!=D||!I->Graph.IsResolved()||I->Graph.Get()!=nullptr)return Bad(TEXT("pcg_subgraph_default_instance_pending"),TEXT("Existing constructor default Instance/null Graph must be observed and held"));
  SubgraphDefaults.Reset(D);SubgraphDefaultInstance.Reset(I);return Current();
 }
 bool SubgraphFile(const FString& File,FString& Hash)
 {
  const int64 Size=IFileManager::Get().FileSize(*File);if(Size<0||Size>64ll*1024*1024)return Bad(TEXT("pcg_subgraph_source_file_pending"),TEXT("Saved child file must exist within64MiB"),File);
  TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*File)||Bytes.Num()!=Size)return Bad(TEXT("pcg_subgraph_source_file_failed"),TEXT("Actual source file read failed"),File);Hash=FreshSha256Bytes(Bytes.GetData(),Bytes.Num());return true;
 }
 bool CaptureSubgraphSource(UPCGGraph* Graph)
 {
  // Active identity is checked before completed reuse: a diamond is not a cycle.
  if(Graph==W.Graph||ActiveSubgraphSources.Contains(Graph))return Bad(TEXT("pcg_subgraph_source_cycle_pending"),TEXT("Subgraph source reaches the mutable graph or an active ancestor; no property assignment or Notify is dispatched"),TEXT("source hierarchy"),TEXT("distinct acyclic graph identities"),Graph==W.Graph?TEXT("mutable target reached"):TEXT("active source ancestor reached"));
  if(const auto* Existing=SubgraphSources.FindByPredicate([&](const FStandaloneSubgraphSource& E){return E.Graph.Get()==Graph;}))
  {if(!Existing->Ready)return Bad(TEXT("pcg_subgraph_source_capture_incomplete"),TEXT("A prior incomplete source capture cannot become a new baseline"));++SubgraphSharedReuseCount;W.R.Data->SetNumberField(TEXT("subgraph_source_shared_reuse_count"),SubgraphSharedReuseCount);return Current();}
  if(SubgraphSources.Num()>=8||!Live(Graph)||Graph->GetClass()!=UPCGGraph::StaticClass()||Graph->GetPackage()==W.Graph->GetPackage()||Graph->GetOuter()!=Graph->GetPackage()||!Graph->GetPathName().StartsWith(TEXT("/Game/"))||Forbidden(Graph->GetPathName())||Graph->GetPackage()->IsDirty()||Graph->GetNodes().Num()>64)return Bad(TEXT("pcg_subgraph_child_profile_pending"),TEXT("Each source must be a distinct loaded saved-clean exact Game graph with at most64 finite nodes; at most8 sources, no target-package source"));
  FStandaloneSubgraphSource Source;Source.Graph.Reset(Graph);Source.Package.Reset(Graph->GetPackage());Source.Path=Graph->GetPathName();Source.PackageFlags=Graph->GetPackage()->GetPackageFlags();if(!FPackageName::TryConvertLongPackageNameToFilename(Graph->GetPackage()->GetName(),Source.File,FPackageName::GetAssetPackageExtension()))return Bad(TEXT("pcg_subgraph_child_file_path_failed"),TEXT("Native source file path required"),Source.Path);
  if(!Held.KeepReturned(Graph,TEXT("subgraph_child"))||!Held.KeepReturned(Graph->GetPackage(),TEXT("subgraph_child_package")))return Bad(TEXT("pcg_subgraph_child_unavailable"),TEXT("Source must be held before observers"));
  SubgraphSources.Add(MoveTemp(Source));const int32 At=SubgraphSources.Num()-1;
  ActiveSubgraphSources.Add(Graph);ON_SCOPE_EXIT{ActiveSubgraphSources.Remove(Graph);};SubgraphMaxDepth=FMath::Max(SubgraphMaxDepth,ActiveSubgraphSources.Num());
  if(!W.Admit(TEXT("subgraph_before_source_file"))||!SubgraphFile(SubgraphSources[At].File,SubgraphSources[At].Sha256)||!W.Admit(TEXT("subgraph_after_source_file")))return false;
  FGraphSnapshot Snapshot;if(!CaptureGraph(W,Graph,Snapshot))return false;
  // Freeze the initial source topology before any descendant observer. Readiness
  // is separate so partially enrolled sources cannot be reused as completed DAGs.
  SubgraphSources[At].Snapshot=Snapshot;
  if(!W.Admit(TEXT("subgraph_source_snapshot_current")))return false;
  for(const auto& N:Snapshot.NodeSlots)
  {
   const bool IO=N.Node==Snapshot.Input||N.Node==Snapshot.Output;const bool Nested=!IO&&N.Settings&&StandaloneSubgraphClass(N.Settings->GetClass());
   if(N.Node->GetClass()!=(Nested?UPCGSubgraphNode::StaticClass():UPCGNode::StaticClass())||N.Interface!=N.Settings||N.InstanceTarget||!N.Settings||N.Settings->GetOuter()!=(IO?static_cast<UObject*>(Graph):static_cast<UObject*>(N.Node))||(IO?N.Settings->GetClass()!=UPCGGraphInputOutputSettings::StaticClass():(!StandaloneClass(N.Settings->GetClass())||StandaloneSpawnerClass(N.Settings->GetClass())||StandaloneSamplingDifferenceClass(N.Settings->GetClass())||StandaloneSpatialCompositionClass(N.Settings->GetClass())||FreshDataSourceClass(N.Settings->GetClass())||StandaloneMetadataClass(N.Settings->GetClass()))))return Bad(TEXT("pcg_subgraph_child_nested_or_custom_pending"),TEXT("Child permits existing finite9/Bounds and exact owned native Subgraph roles; custom/shared settings, Spawner, spatial and metadata child profiles remain incomplete"),SubgraphSources[At].Path);
   if(!Held.ClassSource(N.Settings->GetClass()))return false;
   if(Nested){if(!Held.ClassSource(UPCGSubgraphNode::StaticClass())||!Held.ClassSource(UPCGGraphInstance::StaticClass())||!CaptureSubgraph(static_cast<UPCGSubgraphNode*>(N.Node),static_cast<UPCGSubgraphSettings*>(N.Settings),true,Graph))return false;}
   else if(!IO&&!CaptureValues(N.Settings))return false;
   if(IO){
    auto* Settings=static_cast<UPCGGraphInputOutputSettings*>(N.Settings);auto* P=CastField<FArrayProperty>(FreshNativeField(Settings->GetClass(),TEXT("Pins")));auto* Inner=P?CastField<FStructProperty>(P->Inner):nullptr;
    if(!P||P->GetClass()!=FArrayProperty::StaticClass()||P->GetOwnerStruct()!=UPCGGraphInputOutputSettings::StaticClass()||P->ArrayDim!=1||P->GetSize()!=sizeof(TArray<FPCGPinProperties>)||P->HasSetterOrGetter()||!Inner||Inner->GetClass()!=FStructProperty::StaticClass()||Inner->GetOwnerProperty()!=P||Inner->ArrayDim!=1||Inner->GetSize()!=sizeof(FPCGPinProperties)||!Inner->Struct.IsResolved()||Inner->Struct.Get()!=FPCGPinProperties::StaticStruct())return Bad(TEXT("pcg_subgraph_child_io_layout_pending"),TEXT("Native reflected Pins array and exact FPCGPinProperties element required"));
    FStandaloneSubgraphIO Captured;Captured.Settings=Settings;Captured.Field=P;Captured.Inner=Inner;Captured.Fields.Emplace(P);Captured.Fields.Emplace(Inner);Captured.Input=Settings->IsInput();const auto& Pins=*P->ContainerPtrToValuePtr<TArray<FPCGPinProperties>>(Settings);if(Pins.Num()>128)return Bad(TEXT("pcg_subgraph_child_io_budget_pending"),TEXT("Child authored IO declarations are bounded to128 per IO node"));Captured.Pins=Pins;SubgraphSources[At].IOs.Add(MoveTemp(Captured));if(!SubgraphIOCurrent(SubgraphSources[At].IOs.Last()))return false;
   }
  }
  // Enroll every source node's authored fields/IO/reference roles before
  // observing a descendant; recursive callbacks cannot become later baselines.
  if(!W.Admit(TEXT("subgraph_source_before_descendants")))return false;
  for(const auto& N:Snapshot.NodeSlots)if(StandaloneSubgraphClass(N.Settings->GetClass()))
   if(!CaptureSubgraph(static_cast<UPCGSubgraphNode*>(N.Node),static_cast<UPCGSubgraphSettings*>(N.Settings),false,Graph))return false;
  SubgraphSources[At].Ready=true;W.R.Data->SetBoolField(TEXT("subgraph_one_level_source_contract"),!Subgraphs.ContainsByPredicate([&](const FStandaloneSubgraph& E){return E.OwnerGraph!=W.Graph;}));W.R.Data->SetBoolField(TEXT("subgraph_hierarchy_source_contract"),true);W.R.Data->SetNumberField(TEXT("subgraph_source_graph_count"),SubgraphSources.Num());W.R.Data->SetNumberField(TEXT("subgraph_source_max_depth"),SubgraphMaxDepth);W.R.Data->SetNumberField(TEXT("subgraph_source_shared_reuse_count"),SubgraphSharedReuseCount);return W.Admit(TEXT("subgraph_source_captured"));
 }
 bool CaptureSubgraph(UPCGSubgraphNode* Node,UPCGSubgraphSettings* Settings,bool Bootstrap=false,UPCGGraph* OwnerGraph=nullptr)
 {
  SubgraphActive=true;if(!OwnerGraph)OwnerGraph=W.Graph;if(!Live(OwnerGraph)||OwnerGraph->GetClass()!=UPCGGraph::StaticClass())return Bad(TEXT("pcg_subgraph_owner_unavailable"),TEXT("Exact captured owner graph required before owned Subgraph fields"));
  if(auto* Existing=Subgraphs.FindByPredicate([&](const FStandaloneSubgraph& E){return E.Settings==Settings;})){if(Existing->OwnerGraph!=OwnerGraph||!SubgraphRoleCurrent(*Existing))return Bad(TEXT("pcg_subgraph_owner_changed"),TEXT("Captured Subgraph settings cannot change owner graph"));auto* Old=Existing->Override;if(!Bootstrap&&Old&&!CaptureSubgraphSource(static_cast<UPCGGraph*>(Old)))return false;return Bootstrap||Current();}
  if(Subgraphs.Num()>=16||!Live(Node)||!Live(Settings)||Node->GetClass()!=UPCGSubgraphNode::StaticClass()||Settings->GetClass()!=UPCGSubgraphSettings::StaticClass()||Node->GetOuter()!=OwnerGraph||Settings->GetOuter()!=Node||!FreshObjectEquals(Node,TEXT("SettingsInterface"),Settings)||!Settings->SubgraphInstance.IsResolved()||!Settings->SubgraphOverride.IsResolved())return Bad(TEXT("pcg_subgraph_owned_profile_pending"),TEXT("Exact owned Subgraph node/settings and resolved constructor Instance/override required"));
  auto* Instance=Settings->SubgraphInstance.Get();if(!Held.KeepReturned(Instance,TEXT("subgraph_owned_instance")))return Bad(TEXT("pcg_subgraph_instance_unavailable"),TEXT("Hold actual constructor Instance before any next observer"));
  if(Instance->GetClass()!=UPCGGraphInstance::StaticClass()||Instance->GetOuter()!=Settings||Instance->GetPackage()!=OwnerGraph->GetPackage()||!Instance->Graph.IsResolved()||Instance->Graph.Get()!=nullptr)return Bad(TEXT("pcg_subgraph_instance_graph_pending"),TEXT("First profile preserves constructor-owned exact Instance with observed null Graph; Instance graph assignment remains incomplete"));
  FStandaloneSubgraph E;E.OwnerGraph=OwnerGraph;E.Node=Node;E.Settings=Settings;E.Instance=Instance;E.InstanceGraph=nullptr;E.Override=Settings->SubgraphOverride.Get();E.Seed=Settings->Seed;E.Enabled=Settings->bEnabled;E.Debug=Settings->bDebug;
  for(const auto& Cell:TArray<TPair<UClass*,FName>>{{UPCGSubgraphSettings::StaticClass(),TEXT("SubgraphInstance")},{UPCGSubgraphSettings::StaticClass(),TEXT("SubgraphOverride")},{UPCGGraphInstance::StaticClass(),TEXT("Graph")}})
  {
   auto* P=CastField<FObjectProperty>(FreshNativeField(Cell.Key,Cell.Value));if(!P||P->GetOwnerStruct()!=Cell.Key||P->GetClass()!=FObjectProperty::StaticClass()||P->ArrayDim!=1||P->GetSize()!=sizeof(TObjectPtr<UObject>)||P->HasSetterOrGetter()||!P->PropertyClass.IsResolved()||P->PropertyClass.Get()!=(Cell.Value==TEXT("SubgraphInstance")?UPCGGraphInstance::StaticClass():UPCGGraphInterface::StaticClass()))return Bad(TEXT("pcg_subgraph_native_reference_layout_pending"),TEXT("Exact native object-reference storage/owner/type required"));
   E.Fields.Emplace(P);if(Cell.Value==TEXT("SubgraphOverride"))E.OverrideField=P;
  }
  if(E.Override){if(!Held.KeepReturned(E.Override,TEXT("subgraph_existing_override"))||E.Override->GetClass()!=UPCGGraph::StaticClass())return Bad(TEXT("pcg_subgraph_override_source_pending"),TEXT("Existing override must be an exact loaded child graph"));}
  Subgraphs.Add(MoveTemp(E));if(!SubgraphRoleCurrent(Subgraphs.Last()))return false;
  if(!Bootstrap&&Subgraphs.Last().Override&&!CaptureSubgraphSource(static_cast<UPCGGraph*>(Subgraphs.Last().Override)))return false;
  return Bootstrap||W.Admit(TEXT("subgraph_owned_roles_captured"));
 }
 bool SubgraphFinal()
 {
  if(Subgraphs.IsEmpty()&&SubgraphSources.IsEmpty())return true;if(!W.Admit(TEXT("subgraph_final_current")))return false;TArray<TSharedPtr<FJsonValue>> Rows;
  for(const auto& S:SubgraphSources){FString Actual;if(!SubgraphFile(S.File,Actual)||!W.Admit(TEXT("subgraph_source_file_final")))return false;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),S.Path);Row->SetStringField(TEXT("file"),S.File);Row->SetStringField(TEXT("before_sha256"),S.Sha256);Row->SetStringField(TEXT("after_sha256"),Actual);Row->SetBoolField(TEXT("saved_file_preserved"),Same(S.Sha256,Actual));Rows.Add(MakeShared<FJsonValueObject>(Row));if(!Same(S.Sha256,Actual))return Bad(TEXT("pcg_subgraph_child_file_changed"),TEXT("Old/new child file changed across the scoped API"),S.Path,S.Sha256,Actual);}
  W.R.Data->SetArrayField(TEXT("subgraph_source_files"),Rows);W.R.Data->SetBoolField(TEXT("subgraph_sources_preserved"),true);W.R.Data->SetBoolField(TEXT("subgraph_runtime_execution_verified"),false);return true;
 }

  bool PlainCurrent()
 {
   if(!Held.Current()||!SubgraphsCurrent()||!SpatialPinsCurrent())return false;
   for(const auto& N:W.Snapshot.NodeSlots){const bool Input=N.Node==W.Snapshot.Input,Output=N.Node==W.Snapshot.Output;if(Input||Output){if(!N.Settings||N.Settings->GetClass()!=UPCGGraphInputOutputSettings::StaticClass()||static_cast<UPCGGraphInputOutputSettings*>(N.Settings)->IsInput()!=Input)return Bad(TEXT("pcg_standalone_factory_io_changed"),TEXT("Captured native factory IO direction changed"));}}
   for(const auto& Spawner:Spawners)if(!Spawner.Role())return Bad(TEXT("pcg_standalone_spawner_role_changed"),TEXT("Stock Spawner selector/type/instanced ownership or packer role changed; no next SDK call"),Spawner.Path,TEXT("captured stock selector, own settings/package and packer roles"),TEXT("compound role check failed"));
  if(!MetadataPlain())return false;
  for(const auto& V:Values)
  {
   FProperty* ActualProperty=FreshNativeField(V.Settings->GetClass(),V.Property->GetFName());
   if(ActualProperty!=V.Property)return Bad(TEXT("pcg_standalone_setting_changed"),TEXT("Captured finite authored field binding changed"),V.Path,FString::Printf(TEXT("layout.property=%p"),static_cast<void*>(V.Property)),FString::Printf(TEXT("layout.property=%p"),static_cast<void*>(ActualProperty)));
   if(!V.SameType())return Bad(TEXT("pcg_standalone_setting_changed"),TEXT("Captured finite authored field layout changed"),V.Path,FString::Printf(TEXT("layout.finite_authored_value=true,size=%d"),V.PointArray?static_cast<int32>(sizeof(TArray<FPCGPoint>)):(CastField<FNameProperty>(V.Property)?static_cast<int32>(sizeof(FName)):V.Bytes.Num())),FString::Printf(TEXT("layout.finite_authored_value=false,size=%d"),V.Property->GetSize()));
   if(!V.EnumCurrent())return Bad(TEXT("pcg_standalone_setting_changed"),TEXT("Captured finite authored enum flags/name/value inventory changed"),V.Path,FString::Printf(TEXT("enum.inventory_unchanged=true,flags=%u,count=%d"),static_cast<uint32>(V.EnumFlags),V.EnumNames.Num()),FString::Printf(TEXT("enum.inventory_unchanged=false,flags=%u,count=%d"),static_cast<uint32>(V.Enum->GetFlags()),V.Enum->NumEnums()));
   if((V.Settings!=MutableSettings||V.Property!=MutableField)&&!V.SameValue())
   {
    if(V.PointArray)return Bad(TEXT("pcg_standalone_setting_changed"),TEXT("Point array values/order/count changed outside selected write; storage/padding are not compared"),V.Path,FString::Printf(TEXT("point_count=%d;semantic_equal=true"),V.Points.Num()),FString::Printf(TEXT("point_count=%d;semantic_equal=false"),FScriptArrayHelper(V.PointArray,V.Property->ContainerPtrToValuePtr<void>(V.Settings)).Num()));
    if(CastField<FNameProperty>(V.Property))return Bad(TEXT("pcg_standalone_setting_changed"),TEXT("FName identity or preserved spelling changed outside selected write"),V.Path,TEXT("same captured case-sensitive FName"),TEXT("different FName"));
    FString ExpectedBytes,ActualBytes;const uint8* Actual=V.Property->ContainerPtrToValuePtr<uint8>(V.Settings);
    for(int32 I=0;I<V.Bytes.Num();++I){ExpectedBytes+=FString::Printf(TEXT("%02X"),static_cast<uint32>(V.Bytes[I]));ActualBytes+=FString::Printf(TEXT("%02X"),static_cast<uint32>(Actual[I]));}
    return Bad(TEXT("pcg_standalone_setting_changed"),TEXT("Captured finite authored POD value changed outside the selected planned write"),V.Path,TEXT("value.pod_hex=")+ExpectedBytes,TEXT("value.pod_hex=")+ActualBytes);
   }
  }
  // Later enum handle reads above may invoke tracking callbacks; close the new authored array again.
  return SpatialPinsCurrent();
 }
  bool CurrentScoped()
  {
   if(!PlainCurrent())return false;
   for(const auto& Spawner:Spawners){const FName Selected=Spawner.Settings==MutableSettings&&MutableField?MutableField->GetFName():NAME_None;const bool Unchanged=Spawner.SameValue(Selected);if(!PlainCurrent())return false;if(!Unchanged)return Bad(TEXT("pcg_standalone_spawner_value_changed"),TEXT("Captured stock Spawner/selector authored values changed outside an owned writer; no baseline adoption"),Spawner.Path,TEXT("captured native authored Spawner fields and selector semantic values"),TEXT("compound semantic comparison failed"));}
  // GetEnum resolves a TObjectPtr. The no-query current check must run before
  // following another physical field after each returned enum handle.
  for(const auto& V:Values)if(V.Enum){UEnum* Returned=static_cast<FEnumProperty*>(V.Property)->GetEnum();TStrongObjectPtr<UEnum> Keep(Safe()&&Live(Returned)?Returned:nullptr);if(!PlainCurrent())return false;if(Keep.Get()!=V.Enum)return Bad(TEXT("pcg_standalone_enum_source_changed"),TEXT("Resolved native enum child changed across its handle read"));}
  if(!MetadataEnumHandles())return false;return SpatialPinsCurrent();
 }
  bool Current(){if(SubgraphStopped)return false;const bool Good=CurrentScoped();if(!Good&&SubgraphActive){SubgraphStopped=true;W.R.bChangeStateKnown=false;W.R.Data->SetBoolField(TEXT("subgraph_unknown_stopped"),true);}return Good;}
  bool CaptureSpawner(UPCGStaticMeshSpawnerSettings* Settings,bool Bootstrap=false)
  {
   if(const auto* Existing=Spawners.FindByPredicate([&](const FStandaloneSpawner& Value){return Value.Settings==Settings;}))return Bootstrap||(Held.ClassSource(Existing->Type)&&Current());
   if(!Live(Settings)||Settings->GetOuter()==nullptr||Settings->GetPackage()!=W.Graph->GetPackage()||!Settings->MeshSelectorParameters.IsResolved()||!Settings->MeshSelectorType.GetGCPtr().IsResolved())return Bad(TEXT("pcg_standalone_spawner_slot_unresolved"),TEXT("Exact live graph-owned Spawner requires already-resolved selector handles"));
   UPCGMeshSelectorBase* Selector=Settings->MeshSelectorParameters.Get();if(!Held.KeepReturned(Selector,TEXT("standalone_spawner_selector")))return Bad(TEXT("pcg_standalone_spawner_selector_unavailable"),TEXT("Stock settings-owned selector must be held before any next SDK observation"));
   UClass* Type=Selector->GetClass();if((Type!=UPCGMeshSelectorWeighted::StaticClass()&&Type!=UPCGMeshSelectorByAttribute::StaticClass())||Settings->MeshSelectorType.GetGCPtr()!=Type||Selector->GetOuter()!=Settings||Selector->GetPackage()!=Settings->GetPackage()||Selector->GetOutermost()!=Settings->GetOutermost()||Selector->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!Settings->InstanceDataPackerParameters.IsResolved()||Settings->InstanceDataPackerParameters!=nullptr||!Settings->InstanceDataPackerType.GetGCPtr().IsResolved())return Bad(TEXT("pcg_standalone_spawner_stock_scope_unsupported"),TEXT("Only exact stock Weighted/ByAttribute owned selector instances with no instanced packer are enrolled; custom/source selector is not adopted"),Settings->GetPathName());
   // Bound copied processing uses the existing request budget. No disk scans
   // or ExportText/loader queries are added to every graph/pin current check.
   int64 Count=Settings->PostProcessFunctionNames.Num()+static_cast<int64>(Settings->StaticMeshComponentPropertyOverrides.Num());
   if(const auto* Weighted=Cast<UPCGMeshSelectorWeighted>(Selector))Count+=Weighted->MeshEntries.Num()+static_cast<int64>(Weighted->MaterialOverrideAttributes.Num());
   if(const auto* ByAttribute=Cast<UPCGMeshSelectorByAttribute>(Selector))Count+=ByAttribute->MaterialOverrideAttributes.Num();
   if(!W.CaptureBudget(Count))return false;
   FStandaloneSpawner Captured;Captured.Settings=Settings;Captured.Selector=Selector;Captured.Type=Type;if(!Captured.Capture())return Bad(TEXT("pcg_standalone_spawner_capture_failed"),TEXT("Exact native Spawner authored layout/semantic values could not be captured"),Settings->GetPathName());
   Spawners.Add(MoveTemp(Captured));auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("settings_path"),Settings->GetPathName());Row->SetStringField(TEXT("selector_path"),Selector->GetPathName());Row->SetStringField(TEXT("selector_class"),Type->GetPathName());Row->SetBoolField(TEXT("settings_owned_instance"),true);Row->SetBoolField(TEXT("inherited_authored_values_guarded"),true);Row->SetBoolField(TEXT("compiler_or_UI_temporary_state_frozen"),false);Row->SetBoolField(TEXT("deep_source_default_or_global_purity_proven"),false);Row->SetBoolField(TEXT("source_CDO_selector_presence_required"),false);Row->SetBoolField(TEXT("custom_selector_or_packer_supported"),false);Row->SetBoolField(TEXT("runtime_generation_verified"),false);
   TArray<TSharedPtr<FJsonValue>> Rows;const TArray<TSharedPtr<FJsonValue>>* OldRows=nullptr;if(W.R.Data->TryGetArrayField(TEXT("standalone_spawner_enrollments"),OldRows))Rows=*OldRows;Rows.Add(MakeShared<FJsonValueObject>(Row));W.R.Data->SetArrayField(TEXT("standalone_spawner_enrollments"),Rows);
   return Bootstrap||(W.Admit(TEXT("standalone_spawner_selector_held"))&&Held.ClassSource(Type)&&W.Admit(TEXT("standalone_stock_spawner_enrolled")));
  }
 TSet<UScriptStruct*> RawStructBound;
 bool BindRawStructSource(UClass* Class,FProperty* P)
 {
  const bool Sampling=RawSamplingProperty(Class,P);if(!Sampling&&!RawVectorProperty(Class,P))return Bad(TEXT("pcg_raw_struct_property_pending"),TEXT("Exact existing finite Vector or GetLandscape SamplingProperties field required"));
  auto* Root=static_cast<FStructProperty*>(P);auto* T=Sampling?FPCGLandscapeDataProps::StaticStruct():TBaseStructure<FVector>::Get();if(RawStructBound.Contains(T))return Current();
  if(!W.Pin(T)||!W.Pin(T->GetPackage())||!T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||T->GetPackage()->IsDirty()||T->GetSuperStruct()!=nullptr||!(T->StructFlags&STRUCT_Native)||T->GetStructureSize()!=(Sampling?sizeof(FPCGLandscapeDataProps):sizeof(FVector))||T->GetMinAlignment()!=(Sampling?alignof(FPCGLandscapeDataProps):alignof(FVector)))return Bad(TEXT("pcg_raw_struct_source_pending"),TEXT("Resident exact compiled native struct source/layout required"));
  auto Source=MakeShared<FStandaloneSelectorSource>();Source->Struct=T;Source->Super=T->GetSuperStruct();Source->Flags=T->StructFlags;Source->Size=T->GetStructureSize();Source->Align=T->GetMinAlignment();int32 Count=0;
  for(TFieldIterator<FProperty> It(T,EFieldIterationFlags::IncludeDeprecated);It;++It){auto* Q=*It;Source->Inventory.Add(Q);Source->Fields.Emplace(Q);if(Q->GetOwnerStruct()!=T||Q->ArrayDim!=1||Q->HasSetterOrGetter()||Q->GetOffset_ForInternal()<0||Q->GetSize()>Source->Size-Q->GetOffset_ForInternal())return false;
   bool Named=false;if(Sampling){for(const TCHAR* N:LandscapeSamplingNames)Named|=Q->GetFName()==N;const auto* B=CastField<FBoolProperty>(Q);if(!Named||!B||!B->IsNativeBool()||Q->GetSize()!=sizeof(bool)||!Q->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||Q->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient))return false;}
   else if((Q->GetFName()!=TEXT("X")&&Q->GetFName()!=TEXT("Y")&&Q->GetFName()!=TEXT("Z"))||Q->GetClass()!=FDoubleProperty::StaticClass()||Q->GetSize()!=sizeof(double))return false;++Count;
  }
  if(Count!=(Sampling?7:3)||!Source->Current())return false;auto* Package=T->GetPackage();const auto Flags=T->GetFlags();const auto PackageFlags=Package->GetPackageFlags();Held.ExternalAssetChecks.Add([Source,Package,Flags,PackageFlags](){return Source->Current()&&Live(Package)&&Source->Struct->GetPackage()==Package&&Source->Struct->GetFlags()==Flags&&!Package->IsDirty()&&Package->GetPackageFlags()==PackageFlags;});RawStructBound.Add(T);return Current();
 }
 bool PrepareRawStruct(UClass* Class,FProperty* P,const TSharedPtr<FJsonValue>& Input,FRawNativeStructValue& Scratch,TSharedPtr<FJsonObject>& Plan,UPCGSettings* Actual=nullptr)
 {
  FString Text;if(!Input||Input->Type!=EJson::String||!Input->TryGetString(Text)||Text.Len()>65536||!DataSourceNativeTextBounded(Text)||Forbidden(Text)||!BindRawStructSource(Class,P))return Bad(TEXT("pcg_raw_struct_text_required"),TEXT("Bounded original native struct ImportText string and resident native field/source required"));
  const auto* Source=Held.Sources.FindByPredicate([&](const FFreshClass& C){return C.Class==Class;});UPCGSettings* From=Actual?Actual:Source?static_cast<UPCGSettings*>(Source->CDO):nullptr;if(!Live(From)||From->GetClass()!=Class||FreshNativeField(Class,P->GetFName())!=P||!CaptureValues(From)||!Scratch.Read(From,P))return Bad(TEXT("pcg_raw_struct_baseline_pending"),TEXT("Actual target or syntax-preflight CDO copied native value required"));
  Plan=MakeShared<FJsonObject>();Plan->SetStringField(TEXT("native_property"),P->GetName());Plan->SetStringField(TEXT("plan_basis"),Actual?TEXT("actual_instance_copy"):TEXT("CDO_syntax_preflight_only"));Plan->SetObjectField(TEXT("before_scratch"),Scratch.JsonValue());
  if(!W.SDK(TEXT("RawStruct_Preflight_ImportText")))return false;FStringOutputDevice Errors;const TCHAR* End=P->ImportText_Direct(*Text,Scratch.Address(),nullptr,PPF_None,&Errors);const FString Tail=End?FString(End).TrimStartAndEnd():FString();Plan->SetBoolField(TEXT("import_returned_non_null"),End!=nullptr);Plan->SetStringField(TEXT("import_diagnostics"),Errors);Plan->SetStringField(TEXT("unconsumed_tail"),Tail);Plan->SetBoolField(TEXT("scratch_valid"),Scratch.Valid());Plan->SetBoolField(TEXT("target_writer_entered"),false);if(Scratch.Valid())Plan->SetObjectField(TEXT("expected"),Scratch.JsonValue());W.R.Data->SetObjectField(TEXT("raw_struct_last_scratch_import"),Plan);if(!W.After(TEXT("RawStruct_Preflight_ImportText")))return false;
  if(!End||!Tail.IsEmpty()||!Errors.IsEmpty()||!Scratch.Valid())return Bad(TEXT("pcg_raw_struct_import_invalid"),TEXT("Native scratch import must return a fully consumed diagnostic-free finite value; target writer was not entered"),P->GetPathName());return Current();
 }
 bool CaptureValues(UPCGSettings* S,bool AggregatesOnly=false)
 {
  if(StandaloneMetadataClass(S->GetClass()))return CaptureMetadata(S);
  // QR defaults are source values, not instance constructor expectations.
  // SurfaceSampler.cpp397-402 intentionally changes instance PointSteepness.
  if((StandaloneSamplingDifferenceClass(S->GetClass())||StandaloneSpatialCompositionClass(S->GetClass()))&&!S->HasAnyFlags(RF_ClassDefaultObject))
  {
   if(!Held.ClassSource(S->GetClass()))return false;const auto* Source=Held.Sources.FindByPredicate([&](const FFreshClass& C){return C.Class==S->GetClass();});
   if(!Source||!Source->CDO||!Live(Source->CDO)||Source->CDO->GetClass()!=S->GetClass()||!CaptureValues(static_cast<UPCGSettings*>(Source->CDO)))return false;
  }
   if(StandaloneSubgraphClass(S->GetClass())){auto* N=Cast<UPCGSubgraphNode>(S->GetOuter());return N&&CaptureSubgraph(N,static_cast<UPCGSubgraphSettings*>(S));}
   const bool Spawner=StandaloneSpawnerClass(S->GetClass());if(Spawner&&!CaptureSpawner(static_cast<UPCGStaticMeshSpawnerSettings*>(S)))return false;
  if(!CaptureSpatialPins(S))return false;
  if(Values.ContainsByPredicate([&](const FStandaloneValue& V){return V.Settings==S;}))return Current();
  if(FreshDataSourceClass(S->GetClass()))
  {
   const int32 AggregateStart=Values.Num();
   for(const auto& Field:StandaloneFields)if(S->GetClass()->GetFName()==Field.Class&&(DataSourceAggregateName(FName(Field.Native))||FName(Field.Native)==TEXT("SamplingProperties")))
   {
    FStandaloneValue V;V.Settings=S;V.Property=FreshNativeField(S->GetClass(),Field.Native);V.Path=S->GetPathName()+TEXT(":")+Field.Native;
    if(!V.Property||!FreshDataSourceProperty(S->GetClass(),V.Property))return Bad(TEXT("pcg_data_source_aggregate_layout_pending"),TEXT("Exact original native selector/array or Landscape sampling property required"),V.Path);if(RawSamplingProperty(S->GetClass(),V.Property)){if(!BindRawStructSource(S->GetClass(),V.Property))return false;V.Sampling=MakeShared<FRawNativeStructValue>();if(!V.Capture()||!V.SameType())return false;Values.Add(MoveTemp(V));continue;}V.Aggregate=MakeShared<FDataSourceAggregateValue>();if(auto* A=CastField<FArrayProperty>(V.Property))V.AggregateInner=MakeShared<FFreshField>(A->Inner);if(!V.Capture()||!V.SameType())return false;Values.Add(MoveTemp(V));
   }
   if(!CaptureDataSourceSources())return false;
   for(int32 I=AggregateStart;I<Values.Num();++I){auto& V=Values[I];if(!V.Sampling&&V.Property->GetFName()!=TEXT("ExpectedPins")&&!HoldDataSourceFilterHandle(V.Aggregate->ClassReference(),V.Property->GetFName()==TEXT("ActorSelector")?AActor::StaticClass():UActorComponent::StaticClass()))return false;}
   if(!Current())return false;if(AggregatesOnly)return true;
  }
  const FStandaloneField* Fields=Spawner?StandaloneSpawnerFields:StandaloneFields;const int32 FieldCount=Spawner?UE_ARRAY_COUNT(StandaloneSpawnerFields):UE_ARRAY_COUNT(StandaloneFields);
  for(int32 FieldIndex=0;FieldIndex<FieldCount;++FieldIndex){const auto& F=Fields[FieldIndex];if(S->GetClass()->GetFName()!=F.Class)continue;
   FStandaloneValue V;V.Settings=S;V.Property=FreshNativeField(S->GetClass(),F.Native);V.Path=S->GetPathName()+TEXT(":")+F.Native;
   if(!V.Property||V.Property->ArrayDim!=1||V.Property->HasSetterOrGetter()||!V.Property->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||(V.Property->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated)||(V.Property->HasAnyPropertyFlags(CPF_Transient)&&!CubelessPCGUE58::IsDensityFilterEditorDebugProperty(S->GetClass(),V.Property)&&!StandaloneSamplingTransient(S->GetClass(),V.Property))))return Bad(TEXT("pcg_standalone_field_unsupported"),TEXT("Exact finite authored native field required"));
   if(FreshDataSourceClass(S->GetClass())&&(DataSourceAggregateName(V.Property->GetFName())||RawSamplingProperty(S->GetClass(),V.Property)))continue;
   if(auto* A=CastField<FArrayProperty>(V.Property))
   {
    V.PointArray=A;V.PointInner=CastField<FStructProperty>(A->Inner);if(!V.PointInner||V.PointInner->GetOwner<FProperty>()!=A||!V.PointInner->Struct.IsResolved())return Bad(TEXT("pcg_standalone_point_inner_invalid"),TEXT("Exact resolved point array child/owner required"),V.Path);
    V.Struct=V.PointInner->Struct.Get();V.PointInnerFlags=V.PointInner->GetPropertyFlags();V.PointInnerOffset=V.PointInner->GetOffset_ForInternal();TArray<UScriptStruct*> Pending{V.Struct};TSet<UScriptStruct*> Seen;
    while(!Pending.IsEmpty())
    {
     UScriptStruct* T=Pending.Pop();if(Seen.Contains(T))continue;Seen.Add(T);if(!Live(T)||!W.Pin(T))return false;const FString TypePath=T->GetPathName();int32 ExpectedSize=0;
     if(Same(TypePath,TEXT("/Script/PCG.PCGPoint")))ExpectedSize=sizeof(FPCGPoint);else if(Same(TypePath,TEXT("/Script/CoreUObject.Transform")))ExpectedSize=sizeof(FTransform);else if(Same(TypePath,TEXT("/Script/CoreUObject.Vector")))ExpectedSize=sizeof(FVector);else if(Same(TypePath,TEXT("/Script/CoreUObject.Vector4")))ExpectedSize=sizeof(FVector4);else if(Same(TypePath,TEXT("/Script/CoreUObject.Quat")))ExpectedSize=sizeof(FQuat);
     if(!ExpectedSize||!(T->StructFlags&STRUCT_Native)||T->GetSuperStruct()!=nullptr||T->GetStructureSize()!=ExpectedSize||!W.Pin(T->GetPackage())||!T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn)||T->GetPackage()->IsDirty())return Bad(TEXT("pcg_standalone_point_source_invalid"),TEXT("Exact finite native point/math source layout required"),TypePath);
     FStandalonePointLayout L;L.Struct=T;L.Path=TypePath;L.Package=T->GetPackage();L.PackageFlags=L.Package->GetPackageFlags();L.ObjectFlags=T->GetFlags();L.Flags=T->StructFlags;L.Size=T->GetStructureSize();L.Alignment=T->GetMinAlignment();
     for(TFieldIterator<FProperty> It(T);It;++It){if(It->HasSetterOrGetter())return false;L.Inventory.Add(*It);L.Fields.Emplace(*It);if(auto* Child=CastField<FStructProperty>(*It)){if(!Child->Struct.IsResolved())return false;L.Nested.Emplace(Child,Child->Struct);Pending.Add(Child->Struct.Get());}}
     V.PointLayouts.Add(MoveTemp(L));
    }
   }
   if(auto* P=CastField<FStructProperty>(V.Property)){if(!P->Struct.IsResolved()||P->Struct!=TBaseStructure<FVector>::Get())return Bad(TEXT("pcg_standalone_vector_source"),TEXT("Exact resolved native Vector field required"),V.Path);V.Struct=TBaseStructure<FVector>::Get();if(!W.Pin(V.Struct)||!W.Pin(V.Struct->GetPackage()))return false;}
   if(auto* P=CastField<FEnumProperty>(V.Property)){UEnum* Returned=P->GetEnum();TStrongObjectPtr<UEnum> Hold(Safe()&&Live(Returned)?Returned:nullptr);if(!W.Admit(TEXT("standalone_enum_return"))||!Hold.IsValid()||!W.Pin(Hold.Get())||!W.Pin(Hold->GetPackage()))return false;V.Enum=Hold.Get();V.EnumStorage=P->GetUnderlyingProperty();V.EnumFlags=V.Enum->GetFlags();for(int32 I=0;I<V.Enum->NumEnums();++I)V.EnumNames.Emplace(V.Enum->GetNameByIndex(I),V.Enum->GetValueByIndex(I));}
   if(!V.SameType())return Bad(TEXT("pcg_standalone_field_layout"),TEXT("Finite POD/FName/native point-array storage differs"));if(!V.Capture())return Bad(TEXT("pcg_standalone_point_count_unsupported"),TEXT("Point array exceeds existing finite 128-element bound"),V.Path);Values.Add(MoveTemp(V));
  }
  return Current();
 }
 bool PrepareSampling(UClass* Class,const TSharedPtr<FJsonObject>& Input,TSharedPtr<FJsonObject>& Plans,TSharedPtr<FJsonObject>& Aliases)
 {
  const auto* Source=Held.Sources.FindByPredicate([&](const FFreshClass& C){return C.Class==Class;});
  if(!Source||!Source->CDO||!Live(Source->CDO)||Source->CDO->GetClass()!=Class||!CaptureValues(static_cast<UPCGSettings*>(Source->CDO)))return false;
  auto* Defaults=static_cast<UPCGSettings*>(Source->CDO);Plans=MakeShared<FJsonObject>();Aliases=MakeShared<FJsonObject>();
  for(const auto& Pair:Input->Values)
  {
   const FString Requested(Pair.Key.ToView());if(Requested.Len()>=NAME_SIZE||!NoEmbeddedNul(Requested))return Bad(TEXT("pcg_sampling_property_name_invalid"),TEXT("Bounded original native property name required before FName construction"));
   const auto* Field=StandaloneField(Class,FName(*Requested));if(!Field)return Bad(TEXT("pcg_sampling_property_pending"),TEXT("Only SurfaceSampler8/Difference4 and Union2/InnerIntersection2/OuterIntersection3 own public fields supported; deprecated/inherited fields remain incomplete"),Requested);
   FProperty* P=FreshNativeField(Class,Field->Native);const auto* Captured=Values.FindByPredicate([&](const FStandaloneValue& V){return V.Settings==Defaults&&V.Property==P;});
   if(!Captured||!Captured->SameType())return Bad(TEXT("pcg_sampling_property_layout_changed"),TEXT("Held direct native field/type/enum storage required"),Requested);
   const auto& Value=Pair.Value;auto Plan=MakeShared<FJsonObject>();TSharedPtr<FJsonValue> Expected;
   if(CastField<FBoolProperty>(P)){bool B=false;if(!Value||Value->Type!=EJson::Boolean||!Value->TryGetBool(B))return Bad(TEXT("pcg_sampling_boolean_required"),TEXT("Typed slice requires JSON boolean"),Requested);Expected=MakeShared<FJsonValueBoolean>(B);Plan->SetStringField(TEXT("kind"),TEXT("bool"));}
   else if(CastField<FFloatProperty>(P)){double N=0;if(!Value||Value->Type!=EJson::Number||!Value->TryGetNumber(N)||!FMath::IsFinite(N)||FMath::Abs(N)>static_cast<double>(MAX_flt))return Bad(TEXT("pcg_sampling_float_range"),TEXT("Finite JSON number within native float32 required; UI clamps are not synthesized"),Requested);Expected=MakeShared<FJsonValueNumber>(static_cast<float>(N));Plan->SetStringField(TEXT("kind"),TEXT("float32"));}
   else if(CastField<FStructProperty>(P))
   {
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(!Value||!Value->TryGetArray(A)||A->Num()!=3)return Bad(TEXT("pcg_sampling_vector_required"),TEXT("PointExtents requires exactly three finite numeric Vector components"),Requested);
    TArray<TSharedPtr<FJsonValue>> XYZ;for(const auto& Component:*A){double N=0;if(!Component||Component->Type!=EJson::Number||!Component->TryGetNumber(N)||!FMath::IsFinite(N))return Bad(TEXT("pcg_sampling_vector_nonfinite"),TEXT("Finite native double Vector components required"),Requested);XYZ.Add(MakeShared<FJsonValueNumber>(N));}Expected=MakeShared<FJsonValueArray>(XYZ);Plan->SetStringField(TEXT("kind"),TEXT("Vector"));
   }
   else
   {
    int64 Native=INDEX_NONE;double N=0;FString Token;
    if(Value&&Value->Type==EJson::Number&&Value->TryGetNumber(N)&&FMath::IsFinite(N)&&N>=0&&N<256)Native=static_cast<int64>(N);
    else if(Value&&Value->Type==EJson::String&&Value->TryGetString(Token)&&Token.Len()<NAME_SIZE&&NoEmbeddedNul(Token))
    {
     if(Token.IsNumeric()){if(!LexTryParseString(Native,*Token))Native=INDEX_NONE;}
     else for(const auto& Entry:Captured->EnumNames){const FString Full=Entry.Key.ToString();FString Short=Full;if(Short.Contains(TEXT("::")))Short.Split(TEXT("::"),nullptr,&Short);if(Same(Token,Full)||Same(Token,Short)){Native=Entry.Value;break;}}
    }
    if(Native<0||Native>255)return Bad(TEXT("pcg_sampling_enum_value_invalid"),TEXT("Native token/scoped token or numeric uint8 storage required; display aliases incomplete"),Requested);
    Expected=MakeShared<FJsonValueNumber>(Native);Plan->SetStringField(TEXT("kind"),TEXT("enum_uint8"));Plan->SetStringField(TEXT("enum_path"),Captured->Enum->GetPathName());
   }
   Plan->SetStringField(TEXT("native_property"),Field->Native);Plan->SetField(TEXT("payload"),Value);Plan->SetField(TEXT("expected"),Expected);Plan->SetBoolField(TEXT("transient_editor_state"),StandaloneSamplingTransient(Class,P));Plan->SetBoolField(TEXT("serialized_persistence_verified"),false);Plans->SetObjectField(Field->Alias,Plan);Aliases->SetField(Field->Alias,Value);
  }
  W.R.Data->SetBoolField(TEXT("standalone_sampling_difference_profile"),true);W.R.Data->SetObjectField(TEXT("sampling_difference_field_plan"),Plans);W.R.Data->SetBoolField(TEXT("sampling_runtime_execution_verified"),false);W.R.Data->SetBoolField(TEXT("QO_child_sampling_difference_admitted"),false);return W.Admit(TEXT("sampling_difference_preflight_current"));
 }
 bool PrepareSpawner(const TSharedPtr<FJsonObject>& Input,TSharedPtr<FJsonObject>& Plans,TSharedPtr<FJsonObject>& Aliases)
 {
  Plans=MakeShared<FJsonObject>();Aliases=MakeShared<FJsonObject>();
  for(const auto& Pair:Input->Values){const FString Requested(Pair.Key.ToView());if(Requested.Len()>=NAME_SIZE||!NoEmbeddedNul(Requested))return Bad(TEXT("pcg_standalone_spawner_property_name_invalid"),TEXT("Bounded original native property name required before FName construction"));
   const auto* Field=StandaloneSpawnerField(FName(*Requested));if(!Field)return Bad(TEXT("pcg_standalone_spawner_property_pending"),TEXT("This stock Spawner writer supports Seed, bEnabled, six native general bools and OutAttributeName; other original fields remain incomplete"),Requested);
   FProperty* Property=FreshNativeField(UPCGStaticMeshSpawnerSettings::StaticClass(),Field->Native);if(!StandaloneSpawnerProperty(Property))return Bad(TEXT("pcg_standalone_spawner_property_layout_changed"),TEXT("Exact native authored owner/type/storage required"),Requested);
   TSharedPtr<FJsonValue> Expected;const auto& Value=Pair.Value;auto Plan=MakeShared<FJsonObject>();Plan->SetStringField(TEXT("native_property"),Field->Native);Plan->SetField(TEXT("payload"),Value);
   if(CastField<FBoolProperty>(Property)){bool Bool=false;if(!Value||Value->Type!=EJson::Boolean||!Value->TryGetBool(Bool))return Bad(TEXT("pcg_standalone_spawner_boolean_required"),TEXT("Bounded bool writer requires a JSON boolean; implicit nonboolean AsBool coercions are not admitted"),Requested);Expected=MakeShared<FJsonValueBoolean>(Bool);Plan->SetStringField(TEXT("kind"),TEXT("bool"));}
   else if(CastField<FIntProperty>(Property)){double Number=0;if(!Value||Value->Type!=EJson::Number||!Value->TryGetNumber(Number)||!FMath::IsFinite(Number)||Number<static_cast<double>(MIN_int32)||Number>=2147483648.0)return Bad(TEXT("pcg_standalone_spawner_integer_range"),TEXT("Seed requires a finite JSON number within int32 before the original truncating cast"),Requested);const int32 Native=static_cast<int32>(Number);Expected=MakeShared<FJsonValueNumber>(Native);Plan->SetStringField(TEXT("kind"),TEXT("int32"));Plan->SetBoolField(TEXT("numeric_fraction_truncated"),Number!=Native);}
   else{FString Name;if(!Value||Value->Type!=EJson::String||!Value->TryGetString(Name)||Name.Len()>=NAME_SIZE||!NoEmbeddedNul(Name))return Bad(TEXT("pcg_standalone_spawner_name_required"),TEXT("OutAttributeName requires a bounded native FName string before construction"),Requested);Expected=MakeShared<FJsonValueString>(FName(*Name).ToString());Plan->SetStringField(TEXT("kind"),TEXT("name"));}
   Plan->SetField(TEXT("expected"),Expected);Plans->SetObjectField(Field->Alias,Plan);Aliases->SetField(Field->Alias,Value);
  }
  W.R.Data->SetObjectField(TEXT("standalone_spawner_general_field_plan"),Plans);W.R.Data->SetBoolField(TEXT("standalone_stock_spawner_general_fields_profile"),true);return W.Admit(TEXT("standalone_stock_spawner_plan_current"));
 }
 bool Prepare(UClass* Class,const TSharedPtr<FJsonObject>& Input,TSharedPtr<FJsonObject>& Plans,TSharedPtr<FJsonObject>& Aliases)
 {
  if(!StandaloneClass(Class)||!Held.ClassSource(Class))return Bad(TEXT("pcg_standalone_class_unsupported"),TEXT("Standalone supports existing finite9/Bounds/Spawner, SurfaceSampler8/Difference4, Union2/InnerIntersection2/OuterIntersection3, DataFromActor15/GetLandscape16 bounded scalar/aggregate fields, CopyAttributes/Rename own scalar/selector fields and raw SubgraphOverride with bounded saved native child DAGs; broader fields/parameterized or custom child graphs/runtime remain incomplete"));
    if(StandaloneSubgraphClass(Class)){
     SubgraphActive=true;
     if(!Held.ClassSource(UPCGSubgraphNode::StaticClass())||!Held.ClassSource(UPCGGraphInstance::StaticClass()))return false;
     if(!CaptureSubgraphDefaults())return false;
     for(const auto& Pair:Input->Values)if(FName(*FString(Pair.Key))!=TEXT("SubgraphOverride"))return Bad(TEXT("pcg_subgraph_property_pending"),TEXT("First Subgraph supports raw native SubgraphOverride only"));
     Plans=MakeShared<FJsonObject>();Aliases=MakeShared<FJsonObject>(*Input);W.R.Data->SetBoolField(TEXT("standalone_subgraph_profile"),true);return W.Admit(TEXT("subgraph_native_sources_prepared"));}
  auto Filtered=MakeShared<FJsonObject>(*Input);auto RawPlans=MakeShared<FJsonObject>();auto RawAliases=MakeShared<FJsonObject>();
  for(const auto& Pair:Input->Values){const auto* Field=StandaloneField(Class,FName(*FString(Pair.Key)));FProperty* P=Field?FreshNativeField(Class,Field->Native):nullptr;const bool Sampling=RawSamplingProperty(Class,P),VectorText=Pair.Value&&Pair.Value->Type==EJson::String&&RawVectorProperty(Class,P);if(!Sampling&&!VectorText)continue;
   FRawNativeStructValue Scratch;TSharedPtr<FJsonObject> Plan;if(!PrepareRawStruct(Class,P,Pair.Value,Scratch,Plan))return false;RawPlans->SetObjectField(Field->Alias,Plan);RawAliases->SetField(Field->Alias,Pair.Value);Filtered->RemoveField(FString(Pair.Key));}
  if(!RawPlans->Values.IsEmpty()){
   // Recursively reuse existing planners only for the remainder; new text fields are absent.
   if(Filtered->Values.IsEmpty()){Plans=MakeShared<FJsonObject>();Aliases=MakeShared<FJsonObject>();}else if(!Prepare(Class,Filtered,Plans,Aliases))return false;
   for(const auto& Pair:RawPlans->Values)Plans->SetField(FString(Pair.Key),Pair.Value);for(const auto& Pair:RawAliases->Values)Aliases->SetField(FString(Pair.Key),Pair.Value);return Current();
  }
     if(StandaloneSpawnerClass(Class))return PrepareSpawner(Input,Plans,Aliases);
     if(StandaloneSamplingDifferenceClass(Class)||StandaloneSpatialCompositionClass(Class))return PrepareSampling(Class,Input,Plans,Aliases);
     if(StandaloneMetadataClass(Class))return PrepareMetadata(Class,Input,Plans,Aliases);
  auto FiniteInput=MakeShared<FJsonObject>(*Input);auto AggregatePlans=MakeShared<FJsonObject>();auto AggregateAliases=MakeShared<FJsonObject>();
  if(FreshDataSourceClass(Class))for(const auto& Pair:Input->Values){const FString Requested(Pair.Key.ToView());if(Requested.Len()>=NAME_SIZE||!NoEmbeddedNul(Requested))return Bad(TEXT("pcg_data_source_property_name_invalid"),TEXT("Bounded original native property name required"));const auto* Field=StandaloneField(Class,FName(*Requested));if(!Field||!DataSourceAggregateName(FName(Field->Native)))continue;FProperty* P=FreshNativeField(Class,Field->Native);FDataSourceAggregateValue Scratch;TSharedPtr<FJsonObject> Plan;if(!P||!PrepareDataSourceAggregate(Class,P,Pair.Value,Scratch,Plan))return false;AggregatePlans->SetObjectField(Field->Alias,Plan);AggregateAliases->SetField(Field->Alias,Pair.Value);FiniteInput->RemoveField(FString(Pair.Key));}
  Aliases=MakeShared<FJsonObject>();for(const auto& Pair:FiniteInput->Values){const auto* F=StandaloneField(Class,FName(*FString(Pair.Key)));if(!F)return Bad(TEXT("pcg_standalone_property_unsupported"),TEXT("Use an original native property name in the finite authored field slice; arbitrary reflection/import remains blocked"));if(Class->GetFName()==TEXT("PCGGetActorPropertySettings")&&FString(F->Native)==TEXT("PropertyName")){FString Name;if(!Pair.Value||Pair.Value->Type!=EJson::String||!Pair.Value->TryGetString(Name)||Name.Len()>=NAME_SIZE||!NoEmbeddedNul(Name))return Bad(TEXT("pcg_standalone_name_value_unsupported"),TEXT("PropertyName requires a bounded native FName string before FName construction"));}Aliases->SetField(F->Alias,Pair.Value);}
  if(!CubelessPCGUE58::PrepareFreshSettings(Class,Aliases,Plans,[&]{return W.Admit(TEXT("standalone_finite_preflight"));},true)){if(Plans&&Plans->HasField(TEXT("point_import_preflight"))){W.R.Data->SetObjectField(TEXT("point_import_preflight"),Plans->GetObjectField(TEXT("point_import_preflight")));return Bad(TEXT("pcg_standalone_point_import_roundtrip_unsupported"),TEXT("Prepared point ImportText roundtrip could not prove requested values before target mutation"));}return Bad(TEXT("pcg_standalone_setting_type_invalid"),TEXT("Finite typed field request rejected before writer"));}
  for(const auto& Pair:FiniteInput->Values){const auto* F=StandaloneField(Class,FName(*FString(Pair.Key)));const TSharedPtr<FJsonObject>* Plan=nullptr;if(!F||!Plans->TryGetObjectField(F->Alias,Plan)||!Same((*Plan)->GetStringField(TEXT("native_property")),F->Native))return Bad(TEXT("pcg_standalone_property_role_changed"),TEXT("Finite helper native field mapping differs from requested original property"));}
  for(const auto& Pair:AggregatePlans->Values)Plans->SetField(FString(Pair.Key),Pair.Value);for(const auto& Pair:AggregateAliases->Values)Aliases->SetField(FString(Pair.Key),Pair.Value);
  if(StandalonePointBoundsClass(Class)){W.R.Data->SetBoolField(TEXT("standalone_point_bounds_profile"),true);W.R.Data->SetStringField(TEXT("standalone_point_bounds_class_path"),Class->GetPathName());W.R.Data->SetNumberField(TEXT("standalone_point_bounds_field_count"),Class->GetFName()==TEXT("PCGPointExtentsModifierSettings")?2:5);W.R.Data->SetBoolField(TEXT("point_bounds_runtime_generation_verified"),false);}
  if(FreshDataSourceClass(Class)){W.R.Data->SetBoolField(TEXT("standalone_data_source_profile"),true);W.R.Data->SetStringField(TEXT("standalone_data_source_class_path"),Class->GetPathName());W.R.Data->SetNumberField(TEXT("standalone_data_source_field_count"),Class==UPCGDataFromActorSettings::StaticClass()?15:17);W.R.Data->SetBoolField(TEXT("data_source_runtime_generation_verified"),false);W.R.Data->SetStringField(TEXT("standalone_data_source_scope"),TEXT("Original public Add/Set/Connect reuse the finite scalar fields plus raw native-text ActorSelector, ComponentSelector and ExpectedPins, actual reflected semantic value preservation, source/current roles and Structural notification. Loaded native (including abstract) and saved-clean current generated Actor/Component class filters are held/audited; transient reflected show flags are planned values, while wildcard/prepared/tag-string caches and non-UPROPERTY ComponentList are not frozen. Landscape SamplingProperties raw native-text authoring is implemented / usage validation pending; unloaded class aliases, unsaved/generated source variants, broader input coercions and runtime generation remain incomplete. The separate graph-spec factory reports its dict/list implementation and usage-validation limits independently. Constructor instance defaults are captured as returned, never required to equal the CDO."));}
  return W.Admit(TEXT("standalone_plan_current"));
 }
};
bool FWork::StandaloneConstruction(const TCHAR* Operation)
{
 if(!Standalone||!FreshCurrent||!Graph||Standalone->Held.Target.Get()!=Graph||!Admit(TEXT("standalone_construction_current")))return false;
 const bool Add=Same(Operation,TEXT("Graph_AddNodeOfType"))&&Standalone->PreparedAdd;
 const bool Edge=Same(Operation,TEXT("Graph_AddEdge_allocates_UPCGEdge"));
 const bool Setting=Same(Operation,TEXT("Settings_Modify_and_structural_UpdatePins"))&&Standalone->MutableSettings&&Standalone->MutableField;
 return (Add||Edge||Setting)||Fail(TEXT("pcg_standalone_construction_not_planned"),TEXT("Only the exact enrolled Add/edge/selected finite setting is authorized"));
}
bool EnrollStandalone(FWork& W)
{
 if(W.FreshCurrent||W.Standalone)return true;auto* G=W.Graph;
 // Plain eligibility mismatch preserves the old generic/existing-edge route.
 if(!G||G->GetClass()!=UPCGGraph::StaticClass()||!G->GetPathName().StartsWith(TEXT("/Game/"))||G->GetOuter()!=G->GetPackage()||G->HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject)||!W.Snapshot.Input||!W.Snapshot.Output||W.Snapshot.Input==W.Snapshot.Output)return true;
 for(const auto& N:W.Snapshot.NodeSlots){const bool IO=N.Node==W.Snapshot.Input||N.Node==W.Snapshot.Output;if((N.Node->GetClass()!=UPCGNode::StaticClass()&&N.Node->GetClass()!=UPCGSubgraphNode::StaticClass())||((N.Node->GetClass()==UPCGSubgraphNode::StaticClass())!=StandaloneSubgraphClass(N.Settings?N.Settings->GetClass():nullptr))||N.Interface!=N.Settings||N.InstanceTarget||!N.Settings||N.Settings->GetPackage()!=G->GetPackage()||N.Settings->GetOuter()!=(IO?static_cast<UObject*>(G):static_cast<UObject*>(N.Node))||(IO?N.Settings->GetClass()!=UPCGGraphInputOutputSettings::StaticClass():!StandaloneClass(N.Settings->GetClass())))return true;}
 for(const auto& Pin:W.Snapshot.Pins)if(Pin.Pin->GetClass()!=UPCGPin::StaticClass())return true;for(const auto& Edge:W.Snapshot.Edges)if(Edge.Edge->GetClass()!=UPCGEdge::StaticClass())return true;
 auto Owned=MakeShared<FStandaloneRequest>(W);auto* F=&Owned.Get();W.Standalone=Owned;
 F->Held.Target.Reset(G);F->Held.TargetPackage=G->GetPackage();F->Held.ObjectPath=G->GetPathName();F->Held.TargetFlags=G->GetFlags();F->Held.PackageObjectFlags=G->GetPackage()->GetFlags();F->Held.PackageFlags=G->GetPackage()->GetPackageFlags();
 W.FreshCurrent=[F]{return F->Current();};W.SubgraphFinalCheck=[F]{return F->SubgraphFinal();};W.FreshSnapshot=[&W]{return FreshSnapshotCurrent(W.Snapshot)||W.Fail(TEXT("pcg_standalone_snapshot_changed"),TEXT("Existing captured resolved physical topology changed"));};
 W.FreshCapture=[F](UPCGGraph* Target){return F->Held.CaptureBegin(Target);};W.FreshCaptureDone=[F]{F->Held.CapturingReferences=false;};
 W.FreshPins=[F](UPCGNode* N,bool Output){TArray<UPCGPin*> Out;for(UObject* O:F->Held.CaptureValues(N,Output?TEXT("OutputPins"):TEXT("InputPins"))){if(O->GetClass()!=UPCGPin::StaticClass()){F->Bad(TEXT("pcg_standalone_pin_class"),TEXT("Exact native current pin required"));return TArray<UPCGPin*>();}Out.Add(static_cast<UPCGPin*>(O));}return Out;};
 W.FreshEdges=[F](UPCGPin* P){TArray<UPCGEdge*> Out;for(UObject* O:F->Held.CaptureValues(P,TEXT("Edges"))){if(O->GetClass()!=UPCGEdge::StaticClass()){F->Bad(TEXT("pcg_standalone_edge_class"),TEXT("Exact native current edge required"));return TArray<UPCGEdge*>();}Out.Add(static_cast<UPCGEdge*>(O));}return Out;};
 W.FreshEdgePin=[F](UPCGEdge* E,bool Input)->UPCGPin*{auto Out=F->Held.CaptureValues(E,Input?TEXT("InputPin"):TEXT("OutputPin"));return Out.Num()==1&&Out[0]->GetClass()==UPCGPin::StaticClass()?static_cast<UPCGPin*>(Out[0]):nullptr;};
 for(const auto& N:W.Snapshot.NodeSlots)if(StandaloneSubgraphClass(N.Settings->GetClass())&&!F->CaptureSubgraph(static_cast<UPCGSubgraphNode*>(N.Node),static_cast<UPCGSubgraphSettings*>(N.Settings),true))return false;
 for(const auto& N:W.Snapshot.NodeSlots)if(StandaloneSpawnerClass(N.Settings->GetClass())&&!F->CaptureSpawner(static_cast<UPCGStaticMeshSpawnerSettings*>(N.Settings),true))return false;
 if(!W.Pin(G->GetPackage()))return false;for(UClass* C:{UPCGGraph::StaticClass(),UPCGNode::StaticClass(),UPCGPin::StaticClass(),UPCGEdge::StaticClass(),UPCGGraphInputOutputSettings::StaticClass()})if(!F->Held.ClassSource(C))return false;
 for(const auto& N:W.Snapshot.NodeSlots){if(StandaloneSpawnerClass(N.Settings->GetClass())&&!F->CaptureValues(N.Settings))return false;if(!F->Held.ClassSource(N.Settings->GetClass()))return false;if(N.Node!=W.Snapshot.Input&&N.Node!=W.Snapshot.Output&&!F->CaptureValues(N.Settings))return false;}
 // Enrollment never recaptures/adopts a changed before state.
 if(!FreshSnapshotCurrent(W.Snapshot)||!W.Admit(TEXT("standalone_existing_snapshot_enrolled")))return W.Fail(TEXT("pcg_standalone_enrollment_changed"),TEXT("Original captured snapshot/source did not remain current during admission"));
 W.R.Data->SetBoolField(TEXT("request_owned_standalone_construction"),true);W.R.Data->SetBoolField(TEXT("construction_available"),true);W.R.Data->SetBoolField(TEXT("construction_GC_health_resolved"),false);return true;
}
bool PrepareStandaloneAdd(FWork& W,UClass* Class,const TSharedPtr<FJsonObject>& P)
{
 TSharedPtr<FJsonObject> Settings=MakeShared<FJsonObject>(),Plans,Aliases;const TSharedPtr<FJsonObject>* Given=nullptr;if(P->TryGetObjectField(TEXT("settings"),Given))Settings=*Given;
 if(!W.Standalone->Prepare(Class,Settings,Plans,Aliases))return false;W.Standalone->PreparedAdd=Class;if(StandaloneSpawnerClass(Class))W.R.Data->SetBoolField(TEXT("standalone_stock_spawner_producer"),true);return true;
}
bool KeepStandaloneReturns(FWork& W,UPCGNode* N,UPCGSettings* S)
{
 const bool NodeHeld=W.Standalone->Held.KeepReturned(N,TEXT("standalone_node"));const bool SettingsHeld=W.Standalone->Held.KeepReturned(S,TEXT("standalone_settings"));if(!NodeHeld||!SettingsHeld)return false;
  if(StandaloneSubgraphClass(S->GetClass())){if(N->GetClass()!=UPCGSubgraphNode::StaticClass()||!W.Standalone->CaptureSubgraph(static_cast<UPCGSubgraphNode*>(N),static_cast<UPCGSubgraphSettings*>(S),true))return false;}
  if(StandaloneSpawnerClass(S->GetClass())){if(N->GetClass()!=UPCGNode::StaticClass()||N->GetOuter()!=W.Graph||S->GetOuter()!=N||S->GetClass()!=W.Standalone->PreparedAdd||N->GetPackage()!=W.Graph->GetPackage()||S->GetPackage()!=W.Graph->GetPackage()||!FreshObjectEquals(N,TEXT("SettingsInterface"),S))return W.Fail(TEXT("pcg_standalone_spawner_return_owner_changed"),TEXT("Returned node/settings actual class/own graph relationship differs before further SDK observation"));auto* Spawner=static_cast<UPCGStaticMeshSpawnerSettings*>(S);if(!Spawner->MeshSelectorParameters.IsResolved())return W.Fail(TEXT("pcg_standalone_spawner_return_unresolved"),TEXT("Returned stock selector slot is unresolved; no further SDK call"));auto* Selector=Spawner->MeshSelectorParameters.Get();if(!W.Standalone->Held.KeepReturned(Selector,TEXT("standalone_spawner_selector")))return W.Fail(TEXT("pcg_standalone_spawner_return_unavailable"),TEXT("Constructor-returned default selector must be immediately held"));if(Selector->GetClass()!=UPCGMeshSelectorWeighted::StaticClass()||!Spawner->MeshSelectorType.GetGCPtr().IsResolved()||Spawner->MeshSelectorType.GetGCPtr()!=UPCGMeshSelectorWeighted::StaticClass()||Selector->GetOuter()!=S||Selector->GetPackage()!=S->GetPackage())return W.Fail(TEXT("pcg_standalone_spawner_constructor_role_changed"),TEXT("Actual constructor result is not an exact owned stock Weighted selector"));if(!W.Standalone->CaptureSpawner(Spawner,true))return false;}
 return true;
}
bool CaptureStandaloneValues(FWork& W,UPCGSettings* S){return W.Standalone->CaptureValues(S);}
bool SetStandaloneSubgraph(FWork& W,UPCGNode* Node,UPCGSettings* Settings,const FString& Name,const TSharedPtr<FJsonValue>& Value,FString& Expected)
{
 auto& F=*W.Standalone;auto* Sub=static_cast<UPCGSubgraphSettings*>(Settings);if(!F.CaptureSubgraph(static_cast<UPCGSubgraphNode*>(Node),Sub))return false;
 auto* Property=CastField<FObjectProperty>(FreshNativeField(Sub->GetClass(),TEXT("SubgraphOverride")));auto* Cell=F.Subgraphs.FindByPredicate([&](const FStandaloneSubgraph& E){return E.Settings==Sub;});
 if(!Cell||Property!=Cell->OverrideField||!F.SubgraphRoleCurrent(*Cell))return false;
 TGuardValue<UPCGSettings*> MutableSettings(F.MutableSettings,Settings);TGuardValue<FProperty*> MutableField(F.MutableField,Property);TGuardValue<UPCGNode*> ActiveNode(F.Held.ActiveSettingsNode,Node);TGuardValue<UPCGSettings*> ActiveSettings(F.Held.ActiveSettings,Settings);
 // Preserve original always-Modify, including type/lookup failures and equal values.
 if(!W.RequireConstruction(TEXT("Settings_Modify_and_structural_UpdatePins")))return false;
 if(!W.Write(TEXT("Settings_Modify"),[&]{Settings->Modify();}))return false;
 if(FName(*Name)!=TEXT("SubgraphOverride"))return W.Fail(TEXT("pcg_subgraph_property_pending"),TEXT("First Subgraph slice writes only the original native SubgraphOverride object field"));
 Property=CastField<FObjectProperty>(FreshNativeField(Sub->GetClass(),TEXT("SubgraphOverride")));Cell=F.Subgraphs.FindByPredicate([&](const FStandaloneSubgraph& E){return E.Settings==Sub;});
 if(!Cell||Property!=Cell->OverrideField||!F.SubgraphRoleCurrent(*Cell))return false;
 FString Path;if(!Value||Value->Type!=EJson::String||!Value->TryGetString(Path))return W.Fail(TEXT("pcg_property_object_value"),TEXT("Object property requires path string; JSON null is not clear"));
 if(Forbidden(Path))return W.Fail(TEXT("pcg_property_dependency_forbidden"),TEXT("Object tooling dependency forbidden"));
 UPCGGraph* Child=nullptr;
 if(!Path.IsEmpty())
 {
  Child=FindObject<UPCGGraph>(nullptr,*NormalizePCGObjectPathForLoad(Path));TStrongObjectPtr<UPCGGraph> Returned(Child);if(!W.Admit(TEXT("subgraph_loaded_child_lookup_return")))return false;
  if(!Child)return W.Fail(TEXT("pcg_subgraph_loaded_child_required"),TEXT("First source slice uses an already-loaded exact child graph; no loader or fallback is synthesized"));
  if(!F.CaptureSubgraphSource(Child))return false;
 }
 // Source capture may grow its own arrays; reacquire the selected row by pointer.
 Cell=F.Subgraphs.FindByPredicate([&](const FStandaloneSubgraph& E){return E.Settings==Sub;});if(!Cell||!F.SubgraphRoleCurrent(*Cell))return false;
 FString Before;if(!ExportProperty(W,Sub,Name,Before))return false;
 Cell=F.Subgraphs.FindByPredicate([&](const FStandaloneSubgraph& E){return E.Settings==Sub;});if(!Cell||Property!=Cell->OverrideField||!F.SubgraphRoleCurrent(*Cell))return false;
 Cell->Planned=Child;Cell->Mutable=true;
 if(!W.SDK(TEXT("Property_SetValue"),true)){Cell->Mutable=false;return false;}
 // The only raw writer is the original public FObjectProperty operation.
 Property->SetObjectPropertyValue_InContainer(Sub,Child);Cell->Override=Child;Cell->Mutable=false;
 if(!W.After(TEXT("Property_SetValue")))return false;
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("property"),Name);Row->SetBoolField(TEXT("SDK_set_or_import_accepted"),true);Row->SetBoolField(TEXT("raw_object_write"),true);Row->SetBoolField(TEXT("eager_SetSubgraph_called"),false);W.PropertyRows.Add(MakeShared<FJsonValueObject>(Row));
 if(!ExportProperty(W,Sub,Name,Expected))return false;W.R.bChanged|=!Same(Before,Expected);
 // Public source-driven declarations are observed before the notification. They
 // predict only these current node pins, not internal callback atomicity.
 TArray<FPCGPinProperties> ExpectedInputs,ExpectedOutputs;
 if(!W.Read(TEXT("Subgraph_AllInputPinProperties"),[&]{return Sub->AllInputPinProperties();},ExpectedInputs)||!W.Read(TEXT("Subgraph_AllOutputPinProperties"),[&]{return Sub->AllOutputPinProperties();},ExpectedOutputs))return false;
 if(ExpectedInputs.Num()>128||ExpectedOutputs.Num()>128)return W.Fail(TEXT("pcg_subgraph_pin_budget"),TEXT("Finite source-driven node pin declarations required"));
 if(!MutateGraph(W,TEXT("Settings_Structural_Notify"),TEXT("notify"),[&]{Sub->OnSettingsChangedDelegate.Broadcast(Sub,EPCGChangeType::Settings|EPCGChangeType::Structural);},Node))return false;
 if(!W.Admit(TEXT("subgraph_notify_postcondition")))return false;
 const auto* Actual=W.Snapshot.NodeSlots.FindByPredicate([&](const FNodeSlot& E){return E.Node==Node;});if(!Actual)return W.Fail(TEXT("pcg_subgraph_returned_node_missing"),TEXT("Returned node was not retained in actual parent inventory"));
 auto CheckPins=[&](const TArray<UPCGPin*>& Pins,const TArray<FPCGPinProperties>& Planned)
 {
  if(Pins.Num()!=Planned.Num())return false;
  for(const auto& E:Planned){const auto* P=W.Snapshot.Pins.FindByPredicate([&](const FPinSlot& Q){return Q.Node==Node&&Q.Properties.Label==E.Label&&Pins.Contains(Q.Pin);});if(!P)return false;const bool SameProperties=FPCGPinProperties::StaticStruct()->CompareScriptStruct(&P->Properties,&E,0);if(!W.Admit(TEXT("subgraph_pin_properties_return"))||!SameProperties)return false;}
  return true;
 };
 if(!CheckPins(Actual->Input,ExpectedInputs)||!CheckPins(Actual->Output,ExpectedOutputs))return W.Fail(TEXT("pcg_subgraph_returned_pin_declarations_differ"),TEXT("Current returned pins differ from pre-notify public declarations; actual partial topology retained"));
 W.R.Data->SetBoolField(TEXT("subgraph_raw_override_contract"),true);W.R.Data->SetBoolField(TEXT("subgraph_eager_setter_called"),false);W.R.Data->SetBoolField(TEXT("subgraph_returned_pin_declarations_verified"),true);W.R.Data->SetStringField(TEXT("subgraph_override_after"),Child?Child->GetPathName():FString());
 return VerifyProperty(W,Sub,Name,Expected);
}

bool SetStandaloneMetadata(FWork& W,UPCGNode* N,UPCGSettings* S,const FString& Name,const TSharedPtr<FJsonValue>& Value,FString& Expected)
{
 auto& F=*W.Standalone;if(!F.CaptureMetadata(S))return false;auto Input=MakeShared<FJsonObject>();Input->SetField(Name,Value);TSharedPtr<FJsonObject> Plans,Aliases;if(!F.PrepareMetadata(S->GetClass(),Input,Plans,Aliases,S))return false;
 auto* Property=FreshNativeField(S->GetClass(),FName(*Name));const auto* P=F.MetadataPlans.Find(Property);if(!P)return W.Fail(TEXT("pcg_metadata_plan_missing"),TEXT("Exact selected native property plan missing"));const FStandaloneMetadataCell Planned=*P;
 TGuardValue<UPCGSettings*> MutableSettings(F.MutableSettings,S);TGuardValue<FProperty*> MutableField(F.MutableField,Property);TGuardValue<UPCGNode*> ActiveNode(F.Held.ActiveSettingsNode,N);TGuardValue<UPCGSettings*> ActiveSettings(F.Held.ActiveSettings,S);
 // Original always-Modify -> raw property setter/ImportText -> Settings|Structural broadcast is unchanged.
 // The actual target's copied selector, including inactive names, was preplanned before this writer.
 if(!SetSettingCore(W,N,S,Name,Value,Expected))return false;if(!W.Admit(TEXT("metadata_before_planned_readback")))return false;
 auto* Captured=F.Metadata.FindByPredicate([&](const FStandaloneMetadata& E){return E.Settings==S;});FStandaloneMetadataCell Actual;Actual.Property=Property;Actual.Name=Planned.Name;if(!Captured||FreshNativeField(S->GetClass(),Planned.Name)!=Property||!Captured->Read(Actual))return W.Fail(TEXT("pcg_metadata_selected_role_changed"),TEXT("Selected own current field unavailable after raw writer"));
 const bool Equal=MetadataCellSame(Planned,Actual);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("settings_path"),S->GetPathName());Row->SetStringField(TEXT("native_property"),Planned.Name.ToString());Row->SetField(TEXT("expected"),MetadataCellJson(Planned));Row->SetField(TEXT("actual"),MetadataCellJson(Actual));Row->SetBoolField(TEXT("planned_typed_readback_passed"),Equal);Row->SetBoolField(TEXT("source_and_unselected_baseline_replaced"),false);Row->SetBoolField(TEXT("runtime_metadata_execution_verified"),false);W.PropertyRows.Add(MakeShared<FJsonValueObject>(Row));
 if(!Equal){auto E=MakeShared<FJsonObject>(),A=MakeShared<FJsonObject>();E->SetField(TEXT("value"),MetadataCellJson(Planned));A->SetField(TEXT("value"),MetadataCellJson(Actual));return F.Bad(TEXT("pcg_metadata_planned_value_mismatch"),TEXT("Actual copied selected value differs from prewriter actual-cell plan; partial prefix retained"),S->GetPathName()+TEXT(":")+Planned.Name.ToString(),Json(E),Json(A));}
 // No SDK/observer intervenes between typed comparison and selected acceptance.
 if(!Captured->Accept(Actual))return W.Fail(TEXT("pcg_metadata_selected_acceptance_failed"),TEXT("Only verified selected cell may update its expectation"));W.R.Data->SetObjectField(TEXT("standalone_last_setting_plan_verified"),Row);return true;
}

bool SetGraphSpecDataSourceAggregate(FWork& W,FStandaloneRequest& F,UPCGNode* N,UPCGSettings* S,const TCHAR* Name,const TSharedPtr<FJsonValue>& Input,const TSharedPtr<FJsonObject>& Receipt)
{
 FProperty* P=FreshNativeField(S->GetClass(),Name);if(!P||!F.CaptureValues(S,true))return false;auto* Captured=F.Values.FindByPredicate([&](const FStandaloneValue& V){return V.Settings==S&&V.Property==P;});if(!Captured||!Captured->Aggregate||!Captured->SameType()||!Captured->SameValue())return F.Bad(TEXT("pcg_graph_spec_aggregate_role_changed"),TEXT("Actual returned instance aggregate must remain current before the original recursive setter"));
 FDataSourceAggregateValue Admission=*Captured->Aggregate;TSharedPtr<FJsonObject> Plan;if(!F.PrepareGraphSpecDataSourceAggregate(S->GetClass(),P,Input,Admission,Plan))return false;Receipt->SetObjectField(TEXT("actual_instance_prewrite_plan"),Plan);TArray<TSharedPtr<FJsonValue>> Steps;Receipt->SetArrayField(TEXT("recursive_editor_setter_steps"),Steps);
 TGuardValue<UPCGSettings*> Mutable(F.MutableSettings,S);TGuardValue<FProperty*> Selected(F.MutableField,P);TGuardValue<UPCGNode*> ActiveNode(F.Held.ActiveSettingsNode,N);TGuardValue<UPCGSettings*> ActiveSettings(F.Held.ActiveSettings,S);
 auto Apply=[&](FProperty* Member,const TSharedPtr<FJsonValue>& Value,bool Parent)->bool
 {
  if(!W.Admit(TEXT("graph_spec_aggregate_before_editor_set"))||FreshNativeField(S->GetClass(),Name)!=P||!Captured->SameType())return false;if(!Captured->SameValue())return F.Bad(TEXT("pcg_graph_spec_aggregate_selected_drift"),TEXT("Selected aggregate differs from the last verified actual value before the next editor setter; no stale baseline write or retry"),Captured->Path,Json(Captured->Aggregate->JsonValue()),TEXT("selected semantic value changed"));FDataSourceAggregateValue Wanted=*Captured->Aggregate;
  if(!Parent){auto One=MakeShared<FJsonObject>();One->SetField(Member->GetName(),Value);TSharedPtr<FJsonObject> OnePlan;if(!F.PrepareGraphSpecDataSourceAggregate(S->GetClass(),P,MakeShared<FJsonValueObject>(One),Wanted,OnePlan))return false;}
  else if(P->GetFName()==TEXT("ExpectedPins"))Wanted=Admission;
  const bool Identical=Member?Member->Identical(Member->ContainerPtrToValuePtr<void>(Captured->Aggregate->Address()),Member->ContainerPtrToValuePtr<void>(Wanted.Address()),PPF_None):P->Identical(Captured->Aggregate->Address(),Wanted.Address(),PPF_None);
  if(Identical)Wanted=*Captured->Aggregate;
  if(!Identical&&Member&&P->GetFName()==TEXT("ActorSelector")&&Member->GetFName()==TEXT("ActorSelection")&&Wanted.Actor.ActorSelection!=EPCGActorSelection::ByClass)
  {
   // Native PostEditChangeProperty resets only this class role when the nested ActorSelection changes away from ByClass.
   TSubclassOf<AActor> Default;if(!W.Read(TEXT("graph_spec_default_actor_selector_class"),[&]{return static_cast<UPCGDataFromActorSettings*>(S)->GetDefaultActorSelectorClass();},Default))return false;Wanted.Actor.ActorSelectionClass=Default;Wanted.RefreshClassReference();if(!F.HoldDataSourceFilterHandle(Wanted.ClassReference(),AActor::StaticClass()))return false;
  }
  auto Step=MakeShared<FJsonObject>();Step->SetStringField(TEXT("native_property"),P->GetName());Step->SetStringField(TEXT("nested_member"),Member?Member->GetName():FString());Step->SetBoolField(TEXT("parent_setter"),Parent);Step->SetBoolField(TEXT("identical_before_editor_set"),Identical);Step->SetObjectField(TEXT("expected"),Wanted.JsonValue());Steps.Add(MakeShared<FJsonValueObject>(Step));Receipt->SetArrayField(TEXT("recursive_editor_setter_steps"),Steps);
  const auto Before=W.Snapshot;W.RetireMutablePins(N);if(!W.SDK(TEXT("graph_spec_data_source_editor_set"),true))return false;if(!Captured->SameValue())return F.Bad(TEXT("pcg_graph_spec_aggregate_selected_drift"),TEXT("Planning/admission callback changed the selected aggregate before the editor setter; no stale planned value is written or retried"),Captured->Path,Json(Captured->Aggregate->JsonValue()),TEXT("selected semantic value changed before setter"));EPropertyAccessResultFlags Access;
  if(Member){const TArray<void*> NoArchetypes;Access=PropertyAccessUtil::SetPropertyValue_InContainer(Member,P->ContainerPtrToValuePtr<void>(S),NoArchetypes,Member,Member->ContainerPtrToValuePtr<void>(Wanted.Address()),INDEX_NONE,PropertyAccessUtil::EditorReadOnlyFlags,false,[&](){auto Notify=PropertyAccessUtil::BuildBasicChangeNotify(P,S,EPropertyAccessChangeNotifyMode::Default,EPropertyChangeType::ValueSet);if(Notify){Notify->ChangedPropertyChain.AddTail(Member);Notify->ChangedPropertyChain.SetActivePropertyNode(Member);Notify->ChangedPropertyChain.SetActiveMemberPropertyNode(P);}return Notify;});}
  else Access=PropertyAccessUtil::SetPropertyValue_Object(P,S,P,Wanted.Address(),INDEX_NONE,PropertyAccessUtil::EditorReadOnlyFlags,EPropertyAccessChangeNotifyMode::Default);
  Step->SetBoolField(TEXT("editor_set_returned"),true);Step->SetNumberField(TEXT("access_result"),static_cast<uint8>(Access));if(!W.After(TEXT("graph_spec_data_source_editor_set")))return false;
  FDataSourceAggregateValue Actual;if(!Actual.Read(S,P))return false;Step->SetObjectField(TEXT("actual"),Actual.JsonValue());const bool Equal=Wanted.SameValue(Actual);Step->SetBoolField(TEXT("typed_expected_verified"),Equal);if(Access!=EPropertyAccessResultFlags::Success||!Equal)return F.Bad(TEXT("pcg_graph_spec_aggregate_editor_set_failed"),TEXT("Actual editor setter prefix retained; returned access/value differs from the planned actual-instance merge"),S->GetPathName()+TEXT(":")+P->GetName(),Json(Wanted.JsonValue()),Json(Actual.JsonValue()));
  if(!Captured->Capture())return false;if(!Transition(W,Before,TEXT("notify"),N))return false;if(!Captured->SameValue())return F.Bad(TEXT("pcg_graph_spec_aggregate_selected_drift"),TEXT("Graph recapture callback changed the selected aggregate after its verified setter; actual prefix retained without another setter or retry"),Captured->Path,Json(Captured->Aggregate->JsonValue()),TEXT("selected semantic value changed after graph recapture"));W.R.bChanged|=!Identical;return true;
 };
 if(P->GetFName()==TEXT("ExpectedPins")){if(!Apply(nullptr,Input,true))return false;}
 else
 {
  auto* Root=P->GetFName()==TEXT("ActorSelector")?FPCGActorSelectorSettings::StaticStruct():FPCGComponentSelectorSettings::StaticStruct();
  for(const auto& Pair:Plan->GetObjectField(TEXT("canonical_members"))->Values){FProperty* Member=FindFProperty<FProperty>(Root,FName(*FString(Pair.Key)));if(!Member)return false;if(CastField<FStructProperty>(Member))continue;if(!Apply(Member,Pair.Value,false))return false;}
  // The original recursion assigns the resulting nested struct back to its parent even for an empty dict.
  if(!Apply(nullptr,Input,true))return false;
 }
 Receipt->SetObjectField(TEXT("actual_final"),Captured->Aggregate->JsonValue());Receipt->SetBoolField(TEXT("actual_instance_merge_verified"),true);Receipt->SetBoolField(TEXT("raw_ImportText_contract_used"),false);Receipt->SetBoolField(TEXT("source_and_unselected_baseline_replaced"),false);return true;
}

bool SetStandaloneDataSourceAggregate(FWork& W,UPCGNode* N,UPCGSettings* S,const FString& Name,const TSharedPtr<FJsonValue>& V,FString& Expected)
{
 auto& F=*W.Standalone;auto Input=MakeShared<FJsonObject>();Input->SetField(Name,V);TSharedPtr<FJsonObject> Plans,Aliases;if(!F.Prepare(S->GetClass(),Input,Plans,Aliases)||!F.CaptureValues(S))return false;
 const auto* Field=StandaloneField(S->GetClass(),FName(*Name));FProperty* P=Field?FreshNativeField(S->GetClass(),Field->Native):nullptr;auto* Captured=F.Values.FindByPredicate([&](const FStandaloneValue& A){return A.Settings==S&&A.Property==P;});if(!P||!Captured||!Captured->Aggregate||!Captured->SameType()||!Captured->SameValue())return F.Bad(TEXT("pcg_data_source_aggregate_role_changed"),TEXT("Exact original selected aggregate/value source must remain current before preparing the writer"));
 FDataSourceAggregateValue Wanted=*Captured->Aggregate;TSharedPtr<FJsonObject> MergedPlan;if(!F.PrepareDataSourceAggregate(S->GetClass(),P,V,Wanted,MergedPlan))return false;
 // Expectation is merged from the actual reflected instance before Modify,
 // including all unprovided authored/show/reference fields, never CDO defaults.
 const auto WantedJson=Wanted.JsonValue();W.R.Data->SetObjectField(TEXT("data_source_aggregate_prewrite_plan"),MergedPlan);W.R.Data->SetObjectField(TEXT("data_source_aggregate_expected"),WantedJson);
 if(!W.Admit(TEXT("data_source_aggregate_before_writer")))return false;
 TGuardValue<UPCGSettings*> Mutable(F.MutableSettings,S);TGuardValue<FProperty*> FieldScope(F.MutableField,P);TGuardValue<UPCGNode*> ActiveNode(F.Held.ActiveSettingsNode,N);TGuardValue<UPCGSettings*> ActiveSettings(F.Held.ActiveSettings,S);
 // Preserve the original always-Modify/raw ImportText/Structural notification,
 // equal-value behavior, cleanup owner and unknown/partial prefix handling.
 if(!SetSettingCore(W,N,S,Name,V,Expected))return false;
 if(!W.Admit(TEXT("data_source_aggregate_planned_readback"))||FreshNativeField(S->GetClass(),P->GetFName())!=P||!Captured->SameType())return false;
 FDataSourceAggregateValue Actual;if(!Actual.Read(S,P))return F.Bad(TEXT("pcg_data_source_aggregate_readback_failed"),TEXT("Returned selected aggregate cannot be copied within the existing current/slot bounds"));const auto ActualJson=Actual.JsonValue();const bool Equal=Wanted.SameValue(Actual);
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("settings_path"),S->GetPathName());Row->SetStringField(TEXT("native_property"),P->GetName());Row->SetObjectField(TEXT("expected"),WantedJson);Row->SetObjectField(TEXT("actual"),ActualJson);Row->SetBoolField(TEXT("planned_typed_readback_passed"),Equal);Row->SetBoolField(TEXT("source_and_unselected_baseline_replaced"),false);Row->SetBoolField(TEXT("regenerated_caches_compared"),false);Row->SetBoolField(TEXT("runtime_generation_verified"),false);W.PropertyRows.Add(MakeShared<FJsonValueObject>(Row));W.R.Data->SetObjectField(TEXT("data_source_aggregate_readback"),Row);
 if(!Equal){auto Failure=MakeShared<FJsonObject>();Failure->SetStringField(TEXT("object"),S->GetPathName());Failure->SetStringField(TEXT("property"),P->GetName());Failure->SetObjectField(TEXT("expected"),WantedJson);Failure->SetObjectField(TEXT("actual"),ActualJson);for(const auto& E:WantedJson->Values){const auto A=ActualJson->TryGetField(FString(E.Key));if(!A||!FJsonValue::CompareEqual(*E.Value,*A)){Failure->SetStringField(TEXT("first_compared_semantic_field"),FString(E.Key));break;}}W.R.Data->SetObjectField(TEXT("data_source_aggregate_first_mismatch"),Failure);return F.Bad(TEXT("pcg_data_source_aggregate_planned_value_mismatch"),TEXT("Actual reflected selector/array values differ from the prewriter actual-instance plan; retain the partial prefix without retry"),S->GetPathName()+TEXT(":")+P->GetName(),Json(WantedJson),Json(ActualJson));}
 // These copies/comparisons have no SDK boundary: accept only the verified cell.
 if(!Captured->Capture())return W.Fail(TEXT("pcg_data_source_aggregate_acceptance_failed"),TEXT("Verified selected reflected value cannot be retained"));W.R.Data->SetObjectField(TEXT("standalone_last_setting_plan_verified"),Row);return true;
}

bool SetStandaloneRawStruct(FWork& W,UPCGNode* N,UPCGSettings* S,const FString& Name,const TSharedPtr<FJsonValue>& V,FString& Expected)
{
 auto& F=*W.Standalone;FProperty* P=FreshNativeField(S->GetClass(),FName(*Name));if(!F.CaptureValues(S))return false;auto* Captured=F.Values.FindByPredicate([&](const FStandaloneValue& A){return A.Settings==S&&A.Property==P;});if(!Captured||!Captured->SameType()||!Captured->SameValue())return F.Bad(TEXT("pcg_raw_struct_selected_role_changed"),TEXT("Exact captured raw struct value required before scratch planning"));FRawNativeStructValue Wanted;TSharedPtr<FJsonObject> Plan;if(!F.PrepareRawStruct(S->GetClass(),P,V,Wanted,Plan,S))return false;
 TGuardValue<UPCGSettings*> Mutable(F.MutableSettings,S);TGuardValue<FProperty*> Field(F.MutableField,P);TGuardValue<UPCGNode*> ActiveNode(F.Held.ActiveSettingsNode,N);TGuardValue<UPCGSettings*> ActiveSettings(F.Held.ActiveSettings,S);
 if(!SetSettingCore(W,N,S,Name,V,Expected))return false;if(!W.Admit(TEXT("raw_struct_before_typed_readback"))||FreshNativeField(S->GetClass(),FName(*Name))!=P)return false;FRawNativeStructValue Actual;if(!Actual.Read(S,P))return false;const bool Equal=Wanted.SameValue(Actual);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("settings_path"),S->GetPathName());Row->SetStringField(TEXT("native_property"),Name);Row->SetObjectField(TEXT("expected"),Wanted.JsonValue());Row->SetObjectField(TEXT("actual"),Actual.JsonValue());Row->SetBoolField(TEXT("planned_typed_readback_passed"),Equal);Row->SetBoolField(TEXT("source_and_unselected_baseline_replaced"),false);Row->SetBoolField(TEXT("runtime_generation_verified"),false);W.PropertyRows.Add(MakeShared<FJsonValueObject>(Row));if(!Equal)return F.Bad(TEXT("pcg_raw_struct_planned_value_mismatch"),TEXT("Actual native struct differs from copied prewriter native import; no retry"),S->GetPathName()+TEXT(":")+Name,Json(Wanted.JsonValue()),Json(Actual.JsonValue()));if(!Captured->Capture())return false;W.R.Data->SetObjectField(TEXT("standalone_last_setting_plan_verified"),Row);return true;
}
bool SetStandaloneSetting(FWork& W,UPCGNode* N,UPCGSettings* S,const FString& Name,const TSharedPtr<FJsonValue>& V,FString& Expected)
{
 FProperty* RawField=FreshNativeField(S->GetClass(),FName(*Name));if(RawSamplingProperty(S->GetClass(),RawField)||(V&&V->Type==EJson::String&&RawVectorProperty(S->GetClass(),RawField)))return SetStandaloneRawStruct(W,N,S,Name,V,Expected);
 if(FreshDataSourceClass(S->GetClass())&&DataSourceAggregateName(FName(*Name)))return SetStandaloneDataSourceAggregate(W,N,S,Name,V,Expected);
 if(StandaloneMetadataClass(S->GetClass()))return SetStandaloneMetadata(W,N,S,Name,V,Expected);
 if(StandaloneSubgraphClass(S->GetClass()))return SetStandaloneSubgraph(W,N,S,Name,V,Expected);
 auto& F=*W.Standalone;auto Input=MakeShared<FJsonObject>();Input->SetField(Name,V);TSharedPtr<FJsonObject> Plans,Aliases;if(!F.Prepare(S->GetClass(),Input,Plans,Aliases))return false;
 const auto* Field=StandaloneField(S->GetClass(),FName(*Name));FProperty* Property=Field?FreshNativeField(S->GetClass(),Field->Native):nullptr;if(!Property||!F.CaptureValues(S))return false;
 TGuardValue<UPCGSettings*> MutableSettings(F.MutableSettings,S);TGuardValue<FProperty*> MutableField(F.MutableField,Property);TGuardValue<UPCGNode*> ActiveNode(F.Held.ActiveSettingsNode,N);TGuardValue<UPCGSettings*> ActiveSetting(F.Held.ActiveSettings,S);
 TSharedPtr<FJsonValue> Payload=V;
 // PA: use the common planner's canonical native enum name at the actual
 // writer boundary; token/display normalization is not a second enum parser.
 if(StandalonePointBoundsClass(S->GetClass())&&CastField<FEnumProperty>(Property)){Payload=Plans->GetObjectField(Field->Alias)->TryGetField(TEXT("payload"));if(!Payload||Payload->Type!=EJson::String)return W.Fail(TEXT("pcg_standalone_point_bounds_enum_plan_missing"),TEXT("Canonical prevalidated enum payload required before Modify"));}
 if(CastField<FStructProperty>(Property)){const auto& A=Plans->GetObjectField(Field->Alias)->GetArrayField(TEXT("expected"));if(A.Num()!=3)return W.Fail(TEXT("pcg_standalone_vector_plan_invalid"),TEXT("Verified native Vector plan must have three components"));Payload=MakeShared<FJsonValueString>(FString::Printf(TEXT("(X=%.17g,Y=%.17g,Z=%.17g)"),A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()));}
 if(CastField<FArrayProperty>(Property))
 {
  const auto& Plan=Plans->GetObjectField(Field->Alias);FString Text;bool Verified=false;if(!Plan->TryGetStringField(TEXT("prepared_point_import_text"),Text)||!Plan->TryGetBoolField(TEXT("point_import_roundtrip_verified"),Verified)||!Verified)return W.Fail(TEXT("pcg_standalone_point_plan_missing"),TEXT("Prepared point import proof unavailable before writer"));Payload=MakeShared<FJsonValueString>(Text);
 }
 // Preserve original always-Modify and explicit Structural notification, even
 // when the request equals the current value. Prepared Vector/point text uses
 // the original checked ImportText branch, not the Toolset ArrayAdd callback path.
 if(!SetSettingCore(W,N,S,Name,Payload,Expected))return false;
  if(StandaloneSpawnerClass(S->GetClass())){
   if(!W.Admit(TEXT("standalone_spawner_before_typed_readback")))return false;
   const auto Plan=Plans->GetObjectField(Field->Alias);const auto ExpectedValue=Plan->TryGetField(TEXT("expected"));if(!ExpectedValue||FreshNativeField(S->GetClass(),Field->Native)!=Property||!StandaloneSpawnerProperty(Property))return W.Fail(TEXT("pcg_standalone_spawner_plan_role_changed"),TEXT("Selected physical native field changed before planned typed readback"));
   TSharedPtr<FJsonValue> Actual;if(const auto* Bool=CastField<FBoolProperty>(Property))Actual=MakeShared<FJsonValueBoolean>(Bool->GetPropertyValue_InContainer(S));else if(const auto* Int=CastField<FIntProperty>(Property))Actual=MakeShared<FJsonValueNumber>(Int->GetPropertyValue_InContainer(S));else Actual=MakeShared<FJsonValueString>(static_cast<FNameProperty*>(Property)->GetPropertyValue_InContainer(S).ToString());
   auto Readback=MakeShared<FJsonObject>();Readback->SetStringField(TEXT("settings_path"),S->GetPathName());Readback->SetStringField(TEXT("native_property"),Field->Native);Readback->SetField(TEXT("expected"),ExpectedValue);Readback->SetField(TEXT("actual"),Actual);auto Wrap=[](const TSharedPtr<FJsonValue>& Value){auto Object=MakeShared<FJsonObject>();Object->SetField(TEXT("value"),Value);return Json(Object);};const bool Equal=Same(Wrap(ExpectedValue),Wrap(Actual));Readback->SetBoolField(TEXT("planned_typed_readback_passed"),Equal);Readback->SetBoolField(TEXT("source_and_unselected_baseline_replaced"),false);Readback->SetBoolField(TEXT("runtime_generation_verified"),false);W.PropertyRows.Add(MakeShared<FJsonValueObject>(Readback));if(!Equal)return W.Fail(TEXT("pcg_standalone_spawner_planned_value_mismatch"),TEXT("Actual native general field differs from prewriter plan; partial prefix retained without retry"));
   auto* Captured=F.Values.FindByPredicate([&](const FStandaloneValue& Value){return Value.Settings==S&&Value.Property==Property;});auto* Spawner=F.Spawners.FindByPredicate([&](const FStandaloneSpawner& Value){return Value.Settings==S;});if(!Captured||!Spawner)return W.Fail(TEXT("pcg_standalone_spawner_selected_field_missing"),TEXT("Original selected value/Spawner baseline absent; no replacement capture"));
   // Readback and selected acceptance below are plain copied/native operations.
   // No SDK/current query can change the selected value between them.
   if(!Captured->Capture()||!Spawner->AcceptSelected(Property->GetFName(),ExpectedValue))return W.Fail(TEXT("pcg_standalone_spawner_value_acceptance_failed"),TEXT("Only the verified selected field may update its stored expectation"));W.R.Data->SetObjectField(TEXT("standalone_last_setting_plan_verified"),Readback);return true;
  }
  if(StandaloneSamplingDifferenceClass(S->GetClass())||StandaloneSpatialCompositionClass(S->GetClass()))
  {
   if(!W.Admit(TEXT("sampling_before_typed_readback")))return false;
   auto* Captured=F.Values.FindByPredicate([&](const FStandaloneValue& Value){return Value.Settings==S&&Value.Property==Property;});const auto Plan=Plans->GetObjectField(Field->Alias);const auto Wanted=Plan->TryGetField(TEXT("expected"));
   if(!Captured||!Wanted||FreshNativeField(S->GetClass(),Field->Native)!=Property||!Captured->SameType())return W.Fail(TEXT("pcg_sampling_selected_field_changed"),TEXT("Original held selected field/type missing before readback"));
   TSharedPtr<FJsonValue> Actual;
   if(const auto* B=CastField<FBoolProperty>(Property))Actual=MakeShared<FJsonValueBoolean>(B->GetPropertyValue_InContainer(S));
   else if(const auto* Float=CastField<FFloatProperty>(Property))Actual=MakeShared<FJsonValueNumber>(Float->GetPropertyValue_InContainer(S));
   else if(CastField<FStructProperty>(Property)){const FVector ActualVector=*Property->ContainerPtrToValuePtr<FVector>(S);Actual=MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(ActualVector.X),MakeShared<FJsonValueNumber>(ActualVector.Y),MakeShared<FJsonValueNumber>(ActualVector.Z)});}
   else Actual=MakeShared<FJsonValueNumber>(Captured->EnumStorage->GetUnsignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(S)));
   auto Wrap=[](const TSharedPtr<FJsonValue>& Value){auto Object=MakeShared<FJsonObject>();Object->SetField(TEXT("value"),Value);return Json(Object);};const bool Equal=Same(Wrap(Wanted),Wrap(Actual));
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("settings_path"),S->GetPathName());Row->SetStringField(TEXT("native_property"),Field->Native);Row->SetField(TEXT("expected"),Wanted);Row->SetField(TEXT("actual"),Actual);Row->SetBoolField(TEXT("planned_typed_readback_passed"),Equal);Row->SetBoolField(TEXT("transient_editor_state"),StandaloneSamplingTransient(S->GetClass(),Property));Row->SetBoolField(TEXT("serialized_persistence_verified"),false);Row->SetBoolField(TEXT("source_and_unselected_baseline_replaced"),false);W.PropertyRows.Add(MakeShared<FJsonValueObject>(Row));
   if(!Equal)return F.Bad(TEXT("pcg_sampling_planned_value_mismatch"),TEXT("Selected value differs from prewriter plan; no retry"),S->GetPathName()+TEXT(":")+Field->Native,Wrap(Wanted),Wrap(Actual));
   // Pure typed readback and selected acceptance have no intervening callback.
   if(!Captured->Capture())return W.Fail(TEXT("pcg_sampling_selected_acceptance_failed"),TEXT("Verified selected cell could not be retained"));W.R.Data->SetObjectField(TEXT("standalone_last_setting_plan_verified"),Row);return true;
  }
  TSharedPtr<FJsonObject> Verified;if(!CubelessPCGUE58::VerifyFreshSettings(W.Graph,N,Aliases,Verified,[&]{return W.Admit(TEXT("standalone_planned_value_current"));}))return W.Fail(TEXT("pcg_standalone_planned_value_mismatch"),TEXT("Actual value differs from prewriter finite plan; partial prefix retained"));
 W.R.Data->SetObjectField(TEXT("standalone_last_setting_plan_verified"),Verified);if(!W.Admit(TEXT("standalone_before_value_acceptance")))return false;
 for(auto& Value:F.Values)if(Value.Settings==S&&Value.Property==Property){if(!Value.Capture())return W.Fail(TEXT("pcg_standalone_point_capture_failed"),TEXT("Verified point value exceeds existing finite bound"));return true;}return W.Fail(TEXT("pcg_standalone_selected_field_missing"),TEXT("Selected exact captured field missing; no replacement baseline adopted"));
}

}
}

namespace Cubeless::UE58::PCGGraphAuthoring
{
namespace
{
bool FreshKeys(const TSharedPtr<FJsonObject>& O,std::initializer_list<const TCHAR*> Accepted)
{if(!O)return false;for(const auto& Pair:O->Values){bool Found=false;for(const TCHAR* K:Accepted)Found|=Same(FString(Pair.Key),K);if(!Found)return false;}return true;}
bool FreshText(const FString& S,bool Empty=false){if((S.IsEmpty()&&!Empty)||S.Len()>=1024||!NoEmbeddedNul(S))return false;for(int32 I=0;I<S.Len();++I){const TCHAR C=S[I];if(C<32||C==127)return false;if(C>=0xd800&&C<=0xdbff){if(++I>=S.Len()||S[I]<0xdc00||S[I]>0xdfff)return false;}else if(C>=0xdc00&&C<=0xdfff)return false;}return true;}
bool FreshInt(const TSharedPtr<FJsonValue>& V,int32& Out){double N=0;if(!V||V->Type!=EJson::Number||!V->TryGetNumber(N)||!FMath::IsFinite(N)||N<MIN_int32||N>MAX_int32||N!=FMath::TruncToDouble(N))return false;Out=static_cast<int32>(N);return true;}

namespace StockSpawner=Cubeless::UE58::PCGContractAuthoring;
bool FreshInlineName(const TSharedPtr<FJsonValue>& Value,FName& Out,FString& Error)
{
 // A JSON number does not retain the Python int-vs-float spelling needed by
 // str(value). Keep this explicit variant pending instead of inventing a name.
 if(!Value||Value->Type==EJson::Array||Value->Type==EJson::Object||Value->Type==EJson::Number){Error=TEXT("Numeric/container-to-name str coercion remains incomplete");return false;}
 const FString Name=StockSpawner::SpawnerInputString(Value);if(Name.Len()>=NAME_SIZE||!NoEmbeddedNul(Name)){Error=TEXT("Inline name exceeds native FName boundary");return false;}Out=FName(*Name);return true;
}
bool FreshInlineMaterialNames(const TSharedPtr<FJsonValue>& Value,TArray<FName>& Out,FString& Error)
{
 if(!Value)return true;TArray<TSharedPtr<FJsonValue>> Items;
 if(Value->Type==EJson::Array)Items=Value->AsArray();
 else if(Value->Type==EJson::Object)
 {
  // Unique-key JSON deserialization appends fields in reader order. This input
  // object has no removal/reinsertion; do not sort or reinterpret key order.
  for(const auto& Pair:Value->AsObject()->Values)Items.Add(MakeShared<FJsonValueString>(FString(Pair.Key)));
 }
 else if(Value->Type==EJson::String)
 {
  const FString Text=Value->AsString();for(int32 Index=0;Index<Text.Len();)
  {
   int32 Width=1;const uint32 First=Text[Index];if(First>=0xD800&&First<=0xDBFF){if(Index+1>=Text.Len()||uint32(Text[Index+1])<0xDC00||uint32(Text[Index+1])>0xDFFF){Error=TEXT("Invalid Unicode material attribute string");return false;}Width=2;}else if(First>=0xDC00&&First<=0xDFFF){Error=TEXT("Invalid Unicode material attribute string");return false;}
   Items.Add(MakeShared<FJsonValueString>(Text.Mid(Index,Width)));Index+=Width;
  }
 }
 else{Error=TEXT("material_override_attributes is not iterable; explicit null/false/number is not an empty list");return false;}
 if(Items.Num()>4096){Error=TEXT("material_override_attributes exceeds4096");return false;}
 for(const auto& Item:Items){FName Name;if(!FreshInlineName(Item,Name,Error))return false;Out.Add(Name);}return true;
}
UObject* FreshInlineAsset(FFreshRequest& F,const TSharedPtr<FJsonValue>& Value,bool Mesh,FString& InputError)
{
 auto& W=F.W;FString Path=StockSpawner::SpawnerInputString(Value);Path=FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();Path.TrimQuotesInline();if(Path.StartsWith(TEXT("/"))&&!Path.Contains(TEXT(".")))Path+=TEXT(".")+FPackageName::GetShortName(Path);
 if(Path.IsEmpty()||!Path.StartsWith(TEXT("/"))||Path.Len()>=1024||Forbidden(Path)){InputError=TEXT("Bounded mounted non-tooling mesh/material path required");return nullptr;}
 if(!W.SDK(Mesh?TEXT("find_loaded_inline_mesh"):TEXT("find_loaded_inline_material")))return nullptr;
 UObject* Asset=StaticFindObject(nullptr,nullptr,*Path,false);const bool Held=F.KeepReturned(Asset,TEXT("inline_loaded_asset"));if(!W.After(TEXT("find_loaded_inline_asset")))return nullptr;
 if(!Held||(Mesh?Asset->GetClass()!=UStaticMesh::StaticClass():!Asset->IsA<UMaterialInterface>())||Forbidden(Asset->GetPathName())||Forbidden(Asset->GetClass()->GetPathName())||Asset->GetPackage()->HasAnyPackageFlags(PKG_EditorOnly))return F.Fail(TEXT("pcg_inline_spawner_asset_not_loaded"),TEXT("Exact already-loaded StaticMesh or runtime MaterialInterface required; no loader or substitution")),nullptr;
 UPackage* Package=Asset->GetPackage();if(!W.Pin(Package)||!W.Pin(Asset->GetClass()))return nullptr;const auto Flags=Asset->GetFlags(),PackageFlags=Package->GetFlags();const uint32 Bits=Package->GetPackageFlags();const bool Dirty=Package->IsDirty();
 F.ExternalAssetChecks.Add([Asset,Package,Flags,PackageFlags,Bits,Dirty]{return Live(Asset)&&Live(Package)&&Asset->GetPackage()==Package&&Asset->GetFlags()==Flags&&Package->GetFlags()==PackageFlags&&uint32(Package->GetPackageFlags())==Bits&&Package->IsDirty()==Dirty;});return Asset;
}
bool FreshInlineSpawnerPlan(FFreshRequest& F,FFreshNodeSpec& N)
{
 auto& W=F.W;const auto Inline=N.Inline;TSharedPtr<FJsonObject> Selector=MakeShared<FJsonObject>();auto Value=Inline->TryGetField(TEXT("mesh_selector"));
 if(StockSpawner::SpawnerInputTruth(Value)){if(Value->Type!=EJson::Object)return F.Fail(TEXT("pcg_inline_spawner_selector_invalid"),TEXT("Truthy mesh_selector must be an object, as required by original inline .get"));Selector=Value->AsObject();}
 auto ModeValue=Selector->TryGetField(TEXT("type"));const FString Mode=StockSpawner::SpawnerInputTruth(ModeValue)?StockSpawner::SpawnerInputString(ModeValue).ToLower():FString();N.AttributePhase=Mode==TEXT("by_attribute")||Mode==TEXT("attribute");
 if(!F.ClassSource(UPCGMeshSelectorWeighted::StaticClass())||!F.ClassSource(UPCGMeshSelectorByAttribute::StaticClass()))return false;
 if(N.AttributePhase)
 {
  auto Name=Selector->TryGetField(TEXT("attribute_name"));if(!Name)Name=MakeShared<FJsonValueString>(TEXT("Meshes"));N.AttributeText=StockSpawner::SpawnerInputString(Name);N.AttributeNameReady=FreshInlineName(Name,N.Attribute,N.AttributeError);
  if(N.AttributeNameReady)N.MaterialsReady=FreshInlineMaterialNames(Selector->TryGetField(TEXT("material_override_attributes")),N.Materials,N.AttributeError);
  // This is an independently pure input-plan error, not an SDK/current failure.
  // Keep it separate so the original later Weighted phase can still execute.
  if(!N.AttributeError.IsEmpty()){auto Error=MakeShared<FJsonObject>();Error->SetStringField(TEXT("node_id"),N.Id);Error->SetStringField(TEXT("phase"),TEXT("by_attribute"));Error->SetStringField(TEXT("error"),N.AttributeError);Error->SetBoolField(TEXT("input_plan_only"),true);W.R.Data->SetObjectField(TEXT("inline_spawner_first_planned_phase_error"),Error);}
 }
 TSharedPtr<FJsonValue> Entries;for(const TCHAR* Key:{TEXT("mesh_entries"),TEXT("static_mesh_entries"),TEXT("spawner_mesh_entries")}){auto Entry=Inline->TryGetField(Key);if(StockSpawner::SpawnerInputTruth(Entry)){Entries=Entry;break;}}if(!Entries){auto Entry=Selector->TryGetField(TEXT("entries"));if(StockSpawner::SpawnerInputTruth(Entry))Entries=Entry;}
 if(Entries)
 {
  N.WeightedPhase=true;if(Entries->Type!=EJson::Array||Entries->AsArray().Num()>4096)N.WeightedError=TEXT("Truthy entries must be an array of at most4096; empty aliases remain a no-op");
  else if(!StockSpawner::BuildWeightedEntryPlan(Entries->AsArray(),N.MeshEntries,
    [&](const TSharedPtr<FJsonValue>& Asset,bool Mesh){return FreshInlineAsset(F,Asset,Mesh,N.WeightedError);},
    [&]{return W.SDK(TEXT("ConstructWeightedDescriptor"));},[&]{return W.After(TEXT("ConstructWeightedDescriptor"));},
    [&]{return W.Admit(TEXT("inline_descriptor_plan_current"));},
    [&](const FPCGMeshSelectorWeightedEntry& Entry){return W.CaptureBudget(1,Json(StockSpawner::WeightedEntryObservation(Entry,true)).Len());},
    [&](const FString& Why){if(N.WeightedError.IsEmpty())N.WeightedError=Why;return false;}))
  {
   if(!W.R.Error.IsEmpty())return false;if(N.WeightedError.IsEmpty())return F.Fail(TEXT("pcg_inline_spawner_plan_unknown"),TEXT("Weighted planner stopped without a copied pure input error"));
  }
 }
 return W.Admit(TEXT("inline_spawner_plan_complete"));
}
struct FFreshSpawnerState
{
 FFreshRequest& Request;FStandaloneSpawner Expected;TSharedPtr<FStandaloneSpawner> Alternative;bool Switching=false;UClass* RequestedType=nullptr;
 bool SavePlanned=false,SavedExports=false;UPCGNode* SaveNode=nullptr;UPCGGraph* SaveGraph=nullptr;TSharedPtr<FJsonObject> SaveFlagsReceipt;
 explicit FFreshSpawnerState(FFreshRequest& In):Request(In){}
 bool Current()const
 {
  const FName Selected=Request.SpawnerMutableSettings==Expected.Settings?Request.SpawnerMutableField:NAME_None;
  if(Switching){if(!Expected.SameValue(Selected,true))return false;const auto Type=Expected.Settings->MeshSelectorType.GetGCPtr();return Type.IsResolved()&&(Type==Expected.Type||Type==RequestedType);}
  return Expected.SameValue(Selected,false,SavedExports)||(Alternative.IsValid()&&Alternative->SameValue(Selected,false,SavedExports));
 }
 bool SaveExports(bool AfterSaved)
 {
  auto& F=Request;auto& W=F.W;
  // Scratch/source and retired selectors never receive this persistent export plan.
  if(!AfterSaved){if(Expected.Package!=F.TargetPackage)return true;if(!Current()||Switching||Alternative.IsValid())return F.Fail(TEXT("pcg_fresh_spawner_save_plan_changed"),TEXT("Exact quiescent Spawner expectation required before save"));
   const auto* Slot=W.Snapshot.NodeSlots.FindByPredicate([&](const FNodeSlot& N){return N.Settings==Expected.Settings&&N.Interface==Expected.Settings;});
   if(!Slot)return F.Fail(TEXT("pcg_fresh_spawner_save_owner_changed"),TEXT("Persistent settings must be the exact direct node interface"));SaveNode=Slot->Node;SaveGraph=F.Target.Get();
  }else if(!SavePlanned)return true;
  if(!Safe()||!Live(SaveGraph)||SaveGraph!=F.Target.Get()||W.Graph!=SaveGraph||W.Snapshot.Graph!=SaveGraph||!Live(SaveNode)||SaveNode->GetOuter()!=SaveGraph||!FreshArrayEquals(SaveGraph,TEXT("Nodes"),W.Snapshot.Nodes)||!W.Snapshot.Nodes.Contains(SaveNode)||!FreshObjectEquals(SaveNode,TEXT("SettingsInterface"),Expected.Settings)||!Live(Expected.Settings)||Expected.Settings->GetOuter()!=SaveNode||Expected.Settings->GetClass()!=UPCGStaticMeshSpawnerSettings::StaticClass()||Expected.Settings->GetPackage()!=F.TargetPackage||!Expected.Settings->MeshSelectorParameters.IsResolved()||Expected.Settings->MeshSelectorParameters!=Expected.Selector||!Live(Expected.Selector)||Expected.Selector->GetClass()!=Expected.Type||Expected.Selector->GetOuter()!=Expected.Settings||Expected.Selector->GetPackage()!=F.TargetPackage)return F.Fail(TEXT("pcg_fresh_spawner_save_owner_changed"),TEXT("Save prefix retained; planned persistent export identity/ownership changed"));
  const auto Flags=[](EObjectFlags V){return FString::Printf(TEXT("%u"),static_cast<uint32>(V));};
  if(!AfterSaved){
   SavePlanned=true;SaveFlagsReceipt=MakeShared<FJsonObject>();SaveFlagsReceipt->SetStringField(TEXT("settings_path"),Expected.Path);SaveFlagsReceipt->SetStringField(TEXT("selector_path"),Expected.SelectorPath);SaveFlagsReceipt->SetStringField(TEXT("settings_flags_before"),Flags(Expected.SettingsFlags));SaveFlagsReceipt->SetStringField(TEXT("selector_flags_before"),Flags(Expected.SelectorFlags));SaveFlagsReceipt->SetStringField(TEXT("settings_flags_planned"),Flags(Expected.SettingsFlags|RF_WasLoaded|RF_LoadCompleted));SaveFlagsReceipt->SetStringField(TEXT("selector_flags_planned"),Flags(Expected.SelectorFlags|RF_WasLoaded|RF_LoadCompleted));SaveFlagsReceipt->SetBoolField(TEXT("after_successful_save_observed"),false);
   const TArray<TSharedPtr<FJsonValue>>* Existing=nullptr;TArray<TSharedPtr<FJsonValue>> Rows;if(W.R.Data->TryGetArrayField(TEXT("spawner_save_export_flags"),Existing))Rows=*Existing;Rows.Add(MakeShared<FJsonValueObject>(SaveFlagsReceipt));W.R.Data->SetArrayField(TEXT("spawner_save_export_flags"),Rows);return true;
  }
  const auto SettingsActual=Expected.Settings->GetFlags(),SelectorActual=Expected.Selector->GetFlags();
  const auto SettingsPlanned=Expected.SettingsFlags|RF_WasLoaded|RF_LoadCompleted,SelectorPlanned=Expected.SelectorFlags|RF_WasLoaded|RF_LoadCompleted;
  SaveFlagsReceipt->SetBoolField(TEXT("after_successful_save_observed"),true);SaveFlagsReceipt->SetStringField(TEXT("settings_flags_after"),Flags(SettingsActual));SaveFlagsReceipt->SetStringField(TEXT("selector_flags_after"),Flags(SelectorActual));SaveFlagsReceipt->SetBoolField(TEXT("planned_flags_match"),SettingsActual==SettingsPlanned&&SelectorActual==SelectorPlanned);
  if((SettingsActual!=SettingsPlanned||SelectorActual!=SelectorPlanned)&&!W.R.Data->HasField(TEXT("first_spawner_save_flags_mismatch"))){
   const bool SettingsMismatch=SettingsActual!=SettingsPlanned;auto Failure=MakeShared<FJsonObject>();Failure->SetStringField(TEXT("role"),SettingsMismatch?TEXT("persistent_settings_export"):TEXT("persistent_selector_export"));Failure->SetStringField(TEXT("object_path"),SettingsMismatch?Expected.Path:Expected.SelectorPath);Failure->SetStringField(TEXT("property"),TEXT("EObjectFlags"));Failure->SetStringField(TEXT("before"),Flags(SettingsMismatch?Expected.SettingsFlags:Expected.SelectorFlags));Failure->SetStringField(TEXT("expected"),Flags(SettingsMismatch?SettingsPlanned:SelectorPlanned));Failure->SetStringField(TEXT("actual"),Flags(SettingsMismatch?SettingsActual:SelectorActual));Failure->SetBoolField(TEXT("expected_actual_observed"),true);Failure->SetBoolField(TEXT("independent_engine_reread"),false);W.R.Data->SetObjectField(TEXT("first_spawner_save_flags_mismatch"),Failure);
  }
  // Only the exact preplanned exports use old|LoadBits after Saved=True. No
  // baseline adopts actual flags; SameValue still checks every authored field.
  SavedExports=true;return true;
 }
};
bool FreshEnrollSpawner(FFreshRequest& F,UPCGSettings* Settings,TSharedPtr<FFreshSpawnerState>& Out)
{
 if(Settings->GetClass()!=UPCGStaticMeshSpawnerSettings::StaticClass())return true;auto* Spawner=static_cast<UPCGStaticMeshSpawnerSettings*>(Settings);
 if(!Spawner->MeshSelectorParameters.IsResolved()||!Spawner->MeshSelectorType.GetGCPtr().IsResolved())return F.Fail(TEXT("pcg_inline_spawner_constructor_unresolved"),TEXT("Resolved constructor selector/type slots required"));
 auto* Selector=Spawner->MeshSelectorParameters.Get();if(!F.KeepReturned(Selector,TEXT("inline_constructor_selector"))||!F.W.Admit(TEXT("inline_constructor_selector_held")))return false;
 if((Selector->GetClass()!=UPCGMeshSelectorWeighted::StaticClass()&&Selector->GetClass()!=UPCGMeshSelectorByAttribute::StaticClass())||Spawner->MeshSelectorType.GetGCPtr()!=Selector->GetClass()||Selector->GetOuter()!=Spawner||Selector->GetPackage()!=Spawner->GetPackage())return F.Fail(TEXT("pcg_inline_spawner_constructor_role_invalid"),TEXT("Actual exact stock settings-owned selector and matching declared type required; constructor/CDO slot values are not assumed"));
 auto State=MakeShared<FFreshSpawnerState>(F);State->Expected.Settings=Spawner;State->Expected.Selector=Selector;State->Expected.Type=Selector->GetClass();
 if(!State->Expected.Capture())return F.Fail(TEXT("pcg_inline_spawner_capture_failed"),TEXT("Actual first instance authored baseline could not be captured"));
 F.SpawnerChecks.Add([State]{return State->Current();});F.SpawnerSaveChecks.Add([State](bool AfterSaved){return State->SaveExports(AfterSaved);});Out=State;return F.W.Admit(TEXT("inline_constructor_expected_bound"));
}
bool FreshSpawnerGeneral(FFreshRequest& F,const FFreshNodeSpec& Spec,UPCGNode* N,const TSharedPtr<FFreshSpawnerState>& State,const TSharedPtr<FJsonObject>& Row)
{
 auto& W=F.W;auto* Settings=State->Expected.Settings;TArray<TSharedPtr<FJsonValue>> Results;Row->SetObjectField(TEXT("inline_spawner_general_field_plan"),Spec.Plans);
 for(const auto& Pair:Spec.Settings->Values)
 {
  const auto* Field=StandaloneSpawnerField(FName(*FString(Pair.Key)));if(!Field)return F.Fail(TEXT("pcg_inline_spawner_general_plan_missing"),TEXT("Existing finite Spawner general field plan required"));const auto Plan=Spec.Plans->GetObjectField(Field->Alias);FProperty* Property=FreshNativeField(Settings->GetClass(),Field->Native);const auto Expected=Plan->TryGetField(TEXT("expected"));
  if(!Expected||!StandaloneSpawnerProperty(Property)||!W.Admit(TEXT("inline_general_before_writer")))return false;
  const auto Before=W.Snapshot;W.RetireMutablePins(N);TGuardValue<bool> Capturing(W.Capturing,true);TGuardValue<UPCGStaticMeshSpawnerSettings*> Mutable(F.SpawnerMutableSettings,Settings);TGuardValue<FName> Selected(F.SpawnerMutableField,Property->GetFName());
  FString Applied;if(!SetSettingCore(W,N,Settings,Field->Native,Plan->TryGetField(TEXT("payload")),Applied))return false;
  if(!W.Admit(TEXT("inline_general_planned_readback"))||FreshNativeField(Settings->GetClass(),Field->Native)!=Property)return false;
  TSharedPtr<FJsonValue> Actual;if(const auto* Bool=CastField<FBoolProperty>(Property))Actual=MakeShared<FJsonValueBoolean>(Bool->GetPropertyValue_InContainer(Settings));else if(const auto* Int=CastField<FIntProperty>(Property))Actual=MakeShared<FJsonValueNumber>(Int->GetPropertyValue_InContainer(Settings));else Actual=MakeShared<FJsonValueString>(static_cast<FNameProperty*>(Property)->GetPropertyValue_InContainer(Settings).ToString());
  auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("native_property"),Field->Native);Receipt->SetField(TEXT("expected"),Expected);Receipt->SetField(TEXT("actual"),Actual);Receipt->SetStringField(TEXT("applied_text"),Applied);Results.Add(MakeShared<FJsonValueObject>(Receipt));Row->SetArrayField(TEXT("inline_spawner_general_readback"),Results);
  auto Wrap=[](const TSharedPtr<FJsonValue>& V){auto J=MakeShared<FJsonObject>();J->SetField(TEXT("value"),V);return Json(J);};if(!Same(Wrap(Expected),Wrap(Actual)))return F.Fail(TEXT("pcg_inline_spawner_general_mismatch"),TEXT("Actual general field differs from prewriter plan"));
  if(!State->Expected.AcceptSelected(Property->GetFName(),Expected))return F.Fail(TEXT("pcg_inline_spawner_selected_accept_failed"),TEXT("Only the verified selected general expected value may change"));F.SpawnerMutableSettings=nullptr;F.SpawnerMutableField=NAME_None;if(!State->Current())return false;
  if(!Transition(W,Before,TEXT("notify"),N))return false;
 }
 return W.Admit(TEXT("inline_general_complete"));
}
bool FreshSwitchSpawner(FFreshRequest& F,const TSharedPtr<FFreshSpawnerState>& State,UClass* Requested,const TSharedPtr<FJsonObject>& Step)
{
 auto& W=F.W;auto* Settings=State->Expected.Settings;auto* Old=State->Expected.Selector;const bool Allocating=State->Expected.Type!=Requested;
 Step->SetStringField(TEXT("selector_before"),State->Expected.SelectorPath);Step->SetStringField(TEXT("requested_type"),Requested->GetPathName());Step->SetBoolField(TEXT("allocation_required"),Allocating);Step->SetBoolField(TEXT("SetMeshSelectorType_entered"),false);Step->SetBoolField(TEXT("SetMeshSelectorType_returned"),false);
 if(!W.SDK(TEXT("inline_SetMeshSelectorType"),true))return false;Step->SetBoolField(TEXT("SetMeshSelectorType_entered"),true);
 if(!Allocating){Settings->SetMeshSelectorType(Requested);Step->SetBoolField(TEXT("SetMeshSelectorType_returned"),true);return W.After(TEXT("inline_SetMeshSelectorType"));}
 FObjectIdentity* OldRole=nullptr;FObjectIdentity* SettingsRole=nullptr;for(const auto& Object:W.Objects){if(!Object->Retired&&Object->Hold.Get()==Old)OldRole=Object.Get();if(!Object->Retired&&Object->Hold.Get()==Settings)SettingsRole=Object.Get();}
 if(!OldRole||!SettingsRole)return F.Fail(TEXT("pcg_inline_spawner_retirement_role_missing"),TEXT("Exact held old selector/settings required"));const TWeakObjectPtr<UPCGMeshSelectorBase> OldWeak(Old);OldRole->Retired=true;State->Switching=true;State->RequestedType=Requested;
 Settings->SetMeshSelectorType(Requested);Step->SetBoolField(TEXT("SetMeshSelectorType_returned"),true);
 TStrongObjectPtr<UPCGMeshSelectorBase> Returned(nullptr);if(Safe()&&SettingsRole->Exact()&&Settings->MeshSelectorParameters.IsResolved()){auto* Slot=Settings->MeshSelectorParameters.Get();if(Live(Slot))Returned.Reset(Slot);}
 if(!W.After(TEXT("inline_SetMeshSelectorType")))return false;
 Step->SetBoolField(TEXT("new_slot_held"),Returned.IsValid());Step->SetBoolField(TEXT("old_weak_retired"),!OldWeak.IsValid());Step->SetBoolField(TEXT("old_memory_destruction_proven"),false);
 if(!Returned.IsValid())return F.Fail(TEXT("pcg_inline_spawner_return_missing"),TEXT("No held live selector after actual public type switch"));auto* New=Returned.Get();if(!F.KeepReturned(New,TEXT("inline_new_selector")))return false;
 if(OldWeak.IsValid()||TWeakObjectPtr<UPCGMeshSelectorBase>(New)==OldWeak||New->GetClass()!=Requested||New->GetOuter()!=Settings||New->GetPackage()!=Settings->GetPackage()||New->GetOutermost()!=Settings->GetOutermost()||Settings->MeshSelectorType.GetGCPtr()!=Requested||Settings->MeshSelectorParameters!=New)return F.Fail(TEXT("pcg_inline_spawner_return_role_changed"),TEXT("Exact stock new selector identity/owner/type and old retirement required"));
 // Only the newborn selector fields receive their first normal baseline. All
 // settings/inherited/packer expectations stay in Expected from constructor.
 State->Expected.Selector=New;State->Expected.Type=Requested;State->Expected.TypeHandle=Requested;State->Expected.SelectorFlags=New->GetFlags();State->Expected.SelectorPath=New->GetPathName();State->Expected.CaptureSelectorValues();State->Switching=false;State->RequestedType=nullptr;
 Step->SetStringField(TEXT("selector_after"),State->Expected.SelectorPath);Step->SetBoolField(TEXT("new_role_current_verified"),State->Current());return W.Admit(TEXT("inline_new_selector_bound"));
}
bool FreshEditSpawner(FFreshRequest& F,const TSharedPtr<FFreshSpawnerState>& State,const TCHAR* Name,const TFunctionRef<void(FStandaloneSpawner&)>& Plan,const TFunctionRef<void()>& Assign,const TSharedPtr<FJsonObject>& Step)
{
 auto& W=F.W;auto* Selector=State->Expected.Selector;FProperty* Property=FreshNativeField(Selector->GetClass(),Name);if(!Property||!W.Admit(TEXT("inline_selector_before_edit")))return F.Fail(TEXT("pcg_inline_spawner_property_missing"),TEXT("Exact planned selector property required"));
 auto Planned=MakeShared<FStandaloneSpawner>(State->Expected);Plan(*Planned);
 if(!W.Write(TEXT("inline_Selector_Modify"),[&]{Selector->Modify();}))return false;
 if(FreshNativeField(Selector->GetClass(),Name)!=Property)return F.Fail(TEXT("pcg_inline_spawner_property_changed"),TEXT("Selected property binding changed after Modify"));
 if(!W.Write(TEXT("inline_Selector_PreEditChange"),[&]{Selector->PreEditChange(Property);}))return false;
 State->Alternative=Planned;if(!W.SDK(TEXT("inline_AssignSelectorField"),true))return false;Assign();Step->SetStringField(TEXT("last_field_assigned"),Name);if(!W.After(TEXT("inline_AssignSelectorField")))return false;
 if(!Planned->SameValue())return F.Fail(TEXT("pcg_inline_spawner_assignment_mismatch"),TEXT("Actual selected assignment differs from old-baseline plus planned one-field change"));State->Expected=*Planned;State->Alternative.Reset();
 if(FreshNativeField(Selector->GetClass(),Name)!=Property)return F.Fail(TEXT("pcg_inline_spawner_property_changed"),TEXT("Selected property binding changed before notify"));
 if(!W.SDK(TEXT("inline_Selector_PostEditChangeProperty"),true))return false;FPropertyChangedEvent Event(Property,EPropertyChangeType::ValueSet);Selector->PostEditChangeProperty(Event);return W.After(TEXT("inline_Selector_PostEditChangeProperty"));
}
TSharedPtr<FJsonObject> FreshSpawnerObservation(const FStandaloneSpawner& State)
{
 auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("settings_path"),State.Path);J->SetStringField(TEXT("selector_path"),State.SelectorPath);J->SetStringField(TEXT("selector_class"),State.Type->GetName());J->SetBoolField(TEXT("runtime_generation_verified"),false);
 if(State.Type==UPCGMeshSelectorWeighted::StaticClass()){TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& Entry:State.Entries)Rows.Add(MakeShared<FJsonValueObject>(StockSpawner::WeightedEntryObservation(Entry,true)));J->SetArrayField(TEXT("mesh_entries"),Rows);}
 else{J->SetStringField(TEXT("attribute_name"),State.Attribute.ToString());TArray<TSharedPtr<FJsonValue>> Names;for(const auto& Name:State.Materials)Names.Add(MakeShared<FJsonValueString>(Name.ToString()));J->SetArrayField(TEXT("material_override_attributes"),Names);J->SetBoolField(TEXT("use_attribute_material_overrides"),State.UseMaterials);}
 return J;
}
bool FreshApplyInlineSpawner(FFreshRequest& F,const FFreshNodeSpec& Spec,const TSharedPtr<FFreshSpawnerState>& State,const TSharedPtr<FJsonObject>& Row)
{
 TArray<TSharedPtr<FJsonValue>> Steps,Errors;Row->SetArrayField(TEXT("inline_spawner_steps"),Steps);FString PhaseError;
 auto Step=[&](const TCHAR* Phase){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("phase"),Phase);Steps.Add(MakeShared<FJsonValueObject>(J));Row->SetArrayField(TEXT("inline_spawner_steps"),Steps);return J;};
 if(Spec.AttributePhase)
 {
  auto J=Step(TEXT("by_attribute"));if(!FreshSwitchSpawner(F,State,UPCGMeshSelectorByAttribute::StaticClass(),J))return false;
  auto* Target=static_cast<UPCGMeshSelectorByAttribute*>(State->Expected.Selector);
  if(Spec.AttributeNameReady)
  {
   if(!FreshEditSpawner(F,State,TEXT("AttributeName"),[&](auto& P){P.Attribute=Spec.Attribute;},[&]{Target->AttributeName=Spec.Attribute;},J))return false;
   if(Spec.MaterialsReady)
   {
    if(!FreshEditSpawner(F,State,TEXT("MaterialOverrideAttributes"),[&](auto& P){P.Materials=Spec.Materials;},[&]{Target->MaterialOverrideAttributes=Spec.Materials;},J)
      ||!FreshEditSpawner(F,State,TEXT("bUseAttributeMaterialOverrides"),[&](auto& P){P.UseMaterials=!Spec.Materials.IsEmpty();},[&]{Target->bUseAttributeMaterialOverrides=!Spec.Materials.IsEmpty();},J))return false;
   }
  }
  if(!F.W.Admit(TEXT("inline_by_attribute_phase_current")))return false;
  J->SetObjectField(TEXT("actual_readback"),FreshSpawnerObservation(State->Expected));PhaseError=Spec.AttributeError;
  if(PhaseError.IsEmpty()&&!State->Expected.Attribute.ToString().Equals(Spec.AttributeText,ESearchCase::CaseSensitive))PhaseError=TEXT("Original requested attribute string differs from actual native FName snapshot");
  J->SetBoolField(TEXT("verified"),PhaseError.IsEmpty());if(!PhaseError.IsEmpty())
  {
   auto Error=MakeShared<FJsonObject>();Error->SetStringField(TEXT("mesh_selector"),TEXT("by_attribute"));Error->SetStringField(TEXT("error"),PhaseError);Error->SetBoolField(TEXT("known_current_after_phase"),true);Errors.Add(MakeShared<FJsonValueObject>(Error));Row->SetArrayField(TEXT("inline_spawner_phase_errors"),Errors);J->SetObjectField(TEXT("phase_error"),Error);
   // Global SDK/identity errors are never cleared. Only this copied, known-current
   // input/readback error is deferred until the original later Weighted phase.
  }
 }
 if(Spec.WeightedPhase)
 {
  auto J=Step(TEXT("weighted"));if(!FreshSwitchSpawner(F,State,UPCGMeshSelectorWeighted::StaticClass(),J))return false;auto* Target=static_cast<UPCGMeshSelectorWeighted*>(State->Expected.Selector);
  if(Spec.WeightedError.IsEmpty())
  {
   if(!FreshEditSpawner(F,State,TEXT("MeshEntries"),[&](auto& P){P.Entries=Spec.MeshEntries;},[&]{Target->MeshEntries=Spec.MeshEntries;},J))return false;
  }
  if(!F.W.Admit(TEXT("inline_weighted_phase_current")))return false;J->SetObjectField(TEXT("actual_readback"),FreshSpawnerObservation(State->Expected));J->SetBoolField(TEXT("verified"),Spec.WeightedError.IsEmpty());
  if(!Spec.WeightedError.IsEmpty()){auto Error=MakeShared<FJsonObject>();Error->SetStringField(TEXT("mesh_entries"),TEXT("weighted"));Error->SetStringField(TEXT("error"),Spec.WeightedError);Error->SetBoolField(TEXT("known_current_after_phase"),true);Errors.Add(MakeShared<FJsonValueObject>(Error));Row->SetArrayField(TEXT("inline_spawner_phase_errors"),Errors);J->SetObjectField(TEXT("phase_error"),Error);if(PhaseError.IsEmpty())PhaseError=Spec.WeightedError;}
 }
 if(!F.W.Admit(TEXT("inline_spawner_final_readback")))return false;Row->SetObjectField(TEXT("inline_spawner_final_readback"),FreshSpawnerObservation(State->Expected));Row->SetBoolField(TEXT("inline_spawner_semantic_expected_verified"),true);if(!PhaseError.IsEmpty())return F.Fail(TEXT("pcg_inline_spawner_phase_failed"),PhaseError);return true;
}

bool FreshParse(FFreshRequest& F,const TSharedPtr<FJsonObject>& P)
{
 if(!FreshKeys(P,{TEXT("graph_spec"),TEXT("overwrite_existing"),TEXT("allow_overwrite_non_temp"),TEXT("save"),TEXT("dry_run"),TEXT("operation_id"),TEXT("request_owned_general")}))return F.Fail(TEXT("pcg_graph_invalid_params_json"),TEXT("Only the normalized existing six-input carrier is supported"));
 if(P->HasField(TEXT("request_owned_general"))&&!P->TryGetBoolField(TEXT("request_owned_general"),F.GeneralNative))return F.Fail(TEXT("pcg_general_worker_invalid"),TEXT("Internal general worker selection must be boolean"));
  const TSharedPtr<FJsonObject>* Spec=nullptr;if(!P->TryGetObjectField(TEXT("graph_spec"),Spec)||!FreshKeys(*Spec,{TEXT("asset_path"),TEXT("nodes"),TEXT("edges"),TEXT("forbidden_dependency_prefixes")}))return F.Fail(TEXT("pcg_invalid_graph_spec"),TEXT("Normalized graph_spec object required"));
 if(!P->TryGetBoolField(TEXT("save"),F.Save)||!P->TryGetBoolField(TEXT("dry_run"),F.DryRun)||!P->TryGetBoolField(TEXT("overwrite_existing"),F.Overwrite)||!P->TryGetBoolField(TEXT("allow_overwrite_non_temp"),F.AllowOverwrite))return F.Fail(TEXT("pcg_boolean_invalid"),TEXT("Existing boolean input values required"));
 if(!(*Spec)->TryGetStringField(TEXT("asset_path"),F.PackagePath)||!FreshText(F.PackagePath)||!F.PackagePath.StartsWith(TEXT("/Game/"))||F.PackagePath.Contains(TEXT(".."))||F.PackagePath.Contains(TEXT("\\"))||F.PackagePath.Contains(TEXT("."))||!FPackageName::IsValidLongPackageName(F.PackagePath))return F.Fail(TEXT("pcg_invalid_asset_path"),TEXT("Exact /Game long package name required"));
 F.AssetName=FPackageName::GetShortName(F.PackagePath);F.Folder=FPackageName::GetLongPackagePath(F.PackagePath);F.ObjectPath=F.PackagePath+TEXT(".")+F.AssetName;
 if(!FPackageName::TryConvertLongPackageNameToFilename(F.PackagePath,F.File,FPackageName::GetAssetPackageExtension()))return F.Fail(TEXT("pcg_invalid_asset_path"),TEXT("Mounted package file path unavailable"));F.File=FPaths::ConvertRelativePathToFull(F.File);
 const TArray<TSharedPtr<FJsonValue>> *Rows=nullptr,*EdgeRows=nullptr,*Prefixes=nullptr;
 if(!(*Spec)->TryGetArrayField(TEXT("nodes"),Rows)||(!F.GeneralNative&&Rows->IsEmpty())||Rows->Num()>256||!(*Spec)->TryGetArrayField(TEXT("edges"),EdgeRows)||EdgeRows->Num()>2048||!(*Spec)->TryGetArrayField(TEXT("forbidden_dependency_prefixes"),Prefixes)||Prefixes->Num()>64)return F.Fail(TEXT("pcg_invalid_graph_spec"),TEXT("Nodes0..256 in request-owned general worker, otherwise1..256; edges0..2048/prefixes0..64 required"));
 TSet<FString> Aliases{TEXT("input"),TEXT("output"),TEXT("__input__"),TEXT("__output__")};
 for(const auto& Value:*Rows)
 {
  if(!Value||Value->Type!=EJson::Object)return F.Fail(TEXT("pcg_invalid_graph_spec"),TEXT("Node must be an object"));auto O=Value->AsObject();FFreshNodeSpec N;
  if(!FreshKeys(O,{TEXT("id"),TEXT("title"),TEXT("settings_class"),TEXT("position"),TEXT("settings"),TEXT("mesh_entries"),TEXT("static_mesh_entries"),TEXT("spawner_mesh_entries"),TEXT("mesh_selector")})||!O->TryGetStringField(TEXT("id"),N.Id)||!O->TryGetStringField(TEXT("title"),N.Title)||!O->TryGetStringField(TEXT("settings_class"),N.ClassName)||!FreshText(N.Id)||!FreshText(N.Title)||!FreshText(N.ClassName))return F.Fail(TEXT("pcg_invalid_graph_spec"),TEXT("Normalized bounded node id/title/class required"));
  if(Aliases.Contains(N.Id.ToLower())||Aliases.Contains(N.Title.ToLower()))return F.Fail(TEXT("pcg_duplicate_or_reserved_node_id"),TEXT("Node id/title collides with a factory or earlier alias"));Aliases.Add(N.Id.ToLower());Aliases.Add(N.Title.ToLower());
  const TArray<TSharedPtr<FJsonValue>>* Pos=nullptr;const TSharedPtr<FJsonObject>* Settings=nullptr;if(!O->TryGetArrayField(TEXT("position"),Pos)||Pos->Num()!=2||!FreshInt((*Pos)[0],N.X)||!FreshInt((*Pos)[1],N.Y)||!O->TryGetObjectField(TEXT("settings"),Settings))return F.Fail(TEXT("pcg_position_invalid"),TEXT("Exact two int32 position and settings object required"));N.Settings=*Settings;N.Inline=O;F.Nodes.Add(MoveTemp(N));
 }
 for(const auto& Value:*Prefixes){FString S;if(!Value||!Value->TryGetString(S)||!FreshText(S)||!S.StartsWith(TEXT("/")))return F.Fail(TEXT("pcg_dependency_policy_invalid"),TEXT("Bounded mounted dependency prefixes required"));F.ForbiddenPrefixes.Add(S);}
 if(!F.ForbiddenPrefixes.IsEmpty()&&!F.Save)return F.Fail(TEXT("pcg_dependency_policy_requires_save"),TEXT("save=True required for dependency audit"));
 for(const auto& Value:*EdgeRows)
 {
  if(!Value||Value->Type!=EJson::Object||!FreshKeys(Value->AsObject(),{TEXT("from"),TEXT("to"),TEXT("from_pin"),TEXT("to_pin")}))return F.Fail(TEXT("pcg_edge_invalid"),TEXT("Normalized exact endpoint edge required"));
  for(const TCHAR* K:{TEXT("from"),TEXT("to"),TEXT("from_pin"),TEXT("to_pin")}){FString S;if(!Value->AsObject()->TryGetStringField(K,S)||!FreshText(S))return F.Fail(TEXT("pcg_edge_invalid"),TEXT("Bounded edge endpoint/label required"));}F.Edges.Add(Value);
 }
 // No loader is authorized by a settings dependency string in this finite cohort.
 TFunction<bool(const TSharedPtr<FJsonValue>&)> Strings=[&](const TSharedPtr<FJsonValue>& V){if(V->Type==EJson::String){const FString S=V->AsString();if(S.StartsWith(TEXT("/"))&&Forbidden(S))return false;for(const auto& Prefix:F.ForbiddenPrefixes)if(S.StartsWith(Prefix,ESearchCase::CaseSensitive))return false;}else if(V->Type==EJson::Array){if(V->AsArray().Num()>4096)return false;for(const auto& X:V->AsArray())if(!Strings(X))return false;}else if(V->Type==EJson::Object){for(const auto& Pair:V->AsObject()->Values)if(!FreshText(FString(Pair.Key))||!Strings(Pair.Value))return false;}return true;};
 for(const auto& N:F.Nodes)if(Forbidden(N.ClassName)||!Strings(MakeShared<FJsonValueObject>(N.Inline)))return F.Fail(TEXT("pcg_graph_spec_preflight_failed"),TEXT("Tooling/forbidden dependency or unsupported bounded setting payload"));
 return true;
}
// ADT: a request-owned constructor route; no Python UObject/struct/container is returned.
// The existing finite contracts remain unchanged. This route admits only public
// scalar fields; recursive dict/list/object coercion is intentionally not raw ImportText.
bool FreshKnownFiniteClass(UClass* C)
{
 if(!C)return false;const FName N=C->GetFName();
 for(const TCHAR* Name:{TEXT("PCGCreatePointsGridSettings"),TEXT("PCGCreatePointsSettings"),TEXT("PCGDistanceSettings"),TEXT("PCGDensityRemapSettings"),TEXT("PCGDensityFilterSettings"),TEXT("PCGAttributeNoiseSettings"),TEXT("PCGAttributeFilteringSettings"),TEXT("PCGGetActorPropertySettings"),TEXT("PCGRandomChoiceSettings"),TEXT("PCGPointExtentsModifierSettings"),TEXT("PCGBoundsModifierSettings"),TEXT("PCGStaticMeshSpawnerSettings"),TEXT("PCGTransformPointsSettings"),TEXT("PCGSurfaceSamplerSettings"),TEXT("PCGDifferenceSettings"),TEXT("PCGDataFromActorSettings"),TEXT("PCGGetLandscapeSettings"),TEXT("PCGProjectionSettings")})if(N==Name)return true;
 return false;
}
// AEF: closed native math storage only. Original pcg_tools.py1075-1136 uses
// Vector/Rotator first-three arrays and recursively merges dicts into the actual
// returned struct. This is not an object/container/custom-struct serializer.
enum class EGeneralMath : uint8 { None,Vector,Vector2D,Rotator,Quat,LinearColor,Transform,Box };
EGeneralMath GeneralMathType(UScriptStruct* T,int32& Size,int32& Alignment)
{
 if(!Live(T)||T->GetClass()!=UScriptStruct::StaticClass()||!Same(T->GetPackage()->GetName(),TEXT("/Script/CoreUObject")))return EGeneralMath::None;
#define AEF_MATH_ROLE(Name,Type) if(T==TBaseStructure<Type>::Get()){Size=sizeof(Type);Alignment=alignof(Type);return EGeneralMath::Name;}
 AEF_MATH_ROLE(Vector,FVector) AEF_MATH_ROLE(Vector2D,FVector2D) AEF_MATH_ROLE(Rotator,FRotator)
 AEF_MATH_ROLE(Quat,FQuat) AEF_MATH_ROLE(LinearColor,FLinearColor) AEF_MATH_ROLE(Transform,FTransform)
 // UE58 registers the aliased Box reflected type but provides no TBaseStructure<FBox>::Get.
 // Use its already-loaded exact native identity; Bind still checks full native layout.
 if(T==StaticFindObject(UScriptStruct::StaticClass(),nullptr,TEXT("/Script/CoreUObject.Box"))){Size=sizeof(FBox);Alignment=alignof(FBox);return EGeneralMath::Box;}
#undef AEF_MATH_ROLE
 return EGeneralMath::None;
}
UScriptStruct* GeneralMathResident(const TObjectPtr<UScriptStruct>& Handle)
{
 if(!Handle.IsResolved()||Handle==nullptr)return nullptr;
 auto* T=Cast<UScriptStruct>(StaticFindObject(UScriptStruct::StaticClass(),nullptr,*Handle.GetPathName()));
 return Live(T)&&Handle==T?T:nullptr;
}
FString GeneralMathKey(FString Name){Name.ReplaceInline(TEXT("_"),TEXT(""));return Name.ToLower();}
struct FGeneralMathLease
{
 FFreshRequest& F;UClass* Class;FStructProperty* Property;FFreshField Root;UScriptStruct* Struct=nullptr;
 TArray<FStandalonePointLayout> Layouts;TArray<TStrongObjectPtr<UObject>> Holds;
 FGeneralMathLease(FFreshRequest& In,UClass* C,FStructProperty* P):F(In),Class(C),Property(P),Root(P){}
 bool Exact()const
 {
  const auto* Source=F.Sources.FindByPredicate([&](const FFreshClass& S){return S.Class==Class;});
  if(!Safe()||!Source||!Source->Exact()||FreshNativeField(Class,Root.Name)!=Property||!Root.Exact()||Property->HasSetterOrGetter()||!Property->Struct.IsResolved()||Property->Struct!=Struct)return false;
  for(const auto& L:Layouts)if(!L.Exact())return false;return true;
 }
 bool Bind()
 {
  Struct=GeneralMathResident(Property->Struct);if(!Struct)return false;
  TArray<UScriptStruct*> Pending{Struct};TSet<UScriptStruct*> Seen;
  // Hold the native types/complete immediate field inventory before W.Pin can notify.
  while(!Pending.IsEmpty())
  {
   auto* T=Pending.Pop();if(Seen.Contains(T))continue;if(Seen.Num()>=4)return false;Seen.Add(T);
   int32 Size=0,Alignment=0;const auto Kind=GeneralMathType(T,Size,Alignment);
   if(Kind==EGeneralMath::None||!(T->StructFlags&STRUCT_Native)||T->GetSuperStruct()||T->GetStructureSize()!=Size||T->GetMinAlignment()!=Alignment||T->GetPackage()->IsDirty()||!T->GetPackage()->HasAnyPackageFlags(PKG_CompiledIn))return false;
   Holds.Emplace(T);Holds.Emplace(T->GetPackage());FStandalonePointLayout L;L.Struct=T;L.Package=T->GetPackage();L.Path=T->GetPathName();L.PackageFlags=L.Package->GetPackageFlags();L.ObjectFlags=T->GetFlags();L.Flags=T->StructFlags;L.Size=Size;L.Alignment=Alignment;
   TSet<FName> Names;
   for(TFieldIterator<FProperty> It(T);It;++It)
   {
    FProperty* P=*It;if(P->GetOwnerStruct()!=T||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetOffset_ForInternal()<0||P->GetSize()>Size-P->GetOffset_ForInternal())return false;
    const FName N=P->GetFName();if(Names.Contains(N))return false;Names.Add(N);L.Inventory.Add(P);L.Fields.Emplace(P);
    if(auto* Child=CastField<FStructProperty>(P))
    {
     auto* S=GeneralMathResident(Child->Struct);int32 ChildSize=0,ChildAlign=0;const auto ChildKind=GeneralMathType(S,ChildSize,ChildAlign);
     const bool Role=(Kind==EGeneralMath::Transform&&((N==TEXT("Rotation")&&ChildKind==EGeneralMath::Quat)||((N==TEXT("Translation")||N==TEXT("Scale3D"))&&ChildKind==EGeneralMath::Vector)))||(Kind==EGeneralMath::Box&&(N==TEXT("Min")||N==TEXT("Max"))&&ChildKind==EGeneralMath::Vector);
     if(!Role||P->GetSize()!=ChildSize)return false;L.Nested.Emplace(Child,Child->Struct);Pending.Add(S);
    }
    else if(Kind==EGeneralMath::Box&&N==TEXT("IsValid"))
    {const auto* B=CastField<FBoolProperty>(P);if(!B||!B->IsNativeBool()||P->GetSize()!=sizeof(bool))return false;}
    else
    {
     const bool Named=(Kind==EGeneralMath::Vector&&(N==TEXT("X")||N==TEXT("Y")||N==TEXT("Z")))||(Kind==EGeneralMath::Vector2D&&(N==TEXT("X")||N==TEXT("Y")))||(Kind==EGeneralMath::Rotator&&(N==TEXT("Pitch")||N==TEXT("Yaw")||N==TEXT("Roll")))||(Kind==EGeneralMath::Quat&&(N==TEXT("X")||N==TEXT("Y")||N==TEXT("Z")||N==TEXT("W")))||(Kind==EGeneralMath::LinearColor&&(N==TEXT("R")||N==TEXT("G")||N==TEXT("B")||N==TEXT("A")));
     if(!Named||(Kind==EGeneralMath::LinearColor?(P->GetClass()!=FFloatProperty::StaticClass()||P->GetSize()!=sizeof(float)):(P->GetClass()!=FDoubleProperty::StaticClass()||P->GetSize()!=sizeof(double))))return false;
    }
   }
   const int32 Count=Kind==EGeneralMath::Vector2D?2:(Kind==EGeneralMath::Quat||Kind==EGeneralMath::LinearColor?4:3);
   if(Names.Num()!=Count||!L.Exact())return false;Layouts.Add(MoveTemp(L));
  }
  return Property->GetSize()==Struct->GetStructureSize()&&Exact();
 }
 bool SameStorage(UScriptStruct* T,const void* A,const void* B,int32 Depth=0)const
 {
  if(!A||!B||Depth>2)return false;
  for(TFieldIterator<FProperty> It(T);It;++It)
  {
   auto* P=*It;const void* X=P->ContainerPtrToValuePtr<void>(A);const void* Y=P->ContainerPtrToValuePtr<void>(B);
   if(auto* N=CastField<FNumericProperty>(P)){const double V=N->GetFloatingPointPropertyValue(X),U=N->GetFloatingPointPropertyValue(Y);if(!FMath::IsFinite(V)||!FMath::IsFinite(U)||V!=U)return false;}
   else if(auto* Bool=CastField<FBoolProperty>(P)){if(Bool->GetPropertyValue(X)!=Bool->GetPropertyValue(Y))return false;}
   else if(auto* S=CastField<FStructProperty>(P)){auto* Child=GeneralMathResident(S->Struct);if(!Child||!SameStorage(Child,X,Y,Depth+1))return false;}else return false;
  }return true;
 }
 bool SameValue(const void* A,const void* B)const{return Exact()&&SameStorage(Struct,A,B);}
 void Seed(void* D)const
 {
  int32 Size=0,Alignment=0;switch(GeneralMathType(Struct,Size,Alignment))
  {
   case EGeneralMath::Vector:*static_cast<FVector*>(D)=FVector::ZeroVector;break;
   case EGeneralMath::Vector2D:*static_cast<FVector2D*>(D)=FVector2D::ZeroVector;break;
   case EGeneralMath::Rotator:*static_cast<FRotator*>(D)=FRotator::ZeroRotator;break;
   case EGeneralMath::Quat:*static_cast<FQuat*>(D)=FQuat::Identity;break;
   case EGeneralMath::LinearColor:*static_cast<FLinearColor*>(D)=FLinearColor(0,0,0,1);break;
   case EGeneralMath::Transform:*static_cast<FTransform*>(D)=FTransform::Identity;break;
   case EGeneralMath::Box:*static_cast<FBox*>(D)=FBox(ForceInit);break;default:break;
  }
 }
 bool Merge(UScriptStruct* T,void* D,const TSharedPtr<FJsonValue>& V,int32 Depth=0)
 {
  if(!V||Depth>2||!Exact())return false;int32 Size=0,Alignment=0;const auto Kind=GeneralMathType(T,Size,Alignment);
  if(V->Type==EJson::Array&&(Kind==EGeneralMath::Vector||Kind==EGeneralMath::Rotator))
  {
   const auto& A=V->AsArray();if(A.Num()<3)return F.Fail(TEXT("pcg_general_math_array_shape"),TEXT("Legacy Vector/Rotator array requires at least three finite JSON numbers; wider float coercions remain incomplete"));
   double X[3];for(int32 I=0;I<3;++I){if(!A[I]||A[I]->Type!=EJson::Number||!FMath::IsFinite(X[I]=A[I]->AsNumber()))return F.Fail(TEXT("pcg_general_math_numeric_required"),TEXT("Finite JSON numeric math components required"));}
   if(Kind==EGeneralMath::Vector)*static_cast<FVector*>(D)=FVector(X[0],X[1],X[2]);else *static_cast<FRotator*>(D)=FRotator(X[0],X[1],X[2]);return true;
  }
  if(V->Type!=EJson::Object||V->AsObject()->Values.Num()>4)return F.Fail(TEXT("pcg_general_math_object_required"),TEXT("Native math field object required; only Vector/Rotator additionally use legacy arrays"));
  TSet<FName> Seen;
  for(const auto& Pair:V->AsObject()->Values)
  {
   FProperty* P=nullptr;const FString Key=GeneralMathKey(FString(Pair.Key));for(TFieldIterator<FProperty> It(T);It;++It)if(GeneralMathKey(It->GetName())==Key){if(P)return false;P=*It;}
   if(!P||Seen.Contains(P->GetFName())||PropertyAccessUtil::CanSetPropertyValue(P,PropertyAccessUtil::EditorReadOnlyFlags,false)!=EPropertyAccessResultFlags::Success)return F.Fail(TEXT("pcg_general_math_member_unsupported"),TEXT("Unique editable native math member required; arbitrary keys/reference/container/custom structs remain incomplete"));Seen.Add(P->GetFName());
   void* Address=P->ContainerPtrToValuePtr<void>(D);const auto& Value=Pair.Value;
   if(auto* N=CastField<FNumericProperty>(P)){if(!Value||Value->Type!=EJson::Number||!FMath::IsFinite(Value->AsNumber())||!PutNumeric(F.W,N,Address,Value->AsNumber()))return F.Fail(TEXT("pcg_general_math_numeric_range"),TEXT("Finite typed numeric math member required within its native storage range"));}
   else if(auto* B=CastField<FBoolProperty>(P)){if(!Value||Value->Type!=EJson::Boolean)return F.Fail(TEXT("pcg_general_math_bool_required"),TEXT("Box IsValid requires a JSON boolean"));B->SetPropertyValue(Address,Value->AsBool());}
   else if(auto* S=CastField<FStructProperty>(P)){auto* Child=GeneralMathResident(S->Struct);if(!Child||!Merge(Child,Address,Value,Depth+1))return false;}else return false;
  }
  return true;
 }
};
bool GeneralScalarProperty(FProperty* P)
{
 if(!P||P->ArrayDim!=1||P->HasSetterOrGetter()||!P->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated|CPF_Transient))return false;
 return CastField<FBoolProperty>(P)||CastField<FNumericProperty>(P)||CastField<FEnumProperty>(P)||CastField<FStrProperty>(P)||CastField<FNameProperty>(P)||CastField<FStructProperty>(P);
}
// Both blocks own initialized native scalar/math storage; no reflected container
// pointer, struct buffer or FString storage is borrowed across SDK callbacks.
struct FFreshScalarCopy
{
 FProperty* Property;void* Value;TSharedPtr<FGeneralMathLease> Math;
 explicit FFreshScalarCopy(FProperty* P):Property(P),Value(P->AllocateAndInitializeValue()){}
 ~FFreshScalarCopy(){Property->DestroyAndFreeValue(Value);}
 FFreshScalarCopy(const FFreshScalarCopy&)=delete;FFreshScalarCopy& operator=(const FFreshScalarCopy&)=delete;
 bool SameValue(const void* Other)const
 {
  if(Math)return Math->SameValue(Value,Other);
  if(auto* B=CastField<FBoolProperty>(Property))return B->GetPropertyValue(Value)==B->GetPropertyValue(Other);
  if(auto* S=CastField<FStrProperty>(Property))return S->GetPropertyValue(Value).Equals(S->GetPropertyValue(Other),ESearchCase::CaseSensitive);
  if(auto* N=CastField<FNameProperty>(Property))return N->GetPropertyValue(Value).IsEqual(N->GetPropertyValue(Other),ENameCase::CaseSensitive);
  FNumericProperty* N=CastField<FNumericProperty>(Property);if(auto* E=CastField<FEnumProperty>(Property))N=E->GetUnderlyingProperty();
  if(!N)return false;if(N->IsFloatingPoint()){const double A=N->GetFloatingPointPropertyValue(Value),B=N->GetFloatingPointPropertyValue(Other);return FMath::IsFinite(A)&&FMath::IsFinite(B)&&A==B;}
  return Unsigned(N)?N->GetUnsignedIntPropertyValue(Value)==N->GetUnsignedIntPropertyValue(Other):N->GetSignedIntPropertyValue(Value)==N->GetSignedIntPropertyValue(Other);
 }
};
bool PrepareGeneralScalar(FFreshRequest& F,UClass* C,const FString& Name,const TSharedPtr<FJsonValue>& Input,TSharedPtr<FFreshScalarCopy>& Wanted,UPCGSettings* CurrentSettings=nullptr)
{
 auto& W=F.W;if(!W.Admit(TEXT("general_scalar_prepare"))||!Input)return false;
 FProperty* P=FreshNativeField(C,FName(*Name));
 if(!GeneralScalarProperty(P)||PropertyAccessUtil::CanSetPropertyValue(P,PropertyAccessUtil::EditorReadOnlyFlags,false)!=EPropertyAccessResultFlags::Success)return F.Fail(TEXT("pcg_general_property_unsupported"),TEXT("Existing editable public native scalar field required; nested/container/object/accessor/readonly fields remain incomplete: ")+Name);
 auto* Owner=Cast<UClass>(P->GetOwnerStruct());if(!Owner||!Owner->HasAllClassFlags(CLASS_Native)||!Owner->GetPathName().StartsWith(TEXT("/Script/PCG.")))return F.Fail(TEXT("pcg_general_field_owner_unsupported"),TEXT("Native /Script/PCG settings inheritance owner required"));
 UEnum* Enum=nullptr;FNumericProperty* Number=CastField<FNumericProperty>(P);FProperty* Underlying=nullptr;
 if(auto* E=CastField<FEnumProperty>(P)){Number=E->GetUnderlyingProperty();Underlying=Number;Enum=E->GetEnum();}else if(auto* B=CastField<FByteProperty>(P))Enum=B->GetIntPropertyEnum();
 TStrongObjectPtr<UEnum> Immediate(Enum);
 // Even a null tracked enum return can notify: close the source before storage use.
 if(!W.Admit(TEXT("general_enum_handle_read")))return false;
 const auto* CurrentSource=F.Sources.FindByPredicate([&](const FFreshClass& S){return S.Class==C;});
 if(!CurrentSource||!CurrentSource->Exact()||FreshNativeField(C,FName(*Name))!=P)return F.Fail(TEXT("pcg_general_enum_descriptor_changed"),TEXT("Held native descriptor changed after enum handle read"));
 if(Enum)
 {
  if(!W.Admit(TEXT("general_enum_source"))||!Live(Enum)||!Enum->GetPathName().StartsWith(TEXT("/Script/PCG."))||!W.Pin(Enum)||!W.Pin(Enum->GetPackage()))return F.Fail(TEXT("pcg_general_enum_source_unsupported"),TEXT("Held installed native PCG enum source required"));
  const int32 Count=Enum->NumEnums();TArray<TPair<FName,int64>> Entries;for(int32 I=0;I<Count;++I)Entries.Emplace(Enum->GetNameByIndex(I),Enum->GetValueByIndex(I));const EObjectFlags Flags=Enum->GetFlags();const FFreshField Root(P);TSharedPtr<FFreshField> Inner;if(Underlying)Inner=MakeShared<FFreshField>(Underlying);
  F.GeneralScalarChecks.Add([&F,C,P,Enum,Underlying,Entries,Flags,Root,Inner]
  {
   // Tracked enum-handle reads may notify a caller. Reclose the held source
   // and physical descriptor inventory before touching storage after that read.
   auto LayoutCurrent=[&]()
   {
    const auto* Source=F.Sources.FindByPredicate([&](const FFreshClass& S){return S.Class==C;});
    if(!Source||!Source->Exact()||FreshNativeField(C,Root.Name)!=P||!Root.Exact())return false;
    if(auto* E=CastField<FEnumProperty>(P))if(E->GetUnderlyingProperty()!=Underlying)return false;
    return !Inner||Inner->Exact();
   };
   if(!Safe()||!LayoutCurrent())return false;
   UEnum* Returned=nullptr;if(auto* E=CastField<FEnumProperty>(P))Returned=E->GetEnum();else if(auto* B=CastField<FByteProperty>(P))Returned=B->GetIntPropertyEnum();
   TStrongObjectPtr<UEnum> ImmediateReturned(Returned);
   if(!Safe()||!LayoutCurrent()||Returned!=Enum||!Live(Enum)||Enum->GetFlags()!=Flags||Enum->NumEnums()!=Entries.Num())return false;
   for(int32 I=0;I<Entries.Num();++I)if(Enum->GetNameByIndex(I)!=Entries[I].Key||Enum->GetValueByIndex(I)!=Entries[I].Value)return false;return true;
  });
 }
 TSharedPtr<FGeneralMathLease> Math;
 if(auto* S=CastField<FStructProperty>(P))
 {
  Math=MakeShared<FGeneralMathLease>(F,C,S);if(!Math->Bind())return F.Fail(TEXT("pcg_general_math_source_pending"),TEXT("Exact resolved native CoreUObject Vector/Vector2D/Rotator/Quat/LinearColor/Transform/Box and complete current field layout required"));
  F.GeneralScalarChecks.Add([Math]{return Math->Exact();});
  for(const auto& Hold:Math->Holds)if(!W.Pin(Hold.Get())||!Math->Exact())return false;
 }
 Wanted=MakeShared<FFreshScalarCopy>(P);Wanted->Math=Math;void* D=Wanted->Value;
 if(Math)
 {
  Math->Seed(D);
  // Never pass a borrowed struct address across admission/SDK calls. Copy the
  // actual current instance only after every source callback has returned.
  if(CurrentSettings){if(!Live(CurrentSettings)||CurrentSettings->GetClass()!=C||!Math->Exact())return F.Fail(TEXT("pcg_general_math_receiver_changed"),TEXT("Current exact settings/source required before owned math copy"));P->CopyCompleteValue(D,P->ContainerPtrToValuePtr<void>(CurrentSettings));}
  if(!Math->SameValue(D,D))return F.Fail(TEXT("pcg_general_math_nonfinite_baseline"),TEXT("Finite actual native math components required before merge"));
  if(!Math->Merge(Math->Struct,D,Input)||!Math->SameValue(D,D))return F.Fail(TEXT("pcg_general_math_value_invalid"),TEXT("Finite supported math fields required; actual source and unprovided components are preserved"));
 }
 else if(auto* B=CastField<FBoolProperty>(P)){if(Input->Type!=EJson::Boolean)return F.Fail(TEXT("pcg_general_bool_required"),Name+TEXT(" requires a JSON boolean"));B->SetPropertyValue(D,Input->AsBool());}
 else if(auto* S=CastField<FStrProperty>(P)){if(Input->Type!=EJson::String)return F.Fail(TEXT("pcg_general_string_required"),Name+TEXT(" requires a JSON string"));S->SetPropertyValue(D,Input->AsString());}
 else if(auto* N=CastField<FNameProperty>(P)){if(Input->Type!=EJson::String||Input->AsString().Len()>=NAME_SIZE)return F.Fail(TEXT("pcg_general_name_required"),Name+TEXT(" requires a bounded JSON string Name; wider legacy coercion remains incomplete"));N->SetPropertyValue(D,FName(*Input->AsString()));}
 else if(Number)
 {
  if(Enum&&Input->Type!=EJson::String)return F.Fail(TEXT("pcg_general_enum_token_required"),Name+TEXT(" requires a nonhidden native enum string token; numeric enum coercion is not admitted"));
  if(Enum&&Input->Type==EJson::String)
  {
   FString Token=Input->AsString();int32 Dot=INDEX_NONE;if(Token.FindLastChar('.',Dot))Token=Token.Mid(Dot+1);if(Token.Contains(TEXT("::")))Token.Split(TEXT("::"),nullptr,&Token);Token.ReplaceInline(TEXT("_"),TEXT(""));int64 Value=0;bool Found=false;
   for(int32 I=0;I<Enum->NumEnums();++I){FString Entry=Enum->GetNameStringByIndex(I);Entry.ReplaceInline(TEXT("_"),TEXT(""));if(Entry.Equals(Token,ESearchCase::IgnoreCase)){bool Hidden=false;if(!W.Read(TEXT("general_enum_hidden_metadata"),[&]{return Enum->HasMetaData(TEXT("Hidden"),I);},Hidden))return false;if(Hidden)continue;if(Found)return F.Fail(TEXT("pcg_general_enum_ambiguous"),TEXT("Enum token is ambiguous"));Value=Enum->GetValueByIndex(I);Found=true;}}
   if(!Found||!PutInteger(W,Number,D,Value))return F.Fail(TEXT("pcg_general_enum_invalid"),TEXT("Native nonhidden PCG enum token required"));
  }
  else
  {
   if(Input->Type!=EJson::Number)return F.Fail(TEXT("pcg_general_numeric_required"),Name+TEXT(" requires a finite JSON number or supported enum token"));const double Value=Input->AsNumber();
   if(Number->IsInteger()&&FMath::Abs(Value)>9007199254740991.0)return F.Fail(TEXT("pcg_general_integer_precision"),TEXT("Integer beyond lossless JSON numeric range remains incomplete"));
   if(!PutNumeric(W,Number,D,Value))return false;
  }
 }
 else return F.Fail(TEXT("pcg_general_scalar_type_unsupported"),TEXT("Unsupported scalar layout"));
 return W.Admit(TEXT("general_scalar_plan_ready"));
}
bool PrepareGeneralSettings(FFreshRequest& F,FFreshNodeSpec& Spec)
{
 Spec.Plans=MakeShared<FJsonObject>();
 for(const auto& Pair:Spec.Settings->Values){TSharedPtr<FFreshScalarCopy> Wanted;if(!PrepareGeneralScalar(F,Spec.Class,FString(Pair.Key),Pair.Value,Wanted))return false;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("native_property"),Wanted->Property->GetName());J->SetStringField(TEXT("scope"),Wanted->Math?TEXT("public native closed math editor setter; owned actual-instance partial merge, exact finite semantic values, no ImportText or Python wrapper"):TEXT("public native scalar editor setter; no ImportText or Python wrapper"));Spec.Plans->SetObjectField(FString(Pair.Key),J);}
 return F.W.Admit(TEXT("general_native_settings_prepared"));
}
bool ApplyGeneralSettings(FFreshRequest& F,const FFreshNodeSpec& Spec,UPCGNode* N,UPCGSettings* S,const TSharedPtr<FJsonObject>& Row)
{
 auto& W=F.W;TArray<TSharedPtr<FJsonValue>> Rows;Row->SetArrayField(TEXT("settings_readback"),Rows);bool HasMath=false;
 // Capture requested fields before the first writer. Later node creation,
 // notifications, edge construction and saving retain the semantic expectations.
 TMap<FName,TSharedPtr<FFreshScalarCopy>> Baselines;
 for(const auto& Pair:Spec.Settings->Values)
 {
  TSharedPtr<FFreshScalarCopy> Wanted;if(!PrepareGeneralScalar(F,Spec.Class,FString(Pair.Key),Pair.Value,Wanted,S))return false;FProperty* P=Wanted->Property;const FFreshField Layout(P);const FName Name=P->GetFName();
  auto Baseline=MakeShared<FFreshScalarCopy>(P);Baseline->Math=Wanted->Math;HasMath|=Wanted->Math.IsValid();P->CopyCompleteValue(Baseline->Value,P->ContainerPtrToValuePtr<void>(S));Baselines.Add(Name,Baseline);
  UPCGGraph* OwningGraph=Cast<UPCGGraph>(N->GetOuter());if(!OwningGraph||!W.FreshGraphs.Contains(OwningGraph))return F.Fail(TEXT("pcg_general_graph_owner_invalid"),TEXT("Scalar owner must be a held request-created graph"));
  F.GeneralScalarChecks.Add([&F,N,S,P,Layout,Baseline,OwningGraph]{if(!Live(N)||!Live(S)||!Live(OwningGraph)||!F.W.FreshGraphs.Contains(OwningGraph)||N->GetOuter()!=OwningGraph||S->GetOuter()!=N||!FreshObjectEquals(N,TEXT("SettingsInterface"),S)||FreshNativeField(S->GetClass(),Layout.Name)!=P||!Layout.Exact())return false;return (F.GeneralMutableSettings==S&&F.GeneralMutableField==P)||Baseline->SameValue(P->ContainerPtrToValuePtr<void>(S));});
 }
 for(const auto& Pair:Spec.Settings->Values)
 {
  TSharedPtr<FFreshScalarCopy> Wanted;if(!PrepareGeneralScalar(F,Spec.Class,FString(Pair.Key),Pair.Value,Wanted,S))return false;FProperty* P=Wanted->Property;auto Baseline=Baselines[P->GetFName()];auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_property"),FString(Pair.Key));J->SetStringField(TEXT("native_property"),P->GetName());if(Wanted->Math){J->SetStringField(TEXT("math_struct"),Wanted->Math->Struct->GetPathName());J->SetBoolField(TEXT("partial_merge_from_actual_instance"),Pair.Value->Type==EJson::Object);J->SetBoolField(TEXT("borrowed_struct_buffer_across_callbacks"),false);}Rows.Add(MakeShared<FJsonValueObject>(J));Row->SetArrayField(TEXT("settings_readback"),Rows);
  const auto Before=W.Snapshot;if(!W.SDK(TEXT("general_native_editor_set"),true))return false;W.RetireMutablePins(N);
  EPropertyAccessResultFlags Access;{
   TGuardValue<bool> Capture(W.Capturing,true);TGuardValue<UPCGNode*> ActiveNode(F.ActiveSettingsNode,N);TGuardValue<UPCGSettings*> Active(F.ActiveSettings,S);TGuardValue<UPCGSettings*> Mutable(F.GeneralMutableSettings,S);TGuardValue<FProperty*> Field(F.GeneralMutableField,P);
   Access=PropertyAccessUtil::SetPropertyValue_Object(P,S,P,Wanted->Value,INDEX_NONE,PropertyAccessUtil::EditorReadOnlyFlags,EPropertyAccessChangeNotifyMode::Default);
   if(!W.After(TEXT("general_native_editor_set")))return false;
   const bool Equal=Wanted->SameValue(P->ContainerPtrToValuePtr<void>(S));J->SetBoolField(TEXT("setter_returned"),true);J->SetBoolField(TEXT("setter_success"),Access==EPropertyAccessResultFlags::Success);J->SetBoolField(TEXT("typed_expected_verified"),Equal);
   if(Access!=EPropertyAccessResultFlags::Success||!Equal)return F.Fail(TEXT("pcg_general_scalar_readback_failed"),Wanted->Math?TEXT("Actual public editor setter result differs from the prewriter typed math plan; actual prefix retained without retry"):TEXT("Actual public editor setter result differs from the prewriter scalar plan; actual prefix retained without retry"));
   W.R.bChanged|=!Baseline->SameValue(Wanted->Value);P->CopyCompleteValue(Baseline->Value,Wanted->Value);
   if(!Transition(W,Before,TEXT("notify"),N))return false;
  }
  if(!W.Admit(TEXT("general_scalar_accepted")))return false;
 }
  if(HasMath)Row->SetBoolField(TEXT("general_native_math_settings_verified"),true);Row->SetBoolField(TEXT("general_native_scalar_settings_verified"),true);return true;
}
bool FreshSources(FFreshRequest& F)
{
 auto& W=F.W;auto* AR=FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));auto* AT=FModuleManager::GetModulePtr<FAssetToolsModule>(TEXT("AssetTools"));F.SourceControl=FModuleManager::GetModulePtr<ISourceControlModule>(TEXT("SourceControl"));
 if(!AR||!AT||!F.SourceControl||F.SourceControl->IsEnabled())return F.Fail(TEXT("pcg_source_control_policy_unsupported"),TEXT("Already-loaded AssetRegistry/AssetTools and disabled SourceControl are required; no module/preferences change"));F.Registry=&AR->Get();F.AssetTools=&AT->Get();
 for(auto& N:F.Nodes)
 {
  FString Short=N.ClassName;if(Short.StartsWith(TEXT("/Script/PCG.")))Short.RightChopInline(12);
  const FString Path=TEXT("/Script/PCG.")+Short;N.Class=Cast<UClass>(StaticFindObject(nullptr,nullptr,*Path,false));TStrongObjectPtr<UClass> ImmediateClass(N.Class);if(!W.Admit(TEXT("find_finite_settings_class")))return false;
  if(!F.GeneralNative&&StandaloneSpatialCompositionClass(N.Class))return F.Fail(TEXT("pcg_spatial_graph_spec_pending"),TEXT("Spatial composition requires the request-owned general Native worker; the old finite profile is unchanged"));
  if(!N.Class||!Same(N.Class->GetPathName(),Path)||!(Same(N.ClassName,Short)||Same(N.ClassName,Path))||!N.Class->IsChildOf(UPCGSettings::StaticClass())||!F.ClassSource(N.Class))return F.Fail(TEXT("pcg_setting_contract_unsupported"),TEXT("Already-loaded exact finite native family, PointExtents/Bounds, TransformPoints, or SurfaceSampler/Difference settings class required"));
  if(F.GeneralNative&&!FreshKnownFiniteClass(N.Class)){N.GeneralNative=true;if(!PrepareGeneralSettings(F,N))return false;continue;}
   if(FreshProjectionClass(N.Class)&&!FreshProjectionSource(F))return false;
   if(N.Class==UPCGStaticMeshSpawnerSettings::StaticClass())
   {
    N.Spawner=true;F.InlineSpawnerProfile=true;
    FStandaloneRequest Planner(W);if(!Planner.PrepareSpawner(N.Settings,N.Plans,N.Aliases)||!FreshInlineSpawnerPlan(F,N))return false;
    continue;
   }
  auto FiniteSettings=N.Settings;TSharedPtr<FJsonObject> AggregateSettings,AggregatePlans,AggregateAliases;
  if(FreshDataSourceClass(N.Class))
  {
   if(!F.DataSourceWork)return F.Fail(TEXT("pcg_fresh_data_source_work_missing"),TEXT("Request-owned original aggregate source/value work required before construction"));
   if(!F.DataSourceProfile){F.DataSourceProfile=true;auto* Work=F.DataSourceWork;F.ExternalAssetChecks.Add([Work]{return Work->Current();});}
   FiniteSettings=MakeShared<FJsonObject>(*N.Settings);AggregateSettings=MakeShared<FJsonObject>();
   for(const auto& Pair:N.Settings->Values){const FString Name(Pair.Key.ToView());if(Name.Len()>=NAME_SIZE||!NoEmbeddedNul(Name))continue;const auto* Field=GraphSpecDataSourceAggregateField(N.Class,Name);if(Field&&DataSourceAggregateName(FName(Field->Native))){AggregateSettings->SetField(Name,Pair.Value);FiniteSettings->RemoveField(Name);}}
   if(!F.DataSourceWork->Held.ClassSource(N.Class)||!F.DataSourceWork->CaptureDataSourceSources())return false;AggregatePlans=MakeShared<FJsonObject>();
   for(const auto& Pair:AggregateSettings->Values){const auto* Field=GraphSpecDataSourceAggregateField(N.Class,FString(Pair.Key));FProperty* P=Field?FreshNativeField(N.Class,Field->Native):nullptr;FDataSourceAggregateValue ParserScratch;TSharedPtr<FJsonObject> Plan;if(!P||!F.DataSourceWork->PrepareGraphSpecDataSourceAggregate(N.Class,P,Pair.Value,ParserScratch,Plan))return false;AggregatePlans->SetObjectField(Field->Alias,Plan);}
  }
  if(!CubelessPCGUE58::PrepareFreshSettings(N.Class,FiniteSettings,N.Plans,[&]{return W.Admit(TEXT("finite_setting_preflight"));}))
  {
   if(!W.R.Data->HasField(TEXT("fresh_setting_first_failure")))
   {
    auto Failure=MakeShared<FJsonObject>();FString InputField;const bool FieldObserved=N.Plans&&N.Plans->TryGetStringField(TEXT("preflight_input_field"),InputField);
    Failure->SetStringField(TEXT("node_id"),N.Id);Failure->SetStringField(TEXT("node_title"),N.Title);Failure->SetStringField(TEXT("requested_class"),N.ClassName);Failure->SetStringField(TEXT("admitted_class_path"),Path);
    Failure->SetStringField(TEXT("input_field"),InputField);Failure->SetBoolField(TEXT("input_field_copied"),FieldObserved);Failure->SetStringField(TEXT("condition"),TEXT("finite_setting_preflight_returned"));Failure->SetBoolField(TEXT("expected"),true);Failure->SetBoolField(TEXT("actual"),false);Failure->SetBoolField(TEXT("expected_actual_observed"),true);
    Failure->SetStringField(TEXT("diagnostic_scope"),TEXT("Copied request identity/current input key and actual preflight False; no independent field type/value rejection or unsafe source requery is inferred."));Failure->SetBoolField(TEXT("source_requeried_after_failure"),false);W.R.Data->SetObjectField(TEXT("fresh_setting_first_failure"),Failure);
   }
   return F.Fail(TEXT("pcg_setting_type_invalid"),TEXT("Only existing finite native fields, TransformPoints13, or SurfaceSampler8/Difference4 are admitted before construction"));
  }
  if(AggregateSettings)for(const auto& Pair:AggregateSettings->Values){const auto* Field=GraphSpecDataSourceAggregateField(N.Class,FString(Pair.Key));const TSharedPtr<FJsonObject>* Plan=nullptr;if(!Field||!AggregatePlans->TryGetObjectField(Field->Alias,Plan))return F.Fail(TEXT("pcg_fresh_data_source_plan_missing"),TEXT("Original aggregate preflight plan must retain its requested/native field mapping"));N.Plans->SetObjectField(FString(Pair.Key),Plan->ToSharedRef());}
  if(StandalonePointBoundsClass(N.Class))F.PointBoundsProfile=true;
  if(FreshTransformPointsClass(N.Class))F.TransformPointsProfile=true;
  if(StandaloneSamplingDifferenceClass(N.Class))F.SamplingDifferenceProfile=true;
 }
 for(UClass* C:{UPCGGraph::StaticClass(),UPCGNode::StaticClass(),UPCGPin::StaticClass(),UPCGEdge::StaticClass()})if(!F.ClassSource(C))return false;
 F.FactoryClass=Cast<UClass>(StaticFindObject(nullptr,nullptr,TEXT("/Script/PCGEditor.PCGGraphFactory"),false));if(!W.Admit(TEXT("find_loaded_factory_class")))return false;
 if(!F.FactoryClass||!Same(F.FactoryClass->GetPathName(),TEXT("/Script/PCGEditor.PCGGraphFactory"))||!F.FactoryClass->IsChildOf(UFactory::StaticClass())||!F.ClassSource(F.FactoryClass))return F.Fail(TEXT("pcg_fresh_factory_unavailable"),TEXT("Already-loaded exact native PCGGraphFactory subclass required; private header/UI/template path unsupported"));
 F.TemplateProperty=CastField<FObjectProperty>(FreshNativeField(F.FactoryClass,TEXT("TemplateGraph")));F.CreateProperty=CastField<FBoolProperty>(FreshNativeField(F.FactoryClass,TEXT("bCreateNew")));
 if(!F.TemplateProperty||F.TemplateProperty->PropertyClass!=UPCGGraph::StaticClass()||F.TemplateProperty->ArrayDim!=1||F.TemplateProperty->HasSetterOrGetter()||!F.CreateProperty||F.CreateProperty->HasSetterOrGetter())return F.Fail(TEXT("pcg_fresh_factory_schema"),TEXT("Exact public reflected TemplateGraph and native create flag layout required"));
 const auto* FactorySource=F.Sources.FindByPredicate([&](const FFreshClass& S){return S.Class==F.FactoryClass;});if(!FactorySource||!FactorySource->CDO)return F.Fail(TEXT("pcg_fresh_factory_cdo_unavailable"),TEXT("Factory must have an already-existing CDO; no lazy CDO construction"));
 auto* CDO=Cast<UFactory>(FactorySource->CDO);auto Supported=CDO->SupportedClass;auto* Template=F.TemplateProperty->GetObjectPtrPropertyValuePtr(F.TemplateProperty->ContainerPtrToValuePtr<void>(CDO));
 if(!Supported.GetGCPtr().IsResolved()||Supported.GetGCPtr()!=UPCGGraph::StaticClass()||!Template->IsResolved()||*Template!=nullptr||!F.CreateProperty->GetPropertyValue_InContainer(CDO))return F.Fail(TEXT("pcg_fresh_factory_template_unsupported"),TEXT("Factory CDO must be graph-supported/create-new/null-template; no ConfigureProperties"));
 W.R.Data->SetBoolField(TEXT("factory_private_header_used"),false);W.R.Data->SetBoolField(TEXT("factory_configure_properties_called"),false);W.R.Data->SetBoolField(TEXT("source_cdo_values_sampled"),false);W.R.Data->SetBoolField(TEXT("factory_cdo_policy_fields_observed"),true);
 F.DirtyBefore=F.Dirty();W.R.Data->SetObjectField(TEXT("creation_policy"),MakeShared<FJsonObject>());W.R.Data->GetObjectField(TEXT("creation_policy"))->SetBoolField(TEXT("source_control_enabled"),false);
 return W.Admit(TEXT("fresh_sources_admitted"));
}
bool FreshBind(FFreshRequest& F,UPCGGraph* G)
{
 auto& W=F.W;W.Snapshot={};W.Graph=G;W.CreatedNode=nullptr;W.FreshGraphs.Add(G);
 if(!Live(G)||G->GetClass()!=UPCGGraph::StaticClass()||!W.Pin(G))return F.Fail(TEXT("pcg_fresh_graph_invalid"),TEXT("Returned ordinary exact native graph required"));
 W.DirtyBefore=G->GetPackage()->IsDirty();FGraphSnapshot S;if(!CaptureGraph(W,G,S))return false;
 if(!S.Input||!S.Output||!S.Nodes.IsEmpty()||!S.Edges.IsEmpty())return F.Fail(TEXT("pcg_fresh_factory_not_empty"),TEXT("Fresh factory must return only its original Input/Output nodes with no authored edges"));
 W.Snapshot=MoveTemp(S);return W.Admit(TEXT("fresh_graph_bound"));
}
// PG: retain only the existing PA two/five authored values. Class/layout/CDO
// sources were held before construction; no inherited/default buffer is sampled.
bool FreshPointBoundsValues(FFreshRequest& F,UPCGSettings* Settings,TArray<TSharedPtr<FStandaloneValue>>& Values)
{
 auto& W=F.W;if(!StandalonePointBoundsClass(Settings->GetClass())&&!FreshTransformPointsClass(Settings->GetClass())&&!StandaloneSamplingDifferenceClass(Settings->GetClass())&&!FreshDataSourceClass(Settings->GetClass())&&!FreshProjectionClass(Settings->GetClass()))return true;
 if(!W.Admit(TEXT("fresh_point_bounds_before_values")))return false;
 UClass* Class=Settings->GetClass();
 for(const auto& Field:StandaloneFields)
 {
  if(Class->GetFName()!=Field.Class)continue;
  // Aggregate typed values are held separately by the shared request-owned data-source work.
  if(FreshDataSourceClass(Class)&&(DataSourceAggregateName(FName(Field.Native))||FName(Field.Native)==TEXT("SamplingProperties")))continue;
  auto V=MakeShared<FStandaloneValue>();V->Settings=Settings;V->Property=FreshNativeField(Class,Field.Native);V->Path=Settings->GetPathName()+TEXT(":")+Field.Native;
  FProperty* P=V->Property;if(!P||(FreshDataSourceClass(Class)?!FreshDataSourceProperty(Class,P):P->GetOwnerStruct()!=Class)||P->ArrayDim!=1||P->HasSetterOrGetter()||!P->HasAnyPropertyFlags(CPF_Edit|CPF_BlueprintVisible)||(P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated)||(P->HasAnyPropertyFlags(CPF_Transient)&&!StandaloneSamplingTransient(Class,P))))return F.Fail(TEXT("pcg_fresh_point_bounds_field_unsupported"),TEXT("Exact existing bounds/TransformPoints authored field owner/type/layout required"));
  TSharedPtr<FStandalonePointLayout> VectorLayout;
  if(auto* Struct=CastField<FStructProperty>(P))
  {
   if(!Struct->Struct.IsResolved()||(Struct->Struct!=TBaseStructure<FVector>::Get()&&(!FreshTransformPointsClass(Class)||Struct->Struct!=TBaseStructure<FRotator>::Get())&&(!FreshProjectionClass(Class)||Struct->Struct!=FPCGProjectionParams::StaticStruct())))return F.Fail(TEXT("pcg_fresh_point_bounds_vector_source"),TEXT("Exact resolved native Vector/TransformPoints Rotator binding required"));
   V->Struct=Struct->Struct.Get();if(!W.Pin(V->Struct)||!W.Pin(V->Struct->GetPackage())||!W.Admit(TEXT("fresh_point_bounds_vector_held")))return false;
   auto* T=V->Struct;VectorLayout=MakeShared<FStandalonePointLayout>();auto& L=*VectorLayout;L.Struct=T;L.Path=T->GetPathName();L.Package=T->GetPackage();L.PackageFlags=L.Package->GetPackageFlags();L.ObjectFlags=T->GetFlags();L.Flags=T->StructFlags;L.Size=T->GetStructureSize();L.Alignment=T->GetMinAlignment();
   for(TFieldIterator<FProperty> It(T);It;++It){if(It->HasSetterOrGetter())return F.Fail(TEXT("pcg_fresh_point_bounds_vector_layout"),TEXT("Plain native Vector fields required"));L.Inventory.Add(*It);L.Fields.Emplace(*It);}
   if(FreshProjectionClass(Class))V->Projection=MakeShared<FPCGProjectionParams>();
   if(!L.Exact()||L.Size!=(FreshProjectionClass(Class)?sizeof(FPCGProjectionParams):(T==TBaseStructure<FRotator>::Get()?sizeof(FRotator):sizeof(FVector))))return F.Fail(TEXT("pcg_fresh_point_bounds_vector_layout"),TEXT("Held native Vector/Rotator source layout is not current"));
  }
  if(auto* EnumField=CastField<FEnumProperty>(P))
  {
   UEnum* Returned=EnumField->GetEnum();TStrongObjectPtr<UEnum> Hold(Safe()&&Live(Returned)?Returned:nullptr);
   if(!W.Admit(TEXT("fresh_point_bounds_enum_return"))||!Hold.IsValid()||!W.Pin(Hold.Get())||!W.Pin(Hold->GetPackage()))return false;
   if(FreshNativeField(Class,Field.Native)!=P||EnumField->GetEnum()!=Hold.Get())return F.Fail(TEXT("pcg_fresh_point_bounds_enum_binding"),TEXT("Returned held Enum must still be the exact native Mode source binding"));
   V->Enum=Hold.Get();V->EnumStorage=EnumField->GetUnderlyingProperty();V->EnumFlags=V->Enum->GetFlags();for(int32 I=0;I<V->Enum->NumEnums();++I)V->EnumNames.Emplace(V->Enum->GetNameByIndex(I),V->Enum->GetValueByIndex(I));
  }
  if(!V->SameType()||!V->Capture())return F.Fail(TEXT("pcg_fresh_point_bounds_value_unsupported"),TEXT("Exact finite Vector/Rotator/Name/enum/bool/float native storage required"));
  const FFreshField Layout(P);const FName Name=P->GetFName();
  F.PointBoundsValueChecks.Add([&F,V,Layout,Class,Name,VectorLayout]()
  {
   // Current() checks original class field membership before this retained role.
   if(!Live(V->Settings)||V->Settings->GetClass()!=Class||FreshNativeField(Class,Name)!=V->Property||!Layout.Exact())return false;
   if(V->Enum)
   {
    const auto* Mode=static_cast<const FEnumProperty*>(V->Property);UEnum* Returned=Mode->GetEnum();TStrongObjectPtr<UEnum> Hold(Safe()&&Live(Returned)?Returned:nullptr);
    const auto* Source=F.Sources.FindByPredicate([&](const FFreshClass& S){return S.Class==Class;});
    if(!Source||!Source->Exact()||FreshNativeField(Class,Name)!=V->Property||!Layout.Exact()||Hold.Get()!=V->Enum||Mode->GetUnderlyingProperty()!=V->EnumStorage)return false;
   }
   if(!V->SameType()||!V->EnumCurrent()||(VectorLayout&&!VectorLayout->Exact()))return false;
   return (F.PointBoundsMutableSettings==V->Settings&&F.PointBoundsMutableField==V->Property)||V->SameValue();
  });Values.Add(V);
 }
 return W.Admit(TEXT("fresh_point_bounds_values_bound"));
}
bool FreshBuild(FFreshRequest& F,UPCGGraph* G,const TSharedPtr<FJsonObject>& Built)
{
 auto& W=F.W;if(!FreshBind(F,G))return false;TArray<TSharedPtr<FJsonValue>> Created,Connected;Built->SetArrayField(TEXT("created_nodes"),Created);Built->SetArrayField(TEXT("connected_edges"),Connected);
 TMap<FString,UPCGNode*> Lookup;Lookup.Add(TEXT("input"),W.Snapshot.Input);Lookup.Add(TEXT("__input__"),W.Snapshot.Input);Lookup.Add(TEXT("output"),W.Snapshot.Output);Lookup.Add(TEXT("__output__"),W.Snapshot.Output);
 TArray<UPCGNode*> Members;
 for(const auto& Spec:F.Nodes)
 {
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),Spec.Id);Row->SetStringField(TEXT("title"),Spec.Title);Row->SetStringField(TEXT("settings_class"),Spec.Class->GetPathName());Created.Add(MakeShared<FJsonValueObject>(Row));Built->SetArrayField(TEXT("created_nodes"),Created);
  if(!W.RequireConstruction(TEXT("fresh_Graph_AddNodeOfType"))||!W.SDK(TEXT("fresh_Graph_AddNodeOfType"),true))return false;const auto Before=W.Snapshot;
  UPCGSettings* Settings=nullptr;UPCGNode* N=nullptr;TSharedPtr<FFreshSpawnerState> SpawnerState;
  {
   TGuardValue<bool> Capturing(W.Capturing,true);N=G->AddNodeOfType(Spec.Class,Settings);
   Row->SetBoolField(TEXT("node_returned_nonnull"),N!=nullptr);Row->SetBoolField(TEXT("settings_returned_nonnull"),Settings!=nullptr);
   const bool NodeObserved=F.KeepReturned(N,TEXT("fresh_node")),SettingsObserved=F.KeepReturned(Settings,TEXT("fresh_settings"));
   if(NodeObserved){Row->SetStringField(TEXT("node_path"),N->GetPathName());Row->SetStringField(TEXT("node_name"),N->GetName());}if(SettingsObserved)Row->SetStringField(TEXT("settings_path"),Settings->GetPathName());
    if(Spec.Spawner&&SettingsObserved&&Settings->GetClass()==UPCGStaticMeshSpawnerSettings::StaticClass())
    {
     auto* Spawner=static_cast<UPCGStaticMeshSpawnerSettings*>(Settings);if(!Spawner->MeshSelectorParameters.IsResolved()||!F.KeepReturned(Spawner->MeshSelectorParameters.Get(),TEXT("inline_returned_constructor_selector")))return F.Fail(TEXT("pcg_inline_spawner_constructor_unheld"),TEXT("Constructor-returned selector must be held before next current/query"));
    }
   if(!W.After(TEXT("fresh_Graph_AddNodeOfType")))return false;
   if(!NodeObserved||!SettingsObserved||N->GetOuter()!=G||N->GetGraph()!=G||Settings->GetOuter()!=N||Settings->GetClass()!=Spec.Class||N->GetSettingsInterface()!=Settings||N->GetPackage()!=G->GetPackage()||Settings->GetPackage()!=G->GetPackage())return F.Fail(TEXT("pcg_fresh_returned_owner_changed"),TEXT("Actual node/settings identities do not have the exact newly-owned role"));
    if(Spec.Spawner&&!FreshEnrollSpawner(F,Settings,SpawnerState))return false;
   W.CreatedNode=N;W.R.bChanged=true;if(!Transition(W,Before,TEXT("add"),nullptr,N))return false;
  }
  if(FreshDataSourceClass(Spec.Class)&&(!F.DataSourceWork||!F.DataSourceWork->CaptureValues(Settings,true)))return false;
  TArray<TSharedPtr<FStandaloneValue>> PointBoundsValues;if(!FreshPointBoundsValues(F,Settings,PointBoundsValues))return false;
  if(StandalonePointBoundsClass(Spec.Class)){Row->SetObjectField(TEXT("point_bounds_preflight_plan"),Spec.Plans);Row->SetNumberField(TEXT("point_bounds_captured_field_count"),PointBoundsValues.Num());}
  if(FreshTransformPointsClass(Spec.Class)){Row->SetObjectField(TEXT("transform_points_preflight_plan"),Spec.Plans);Row->SetNumberField(TEXT("transform_points_captured_field_count"),PointBoundsValues.Num());}
  if(StandaloneSamplingDifferenceClass(Spec.Class)){Row->SetObjectField(TEXT("sampling_difference_preflight_plan"),Spec.Plans);Row->SetNumberField(TEXT("sampling_difference_captured_field_count"),PointBoundsValues.Num());Row->SetBoolField(TEXT("surface_keep_zero_is_transient_editor_state"),Spec.Class==UPCGSurfaceSamplerSettings::StaticClass());Row->SetBoolField(TEXT("transient_editor_state_persistence_verified"),false);}
  if(FreshDataSourceClass(Spec.Class)){Row->SetObjectField(TEXT("data_source_preflight_plan"),Spec.Plans);Row->SetNumberField(TEXT("data_source_captured_field_count"),PointBoundsValues.Num());Row->SetStringField(TEXT("data_source_scope"),TEXT("Actual native constructor defaults, finite scalar fields and original dict/list ActorSelector/ComponentSelector/ExpectedPins; partial selector dicts merge into each actual returned instance using public editor property setters and nested change notifications. Existing class filter sources and reflected authored values remain held through scratch, persistent build and save. Transient-only show members are permission-denied by the original editor setter policy; the raw Add/Set ImportText skip rule is not used. Regenerated caches are not frozen. Landscape SamplingProperties raw native-text authoring is implemented / usage validation pending; unloaded class aliases and broader generated sources remain incomplete. Dynamic pins are reacquired after Structural notify before requested edges. No actor search/generation/runtime proof."));}
  if(!MutateGraph(W,TEXT("fresh_Node_SetPosition"),TEXT("metadata"),[&]{N->SetNodePosition(Spec.X,Spec.Y);},N))return false;
  int32 X=0,Y=0;N->GetNodePosition(X,Y);if(X!=Spec.X||Y!=Spec.Y)return F.Fail(TEXT("pcg_position_readback_failed"),TEXT("Exact int32 position readback differs"));
  const FName Title(*Spec.Title);if(!MutateGraph(W,TEXT("fresh_Node_SetTitle"),TEXT("metadata"),[&]{N->NodeTitle=Title;},N)||N->NodeTitle!=Title)return F.Fail(TEXT("pcg_title_readback_failed"),TEXT("FName title readback differs"));
  TArray<TSharedPtr<FJsonValue>> SettingsRows;
   if(Spec.GeneralNative)
   {
    if(!ApplyGeneralSettings(F,Spec,N,Settings,Row))return false;
   }
   else if(Spec.Spawner)
   {
    if(!FreshSpawnerGeneral(F,Spec,N,SpawnerState,Row)||!FreshApplyInlineSpawner(F,Spec,SpawnerState,Row))return false;
   }
   else for(const auto& Pair:Spec.Settings->Values)
   {
   TSharedPtr<FJsonObject> SettingRow=MakeShared<FJsonObject>();SettingsRows.Add(MakeShared<FJsonValueObject>(SettingRow));Row->SetArrayField(TEXT("settings_readback"),SettingsRows);
   const FString SettingName(Pair.Key.ToView());const auto* AggregateField=GraphSpecDataSourceAggregateField(Spec.Class,SettingName);
   if(AggregateField&&DataSourceAggregateName(FName(AggregateField->Native)))
   {
    TGuardValue<bool> CaptureSetting(W.Capturing,true);TGuardValue<UPCGNode*> SettingsNode(F.ActiveSettingsNode,N);TGuardValue<UPCGSettings*> SettingObject(F.ActiveSettings,Settings);
    if(!F.DataSourceWork||!SetGraphSpecDataSourceAggregate(W,*F.DataSourceWork,N,Settings,AggregateField->Native,Pair.Value,SettingRow))return false;
    if(!W.Admit(TEXT("fresh_data_source_aggregate_readback")))return false;SettingRow->SetStringField(TEXT("requested_property"),SettingName);SettingRow->SetStringField(TEXT("native_property"),AggregateField->Native);continue;
   }
   if(!W.SDK(TEXT("fresh_finite_setting"),true))return false;const auto SettingBefore=W.Snapshot;W.RetireMutablePins(N);TGuardValue<bool> CaptureSetting(W.Capturing,true);
   TGuardValue<UPCGNode*> SettingsNode(F.ActiveSettingsNode,N);TGuardValue<UPCGSettings*> SettingObject(F.ActiveSettings,Settings);
   FProperty* PointBoundsField=nullptr;if(StandalonePointBoundsClass(Spec.Class)||FreshTransformPointsClass(Spec.Class)||StandaloneSamplingDifferenceClass(Spec.Class)||FreshDataSourceClass(Spec.Class)||FreshProjectionClass(Spec.Class)){const auto Plan=Spec.Plans->GetObjectField(FString(Pair.Key));PointBoundsField=FreshNativeField(Spec.Class,FName(*Plan->GetStringField(TEXT("native_property"))));if(!PointBoundsField)return F.Fail(TEXT("pcg_fresh_point_bounds_plan_missing"),TEXT("Prevalidated exact field plan required before writer"));}
   TGuardValue<UPCGSettings*> MutablePointSettings(F.PointBoundsMutableSettings,PointBoundsField?Settings:nullptr);TGuardValue<FProperty*> MutablePointField(F.PointBoundsMutableField,PointBoundsField);
   const bool Applied=CubelessPCGUE58::ApplyFreshSetting(G,N,FString(Pair.Key),Pair.Value,SettingRow,[&]{return W.Admit(TEXT("fresh_setting_callback_current"));});
    // Preserve the inner receipt before After/current can stop unsafe observation.
    // This is a copied return DTO, not a successful reclassification or live query.
    auto Last=MakeShared<FJsonObject>();Last->SetStringField(TEXT("node_id"),Spec.Id);Last->SetStringField(TEXT("requested_property"),FString(Pair.Key));Last->SetBoolField(TEXT("apply_returned"),true);Last->SetBoolField(TEXT("apply_success"),Applied);if(SettingRow)Last->SetObjectField(TEXT("native_receipt"),MakeShared<FJsonObject>(*SettingRow));W.R.Data->SetObjectField(TEXT("fresh_setting_last_receipt"),Last);
    SettingsRows.Last()=MakeShared<FJsonValueObject>(SettingRow);Row->SetArrayField(TEXT("settings_readback"),SettingsRows);
   if(!W.After(TEXT("fresh_finite_setting")))return false;if(!Applied)return F.Fail(TEXT("pcg_fresh_setting_failed"),TEXT("Finite setting failed; actual native prefix retained without cleanup/retry"));
   if(!Transition(W,SettingBefore,TEXT("notify"),N))return false;
   if(PointBoundsField)
   {
    auto One=MakeShared<FJsonObject>();One->SetField(FString(Pair.Key),Pair.Value);TSharedPtr<FJsonObject> Verified;
    if(!CubelessPCGUE58::VerifyFreshSettings(G,N,One,Verified,[&]{return W.Admit(TEXT("fresh_point_bounds_planned_readback"));}))return F.Fail(TEXT("pcg_fresh_point_bounds_value_mismatch"),TEXT("Selected actual field differs from the prewriter finite plan; no expectation is accepted"));
    // The partial request's complete merged expectation was fixed before the writer.
    // Do not adopt a later observer's changes to unprovided components as a new baseline.
    if(FreshTransformPointsClass(Spec.Class)||StandaloneSamplingDifferenceClass(Spec.Class)||FreshDataSourceClass(Spec.Class)||FreshProjectionClass(Spec.Class))
    {
     const auto Actual=Verified->GetObjectField(FString(Pair.Key))->TryGetField(TEXT("actual"));const auto Expected=SettingRow->TryGetField(TEXT("expected"));
     bool AuthoredStringExact=true;
     if(FreshProjectionClass(Spec.Class)&&PointBoundsField->GetFName()==TEXT("ProjectionParams"))
     {
      FString ActualList,ExpectedList;
      AuthoredStringExact=Actual&&Expected&&Actual->Type==EJson::Object&&Expected->Type==EJson::Object&&Actual->AsObject()->TryGetStringField(TEXT("AttributeList"),ActualList)&&Expected->AsObject()->TryGetStringField(TEXT("AttributeList"),ExpectedList)&&ActualList.Equals(ExpectedList,ESearchCase::CaseSensitive);
     }
     if(!Actual||!Expected||!AuthoredStringExact||!FJsonValue::CompareEqual(*Actual,*Expected))return F.Fail(TEXT("pcg_fresh_transform_components_changed"),TEXT("Actual whole selected field differs from the prewriter typed expectation, including case-sensitive Projection AttributeList; no baseline update or retry"));
    }
    const auto* Selected=PointBoundsValues.FindByPredicate([&](const TSharedPtr<FStandaloneValue>& V){return V->Property==PointBoundsField;});
    if(!Selected||!(*Selected)->Capture())return F.Fail(TEXT("pcg_fresh_point_bounds_selected_missing"),TEXT("Only the verified selected field may update its expectation"));
    SettingRow->SetObjectField(TEXT("point_bounds_planned_readback"),Verified);SettingRow->SetBoolField(TEXT("point_bounds_selected_only_accepted"),true);
   }
  }
  Members.Add(N);Row->SetBoolField(TEXT("node_settings_complete"),true);
  for(const FString Alias:{Spec.Id,Spec.Title,N->GetName(),N->GetPathName()}){if(Lookup.Contains(Alias)&&Lookup[Alias]!=N)return F.Fail(TEXT("pcg_node_alias_collision"),TEXT("Engine actual node alias collides with an earlier alias"));Lookup.Add(Alias,N);}
 }
 for(const auto& Edge:F.Edges)
 {
  auto Spec=Edge->AsObject();const FString FromId=Spec->GetStringField(TEXT("from")),ToId=Spec->GetStringField(TEXT("to"));auto* FromPtr=Lookup.Find(FromId);auto* ToPtr=Lookup.Find(ToId);
  if(!FromPtr||!ToPtr)return F.Fail(TEXT("pcg_unknown_edge_node"),TEXT("Edge alias is not an exact current owned node"));auto* From=*FromPtr;auto* To=*ToPtr;
  if(!W.Admit(TEXT("fresh_edge_current")))return false;auto* A=From->GetOutputPin(FName(*Spec->GetStringField(TEXT("from_pin"))));if(!W.Admit(TEXT("fresh_after_output_pin_lookup")))return false;auto* B=To->GetInputPin(FName(*Spec->GetStringField(TEXT("to_pin"))));
  if(!A||!B||!W.PinNode(A)||!W.PinNode(B))return F.Fail(TEXT("pcg_edge_pin_missing"),TEXT("Requested exact physical output/input pin is absent"));
  bool Compatible=false;if(!W.Read(TEXT("fresh_Pin_IsCompatible"),[&]{return PCGDataTypeCompatibilityResult::IsValid(B->GetCompatibilityWithOtherPin(A));},Compatible))return false;
  bool Multiple=false;if(!W.Read(TEXT("fresh_Pin_AllowsMultipleConnections"),[&]{return B->AllowsMultipleConnections();},Multiple))return false;if(!Compatible||(!Multiple&&!B->Edges.IsEmpty()))return F.Fail(TEXT("pcg_edge_requires_conversion_or_replacement"),TEXT("Fresh spec requires direct compatible edges without replacing an input edge"));
  auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("graph_path"),G->GetPathName());Params->SetStringField(TEXT("from_node_id"),From->GetPathName());Params->SetStringField(TEXT("to_node_id"),To->GetPathName());Params->SetStringField(TEXT("from_pin"),Spec->GetStringField(TEXT("from_pin")));Params->SetStringField(TEXT("to_pin"),Spec->GetStringField(TEXT("to_pin")));
  auto Receipt=HandleConnect(Params);if(!W.R.Error.IsEmpty())return false;Receipt->SetBoolField(TEXT("actual_edge_union_verified"),true);Connected.Add(MakeShared<FJsonValueObject>(Receipt));Built->SetArrayField(TEXT("connected_edges"),Connected);
 }
 for(int32 I=0;I<Members.Num();++I)
 {
   if(F.Nodes[I].GeneralNative){if(!W.Admit(TEXT("general_native_final_settings")))return false;Created[I]->AsObject()->SetBoolField(TEXT("general_native_final_scalar_values_preserved"),true);continue;}
    if(F.Nodes[I].Spawner){if(!W.Admit(TEXT("fresh_final_inline_spawner")))return false;Created[I]->AsObject()->SetBoolField(TEXT("inline_spawner_final_expected_verified"),true);continue;}
  auto FiniteSettings=F.Nodes[I].Settings;if(FreshDataSourceClass(F.Nodes[I].Class)){FiniteSettings=MakeShared<FJsonObject>(*FiniteSettings);for(const auto& Pair:F.Nodes[I].Settings->Values){const FString Name(Pair.Key.ToView());const auto* Field=GraphSpecDataSourceAggregateField(F.Nodes[I].Class,Name);if(Field&&DataSourceAggregateName(FName(Field->Native)))FiniteSettings->RemoveField(Name);}if(!F.DataSourceWork||!F.DataSourceWork->Current())return false;Created[I]->AsObject()->SetBoolField(TEXT("data_source_aggregate_final_typed_values_preserved"),true);}
  TSharedPtr<FJsonObject> Final;if(!CubelessPCGUE58::VerifyFreshSettings(G,Members[I],FiniteSettings,Final,[&]{return W.Admit(TEXT("fresh_final_settings"));}))return F.Fail(TEXT("pcg_final_setting_readback_mismatch"),TEXT("Requested finite setting drifted during later callbacks"));Created[I]->AsObject()->SetObjectField(TEXT("settings_final_readback"),Final);
 }
 if(!W.Admit(TEXT("fresh_complete_build"))||W.Snapshot.Nodes.Num()!=F.Nodes.Num()||W.Snapshot.Edges.Num()!=F.Edges.Num())return F.Fail(TEXT("pcg_graph_inventory_mismatch"),TEXT("Actual authored nodes and physical edges differ from complete normalized spec"));
 auto Snapshot=SnapshotJson(W.Snapshot);Snapshot->SetStringField(TEXT("graph"),G->GetPathName());Snapshot->SetNumberField(TEXT("authored_node_count"),W.Snapshot.Nodes.Num());Snapshot->SetNumberField(TEXT("node_count_including_factory"),W.Snapshot.NodeSlots.Num());Snapshot->SetNumberField(TEXT("edge_count"),W.Snapshot.Edges.Num());Built->SetObjectField(TEXT("snapshot"),Snapshot);Built->SetBoolField(TEXT("build_verified"),true);return true;
}
}
}

namespace Cubeless::UE58::PCGGraphAuthoring { namespace {
// Exact local reuse of the existing platform-independent evidence SHA256 body.
FString FreshSha256Bytes(const void* Data, uint32 ByteCount)
{
    // FPlatformMisc::GetSHA256Signature asserts on stock UE 5.6 Windows because
    // that platform hook has no implementation. Keep evidence hashing fully
    // deterministic and platform-independent inside the authoring plugin.
    static constexpr uint32 K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

    uint32 State[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    const auto RotateRight = [](uint32 Value, uint32 Count) -> uint32
    {
        return (Value >> Count) | (Value << (32u - Count));
    };
    const auto ProcessBlock = [&State, &RotateRight](const uint8* Block)
    {
        uint32 W[64];
        for (int32 Index = 0; Index < 16; ++Index)
        {
            const int32 Offset = Index * 4;
            W[Index] = (static_cast<uint32>(Block[Offset]) << 24) |
                (static_cast<uint32>(Block[Offset + 1]) << 16) |
                (static_cast<uint32>(Block[Offset + 2]) << 8) |
                static_cast<uint32>(Block[Offset + 3]);
        }
        for (int32 Index = 16; Index < 64; ++Index)
        {
            const uint32 S0 = RotateRight(W[Index - 15], 7) ^ RotateRight(W[Index - 15], 18) ^ (W[Index - 15] >> 3);
            const uint32 S1 = RotateRight(W[Index - 2], 17) ^ RotateRight(W[Index - 2], 19) ^ (W[Index - 2] >> 10);
            W[Index] = W[Index - 16] + S0 + W[Index - 7] + S1;
        }

        uint32 A = State[0], B = State[1], C = State[2], D = State[3];
        uint32 E = State[4], F = State[5], G = State[6], H = State[7];
        for (int32 Index = 0; Index < 64; ++Index)
        {
            const uint32 S1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
            const uint32 Choice = (E & F) ^ ((~E) & G);
            const uint32 Temp1 = H + S1 + Choice + K[Index] + W[Index];
            const uint32 S0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
            const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
            const uint32 Temp2 = S0 + Majority;
            H = G; G = F; F = E; E = D + Temp1;
            D = C; C = B; B = A; A = Temp1 + Temp2;
        }
        State[0] += A; State[1] += B; State[2] += C; State[3] += D;
        State[4] += E; State[5] += F; State[6] += G; State[7] += H;
    };

    const uint8* Bytes = static_cast<const uint8*>(Data);
    uint32 Offset = 0;
    while (Offset + 64u <= ByteCount)
    {
        ProcessBlock(Bytes + Offset);
        Offset += 64u;
    }

    uint8 Tail[128] = {};
    const uint32 Remaining = ByteCount - Offset;
    if (Remaining > 0u && Bytes)
    {
        FMemory::Memcpy(Tail, Bytes + Offset, Remaining);
    }
    Tail[Remaining] = 0x80u;
    const uint32 TailBytes = Remaining < 56u ? 64u : 128u;
    const uint64 BitCount = static_cast<uint64>(ByteCount) * 8ull;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        Tail[TailBytes - 1u - Index] = static_cast<uint8>(BitCount >> (Index * 8));
    }
    ProcessBlock(Tail);
    if (TailBytes == 128u)
    {
        ProcessBlock(Tail + 64);
    }

    FString Result;
    Result.Reserve(64);
    for (const uint32 Word : State)
    {
        Result += FString::Printf(TEXT("%08x"), Word);
    }
    return Result;
}
} }

namespace Cubeless::UE58::PCGGraphAuthoring
{
namespace
{
TSharedPtr<FJsonObject> FreshFile(FFreshRequest& F)
{
 auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("path"),F.File);const int64 Size=IFileManager::Get().FileSize(*F.File);O->SetBoolField(TEXT("exists"),Size>=0);O->SetNumberField(TEXT("size"),Size);O->SetField(TEXT("sha256"),MakeShared<FJsonValueNull>());
 if(Size>=0&&Size<=256ll*1024*1024){TArray<uint8> Bytes;if(FFileHelper::LoadFileToArray(Bytes,*F.File)&&Bytes.Num()==Size)O->SetStringField(TEXT("sha256"),FreshSha256Bytes(Bytes.GetData(),Bytes.Num()));}return O;
}
bool FreshSave(FFreshRequest& F)
{
 auto& W=F.W;if(!W.Admit(TEXT("fresh_before_save"))||!F.PreservedDirty())return false;
 for(const auto& Check:F.SpawnerSaveChecks)if(!Check(false))return false;
 const auto BeforeObject=F.TargetFlags,BeforePackage=F.PackageObjectFlags;const uint32 BeforePackageFlags=F.PackageFlags;const bool BeforeDirty=F.TargetPackage->IsDirty();
 W.R.bSaveAttempted=true;W.R.Data->SetBoolField(TEXT("persistence_attempted"),true);auto Receipt=MakeShared<FJsonObject>();W.R.Data->SetObjectField(TEXT("save_receipt"),Receipt);Receipt->SetBoolField(TEXT("requested"),true);Receipt->SetBoolField(TEXT("attempted"),true);Receipt->SetBoolField(TEXT("dirty_before"),BeforeDirty);
 if(!W.SDK(TEXT("fresh_SavePackage"),true))return false;
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;
 const bool Saved=UPackage::SavePackage(F.TargetPackage,F.Target.Get(),*F.File,Args);
 W.R.bSaved=Saved;Receipt->SetBoolField(TEXT("call_returned"),true);Receipt->SetBoolField(TEXT("saved"),Saved);W.R.Data->SetBoolField(TEXT("saved"),Saved);W.R.Data->SetBoolField(TEXT("persistence_performed"),Saved);
 // File evidence is plain IO and is retained even if the next engine role fence fails.
 auto File=FreshFile(F);W.R.Data->SetObjectField(TEXT("save_file"),File);
 if(!Safe()||!Live(F.Target.Get())||!Live(F.TargetPackage))return F.Fail(TEXT("pcg_fresh_after_save_unobservable"),TEXT("Save bool/file retained; safe selected role unavailable"));
 const auto ActualObject=F.Target->GetFlags(),ActualPackage=F.TargetPackage->GetFlags();const uint32 ActualPackageFlags=F.TargetPackage->GetPackageFlags();
 const EObjectFlags LoadBits=RF_WasLoaded|RF_LoadCompleted;const bool SameUndo=GUndo==F.Undo;
 const bool ObjectFlags=ActualObject==BeforeObject||(Saved&&ActualObject==(BeforeObject|LoadBits));
 const bool PackageFlags=ActualPackage==BeforePackage||(Saved&&ActualPackage==(BeforePackage|LoadBits))||(Saved&&SameUndo&&F.Undo&&BeforeDirty&&(ActualPackage==(BeforePackage|RF_Transactional)||ActualPackage==(BeforePackage|RF_Transactional|LoadBits)));
 const bool StorageFlags=ActualPackageFlags==BeforePackageFlags||(Saved&&ActualPackageFlags==(BeforePackageFlags&~static_cast<uint32>(PKG_NewlyCreated)));
 const bool ExactRole=F.Target->GetClass()==UPCGGraph::StaticClass()&&F.Target->GetOuter()==F.TargetPackage&&F.TargetPackage->GetOuter()==nullptr&&Same(F.Target->GetPathName(),F.ObjectPath)&&Same(F.TargetPackage->GetName(),F.PackagePath);
 Receipt->SetStringField(TEXT("object_flags_before"),FString::Printf(TEXT("%u"),static_cast<uint32>(BeforeObject)));Receipt->SetStringField(TEXT("object_flags_after"),FString::Printf(TEXT("%u"),static_cast<uint32>(ActualObject)));Receipt->SetStringField(TEXT("package_object_flags_before"),FString::Printf(TEXT("%u"),static_cast<uint32>(BeforePackage)));Receipt->SetStringField(TEXT("package_object_flags_after"),FString::Printf(TEXT("%u"),static_cast<uint32>(ActualPackage)));Receipt->SetNumberField(TEXT("package_flags_before"),BeforePackageFlags);Receipt->SetNumberField(TEXT("package_flags_after"),ActualPackageFlags);Receipt->SetBoolField(TEXT("same_active_undo"),SameUndo);
 if(!ObjectFlags||!PackageFlags||!StorageFlags||!SameUndo||!ExactRole)return F.Fail(TEXT("pcg_fresh_save_identity_changed"),TEXT("Save prefix retained; selected exact package/object flags changed outside qualified Save transitions"));
 // Accept only the selected normal Save transition; all source/other identities
 // remain frozen and are checked before any subsequent SDK call.
 F.TargetFlags=ActualObject;F.PackageObjectFlags=ActualPackage;F.PackageFlags=ActualPackageFlags;
 if(Saved)for(const auto& Check:F.SpawnerSaveChecks)if(!Check(true))return false;
 if(!W.After(TEXT("fresh_SavePackage")))return false;
 if(Saved&&!F.SpawnerSaveChecks.IsEmpty())W.R.Data->SetBoolField(TEXT("spawner_save_export_flags_current_verified"),true);
 if(!Saved)return F.Fail(TEXT("pcg_graph_save_failed"),TEXT("Save returned false; file and memory prefix retained"));
 FString Hash;if(!File->GetBoolField(TEXT("exists"))||!File->TryGetStringField(TEXT("sha256"),Hash)||Hash.Len()!=64)return F.Fail(TEXT("pcg_graph_save_failed"),TEXT("Save bool was true but exact file SHA256 observation is incomplete"));
 if(!W.Write(TEXT("fresh_clear_saved_dirty"),[&]{F.TargetPackage->SetDirtyFlag(false);}))return false;Receipt->SetBoolField(TEXT("dirty_after"),F.TargetPackage->IsDirty());if(F.TargetPackage->IsDirty())return F.Fail(TEXT("pcg_graph_save_failed"),TEXT("Saved target package remains dirty"));
 // Direct SavePackage can precede the on-disk registry cache; refresh this exact saved owned file.
 if(!W.SDK(TEXT("fresh_registry_saved_file_refresh")))return false;F.Registry->ScanModifiedAssetFiles({F.File});W.R.Data->SetBoolField(TEXT("saved_file_registry_refresh_returned"),true);if(!W.After(TEXT("fresh_registry_saved_file_refresh")))return false;
 const auto RefreshedFile=FreshFile(F);W.R.Data->SetObjectField(TEXT("save_file_after_registry_refresh"),RefreshedFile);FString RefreshedHash;
 if(!RefreshedFile->GetBoolField(TEXT("exists"))||!RefreshedFile->TryGetStringField(TEXT("sha256"),RefreshedHash)||RefreshedHash!=Hash||!Same(RefreshedFile->GetStringField(TEXT("path")),File->GetStringField(TEXT("path")))||RefreshedFile->GetNumberField(TEXT("size"))!=File->GetNumberField(TEXT("size"))||F.TargetPackage->IsDirty())return F.Fail(TEXT("pcg_saved_file_changed_during_registry_refresh"),TEXT("Saved prefix retained; exact file or target dirty state changed during registry callbacks"));
 if(!F.PreservedDirty())return false;
 if(!W.SDK(TEXT("fresh_saved_dependencies")))return false;const FString AuditText=::Cubeless::Operations::AuditPackageDependencies({F.PackagePath},true,4096);W.R.Data->SetStringField(TEXT("dependency_audit_raw"),AuditText);if(!W.After(TEXT("fresh_saved_dependencies")))return false;
 TSharedPtr<FJsonObject> Audit;if(!Parse(AuditText,Audit,8*1024*1024))return F.Fail(TEXT("pcg_dependency_audit_unavailable"),TEXT("Saved prefix retained but existing native audit JSON is incomplete"));W.R.Data->SetObjectField(TEXT("audit"),Audit);bool Success=false,Passed=false;
 const TArray<TSharedPtr<FJsonValue>>* AuditEdges=nullptr;TArray<TSharedPtr<FJsonValue>> Hits;
 if(Audit->TryGetArrayField(TEXT("edges"),AuditEdges))for(const auto& Edge:*AuditEdges){if(!Edge||Edge->Type!=EJson::Object)continue;FString Path;if(!Edge->AsObject()->TryGetStringField(TEXT("to"),Path))continue;for(const auto& Prefix:F.ForbiddenPrefixes)if(Path.StartsWith(Prefix,ESearchCase::CaseSensitive)){Hits.Add(MakeShared<FJsonValueString>(Path));break;}}
 W.R.Data->SetArrayField(TEXT("forbidden_dependency_hits"),Hits);W.R.Data->SetStringField(TEXT("dependency_audit_scope"),TEXT("Existing recursive native saved-package audit, bounded4096; no new audit engine"));
 if(!Audit->TryGetBoolField(TEXT("success"),Success)||!Success||!Audit->TryGetBoolField(TEXT("passed"),Passed)||!Passed||!AuditEdges)return F.Fail(TEXT("pcg_dependency_audit_unavailable"),TEXT("Save succeeded but existing recursive audit did not pass"));
 W.R.Data->GetObjectField(TEXT("validation"))->SetStringField(TEXT("complete_dependency_scan"),TEXT("passed"));
 if(!Hits.IsEmpty())return F.Fail(TEXT("pcg_forbidden_dependency"),TEXT("Saved graph has a requested forbidden recursive dependency; saved prefix preserved"));return F.PreservedDirty();
}
bool FreshDescriptions(FFreshRequest& F)
{
 auto& W=F.W;TArray<UObject*> Owned{F.Target.Get()};for(const auto& N:W.Snapshot.NodeSlots)if(N.Node!=W.Snapshot.Input&&N.Node!=W.Snapshot.Output)Owned.Add(N.Settings);
 TArray<TSharedPtr<FJsonValue>> Observations;
 for(UObject* O:Owned)
 {
  if(!W.Admit(TEXT("fresh_description_owner")))return false;FProperty* P=FreshNativeField(O->GetClass(),TEXT("Description"));if(!P)continue;auto* Text=CastField<FTextProperty>(P);
  if(!Text||Text->HasSetterOrGetter())return F.Fail(TEXT("pcg_fresh_description_unsupported"),TEXT("Optional Description is not the exact native text field"));
  if(!W.SDK(TEXT("fresh_description_read")))return false;const FText Copy=Text->GetPropertyValue_InContainer(O);const FString Value=Copy.ToString();if(!W.After(TEXT("fresh_description_read")))return false;
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("object_path"),O->GetPathName());Row->SetNumberField(TEXT("tchar_length"),Value.Len());Row->SetBoolField(TEXT("unchanged"),true);Observations.Add(MakeShared<FJsonValueObject>(Row));
  if(Value.Len()>240)return F.Fail(TEXT("pcg_fresh_description_sanitize_unsupported"),TEXT("Fresh native description exceeds the old scan bound; no hidden description write or save executed"));
 }
 W.R.Data->SetArrayField(TEXT("description_observations"),Observations);W.R.Data->SetArrayField(TEXT("sanitized_descriptions"),TArray<TSharedPtr<FJsonValue>>());W.R.Data->SetBoolField(TEXT("description_sanitization_attempted"),false);return true;
}
bool FreshConstruct(FFreshRequest& F)
{
 auto& W=F.W;auto* RegistryModule=FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));if(!RegistryModule)return F.Fail(TEXT("pcg_fresh_registry_unavailable"),TEXT("Already-loaded AssetRegistry required"));F.Registry=&RegistryModule->Get();
 if(!F.Absent(TEXT("target_before"),false))
 {
  if(!W.R.Error.IsEmpty())return false;
  if(!F.DryRun)return F.Fail(F.Overwrite?TEXT("pcg_existing_target_overwrite_unsupported"):TEXT("pcg_asset_exists"),TEXT("Existing target is preserved; use a new candidate path"));
  W.R.Data->SetBoolField(TEXT("target_exists"),true);W.R.Data->SetBoolField(TEXT("can_execute"),false);W.R.Data->SetBoolField(TEXT("would_create_new"),false);W.R.Data->SetBoolField(TEXT("would_overwrite"),false);W.R.Data->SetStringField(TEXT("blocked_reason"),TEXT("Existing targets are never overwritten; class/property/pin preflight not run"));W.R.Data->GetObjectField(TEXT("validation"))->SetStringField(TEXT("complete_dependency_scan"),TEXT("not_run_no_persistent_asset"));return true;
 }
 if(!FreshSources(F))return false;
 W.R.Data->SetBoolField(TEXT("transient_graph_construction_attempted"),true);W.R.Data->SetBoolField(TEXT("transient_outcome_unknown"),true);W.R.Data->SetBoolField(TEXT("transient_preflight_executed"),true);W.R.Data->SetStringField(TEXT("mutation_scope"),TEXT("transient_graph_only"));
 if(!W.SDK(TEXT("fresh_NewObject_scratch"),true))return false;UPCGGraph* Scratch=NewObject<UPCGGraph>(GetTransientPackage(),UPCGGraph::StaticClass(),NAME_None,RF_Transient);
 const bool ScratchObserved=F.KeepReturned(Scratch,TEXT("scratch_graph"));if(ScratchObserved){F.Scratch.Reset(Scratch);W.R.bChanged=true;W.R.Data->SetStringField(TEXT("transient_graph_path"),Scratch->GetPathName());W.R.Data->SetBoolField(TEXT("transient_graph_construction_performed"),true);}
 if(!W.After(TEXT("fresh_NewObject_scratch")))return false;if(!ScratchObserved||Scratch->GetOuter()!=GetTransientPackage()||Scratch->GetClass()!=UPCGGraph::StaticClass())return F.Fail(TEXT("pcg_fresh_scratch_invalid"),TEXT("Native returned scratch graph role unavailable"));
 auto Preflight=MakeShared<FJsonObject>();W.R.Data->SetObjectField(TEXT("preflight"),Preflight);if(!FreshBuild(F,Scratch,Preflight))return false;
 W.R.Data->SetBoolField(TEXT("transient_outcome_unknown"),false);W.R.Data->SetBoolField(TEXT("transient_preflight_verified"),true);F.Completed.Add(W.Snapshot);
 if(!F.PreservedDirty()||!F.Absent(TEXT("target_after_preflight")))return false;
 if(F.DryRun){W.R.Data->SetBoolField(TEXT("can_execute"),true);W.R.Data->SetBoolField(TEXT("would_create_new"),true);W.R.Data->SetBoolField(TEXT("would_overwrite"),false);W.R.Data->GetObjectField(TEXT("validation"))->SetStringField(TEXT("complete_dependency_scan"),TEXT("not_run_no_persistent_asset"));return true;}
 // Creation of the factory object itself is part of the reported transient prefix.
 W.Snapshot={};W.Graph=nullptr;if(!W.SDK(TEXT("fresh_NewObject_factory"),true))return false;UFactory* Factory=NewObject<UFactory>(GetTransientPackage(),F.FactoryClass,NAME_None,RF_Transient);
 const bool FactoryObserved=F.KeepReturned(Factory,TEXT("factory_object"));if(FactoryObserved)F.Factory.Reset(Factory);if(!W.After(TEXT("fresh_NewObject_factory")))return false;if(!FactoryObserved)return F.Fail(TEXT("pcg_fresh_factory_allocation_failed"),TEXT("No safely observed factory object"));
 if(!F.Absent(TEXT("target_before_factory"))||!F.PreservedDirty())return false;
 W.R.Data->SetBoolField(TEXT("persistent_target_factory_attempted"),true);W.R.Data->SetStringField(TEXT("mutation_scope"),TEXT("persistent_target_and_transient_preflight"));
 if(!W.SDK(TEXT("fresh_AssetTools_CreateAsset"),true))return false;
 UObject* Returned=F.AssetTools->CreateAsset(F.AssetName,F.Folder,UPCGGraph::StaticClass(),Factory,NAME_None,false);
 const bool Observed=F.KeepReturned(Returned,TEXT("persistent_graph"));W.R.Data->SetBoolField(TEXT("factory_call_returned"),true);
 if(Observed){auto* G=Cast<UPCGGraph>(Returned);if(G){F.Target.Reset(G);F.TargetPackage=G->GetOutermost();F.TargetFlags=G->GetFlags();F.PackageObjectFlags=F.TargetPackage->GetFlags();F.PackageFlags=F.TargetPackage->GetPackageFlags();F.KeepReturned(F.TargetPackage,TEXT("persistent_package"));W.R.bChanged=true;W.R.Data->SetBoolField(TEXT("persistent_target_mutation_performed"),true);W.R.Data->SetBoolField(TEXT("created_new_asset"),true);W.R.Data->SetStringField(TEXT("object_path"),G->GetPathName());}}
 if(!W.After(TEXT("fresh_AssetTools_CreateAsset")))return false;
 if(!Observed||!F.Target||!F.Current())return F.Fail(TEXT("pcg_fresh_factory_return_invalid"),TEXT("Public AssetTools returned no exact new graph; internal package/registry/dirty steps are not individually observed"));
 const FAssetData Registered=F.Registry->GetAssetByObjectPath(FSoftObjectPath(F.ObjectPath));if(!W.Admit(TEXT("factory_registry_current"))||!Registered.IsValid()||Registered.GetSoftObjectPath()!=FSoftObjectPath(F.ObjectPath)||Registered.AssetClassPath!=UPCGGraph::StaticClass()->GetClassPathName())return F.Fail(TEXT("pcg_fresh_registry_missing"),TEXT("Exact returned graph was not currently registered"));
 auto Built=MakeShared<FJsonObject>();W.R.Data->SetObjectField(TEXT("persistent_build"),Built);if(!FreshBuild(F,F.Target.Get(),Built))return false;
 W.R.Data->SetArrayField(TEXT("created_nodes"),Built->GetArrayField(TEXT("created_nodes")));W.R.Data->SetArrayField(TEXT("connected_edges"),Built->GetArrayField(TEXT("connected_edges")));W.R.Data->SetObjectField(TEXT("graph_after"),Built->GetObjectField(TEXT("snapshot")));
 if(!FreshDescriptions(F))return false;
 if(F.Save){if(!FreshSave(F))return false;}else{auto File=FreshFile(F);W.R.Data->SetObjectField(TEXT("save_file"),File);W.R.Data->GetObjectField(TEXT("validation"))->SetStringField(TEXT("complete_dependency_scan"),TEXT("not_run_unsaved"));if(File->GetBoolField(TEXT("exists")))return F.Fail(TEXT("pcg_unrequested_persistence"),TEXT("Unsaved factory build unexpectedly produced an on-disk target"));}
 if(!F.PreservedDirty()||!W.Admit(TEXT("fresh_final_current")))return false;W.R.Data->GetObjectField(TEXT("validation"))->SetBoolField(TEXT("unrelated_dirty_state_preserved"),true);return true;
}
FString FreshFinish(FFreshRequest& F,bool Complete)
{
 auto& W=F.W;if(!Complete&&W.R.Error.IsEmpty())F.Fail(TEXT("pcg_fresh_incomplete"),TEXT("Fresh native operation did not complete; observed prefix retained"));const bool Verified=Complete&&W.R.Error.IsEmpty()&&W.Admit(TEXT("fresh_finish"));W.R.bSuccess=Verified;W.R.bPostconditionPassed=Verified;
 if(Verified)W.R.bChangeStateKnown=true;else if(W.R.bMutationAttempted)W.R.bChangeStateKnown=false;
 if(F.GeneralNative&&!Verified&&!W.R.bMutationAttempted&&!F.Scratch&&!F.Target&&!F.Factory&&Safe()&&F.Current())
 {
  bool IdentitiesCurrent=true;for(const auto& I:W.Objects)IdentitiesCurrent&=I->Exact();
  if(IdentitiesCurrent){W.R.bChangeStateKnown=true;W.R.Data->SetBoolField(TEXT("general_preflight_rejection_known_no_writer"),true);W.R.Data->SetStringField(TEXT("change_state_scope"),TEXT("No native writer/factory dispatched; held source identities/layout remain current. Does not claim global SDK purity."));}
 }
 W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("supported_variant_complete"),Verified);W.R.Data->SetStringField(TEXT("operation"),TEXT("create_pcg_graph_from_spec"));W.R.Data->SetStringField(TEXT("construction_route"),F.GeneralNative?TEXT("request_owned_native_fresh"):TEXT("finite_native_fresh"));W.R.Data->SetBoolField(TEXT("global_construction_gate_changed"),false);
 W.R.Data->SetBoolField(TEXT("outcome_unknown"),!W.R.bChangeStateKnown);W.R.Data->SetBoolField(TEXT("mutation_attempted"),W.R.bMutationAttempted);if(W.R.bChangeStateKnown)W.R.Data->SetBoolField(TEXT("mutation_performed"),W.R.bChanged);else W.R.Data->SetField(TEXT("mutation_performed"),MakeShared<FJsonValueNull>());
 W.R.Data->SetObjectField(TEXT("native_call_counts"),W.Counts);W.R.Data->SetArrayField(TEXT("actual_call_prefix"),W.Calls);W.R.Data->SetArrayField(TEXT("graph_transition_rows"),W.Transitions);W.R.Data->SetBoolField(TEXT("explicit_compile_attempted"),false);W.R.Data->SetBoolField(TEXT("PCG_generation_completed"),false);W.R.Data->SetBoolField(TEXT("construction_GC_health_resolved"),false);W.R.Data->SetBoolField(TEXT("game_runtime_Cubeless_dependency_added"),false);W.R.Data->SetBoolField(TEXT("post_failure_live_projection_skipped"),!W.R.Error.IsEmpty());
  if(F.InlineSpawnerProfile){W.R.Data->SetBoolField(TEXT("inline_stock_spawner_factory_profile"),true);W.R.Data->SetBoolField(TEXT("inline_stock_spawner_factory_values_preserved"),Verified);W.R.Data->SetBoolField(TEXT("inline_spawner_runtime_generation_verified"),false);W.R.Data->SetStringField(TEXT("inline_spawner_factory_scope"),TEXT("Exact stock Weighted/ByAttribute constructor/retirement, original inline phase/alias/empty semantics, loaded mesh/material and existing9 general fields. Unloaded asset loading, custom selector/packer, arbitrary inherited settings, numeric/container-to-name coercion and runtime generation remain incomplete. Copied pure input/readback ByAttribute errors preserve known-current earlier effects and continue Weighted before final failure; SDK/identity/current unknown errors stop immediately."));}
 if(F.DataSourceProfile){W.R.Data->SetBoolField(TEXT("data_source_aggregate_factory_profile"),true);W.R.Data->SetBoolField(TEXT("data_source_aggregate_authored_values_preserved"),Verified);W.R.Data->SetBoolField(TEXT("data_source_runtime_generation_verified"),false);W.R.Data->SetStringField(TEXT("data_source_aggregate_factory_scope"),TEXT("Original dict/list ActorSelector/ComponentSelector/ExpectedPins with actual-instance partial merge, public editor permission/change-chain setters, source/class holds and selected-only typed acceptance. Bool/null, string/null/bool Name values, nonhidden native Python enum tokens, null class clearing, empty nested selector dict and ordered string Name lists are implemented; existing class refs are preserved. Transient-only show writes are rejected rather than PPF_None-skipped. Nonempty ActorReferenceSelector, numeric/hidden/ScriptName coercions, property-error continuation across later settings/nodes/edges, broader source profiles and runtime generation remain incomplete; implemented usage validation pending."));}
 if(F.SamplingDifferenceProfile){W.R.Data->SetBoolField(TEXT("sampling_difference_factory_profile"),true);W.R.Data->SetBoolField(TEXT("sampling_difference_values_preserved"),Verified);W.R.Data->SetBoolField(TEXT("sampling_runtime_execution_verified"),false);W.R.Data->SetBoolField(TEXT("surface_keep_zero_serialized_persistence_verified"),false);W.R.Data->SetStringField(TEXT("sampling_difference_factory_scope"),TEXT("Exact SurfaceSampler8/Difference4 native fields using existing scalar/vector/enum and current guards. Surface bKeepZeroDensityPoints is transient Editor debug state; deprecated PointRadius is excluded. No runtime generation or all inherited-property claim."));}
 if(F.TransformPointsProfile){W.R.Data->SetBoolField(TEXT("transform_points_factory_profile"),true);W.R.Data->SetBoolField(TEXT("transform_points_authored_values_preserved"),Verified);W.R.Data->SetBoolField(TEXT("transform_points_runtime_generation_verified"),false);W.R.Data->SetStringField(TEXT("transform_points_factory_scope"),TEXT("Exact native TransformPoints thirteen own authored fields with held Vector/Rotator layouts; partial dicts preserve actual unprovided components. No generation/GPU execution or wider inherited setting claim."));}
 if(F.PointBoundsProfile){W.R.Data->SetBoolField(TEXT("point_bounds_factory_profile"),true);W.R.Data->SetBoolField(TEXT("point_bounds_factory_field_values_preserved"),Verified);W.R.Data->SetBoolField(TEXT("point_bounds_runtime_generation_verified"),false);W.R.Data->SetStringField(TEXT("point_bounds_factory_scope"),TEXT("Exact existing PA two/five authored fields and held source/layout/current topology; inherited/unobserved properties and general graph classes remain incomplete"));}
 if(F.GeneralNative){W.R.Data->SetBoolField(TEXT("request_owned_current_proof"),Verified);W.R.Data->SetBoolField(TEXT("Python_PCG_UObject_wrappers_used"),false);W.R.Data->SetBoolField(TEXT("general_native_constructor_profile"),true);W.R.Data->SetStringField(TEXT("general_native_scope"),TEXT("Already-loaded concrete installed /Script/PCG settings classes with existing held native CDO/layout; fresh owned graph/node/settings and current graph snapshots. Public bool/numeric/String/Name/native PCG enum scalar fields retain planned native copies/editor setters. Closed native CoreUObject Vector/Vector2D/Rotator/Quat/LinearColor/Transform/Box fields additionally use owned typed copies, actual-instance partial dict merge, native field-layout leases and exact finite semantic readback. Vector/Rotator numeric arrays consume the original first three components. Existing finite18 behavior retained. Other struct/container/object/class coercion, nonnumeric legacy float coercions, duplicate aliases, custom Blueprint settings, lazy source CDO creation, property-error continuation, replacement of existing targets and runtime generation remain incomplete. No construction-health global release or Python UObject/container path."));}
  W.R.Stage=Verified?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,TEXT("create_pcg_graph_from_spec"));
}
}
FString CreateGraphFromSpec(const FString& ParamsJson)
{
 FWork W;FFreshRequest F(W);FStandaloneRequest DataSourceWork(W);F.DataSourceWork=&DataSourceWork;TSharedPtr<FJsonObject> P;
 if(!Parse(ParamsJson,P,2097152)){W.Fail(TEXT("pcg_graph_invalid_params_json"),TEXT("Bounded normalized JSON carrier without duplicate keys required"));return FreshFinish(F,false);}
 if(!FreshParse(F,P))return FreshFinish(F,false);
 W.R.bSaveRequested=F.Save&&!F.DryRun;W.R.Data->SetStringField(TEXT("asset_path"),F.PackagePath);W.R.Data->SetBoolField(TEXT("dry_run"),F.DryRun);W.R.Data->SetBoolField(TEXT("save_requested"),F.Save);W.R.Data->SetBoolField(TEXT("saved"),false);W.R.Data->SetBoolField(TEXT("overwrite_existing_requested"),F.Overwrite);W.R.Data->SetBoolField(TEXT("allow_overwrite_non_temp_requested"),F.AllowOverwrite);W.R.Data->SetNumberField(TEXT("requested_node_count"),F.Nodes.Num());W.R.Data->SetNumberField(TEXT("requested_edge_count"),F.Edges.Num());W.R.Data->SetObjectField(TEXT("validation"),MakeShared<FJsonObject>());
 for(const TCHAR* K:{TEXT("transient_graph_construction_attempted"),TEXT("transient_graph_construction_performed"),TEXT("transient_outcome_unknown"),TEXT("transient_preflight_executed"),TEXT("transient_preflight_verified"),TEXT("persistent_target_factory_attempted"),TEXT("persistent_target_mutation_performed")})W.R.Data->SetBoolField(K,false);
 if(GIsImportingT3D){W.Fail(TEXT("pcg_fresh_import_unsupported"),TEXT("T3D import is active; rejected before the first lookup"));return FreshFinish(F,false);}
 if(!IsInGameThread()||Running){W.Fail(TEXT("pcg_graph_reentrant_or_thread"),TEXT("Requires a non-reentrant Editor game-thread request"));return FreshFinish(F,false);}
 TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);W.FreshCurrent=[&]{return F.Current();};if(!W.Admit(TEXT("fresh_preflight")))return FreshFinish(F,false);
 W.FreshSnapshot=[&]{return FreshSnapshotCurrent(W.Snapshot)||F.Fail(TEXT("pcg_fresh_snapshot_changed"),TEXT("Current owned graph resolved physical inventory changed"));};W.FreshCapture=[&](UPCGGraph* G){return F.CaptureBegin(G);};W.FreshCaptureDone=[&]{F.CapturingReferences=false;};W.FreshPins=[&](UPCGNode* N,bool Output){TArray<UPCGPin*> Out;for(UObject* O:F.CaptureValues(N,Output?TEXT("OutputPins"):TEXT("InputPins"))){if(O->GetClass()!=UPCGPin::StaticClass()){F.Fail(TEXT("pcg_fresh_pin_class"),TEXT("Expected exact current pin"));return TArray<UPCGPin*>();}Out.Add(static_cast<UPCGPin*>(O));}return Out;};W.FreshEdges=[&](UPCGPin* P){TArray<UPCGEdge*> Out;for(UObject* O:F.CaptureValues(P,TEXT("Edges"))){if(O->GetClass()!=UPCGEdge::StaticClass()){F.Fail(TEXT("pcg_fresh_edge_class"),TEXT("Expected exact current edge"));return TArray<UPCGEdge*>();}Out.Add(static_cast<UPCGEdge*>(O));}return Out;};W.FreshEdgePin=[&](UPCGEdge* E,bool Input){auto Values=F.CaptureValues(E,Input?TEXT("InputPin"):TEXT("OutputPin"));return Values.Num()==1&&Values[0]->GetClass()==UPCGPin::StaticClass()?static_cast<UPCGPin*>(Values[0]):nullptr;};const bool Complete=FreshConstruct(F);const FString Result=FreshFinish(F,Complete);W.FreshCurrent=nullptr;return Result;
}
}
