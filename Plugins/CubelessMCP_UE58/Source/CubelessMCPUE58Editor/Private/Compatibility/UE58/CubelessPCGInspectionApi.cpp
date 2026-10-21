// GI dedicated read-only PCG inspection. Editor source candidate; Engine execution unverified.
#include "Compatibility/UE58/CubelessPCGInspectionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Data/PCGBasePointData.h"
#include "Data/Registry/PCGDataType.h"
#include "Data/Registry/PCGDataTypeIdentifier.h"
#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGManagedResource.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::PCGInspection
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O){FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));}return S;}
struct FIdentity
{TStrongObjectPtr<const UObject> Hold{nullptr};TWeakObjectPtr<const UObject> Object,Outer;TWeakObjectPtr<UPackage> Package;TWeakObjectPtr<UClass> Class;FString Path;explicit FIdentity(const UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Package(O->GetOutermost()),Class(O->GetClass()),Path(O->GetPathName()){}bool Exact()const{const auto* O=Object.Get();return Live(O)&&Live(Package.Get())&&Live(Class.Get())&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetClass()==Class.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}};
struct FGraphIdentity{TWeakObjectPtr<UPCGGraph> Graph;TWeakObjectPtr<UPCGNode> Input,Output;TArray<TWeakObjectPtr<UPCGNode>> Nodes;};
struct FNodeIdentity{TWeakObjectPtr<UPCGNode> Node;TWeakObjectPtr<UPCGSettingsInterface> Interface;TWeakObjectPtr<UPCGSettings> InstanceSettings;bool IsInstance=false;TArray<TWeakObjectPtr<UPCGPin>> Input,Output;};
struct FPinIdentity{TWeakObjectPtr<const UPCGPin> Pin;TWeakObjectPtr<const UPCGNode> Node;FName Label;FPCGDataTypeIdentifier Allowed;TArray<TWeakObjectPtr<const UPCGEdge>> Edges;};
struct FEdgeIdentity{TWeakObjectPtr<const UPCGEdge> Edge;TWeakObjectPtr<UPCGPin> Input,Output;};
struct FTaggedSnapshot{const UPCGData* Data=nullptr;FName Pin;TSet<FString> Tags;bool bPinlessData=false,bIsUsedMultipleTimes=false;int32 OriginalIndex=INDEX_NONE;};
struct FActorComponentsIdentity{TWeakObjectPtr<AActor> Actor;TWeakObjectPtr<ULevel> Level;TArray<TWeakObjectPtr<UPCGComponent>> Components;};
struct FComponentIdentity{TWeakObjectPtr<UPCGComponent> Component;TWeakObjectPtr<AActor> Owner;TArray<FTaggedSnapshot> Output;};
bool SameTags(const TSet<FString>& A,const TSet<FString>& B){if(A.Num()!=B.Num())return false;for(const FString& V:A){const FString* Found=B.Find(V);if(!Found||!V.Equals(*Found,ESearchCase::CaseSensitive))return false;}return true;}
struct FWork
{
    FResult R;FString FailedStage;bool WorldCaptured=false;TWeakObjectPtr<UWorld> World;TArray<TUniquePtr<FIdentity>> Objects;TArray<FGraphIdentity> Graphs;TArray<FNodeIdentity> Nodes;TArray<FPinIdentity> Pins;TArray<FEdgeIdentity> Edges;TArray<FComponentIdentity> Components;TArray<FActorComponentsIdentity> ActorComponents;TMap<const UPCGBasePointData*,int32> PointCounts;int32 SDKCalls=0;int64 CaptureSlots=0,OutputRows=0,TextUnits=0;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Partial;
    FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_observation_prefix"),Prefix);R.Data->SetBoolField(TEXT("explicit_source_mutation_attempted"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty()){Fail(TEXT("pcg_inspection_failed"),Message);}auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
    bool Pin(const UObject* O){if(!Live(O)){return Fail(TEXT("pcg_inspection_object_unavailable"),TEXT("Expected a live loaded object."));}for(const auto& I:Objects){if(I->Object.Get()==O){return I->Exact()||Fail(TEXT("pcg_inspection_identity_changed"),TEXT("Previously captured object identity changed."));}}if(!CaptureBudget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool Admit(const FString& Stage,bool ObserveFailure=false)
    {
        R.Stage=Stage;if(!R.Error.IsEmpty()&&!ObserveFailure){return false;}if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||GIsSavingPackage){return Fail(TEXT("pcg_inspection_context_unavailable"),TEXT("Editor game thread without GC/exit/save required. The PCG GC gate is not relaxed; no replay."));}
        if(WorldCaptured){auto* CurrentWorld=GEditor->GetEditorWorldContext().World();if(!Live(World.Get())||CurrentWorld!=World.Get()||World->bIsTearingDown){return Fail(TEXT("pcg_inspection_world_changed"),TEXT("Captured Editor world changed or is tearing down. No PIE-world substitution."));}}
        for(const auto& I:Objects){if(!I->Exact()){return Fail(TEXT("pcg_inspection_identity_changed"),TEXT("Captured class/outer/package/path changed; no replacement was adopted."));}}
        for(const auto& G:Graphs){auto* O=G.Graph.Get();if(!Live(O)||O->GetInputNode()!=G.Input.Get()||O->GetOutputNode()!=G.Output.Get()||O->GetNodes().Num()!=G.Nodes.Num()){return Fail(TEXT("pcg_inspection_graph_topology_changed"),TEXT("PCG input/output/node membership changed."));}for(int32 I=0;I<G.Nodes.Num();++I){if(O->GetNodes()[I]!=G.Nodes[I].Get()){return Fail(TEXT("pcg_inspection_graph_topology_changed"),TEXT("Native node order changed."));}}}
        auto SamePins=[](const auto& A,const auto& E){if(A.Num()!=E.Num()){return false;}for(int32 I=0;I<A.Num();++I){if(A[I]!=E[I].Get()){return false;}}return true;};
        for(const auto& N:Nodes){auto* O=N.Node.Get();if(!Live(O)||O->GetSettingsInterface()!=N.Interface.Get()||!SamePins(O->GetInputPins(),N.Input)||!SamePins(O->GetOutputPins(),N.Output)){return Fail(TEXT("pcg_inspection_node_binding_changed"),TEXT("Node settings interface/physical pins changed."));}if(N.IsInstance){auto* I=Cast<UPCGSettingsInstance>(N.Interface.Get());if(!Live(I)||I->Settings!=N.InstanceSettings.Get()){return Fail(TEXT("pcg_inspection_settings_instance_changed"),TEXT("Settings instance target changed across SDK callback."));}}}
        for(const auto& P:Pins){auto* O=P.Pin.Get();if(!Live(O)||O->Node!=P.Node.Get()||O->Properties.Label!=P.Label||!O->Properties.AllowedTypes.IsIdentical(P.Allowed)||O->Edges.Num()!=P.Edges.Num()){return Fail(TEXT("pcg_inspection_pin_binding_changed"),TEXT("Pin owner/label/type/edge count changed."));}for(int32 I=0;I<P.Edges.Num();++I){if(O->Edges[I]!=P.Edges[I].Get()){return Fail(TEXT("pcg_inspection_pin_binding_changed"),TEXT("Pin edge order changed."));}}}
        for(const auto& E:Edges){auto* O=E.Edge.Get();if(!Live(O)||O->InputPin!=E.Input.Get()||O->OutputPin!=E.Output.Get()){return Fail(TEXT("pcg_inspection_edge_binding_changed"),TEXT("Captured edge endpoints changed."));}}
        for(const auto& A:ActorComponents){auto* O=A.Actor.Get();if(!Live(O)||O->GetLevel()!=A.Level.Get()||!Live(A.Level.Get())||A.Level->OwningWorld!=World.Get()){return Fail(TEXT("pcg_inspection_actor_world_changed"),TEXT("Actor level/Editor-world identity changed."));}TArray<UPCGComponent*> Actual;O->GetComponents<UPCGComponent>(Actual);if(Actual.Num()!=A.Components.Num()){return Fail(TEXT("pcg_inspection_actor_components_changed"),TEXT("Actor PCG component membership changed."));}for(int32 I=0;I<Actual.Num();++I){if(Actual[I]!=A.Components[I].Get())return Fail(TEXT("pcg_inspection_actor_components_changed"),TEXT("Actor PCG component order changed."));}}
        for(const auto& C:Components){auto* O=C.Component.Get();if(!Live(O)||O->GetOwner()!=C.Owner.Get()){return Fail(TEXT("pcg_inspection_component_binding_changed"),TEXT("Component owner/world changed."));}const auto& A=O->GetGeneratedGraphOutput().TaggedData;const auto& E=C.Output;if(A.Num()!=E.Num()){return Fail(TEXT("pcg_inspection_output_changed"),TEXT("Generated output count changed during observation."));}for(int32 I=0;I<A.Num();++I){if(A[I].Data.Get()!=E[I].Data||A[I].Pin!=E[I].Pin||A[I].OriginalIndex!=E[I].OriginalIndex||A[I].bPinlessData!=E[I].bPinlessData||A[I].bIsUsedMultipleTimes!=E[I].bIsUsedMultipleTimes||!SameTags(A[I].Tags,E[I].Tags)){return Fail(TEXT("pcg_inspection_output_changed"),TEXT("Generated tagged-data pointer/pin/tags/order changed."));}}}
        return true;
    }
    bool CaptureBudget(int64 Slots,int64 Text=0){CaptureSlots+=Slots;TextUnits+=Text;Prefix->SetNumberField(TEXT("capture_slots"),static_cast<double>(CaptureSlots));Prefix->SetNumberField(TEXT("observed_text_utf16_units"),static_cast<double>(TextUnits));if(CaptureSlots>262144||TextUnits>32ll*1024*1024)return Fail(TEXT("pcg_inspection_capture_budget_exceeded"),TEXT("Request-wide capture/text observation budget exceeded; partial evidence retained."));return true;}
    bool OutputBudget(int64 Text=0){++OutputRows;TextUnits+=Text;Prefix->SetNumberField(TEXT("output_rows"),static_cast<double>(OutputRows));Prefix->SetNumberField(TEXT("observed_text_utf16_units"),static_cast<double>(TextUnits));if(OutputRows>262144||TextUnits>32ll*1024*1024)return Fail(TEXT("pcg_inspection_output_budget_exceeded"),TEXT("Request-wide output-row/text budget exceeded; no completeness claim."));return true;}
    bool SDK(const TCHAR* Stage){if(!Admit(Stage)){return false;}if(++SDKCalls>262144){return Fail(TEXT("pcg_inspection_observation_budget_exceeded"),TEXT("SDK observation budget exceeded; partial evidence retained and no completeness claim."));}double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);return true;}
    bool After(const TCHAR* Stage){Prefix->SetBoolField(FString(Stage)+TEXT("_returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
    template<typename F,typename T>bool Read(const TCHAR* Stage,F&& Getter,T& Out){if(!SDK(Stage)){return false;}Out=Getter();return After(Stage);}
    bool CaptureWorld(UWorld* O){if(!Live(O)||O->bIsTearingDown){return Fail(TEXT("pcg_inspection_world_unavailable"),TEXT("Failed to get editor world"));}World=O;WorldCaptured=true;return true;}
    bool Edge(const UPCGEdge* O){if(!O){return true;}for(const auto& E:Edges){if(E.Edge.Get()==O){return true;}}if(!Pin(O)){return false;}FEdgeIdentity E;E.Edge=O;E.Input=O->InputPin;E.Output=O->OutputPin;for(const auto* P:{E.Input.Get(),E.Output.Get()}){if(P&&(!Pin(P)||(P->Node&&!Pin(P->Node.Get())))){return false;}}Edges.Add(E);return true;}
    bool PinNode(const UPCGPin* O)
    {if(!O){return true;}for(const auto& P:Pins){if(P.Pin.Get()==O){return true;}}if(!Pin(O)){return false;}FPinIdentity P;P.Pin=O;P.Node=O->Node;P.Label=O->Properties.Label;P.Allowed=O->Properties.AllowedTypes;if(!CaptureBudget(O->Edges.Num()))return false;if(O->Edges.Num()>262144){return Fail(TEXT("pcg_inspection_edge_bound"),TEXT("Pin edge bound exceeded."));}for(const UPCGEdge* E:O->Edges){P.Edges.Add(E);if(!Edge(E)){return false;}if(E&&E->InputPin!=O&&E->OutputPin!=O){return Fail(TEXT("pcg_inspection_malformed_edge"),TEXT("Pin references an edge with different endpoints."));}}Pins.Add(MoveTemp(P));return true;}
    bool Node(UPCGNode* O)
    {if(!O){return true;}for(const auto& N:Nodes){if(N.Node.Get()==O){return true;}}if(!Pin(O)){return false;}FNodeIdentity N;N.Node=O;N.Interface=O->GetSettingsInterface();if(N.Interface.IsValid()&&!Pin(N.Interface.Get())){return false;}if(auto* I=Cast<UPCGSettingsInstance>(N.Interface.Get())){N.IsInstance=true;N.InstanceSettings=I->Settings;if(N.InstanceSettings.IsValid()&&!Pin(N.InstanceSettings.Get())){return false;}}if(!CaptureBudget(static_cast<int64>(O->GetInputPins().Num())+O->GetOutputPins().Num()))return false;if(O->GetInputPins().Num()+O->GetOutputPins().Num()>16384){return Fail(TEXT("pcg_inspection_node_pin_bound"),TEXT("Node pin bound exceeded."));}for(UPCGPin* P:O->GetInputPins()){if(P&&(P->Node!=O||O->GetOutputPins().Contains(P))){return Fail(TEXT("pcg_inspection_input_pin_owner_mismatch"),TEXT("Input slot pin has a foreign owner or also belongs to output."));}N.Input.Add(P);if(!PinNode(P)){return false;}}for(UPCGPin* P:O->GetOutputPins()){if(P&&(P->Node!=O||O->GetInputPins().Contains(P))){return Fail(TEXT("pcg_inspection_output_pin_owner_mismatch"),TEXT("Output slot pin has a foreign owner or also belongs to input."));}N.Output.Add(P);if(!PinNode(P)){return false;}}Nodes.Add(MoveTemp(N));return true;}
    bool Graph(UPCGGraph* O)
    {if(!O){return true;}for(const auto& G:Graphs){if(G.Graph.Get()==O){return true;}}if(!Pin(O)||!Pin(O->GetOutermost())){return false;}if(!CaptureBudget(O->GetNodes().Num()))return false;if(O->GetNodes().Num()>100000){return Fail(TEXT("pcg_inspection_graph_node_bound"),TEXT("PCG graph node bound exceeded."));}FGraphIdentity G;G.Graph=O;G.Input=O->GetInputNode();G.Output=O->GetOutputNode();if((G.Input.IsValid()&&!Pin(G.Input.Get()))||(G.Output.IsValid()&&!Pin(G.Output.Get()))){return false;}for(UPCGNode* N:O->GetNodes()){G.Nodes.Add(N);if(N&&!Pin(N)){return false;}}Graphs.Add(MoveTemp(G));return true;}
    bool CaptureActorComponents(AActor* O,const TArray<UPCGComponent*>& List)
    {if(!Pin(O)||!Pin(O->GetLevel()))return false;if(O->GetLevel()->OwningWorld!=World.Get())return Fail(TEXT("pcg_inspection_actor_world_mismatch"),TEXT("Selected actor does not belong to the captured Editor world."));if(!CaptureBudget(List.Num()))return false;if(List.Num()>100000)return Fail(TEXT("pcg_inspection_actor_component_bound"),TEXT("PCG component list exceeds100000."));FActorComponentsIdentity A;A.Actor=O;A.Level=O->GetLevel();for(UPCGComponent* C:List){if(C&&(!Pin(C)||C->GetOwner()!=O))return Fail(TEXT("pcg_inspection_component_owner_mismatch"),TEXT("Actor component list has a foreign PCG component."));A.Components.Add(C);}ActorComponents.Add(MoveTemp(A));return true;}
    bool Component(UPCGComponent* O)
    {for(const auto& C:Components){if(C.Component.Get()==O){return true;}}if(!Pin(O)||!Pin(O->GetOwner())){return false;}FComponentIdentity C;C.Component=O;C.Owner=O->GetOwner();const auto& Tagged=O->GetGeneratedGraphOutput().TaggedData;if(!CaptureBudget(Tagged.Num()))return false;if(Tagged.Num()>100000){return Fail(TEXT("pcg_inspection_tagged_data_bound"),TEXT("Generated tagged data exceeds100000; no false completeness."));}for(const auto& T:Tagged){FTaggedSnapshot V;V.Data=T.Data.Get();V.Pin=T.Pin;for(const FString& Tag:T.Tags){if(!CaptureBudget(1,Tag.Len()))return false;}V.Tags=T.Tags;V.bPinlessData=T.bPinlessData;V.bIsUsedMultipleTimes=T.bIsUsedMultipleTimes;V.OriginalIndex=T.OriginalIndex;if(V.Data&&!Pin(V.Data)){return false;}C.Output.Add(MoveTemp(V));}Components.Add(MoveTemp(C));return true;}
    bool PointCount(const UPCGBasePointData* O,int32& Out)
    {if(!Pin(O)||!Read(TEXT("point_GetNumPoints"),[&]{return O->GetNumPoints();},Out)){return false;}if(Out<0){return Fail(TEXT("pcg_inspection_invalid_point_count"),TEXT("Point data returned a negative point count."));}if(const auto* E=PointCounts.Find(O)){if(*E!=Out){return Fail(TEXT("pcg_inspection_point_count_changed"),TEXT("Point count changed across virtual getters."));}}else{PointCounts.Add(O,Out);}return true;}
    bool VerifyPointCount(const UPCGBasePointData* O){int32 Count=0;return PointCount(O,Count);}
    bool AddCount(int32& Target,int32 Add,const TCHAR* Field){const int64 Next=static_cast<int64>(Target)+Add;Prefix->SetStringField(FString(Field)+TEXT("_observed_sum_decimal"),LexToString(Next));if(Add<0||Next>MAX_int32){return Fail(TEXT("pcg_inspection_count_overflow"),TEXT("Original int32 aggregate count would overflow; exact observed subtotal retained."));}Target=static_cast<int32>(Next);return true;}
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("read_continuation"));}
UPCGSettings* SettingsFor(UPCGNode* O){if(!O||!Current||!Current->Node(O)){return nullptr;}UPCGSettings* S=nullptr;if(!Current->Read(TEXT("node_GetSettings"),[&]{return O->GetSettings();},S)){return nullptr;}if(S&&!Current->Pin(S)){return nullptr;}return S;}
FString NodeTitle(UPCGNode* O){FString S;if(!Current||!O||!Current->Node(O)||!Current->Read(TEXT("node_GetNodeTitle"),[&]{return O->GetNodeTitle(EPCGNodeTitleType::ListView).ToString();},S)){return FString();}if(!Current->CaptureBudget(0,S.Len()))return FString();return S;}
bool RawDepth(const FString& S)
{
    int32 Depth=0;bool String=false,Escape=false;for(TCHAR C:S){if(C==0){return false;}if(String){if(Escape){Escape=false;}else if(C=='\\'){Escape=true;}else if(C=='"'){String=false;}continue;}if(C=='"'){String=true;}else if(C=='{'||C=='['){if(++Depth>64){return false;}}else if(C=='}'||C==']'){if(--Depth<0){return false;}}}return !String&&Depth==0;
}

bool FiniteValues(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Slots)
{
    if(!V.IsValid()||Depth>64||++Slots>262144){return false;}if(V->Type==EJson::Number){double X=0;return V->TryGetNumber(X)&&FMath::IsFinite(X);}if(V->Type==EJson::String){for(TCHAR C:V->AsString()){if(C==0){return false;}}return true;}
    if(V->Type==EJson::Array){for(const auto& I:V->AsArray()){if(!FiniteValues(I,Depth+1,Slots)){return false;}}}
    else if(V->Type==EJson::Object){for(const auto& P:V->AsObject()->Values){if(!FiniteValues(P.Value,Depth+1,Slots)){return false;}}}return true;
}
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>{static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}};
bool ParseParams(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>1024*1024||!RawDepth(Text)){return false;}
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));
    TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>262144){return false;}
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        {const FString Key(Reader->GetIdentifier());for(TCHAR C:Key){if(C==0){return false;}}if(Keys.Last().Contains(Key)){return false;}Keys.Last().Add(Key);}
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        {Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>65){return false;}}
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        {if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()||Values[0]->Type!=EJson::Object){return false;}
    int32 Visits=0;if(!FiniteValues(Values[0],0,Visits)){return false;}Out=Values[0]->AsObject();return Out.IsValid();
}


