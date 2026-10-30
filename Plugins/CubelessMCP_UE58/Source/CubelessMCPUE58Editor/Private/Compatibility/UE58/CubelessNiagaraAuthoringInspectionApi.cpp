// GH dedicated Editor-only Niagara authoring inspection; source candidate, not execution proof.
#include "Compatibility/UE58/CubelessNiagaraAuthoringInspectionApi.h"
#include "Compatibility/UE58/CubelessNiagaraScratchUserRecipeAsyncApi.h"
#include "CubelessNiagaraScratchUserRecipeJob.h"
#include "Containers/Ticker.h"
#include "AssetCompilingManager.h"
#include "Algo/Count.h"
#include "HAL/PlatformTime.h"
#include "UObject/SavePackage.h"
namespace Cubeless::UE58::NiagaraScratchUserRecipeAsync { bool CanPublishJobObject(); }

#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRenderTarget2D.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraActions.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::NiagaraAuthoringInspection
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O){FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));}return S;}
FString NormalizeNiagaraObjectPathForLoad(const FString&);
bool Forbidden(const FString& S){const FString V=S.ToLower();return V.Contains(TEXT("unrealmcp"))||V.Contains(TEXT("cubelessmcp"))||V.Contains(TEXT("modelcontextprotocol"))||V.Contains(TEXT("toolsetregistry"));}
FString NodeContent(const UEdGraphNode* N)
{auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("comment"),N->NodeComment);J->SetNumberField(TEXT("x"),N->NodePosX);J->SetNumberField(TEXT("y"),N->NodePosY);TArray<TSharedPtr<FJsonValue>> Ps;for(const UEdGraphPin* P:N->Pins){auto O=MakeShared<FJsonObject>();if(P){O->SetStringField(TEXT("name"),P->PinName.ToString());O->SetStringField(TEXT("friendly"),P->PinFriendlyName.ToString());O->SetStringField(TEXT("tooltip"),P->PinToolTip);O->SetStringField(TEXT("default"),P->DefaultValue);O->SetStringField(TEXT("default_object"),P->DefaultObject?P->DefaultObject->GetPathName():FString());O->SetStringField(TEXT("default_text"),P->DefaultTextValue.ToString());O->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());O->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());O->SetStringField(TEXT("subcategory_object"),P->PinType.PinSubCategoryObject.IsValid()?P->PinType.PinSubCategoryObject->GetPathName():FString());O->SetNumberField(TEXT("direction"),static_cast<int32>(P->Direction));}Ps.Add(MakeShared<FJsonValueObject>(O));}J->SetArrayField(TEXT("pins"),Ps);if(const auto* F=Cast<UNiagaraNodeFunctionCall>(N)){J->SetStringField(TEXT("function_script"),F->FunctionScript?F->FunctionScript->GetPathName():FString());J->SetStringField(TEXT("function_asset_path"),F->FunctionScriptAssetObjectPath.ToString());J->SetStringField(TEXT("signature"),F->Signature.GetNameString());}return Json(J);}
struct FObjectIdentity
{TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path;explicit FObjectIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()){}bool Exact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&Live(Package.Get())&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}};
struct FPinIdentity{UEdGraphPin* Pin=nullptr;FGuid Id;TArray<UEdGraphPin*> Links;};
struct FNodeIdentity{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;FString Content;TArray<FPinIdentity> Pins;};
struct FGraphIdentity{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNodeIdentity> Nodes;};

// HR: only owned action-preview nodes are admitted here; source graphs stay strict.
FString PreviewNodeContent(const UEdGraphNode* Node)
{
    FString Value=NodeContent(Node);
    for(const UEdGraphPin* Pin:Node->Pins)
    {if(Pin){Value+=FString::Printf(TEXT("|%s:%d:%d:%d:%d:%d"),*Pin->PersistentGuid.ToString(),Pin->bHidden,Pin->bNotConnectable,Pin->bAdvancedView,Pin->bDefaultValueIsIgnored,Pin->bDefaultValueIsReadOnly);}}
    return Value;
}
struct FPreviewIdentity
{
    TWeakObjectPtr<UNiagaraGraph> Graph;TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;FString Content;
    TArray<TWeakObjectPtr<UEdGraphNode>> Nodes;TArray<FPinIdentity> Pins;TArray<FGuid> Persistent;TArray<FEdGraphPinType> Types;TArray<TWeakObjectPtr<UObject>> DefaultObjects;
    TWeakObjectPtr<UNiagaraScript> FunctionScript;
};
struct FScriptIdentity{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;};
struct FEmitterIdentity{FGuid Id,InstanceVersion,DataVersion;FString Name;bool Enabled=false;uint8 Mode=0;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,Parent;TArray<TWeakObjectPtr<UNiagaraScript>> Scripts,ScratchScripts,ParentScripts;};
struct FWork
{
    // HN phase state. Original handler core executes once; only public compile observation resumes.
    bool DeferredMode=false,WaitDeferred=false,WaitRequested=false,WaitTimedOut=false,WaitIdle=true,HNCancelRequested=false;
    bool WaitRequestCompile=false,WaitCompileAccepted=false,WaitBefore=false,WaitAfterRequest=false;
    FString HNCancelReason;double WaitStarted=0.0,WaitTimeout=10.0,WaitPoll=0.1,WaitNext=0.0;int32 WaitIterations=0;
    TSharedPtr<FJsonObject> WaitObject;

    TArray<FPreviewIdentity> PreviewNodes;TArray<TUniquePtr<FObjectIdentity>> PreviewReferences;
    FResult R;bool Writer=false,WorldCaptured=false;FString FailedStage;TWeakObjectPtr<UEditorEngine> CapturedEditor;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<TWeakObjectPtr<UNiagaraScript>> SystemScratch;TArray<FEmitterIdentity> Emitters;TArray<FGraphIdentity> Graphs;TArray<FScriptIdentity> Scripts;TArray<TUniquePtr<FObjectIdentity>> Objects;TSharedPtr<FJsonObject> Before;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Transient=MakeShared<FJsonObject>(),Compile=MakeShared<FJsonObject>();
    explicit FWork(bool W):Writer(W){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("transient_work_prefix"),Transient);R.Data->SetObjectField(TEXT("actual_compile_prefix"),Compile);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("source_asset_mutation_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty()){Fail(TEXT("niagara_inspection_request_failed"),Message);}auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
    bool Pin(UObject* O){if(!Live(O)){return Fail(TEXT("niagara_inspection_object_unavailable"),TEXT("Captured Engine object is not live."));}for(const auto& I:Objects){if(I->Object.Get()==O){return I->Exact();}}Objects.Add(MakeUnique<FObjectIdentity>(O));return true;}
    bool Admit(const FString& Stage,bool FailureObservation=false)
    {
        R.Stage=Stage;if(HNCancelRequested)return Fail(TEXT("niagara_recipe_wait_cancelled"),HNCancelReason);if(!R.Error.IsEmpty()&&!FailureObservation){return false;}if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||!GEditor||IsGarbageCollecting()||UE::IsSavingPackage()){return Fail(TEXT("niagara_inspection_context_unavailable"),TEXT("Live Editor game thread without GC/exit/save required; no replay."));}if(Writer&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld)){return Fail(TEXT("niagara_inspection_play_session_active"),TEXT("Compile-status authoring requires stopped PIE/SIE."));}auto* Current=GEditor->GetEditorWorldContext().World();if(!Live(Current)||Current->bIsTearingDown){return Fail(TEXT("niagara_inspection_world_unavailable"),TEXT("Editor world unavailable or tearing down."));}if(!WorldCaptured){CapturedEditor=GEditor;World=Current;WorldCaptured=true;}else if(CapturedEditor.Get()!=GEditor){return Fail(TEXT("niagara_inspection_editor_changed"),TEXT("Captured Editor instance changed across SDK dispatch."));}else if(World.Get()!=Current){return Fail(TEXT("niagara_inspection_world_changed"),TEXT("Editor world changed across SDK dispatch."));}for(const auto& I:Objects){if(!I->Exact()){return Fail(TEXT("niagara_inspection_object_identity_changed"),TEXT("Captured class/outer/package/path changed; replacement was not adopted."));}}
        auto SameScripts=[](const auto* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& Expected){if(!C){return Expected.IsEmpty();}if(C->Scripts.Num()!=Expected.Num()){return false;}for(int32 I=0;I<Expected.Num();++I){if(C->Scripts[I]!=Expected[I].Get()){return false;}}return true;};
        if(System.IsValid())
        {auto* S=System.Get();if(S->GetSystemSpawnScript()!=Spawn.Get()||S->GetSystemUpdateScript()!=Update.Get()||S->ScratchPadScripts.Num()!=SystemScratch.Num()||S->GetEmitterHandles().Num()!=Emitters.Num()){return Fail(TEXT("niagara_inspection_system_routing_changed"),TEXT("System script/emitter membership changed."));}for(int32 I=0;I<SystemScratch.Num();++I){if(S->ScratchPadScripts[I]!=SystemScratch[I].Get()){return Fail(TEXT("niagara_inspection_system_routing_changed"),TEXT("System scratch script order changed."));}}for(int32 I=0;I<Emitters.Num();++I){const auto& H=S->GetEmitterHandles()[I];const auto& E=Emitters[I];if(H.GetId()!=E.Id||!H.GetName().ToString().Equals(E.Name,ESearchCase::CaseSensitive)||H.GetIsEnabled()!=E.Enabled||static_cast<uint8>(H.GetEmitterMode())!=E.Mode||H.GetInstance().Version!=E.InstanceVersion||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion)){return Fail(TEXT("niagara_inspection_emitter_version_changed"),TEXT("Physical emitter version/ref changed."));}if(E.Data){if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.Parent.Get()||!SameScripts(E.Scratch.Get(),E.ScratchScripts)||!SameScripts(E.Parent.Get(),E.ParentScripts)){return Fail(TEXT("niagara_inspection_emitter_routing_changed"),TEXT("Emitter source/scratch routing changed."));}TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);if(A.Num()!=E.Scripts.Num()){return Fail(TEXT("niagara_inspection_emitter_routing_changed"),TEXT("Emitter script count changed."));}for(int32 J=0;J<A.Num();++J){if(A[J]!=E.Scripts[J].Get()){return Fail(TEXT("niagara_inspection_emitter_routing_changed"),TEXT("Emitter script order changed."));}}}}}
        for(const auto& S:Scripts){if(!Live(S.Script.Get())||S.Script->GetLatestSource()!=S.Source.Get()||S.Script->GetExposedVersion().VersionGuid!=S.Version){return Fail(TEXT("niagara_inspection_script_source_changed"),TEXT("Script source/exposed version changed."));}}
        for(const auto& G:Graphs)
        {if(!Live(G.Source.Get())||G.Source->NodeGraph!=G.Graph.Get()||!Live(G.Graph.Get())||G.Graph->Nodes.Num()!=G.Nodes.Num()){return Fail(TEXT("niagara_inspection_graph_changed"),TEXT("Source graph/node membership changed."));}for(int32 I=0;I<G.Nodes.Num();++I){const auto& E=G.Nodes[I];auto* N=E.Node.Get();if(G.Graph->Nodes[I]!=N){return Fail(TEXT("niagara_inspection_node_changed"),TEXT("Physical node order changed."));}if(!N){continue;}if(!Live(N)||N->NodeGuid!=E.Id||N->Pins.Num()!=E.Pins.Num()){return Fail(TEXT("niagara_inspection_node_changed"),TEXT("Captured node/pin identity changed."));}for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& EP=E.Pins[J];if(P!=EP.Pin||(P&&(P->PinId!=EP.Id||P->GetOwningNodeUnchecked()!=N||P->LinkedTo!=EP.Links))){return Fail(TEXT("niagara_inspection_pin_changed"),TEXT("Exact pin identity/links changed; stale pointer not dereferenced."));}}if(!NodeContent(N).Equals(E.Content,ESearchCase::CaseSensitive)){return Fail(TEXT("niagara_inspection_node_content_changed"),TEXT("Source node content changed during observation."));}}}

        for(const auto& Reference:PreviewReferences){if(!Reference->Exact())return Fail(TEXT("niagara_graph_action_preview_reference_changed"),TEXT("Captured preview reference identity changed; no stale object read."));}
        for(const auto& Preview:PreviewNodes)
        {
            auto* PreviewGraph=Preview.Graph.Get();auto* PreviewNode=Preview.Node.Get();
            if(!Live(PreviewGraph)||!Live(PreviewNode)||PreviewNode->GetOuter()!=PreviewGraph||PreviewNode->GetGraph()!=PreviewGraph||PreviewGraph->Nodes.Num()!=Preview.Nodes.Num()||PreviewNode->NodeGuid!=Preview.Id||PreviewNode->Pins.Num()!=Preview.Pins.Num())return Fail(TEXT("niagara_graph_action_preview_identity_changed"),TEXT("Owned preview/template topology changed during callback."));
            for(int32 NodeSlot=0;NodeSlot<Preview.Nodes.Num();++NodeSlot)if(PreviewGraph->Nodes[NodeSlot].Get()!=Preview.Nodes[NodeSlot].Get())return Fail(TEXT("niagara_graph_action_preview_identity_changed"),TEXT("Preview graph physical node order changed."));
            for(int32 PinSlot=0;PinSlot<Preview.Pins.Num();++PinSlot)
            {auto* Pin=PreviewNode->Pins[PinSlot];const auto& Expected=Preview.Pins[PinSlot];if(Pin!=Expected.Pin||!Pin||Pin->GetOwningNodeUnchecked()!=PreviewNode||Pin->PinId!=Expected.Id||Pin->PersistentGuid!=Preview.Persistent[PinSlot]||Pin->PinType!=Preview.Types[PinSlot]||Pin->DefaultObject!=Preview.DefaultObjects[PinSlot].Get()||Pin->LinkedTo!=Expected.Links)return Fail(TEXT("niagara_graph_action_preview_pin_changed"),TEXT("Preview current pin membership/type/links changed; old raw pin was not read."));}
            if(const auto* PreviewCall=Cast<UNiagaraNodeFunctionCall>(PreviewNode)){if(PreviewCall->FunctionScript.Get()!=Preview.FunctionScript.Get())return Fail(TEXT("niagara_graph_action_preview_function_changed"),TEXT("Preview FunctionScript binding changed."));}
            if(!PreviewNodeContent(PreviewNode).Equals(Preview.Content,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_graph_action_preview_content_changed"),TEXT("Preview/template content changed during projection callback."));
        }
        return true;
    }

    bool PreviewReference(UObject* Object)
    {
        if(!Object)return true;if(!Live(Object))return Fail(TEXT("niagara_graph_action_preview_reference_unavailable"),TEXT("Preview references unavailable object."));
        for(const auto& Existing:PreviewReferences)if(Existing->Object.Get()==Object)return Existing->Exact();
        PreviewReferences.Add(MakeUnique<FObjectIdentity>(Object));return true;
    }
    bool CapturePreview(UNiagaraGraph* Graph,UEdGraphNode* Node)
    {
        if(!Admit(TEXT("capture_owned_action_preview"))||!Live(Graph)||!Live(Node)||Node->GetGraph()!=Graph||Node->GetOuter()!=Graph||Graph->GetOutermost()!=GetTransientPackage()||Graph->GetPackage()!=GetTransientPackage()||Node->GetPackage()!=GetTransientPackage()||Node->Pins.Num()>4096||Graph->Nodes.Num()>16384)return Fail(TEXT("niagara_graph_action_preview_scope_invalid"),TEXT("Exact bounded transient graph/template required."));
        if(!Pin(Graph)||!Pin(Node))return false;FPreviewIdentity Preview;Preview.Graph=Graph;Preview.Node=Node;Preview.Id=Node->NodeGuid;
        for(const auto& NodeEntry:Graph->Nodes)Preview.Nodes.Add(NodeEntry.Get());
        TSet<const UEdGraphPin*> Members;for(auto* Pin:Node->Pins){if(!Pin||Pin->GetOwningNodeUnchecked()!=Node||Members.Contains(Pin))return Fail(TEXT("niagara_graph_action_preview_pin_invalid"),TEXT("Preview contains null/duplicate/foreign physical pin."));Members.Add(Pin);}
        for(auto* Pin:Node->Pins)
        {
            if(!PreviewReference(Pin->DefaultObject)||!PreviewReference(Pin->PinType.PinSubCategoryObject.Get())||!PreviewReference(Pin->PinType.PinValueType.TerminalSubCategoryObject.Get()))return false;
            for(auto* Link:Pin->LinkedTo)if(!Members.Contains(Link)||Link->GetOwningNodeUnchecked()!=Node||Algo::Count(Link->LinkedTo, Pin)!=1)return Fail(TEXT("niagara_graph_action_preview_external_link"),TEXT("Owned preview/template pin has external or nonreciprocal link."));
            FPinIdentity Physical;Physical.Pin=Pin;Physical.Id=Pin->PinId;Physical.Links=Pin->LinkedTo;Preview.Pins.Add(MoveTemp(Physical));Preview.Persistent.Add(Pin->PersistentGuid);Preview.Types.Add(Pin->PinType);Preview.DefaultObjects.Add(Pin->DefaultObject);
        }
        if(const auto* PreviewCall=Cast<UNiagaraNodeFunctionCall>(Node)){Preview.FunctionScript=PreviewCall->FunctionScript.Get();if(!PreviewReference(PreviewCall->FunctionScript.Get()))return false;}
        Preview.Content=PreviewNodeContent(Node);if(Preview.Content.Len()>1024*1024)return Fail(TEXT("niagara_graph_action_preview_content_bound"),TEXT("One preview/template content exceeds1Mi UTF16."));PreviewNodes.Add(MoveTemp(Preview));return Admit(TEXT("captured_owned_action_preview"));
    }
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage)){return false;}R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);return true;}
    bool After(const TCHAR* Stage){return Admit(FString(TEXT("after_"))+Stage);}
    bool Graph(UNiagaraScriptSourceBase* Base)
    {auto* S=Cast<UNiagaraScriptSource>(Base);if(!S||!S->NodeGraph){return true;}auto* G=S->NodeGraph.Get();for(const auto& E:Graphs){if(E.Source.Get()==S&&E.Graph.Get()==G){return true;}}if(!Pin(S)||!Pin(G)||G->Nodes.Num()>16384){return Fail(TEXT("niagara_inspection_graph_bound"),TEXT("Graph inventory unavailable or exceeds bound."));}FGraphIdentity E;E.Source=S;E.Graph=G;int32 Slots=0;for(UEdGraphNode* N:G->Nodes){FNodeIdentity NE;NE.Node=N;if(!N){return Fail(TEXT("niagara_inspection_malformed_graph"),TEXT("Graph contains a null node; reference sort/getter would be unsafe. No source mutation attempted."));}if(N){if(!Pin(N)){return false;}NE.Id=N->NodeGuid;NE.Content=NodeContent(N);for(UEdGraphPin* P:N->Pins){if(++Slots>262144){return Fail(TEXT("niagara_inspection_pin_bound"),TEXT("Graph pin bound exceeded."));}FPinIdentity PE;PE.Pin=P;if(!P){return Fail(TEXT("niagara_inspection_malformed_graph"),TEXT("Graph contains a null pin; public Niagara input/output enumeration dereferences pin slots. No source mutation attempted."));}if(P){PE.Id=P->PinId;PE.Links=P->LinkedTo;}NE.Pins.Add(MoveTemp(PE));}}E.Nodes.Add(MoveTemp(NE));}Graphs.Add(MoveTemp(E));return true;}
    bool Script(UNiagaraScript* S)
    {if(!S){return true;}for(const auto& E:Scripts){if(E.Script.Get()==S){return true;}}if(!Pin(S)){return false;}FScriptIdentity E;E.Script=S;E.Source=S->GetLatestSource();E.Version=S->GetExposedVersion().VersionGuid;Scripts.Add(E);return Graph(E.Source.Get());}
    UNiagaraSystem* LoadSystem(const FString& Path)
    {if(Forbidden(Path)){Fail(TEXT("niagara_inspection_runtime_dependency_rejected"),TEXT("MCP/Cubeless/Toolset content reference forbidden."));return nullptr;}if(!SDK(TEXT("load_system"))){return nullptr;}const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* S=LoadObject<UNiagaraSystem>(nullptr,*Expected);R.Data->SetBoolField(TEXT("load_system_returned_nonnull"),S!=nullptr);if(!After(TEXT("load_system"))){return nullptr;}if(!S){return nullptr;}if(!S->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Pin(S)||!Pin(S->GetOutermost())){Fail(TEXT("niagara_inspection_loaded_identity_unexpected"),TEXT("Loaded System does not match exact normalized path."));return nullptr;}System=S;Spawn=S->GetSystemSpawnScript();Update=S->GetSystemUpdateScript();if(!Script(Spawn.Get())||!Script(Update.Get())){return nullptr;}for(UNiagaraScript* P:S->ScratchPadScripts){SystemScratch.Add(P);if(!Script(P)){return nullptr;}}if(S->GetEmitterHandles().Num()>4096){Fail(TEXT("niagara_inspection_emitter_bound"),TEXT("Emitter bound exceeded."));return nullptr;}for(const auto& H:S->GetEmitterHandles()){FEmitterIdentity E;E.Id=H.GetId();E.Name=H.GetName().ToString();E.Enabled=H.GetIsEnabled();E.Mode=static_cast<uint8>(H.GetEmitterMode());E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Data){if(!Pin(E.Emitter.Get())){return nullptr;}E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.Parent=E.Data->ParentScratchPads;if(!Graph(E.Source.Get())){return nullptr;}TArray<UNiagaraScript*> ES;E.Data->GetScripts(ES,false,false);for(auto* P:ES){E.Scripts.Add(P);if(!Script(P)){return nullptr;}}if(E.Scratch.IsValid()){if(!Pin(E.Scratch.Get())){return nullptr;}for(UNiagaraScript* P:E.Scratch->Scripts){E.ScratchScripts.Add(P);if(!Script(P)){return nullptr;}}}if(E.Parent.IsValid()){if(!Pin(E.Parent.Get())){return nullptr;}for(UNiagaraScript* P:E.Parent->Scripts){E.ParentScripts.Add(P);if(!Script(P)){return nullptr;}}}}Emitters.Add(MoveTemp(E));}Before=Projection();return R.Error.IsEmpty()?S:nullptr;}
    TSharedPtr<FJsonObject> Projection()
    {auto O=MakeShared<FJsonObject>();if(!Admit(TEXT("current_source_projection"),true)){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);return O;}TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& I:Objects){auto* A=I->Object.Get();if(A->GetOutermost()==GetTransientPackage()){continue;}auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),I->Path);J->SetStringField(TEXT("package"),A->GetOutermost()->GetName());J->SetBoolField(TEXT("dirty"),A->GetOutermost()->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("objects"),Rows);O->SetBoolField(TEXT("source_projection_complete"),true);return O;}
    bool Request(bool Force,bool& Accepted)
    {R.bCompileRequested=true;if(!SDK(TEXT("RequestCompile"),true)){return false;}R.bCompileAttempted=true;R.Compiled.Reset();Compile->SetBoolField(TEXT("attempted"),true);Accepted=System->RequestCompile(Force);Compile->SetBoolField(TEXT("SDK_returned"),true);Compile->SetBoolField(TEXT("accepted"),Accepted);Compile->SetBoolField(TEXT("completion_verified"),false);return After(TEXT("RequestCompile"));}
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("read_continuation"));}
FString ObserveNodeText(const UEdGraphNode* N,bool Tooltip)
{if(!Current||!Current->SDK(Tooltip?TEXT("node_GetTooltipText"):TEXT("node_GetNodeTitle"))){return FString();}const FString S=Tooltip?N->GetTooltipText().ToString():N->GetNodeTitle(ENodeTitleType::ListView).ToString();if(!Current->After(Tooltip?TEXT("node_GetTooltipText"):TEXT("node_GetNodeTitle"))){return FString();}return S;}