int32 IntegerField(const TSharedPtr<FJsonObject>& P,const TCHAR* Key)
{double V=0;P->TryGetNumberField(Key,V);if(!FMath::IsFinite(V)||V<MIN_int32||V>MAX_int32){Current->Fail(TEXT("pcg_inspection_integer_out_of_range"),FString::Printf(TEXT("'%s' cannot safely use the original int32 cast."),Key));return 0;}return static_cast<int32>(V);}
UPCGGraph* LoadGraph(const FString& Path)
{UPCGGraph* G=nullptr;if(!Current->Read(TEXT("LoadObject_graph"),[&]{return LoadObject<UPCGGraph>(nullptr,*Path);},G))return nullptr;if(G&&(!Current->Pin(G)||!Current->Pin(G->GetOutermost())))return nullptr;return G;}

bool JsonBoolOrDefault(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, bool bDefault);
bool ComponentHasTagString(const UActorComponent* Component, const FString& Tag);
TArray<AActor*> FindActorsForPCGCommands(
    UWorld* World,
    const FString& ActorLabel,
    const FString& ActorName,
    const FString& ActorPath,
    const FString& ActorTag,
    const FString& ActorLabelPrefix,
    bool bSelectedOnly);
TSharedPtr<FJsonObject> ActorSummaryToJson(const AActor* Actor);
TArray<FString> FindPCGGraphAssetPaths(const FString& GraphPathOrName);
UPCGGraph* FindPCGGraph(const FString& GraphPathOrName);
UPCGNode* FindPCGNodeById(UPCGGraph* Graph, const FString& NodeId);
TSharedPtr<FJsonObject> PCGNodeToJson(UPCGNode* Node, bool bIncludePins);
bool IsPCGCustomHLSLSettings(const UPCGSettings* Settings);
TSharedPtr<FJsonObject> PCGCustomHLSLNodeToJson(
    UPCGNode* Node,
    bool bIncludePins,
    bool bIncludeDeclarations,
    int32 MaxSourceChars);