struct FResolvedSystemScratchPadGraph{UNiagaraSystem* System=nullptr;UNiagaraScript* Script=nullptr;UNiagaraScriptSource* Source=nullptr;UNiagaraGraph* Graph=nullptr;int32 ScriptIndex=INDEX_NONE;};
TArray<TSharedPtr<FNiagaraAction_NewNode>> CollectNiagaraGraphActions(UNiagaraGraph* Graph,UNiagaraGraph*& OutTemporaryOwner)
{
    OutTemporaryOwner=nullptr;if(!Current||!Current->SDK(TEXT("allocate_transient_action_owner"))){return {};}
    TStrongObjectPtr<UNiagaraGraph> Owner(NewObject<UNiagaraGraph>(GetTransientPackage(),NAME_None,RF_Transient));Current->Transient->SetBoolField(TEXT("owner_allocation_returned_nonnull"),Owner.Get()!=nullptr);
    if(!Current->After(TEXT("allocate_transient_action_owner"))||!Owner.Get()){return {};}if(!Current->Pin(Owner.Get())){return {};}OutTemporaryOwner=Owner.Get();if(!Graph){return {};}
    if(!Current->SDK(TEXT("GetGraphActions"))){return {};}const UEdGraphSchema_Niagara* Schema=GetDefault<UEdGraphSchema_Niagara>();TArray<TSharedPtr<FNiagaraAction_NewNode>> Actions=Schema?Schema->GetGraphActions(Graph,nullptr,OutTemporaryOwner):TArray<TSharedPtr<FNiagaraAction_NewNode>>();Current->Transient->SetBoolField(TEXT("GetGraphActions_returned"),true);Current->Transient->SetNumberField(TEXT("returned_action_count"),Actions.Num());if(!Current->After(TEXT("GetGraphActions"))){return {};}if(Actions.Num()>100000){Current->Fail(TEXT("niagara_graph_action_inventory_bound"),TEXT("Engine action inventory exceeds bounded100000."));return {};}
    for(const auto& Action:Actions){if(Action.IsValid()&&Action->WeakNodeTemplate.IsValid()){if(!Current->Pin(Action->WeakNodeTemplate.Get())){return {};}}}return Actions;
}

FString NormalizeNiagaraObjectPathForLoad(const FString& ObjectPath)
{
    FString NormalizedPath = FPackageName::ExportTextPathToObjectPath(ObjectPath).TrimStartAndEnd();
    NormalizedPath.TrimQuotesInline();

    if (NormalizedPath.StartsWith(TEXT("/")) && !NormalizedPath.StartsWith(TEXT("/Script/")) && !NormalizedPath.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(NormalizedPath);
        NormalizedPath = FString::Printf(TEXT("%s.%s"), *NormalizedPath, *AssetName);
    }

    return NormalizedPath;
}

FString PackagePathFromObjectPath(const FString& ObjectPath)
{
    FString PackagePath = FPackageName::ExportTextPathToObjectPath(ObjectPath).TrimStartAndEnd();
    PackagePath.TrimQuotesInline();
    if (PackagePath.EndsWith(TEXT(".uasset"), ESearchCase::IgnoreCase))
    {
        PackagePath.LeftChopInline(7);
    }
    const int32 LastSlashIndex = PackagePath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    const int32 DotIndex = PackagePath.Find(
        TEXT("."),
        ESearchCase::CaseSensitive,
        ESearchDir::FromStart,
        LastSlashIndex == INDEX_NONE ? 0 : LastSlashIndex + 1);
    if (DotIndex != INDEX_NONE)
    {
        PackagePath.LeftInline(DotIndex);
    }
    return PackagePath;
}

bool IsTempGeneratedNiagaraPath(const FString& ObjectPath)
{
    const FString PackagePath = PackagePathFromObjectPath(ObjectPath);
    FString TempRoot = NiagaraTempGenerationRoot;
    while (TempRoot.Len() > 1 && TempRoot.EndsWith(TEXT("/")))
    {
        TempRoot.LeftChopInline(1);
    }
    return PackagePath.Equals(TempRoot) || PackagePath.StartsWith(TempRoot + TEXT("/"));
}

void AddNiagaraScriptSourceIfValid(
    UNiagaraScriptSourceBase* SourceBase,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources)
{
    UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
    if (Source && Source->NodeGraph && !SeenSources.Contains(Source))
    {
        SeenSources.Add(Source);
        OutSources.Add(Source);
    }
}

void AddNiagaraScriptLatestSourceIfValid(
    UNiagaraScript* Script,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources)
{
    if (Script)
    {
        AddNiagaraScriptSourceIfValid(Script->GetLatestSource(), OutSources, SeenSources);
    }
}

void AddScratchPadContainerScriptSources(
    UNiagaraScratchPadContainer* Container,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources)
{
    if (!Container)
    {
        return;
    }

    for (const TObjectPtr<UNiagaraScript> Script : TArray<TObjectPtr<UNiagaraScript>>(Container->Scripts))
    {
        AddNiagaraScriptLatestSourceIfValid(Script.Get(), OutSources, SeenSources);
    }
}

TArray<UNiagaraScriptSource*> CollectTargetNiagaraScriptSources(UNiagaraSystem* System)
{
    TArray<UNiagaraScriptSource*> Sources;
    TSet<UNiagaraScriptSource*> SeenSources;
    if (!System)
    {
        return Sources;
    }

    AddNiagaraScriptLatestSourceIfValid(System->GetSystemSpawnScript(), Sources, SeenSources);
    AddNiagaraScriptLatestSourceIfValid(System->GetSystemUpdateScript(), Sources, SeenSources);

    for (const TObjectPtr<UNiagaraScript> ScratchPadScript : TArray<TObjectPtr<UNiagaraScript>>(System->ScratchPadScripts))
    {
        AddNiagaraScriptLatestSourceIfValid(ScratchPadScript.Get(), Sources, SeenSources);
    }

    for (const FNiagaraEmitterHandle& EmitterHandle : TArray<FNiagaraEmitterHandle>(System->GetEmitterHandles()))
    {
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            AddNiagaraScriptSourceIfValid(EmitterData->GraphSource, Sources, SeenSources);
            AddScratchPadContainerScriptSources(EmitterData->ScratchPads, Sources, SeenSources);
            AddScratchPadContainerScriptSources(EmitterData->ParentScratchPads, Sources, SeenSources);
        }
    }

    return Sources;
}

FString NiagaraScriptUsageName(ENiagaraScriptUsage Usage)
{
    if (const UEnum* Enum = StaticEnum<ENiagaraScriptUsage>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Usage));
    }
    return FString::FromInt(static_cast<int32>(Usage));
}

FString NiagaraCompileStatusName(ENiagaraScriptCompileStatus Status)
{
    if (const UEnum* Enum = StaticEnum<ENiagaraScriptCompileStatus>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Status));
    }
    return FString::FromInt(static_cast<int32>(Status));
}

bool IsNiagaraCompileErrorStatus(ENiagaraScriptCompileStatus Status)
{
    return Status == ENiagaraScriptCompileStatus::NCS_Error;
}

bool IsNiagaraCompileWarningStatus(ENiagaraScriptCompileStatus Status)
{
    return Status == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings ||
        Status == ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings;
}

TSharedPtr<FJsonObject> BuildNiagaraScriptCompileStatusJson(
    const UNiagaraScript* Script,
    const FString& OwnerKind,
    const FString& OwnerName,
    int32 ScriptIndex)
{
    TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
    ScriptObject->SetNumberField(TEXT("script_index"), ScriptIndex);
    ScriptObject->SetStringField(TEXT("owner_kind"), OwnerKind);
    ScriptObject->SetStringField(TEXT("owner_name"), OwnerName);
    if (!Script)
    {
        ScriptObject->SetStringField(TEXT("script_name"), FString());
        ScriptObject->SetStringField(TEXT("script_path"), FString());
        ScriptObject->SetStringField(TEXT("usage"), FString());
        ScriptObject->SetStringField(TEXT("compile_status"), TEXT("missing"));
        ScriptObject->SetBoolField(TEXT("has_error"), true);
        ScriptObject->SetBoolField(TEXT("has_warning"), false);
        return ScriptObject;
    }

    if(!Current||!Current->SDK(TEXT("script_compile_status_snapshot"))) return ScriptObject;
    const ENiagaraScriptCompileStatus Status = Script->GetLastCompileStatus();
    ScriptObject->SetStringField(TEXT("script_name"), Script->GetName());
    ScriptObject->SetStringField(TEXT("script_path"), Script->GetPathName());
    ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
    ScriptObject->SetStringField(TEXT("usage_id"), Script->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
    ScriptObject->SetStringField(TEXT("compile_status"), NiagaraCompileStatusName(Status));
    ScriptObject->SetBoolField(TEXT("has_error"), IsNiagaraCompileErrorStatus(Status));
    ScriptObject->SetBoolField(TEXT("has_warning"), IsNiagaraCompileWarningStatus(Status));
    ScriptObject->SetBoolField(TEXT("is_ready_cpu"), Script->IsReadyToRun(ENiagaraSimTarget::CPUSim));
    ScriptObject->SetBoolField(TEXT("is_ready_gpu"), Script->IsReadyToRun(ENiagaraSimTarget::GPUComputeSim));
    // IsReadyToRun(GPU) checks VM/DI compatibility, not the current RHI shader map.
    // Keep that legacy field intact and report the public shader result separately.
    const bool bGPUShaderApplicable = Script->GetUsage() == ENiagaraScriptUsage::ParticleGPUComputeScript;
    ScriptObject->SetBoolField(TEXT("gpu_shader_compilation_applicable"), bGPUShaderApplicable);
    if (bGPUShaderApplicable)
    {
        const bool bShaderPending = Script->IsScriptCompilationPending(true);
        const bool bShaderSucceeded = Script->DidScriptCompilationSucceed(true);
        ScriptObject->SetBoolField(TEXT("gpu_shader_compilation_pending"), bShaderPending);
        ScriptObject->SetBoolField(TEXT("gpu_shader_compilation_succeeded"), bShaderSucceeded);
        ScriptObject->SetBoolField(TEXT("gpu_ready_to_execute"),
            !bShaderPending && bShaderSucceeded && Script->IsReadyToRun(ENiagaraSimTarget::GPUComputeSim)
            && (Status == ENiagaraScriptCompileStatus::NCS_UpToDate
                || IsNiagaraCompileWarningStatus(Status)));
    }
    else
    {
        ScriptObject->SetField(TEXT("gpu_shader_compilation_pending"), MakeShared<FJsonValueNull>());
        ScriptObject->SetField(TEXT("gpu_shader_compilation_succeeded"), MakeShared<FJsonValueNull>());
        ScriptObject->SetField(TEXT("gpu_ready_to_execute"), MakeShared<FJsonValueNull>());
    }
    if(!Current->After(TEXT("script_compile_status_snapshot"))) return ScriptObject;
    return ScriptObject;
}

FString NiagaraObjectPathOrEmpty(const UObject* Object)
{
    return Object ? Object->GetPathName() : FString();
}

TSharedPtr<FJsonObject> GraphPinToJsonObject(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (!Pin)
    {
        return PinObject;
    }

    PinObject->SetStringField(TEXT("name"), Pin->PinName.ToString());
    PinObject->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    PinObject->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
    PinObject->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
    PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    PinObject->SetStringField(TEXT("default_object"), NiagaraObjectPathOrEmpty(Pin->DefaultObject));
    PinObject->SetNumberField(TEXT("linked_to_count"), Pin->LinkedTo.Num());
    return PinObject;
}

TArray<TSharedPtr<FJsonValue>> GraphPinsToJson(const TArray<UEdGraphPin*>& Pins)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const UEdGraphPin* Pin : Pins)
    {
        Values.Add(MakeShared<FJsonValueObject>(GraphPinToJsonObject(Pin)));
    }
    return Values;
}

FString NiagaraTypeName(const FNiagaraTypeDefinition& TypeDef)
{
    return TypeDef.GetNameText().ToString();
}

FString GetNiagaraCustomHlslString(const UNiagaraNodeCustomHlsl* CustomNode)
{
    if (!CustomNode)
    {
        return FString();
    }

    if (const FStrProperty* CustomHlslProperty = FindFProperty<FStrProperty>(CustomNode->GetClass(), TEXT("CustomHlsl")))
    {
        return CustomHlslProperty->GetPropertyValue_InContainer(CustomNode);
    }
    return FString();
}

TSharedPtr<FJsonObject> BuildCustomHlslNodeJson(
    const UNiagaraScriptSource* Source,
    const UNiagaraNodeCustomHlsl* CustomNode,
    int32 NodeIndex,
    int32 MaxHlslChars)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!CustomNode)
    {
        return NodeObject;
    }

    const FString CustomHlsl = GetNiagaraCustomHlslString(CustomNode);
    FString HlslPreview = CustomHlsl;
    bool bTruncated = false;
    if (MaxHlslChars >= 0 && HlslPreview.Len() > MaxHlslChars)
    {
        HlslPreview = HlslPreview.Left(MaxHlslChars);
        bTruncated = true;
    }

    NodeObject->SetNumberField(TEXT("node_index"), NodeIndex);
    NodeObject->SetStringField(TEXT("node_name"), CustomNode->GetName());
    NodeObject->SetStringField(TEXT("node_guid"), CustomNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    NodeObject->SetStringField(TEXT("signature_name"), CustomNode->Signature.GetNameString());
    NodeObject->SetStringField(TEXT("custom_hlsl_script_usage"), NiagaraScriptUsageName(CustomNode->ScriptUsage));
    NodeObject->SetBoolField(TEXT("signature_requires_context"), CustomNode->Signature.bRequiresContext);
    NodeObject->SetStringField(TEXT("source_path"), Source ? Source->GetPathName() : FString());
    NodeObject->SetStringField(TEXT("graph_path"), Source && Source->NodeGraph ? Source->NodeGraph->GetPathName() : FString());
    NodeObject->SetNumberField(TEXT("hlsl_length"), CustomHlsl.Len());
    NodeObject->SetBoolField(TEXT("hlsl_truncated"), bTruncated);
    NodeObject->SetStringField(TEXT("custom_hlsl"), HlslPreview);

    TArray<TSharedPtr<FJsonValue>> SignatureInputValues;
    for (const FNiagaraVariable& Input : CustomNode->Signature.Inputs)
    {
        TSharedPtr<FJsonObject> InputObject = MakeShared<FJsonObject>();
        InputObject->SetStringField(TEXT("name"), Input.GetName().ToString());
        InputObject->SetStringField(TEXT("type"), NiagaraTypeName(Input.GetType()));
        SignatureInputValues.Add(MakeShared<FJsonValueObject>(InputObject));
    }
    NodeObject->SetArrayField(TEXT("signature_inputs"), SignatureInputValues);

    TArray<TSharedPtr<FJsonValue>> SignatureOutputValues;
    for (const FNiagaraVariableBase& Output : CustomNode->Signature.Outputs)
    {
        TSharedPtr<FJsonObject> OutputObject = MakeShared<FJsonObject>();
        OutputObject->SetStringField(TEXT("name"), Output.GetName().ToString());
        OutputObject->SetStringField(TEXT("type"), NiagaraTypeName(Output.GetType()));
        SignatureOutputValues.Add(MakeShared<FJsonValueObject>(OutputObject));
    }
    NodeObject->SetArrayField(TEXT("signature_outputs"), SignatureOutputValues);

    TArray<UEdGraphPin*> InputPins;
    TArray<UEdGraphPin*> OutputPins;
    CustomNode->GetInputPins(InputPins);
    CustomNode->GetOutputPins(OutputPins);
    NodeObject->SetArrayField(TEXT("input_pins"), GraphPinsToJson(InputPins));
    NodeObject->SetArrayField(TEXT("output_pins"), GraphPinsToJson(OutputPins));
    return NodeObject;
}