bool MatchesNodeFilter(UPCGNode* Node, const FString& NodeType, const FString& TitleContains);
TSharedPtr<FJsonObject> BoxToJsonObject(const FBox& Box);
FString GetActorLabelForPCGCommands(const AActor* Actor);
bool ActorMatchesPCGCommandFilters(
    const AActor* Actor,
    const FString& ActorLabel,
    const FString& ActorName,
    const FString& ActorPath,
    const FString& ActorTag,
    const FString& ActorLabelPrefix);
FString NormalizePCGObjectPathForLoad(const FString& ObjectPath);
FString GetReflectedStringPropertyValue(const UObject* Object, const FName& PropertyName, bool& bOutFound);
void AddExportedPropertyIfPresent(
    TSharedPtr<FJsonObject> Object,
    UObject* SourceObject,
    const FName& PropertyName,
    const FString& JsonFieldName);
void AddTruncatedTextField(
    TSharedPtr<FJsonObject> Object,
    const FString& FieldName,
    const FString& Text,
    int32 MaxChars);
void AddReflectedStringPropertyText(
    TSharedPtr<FJsonObject> Object,
    const UObject* SourceObject,
    const FName& PropertyName,
    const FString& JsonFieldName,
    int32 MaxChars);
TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Value);
bool ActorHasTagString(const AActor* Actor, const FString& Tag);
FString PCGDataTypeToString(EPCGDataType DataType);
TArray<TSharedPtr<FJsonValue>> StringSetToJsonArray(const TSet<FString>& Values);
TArray<TSharedPtr<FJsonValue>> Vector4ToJsonArray(const FVector4& Value);
TSharedPtr<FJsonObject> TransformToJsonObject(const FTransform& Transform);
TSharedPtr<FJsonObject> RotatorToJsonObject(const FRotator& Rotator);
TArray<UPCGNode*> GraphNodes(UPCGGraph* G,bool IncludeIO);
TArray<const UPCGPin*> NodePins(UPCGNode* N,bool Output);
bool ExactPinOwner(const UPCGPin* P);
EPCGDataType LegacyType(const FPCGDataTypeIdentifier& Identifier);
TSharedPtr<FJsonObject> PCGPinToJson(const UPCGPin* P);
FString ExportObjectPropertyValue(UObject* Object,const FName& Name,bool& Found);
bool ReflectedInt(UObject* O,FName Name,int32& Out);
TSharedPtr<FJsonObject> PCGPointSampleToJson(const UPCGBasePointData* D,int32 Index);
void AccumulatePCGTaggedDataPointTotals(const FTaggedSnapshot& T,int32& Count,int32& Total);
TSharedPtr<FJsonObject> PCGTaggedDataToJson(const FTaggedSnapshot& T,int32 MaxPoints,bool Samples,int32& IncludedDatas,int32& IncludedPoints,int32& SampledPoints);
TSharedPtr<FJsonObject> PCGDataCollectionToJson(const TArray<FTaggedSnapshot>& Tagged,int32 MaxTaggedData,int32 MaxPoints,bool Samples);
int32 GetPCGManagedResourceCount(UPCGComponent* C);
TSharedPtr<FJsonObject> PCGComponentSummaryToJson(UPCGComponent* C);
void AddHlslCountCompatibility(TSharedPtr<FJsonObject> O,UPCGSettings* S);
TArray<FTaggedSnapshot> SnapshotOutput(UPCGComponent* C);


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
FString ExportObjectPropertyValue(UObject* Object,const FName& Name,bool& Found)
{
    Found=false;if(!Object||!ContinueRead())return FString();FProperty* P=FindFProperty<FProperty>(Object->GetClass(),Name);if(!P)return FString();
    if(!Current->SDK(TEXT("property_ExportTextItem")))return FString();FString Value;P->ExportTextItem_Direct(Value,P->ContainerPtrToValuePtr<void>(Object),nullptr,Object,PPF_None);Found=true;
    if(!Current->After(TEXT("property_ExportTextItem"))||!Current->CaptureBudget(0,Value.Len())){Found=false;return FString();}return Value;
}
bool ReflectedInt(UObject* O,FName Name,int32& Out)
{if(!O||!ContinueRead())return false;const FIntProperty* P=FindFProperty<FIntProperty>(O->GetClass(),Name);if(!P)return false;Out=P->GetPropertyValue_InContainer(O);return true;}