bool NiagaraCustomHlslNodeMatches(
    const UNiagaraNodeCustomHlsl* CustomNode,
    const FString& NodeGuid,
    const FString& SignatureName,
    const FString& ContainsText)
{
    if (!CustomNode)
    {
        return false;
    }

    if (!NodeGuid.IsEmpty() &&
        !CustomNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens).Equals(NodeGuid, ESearchCase::IgnoreCase))
    {
        return false;
    }

    if (!SignatureName.IsEmpty() &&
        !CustomNode->Signature.GetNameString().Equals(SignatureName, ESearchCase::IgnoreCase))
    {
        return false;
    }

    if (!ContainsText.IsEmpty() && !GetNiagaraCustomHlslString(CustomNode).Contains(ContainsText, ESearchCase::CaseSensitive))
    {
        return false;
    }

    return true;
}

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    return JsonValues;
}

void AddUniqueString(TArray<FString>& Values, const FString& Value)
{
    if (!Value.IsEmpty() && !Values.Contains(Value))
    {
        Values.Add(Value);
    }
}

void AddHintIfContains(const FString& SearchText, const FString& Needle, const FString& Hint, TArray<FString>& Hints)
{
    if (SearchText.Contains(Needle, ESearchCase::IgnoreCase))
    {
        AddUniqueString(Hints, Hint);
    }
}

TArray<FString> BuildNiagaraControlHints(const FString& SearchText)
{
    TArray<FString> Hints;
    AddHintIfContains(SearchText, TEXT("color"), TEXT("color_or_tint_control"), Hints);
    AddHintIfContains(SearchText, TEXT("colour"), TEXT("color_or_tint_control"), Hints);
    AddHintIfContains(SearchText, TEXT("tint"), TEXT("color_or_tint_control"), Hints);
    AddHintIfContains(SearchText, TEXT("ribbonwidth"), TEXT("ribbon_width_control"), Hints);
    AddHintIfContains(SearchText, TEXT("ribbon width"), TEXT("ribbon_width_control"), Hints);
    AddHintIfContains(SearchText, TEXT("width"), TEXT("width_control"), Hints);
    AddHintIfContains(SearchText, TEXT("scale"), TEXT("scale_control"), Hints);
    AddHintIfContains(SearchText, TEXT("size"), TEXT("size_control"), Hints);
    AddHintIfContains(SearchText, TEXT("lifetime"), TEXT("lifetime_control"), Hints);
    AddHintIfContains(SearchText, TEXT("life time"), TEXT("lifetime_control"), Hints);
    AddHintIfContains(SearchText, TEXT("duration"), TEXT("duration_control"), Hints);
    AddHintIfContains(SearchText, TEXT("velocity"), TEXT("velocity_control"), Hints);
    AddHintIfContains(SearchText, TEXT("speed"), TEXT("velocity_control"), Hints);
    AddHintIfContains(SearchText, TEXT("spawn"), TEXT("spawn_control"), Hints);
    AddHintIfContains(SearchText, TEXT("materialparam"), TEXT("dynamic_material_parameter_control"), Hints);
    AddHintIfContains(SearchText, TEXT("dynamicmaterial"), TEXT("dynamic_material_parameter_control"), Hints);
    AddHintIfContains(SearchText, TEXT("user."), TEXT("user_parameter_reference"), Hints);
    AddHintIfContains(SearchText, TEXT("scratch"), TEXT("scratch_pad_reference"), Hints);
    AddHintIfContains(SearchText, TEXT("trail"), TEXT("trail_behavior"), Hints);
    AddHintIfContains(SearchText, TEXT("ribbon"), TEXT("ribbon_behavior"), Hints);
    return Hints;
}

TSharedPtr<FJsonObject> NiagaraVariableToJsonObject(const FNiagaraVariableBase& Variable)
{
    TSharedPtr<FJsonObject> VariableObject = MakeShared<FJsonObject>();
    VariableObject->SetStringField(TEXT("name"), Variable.GetName().ToString());
    VariableObject->SetStringField(TEXT("type"), NiagaraTypeName(Variable.GetType()));
    VariableObject->SetBoolField(TEXT("type_is_valid"), Variable.GetType().IsValid());
    VariableObject->SetBoolField(TEXT("type_is_data_interface"), Variable.GetType().IsDataInterface());
    VariableObject->SetBoolField(TEXT("type_is_uobject"), Variable.GetType().IsUObject());
    if (UClass* TypeClass = Variable.GetType().GetClass())
    {
        VariableObject->SetStringField(TEXT("type_class"), TypeClass->GetName());
        VariableObject->SetStringField(TEXT("type_class_path"), TypeClass->GetPathName());
    }
    return VariableObject;
}

TArray<TSharedPtr<FJsonValue>> NiagaraVariablesToJson(const TArray<FNiagaraVariable>& Variables)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FNiagaraVariable& Variable : Variables)
    {
        Values.Add(MakeShared<FJsonValueObject>(NiagaraVariableToJsonObject(Variable)));
    }
    return Values;
}

TArray<TSharedPtr<FJsonValue>> NiagaraVariableBasesToJson(const TArray<FNiagaraVariableBase>& Variables)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FNiagaraVariableBase& Variable : Variables)
    {
        Values.Add(MakeShared<FJsonValueObject>(NiagaraVariableToJsonObject(Variable)));
    }
    return Values;
}

TSharedPtr<FJsonObject> DetailedPinRefToJsonObject(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (!Pin)
    {
        return PinObject;
    }

    const UEdGraphNode* OwningNode = Pin->GetOwningNode();
    PinObject->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::DigitsWithHyphens));
    PinObject->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
    PinObject->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
    PinObject->SetStringField(TEXT("node_name"), OwningNode ? OwningNode->GetName() : FString());
    PinObject->SetStringField(TEXT("node_guid"), OwningNode ? OwningNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString());
    PinObject->SetStringField(TEXT("node_class"), OwningNode ? OwningNode->GetClass()->GetName() : FString());
    return PinObject;
}

TSharedPtr<FJsonObject> DetailedPinToJsonObject(const UEdGraphPin* Pin, bool bIncludeLinkedPins)
{
    TSharedPtr<FJsonObject> PinObject = GraphPinToJsonObject(Pin);
    if (!Pin)
    {
        return PinObject;
    }

    PinObject->SetStringField(TEXT("pin_id"), Pin->PinId.ToString(EGuidFormats::DigitsWithHyphens));
    PinObject->SetStringField(TEXT("persistent_guid"), Pin->PersistentGuid.ToString(EGuidFormats::DigitsWithHyphens));
    PinObject->SetStringField(TEXT("friendly_name"), Pin->PinFriendlyName.ToString());
    PinObject->SetStringField(TEXT("tooltip"), Pin->PinToolTip);
    PinObject->SetBoolField(TEXT("hidden"), Pin->bHidden);
    PinObject->SetBoolField(TEXT("not_connectable"), Pin->bNotConnectable);
    PinObject->SetBoolField(TEXT("advanced_view"), Pin->bAdvancedView);

    if (bIncludeLinkedPins)
    {
        TArray<TSharedPtr<FJsonValue>> LinkedValues;
        for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            LinkedValues.Add(MakeShared<FJsonValueObject>(DetailedPinRefToJsonObject(LinkedPin)));
        }
        PinObject->SetArrayField(TEXT("linked_pins"), LinkedValues);
    }

    return PinObject;
}

TArray<TSharedPtr<FJsonValue>> DetailedPinsToJson(const TArray<UEdGraphPin*>& Pins, bool bIncludeLinkedPins)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const UEdGraphPin* Pin : Pins)
    {
        Values.Add(MakeShared<FJsonValueObject>(DetailedPinToJsonObject(Pin, bIncludeLinkedPins)));
    }
    return Values;
}

TSharedPtr<FJsonObject> DetailedGraphNodeToJsonObject(
    const UEdGraphNode* Node,
    int32 NodeIndex,
    bool bIncludePins,
    bool bIncludeLinkedPins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!Node)
    {
        return NodeObject;
    }

    NodeObject->SetNumberField(TEXT("node_index"), NodeIndex);
    NodeObject->SetStringField(TEXT("node_name"), Node->GetName());
    NodeObject->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    NodeObject->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
    NodeObject->SetStringField(TEXT("node_title"), ObserveNodeText(Node,false));
    if(!ContinueRead()) return NodeObject;
    NodeObject->SetStringField(TEXT("tooltip"), ObserveNodeText(Node,true));
    if(!ContinueRead()) return NodeObject;
    NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
    NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);
    NodeObject->SetNumberField(TEXT("width"), Node->NodeWidth);
    NodeObject->SetNumberField(TEXT("height"), Node->NodeHeight);
    NodeObject->SetNumberField(TEXT("pin_count"), Node->Pins.Num());

    if (const UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
    {
        FString FunctionName = FunctionCall->GetFunctionName();
        if (FunctionName.IsEmpty())
        {
            FunctionName = FunctionCall->Signature.GetNameString();
        }

        const UNiagaraScript* FunctionScript = FunctionCall->FunctionScript;
        const UObject* ScriptOuter = FunctionScript ? FunctionScript->GetOuter() : nullptr;
        NodeObject->SetStringField(TEXT("niagara_node_kind"), TEXT("function_call"));
        NodeObject->SetStringField(TEXT("function_name"), FunctionName);
        NodeObject->SetStringField(TEXT("function_script"), NiagaraObjectPathOrEmpty(FunctionScript));
        NodeObject->SetStringField(TEXT("function_script_asset_object_path"), FunctionCall->FunctionScriptAssetObjectPath.ToString());
        NodeObject->SetStringField(TEXT("called_usage"), NiagaraScriptUsageName(FunctionCall->GetCalledUsage()));
        NodeObject->SetBoolField(TEXT("is_scratch_pad"), ScriptOuter && ScriptOuter->IsA<UNiagaraScratchPadContainer>());
        NodeObject->SetStringField(TEXT("signature_name"), FunctionCall->Signature.GetNameString());
        NodeObject->SetArrayField(TEXT("signature_inputs"), NiagaraVariablesToJson(FunctionCall->Signature.Inputs));
        NodeObject->SetArrayField(TEXT("signature_outputs"), NiagaraVariableBasesToJson(FunctionCall->Signature.Outputs));
        NodeObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(FunctionName)));
    }
    else if (const UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
    {
        NodeObject->SetStringField(TEXT("niagara_node_kind"), TEXT("input"));
        NodeObject->SetObjectField(TEXT("variable"), NiagaraVariableToJsonObject(InputNode->Input));
        NodeObject->SetNumberField(TEXT("call_sort_priority"), InputNode->CallSortPriority);
        NodeObject->SetBoolField(TEXT("is_exposed"), InputNode->IsExposed());
        NodeObject->SetBoolField(TEXT("is_required"), InputNode->IsRequired());
        NodeObject->SetBoolField(TEXT("is_hidden"), InputNode->IsHidden());
        NodeObject->SetBoolField(TEXT("can_auto_bind"), InputNode->CanAutoBind());
        NodeObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(InputNode->Input.GetName().ToString())));
    }
    else if (const UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
    {
        NodeObject->SetStringField(TEXT("niagara_node_kind"), TEXT("output"));
        NodeObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(OutputNode->GetUsage()));
        NodeObject->SetStringField(TEXT("usage_id"), OutputNode->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
        NodeObject->SetArrayField(TEXT("outputs"), NiagaraVariablesToJson(OutputNode->GetOutputs()));
    }
    else
    {
        NodeObject->SetStringField(TEXT("niagara_node_kind"), TEXT("generic"));
    }

    if (bIncludePins)
    {
        NodeObject->SetArrayField(TEXT("pins"), DetailedPinsToJson(Node->Pins, bIncludeLinkedPins));
    }

    return NodeObject;
}

TArray<TSharedPtr<FJsonValue>> GraphLinksToJson(const UNiagaraGraph* Graph, int32 MaxLinks)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!Graph || MaxLinks <= 0)
    {
        return Values;
    }

    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output)
            {
                continue;
            }

            for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (!LinkedPin)
                {
                    continue;
                }

                TSharedPtr<FJsonObject> LinkObject = MakeShared<FJsonObject>();
                LinkObject->SetObjectField(TEXT("from"), DetailedPinRefToJsonObject(Pin));
                LinkObject->SetObjectField(TEXT("to"), DetailedPinRefToJsonObject(LinkedPin));
                Values.Add(MakeShared<FJsonValueObject>(LinkObject));

                if (Values.Num() >= MaxLinks)
                {
                    return Values;
                }
            }
        }
    }

    return Values;
}

TSharedPtr<FJsonObject> BuildDetailedGraphJson(
    const UNiagaraScriptSourceBase* SourceBase,
    const FString& Context,
    bool bIncludePins,
    bool bIncludeLinks,
    int32 MaxNodes,
    int32 MaxLinks)
{
    TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
    GraphObject->SetStringField(TEXT("context"), Context);
    GraphObject->SetStringField(TEXT("source_class"), SourceBase ? SourceBase->GetClass()->GetName() : FString());
    GraphObject->SetStringField(TEXT("source_path"), NiagaraObjectPathOrEmpty(SourceBase));

    const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    GraphObject->SetStringField(TEXT("graph_path"), NiagaraObjectPathOrEmpty(Graph));
    GraphObject->SetBoolField(TEXT("has_graph"), Graph != nullptr);

    if (!Graph)
    {
        GraphObject->SetNumberField(TEXT("node_count"), 0);
        GraphObject->SetNumberField(TEXT("included_node_count"), 0);
        GraphObject->SetNumberField(TEXT("link_count"), 0);
        GraphObject->SetArrayField(TEXT("nodes"), TArray<TSharedPtr<FJsonValue>>());
        GraphObject->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());
        return GraphObject;
    }

    TArray<UEdGraphNode*> Nodes = Graph->Nodes;
    Nodes.Sort(
        [](const UEdGraphNode& Left, const UEdGraphNode& Right)
        {
            if (Left.NodePosY == Right.NodePosY)
            {
                return Left.NodePosX < Right.NodePosX;
            }
            return Left.NodePosY < Right.NodePosY;
        });

    TMap<FString, int32> NodeClassCounts;
    TArray<TSharedPtr<FJsonValue>> NodeValues;
    const int32 SafeMaxNodes = FMath::Max(0, MaxNodes);
    for (int32 Index = 0; Index < Nodes.Num(); ++Index)
    {
        const UEdGraphNode* Node = Nodes[Index];
        if (!Node)
        {
            continue;
        }

        NodeClassCounts.FindOrAdd(Node->GetClass()->GetName()) += 1;
        if (NodeValues.Num() < SafeMaxNodes)
        {
            NodeValues.Add(MakeShared<FJsonValueObject>(DetailedGraphNodeToJsonObject(Node, Index, bIncludePins, bIncludeLinks)));
            if(!ContinueRead()) return GraphObject;
        }
    }

    int32 TotalLinkCount = 0;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node)
        {
            continue;
        }
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output)
            {
                TotalLinkCount += Pin->LinkedTo.Num();
            }
        }
    }

    TArray<TSharedPtr<FJsonValue>> NodeClassValues;
    for (const TPair<FString, int32>& Pair : NodeClassCounts)
    {
        TSharedPtr<FJsonObject> ClassObject = MakeShared<FJsonObject>();
        ClassObject->SetStringField(TEXT("node_class"), Pair.Key);
        ClassObject->SetNumberField(TEXT("count"), Pair.Value);
        NodeClassValues.Add(MakeShared<FJsonValueObject>(ClassObject));
    }

    GraphObject->SetNumberField(TEXT("node_count"), Nodes.Num());
    GraphObject->SetNumberField(TEXT("included_node_count"), NodeValues.Num());
    GraphObject->SetBoolField(TEXT("nodes_truncated"), Nodes.Num() > SafeMaxNodes);
    GraphObject->SetNumberField(TEXT("link_count"), TotalLinkCount);
    GraphObject->SetNumberField(TEXT("included_link_count"), bIncludeLinks ? FMath::Min(TotalLinkCount, FMath::Max(0, MaxLinks)) : 0);
    GraphObject->SetBoolField(TEXT("links_truncated"), bIncludeLinks && TotalLinkCount > FMath::Max(0, MaxLinks));
    GraphObject->SetArrayField(TEXT("node_class_counts"), NodeClassValues);
    GraphObject->SetArrayField(TEXT("nodes"), NodeValues);
    GraphObject->SetArrayField(TEXT("links"), bIncludeLinks ? GraphLinksToJson(Graph, MaxLinks) : TArray<TSharedPtr<FJsonValue>>());
    return GraphObject;
}

TArray<TSharedPtr<FJsonValue>> NiagaraScriptUsagesToJson(const TArray<ENiagaraScriptUsage>& Usages)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (ENiagaraScriptUsage Usage : Usages)
    {
        Values.Add(MakeShared<FJsonValueString>(NiagaraScriptUsageName(Usage)));
    }
    return Values;
}