void AddHlslCountCompatibility(TSharedPtr<FJsonObject> O,UPCGSettings* S)
{
    if(!S||!ContinueRead())return;bool Found=false;const FString Count=ExportObjectPropertyValue(S,TEXT("NumElements"),Found);
    if(Found){O->SetStringField(TEXT("point_count"),Count);O->SetStringField(TEXT("num_elements"),Count);}
    O->SetBoolField(TEXT("point_count_current_authoring_available"),Found);
    int32 X=0,Y=0,Z=0;const bool HasX=ReflectedInt(S,TEXT("NumElementsX"),X),HasY=ReflectedInt(S,TEXT("NumElementsY"),Y),HasZ=ReflectedInt(S,TEXT("NumElementsZ"),Z);
    if(HasX&&HasY)O->SetStringField(TEXT("num_elements_2d"),FString::Printf(TEXT("(X=%d,Y=%d)"),X,Y));
    if(HasX)O->SetNumberField(TEXT("num_elements_x"),X);if(HasY)O->SetNumberField(TEXT("num_elements_y"),Y);if(HasZ)O->SetNumberField(TEXT("num_elements_z"),Z);
    AddExportedPropertyIfPresent(O,S,TEXT("PointCount_DEPRECATED"),TEXT("point_count_deprecated_export"));
    AddExportedPropertyIfPresent(O,S,TEXT("NumElements2D_DEPRECATED"),TEXT("num_elements_2d_deprecated_export"));
    O->SetStringField(TEXT("count_projection_scope"),TEXT("UE58 authoring NumElements and XY settings only; not generated/effective point counts. PostLoad can migrate and zero legacy deprecated properties."));
    O->SetBoolField(TEXT("legacy_2d_projection_covers_z"),HasZ&&Z==1);
}
TArray<FTaggedSnapshot> SnapshotOutput(UPCGComponent* C)
{if(!Current->Component(C))return {};for(const auto& E:Current->Components){if(E.Component.Get()==C)return E.Output;}return {};}

UWorld* GetEditorWorldForPCGCommands()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

bool JsonBoolOrDefault(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, bool bDefault)
{
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return bDefault;
    }

    return Params->GetBoolField(FieldName);
}

bool ComponentHasTagString(const UActorComponent* Component, const FString& Tag)
{
    return Component && !Tag.IsEmpty() && Component->ComponentTags.Contains(FName(*Tag));
}

TArray<AActor*> FindActorsForPCGCommands(
    UWorld* World,
    const FString& ActorLabel,
    const FString& ActorName,
    const FString& ActorPath,
    const FString& ActorTag,
    const FString& ActorLabelPrefix,
    bool bSelectedOnly)
{
    TArray<AActor*> Actors;
    if (!World)
    {
        return Actors;
    }

    auto AddIfMatches = [&](AActor* Actor)
    {
        if(Actors.Num()>=100000){Current->Fail(TEXT("pcg_inspection_actor_bound"),TEXT("Actor observation exceeds100000."));return;}
        if (IsValid(Actor) && Current->Pin(Actor) && ActorMatchesPCGCommandFilters(Actor, ActorLabel, ActorName, ActorPath, ActorTag, ActorLabelPrefix))
        {
            Actors.AddUnique(Actor);
        }
    };

    if (bSelectedOnly)
    {
        USelection* SelectedActors = GEditor ? GEditor->GetSelectedActors() : nullptr;
        if (SelectedActors)
        {
            for (FSelectionIterator It(*SelectedActors); It; ++It)
            {
                if(!ContinueRead())break;
                AddIfMatches(Cast<AActor>(*It));
            }
        }
        return Actors;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if(!ContinueRead())break;
        AddIfMatches(*It);
    }

    return Actors;
}

TSharedPtr<FJsonObject> ActorSummaryToJson(const AActor* Actor)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    if(!Current->OutputBudget())return Object;
    if (!Actor)
    {
        return Object;
    }

    if(!Current->CaptureBudget(0,Actor->GetPathName().Len()+GetActorLabelForPCGCommands(Actor).Len()+Actor->GetName().Len()))return Object;
    Object->SetStringField(TEXT("label"), GetActorLabelForPCGCommands(Actor));
    Object->SetStringField(TEXT("name"), Actor->GetName());
    Object->SetStringField(TEXT("path"), Actor->GetPathName());

    TArray<TSharedPtr<FJsonValue>> Tags;
    for (const FName& Tag : Actor->Tags)
    {
        if(!Current->OutputBudget(Tag.ToString().Len()))break;
        Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    Object->SetArrayField(TEXT("tags"), Tags);
    return Object;
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

UPCGNode* FindPCGNodeById(UPCGGraph* Graph, const FString& NodeId)
{
    if (!Graph)
    {
        return nullptr;
    }

    auto MatchesNode = [&NodeId](UPCGNode* Node)
    {
        if (!Node)
        {
            return false;
        }

        const FString AuthoredTitle = Node->GetAuthoredTitleName().ToString();
        return Node->GetName().Equals(NodeId, ESearchCase::IgnoreCase) ||
            Node->GetPathName().Equals(NodeId, ESearchCase::IgnoreCase) ||
            AuthoredTitle.Equals(NodeId, ESearchCase::IgnoreCase);
    };

    if (MatchesNode(Graph->GetInputNode()))
    {
        return Graph->GetInputNode();
    }
    if (MatchesNode(Graph->GetOutputNode()))
    {
        return Graph->GetOutputNode();
    }

    for (UPCGNode* Node : GraphNodes(Graph,false))
    {
        if (MatchesNode(Node))
        {
            return Node;
        }
    }

    return nullptr;
}


TSharedPtr<FJsonObject> PCGNodeToJson(UPCGNode* N,bool Pins)
{
    auto O=MakeShared<FJsonObject>();if(!Current->OutputBudget()||!N||!ContinueRead()||!Current->Node(N))return O;
    if(!Current->CaptureBudget(0,N->GetPathName().Len()+N->GetName().Len()))return O;
    int32 X=0,Y=0;N->GetNodePosition(X,Y);UPCGSettings* S=SettingsFor(N);if(!ContinueRead())return O;const FString Title=NodeTitle(N);if(!ContinueRead())return O;
    O->SetStringField(TEXT("node_id"),N->GetName());O->SetStringField(TEXT("object_path"),N->GetPathName());O->SetStringField(TEXT("title"),Title);O->SetStringField(TEXT("authored_title"),N->GetAuthoredTitleName().ToString());O->SetStringField(TEXT("settings_class"),S?S->GetClass()->GetName():FString());O->SetStringField(TEXT("settings_path"),S?S->GetPathName():FString());O->SetNumberField(TEXT("x"),X);O->SetNumberField(TEXT("y"),Y);
    if(Pins){TArray<TSharedPtr<FJsonValue>> In,Out;const auto Input=NodePins(N,false),Output=NodePins(N,true);for(const auto* P:Input){if(!ContinueRead())break;In.Add(MakeShared<FJsonValueObject>(PCGPinToJson(P)));}if(ContinueRead()){for(const auto* P:Output){if(!ContinueRead())break;Out.Add(MakeShared<FJsonValueObject>(PCGPinToJson(P)));}}O->SetArrayField(TEXT("input_pins"),In);O->SetArrayField(TEXT("output_pins"),Out);}return O;
}


bool IsPCGCustomHLSLSettings(const UPCGSettings* Settings)
{
    return Settings &&
        Settings->GetClass() &&
        Settings->GetClass()->GetName().Equals(TEXT("PCGCustomHLSLSettings"), ESearchCase::IgnoreCase);
}

TSharedPtr<FJsonObject> PCGCustomHLSLNodeToJson(
    UPCGNode* Node,
    bool bIncludePins,
    bool bIncludeDeclarations,
    int32 MaxSourceChars)
{
    TSharedPtr<FJsonObject> NodeObject = PCGNodeToJson(Node, bIncludePins);
    UPCGSettings* Settings = Node ? SettingsFor(Node) : nullptr;
    if(!ContinueRead())return NodeObject;
    NodeObject->SetBoolField(TEXT("is_custom_hlsl"), IsPCGCustomHLSLSettings(Settings));
    NodeObject->SetStringField(TEXT("source_readback_method"), TEXT("reflected_pcg_custom_hlsl_settings_properties"));

    AddExportedPropertyIfPresent(NodeObject, Settings, TEXT("KernelType"), TEXT("kernel_type"));
    AddHlslCountCompatibility(NodeObject,Settings);
    AddExportedPropertyIfPresent(NodeObject, Settings, TEXT("DispatchThreadCount"), TEXT("dispatch_thread_count"));
    AddExportedPropertyIfPresent(NodeObject, Settings, TEXT("ThreadCountMultiplier"), TEXT("thread_count_multiplier"));
    AddExportedPropertyIfPresent(NodeObject, Settings, TEXT("FixedThreadCount"), TEXT("fixed_thread_count"));
    AddExportedPropertyIfPresent(NodeObject, Settings, TEXT("ThreadCountInputPinLabels"), TEXT("thread_count_input_pin_labels"));
    AddExportedPropertyIfPresent(NodeObject, Settings, TEXT("bMuteUnwrittenPinDataErrors"), TEXT("mute_unwritten_pin_data_errors"));

    AddReflectedStringPropertyText(NodeObject, Settings, TEXT("ShaderSource"), TEXT("shader_source"), MaxSourceChars);
    AddReflectedStringPropertyText(NodeObject, Settings, TEXT("ShaderFunctions"), TEXT("shader_functions"), MaxSourceChars);

    if (bIncludeDeclarations)
    {
        bool bInputDeclarationsFound = false;
        bool bOutputDeclarationsFound = false;
        bool bHelperDeclarationsFound = false;
        const FString InputDeclarations = GetReflectedStringPropertyValue(Settings, TEXT("InputDeclarations"), bInputDeclarationsFound);
        const FString OutputDeclarations = GetReflectedStringPropertyValue(Settings, TEXT("OutputDeclarations"), bOutputDeclarationsFound);
        const FString HelperDeclarations = GetReflectedStringPropertyValue(Settings, TEXT("HelperDeclarations"), bHelperDeclarationsFound);

        NodeObject->SetBoolField(TEXT("input_declarations_available"), bInputDeclarationsFound);
        NodeObject->SetBoolField(TEXT("output_declarations_available"), bOutputDeclarationsFound);
        NodeObject->SetBoolField(TEXT("helper_declarations_available"), bHelperDeclarationsFound);

        if (bInputDeclarationsFound)
        {
            AddTruncatedTextField(NodeObject, TEXT("input_declarations"), InputDeclarations, MaxSourceChars);
        }
        if (bOutputDeclarationsFound)
        {
            AddTruncatedTextField(NodeObject, TEXT("output_declarations"), OutputDeclarations, MaxSourceChars);
        }
        if (bHelperDeclarationsFound)
        {
            AddTruncatedTextField(NodeObject, TEXT("helper_declarations"), HelperDeclarations, MaxSourceChars);
        }

        const FString Declarations = InputDeclarations + TEXT("\n\n") + OutputDeclarations + TEXT("\n\n") + HelperDeclarations;
        AddTruncatedTextField(NodeObject, TEXT("declarations"), Declarations, MaxSourceChars);
    }

    return NodeObject;
}

bool MatchesNodeFilter(UPCGNode* Node, const FString& NodeType, const FString& TitleContains)
{
    if (!Node)
    {
        return false;
    }

    UPCGSettings* Settings = SettingsFor(Node);
    if(!ContinueRead())return false;
    const FString SettingsClassName = Settings ? Settings->GetClass()->GetName() : FString();
    const FString Title = NodeTitle(Node);

    if(!ContinueRead())return false;
    if (!NodeType.IsEmpty() && !SettingsClassName.Contains(NodeType) && !Node->GetName().Contains(NodeType))
    {
        return false;
    }

    if (!TitleContains.IsEmpty() && !Title.Contains(TitleContains))
    {
        return false;
    }

    return true;
}

TSharedPtr<FJsonObject> BoxToJsonObject(const FBox& Box)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    const bool bIsValid = Box.IsValid != 0;
    Object->SetBoolField(TEXT("valid"), bIsValid);
    if (bIsValid)
    {
        Object->SetArrayField(TEXT("min"), VectorToJsonArray(Box.Min));
        Object->SetArrayField(TEXT("max"), VectorToJsonArray(Box.Max));
        Object->SetArrayField(TEXT("center"), VectorToJsonArray(Box.GetCenter()));
        Object->SetArrayField(TEXT("extent"), VectorToJsonArray(Box.GetExtent()));
    }
    return Object;
}