TSharedPtr<FJsonObject> BuildDetailedScratchPadScriptJson(
    const UNiagaraScript* Script,
    const FString& OwnerKind,
    bool bIncludePins,
    bool bIncludeLinks,
    int32 MaxNodes,
    int32 MaxLinks)
{
    TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
    if (!Script)
    {
        return ScriptObject;
    }

    ScriptObject->SetStringField(TEXT("name"), Script->GetName());
    ScriptObject->SetStringField(TEXT("path"), Script->GetPathName());
    ScriptObject->SetStringField(TEXT("owner_kind"), OwnerKind);
    ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
    TArray<ENiagaraScriptUsage> SupportedUsageContexts;
    if (const FVersionedNiagaraScriptData* ScriptData = Script->GetLatestScriptData())
    {
        SupportedUsageContexts = ScriptData->GetSupportedUsageContexts();
    }
    ScriptObject->SetArrayField(TEXT("supported_usage_contexts"), NiagaraScriptUsagesToJson(SupportedUsageContexts));
    ScriptObject->SetObjectField(
        TEXT("graph"),
        BuildDetailedGraphJson(
            Script->GetLatestSource(),
            FString::Printf(TEXT("%s_scratch_pad:%s"), *OwnerKind, *Script->GetName()),
            bIncludePins,
            bIncludeLinks,
            MaxNodes,
            MaxLinks));
    return ScriptObject;
}

void AppendDetailedScratchPadContainerScripts(
    const UNiagaraScratchPadContainer* Container,
    const FString& OwnerKind,
    bool bIncludePins,
    bool bIncludeLinks,
    int32 MaxNodes,
    int32 MaxLinks,
    TArray<TSharedPtr<FJsonValue>>& OutScripts)
{
#if WITH_EDITORONLY_DATA
    if (!Container)
    {
        return;
    }

    for (const TObjectPtr<UNiagaraScript> Script : TArray<TObjectPtr<UNiagaraScript>>(Container->Scripts))
    {
        OutScripts.Add(MakeShared<FJsonValueObject>(
            BuildDetailedScratchPadScriptJson(Script.Get(), OwnerKind, bIncludePins, bIncludeLinks, MaxNodes, MaxLinks)));
        if(!ContinueRead()) return;
    }
#endif
}

TSharedPtr<FJsonObject> BuildFunctionCallJson(const UNiagaraNodeFunctionCall* FunctionCall, int32 NodeIndex, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!FunctionCall)
    {
        return NodeObject;
    }

    FString FunctionName = FunctionCall->GetFunctionName();
    if (FunctionName.IsEmpty())
    {
        FunctionName = FunctionCall->Signature.GetNameString();
    }

    const UNiagaraScript* FunctionScript = FunctionCall->FunctionScript;
    const UObject* ScriptOuter = FunctionScript ? FunctionScript->GetOuter() : nullptr;
    const bool bIsScratchPad = ScriptOuter && ScriptOuter->IsA<UNiagaraScratchPadContainer>();

    NodeObject->SetNumberField(TEXT("node_index"), NodeIndex);
    NodeObject->SetStringField(TEXT("node_name"), FunctionCall->GetName());
    NodeObject->SetStringField(TEXT("node_guid"), FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    NodeObject->SetStringField(TEXT("function_name"), FunctionName);
    NodeObject->SetStringField(TEXT("function_script"), NiagaraObjectPathOrEmpty(FunctionScript));
    NodeObject->SetStringField(TEXT("function_script_asset_object_path"), FunctionCall->FunctionScriptAssetObjectPath.ToString());
    NodeObject->SetStringField(TEXT("script_usage"), FunctionScript ? NiagaraScriptUsageName(FunctionScript->GetUsage()) : FString());
    NodeObject->SetStringField(TEXT("called_usage"), NiagaraScriptUsageName(FunctionCall->GetCalledUsage()));
    NodeObject->SetBoolField(TEXT("is_scratch_pad"), bIsScratchPad);
    NodeObject->SetStringField(TEXT("source_kind"), bIsScratchPad ? TEXT("scratch_pad") : (FunctionScript ? TEXT("script_asset") : TEXT("signature_only")));
    NodeObject->SetArrayField(TEXT("signature_inputs"), NiagaraVariablesToJson(FunctionCall->Signature.Inputs));
    NodeObject->SetArrayField(TEXT("signature_outputs"), NiagaraVariableBasesToJson(FunctionCall->Signature.Outputs));

    TArray<FString> SpecifierValues;
    for (const TPair<FName, FName>& Specifier : FunctionCall->FunctionSpecifiers)
    {
        AddUniqueString(SpecifierValues, FString::Printf(TEXT("%s=%s"), *Specifier.Key.ToString(), *Specifier.Value.ToString()));
    }
    NodeObject->SetArrayField(TEXT("function_specifiers"), StringsToJson(SpecifierValues));

    FString SearchText = FunctionName;
    SearchText += TEXT(" ");
    SearchText += NiagaraObjectPathOrEmpty(FunctionScript);
    for (const FNiagaraVariable& Variable : FunctionCall->Signature.Inputs)
    {
        SearchText += TEXT(" ");
        SearchText += Variable.GetName().ToString();
    }
    for (const FNiagaraVariable& Variable : FunctionCall->Signature.Outputs)
    {
        SearchText += TEXT(" ");
        SearchText += Variable.GetName().ToString();
    }
    NodeObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(SearchText)));

    if (bIncludePins)
    {
        TArray<UEdGraphPin*> InputPins;
        TArray<UEdGraphPin*> OutputPins;
        FunctionCall->GetInputPins(InputPins);
        FunctionCall->GetOutputPins(OutputPins);
        NodeObject->SetArrayField(TEXT("input_pins"), GraphPinsToJson(InputPins));
        NodeObject->SetArrayField(TEXT("output_pins"), GraphPinsToJson(OutputPins));
    }

    return NodeObject;
}

FString TextureRenderTargetFormatName(ETextureRenderTargetFormat Format)
{
    switch (Format)
    {
    case RTF_R8:
        return TEXT("RTF_R8");
    case RTF_RG8:
        return TEXT("RTF_RG8");
    case RTF_RGBA8:
        return TEXT("RTF_RGBA8");
    case RTF_RGBA8_SRGB:
        return TEXT("RTF_RGBA8_SRGB");
    case RTF_R16f:
        return TEXT("RTF_R16f");
    case RTF_RG16f:
        return TEXT("RTF_RG16f");
    case RTF_RGBA16f:
        return TEXT("RTF_RGBA16f");
    case RTF_R32f:
        return TEXT("RTF_R32f");
    case RTF_RG32f:
        return TEXT("RTF_RG32f");
    case RTF_RGBA32f:
        return TEXT("RTF_RGBA32f");
    case RTF_RGB10A2:
        return TEXT("RTF_RGB10A2");
    default:
        return FString::FromInt(static_cast<int32>(Format));
    }
}

TSharedPtr<FJsonObject> BuildUObjectReferenceJson(const UObject* Object)
{
    TSharedPtr<FJsonObject> ObjectJson = MakeShared<FJsonObject>();
    ObjectJson->SetBoolField(TEXT("is_valid"), Object != nullptr);
    if (!Object)
    {
        return ObjectJson;
    }

    ObjectJson->SetStringField(TEXT("name"), Object->GetName());
    ObjectJson->SetStringField(TEXT("path"), Object->GetPathName());
    ObjectJson->SetStringField(TEXT("class"), Object->GetClass() ? Object->GetClass()->GetName() : FString());
    ObjectJson->SetStringField(TEXT("class_path"), Object->GetClass() ? Object->GetClass()->GetPathName() : FString());
    ObjectJson->SetStringField(TEXT("outer_path"), Object->GetOuter() ? Object->GetOuter()->GetPathName() : FString());
    return ObjectJson;
}

TSharedPtr<FJsonObject> BuildRenderTargetObjectJson(const UObject* Object)
{
    TSharedPtr<FJsonObject> ObjectJson = BuildUObjectReferenceJson(Object);
    const UTextureRenderTarget* RenderTarget = Cast<UTextureRenderTarget>(Object);
    ObjectJson->SetBoolField(TEXT("is_texture_render_target"), RenderTarget != nullptr);
    if (RenderTarget)
    {
        ObjectJson->SetNumberField(TEXT("target_gamma"), RenderTarget->TargetGamma);
        ObjectJson->SetBoolField(TEXT("can_create_uav"), RenderTarget->bCanCreateUAV != 0);
        ObjectJson->SetBoolField(TEXT("needs_two_copies"), RenderTarget->bNeedsTwoCopies != 0);
    }

    const UTextureRenderTarget2D* RenderTarget2D = Cast<UTextureRenderTarget2D>(Object);
    ObjectJson->SetBoolField(TEXT("is_texture_render_target_2d"), RenderTarget2D != nullptr);
    if (RenderTarget2D)
    {
        ObjectJson->SetNumberField(TEXT("size_x"), RenderTarget2D->SizeX);
        ObjectJson->SetNumberField(TEXT("size_y"), RenderTarget2D->SizeY);
        ObjectJson->SetStringField(TEXT("render_target_format"), TextureRenderTargetFormatName(RenderTarget2D->RenderTargetFormat));

        TArray<TSharedPtr<FJsonValue>> ClearColorValues;
        ClearColorValues.Add(MakeShared<FJsonValueNumber>(RenderTarget2D->ClearColor.R));
        ClearColorValues.Add(MakeShared<FJsonValueNumber>(RenderTarget2D->ClearColor.G));
        ClearColorValues.Add(MakeShared<FJsonValueNumber>(RenderTarget2D->ClearColor.B));
        ClearColorValues.Add(MakeShared<FJsonValueNumber>(RenderTarget2D->ClearColor.A));
        ObjectJson->SetArrayField(TEXT("clear_color"), ClearColorValues);
    }
    return ObjectJson;
}

const UObject* GetObjectPropertyForInspection(const UObject* Object, const FName PropertyName)
{
    const FObjectPropertyBase* Property = Object
        ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName)
        : nullptr;
    return Property ? Property->GetObjectPropertyValue_InContainer(Object) : nullptr;
}

TSharedPtr<FJsonObject> BuildDataInterfaceInspectionJson(
    const UNiagaraDataInterface* DataInterface,
    UNiagaraSystem* System = nullptr,
    bool bIncludeProperties = false,
    int32 MaxProperties = 120,
    int32 MaxPropertyValueLength = 512)
{
    TSharedPtr<FJsonObject> DataInterfaceJson = BuildUObjectReferenceJson(DataInterface);
    DataInterfaceJson->SetBoolField(TEXT("is_data_interface"), DataInterface != nullptr);
    DataInterfaceJson->SetBoolField(TEXT("is_render_target_2d_data_interface"), DataInterface && DataInterface->IsA<UNiagaraDataInterfaceRenderTarget2D>());
    DataInterfaceJson->SetBoolField(TEXT("properties_included"), false);
    // GH only calls the original default properties=false path; no private mutable ExportText parent/cast path is introduced.

    const UNiagaraDataInterfaceRenderTarget2D* RenderTargetDataInterface = Cast<UNiagaraDataInterfaceRenderTarget2D>(DataInterface);
    if (RenderTargetDataInterface)
    {
        const FNiagaraVariable& UserParameter = RenderTargetDataInterface->RenderTargetUserParameter.Parameter;
        DataInterfaceJson->SetBoolField(TEXT("inherit_user_parameter_settings"), RenderTargetDataInterface->bInheritUserParameterSettings);
        DataInterfaceJson->SetObjectField(TEXT("render_target_user_parameter"), NiagaraVariableToJsonObject(UserParameter));
        DataInterfaceJson->SetStringField(TEXT("render_target_user_parameter_name"), UserParameter.GetName().ToString());
        DataInterfaceJson->SetBoolField(TEXT("has_render_target_user_parameter"), UserParameter.IsValid());

        const UObject* UserParameterObject = nullptr;
        if (System && UserParameter.IsValid())
        {
            UserParameterObject = System->GetExposedParameters().GetUObject(UserParameter).Get();
        }
        DataInterfaceJson->SetObjectField(TEXT("user_parameter_object"), BuildRenderTargetObjectJson(UserParameterObject));
    }
    return DataInterfaceJson;
}

TSharedPtr<FJsonObject> BuildInputNodeJson(const UNiagaraNodeInput* InputNode)
{
    TSharedPtr<FJsonObject> InputObject = MakeShared<FJsonObject>();
    if (!InputNode)
    {
        return InputObject;
    }

    InputObject->SetStringField(TEXT("node_name"), InputNode->GetName());
    InputObject->SetStringField(TEXT("node_guid"), InputNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    InputObject->SetObjectField(TEXT("variable"), NiagaraVariableToJsonObject(InputNode->Input));
    InputObject->SetNumberField(TEXT("call_sort_priority"), InputNode->CallSortPriority);
    InputObject->SetBoolField(TEXT("is_exposed"), InputNode->IsExposed());
    InputObject->SetBoolField(TEXT("is_required"), InputNode->IsRequired());
    InputObject->SetBoolField(TEXT("is_hidden"), InputNode->IsHidden());
    InputObject->SetBoolField(TEXT("can_auto_bind"), InputNode->CanAutoBind());
    InputObject->SetObjectField(TEXT("object_asset"), BuildRenderTargetObjectJson(GetObjectPropertyForInspection(InputNode, TEXT("ObjectAsset"))));
    const UNiagaraDataInterface* DataInterface = Cast<UNiagaraDataInterface>(GetObjectPropertyForInspection(InputNode, TEXT("DataInterface")));
    if (DataInterface || InputNode->Input.GetType().IsDataInterface())
    {
        InputObject->SetObjectField(TEXT("data_interface"), BuildDataInterfaceInspectionJson(DataInterface));
    }
    InputObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(InputNode->Input.GetName().ToString())));
    return InputObject;
}

TSharedPtr<FJsonObject> BuildOutputNodeJson(const UNiagaraNodeOutput* OutputNode)
{
    TSharedPtr<FJsonObject> OutputObject = MakeShared<FJsonObject>();
    if (!OutputNode)
    {
        return OutputObject;
    }

    OutputObject->SetStringField(TEXT("node_name"), OutputNode->GetName());
    OutputObject->SetStringField(TEXT("node_guid"), OutputNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    OutputObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(OutputNode->GetUsage()));
    OutputObject->SetStringField(TEXT("usage_id"), OutputNode->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
    OutputObject->SetArrayField(TEXT("outputs"), NiagaraVariablesToJson(OutputNode->GetOutputs()));
    return OutputObject;
}

TSharedPtr<FJsonObject> BuildGraphAnalysisJson(
    const UNiagaraScriptSourceBase* SourceBase,
    const FString& Context,
    bool bIncludePins,
    int32 MaxFunctionCalls)
{
    TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
    GraphObject->SetStringField(TEXT("context"), Context);
    GraphObject->SetStringField(TEXT("source_class"), SourceBase ? SourceBase->GetClass()->GetName() : FString());
    GraphObject->SetStringField(TEXT("source_path"), NiagaraObjectPathOrEmpty(SourceBase));

    const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    GraphObject->SetStringField(TEXT("graph_path"), NiagaraObjectPathOrEmpty(Graph));
    GraphObject->SetBoolField(TEXT("has_graph"), Graph != nullptr);

    if (!Graph)
    {
        GraphObject->SetNumberField(TEXT("node_count"), 0);
        GraphObject->SetNumberField(TEXT("function_call_count"), 0);
        GraphObject->SetNumberField(TEXT("input_node_count"), 0);
        GraphObject->SetNumberField(TEXT("output_node_count"), 0);
        GraphObject->SetArrayField(TEXT("function_calls"), TArray<TSharedPtr<FJsonValue>>());
        GraphObject->SetArrayField(TEXT("input_nodes"), TArray<TSharedPtr<FJsonValue>>());
        GraphObject->SetArrayField(TEXT("output_nodes"), TArray<TSharedPtr<FJsonValue>>());
        return GraphObject;
    }

    TArray<UNiagaraNodeFunctionCall*> FunctionCalls;
    TArray<UNiagaraNodeInput*> InputNodes;
    TArray<UNiagaraNodeOutput*> OutputNodes;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
        {
            FunctionCalls.Add(FunctionCall);
        }
        else if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
        {
            InputNodes.Add(InputNode);
        }
        else if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(Node))
        {
            OutputNodes.Add(OutputNode);
        }
    }

    FunctionCalls.Sort(
        [](const UNiagaraNodeFunctionCall& Left, const UNiagaraNodeFunctionCall& Right)
        {
            if (Left.NodePosY == Right.NodePosY)
            {
                return Left.NodePosX < Right.NodePosX;
            }
            return Left.NodePosY < Right.NodePosY;
        });
    InputNodes.Sort(
        [](const UNiagaraNodeInput& Left, const UNiagaraNodeInput& Right)
        {
            return Left.CallSortPriority < Right.CallSortPriority;
        });

    TArray<TSharedPtr<FJsonValue>> FunctionValues;
    TArray<FString> ControlHints;
    const int32 SafeMaxFunctionCalls = FMath::Max(0, MaxFunctionCalls);
    for (int32 Index = 0; Index < FunctionCalls.Num() && Index < SafeMaxFunctionCalls; ++Index)
    {
        TSharedPtr<FJsonObject> FunctionObject = BuildFunctionCallJson(FunctionCalls[Index], Index, bIncludePins);
        for (const TSharedPtr<FJsonValue>& HintValue : FunctionObject->GetArrayField(TEXT("control_hints")))
        {
            FString Hint;
            if (HintValue->TryGetString(Hint))
            {
                AddUniqueString(ControlHints, Hint);
            }
        }
        FunctionValues.Add(MakeShared<FJsonValueObject>(FunctionObject));
    }

    TArray<TSharedPtr<FJsonValue>> InputValues;
    for (const UNiagaraNodeInput* InputNode : InputNodes)
    {
        TSharedPtr<FJsonObject> InputObject = BuildInputNodeJson(InputNode);
        for (const TSharedPtr<FJsonValue>& HintValue : InputObject->GetArrayField(TEXT("control_hints")))
        {
            FString Hint;
            if (HintValue->TryGetString(Hint))
            {
                AddUniqueString(ControlHints, Hint);
            }
        }
        InputValues.Add(MakeShared<FJsonValueObject>(InputObject));
    }

    TArray<TSharedPtr<FJsonValue>> OutputValues;
    for (const UNiagaraNodeOutput* OutputNode : OutputNodes)
    {
        OutputValues.Add(MakeShared<FJsonValueObject>(BuildOutputNodeJson(OutputNode)));
    }

    GraphObject->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
    GraphObject->SetNumberField(TEXT("function_call_count"), FunctionCalls.Num());
    GraphObject->SetNumberField(TEXT("input_node_count"), InputNodes.Num());
    GraphObject->SetNumberField(TEXT("output_node_count"), OutputNodes.Num());
    GraphObject->SetBoolField(TEXT("function_calls_truncated"), FunctionCalls.Num() > SafeMaxFunctionCalls);
    GraphObject->SetArrayField(TEXT("control_hints"), StringsToJson(ControlHints));
    GraphObject->SetArrayField(TEXT("function_calls"), FunctionValues);
    GraphObject->SetArrayField(TEXT("input_nodes"), InputValues);
    GraphObject->SetArrayField(TEXT("output_nodes"), OutputValues);
    return GraphObject;
}

TSharedPtr<FJsonObject> BuildScratchPadScriptJson(
    const UNiagaraScript* Script,
    const FString& OwnerKind,
    bool bIncludePins,
    int32 MaxFunctionCalls)
{
    TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
    if (!Script)
    {
        return ScriptObject;
    }

    ScriptObject->SetStringField(TEXT("name"), Script->GetName());
    ScriptObject->SetStringField(TEXT("path"), Script->GetPathName());
    ScriptObject->SetStringField(TEXT("owner_kind"), OwnerKind);
    ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
    TArray<ENiagaraScriptUsage> SupportedUsageContexts;
    if (const FVersionedNiagaraScriptData* ScriptData = Script->GetLatestScriptData())
    {
        SupportedUsageContexts = ScriptData->GetSupportedUsageContexts();
    }
    ScriptObject->SetArrayField(TEXT("supported_usage_contexts"), NiagaraScriptUsagesToJson(SupportedUsageContexts));
    ScriptObject->SetObjectField(
        TEXT("graph"),
        BuildGraphAnalysisJson(Script->GetLatestSource(), FString::Printf(TEXT("%s_scratch_pad:%s"), *OwnerKind, *Script->GetName()), bIncludePins, MaxFunctionCalls));
    return ScriptObject;
}

void AppendScratchPadContainerScripts(
    const UNiagaraScratchPadContainer* Container,
    const FString& OwnerKind,
    bool bIncludePins,
    int32 MaxFunctionCalls,
    TArray<TSharedPtr<FJsonValue>>& OutScripts)
{
#if WITH_EDITORONLY_DATA
    if (!Container)
    {
        return;
    }

    for (const TObjectPtr<UNiagaraScript> Script : TArray<TObjectPtr<UNiagaraScript>>(Container->Scripts))
    {
        OutScripts.Add(MakeShared<FJsonValueObject>(BuildScratchPadScriptJson(Script.Get(), OwnerKind, bIncludePins, MaxFunctionCalls)));
    }
#endif
}

TSharedPtr<FJsonObject> BuildScratchPadInterfaceJson(
    const UNiagaraScript* Script,
    const FString& OwnerKind,
    const FString& OwnerName,
    int32 OwnerEmitterIndex,
    const FString& ContainerKind,
    int32 ScriptIndex,
    bool bIncludeGraphSummary,
    int32 MaxFunctionCalls)
{
    TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
    if (!Script)
    {
        return ScriptObject;
    }

    TArray<ENiagaraScriptUsage> SupportedUsageContexts;
    if (const FVersionedNiagaraScriptData* ScriptData = Script->GetLatestScriptData())
    {
        SupportedUsageContexts = ScriptData->GetSupportedUsageContexts();
    }

    const FString Context = FString::Printf(
        TEXT("%s:%s:%s"),
        *OwnerKind,
        *ContainerKind,
        *Script->GetName());
    TSharedPtr<FJsonObject> GraphObject = BuildGraphAnalysisJson(
        Script->GetLatestSource(),
        Context,
        false,
        MaxFunctionCalls);

    ScriptObject->SetStringField(TEXT("name"), Script->GetName());
    ScriptObject->SetStringField(TEXT("path"), Script->GetPathName());
    ScriptObject->SetStringField(TEXT("owner_kind"), OwnerKind);
    ScriptObject->SetStringField(TEXT("owner_name"), OwnerName);
    ScriptObject->SetNumberField(TEXT("owner_emitter_index"), OwnerEmitterIndex);
    ScriptObject->SetStringField(TEXT("container_kind"), ContainerKind);
    ScriptObject->SetNumberField(TEXT("script_index"), ScriptIndex);
    ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
    ScriptObject->SetArrayField(TEXT("supported_usage_contexts"), NiagaraScriptUsagesToJson(SupportedUsageContexts));
    ScriptObject->SetBoolField(TEXT("has_graph"), GraphObject->GetBoolField(TEXT("has_graph")));
    ScriptObject->SetNumberField(TEXT("node_count"), GraphObject->GetNumberField(TEXT("node_count")));
    ScriptObject->SetNumberField(TEXT("function_call_count"), GraphObject->GetNumberField(TEXT("function_call_count")));
    ScriptObject->SetNumberField(TEXT("input_count"), GraphObject->GetNumberField(TEXT("input_node_count")));
    ScriptObject->SetNumberField(TEXT("output_count"), GraphObject->GetNumberField(TEXT("output_node_count")));
    const TArray<TSharedPtr<FJsonValue>>* ControlHints = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
    ScriptObject->SetArrayField(TEXT("control_hints"), GraphObject->TryGetArrayField(TEXT("control_hints"), ControlHints) ? *ControlHints : TArray<TSharedPtr<FJsonValue>>());
    ScriptObject->SetArrayField(TEXT("inputs"), GraphObject->TryGetArrayField(TEXT("input_nodes"), Inputs) ? *Inputs : TArray<TSharedPtr<FJsonValue>>());
    ScriptObject->SetArrayField(TEXT("outputs"), GraphObject->TryGetArrayField(TEXT("output_nodes"), Outputs) ? *Outputs : TArray<TSharedPtr<FJsonValue>>());
    ScriptObject->SetBoolField(TEXT("read_only"), true);
    ScriptObject->SetStringField(TEXT("authoring_status"), TEXT("interface_inspection_only"));
    if (bIncludeGraphSummary)
    {
        ScriptObject->SetObjectField(TEXT("graph_summary"), GraphObject);
    }
    return ScriptObject;
}

TSharedPtr<FJsonObject> MakeNiagaraGraphRecipeError(const FString& Message, const FString& ErrorCode)
{
    TSharedPtr<FJsonObject> Error=MakeShared<FJsonObject>();
    Error->SetBoolField(TEXT("success"),false);Error->SetStringField(TEXT("error"),Message);
    Error->SetStringField(TEXT("error_code"), ErrorCode);
    Error->SetBoolField(TEXT("saved"), false);
    Error->SetBoolField(TEXT("mutation_performed"), false);
    Error->SetBoolField(TEXT("mutation_applied_in_memory"), false);
    return Error;
}

bool ResolveSystemScratchPadGraph(
    const TSharedPtr<FJsonObject>& Params,
    FResolvedSystemScratchPadGraph& OutResolved,
    FString& OutError)
{
    FString SystemPath;
    FString ScratchPadName;
    FString ScratchPadScriptPath;
    int32 ScratchPadScriptIndex = INDEX_NONE;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        OutError = TEXT("Missing 'system_path' parameter");
        return false;
    }
    Params->TryGetStringField(TEXT("scratch_pad_name"), ScratchPadName);
    Params->TryGetStringField(TEXT("scratch_pad_script_path"), ScratchPadScriptPath);
    Params->TryGetNumberField(TEXT("scratch_pad_script_index"), ScratchPadScriptIndex);
    ScratchPadName.TrimStartAndEndInline();
    ScratchPadScriptPath.TrimStartAndEndInline();

    if (ScratchPadName.IsEmpty() && ScratchPadScriptPath.IsEmpty() && ScratchPadScriptIndex == INDEX_NONE)
    {
        OutError = TEXT("Provide scratch_pad_name, scratch_pad_script_path, or scratch_pad_script_index");
        return false;
    }

    UNiagaraSystem* System = Current?Current->LoadSystem(SystemPath):nullptr;
    if (!System)
    {
        OutError = FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath);
        return false;
    }

    const FString NormalizedScriptPath = NormalizeNiagaraObjectPathForLoad(ScratchPadScriptPath);
    UNiagaraScript* MatchedScript = nullptr;
    int32 MatchedIndex = INDEX_NONE;
    int32 MatchCount = 0;
    for (int32 Index = 0; Index < System->ScratchPadScripts.Num(); ++Index)
    {
        UNiagaraScript* Candidate = System->ScratchPadScripts[Index].Get();
        if (!Candidate)
        {
            continue;
        }

        const bool bNameMatches = ScratchPadName.IsEmpty() || Candidate->GetName().Equals(ScratchPadName, ESearchCase::IgnoreCase);
        const bool bPathMatches = ScratchPadScriptPath.IsEmpty() || Candidate->GetPathName().Equals(NormalizedScriptPath, ESearchCase::IgnoreCase);
        const bool bIndexMatches = ScratchPadScriptIndex == INDEX_NONE || ScratchPadScriptIndex == Index;
        if (bNameMatches && bPathMatches && bIndexMatches)
        {
            MatchedScript = Candidate;
            MatchedIndex = Index;
            ++MatchCount;
        }
    }

    if (MatchCount != 1 || !MatchedScript)
    {
        OutError = FString::Printf(TEXT("System Scratch Pad selector must resolve exactly once; matched %d"), MatchCount);
        return false;
    }

    UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(MatchedScript->GetLatestSource());
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    if (!Graph)
    {
        OutError = FString::Printf(TEXT("Scratch Pad script has no editable graph: %s"), *MatchedScript->GetPathName());
        return false;
    }

    OutResolved.System = System;
    OutResolved.Script = MatchedScript;
    OutResolved.Source = Source;
    OutResolved.Graph = Graph;
    OutResolved.ScriptIndex = MatchedIndex;
    return true;
}

FString ReadNiagaraActionNameProperty(const UObject* Object, const TCHAR* PropertyName)
{
    if (!Object)
    {
        return FString();
    }
    if (const FNameProperty* Property = FindFProperty<FNameProperty>(Object->GetClass(), PropertyName))
    {
        return Property->GetPropertyValue_InContainer(Object).ToString();
    }
    return FString();
}

FString ReadNiagaraActionStringProperty(const UObject* Object, const TCHAR* PropertyName)
{
    if (!Object)
    {
        return FString();
    }
    if (const FStrProperty* Property = FindFProperty<FStrProperty>(Object->GetClass(), PropertyName))
    {
        return Property->GetPropertyValue_InContainer(Object);
    }
    return FString();
}

bool IsNiagaraGraphRecipeNodeClassAllowed(const FString& ClassName, const FString& FunctionScriptPath)
{
    if (ClassName == TEXT("NiagaraNodeOp") ||
        ClassName == TEXT("NiagaraNodeConvert") ||
        ClassName == TEXT("NiagaraNodeSelect") ||
        ClassName == TEXT("NiagaraNodeReroute"))
    {
        return true;
    }
    if (ClassName == TEXT("NiagaraNodeFunctionCall"))
    {
        return !FunctionScriptPath.IsEmpty() &&
            !Forbidden(FunctionScriptPath);
    }
    return false;
}

FString BuildNiagaraGraphActionCanonical(const FNiagaraAction_NewNode& Action)
{
    UEdGraphNode* Template = Action.WeakNodeTemplate.Get();
    const FString ClassName = Template ? Template->GetClass()->GetName() : FString();
    const FString OpName = ReadNiagaraActionNameProperty(Template, TEXT("OpName"));
    const FString FunctionScriptPath = ReadNiagaraActionNameProperty(Template, TEXT("FunctionScriptAssetObjectPath"));
    const FString Swizzle = ReadNiagaraActionStringProperty(Template, TEXT("AutowireSwizzle"));
    return FString::Printf(
        TEXT("class=%s|op=%s|function=%s|swizzle=%s|display=%s|categories=%s"),
        *ClassName,
        *OpName,
        *FunctionScriptPath,
        *Swizzle,
        *Action.DisplayName.ToString(),
        *FString::Join(Action.Categories, TEXT("/")));
}

FString BuildNiagaraGraphActionId(const FNiagaraAction_NewNode& Action)
{
    return FMD5::HashAnsiString(*BuildNiagaraGraphActionCanonical(Action)).ToLower();
}

TSharedPtr<FJsonObject> NiagaraGraphActionToJson(const FNiagaraAction_NewNode& Action)
{
    UEdGraphNode* Template = Action.WeakNodeTemplate.Get();
    const FString ClassName = Template ? Template->GetClass()->GetName() : FString();
    const FString OpName = ReadNiagaraActionNameProperty(Template, TEXT("OpName"));
    const FString FunctionScriptPath = ReadNiagaraActionNameProperty(Template, TEXT("FunctionScriptAssetObjectPath"));
    const FString Swizzle = ReadNiagaraActionStringProperty(Template, TEXT("AutowireSwizzle"));
    const bool bRecipeSafe = IsNiagaraGraphRecipeNodeClassAllowed(ClassName, FunctionScriptPath);

    TArray<TSharedPtr<FJsonValue>> CategoryValues;
    for (const FString& Category : Action.Categories)
    {
        CategoryValues.Add(MakeShared<FJsonValueString>(Category));
    }

    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("action_id"), BuildNiagaraGraphActionId(Action));
    Object->SetStringField(TEXT("display_name"), Action.DisplayName.ToString());
    Object->SetStringField(TEXT("alternate_search_name"), Action.AlternateSearchName.IsSet() ? Action.AlternateSearchName.GetValue().ToString() : FString());
    Object->SetStringField(TEXT("tooltip"), Action.ToolTip.ToString());
    Object->SetStringField(TEXT("keywords"), Action.Keywords.ToString());
    Object->SetStringField(TEXT("node_class"), ClassName);
    Object->SetStringField(TEXT("op_name"), OpName);
    Object->SetStringField(TEXT("function_script_path"), FunctionScriptPath);
    Object->SetStringField(TEXT("swizzle"), Swizzle);
    Object->SetBoolField(TEXT("recipe_safe"), bRecipeSafe);
    Object->SetArrayField(TEXT("categories"), CategoryValues);
    Object->SetStringField(TEXT("canonical"), BuildNiagaraGraphActionCanonical(Action));
    return Object;
}
bool RawDepth(const FString& S)
{
    int32 Depth=0;bool String=false,Escape=false;for(TCHAR C:S){if(C==0){return false;}if(String){if(Escape){Escape=false;}else if(C=='\\'){Escape=true;}else if(C=='"'){String=false;}continue;}if(C=='"'){String=true;}else if(C=='{'||C=='['){if(++Depth>64){return false;}}else if(C=='}'||C==']'){if(--Depth<0){return false;}}}return !String&&Depth==0;
}

bool FiniteValues(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Slots)
{
    if(!V.IsValid()||Depth>64||++Slots>262144){return false;}if(V->Type==EJson::Number){double X=0;return V->TryGetNumber(X)&&FMath::IsFinite(X);}if(V->Type==EJson::String){for(TCHAR C:V->AsString()){if(C==0){return false;}}return true;}
    if(V->Type==EJson::Array){for(const auto& I:V->AsArray()){if(!FiniteValues(I,Depth+1,Slots)){return false;}}}
    else if(V->Type==EJson::Object){for(const auto& P:V->AsObject()->Values){for(TCHAR C:FString(P.Key)){if(C==0){return false;}}if(!FiniteValues(P.Value,Depth+1,Slots)){return false;}}}return true;
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
        {const FString Key(Reader->GetIdentifier());if(Keys.Last().Contains(Key)){return false;}Keys.Last().Add(Key);}
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        {Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>65){return false;}}
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        {if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()||Values[0]->Type!=EJson::Object){return false;}
    int32 Visits=0;if(!FiniteValues(Values[0],0,Visits)){return false;}Out=Values[0]->AsObject();return Out.IsValid();
}


TSharedPtr<FJsonObject> HandleInspectNiagaraStack(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bIncludePins = false;
    Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);

    int32 MaxFunctionCalls = 200;
    Params->TryGetNumberField(TEXT("max_function_calls"), MaxFunctionCalls);
    MaxFunctionCalls = FMath::Clamp(MaxFunctionCalls, 0, 1000);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> SystemScriptValues;
    if (const UNiagaraScript* SystemSpawnScript = System->GetSystemSpawnScript())
    {
        TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
        ScriptObject->SetStringField(TEXT("name"), SystemSpawnScript->GetName());
        ScriptObject->SetStringField(TEXT("path"), SystemSpawnScript->GetPathName());
        ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(SystemSpawnScript->GetUsage()));
        ScriptObject->SetObjectField(TEXT("graph"), BuildGraphAnalysisJson(SystemSpawnScript->GetLatestSource(), TEXT("system_spawn"), bIncludePins, MaxFunctionCalls));
        SystemScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
    }
    if (const UNiagaraScript* SystemUpdateScript = System->GetSystemUpdateScript())
    {
        TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
        ScriptObject->SetStringField(TEXT("name"), SystemUpdateScript->GetName());
        ScriptObject->SetStringField(TEXT("path"), SystemUpdateScript->GetPathName());
        ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(SystemUpdateScript->GetUsage()));
        ScriptObject->SetObjectField(TEXT("graph"), BuildGraphAnalysisJson(SystemUpdateScript->GetLatestSource(), TEXT("system_update"), bIncludePins, MaxFunctionCalls));
        SystemScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
    }

    TArray<TSharedPtr<FJsonValue>> SystemScratchPadValues;
    for (const TObjectPtr<UNiagaraScript> ScratchPadScript : TArray<TObjectPtr<UNiagaraScript>>(System->ScratchPadScripts))
    {
        SystemScratchPadValues.Add(MakeShared<FJsonValueObject>(BuildScratchPadScriptJson(ScratchPadScript.Get(), TEXT("system"), bIncludePins, MaxFunctionCalls)));
    }

    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    int32 TotalFunctionCallCount = 0;
    int32 TotalScratchPadCount = SystemScratchPadValues.Num();
    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : TArray<FNiagaraEmitterHandle>(System->GetEmitterHandles()))
    {
        TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
        EmitterObject->SetStringField(TEXT("name"), EmitterHandle.GetName().ToString());
        EmitterObject->SetNumberField(TEXT("emitter_index"), EmitterIndex);
        EmitterObject->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());

        TArray<TSharedPtr<FJsonValue>> ScriptValues;
        TArray<TSharedPtr<FJsonValue>> ScratchPadValues;
        TArray<TSharedPtr<FJsonValue>> ParentScratchPadValues;

        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            TSharedPtr<FJsonObject> GraphObject = BuildGraphAnalysisJson(
                EmitterData->GraphSource,
                FString::Printf(TEXT("emitter:%s"), *EmitterHandle.GetName().ToString()),
                bIncludePins,
                MaxFunctionCalls);
            EmitterObject->SetObjectField(TEXT("graph"), GraphObject);
            TotalFunctionCallCount += static_cast<int32>(GraphObject->GetNumberField(TEXT("function_call_count")));

            TArray<UNiagaraScript*> EmitterScripts;
            EmitterData->GetScripts(EmitterScripts, false, false);
            for (const UNiagaraScript* Script : EmitterScripts)
            {
                if (!Script)
                {
                    continue;
                }
                TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
                ScriptObject->SetStringField(TEXT("name"), Script->GetName());
                ScriptObject->SetStringField(TEXT("path"), Script->GetPathName());
                ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
                ScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
            }

            AppendScratchPadContainerScripts(EmitterData->ScratchPads, TEXT("emitter"), bIncludePins, MaxFunctionCalls, ScratchPadValues);
            AppendScratchPadContainerScripts(EmitterData->ParentScratchPads, TEXT("parent_emitter"), bIncludePins, MaxFunctionCalls, ParentScratchPadValues);
        }
        else
        {
            EmitterObject->SetObjectField(TEXT("graph"), BuildGraphAnalysisJson(nullptr, FString::Printf(TEXT("emitter:%s"), *EmitterHandle.GetName().ToString()), bIncludePins, MaxFunctionCalls));
        }

        TotalScratchPadCount += ScratchPadValues.Num() + ParentScratchPadValues.Num();
        EmitterObject->SetArrayField(TEXT("scripts"), ScriptValues);
        EmitterObject->SetArrayField(TEXT("scratch_pad_scripts"), ScratchPadValues);
        EmitterObject->SetArrayField(TEXT("parent_scratch_pad_scripts"), ParentScratchPadValues);
        EmitterValues.Add(MakeShared<FJsonValueObject>(EmitterObject));
        ++EmitterIndex;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetBoolField(TEXT("include_pins"), bIncludePins);
    Result->SetNumberField(TEXT("max_function_calls"), MaxFunctionCalls);
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("system_script_count"), SystemScriptValues.Num());
    Result->SetNumberField(TEXT("system_scratch_pad_count"), SystemScratchPadValues.Num());
    Result->SetNumberField(TEXT("total_scratch_pad_count"), TotalScratchPadCount);
    Result->SetNumberField(TEXT("total_emitter_function_call_count"), TotalFunctionCallCount);
    Result->SetArrayField(TEXT("system_scripts"), SystemScriptValues);
    Result->SetArrayField(TEXT("system_scratch_pad_scripts"), SystemScratchPadValues);
    Result->SetArrayField(TEXT("emitters"), EmitterValues);
    return Result;
}