FString GetActorLabelForPCGCommands(const AActor* Actor)
{
    if (!Actor)
    {
        return FString();
    }

#if WITH_EDITOR
    return Actor->GetActorLabel();
#else
    return Actor->GetName();
#endif
}

bool ActorMatchesPCGCommandFilters(
    const AActor* Actor,
    const FString& ActorLabel,
    const FString& ActorName,
    const FString& ActorPath,
    const FString& ActorTag,
    const FString& ActorLabelPrefix)
{
    if (!Actor)
    {
        return false;
    }

    const FString Label = GetActorLabelForPCGCommands(Actor);
    const FString Name = Actor->GetName();
    const FString Path = Actor->GetPathName();

    if (!ActorLabel.IsEmpty() && !Label.Equals(ActorLabel, ESearchCase::IgnoreCase))
    {
        return false;
    }
    if (!ActorName.IsEmpty() && !Name.Equals(ActorName, ESearchCase::IgnoreCase))
    {
        return false;
    }
    if (!ActorPath.IsEmpty() && !Path.Equals(ActorPath, ESearchCase::IgnoreCase))
    {
        return false;
    }
    if (!ActorTag.IsEmpty() && !ActorHasTagString(Actor, ActorTag))
    {
        return false;
    }
    if (!ActorLabelPrefix.IsEmpty() && !Label.StartsWith(ActorLabelPrefix, ESearchCase::IgnoreCase))
    {
        return false;
    }

    return true;
}

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

FString GetReflectedStringPropertyValue(const UObject* Object, const FName& PropertyName, bool& bOutFound)
{
    bOutFound = false;
    if (!Object || !ContinueRead())
    {
        return FString();
    }

    const FStrProperty* StringProperty = FindFProperty<FStrProperty>(Object->GetClass(), PropertyName);
    if (!StringProperty)
    {
        return FString();
    }

    bOutFound = true;
    return StringProperty->GetPropertyValue_InContainer(Object);
}

void AddExportedPropertyIfPresent(
    TSharedPtr<FJsonObject> Object,
    UObject* SourceObject,
    const FName& PropertyName,
    const FString& JsonFieldName)
{
    bool bFound = false;
    const FString ExportedValue = ExportObjectPropertyValue(SourceObject, PropertyName, bFound);
    if (bFound)
    {
        Object->SetStringField(JsonFieldName, ExportedValue);
    }
}

void AddTruncatedTextField(
    TSharedPtr<FJsonObject> Object,
    const FString& FieldName,
    const FString& Text,
    int32 MaxChars)
{
    const bool bTruncated = MaxChars > 0 && Text.Len() > MaxChars;
    if(!Current->CaptureBudget(0,bTruncated?MaxChars:Text.Len()))return;
    Object->SetStringField(FieldName, bTruncated ? Text.Left(MaxChars) : Text);
    Object->SetNumberField(FieldName + TEXT("_length"), Text.Len());
    Object->SetBoolField(FieldName + TEXT("_truncated"), bTruncated);
}

void AddReflectedStringPropertyText(
    TSharedPtr<FJsonObject> Object,
    const UObject* SourceObject,
    const FName& PropertyName,
    const FString& JsonFieldName,
    int32 MaxChars)
{
    bool bFound = false;
    const FString Text = GetReflectedStringPropertyValue(SourceObject, PropertyName, bFound);
    Object->SetBoolField(JsonFieldName + TEXT("_available"), bFound);
    if (bFound)
    {
        AddTruncatedTextField(Object, JsonFieldName, Text, MaxChars);
    }
}

TArray<TSharedPtr<FJsonValue>> VectorToJsonArray(const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    Array.Add(MakeShared<FJsonValueNumber>(Value.X));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
    return Array;
}

bool ActorHasTagString(const AActor* Actor, const FString& Tag)
{
    return Actor && !Tag.IsEmpty() && Actor->Tags.Contains(FName(*Tag));
}

FString PCGDataTypeToString(EPCGDataType DataType)
{
    if (const UEnum* DataTypeEnum = StaticEnum<EPCGDataType>())
    {
        return DataTypeEnum->GetNameStringByValue(static_cast<int64>(DataType));
    }

    return FString::Printf(TEXT("%llu"), static_cast<uint64>(DataType));
}

TArray<TSharedPtr<FJsonValue>> StringSetToJsonArray(const TSet<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    for (const FString& Value : Values)
    {
        if(!Current->OutputBudget(Value.Len()))break;
        Array.Add(MakeShared<FJsonValueString>(Value));
    }
    return Array;
}

TArray<TSharedPtr<FJsonValue>> Vector4ToJsonArray(const FVector4& Value)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    Array.Add(MakeShared<FJsonValueNumber>(Value.X));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Y));
    Array.Add(MakeShared<FJsonValueNumber>(Value.Z));
    Array.Add(MakeShared<FJsonValueNumber>(Value.W));
    return Array;
}

TSharedPtr<FJsonObject> TransformToJsonObject(const FTransform& Transform)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetArrayField(TEXT("location"), VectorToJsonArray(Transform.GetLocation()));
    Object->SetObjectField(TEXT("rotation"), RotatorToJsonObject(Transform.Rotator()));
    Object->SetArrayField(TEXT("scale"), VectorToJsonArray(Transform.GetScale3D()));
    return Object;
}

TSharedPtr<FJsonObject> RotatorToJsonObject(const FRotator& Rotator)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("pitch"), Rotator.Pitch);
    Object->SetNumberField(TEXT("yaw"), Rotator.Yaw);
    Object->SetNumberField(TEXT("roll"), Rotator.Roll);
    return Object;
}