TSharedPtr<FJsonObject> HandleInspectNiagaraGraph(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bIncludePins = true;
    Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);

    bool bIncludeLinks = true;
    Params->TryGetBoolField(TEXT("include_links"), bIncludeLinks);

    bool bIncludeScratchPads = true;
    Params->TryGetBoolField(TEXT("include_scratch_pads"), bIncludeScratchPads);

    int32 MaxNodesPerGraph = 600;
    Params->TryGetNumberField(TEXT("max_nodes_per_graph"), MaxNodesPerGraph);
    MaxNodesPerGraph = FMath::Clamp(MaxNodesPerGraph, 0, 5000);

    int32 MaxLinksPerGraph = 2000;
    Params->TryGetNumberField(TEXT("max_links_per_graph"), MaxLinksPerGraph);
    MaxLinksPerGraph = FMath::Clamp(MaxLinksPerGraph, 0, 10000);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> SystemScriptValues;
    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    int32 TotalGraphCount = 0;
    int32 TotalScratchPadCount = 0;
    int32 TotalNodeCount = 0;
    int32 TotalLinkCount = 0;

    auto AccumulateGraphCounts = [&TotalNodeCount, &TotalLinkCount](const TSharedPtr<FJsonObject>& GraphObject)
    {
        if (!GraphObject.IsValid())
        {
            return;
        }
        TotalNodeCount += static_cast<int32>(GraphObject->GetNumberField(TEXT("node_count")));
        TotalLinkCount += static_cast<int32>(GraphObject->GetNumberField(TEXT("link_count")));
    };

    if (const UNiagaraScript* SystemSpawnScript = System->GetSystemSpawnScript())
    {
        TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
        ScriptObject->SetStringField(TEXT("name"), SystemSpawnScript->GetName());
        ScriptObject->SetStringField(TEXT("path"), SystemSpawnScript->GetPathName());
        ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(SystemSpawnScript->GetUsage()));
        TSharedPtr<FJsonObject> GraphObject = BuildDetailedGraphJson(
            SystemSpawnScript->GetLatestSource(),
            TEXT("system_spawn"),
            bIncludePins,
            bIncludeLinks,
            MaxNodesPerGraph,
            MaxLinksPerGraph);
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        ScriptObject->SetObjectField(TEXT("graph"), GraphObject);
        AccumulateGraphCounts(GraphObject);
        SystemScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
        ++TotalGraphCount;
    }
    if (const UNiagaraScript* SystemUpdateScript = System->GetSystemUpdateScript())
    {
        TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
        ScriptObject->SetStringField(TEXT("name"), SystemUpdateScript->GetName());
        ScriptObject->SetStringField(TEXT("path"), SystemUpdateScript->GetPathName());
        ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(SystemUpdateScript->GetUsage()));
        TSharedPtr<FJsonObject> GraphObject = BuildDetailedGraphJson(
            SystemUpdateScript->GetLatestSource(),
            TEXT("system_update"),
            bIncludePins,
            bIncludeLinks,
            MaxNodesPerGraph,
            MaxLinksPerGraph);
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        ScriptObject->SetObjectField(TEXT("graph"), GraphObject);
        AccumulateGraphCounts(GraphObject);
        SystemScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
        ++TotalGraphCount;
    }

    TArray<TSharedPtr<FJsonValue>> SystemScratchPadValues;
    if (bIncludeScratchPads)
    {
        for (const TObjectPtr<UNiagaraScript> ScratchPadScript : TArray<TObjectPtr<UNiagaraScript>>(System->ScratchPadScripts))
        {
            SystemScratchPadValues.Add(MakeShared<FJsonValueObject>(
                BuildDetailedScratchPadScriptJson(
                    ScratchPadScript.Get(),
                    TEXT("system"),
                    bIncludePins,
                    bIncludeLinks,
                    MaxNodesPerGraph,
                    MaxLinksPerGraph)));
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        }
    }

    TotalScratchPadCount = SystemScratchPadValues.Num();
    TotalGraphCount += SystemScratchPadValues.Num();

    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : TArray<FNiagaraEmitterHandle>(System->GetEmitterHandles()))
    {
        TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
        EmitterObject->SetStringField(TEXT("name"), EmitterHandle.GetName().ToString());
        EmitterObject->SetNumberField(TEXT("emitter_index"), EmitterIndex);
        EmitterObject->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());

        TArray<TSharedPtr<FJsonValue>> ScratchPadValues;
        TArray<TSharedPtr<FJsonValue>> ParentScratchPadValues;

        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            TSharedPtr<FJsonObject> GraphObject = BuildDetailedGraphJson(
                EmitterData->GraphSource,
                FString::Printf(TEXT("emitter:%s"), *EmitterHandle.GetName().ToString()),
                bIncludePins,
                bIncludeLinks,
                MaxNodesPerGraph,
                MaxLinksPerGraph);
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
            EmitterObject->SetObjectField(TEXT("graph"), GraphObject);
            AccumulateGraphCounts(GraphObject);
            ++TotalGraphCount;

            if (bIncludeScratchPads)
            {
                AppendDetailedScratchPadContainerScripts(
                    EmitterData->ScratchPads,
                    TEXT("emitter"),
                    bIncludePins,
                    bIncludeLinks,
                    MaxNodesPerGraph,
                    MaxLinksPerGraph,
                    ScratchPadValues);
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
                AppendDetailedScratchPadContainerScripts(
                    EmitterData->ParentScratchPads,
                    TEXT("parent_emitter"),
                    bIncludePins,
                    bIncludeLinks,
                    MaxNodesPerGraph,
                    MaxLinksPerGraph,
                    ParentScratchPadValues);
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
            }
        }
        else
        {
            EmitterObject->SetObjectField(
                TEXT("graph"),
                BuildDetailedGraphJson(
                    nullptr,
                    FString::Printf(TEXT("emitter:%s"), *EmitterHandle.GetName().ToString()),
                    bIncludePins,
                    bIncludeLinks,
                    MaxNodesPerGraph,
                    MaxLinksPerGraph));
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        }

        TotalScratchPadCount += ScratchPadValues.Num() + ParentScratchPadValues.Num();
        TotalGraphCount += ScratchPadValues.Num() + ParentScratchPadValues.Num();
        EmitterObject->SetArrayField(TEXT("scratch_pad_scripts"), ScratchPadValues);
        EmitterObject->SetArrayField(TEXT("parent_scratch_pad_scripts"), ParentScratchPadValues);
        EmitterValues.Add(MakeShared<FJsonValueObject>(EmitterObject));
        ++EmitterIndex;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetBoolField(TEXT("include_pins"), bIncludePins);
    Result->SetBoolField(TEXT("include_links"), bIncludeLinks);
    Result->SetBoolField(TEXT("include_scratch_pads"), bIncludeScratchPads);
    Result->SetNumberField(TEXT("max_nodes_per_graph"), MaxNodesPerGraph);
    Result->SetNumberField(TEXT("max_links_per_graph"), MaxLinksPerGraph);
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("system_script_count"), SystemScriptValues.Num());
    Result->SetNumberField(TEXT("system_scratch_pad_count"), SystemScratchPadValues.Num());
    Result->SetNumberField(TEXT("total_scratch_pad_count"), TotalScratchPadCount);
    Result->SetNumberField(TEXT("total_graph_count"), TotalGraphCount);
    Result->SetNumberField(TEXT("total_node_count"), TotalNodeCount);
    Result->SetNumberField(TEXT("total_link_count"), TotalLinkCount);
    Result->SetArrayField(TEXT("system_scripts"), SystemScriptValues);
    Result->SetArrayField(TEXT("system_scratch_pad_scripts"), SystemScratchPadValues);
    Result->SetArrayField(TEXT("emitters"), EmitterValues);
    return Result;
}


// Public owned-template lifecycle follows the SDK spawn order, without UI/undo/grid state.
TSharedPtr<FJsonObject> PreviewNiagaraGraphActionNode(FWork& W,const FNiagaraAction_NewNode& Action,UNiagaraGraph* TemporaryOwner,FString& Unavailable)
{
    Unavailable.Reset();if(!W.Admit(TEXT("action_preview_entry")))return nullptr;
    UEdGraphNode* SourceTemplate=Action.WeakNodeTemplate.Get();
    if(!Live(SourceTemplate)||!Live(TemporaryOwner)||SourceTemplate->GetOuter()!=TemporaryOwner||SourceTemplate->GetGraph()!=TemporaryOwner){W.Fail(TEXT("niagara_graph_action_template_owner_invalid"),TEXT("Action template is not a live member of the exact transient inventory owner."));return nullptr;}
    if(!W.CapturePreview(TemporaryOwner,SourceTemplate))return nullptr;
    const FString Canonical=BuildNiagaraGraphActionCanonical(Action);UClass* const TemplateClass=SourceTemplate->GetClass();
    W.R.Data->SetBoolField(TEXT("template_pin_preview_attempted"),true);
    if(!W.SDK(TEXT("preview_allocate_transient_graph"),true))return nullptr;
    TStrongObjectPtr<UNiagaraGraph> PreviewGraph(NewObject<UNiagaraGraph>(GetTransientPackage(),NAME_None,RF_Transient));
    W.Transient->SetBoolField(TEXT("preview_graph_returned_nonnull"),PreviewGraph.Get()!=nullptr);
    if(!W.After(TEXT("preview_allocate_transient_graph")))return nullptr;
    if(!PreviewGraph.Get()||PreviewGraph->GetClass()!=UNiagaraGraph::StaticClass()||PreviewGraph->GetOuter()!=GetTransientPackage()||PreviewGraph->GetPackage()!=GetTransientPackage()||!PreviewGraph->Nodes.IsEmpty()||!W.Pin(PreviewGraph.Get())){W.Fail(TEXT("niagara_graph_action_preview_graph_invalid"),TEXT("Fresh exact transient preview graph unavailable."));return nullptr;}
    if(!W.SDK(TEXT("preview_duplicate_template"),true))return nullptr;
    TStrongObjectPtr<UEdGraphNode> PreviewNode(Cast<UEdGraphNode>(StaticDuplicateObject(SourceTemplate,PreviewGraph.Get())));
    W.Transient->SetBoolField(TEXT("preview_duplicate_returned_nonnull"),PreviewNode.Get()!=nullptr);
    if(!W.After(TEXT("preview_duplicate_template")))return nullptr;
    if(!PreviewNode.Get()||PreviewNode->GetClass()!=TemplateClass||PreviewNode->GetOuter()!=PreviewGraph.Get()||PreviewNode->GetPackage()!=GetTransientPackage()||!W.Pin(PreviewNode.Get())){W.Fail(TEXT("niagara_graph_action_preview_duplicate_invalid"),TEXT("Exact owned template duplicate unavailable."));return nullptr;}
    bool Added=false;
    auto Owned=[&](const TCHAR* Stage)
    {
        if(!W.Admit(Stage))return false;
        if(Action.WeakNodeTemplate.Get()!=SourceTemplate||!BuildNiagaraGraphActionCanonical(Action).Equals(Canonical,ESearchCase::CaseSensitive)||PreviewNode->GetClass()!=TemplateClass||PreviewNode->GetOuter()!=PreviewGraph.Get()||PreviewNode->GetGraph()!=PreviewGraph.Get()||PreviewGraph->GetPackage()!=GetTransientPackage()||PreviewNode->GetPackage()!=GetTransientPackage()||PreviewGraph->Nodes.Num()!=(Added?1:0)||(Added&&PreviewGraph->Nodes[0].Get()!=PreviewNode.Get()))return W.Fail(TEXT("niagara_graph_action_preview_owner_changed"),TEXT("Exact owned preview/action binding changed during callback."));
        return true;
    };
    if(!Owned(TEXT("preview_preflight")))return nullptr;
    W.R.Data->SetBoolField(TEXT("owned_transient_preview_scope_verified"),true);
    if(auto* NiagaraTemplate=Cast<UNiagaraNode>(PreviewNode.Get()))
    {
        if(!W.SDK(TEXT("preview_CanAddToGraph")))return nullptr;FString Reason;const bool Allowed=NiagaraTemplate->CanAddToGraph(PreviewGraph.Get(),Reason);
        W.Transient->SetBoolField(TEXT("preview_CanAddToGraph_returned"),true);
        if(!W.After(TEXT("preview_CanAddToGraph"))||!Owned(TEXT("preview_after_CanAddToGraph")))return nullptr;
        if(!Allowed){Unavailable=Reason.IsEmpty()?TEXT("Public CanAddToGraph refused this owned preview; no modal shown."):Reason.Left(2048);return nullptr;}
    }
    if(!W.SDK(TEXT("preview_AddNode"),true))return nullptr;PreviewNode->SetFlags(RF_Transient|RF_Transactional);PreviewNode->NodePosX=0;PreviewNode->NodePosY=0;Added=true;PreviewGraph->AddNode(PreviewNode.Get(),false,false);
    W.Transient->SetBoolField(TEXT("preview_AddNode_returned"),true);if(!W.After(TEXT("preview_AddNode"))||!Owned(TEXT("preview_after_AddNode")))return nullptr;
    if(!W.SDK(TEXT("preview_CreateNewGuid"),true))return nullptr;PreviewNode->CreateNewGuid();if(!W.After(TEXT("preview_CreateNewGuid"))||!Owned(TEXT("preview_after_CreateNewGuid")))return nullptr;
    if(!W.SDK(TEXT("preview_PostPlacedNewNode"),true))return nullptr;PreviewNode->PostPlacedNewNode();if(!W.After(TEXT("preview_PostPlacedNewNode"))||!Owned(TEXT("preview_after_PostPlacedNewNode")))return nullptr;
    if(!W.SDK(TEXT("preview_AllocateDefaultPins"),true))return nullptr;PreviewNode->AllocateDefaultPins();if(!W.After(TEXT("preview_AllocateDefaultPins"))||!Owned(TEXT("preview_after_AllocateDefaultPins")))return nullptr;
    if(!W.SDK(TEXT("preview_AutowireNewNode_null"),true))return nullptr;PreviewNode->AutowireNewNode(nullptr);if(!W.After(TEXT("preview_AutowireNewNode_null"))||!Owned(TEXT("preview_after_AutowireNewNode")))return nullptr;
    if(!W.CapturePreview(PreviewGraph.Get(),PreviewNode.Get()))return nullptr;
    auto Result=DetailedGraphNodeToJsonObject(PreviewNode.Get(),INDEX_NONE,true,false);
    if(!W.Admit(TEXT("preview_after_detailed_projection"))||!Owned(TEXT("preview_final_owner")))return nullptr;
    int32 Slots=0;if(!FiniteValues(MakeShared<FJsonValueObject>(Result),0,Slots)||Json(Result).Len()>1024*1024){W.Fail(TEXT("niagara_graph_action_preview_result_bound"),TEXT("Preview exceeds finite/depth64/262144 slots or1Mi UTF16; no public pin truncation."));return nullptr;}
    W.R.Data->SetBoolField(TEXT("owned_transient_preview_scope_verified"),true);return Result;
}

TSharedPtr<FJsonObject> HandleInspectNiagaraGraphActions(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FResolvedSystemScratchPadGraph Resolved;
    FString ResolveError;
    if (!ResolveSystemScratchPadGraph(Params, Resolved, ResolveError))
    {
        return MakeNiagaraGraphRecipeError(ResolveError, TEXT("niagara_graph_action_scratch_pad_resolve_failed"));
    }

    bool bSafeOnly = true;
    bool bIncludeCanonical = false;
    bool bIncludeTemplatePins = false;
    int32 MaxActions = 600;
    FString SearchText;
    Params->TryGetBoolField(TEXT("safe_only"), bSafeOnly);
    Params->TryGetBoolField(TEXT("include_canonical"), bIncludeCanonical);
    Params->TryGetBoolField(TEXT("include_template_pins"), bIncludeTemplatePins);
    Params->TryGetNumberField(TEXT("max_actions"), MaxActions);
    Params->TryGetStringField(TEXT("search_text"), SearchText);
    MaxActions = FMath::Clamp(MaxActions, 0, 5000);
    SearchText.TrimStartAndEndInline();

    UNiagaraGraph* TemporaryOwner = nullptr;
    const TArray<TSharedPtr<FNiagaraAction_NewNode>> Actions = CollectNiagaraGraphActions(Resolved.Graph, TemporaryOwner);
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    TStrongObjectPtr<UNiagaraGraph> TemporaryOwnerGuard(TemporaryOwner);
    TArray<TSharedPtr<FJsonValue>> ActionValues;
    int32 FilteredActionCount = 0;
    int32 SafeActionCount = 0;
    int32 PreviewJsonCharacters = 0;
    W.R.Data->SetBoolField(TEXT("template_pin_preview_requested"),bIncludeTemplatePins);
    for (const TSharedPtr<FNiagaraAction_NewNode>& Action : Actions)
    {
        if (!Action.IsValid() || !Action->WeakNodeTemplate.IsValid())
        {
            continue;
        }
        TSharedPtr<FJsonObject> ActionObject = NiagaraGraphActionToJson(*Action);
        const bool bRecipeSafe = ActionObject->GetBoolField(TEXT("recipe_safe"));
        if (bRecipeSafe)
        {
            ++SafeActionCount;
        }
        if (bSafeOnly && !bRecipeSafe)
        {
            continue;
        }
        if (!SearchText.IsEmpty())
        {
            const FString SearchHaystack = FString::Printf(
                TEXT("%s %s %s %s %s"),
                *ActionObject->GetStringField(TEXT("display_name")),
                *ActionObject->GetStringField(TEXT("node_class")),
                *ActionObject->GetStringField(TEXT("op_name")),
                *ActionObject->GetStringField(TEXT("function_script_path")),
                *ActionObject->GetStringField(TEXT("canonical")));
            if (!SearchHaystack.Contains(SearchText, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }

        ++FilteredActionCount;
        if (!bIncludeCanonical)
        {
            ActionObject->RemoveField(TEXT("canonical"));
        }
        if (ActionValues.Num() < MaxActions)
        {
            if (bIncludeTemplatePins && ActionValues.Num() < 200)
            {
                FString Unavailable;auto TemplateNode=PreviewNiagaraGraphActionNode(W,*Action,TemporaryOwner,Unavailable);
                ActionObject->SetBoolField(TEXT("template_pin_preview_available"),TemplateNode.IsValid());
                if(!Unavailable.IsEmpty())ActionObject->SetStringField(TEXT("template_pin_preview_unavailable_reason"),Unavailable);
                if(TemplateNode.IsValid())
                {
                    const int32 Characters=Json(TemplateNode).Len();if(Characters>4*1024*1024-PreviewJsonCharacters){W.Fail(TEXT("niagara_graph_action_preview_total_bound"),TEXT("Public template previews exceed aggregate4Mi UTF16; no pin truncation or fake availability."));TemplateNode.Reset();ActionObject->SetBoolField(TEXT("template_pin_preview_available"),false);}else{PreviewJsonCharacters+=Characters;ActionObject->SetObjectField(TEXT("template_node"),TemplateNode);}
                }
                if(!W.R.Error.IsEmpty())
                {
                    ActionValues.Add(MakeShared<FJsonValueObject>(ActionObject));auto Partial=MakeNiagaraGraphRecipeError(W.R.Error,W.R.ErrorCode);Partial->SetArrayField(TEXT("actions"),ActionValues);Partial->SetNumberField(TEXT("included_action_count"),ActionValues.Num());Partial->SetBoolField(TEXT("partial_action_projection"),true);return Partial;
                }

            }
            ActionValues.Add(MakeShared<FJsonValueObject>(ActionObject));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetStringField(TEXT("system_path"), Resolved.System->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_script_path"), Resolved.Script->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_name"), Resolved.Script->GetName());
    Result->SetNumberField(TEXT("scratch_pad_script_index"), Resolved.ScriptIndex);
    Result->SetBoolField(TEXT("safe_only"), bSafeOnly);
    Result->SetStringField(TEXT("search_text"), SearchText);
    Result->SetNumberField(TEXT("available_action_count"), Actions.Num());
    Result->SetNumberField(TEXT("safe_action_count"), SafeActionCount);
    Result->SetNumberField(TEXT("filtered_action_count"), FilteredActionCount);
    Result->SetNumberField(TEXT("included_action_count"), ActionValues.Num());
    Result->SetBoolField(TEXT("include_template_pins"), bIncludeTemplatePins);
    Result->SetNumberField(TEXT("template_pin_preview_limit"), 200);
    Result->SetBoolField(TEXT("actions_truncated"), FilteredActionCount > ActionValues.Num());
    Result->SetArrayField(TEXT("actions"), ActionValues);
    Result->SetStringField(TEXT("authoring_boundary"), TEXT("read_only_engine_action_inventory; recipe mutation remains temp-only and allowlisted"));
    return Result;
}

TSharedPtr<FJsonObject> HandleInspectNiagaraCustomHlslNodes(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    FString NodeGuid;
    FString SignatureName;
    FString ContainsText;
    Params->TryGetStringField(TEXT("node_guid"), NodeGuid);
    Params->TryGetStringField(TEXT("signature_name"), SignatureName);
    Params->TryGetStringField(TEXT("contains_text"), ContainsText);
    NodeGuid.TrimStartAndEndInline();
    SignatureName.TrimStartAndEndInline();

    int32 MaxHlslChars = 4000;
    Params->TryGetNumberField(TEXT("max_hlsl_chars"), MaxHlslChars);
    MaxHlslChars = FMath::Clamp(MaxHlslChars, -1, 200000);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> NodeValues;
    int32 SourceCount = 0;
    int32 VisitedNodeCount = 0;
    int32 MatchedNodeCount = 0;
    const TArray<UNiagaraScriptSource*> Sources = CollectTargetNiagaraScriptSources(System);
    for (const UNiagaraScriptSource* Source : Sources)
    {
        if (!Source || !Source->NodeGraph)
        {
            continue;
        }

        ++SourceCount;
        TArray<UNiagaraNodeCustomHlsl*> CustomNodes;
        Source->NodeGraph->GetNodesOfClass<UNiagaraNodeCustomHlsl>(CustomNodes);
        for (int32 NodeIndex = 0; NodeIndex < CustomNodes.Num(); ++NodeIndex)
        {
            UNiagaraNodeCustomHlsl* CustomNode = CustomNodes[NodeIndex];
            ++VisitedNodeCount;
            if (!NiagaraCustomHlslNodeMatches(CustomNode, NodeGuid, SignatureName, ContainsText))
            {
                continue;
            }

            ++MatchedNodeCount;
            NodeValues.Add(MakeShared<FJsonValueObject>(BuildCustomHlslNodeJson(Source, CustomNode, NodeIndex, MaxHlslChars)));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetStringField(TEXT("node_guid_filter"), NodeGuid);
    Result->SetStringField(TEXT("signature_name_filter"), SignatureName);
    Result->SetStringField(TEXT("contains_text_filter"), ContainsText);
    Result->SetNumberField(TEXT("source_count"), SourceCount);
    Result->SetNumberField(TEXT("visited_custom_hlsl_node_count"), VisitedNodeCount);
    Result->SetNumberField(TEXT("matched_custom_hlsl_node_count"), MatchedNodeCount);
    Result->SetArrayField(TEXT("nodes"), NodeValues);
    return Result;
}

TSharedPtr<FJsonObject> HandleInspectNiagaraScratchPadInterface(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bIncludeGraphSummary = true;
    Params->TryGetBoolField(TEXT("include_graph_summary"), bIncludeGraphSummary);

    bool bIncludeParentScratchPads = true;
    Params->TryGetBoolField(TEXT("include_parent_scratch_pads"), bIncludeParentScratchPads);

    int32 MaxScripts = 200;
    Params->TryGetNumberField(TEXT("max_scripts"), MaxScripts);
    MaxScripts = FMath::Clamp(MaxScripts, 0, 1000);

    int32 MaxFunctionCalls = 80;
    Params->TryGetNumberField(TEXT("max_function_calls"), MaxFunctionCalls);
    MaxFunctionCalls = FMath::Clamp(MaxFunctionCalls, 0, 1000);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> FlatScriptValues;
    TArray<TSharedPtr<FJsonValue>> SystemScratchPadValues;
    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    int32 AvailableScratchPadCount = 0;
    int32 IncludedScratchPadCount = 0;
    int32 SystemScratchPadCount = 0;
    int32 EmitterScratchPadCount = 0;
    int32 ParentScratchPadCount = 0;
    int32 TotalInputCount = 0;
    int32 TotalOutputCount = 0;
    bool bScriptsTruncated = false;

    auto AddScratchPadScript = [&](
        const UNiagaraScript* Script,
        const FString& OwnerKind,
        const FString& OwnerName,
        int32 OwnerEmitterIndex,
        const FString& ContainerKind,
        int32 ScriptIndex,
        TArray<TSharedPtr<FJsonValue>>& OwnerScripts)
    {
        if (!Script)
        {
            return;
        }

        ++AvailableScratchPadCount;
        if (IncludedScratchPadCount >= MaxScripts)
        {
            bScriptsTruncated = true;
            return;
        }

        TSharedPtr<FJsonObject> ScriptObject = BuildScratchPadInterfaceJson(
            Script,
            OwnerKind,
            OwnerName,
            OwnerEmitterIndex,
            ContainerKind,
            ScriptIndex,
            bIncludeGraphSummary,
            MaxFunctionCalls);

        TotalInputCount += static_cast<int32>(ScriptObject->GetNumberField(TEXT("input_count")));
        TotalOutputCount += static_cast<int32>(ScriptObject->GetNumberField(TEXT("output_count")));
        ++IncludedScratchPadCount;
        OwnerScripts.Add(MakeShared<FJsonValueObject>(ScriptObject));
        FlatScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
    };

    for (int32 ScriptIndex = 0; ScriptIndex < System->ScratchPadScripts.Num(); ++ScriptIndex)
    {
        ++SystemScratchPadCount;
        AddScratchPadScript(
            System->ScratchPadScripts[ScriptIndex].Get(),
            TEXT("system"),
            System->GetName(),
            INDEX_NONE,
            TEXT("system_scratch_pads"),
            ScriptIndex,
            SystemScratchPadValues);
    }

    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : TArray<FNiagaraEmitterHandle>(System->GetEmitterHandles()))
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
        EmitterObject->SetStringField(TEXT("name"), EmitterName);
        EmitterObject->SetNumberField(TEXT("emitter_index"), EmitterIndex);
        EmitterObject->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());

        TArray<TSharedPtr<FJsonValue>> ScratchPadValues;
        TArray<TSharedPtr<FJsonValue>> ParentScratchPadValues;

        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            if (EmitterData->ScratchPads)
            {
                for (int32 ScriptIndex = 0; ScriptIndex < EmitterData->ScratchPads->Scripts.Num(); ++ScriptIndex)
                {
                    ++EmitterScratchPadCount;
                    AddScratchPadScript(
                        EmitterData->ScratchPads->Scripts[ScriptIndex].Get(),
                        TEXT("emitter"),
                        EmitterName,
                        EmitterIndex,
                        TEXT("emitter_scratch_pads"),
                        ScriptIndex,
                        ScratchPadValues);
                }
            }

            if (bIncludeParentScratchPads && EmitterData->ParentScratchPads)
            {
                for (int32 ScriptIndex = 0; ScriptIndex < EmitterData->ParentScratchPads->Scripts.Num(); ++ScriptIndex)
                {
                    ++ParentScratchPadCount;
                    AddScratchPadScript(
                        EmitterData->ParentScratchPads->Scripts[ScriptIndex].Get(),
                        TEXT("parent_emitter"),
                        EmitterName,
                        EmitterIndex,
                        TEXT("parent_scratch_pads"),
                        ScriptIndex,
                        ParentScratchPadValues);
                }
            }
        }

        EmitterObject->SetNumberField(TEXT("scratch_pad_count"), ScratchPadValues.Num());
        EmitterObject->SetNumberField(TEXT("parent_scratch_pad_count"), ParentScratchPadValues.Num());
        EmitterObject->SetArrayField(TEXT("scratch_pad_scripts"), ScratchPadValues);
        EmitterObject->SetArrayField(TEXT("parent_scratch_pad_scripts"), ParentScratchPadValues);
        EmitterValues.Add(MakeShared<FJsonValueObject>(EmitterObject));
        ++EmitterIndex;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetStringField(TEXT("source_policy"), TEXT("read_only"));
    Result->SetStringField(TEXT("authoring_status"), TEXT("scratch_pad_interface_inspection_only"));
    Result->SetBoolField(TEXT("include_graph_summary"), bIncludeGraphSummary);
    Result->SetBoolField(TEXT("include_parent_scratch_pads"), bIncludeParentScratchPads);
    Result->SetNumberField(TEXT("max_scripts"), MaxScripts);
    Result->SetNumberField(TEXT("max_function_calls"), MaxFunctionCalls);
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("system_scratch_pad_count"), SystemScratchPadCount);
    Result->SetNumberField(TEXT("emitter_scratch_pad_count"), EmitterScratchPadCount);
    Result->SetNumberField(TEXT("parent_scratch_pad_count"), ParentScratchPadCount);
    Result->SetNumberField(TEXT("available_scratch_pad_count"), AvailableScratchPadCount);
    Result->SetNumberField(TEXT("included_scratch_pad_count"), IncludedScratchPadCount);
    Result->SetNumberField(TEXT("total_input_count"), TotalInputCount);
    Result->SetNumberField(TEXT("total_output_count"), TotalOutputCount);
    Result->SetBoolField(TEXT("scripts_truncated"), bScriptsTruncated);
    Result->SetArrayField(TEXT("scratch_pad_scripts"), FlatScriptValues);
    Result->SetArrayField(TEXT("system_scratch_pad_scripts"), SystemScratchPadValues);
    Result->SetArrayField(TEXT("emitters"), EmitterValues);
    return Result;
}

TSharedPtr<FJsonObject> BuildCompileStatusResult(FWork& W,UNiagaraSystem* System,bool bRequestCompile,bool bCompileRequested,bool bWaitForCompletion,double TimeoutSeconds,double PollIntervalSeconds,bool bOutstandingBefore,bool bOutstandingAfterRequest,bool bOutstandingAfter,bool bActiveCompilationsAfter,bool bWaitTimedOut,int32 WaitIterations,double WaitElapsedSeconds)
{
    TArray<TSharedPtr<FJsonValue>> ScriptValues;
    int32 ScriptIndex = 0;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;
    int32 DirtyCount = 0;
    int32 UnknownCount = 0;
    int32 MissingCount = 0;

    auto AddScriptStatus = [&ScriptValues, &ScriptIndex, &ErrorCount, &WarningCount, &DirtyCount, &UnknownCount, &MissingCount](
        const UNiagaraScript* Script,
        const FString& OwnerKind,
        const FString& OwnerName)
    {
        TSharedPtr<FJsonObject> ScriptObject = BuildNiagaraScriptCompileStatusJson(Script, OwnerKind, OwnerName, ScriptIndex++);
        if(!ContinueRead()) return;
        const FString CompileStatus = ScriptObject->GetStringField(TEXT("compile_status"));
        if (ScriptObject->GetBoolField(TEXT("has_error")))
        {
            ++ErrorCount;
        }
        if (ScriptObject->GetBoolField(TEXT("has_warning")))
        {
            ++WarningCount;
        }
        if (CompileStatus == TEXT("NCS_Dirty"))
        {
            ++DirtyCount;
        }
        else if (CompileStatus == TEXT("NCS_Unknown"))
        {
            ++UnknownCount;
        }
        else if (CompileStatus == TEXT("missing"))
        {
            ++MissingCount;
        }
        ScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
    };

    AddScriptStatus(System->GetSystemSpawnScript(), TEXT("system"), TEXT("system_spawn"));
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    AddScriptStatus(System->GetSystemUpdateScript(), TEXT("system"), TEXT("system_update"));
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);

    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : TArray<FNiagaraEmitterHandle>(System->GetEmitterHandles()))
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            TArray<UNiagaraScript*> EmitterScripts;
            EmitterData->GetScripts(EmitterScripts, false, false);
            for (UNiagaraScript* Script : EmitterScripts)
            {
                AddScriptStatus(Script, FString::Printf(TEXT("emitter:%d"), EmitterIndex), EmitterName);
    if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
            }
        }
        ++EmitterIndex;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetBoolField(TEXT("read_only"), !bRequestCompile);
    Result->SetBoolField(TEXT("request_compile"), bRequestCompile);
    Result->SetBoolField(TEXT("compile_requested"), bCompileRequested);
    Result->SetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
    Result->SetBoolField(TEXT("wait_timed_out"), bWaitTimedOut);
    Result->SetNumberField(TEXT("wait_elapsed_seconds"), WaitElapsedSeconds);
    Result->SetNumberField(TEXT("wait_iterations"), WaitIterations);
    Result->SetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
    Result->SetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);
    Result->SetBoolField(TEXT("outstanding_compilation_requests_before"), bOutstandingBefore);
    Result->SetBoolField(TEXT("outstanding_compilation_requests_after_request"), bOutstandingAfterRequest);
    Result->SetBoolField(TEXT("outstanding_compilation_requests_after"), bOutstandingAfter);
    Result->SetBoolField(TEXT("active_compilations_after"), bActiveCompilationsAfter);
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("script_count"), ScriptValues.Num());
    Result->SetNumberField(TEXT("error_count"), ErrorCount);
    Result->SetNumberField(TEXT("warning_count"), WarningCount);
    Result->SetNumberField(TEXT("dirty_count"), DirtyCount);
    Result->SetNumberField(TEXT("unknown_count"), UnknownCount);
    Result->SetNumberField(TEXT("missing_count"), MissingCount);
    Result->SetBoolField(TEXT("has_errors"), ErrorCount > 0);
    Result->SetBoolField(TEXT("has_warnings"), WarningCount > 0);
    Result->SetArrayField(TEXT("scripts"), ScriptValues);
    return Result;
}
TSharedPtr<FJsonObject> HandleInspectNiagaraCompileStatus(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bRequestCompile = false;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    W.R.bCompileRequested=bRequestCompile;

    bool bForce = false;
    Params->TryGetBoolField(TEXT("force"), bForce);

    bool bAllowSourceCompile = false;
    Params->TryGetBoolField(TEXT("allow_source_compile"), bAllowSourceCompile);

    bool bWaitForCompletion = false;
    Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);

    double TimeoutSeconds = 10.0;
    Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
    TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 0.0, 60.0);

    double PollIntervalSeconds = 0.1;
    Params->TryGetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);
    PollIntervalSeconds = FMath::Clamp(PollIntervalSeconds, 0.02, 1.0);

    if (bRequestCompile && !bAllowSourceCompile && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to request Niagara compile outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    if(!W.SDK(TEXT("compile_request_state_before"))) return W.Error(W.R.Error);
    const bool bOutstandingBefore=System->HasOutstandingCompilationRequests(true);const bool bActiveBefore=System->HasActiveCompilations();
    W.Compile->SetBoolField(TEXT("outstanding_before"),bOutstandingBefore);W.Compile->SetBoolField(TEXT("active_before"),bActiveBefore);
    if(!W.After(TEXT("compile_request_state_before"))) return W.Error(W.R.Error);
    if(!W.DeferredMode&&bWaitForCompletion&&(bRequestCompile||bOutstandingBefore||bActiveBefore)){W.Fail(TEXT("niagara_inspection_async_wait_unsupported"),TEXT("Pending or requested compile plus wait requires bounded asynchronous completion/save continuation not implemented by this synchronous inspector. Rejected before RequestCompile/poll/processing SDK. No fake timeout or completion."));return W.Error(W.R.Error);}
    bool bCompileRequested = false;
    if (bRequestCompile)
    {
        if(!W.Request(bForce,bCompileRequested)) return W.Error(W.R.Error);
    }
    if(!W.SDK(TEXT("compile_request_state_after"))) return W.Error(W.R.Error);
    const bool bOutstandingAfterRequest=System->HasOutstandingCompilationRequests(true);
    const bool bOutstandingAfter=bOutstandingAfterRequest;
    const bool bActiveCompilationsAfter=System->HasActiveCompilations();
    W.Compile->SetBoolField(TEXT("outstanding_after"),bOutstandingAfter);W.Compile->SetBoolField(TEXT("active_after"),bActiveCompilationsAfter);
    if(!W.After(TEXT("compile_request_state_after"))) return W.Error(W.R.Error);
    const bool bWaitTimedOut=false;const int32 WaitIterations=0;const double WaitElapsedSeconds=0.0;
    if(W.DeferredMode&&bWaitForCompletion)
    {
        W.WaitDeferred=true;W.WaitRequested=true;W.WaitRequestCompile=bRequestCompile;W.WaitCompileAccepted=bCompileRequested;W.WaitBefore=bOutstandingBefore;W.WaitAfterRequest=bOutstandingAfterRequest;
        W.WaitStarted=FPlatformTime::Seconds();W.WaitTimeout=TimeoutSeconds;W.WaitPoll=PollIntervalSeconds;W.WaitNext=W.WaitStarted;W.WaitIdle=!bOutstandingAfter&&!bActiveCompilationsAfter;
        auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetStringField(TEXT("system_path"),System->GetPathName());Pending->SetBoolField(TEXT("wait_for_completion"),true);Pending->SetBoolField(TEXT("wait_timed_out"),false);return Pending;
    }
    return BuildCompileStatusResult(W,System,bRequestCompile,bCompileRequested,bWaitForCompletion,TimeoutSeconds,PollIntervalSeconds,bOutstandingBefore,bOutstandingAfterRequest,bOutstandingAfter,bActiveCompilationsAfter,bWaitTimedOut,WaitIterations,WaitElapsedSeconds);
}
FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
    bool Success=false;if(Legacy){Legacy->TryGetBoolField(TEXT("success"),Success);}bool ObservationComplete=false;
    if(W.Before){auto After=W.Projection();W.R.Data->SetObjectField(TEXT("before"),W.Before);W.R.Data->SetObjectField(TEXT("after"),After);bool B=false,A=false;W.Before->TryGetBoolField(TEXT("source_projection_complete"),B);After->TryGetBoolField(TEXT("source_projection_complete"),A);if(B&&A){W.R.bChanged|=!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive);ObservationComplete=W.R.Error.IsEmpty()&&W.Admit(TEXT("final_source_observation"));}}
    if(!Success&&W.R.Error.IsEmpty()){FString Error,Code;if(Legacy){Legacy->TryGetStringField(TEXT("error"),Error);Legacy->TryGetStringField(TEXT("error_code"),Code);}if(Error.IsEmpty()){Error=TEXT("Original inspection returned failure; see legacy_result.");}W.Fail(Code.IsEmpty()?TEXT("niagara_inspection_request_failed"):*Code,Error);}
    W.R.bSuccess=Success&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;const bool ScopedKnown=W.R.bSuccess&&W.R.bPostconditionPassed&&ObservationComplete;if(ScopedKnown){W.R.bChangeStateKnown=true;}
    W.R.Data->SetBoolField(TEXT("scoped_source_observation_complete"),ObservationComplete);W.R.Data->SetBoolField(TEXT("scoped_source_change_state_known"),ScopedKnown);W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("postcondition_passed"),W.R.bPostconditionPassed);W.R.Data->SetStringField(TEXT("source_asset_mutation_scope"),TEXT("No explicit asset Modify/property writer/save in the five inspectors. Action inventory creates owned transient templates; loader/getter/cache/global SDK effects remain unverified. Optional RequestCompile is an attempted Engine operation, not guaranteed source purity."));W.R.Data->SetStringField(TEXT("change_state_known_scope"),TEXT("Successful captured source snapshot, exact class/outer/package/world, source/script/emitter/physical graph/node/pin/link/content guards and final admission only; global SDK cache side effects, all source properties, compile/shader/render completion and serialization are outside this scope."));W.R.Data->SetStringField(TEXT("compile_scope"),W.DeferredMode?TEXT("HN observes the original bounded wait on later GT frames with one public poll/process pass per due frame; timeout/error statuses remain valid inspection results, not successful compilation."):TEXT("Requested intent, actual RequestCompile return and outstanding/active/status snapshots are separate. No GT sleep/poll/process/flush or actual pending-wait continuation is implemented. Legacy successTrue with compile errors remains inspection success; it does not claim successful compilation."));W.R.Data->SetStringField(TEXT("transient_scope"),TEXT("The request owns transient action inventory and optional duplicate preview graphs/nodes only until return. Public node lifecycle is invoked without UI selection, modal, ScopedTransaction or grid state; no source graph edit, package save or persistent Cubeless asset dependency. SDK internal effects remain unverified."));W.R.Data->SetBoolField(TEXT("template_pin_preview_implemented"),FString(Operation).Equals(TEXT("inspect_niagara_graph_actions"),ESearchCase::CaseSensitive));W.R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);W.R.Data->SetBoolField(TEXT("transient_creation_only"),FString(Operation).Equals(TEXT("inspect_niagara_graph_actions"),ESearchCase::CaseSensitive)&&!W.R.bCompileAttempted&&!W.R.bSaveAttempted);W.R.Data->SetBoolField(TEXT("pending_compile_wait_implemented"),W.DeferredMode);W.R.Data->SetBoolField(TEXT("actual_render_completion_verified"),false);W.R.Data->SetBoolField(TEXT("Cubeless_runtime_asset_dependency_allowed"),false);W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,bool Writer,FHandler Handler)
{
    FWork W(Writer);TSharedPtr<FJsonObject> Params;if(!ParseParams(Text,Params)){W.Fail(TEXT("niagara_inspection_invalid_params_json"),TEXT("Expected one finite JSON object within1Mi UTF16/depth64/262144 notation tokens and visits, without duplicate case-sensitive keys or NUL."));return Finish(W,W.Error(W.R.Error),Operation);}if(Running){W.Fail(TEXT("niagara_inspection_reentrant_request"),TEXT("A dedicated inspection request is active; no replay."));return Finish(W,W.Error(W.R.Error),Operation);}TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);if(!W.Admit(TEXT("preflight"))){return Finish(W,W.Error(W.R.Error),Operation);}return Finish(W,Handler(W,Params),Operation);
}
}
FString InspectStack(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_stack"),false,&HandleInspectNiagaraStack);}
FString InspectGraph(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_graph"),false,&HandleInspectNiagaraGraph);}
FString InspectGraphActions(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_graph_actions"),false,&HandleInspectNiagaraGraphActions);}
FString InspectCustomHlsl(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_custom_hlsl_nodes"),false,&HandleInspectNiagaraCustomHlslNodes);}
FString InspectScratchInterface(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_scratch_pad_interface"),false,&HandleInspectNiagaraScratchPadInterface);}
FString InspectCompileStatus(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_compile_status"),true,&HandleInspectNiagaraCompileStatus);}
// HN: this concrete controller resumes only this existing leaf. Core/handler/Finish above are shared.
namespace
{
using HNJob=UCubelessNiagaraScratchUserRecipeJob;
namespace HN=NiagaraScratchUserRecipeAsync;
bool HNGlobals(){return IsInGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
struct FHNLeafJob;
TSharedPtr<FHNLeafJob> HNPending;
TArray<TSharedPtr<FHNLeafJob>> HNRetainedUnsafe;
struct FHNLeafJob:TSharedFromThis<FHNLeafJob>
{
    FWork W{true};
    TWeakObjectPtr<HNJob> Job;HNJob* Key=nullptr;FGuid Id=FGuid::NewGuid();FString JobPath,LastJson;TSharedPtr<FJsonObject> Params,Legacy;
    FTSTicker::FDelegateHandle Ticker;int64 Revision=0;uint64 LastFrame=MAX_uint64;bool Executing=false,Dispatched=false,Terminal=false,Quarantined=false;
    const TCHAR* Operation=TEXT("inspect_niagara_compile_status");
    FHNLeafJob(){W.DeferredMode=true;W.R.Data->SetBoolField(TEXT("external_transaction_across_yield_allowed"),false);W.R.Data->SetBoolField(TEXT("all_SDK_calls_nonblocking_proven"),false);W.R.Data->SetBoolField(TEXT("compiler_cancel_attempted"),false);}
    void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
    bool JobExact()const{return HN::CanPublishJobObject()&&Job.Get()==Key&&Live(Key)&&Key->GetClass()==HNJob::StaticClass()&&Key->GetOuter()==GetTransientPackage()&&Key->GetOutermost()==GetTransientPackage()&&Key->GetPackage()==GetTransientPackage()&&Key->HasAnyFlags(RF_Transient)&&Key->GetPathName().Equals(JobPath,ESearchCase::CaseSensitive);}
    void ReleaseExecutionHolds(){if(HN::CanPublishJobObject())for(auto& Identity:W.Objects)Identity->Hold.Reset();}
    bool AcquireExecutionHolds(){if(!W.Admit(TEXT("HN_resume_before_execution_pins")))return false;for(auto& Identity:W.Objects){auto* O=Identity->Object.Get();if(!Live(O))return W.Fail(TEXT("niagara_recipe_wait_object_expired"),TEXT("Captured weak target expired; no reload or retry."));Identity->Hold=TStrongObjectPtr<UObject>(O);}return W.Admit(TEXT("HN_resume_after_execution_pins"));}
    void Prefix(bool Final)
    {
        W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::Digits));W.R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
        W.R.Data->SetBoolField(TEXT("native_job_terminal"),Final);W.R.Data->SetBoolField(TEXT("operation_complete"),Final);W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);W.R.Data->SetBoolField(TEXT("single_core_dispatch"),Dispatched);
        W.R.Data->SetBoolField(TEXT("bounded_wait_policy_observation"),true);W.R.Data->SetBoolField(TEXT("compile_idle_observed_at_wait_end"),W.WaitRequested&&W.WaitIdle);W.R.Data->SetBoolField(TEXT("bounded_wait_timed_out"),W.WaitTimedOut);
        W.R.Data->SetBoolField(TEXT("compiler_may_continue_after_job"),W.R.bCompileAttempted||(W.WaitRequested&&!W.WaitIdle));W.R.Data->SetBoolField(TEXT("successful_compilation_proven"),false);W.R.Data->SetBoolField(TEXT("required_compile_wait_implemented"),true);
        if(Legacy)W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);if(W.Before)W.R.Data->SetObjectField(TEXT("before"),W.Before);
        W.R.Data->SetObjectField(TEXT("actual_compile_prefix"),W.Compile);W.R.Data->SetObjectField(TEXT("transient_work_prefix"),W.Transient);
    }
    void PublishPending()
    {
        W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;Prefix(false);LastJson=Operations::EncodeVariableResult(W.R,Operation);HN::Publish(Key,LastJson,false,JobExact());
    }
    void Quarantine(const FString& Reason)
    {
        if(Terminal||Quarantined)return;Unhook();Quarantined=true;W.Fail(TEXT("niagara_recipe_wait_unsafe_publication"),Reason);W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;
        W.R.Data->SetBoolField(TEXT("native_writers_stopped"),true);W.R.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);W.R.Data->SetBoolField(TEXT("cleanup_complete"),false);Prefix(false);
        LastJson=Operations::EncodeVariableResult(W.R,Operation);HN::Publish(Key,LastJson,false,false);if(!HNRetainedUnsafe.Contains(AsShared()))HNRetainedUnsafe.Add(AsShared());
        // Unsafe: keep prior owners/execution pins and cached plain prefix. Do not pretend final delivery/cleanup.
    }
    void Final()
    {
        if(Terminal||Quarantined)return;if(!JobExact()){Quarantine(TEXT("Job/context cannot safely publish terminal; exact cached prefix retained."));return;}
        bool SDKUnknown=false;W.R.Data->TryGetBoolField(TEXT("SDK_global_purity_unverified"),SDKUnknown);
        if(!W.R.Error.IsEmpty())W.R.bChangeStateKnown=!SDKUnknown&&!W.R.bMutationAttempted&&!W.R.bCompileAttempted&&!W.R.bSaveAttempted;
        Prefix(true);if(!W.R.Error.IsEmpty()){W.R.bSuccess=W.R.bPostconditionPassed=false;auto Skipped=MakeShared<FJsonObject>();Skipped->SetBoolField(TEXT("projection_skipped_after_error"),true);W.R.Data->SetObjectField(TEXT("after"),Skipped);LastJson=Operations::EncodeVariableResult(W.R,Operation);}else LastJson=Finish(W,Legacy?Legacy:W.Error(W.R.Error),Operation);
        if(!JobExact()){Quarantine(TEXT("Context/Job changed during final projection; sealed terminal was not published."));return;}
        Terminal=true;Unhook();HN::Publish(Key,LastJson,true,true);ReleaseExecutionHolds();if(HNPending.Get()==this)HNPending.Reset();
    }
    bool Observe(bool& Outstanding,bool& Active)
    {
        if(!W.SDK(TEXT("HN_HasOutstandingCompilationRequests")))return false;Outstanding=W.System->HasOutstandingCompilationRequests(true);W.R.Data->SetBoolField(TEXT("last_outstanding_compilation_requests"),Outstanding);if(!W.After(TEXT("HN_HasOutstandingCompilationRequests")))return false;
        if(!W.SDK(TEXT("HN_HasActiveCompilations")))return false;Active=W.System->HasActiveCompilations();W.R.Data->SetBoolField(TEXT("last_active_compilations"),Active);return W.After(TEXT("HN_HasActiveCompilations"));
    }
    bool WaitStep()
    {
        bool Outstanding=false,Active=false;if(!Observe(Outstanding,Active))return false;
        if(Outstanding||Active)
        {
            if(!W.SDK(TEXT("HN_PollForCompilationComplete")))return false;W.System->PollForCompilationComplete();if(!W.After(TEXT("HN_PollForCompilationComplete")))return false;
            if(!W.SDK(TEXT("HN_ProcessAsyncTasks")))return false;FAssetCompilingManager::Get().ProcessAsyncTasks(true);if(!W.After(TEXT("HN_ProcessAsyncTasks"))||!Observe(Outstanding,Active))return false;
        }
        W.WaitIdle=!Outstanding&&!Active;
        if(!W.WaitIdle&&FPlatformTime::Seconds()>=W.WaitStarted+W.WaitTimeout)W.WaitTimedOut=true;
        if(W.WaitIdle||W.WaitTimedOut)
        {
            const double Elapsed=FPlatformTime::Seconds()-W.WaitStarted;
            Legacy=BuildCompileStatusResult(W,W.System.Get(),W.WaitRequestCompile,W.WaitCompileAccepted,true,W.WaitTimeout,W.WaitPoll,W.WaitBefore,W.WaitAfterRequest,Outstanding,Active,W.WaitTimedOut,W.WaitIterations,Elapsed);
            if(!W.Admit(TEXT("HN_bounded_wait_terminal_readback")))return false;W.WaitDeferred=false;return true;
        }
        ++W.WaitIterations;W.WaitNext=FPlatformTime::Seconds()+W.WaitPoll;return true;
    }
    void CancelPlain(const FString& Reason)
    {
        if(Terminal||Quarantined)return;W.HNCancelRequested=true;W.HNCancelReason=Reason.Left(8192);
        if(Executing)return;Unhook();W.Fail(TEXT("niagara_recipe_wait_cancelled"),W.HNCancelReason);if(!HN::CanPublishJobObject()){Quarantine(W.HNCancelReason);return;}Legacy=W.Error(W.R.Error);Final();
    }
    bool Step()
    {
        if(Terminal||Quarantined)return false;if(!HNGlobals()){Quarantine(TEXT("Unsafe GT/GC/exit/save boundary; no further UObject access, compiler pump or save."));return false;}
        if(Running&&!Executing){W.Fail(TEXT("niagara_recipe_wait_core_busy"),TEXT("Another request owns this exact core lease; no nested target SDK."));Legacy=W.Error(W.R.Error);Final();return false;}
        if(HN::IsStopping()){CancelPlain(TEXT("HN shutdown; no asset replay."));return false;}if(Executing||LastFrame==GFrameCounter)return true;LastFrame=GFrameCounter;
        if(!JobExact()){Quarantine(TEXT("Exact transient Job changed or expired."));return false;}
        if(W.HNCancelRequested){CancelPlain(W.HNCancelReason);return false;}if(!W.R.Error.IsEmpty()){Final();return false;}
        if(Dispatched&&W.WaitDeferred&&FPlatformTime::Seconds()<W.WaitNext)return true;
        TGuardValue<bool> Execution(Executing,true);TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);
        if(!AcquireExecutionHolds()){if(!HNGlobals())Quarantine(W.R.Error);else{Legacy=W.Error(W.R.Error);Final();}return false;}
        if(!Dispatched){Dispatched=true;Legacy=HandleInspectNiagaraCompileStatus(W,Params);}
        else if(W.WaitDeferred&&!WaitStep()){if(!HNGlobals())Quarantine(W.R.Error);else{Legacy=W.Error(W.R.Error);Final();}return false;}
        if(!HNGlobals()){Quarantine(TEXT("SDK returned across unsafe lifetime; actual prefix cached without publication/cleanup."));return false;}
        if(!W.R.Error.IsEmpty()||W.HNCancelRequested){Legacy=W.Error(W.HNCancelRequested?W.HNCancelReason:W.R.Error);Final();return false;}
        if(!W.WaitDeferred){Final();return false;}PublishPending();ReleaseExecutionHolds();return !Quarantined;
    }
};
}
UCubelessNiagaraScratchUserRecipeJob* StartCompileStatusContinuation(const FString& Text)
{
    if(!HNGlobals()||NiagaraScratchUserRecipeAsync::IsStopping())return nullptr;
    auto State=MakeShared<FHNLeafJob>();bool Parsed=ParseParams(Text,State->Params);if(!Parsed)State->W.Fail(TEXT("niagara_recipe_wait_invalid_params_json"),TEXT("Expected original bounded finite leaf params object."));
    if(Parsed){FString Path;State->Params->TryGetStringField(TEXT("system_path"),Path);const FString Expanded=NormalizeNiagaraObjectPathForLoad(Path);if(Path.Len()>=NAME_SIZE||Expanded.Len()>=NAME_SIZE)State->W.Fail(TEXT("niagara_recipe_wait_path_bound"),TEXT("Raw/expanded FName path must fit NAME_SIZE before any target load."));}
    if(Running)State->W.Fail(TEXT("niagara_recipe_wait_core_busy"),TEXT("Existing synchronous/deferred core lease is active; no nested dispatch."));
    if(HNPending.IsValid())State->W.Fail(TEXT("niagara_recipe_wait_leaf_busy"),TEXT("This exact leaf controller already has a pending/quarantined invocation; no replay."));
    if(State->W.R.Error.IsEmpty()&&!State->W.Admit(TEXT("HN_before_job_creation"))){}
    auto* Created=NewObject<UCubelessNiagaraScratchUserRecipeJob>(GetTransientPackage(),NAME_None,RF_Transient);State->Key=Created;State->Job=Created;if(!HNGlobals()||!Live(Created))return nullptr;State->JobPath=Created->GetPathName();NiagaraScratchUserRecipeAsync::RegisterJob(Created,NiagaraScratchUserRecipeAsync::EJobKind::CompileStatus,State->Id);
    if(!State->W.R.Error.IsEmpty()){State->Legacy=State->W.Error(State->W.R.Error);State->Final();return Created;}
    HNPending=State;State->PublishPending();State->Step();if(!State->Terminal&&!State->Quarantined){TWeakPtr<FHNLeafJob> Weak=State;State->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float){auto Strong=Weak.Pin();return Strong.IsValid()&&Strong->Step();}));}return Created;
}
bool CancelCompileStatusContinuation(UCubelessNiagaraScratchUserRecipeJob* Job,const FString& Reason)
{if(!IsInGameThread()||!HNPending.IsValid()||HNPending->Key!=Job)return false;auto State=HNPending;State->CancelPlain(Reason);return true;}
void ShutdownCompileStatusContinuation(const FString& Reason)
{if(!IsInGameThread())return;if(auto State=HNPending){State->Unhook();State->CancelPlain(Reason);}}

}