TSharedPtr<FJsonObject> PCGPointSampleToJson(const UPCGBasePointData* D,int32 Index)
{
    auto O=MakeShared<FJsonObject>();if(!Current->OutputBudget()||!D)return O;int32 Count=0;if(!Current->PointCount(D,Count)||Index<0||Index>=Count)return O;
    FTransform Transform;FVector Min,Max;FVector4 Color;float Density=0,Steepness=0;int32 Seed=0;int64 Metadata=0;
    if(!Current->Read(TEXT("point_GetTransform"),[&]{return D->GetTransform(Index);},Transform)||!Current->VerifyPointCount(D))return O;
    if(!Current->Read(TEXT("point_GetBoundsMin"),[&]{return D->GetBoundsMin(Index);},Min)||!Current->VerifyPointCount(D))return O;
    if(!Current->Read(TEXT("point_GetBoundsMax"),[&]{return D->GetBoundsMax(Index);},Max)||!Current->VerifyPointCount(D))return O;
    if(!Current->Read(TEXT("point_GetColor"),[&]{return D->GetColor(Index);},Color)||!Current->VerifyPointCount(D))return O;
    if(!Current->Read(TEXT("point_GetDensity"),[&]{return D->GetDensity(Index);},Density)||!Current->VerifyPointCount(D))return O;
    if(!Current->Read(TEXT("point_GetSteepness"),[&]{return D->GetSteepness(Index);},Steepness)||!Current->VerifyPointCount(D))return O;
    if(!Current->Read(TEXT("point_GetSeed"),[&]{return D->GetSeed(Index);},Seed)||!Current->VerifyPointCount(D))return O;
    if(!Current->Read(TEXT("point_GetMetadataEntry"),[&]{return D->GetMetadataEntry(Index);},Metadata)||!Current->VerifyPointCount(D))return O;
    if(!Transform.IsValid()||Min.ContainsNaN()||Max.ContainsNaN()||Color.ContainsNaN()||!FMath::IsFinite(Density)||!FMath::IsFinite(Steepness)){Current->Fail(TEXT("pcg_inspection_nonfinite_sample"),TEXT("Point getter returned nonfinite sample; prior observations retained."));return O;}
    O->SetNumberField(TEXT("index"),Index);O->SetObjectField(TEXT("transform"),TransformToJsonObject(Transform));O->SetArrayField(TEXT("bounds_min"),VectorToJsonArray(Min));O->SetArrayField(TEXT("bounds_max"),VectorToJsonArray(Max));O->SetArrayField(TEXT("color"),Vector4ToJsonArray(Color));O->SetNumberField(TEXT("density"),Density);O->SetNumberField(TEXT("steepness"),Steepness);O->SetNumberField(TEXT("seed"),Seed);O->SetStringField(TEXT("metadata_entry"),LexToString(Metadata));return O;
}
void AccumulatePCGTaggedDataPointTotals(const FTaggedSnapshot& T,int32& Count,int32& Total)
{const auto* D=Cast<UPCGBasePointData>(T.Data);if(!D||!ContinueRead())return;int32 N=0;if(!Current->PointCount(D,N))return;Current->AddCount(Count,1,TEXT("point_data_count"));Current->AddCount(Total,N,TEXT("total_point_count"));}
TSharedPtr<FJsonObject> PCGTaggedDataToJson(const FTaggedSnapshot& T,int32 MaxPoints,bool Samples,int32& IncludedDatas,int32& IncludedPoints,int32& SampledPoints)
{
    auto O=MakeShared<FJsonObject>();if(!Current->OutputBudget())return O;const UPCGData* D=T.Data;O->SetStringField(TEXT("pin"),T.Pin.ToString());O->SetBoolField(TEXT("pinless_data"),T.bPinlessData);O->SetBoolField(TEXT("is_used_multiple_times"),T.bIsUsedMultipleTimes);O->SetNumberField(TEXT("original_index"),T.OriginalIndex);O->SetArrayField(TEXT("tags"),StringSetToJsonArray(T.Tags));O->SetBoolField(TEXT("has_data"),D!=nullptr);if(!D||!ContinueRead())return O;
    O->SetStringField(TEXT("data_class"),D->GetClass()->GetName());O->SetStringField(TEXT("data_path"),D->GetPathName());
    EPCGDataType Type=EPCGDataType::None,Underlying=EPCGDataType::None;bool Serializable=false,Cacheable=false,Transient=false;
    if(!Current->Read(TEXT("data_GetDataType"),[&]{return D->GetDataType();},Type))return O;O->SetStringField(TEXT("data_type"),PCGDataTypeToString(Type));
    if(!Current->Read(TEXT("data_GetUnderlyingDataType"),[&]{return D->GetUnderlyingDataType();},Underlying))return O;O->SetStringField(TEXT("underlying_data_type"),PCGDataTypeToString(Underlying));
    if(!Current->Read(TEXT("data_CanBeSerialized"),[&]{return D->CanBeSerialized();},Serializable))return O;O->SetBoolField(TEXT("can_be_serialized"),Serializable);
    if(!Current->Read(TEXT("data_IsCacheable"),[&]{return D->IsCacheable();},Cacheable))return O;O->SetBoolField(TEXT("is_cacheable"),Cacheable);
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const bool TransientRead=Current->Read(TEXT("data_HoldsTransientResources_legacy"),[&]{return D->HoldsTransientResources();},Transient);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    if(!TransientRead)return O;O->SetBoolField(TEXT("holds_transient_resources"),Transient);O->SetStringField(TEXT("holds_transient_resources_scope"),TEXT("deprecated_legacy_getter_not_GPU_resource_completion"));
    const auto* Points=Cast<UPCGBasePointData>(D);O->SetBoolField(TEXT("is_point_data"),Points!=nullptr);if(!Points)return O;int32 N=0;if(!Current->PointCount(Points,N))return O;const int32 SampleCount=Samples?FMath::Min(N,MaxPoints):0;
    if(!Current->AddCount(IncludedDatas,1,TEXT("included_point_data_count"))||!Current->AddCount(IncludedPoints,N,TEXT("included_point_count"))||!Current->AddCount(SampledPoints,SampleCount,TEXT("sampled_point_count")))return O;
    O->SetNumberField(TEXT("point_count"),N);O->SetNumberField(TEXT("point_sample_count"),SampleCount);O->SetBoolField(TEXT("point_sample_limit_hit"),SampleCount<N);
    if(Samples){TArray<TSharedPtr<FJsonValue>> Values;for(int32 I=0;I<SampleCount;++I){auto V=PCGPointSampleToJson(Points,I);if(!ContinueRead())break;Values.Add(MakeShared<FJsonValueObject>(V));O->SetArrayField(TEXT("points"),Values);}O->SetArrayField(TEXT("points"),Values);}return O;
}
TSharedPtr<FJsonObject> PCGDataCollectionToJson(const TArray<FTaggedSnapshot>& Tagged,int32 MaxTaggedData,int32 MaxPoints,bool Samples)
{
    auto O=MakeShared<FJsonObject>();Current->R.Data->SetObjectField(TEXT("partial_generated_output"),O);
    const int32 Count=Tagged.Num(),Included=FMath::Min(Count,MaxTaggedData);int32 PointDatas=0,Points=0,IncludedDatas=0,IncludedPoints=0,Sampled=0;
    O->SetNumberField(TEXT("tagged_data_count"),Count);O->SetNumberField(TEXT("included_tagged_data_count"),Included);O->SetBoolField(TEXT("tagged_data_limit_hit"),Included<Count);
    // Full totals precede prefix/sample serialization, including entries outside MaxTaggedData.
    for(const auto& T:Tagged){AccumulatePCGTaggedDataPointTotals(T,PointDatas,Points);O->SetNumberField(TEXT("point_data_count"),PointDatas);O->SetNumberField(TEXT("total_point_count"),Points);if(!ContinueRead())return O;}
    TArray<TSharedPtr<FJsonValue>> Values;for(int32 I=0;I<Included;++I){auto V=PCGTaggedDataToJson(Tagged[I],MaxPoints,Samples,IncludedDatas,IncludedPoints,Sampled);V->SetNumberField(TEXT("tagged_data_index"),I);Values.Add(MakeShared<FJsonValueObject>(V));O->SetArrayField(TEXT("tagged_data"),Values);if(!ContinueRead())break;}
    O->SetNumberField(TEXT("included_point_data_count"),IncludedDatas);O->SetNumberField(TEXT("included_point_count"),IncludedPoints);O->SetNumberField(TEXT("sampled_point_count"),Sampled);O->SetBoolField(TEXT("point_samples_included"),Samples);O->SetNumberField(TEXT("max_points_per_data"),MaxPoints);O->SetArrayField(TEXT("tagged_data"),Values);return O;
}
int32 GetPCGManagedResourceCount(UPCGComponent* C)
{
    if(!C||!ContinueRead())return 0;int64 Count=0;if(C->AreManagedResourcesAccessible()){if(!Current->SDK(TEXT("component_ForEachConstManagedResource")))return 0;C->ForEachConstManagedResource([&](const UPCGManagedResource*){++Count;});if(!Current->After(TEXT("component_ForEachConstManagedResource")))return 0;}
    if(Count>MAX_int32){Current->Fail(TEXT("pcg_inspection_resource_count_overflow"),TEXT("Managed-resource count exceeds original int32 range."));return 0;}return static_cast<int32>(Count);
}
TSharedPtr<FJsonObject> PCGComponentSummaryToJson(UPCGComponent* C)
{
    auto O=MakeShared<FJsonObject>();if(!C||!Current->Component(C))return O;const int32 Count=GetPCGManagedResourceCount(C);if(!ContinueRead())return O;
    const UPCGGraph* G=nullptr;if(!Current->Read(TEXT("component_GetGraph"),[&]{return C->GetGraph();},G))return O;if(G&&!Current->Pin(G))return O;
    O->SetStringField(TEXT("name"),C->GetName());O->SetStringField(TEXT("path"),C->GetPathName());
    O->SetBoolField(TEXT("active"),C->IsActive());O->SetBoolField(TEXT("registered"),C->IsRegistered());O->SetBoolField(TEXT("generating"),C->IsGenerating());O->SetBoolField(TEXT("cleaning_up"),C->IsCleaningUp());O->SetBoolField(TEXT("managed_resources_accessible"),C->AreManagedResourcesAccessible());O->SetNumberField(TEXT("managed_resource_count"),Count);O->SetNumberField(TEXT("generated_output_data_count"),C->GetGeneratedGraphOutput().TaggedData.Num());O->SetStringField(TEXT("graph"),G?G->GetPathName():FString());
    FBox Bounds;if(!Current->Read(TEXT("component_GetLastGeneratedBounds"),[&]{return C->GetLastGeneratedBounds();},Bounds))return O;O->SetObjectField(TEXT("last_generated_bounds"),BoxToJsonObject(Bounds));
    O->SetBoolField(TEXT("refresh_in_progress"),C->IsRefreshInProgress());O->SetBoolField(TEXT("was_generated_this_session"),C->WasGeneratedThisSession());return O;
}

TSharedPtr<FJsonObject> HandleResolvePCGGraph(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString GraphPath;
    if (!Params->TryGetStringField(TEXT("graph_path"), GraphPath))
    {
        return W.Error(TEXT("Missing 'graph_path' parameter"));
    }

    TArray<TSharedPtr<FJsonValue>> CandidateArray;
    const TArray<FString> CandidatePaths = FindPCGGraphAssetPaths(GraphPath);
    if(!ContinueRead())return W.Error(W.R.Error);
    for (const FString& CandidatePath : CandidatePaths)
    {
        if(!W.OutputBudget(CandidatePath.Len()))break;
        CandidateArray.Add(MakeShared<FJsonValueString>(CandidatePath));
    }

    W.R.Data->SetArrayField(TEXT("partial_candidates"),CandidateArray);
    if(!ContinueRead())return W.Error(W.R.Error);
    UPCGGraph* Graph = FindPCGGraph(GraphPath);
    if(!ContinueRead())return W.Error(W.R.Error);
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("resolved"), Graph != nullptr);
    if (Graph)
    {
        ResultObj->SetStringField(TEXT("name"), Graph->GetName());
        ResultObj->SetStringField(TEXT("asset_path"), Graph->GetPathName());
    }
    ResultObj->SetArrayField(TEXT("candidates"), CandidateArray);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleListPCGGraphNodes(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString GraphPath;
    if (!Params->TryGetStringField(TEXT("graph_path"), GraphPath))
    {
        return W.Error(TEXT("Missing 'graph_path' parameter"));
    }

    FString NodeType;
    Params->TryGetStringField(TEXT("node_type"), NodeType);

    FString TitleContains;
    Params->TryGetStringField(TEXT("title_contains"), TitleContains);

    bool bIncludePins = true;
    if (Params->HasField(TEXT("include_pins")))
    {
        bIncludePins = Params->GetBoolField(TEXT("include_pins"));
    }

    if(!ContinueRead())return W.Error(W.R.Error);
    UPCGGraph* Graph = FindPCGGraph(GraphPath);
    if(!ContinueRead())return W.Error(W.R.Error);
    if (!Graph)
    {
        return W.Error(FString::Printf(TEXT("PCG graph not found: %s"), *GraphPath));
    }
    if(!W.Graph(Graph))return W.Error(W.R.Error);

    TArray<TSharedPtr<FJsonValue>> Nodes;
    auto AddNodeIfMatching = [&Nodes, bIncludePins, &NodeType, &TitleContains](UPCGNode* Node)
    {
        if(!ContinueRead())return;
        if (MatchesNodeFilter(Node, NodeType, TitleContains))
        {
            Nodes.Add(MakeShared<FJsonValueObject>(PCGNodeToJson(Node, bIncludePins)));
            Current->R.Data->SetArrayField(TEXT("partial_nodes"),Nodes);
        }
    };

    const TArray<UPCGNode*> OrderedNodes=GraphNodes(Graph,true);
    for(UPCGNode* Node:OrderedNodes){if(!ContinueRead())break;AddNodeIfMatching(Node);}
    if(!ContinueRead())return W.Error(W.R.Error);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("graph"), Graph->GetPathName());
    ResultObj->SetArrayField(TEXT("nodes"), Nodes);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleInspectPCGCustomHLSLNodes(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString GraphPath;
    if (!Params->TryGetStringField(TEXT("graph_path"), GraphPath))
    {
        return W.Error(TEXT("Missing 'graph_path' parameter"));
    }

    FString NodeId;
    Params->TryGetStringField(TEXT("node_id"), NodeId);

    FString TitleContains;
    Params->TryGetStringField(TEXT("title_contains"), TitleContains);

    const bool bIncludePins = JsonBoolOrDefault(Params, TEXT("include_pins"), true);
    const bool bIncludeDeclarations = JsonBoolOrDefault(Params, TEXT("include_declarations"), true);

    int32 MaxSourceChars = 4000;
    if (Params->HasField(TEXT("max_source_chars")))
    {
        MaxSourceChars = IntegerField(Params,TEXT("max_source_chars"));
    }
    MaxSourceChars = FMath::Clamp(MaxSourceChars, 0, 1000000);

    if(!ContinueRead())return W.Error(W.R.Error);
    UPCGGraph* Graph = FindPCGGraph(GraphPath);
    if(!ContinueRead())return W.Error(W.R.Error);
    if (!Graph)
    {
        return W.Error(FString::Printf(TEXT("PCG graph not found: %s"), *GraphPath));
    }
    if(!W.Graph(Graph))return W.Error(W.R.Error);

    TArray<TSharedPtr<FJsonValue>> Nodes;
    bool bRequestedNodeFound = NodeId.IsEmpty();
    bool bRequestedNodeIsCustomHLSL = false;

    auto AddNodeIfMatching = [&Nodes, &TitleContains, bIncludePins, bIncludeDeclarations, MaxSourceChars, &bRequestedNodeIsCustomHLSL](UPCGNode* Node)
    {
        if (!Node)
        {
            return;
        }

        if(!ContinueRead())return;
        UPCGSettings* Settings = SettingsFor(Node);
        if(!ContinueRead())return;
        if (!IsPCGCustomHLSLSettings(Settings))
        {
            return;
        }

        bRequestedNodeIsCustomHLSL = true;

        const FString Title = NodeTitle(Node);
        if(!ContinueRead())return;
        if (!TitleContains.IsEmpty() && !Title.Contains(TitleContains, ESearchCase::IgnoreCase))
        {
            return;
        }

        Nodes.Add(MakeShared<FJsonValueObject>(PCGCustomHLSLNodeToJson(Node, bIncludePins, bIncludeDeclarations, MaxSourceChars)));
        Current->R.Data->SetArrayField(TEXT("partial_nodes"),Nodes);
    };

    if (!NodeId.IsEmpty())
    {
        if (UPCGNode* RequestedNode = FindPCGNodeById(Graph, NodeId))
        {
            bRequestedNodeFound = true;
            AddNodeIfMatching(RequestedNode);
        }
        else
        {
            return W.Error(FString::Printf(TEXT("PCG node not found in graph %s: %s"), *GraphPath, *NodeId));
        }
    }
    else
    {
        for (UPCGNode* Node : GraphNodes(Graph,false))
        {
            if(!ContinueRead())break;
            AddNodeIfMatching(Node);
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    if(!ContinueRead())return W.Error(W.R.Error);
    ResultObj->SetStringField(TEXT("graph"), Graph->GetPathName());
    ResultObj->SetStringField(TEXT("node_id"), NodeId);
    ResultObj->SetStringField(TEXT("title_contains"), TitleContains);
    ResultObj->SetBoolField(TEXT("requested_node_found"), bRequestedNodeFound);
    if (!NodeId.IsEmpty())
    {
        ResultObj->SetBoolField(TEXT("requested_node_is_custom_hlsl"), bRequestedNodeIsCustomHLSL);
    }
    ResultObj->SetBoolField(TEXT("include_pins"), bIncludePins);
    ResultObj->SetBoolField(TEXT("include_declarations"), bIncludeDeclarations);
    ResultObj->SetNumberField(TEXT("max_source_chars"), MaxSourceChars);
    ResultObj->SetNumberField(TEXT("node_count"), Nodes.Num());
    ResultObj->SetArrayField(TEXT("nodes"), Nodes);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleInspectPCGComponentOutputs(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GetEditorWorldForPCGCommands();
    if (!World || !W.CaptureWorld(World))
    {
        return W.Error(TEXT("Failed to get editor world"));
    }

    FString ActorLabel;
    FString ActorName;
    FString ActorPath;
    FString ActorTag;
    FString ActorLabelPrefix;
    FString ComponentName;
    FString ComponentTag;
    Params->TryGetStringField(TEXT("actor_label"), ActorLabel);
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("actor_path"), ActorPath);
    Params->TryGetStringField(TEXT("actor_tag"), ActorTag);
    Params->TryGetStringField(TEXT("actor_label_prefix"), ActorLabelPrefix);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    Params->TryGetStringField(TEXT("component_tag"), ComponentTag);

    const bool bSelectedOnly = JsonBoolOrDefault(Params, TEXT("selected_only"), false);
    const bool bIncludePointSamples = JsonBoolOrDefault(Params, TEXT("include_point_samples"), true);

    int32 MaxComponents = 16;
    if (Params->HasField(TEXT("max_components")))
    {
        MaxComponents = IntegerField(Params,TEXT("max_components"));
    }
    MaxComponents = FMath::Clamp(MaxComponents, 1, 1000);

    int32 MaxTaggedData = 64;
    if (Params->HasField(TEXT("max_tagged_data")))
    {
        MaxTaggedData = IntegerField(Params,TEXT("max_tagged_data"));
    }
    MaxTaggedData = FMath::Clamp(MaxTaggedData, 0, 10000);

    int32 MaxPoints = 8;
    if (Params->HasField(TEXT("max_points")))
    {
        MaxPoints = IntegerField(Params,TEXT("max_points"));
    }
    MaxPoints = FMath::Clamp(MaxPoints, 0, 1000);

    if(!ContinueRead())return W.Error(W.R.Error);
    const TArray<AActor*> MatchingActors = FindActorsForPCGCommands(World, ActorLabel, ActorName, ActorPath, ActorTag, ActorLabelPrefix, bSelectedOnly);
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    TArray<TSharedPtr<FJsonValue>> ComponentArray;

    int32 PCGActorCount = 0;
    int32 ComponentCount = 0;
    int32 TotalGeneratedOutputDataCount = 0;
    bool bComponentLimitHit = false;

    for (AActor* Actor : MatchingActors)
    {
        if(!ContinueRead())break;
        TArray<UPCGComponent*> PCGComponents;
        Actor->GetComponents<UPCGComponent>(PCGComponents);
        if(!W.CaptureActorComponents(Actor,PCGComponents))break;
        if (PCGComponents.Num() == 0)
        {
            continue;
        }

        bool bActorHasIncludedComponent = false;
        for (UPCGComponent* PCGComponent : PCGComponents)
        {
            if(!ContinueRead())break;
            if (!IsValid(PCGComponent))
            {
                continue;
            }
            if (!ComponentName.IsEmpty() && !PCGComponent->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
            {
                continue;
            }
            if (!ComponentTag.IsEmpty() && !ComponentHasTagString(PCGComponent, ComponentTag))
            {
                continue;
            }

            if (ComponentCount >= MaxComponents)
            {
                bComponentLimitHit = true;
                break;
            }

            const TArray<FTaggedSnapshot> GeneratedOutput = SnapshotOutput(PCGComponent);
            if(!ContinueRead()||!W.AddCount(TotalGeneratedOutputDataCount,GeneratedOutput.Num(),TEXT("total_generated_output_data_count")))break;

            TSharedPtr<FJsonObject> ComponentObject = PCGComponentSummaryToJson(PCGComponent);
            if(!ContinueRead()){W.R.Data->SetObjectField(TEXT("partial_component"),ComponentObject);break;}
            ComponentObject->SetObjectField(TEXT("actor"), ActorSummaryToJson(Actor));
            ComponentObject->SetObjectField(
                TEXT("generated_output"),
                PCGDataCollectionToJson(GeneratedOutput, MaxTaggedData, MaxPoints, bIncludePointSamples));
            ComponentArray.Add(MakeShared<FJsonValueObject>(ComponentObject));
            W.R.Data->SetArrayField(TEXT("partial_components"),ComponentArray);
            if(!ContinueRead())break;

            bActorHasIncludedComponent = true;
            ++ComponentCount;
        }

        if (bActorHasIncludedComponent && ContinueRead())
        {
            ActorArray.Add(MakeShared<FJsonValueObject>(ActorSummaryToJson(Actor)));
            ++PCGActorCount;
        }

        if (bComponentLimitHit)
        {
            break;
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetNumberField(TEXT("matched_actor_count"), MatchingActors.Num());
    ResultObj->SetNumberField(TEXT("pcg_actor_count"), PCGActorCount);
    ResultObj->SetNumberField(TEXT("component_count"), ComponentCount);
    ResultObj->SetNumberField(TEXT("total_generated_output_data_count"), TotalGeneratedOutputDataCount);
    ResultObj->SetBoolField(TEXT("component_limit_hit"), bComponentLimitHit);
    ResultObj->SetBoolField(TEXT("selected_only"), bSelectedOnly);
    ResultObj->SetBoolField(TEXT("include_point_samples"), bIncludePointSamples);
    ResultObj->SetNumberField(TEXT("max_components"), MaxComponents);
    ResultObj->SetNumberField(TEXT("max_tagged_data"), MaxTaggedData);
    ResultObj->SetNumberField(TEXT("max_points"), MaxPoints);
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    ResultObj->SetArrayField(TEXT("components"), ComponentArray);
    return ResultObj;
}

FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
    bool LegacySuccess=true;FString LegacyError;if(Legacy){if(Legacy->HasField(TEXT("success")))Legacy->TryGetBoolField(TEXT("success"),LegacySuccess);Legacy->TryGetStringField(TEXT("error"),LegacyError);}else LegacySuccess=false;
    if((!LegacySuccess||!LegacyError.IsEmpty())&&W.R.Error.IsEmpty())W.Fail(TEXT("pcg_inspection_request_failed"),LegacyError.IsEmpty()?TEXT("Original PCG inspector returned failure; see legacy_result."):LegacyError);
    bool Complete=W.R.Error.IsEmpty()&&LegacySuccess;if(Complete)Complete=W.Admit(TEXT("final_observation"));
    W.R.bSuccess=Complete;W.R.bPostconditionPassed=Complete;
    // This read-only wrapper never promotes guarded SDK observations to global purity.
    W.R.bMutationAttempted=false;W.R.bCompileRequested=false;W.R.bCompileAttempted=false;W.R.Compiled=false;W.R.bSaveRequested=false;W.R.bSaveAttempted=false;W.R.bSaved=false;
    W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("postcondition_passed"),Complete);W.R.Data->SetBoolField(TEXT("observation_complete"),Complete);
    W.R.Data->SetStringField(TEXT("postcondition_scope"),TEXT("The dedicated requested snapshot completed with exact loaded object identity and captured graph/pin/output membership at guarded boundaries; empty/unresolved results can be normal. This is not graph construction, generation readiness, payload freshness or global purity."));
    W.R.Data->SetStringField(TEXT("mutation_scope"),TEXT("No explicit Modify, transaction, property write, Generate, Refresh, EnableInspection, compile or save. Load/PostLoad, virtual getter and reflection export cache/global effects remain unknown after any SDK entry."));
    W.R.Data->SetStringField(TEXT("payload_snapshot_scope"),TEXT("Plain pointers/tags/pin/flags/index snapshots with request-only const strong Data pins. No owning FPCGDataCollection or TaggedData wrapper copies. Each virtual point field is copied immediately and followed by identity/count checks; immutable payload atomicity is not guaranteed."));
    W.R.Data->SetStringField(TEXT("count_scope"),TEXT("Original full tagged-data point totals precede crop/sample prefix. Original int32 overflow and unsafe out-of-range input casts are rejected with partial observed evidence; included/sample totals are distinct."));
    W.R.Data->SetBoolField(TEXT("construction_gate_relaxed"),false);W.R.Data->SetBoolField(TEXT("generated_output_freshness_verified"),false);W.R.Data->SetBoolField(TEXT("GPU_readback_completion_verified"),false);W.R.Data->SetBoolField(TEXT("Cubeless_runtime_asset_dependency_allowed"),false);
    W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,FHandler Handler)
{
    FWork W;TSharedPtr<FJsonObject> Params;
    if(!ParseParams(Text,Params)){W.Fail(TEXT("pcg_inspection_invalid_params_json"),TEXT("Expected one finite JSON object within1Mi UTF16/depth64/262144 notation tokens and value visits, without duplicate case-sensitive keys or NUL."));return Finish(W,W.Error(W.R.Error),Operation);}
    if(Running){W.Fail(TEXT("pcg_inspection_reentrant_request"),TEXT("A dedicated PCG inspection request is active; no replay."));return Finish(W,W.Error(W.R.Error),Operation);}
    TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);return Finish(W,Handler(W,Params),Operation);
}
}
FString ResolveGraph(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("resolve_pcg_graph"),&HandleResolvePCGGraph);}
FString ListNodes(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("list_pcg_graph_nodes"),&HandleListPCGGraphNodes);}
FString InspectCustomHlsl(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_pcg_custom_hlsl_nodes"),&HandleInspectPCGCustomHLSLNodes);}
FString InspectComponentOutputs(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_pcg_component_outputs"),&HandleInspectPCGComponentOutputs);}
}
