
// GL dedicated Editor-only Niagara User input paths. Source candidate; no Engine/build evidence.
#include "Compatibility/UE58/CubelessNiagaraModuleInputPathApi.h"
#include "Compatibility/UE58/CubelessNiagaraScratchUserRecipeAsyncApi.h"
#include "CubelessNiagaraScratchUserRecipeJob.h"
#include "Containers/Ticker.h"
#include "AssetCompilingManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/SavePackage.h"
namespace Cubeless::UE58::NiagaraScratchUserRecipeAsync { bool CanPublishJobObject(); }

#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSimulationStageCompileData.h"
#include "NiagaraParameterBinding.h"
#include "NiagaraParameterStore.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeWithDynamicPins.h"
#include "NiagaraNodeCustomHlsl.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "NiagaraParameterMapHistory.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraDataInterfaceArrayInt.h"
#include "NiagaraDataInterfaceArray.h"
#include "Misc/SecureHash.h"
#include "NiagaraSystemEditorData.h"
#include "EdGraphSchema_Niagara.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::NiagaraModuleInputPaths
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
FString NormalizeNiagaraObjectPathForLoad(const FString&);
struct FIdentity
{
    TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer,Root;TWeakObjectPtr<UPackage> Package,EffectivePackage;TWeakObjectPtr<UClass> Class;FString Path,RetiredRelativePath;TWeakObjectPtr<UObject> RetiredRoot;bool Writer=false,Retired=false;
    explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Package(O->GetOutermost()),EffectivePackage(O->GetPackage()),Class(O->GetClass()),Path(O->GetPathName()){}
    bool Exact()const{auto* O=Object.Get();if(!Live(O)||O->GetClass()!=Class.Get())return false;if(Retired){auto* RetiredOwner=RetiredRoot.Get();if(!Live(RetiredOwner)||RetiredOwner->GetOuter()!=GetTransientPackage()||RetiredOwner->GetPackage()!=GetTransientPackage()||O->GetOutermost()!=GetTransientPackage()||O->GetPackage()!=GetTransientPackage())return false;if(O==RetiredOwner)return true;return O->GetOuter()==Outer.Get()&&O->IsIn(RetiredOwner)&&O->GetPathName(RetiredOwner).Equals(RetiredRelativePath,ESearchCase::CaseSensitive);}if(!Live(Package.Get())||!Live(EffectivePackage.Get())||O->GetPackage()!=EffectivePackage.Get()||O->GetOuter()!=Outer.Get()||O->GetOutermost()!=Package.Get()||!O->GetPathName().Equals(Path,ESearchCase::CaseSensitive))return false;auto* R=Root.Get();return !Writer||(Live(R)&&(O==R||O->IsIn(R))&&O->GetOutermost()==R->GetOutermost()&&O->GetPackage()==R->GetPackage());}
};
struct FPinIdentity
{UEdGraphPin* Pointer=nullptr;FGuid Id,PersistentId;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;TArray<UEdGraphPin*> Links;FString DefaultValue,AutoDefault,DefaultText;TWeakObjectPtr<UObject> DefaultObject;};
struct FNodeIdentity
{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;int32 X=0,Y=0;TArray<FPinIdentity> Pins;};
FString GraphMetadataSignature(UNiagaraGraph* G);
struct FGraphIdentity
{TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNodeIdentity> Nodes;FString Meta;};
struct FSourceIdentity
{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;};
struct FEmitterIdentity
{FGuid Id,InstanceVersion,DataVersion;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,ParentScratch;TArray<TWeakObjectPtr<UNiagaraScript>> ScratchScripts,ParentScripts;TArray<TWeakObjectPtr<UNiagaraSimulationStageBase>> Stages;};
struct FScriptBinding{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;};
struct FStageBinding{TWeakObjectPtr<UNiagaraSimulationStageBase> Stage;TWeakObjectPtr<UNiagaraScript> Script;FName Name;};
struct FWork
{
    // HN phase state. Original handler core executes once; only public compile observation resumes.
    bool DeferredMode=false,WaitDeferred=false,WaitRequested=false,WaitTimedOut=false,WaitIdle=true,HNCancelRequested=false;
    bool WaitRequestCompile=false,WaitCompileAccepted=false,WaitBefore=false,WaitAfterRequest=false;
    FString HNCancelReason;double WaitStarted=0.0,WaitTimeout=10.0,WaitPoll=0.1,WaitNext=0.0;int32 WaitIterations=0;
    TSharedPtr<FJsonObject> WaitObject;
    bool GuardedRemoval=false,RemovalMutationStarted=false;TFunction<TSharedPtr<FJsonObject>()> RemovalTail;TFunction<bool()> RemovalReadAdmission;TFunction<bool(UEdGraphNode*,const FGuid&)> RemovalPairPlan;

    FResult R;FString FailedStage;TWeakObjectPtr<UEditorEngine> CapturedEditor;TWeakObjectPtr<UWorld> World;bool WorldCaptured=false,SaveHealth=true,WriterIntent=true;
    TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<TWeakObjectPtr<UNiagaraScript>> Scratch;TArray<FEmitterIdentity> Emitters;
    TArray<FScriptBinding> ScriptBindings;TArray<FStageBinding> StageBindings;TArray<TUniquePtr<FIdentity>> Objects;TArray<FGraphIdentity> Graphs;TArray<FSourceIdentity> Sources;
    UNiagaraGraph* TransitionGraph=nullptr;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Saves,Compiles,Partial,MutationRows;TSharedPtr<FJsonObject> Before;int64 Slots=0,TextUnits=0,Calls=0;
    FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty())Fail(TEXT("niagara_input_path_request_failed"),Message);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);if(GuardedRemoval&&RemovalMutationStarted){O->SetStringField(TEXT("error_code"),R.ErrorCode);O->SetBoolField(TEXT("mutation_applied_in_memory"),true);O->SetBoolField(TEXT("requires_reload_or_undo"),true);O->SetBoolField(TEXT("saved"),false);}return O;}
    bool Budget(int64 N,int64 Text=0){Slots+=N;TextUnits+=Text;Prefix->SetNumberField(TEXT("captured_slots"),static_cast<double>(Slots));Prefix->SetNumberField(TEXT("observed_text_utf16_units"),static_cast<double>(TextUnits));return (Slots<=262144&&TextUnits<=32ll*1024*1024)||Fail(TEXT("niagara_input_path_budget_exceeded"),TEXT("Bounded capture/output budget exceeded; partial prefix retained."));}
    bool Pin(UObject* O){if(!Live(O))return Fail(TEXT("niagara_input_path_object_unavailable"),TEXT("Expected live loaded object."));for(const auto& I:Objects){if(I->Object.Get()==O)return I->Exact()||Fail(TEXT("niagara_input_path_identity_changed"),TEXT("Captured object changed."));}if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool Own(UObject* O)
    {auto* Root=System.Get();if(!Admit(TEXT("writer_ownership"))||!Live(O)||!Live(Root)||(O!=Root&&!O->IsIn(Root))||O->GetOutermost()!=Root->GetOutermost()||O->GetPackage()!=Root->GetPackage())return Fail(TEXT("niagara_input_path_external_writer_unsupported"),TEXT("Actual writer is outside the specified System outer chain/package. No external emitter/shared graph/source was changed."));if(!Pin(O))return false;for(auto& I:Objects){if(I->Object.Get()==O){I->Writer=true;I->Root=Root;}}return true;}
    bool Admit(const FString& Stage)
    {
        R.Stage=Stage;if(HNCancelRequested)return Fail(TEXT("niagara_recipe_wait_cancelled"),HNCancelReason);if(!R.Error.IsEmpty())return false;
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||UE::IsSavingPackage())return Fail(TEXT("niagara_input_path_context_unavailable"),TEXT("Live Editor game thread without GC/exit/save required; no replay."));
        if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_input_path_editor_changed"),TEXT("Captured Editor instance changed before deferred SDK admission."));
        if(WriterIntent&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext()))return Fail(TEXT("niagara_input_path_play_session_active"),TEXT("Niagara cleanup requires stopped PIE/SIE."));
        UWorld* CurrentWorld=GEditor->GetEditorWorldContext().World();if(!Live(CurrentWorld)||CurrentWorld->bIsTearingDown)return Fail(TEXT("niagara_input_path_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){CapturedEditor=GEditor;World=CurrentWorld;WorldCaptured=true;}else if(CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_input_path_editor_changed"),TEXT("Captured Editor instance changed across SDK dispatch."));else if(World.Get()!=CurrentWorld)return Fail(TEXT("niagara_input_path_world_changed"),TEXT("Editor world changed across SDK dispatch."));
        for(const auto& I:Objects){if(!I->Exact())return Fail(TEXT("niagara_input_path_identity_changed"),TEXT("Captured class/outer/package/path/ownership changed; no replacement adopted."));}
        if(System.IsValid())
        {auto* O=System.Get();if(O->GetSystemSpawnScript()!=Spawn.Get()||O->GetSystemUpdateScript()!=Update.Get()||O->ScratchPadScripts.Num()!=Scratch.Num()||O->GetEmitterHandles().Num()!=Emitters.Num())return Fail(TEXT("niagara_input_path_system_topology_changed"),TEXT("Captured System emitter/script topology changed."));
            for(int32 I=0;I<Scratch.Num();++I)if(O->ScratchPadScripts[I]!=Scratch[I].Get())return Fail(TEXT("niagara_input_path_scratch_membership_changed"),TEXT("System scratch membership changed."));
            for(int32 I=0;I<Emitters.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=Emitters[I];if(H.GetId()!=E.Id||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.InstanceVersion||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion))return Fail(TEXT("niagara_input_path_emitter_version_changed"),TEXT("Physical emitter/ref/version changed."));
                if(E.Data){if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.ParentScratch.Get())return Fail(TEXT("niagara_input_path_emitter_routing_changed"),TEXT("Emitter source/scratch binding changed."));
                    auto Same=[](const UNiagaraScratchPadContainer* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& P){if(!C)return P.IsEmpty();if(C->Scripts.Num()!=P.Num())return false;for(int32 J=0;J<P.Num();++J)if(C->Scripts[J]!=P[J].Get())return false;return true;};
                    if(!Same(E.Scratch.Get(),E.ScratchScripts)||!Same(E.ParentScratch.Get(),E.ParentScripts))return Fail(TEXT("niagara_input_path_scratch_membership_changed"),TEXT("Emitter scratch membership changed."));
                    const auto& A=E.Data->GetSimulationStages();if(A.Num()!=E.Stages.Num())return Fail(TEXT("niagara_input_path_stage_membership_changed"),TEXT("Simulation stage count changed."));for(int32 J=0;J<A.Num();++J)if(A[J]!=E.Stages[J].Get())return Fail(TEXT("niagara_input_path_stage_membership_changed"),TEXT("Simulation stage order changed."));
                }
            }
        }
        for(const auto& S:ScriptBindings){auto* O=S.Script.Get();if(!Live(O)||O->GetLatestSource()!=S.Source.Get()||O->GetExposedVersion().VersionGuid!=S.Version)return Fail(TEXT("niagara_input_path_script_source_changed"),TEXT("Script source/exposed version changed."));}
        for(const auto& S:StageBindings){auto* O=S.Stage.Get();if(!Live(O)||O->Script!=S.Script.Get()||O->SimulationStageName!=S.Name)return Fail(TEXT("niagara_input_path_stage_script_changed"),TEXT("Stage name/script binding changed."));}
        for(const auto& S:Sources){if(!Live(S.Source.Get())||S.Source->NodeGraph!=S.Graph.Get())return Fail(TEXT("niagara_input_path_source_graph_changed"),TEXT("Captured source graph binding changed."));}
        for(const auto& G:Graphs){if(G.Graph.Get()==TransitionGraph)continue;auto* O=G.Graph.Get();if(!Live(O)||O->Nodes.Num()!=G.Nodes.Num()||!GraphMetadataSignature(O).Equals(G.Meta,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_input_path_graph_topology_changed"),TEXT("Graph node count changed outside the planned removal."));
            for(int32 I=0;I<G.Nodes.Num();++I){const auto& E=G.Nodes[I];auto* N=E.Node.Get();if(!Live(N)||O->Nodes[I]!=N||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num())return Fail(TEXT("niagara_input_path_node_identity_changed"),TEXT("Captured node/pin membership changed."));
                for(int32 J=0;J<E.Pins.Num();++J){const auto& P=E.Pins[J];if(N->Pins[J]!=P.Pointer)return Fail(TEXT("niagara_input_path_pin_identity_changed"),TEXT("Captured pin disappeared; stale pointer not dereferenced."));if(P.Pointer&&(P.Pointer->PinId!=P.Id||(GuardedRemoval&&P.Pointer->PersistentGuid!=P.PersistentId)||P.Pointer->GetOwningNodeUnchecked()!=N||!P.Pointer->PinName.ToString().Equals(P.Name.ToString(),ESearchCase::CaseSensitive)||P.Pointer->Direction!=P.Direction||P.Pointer->PinType!=P.Type||P.Pointer->LinkedTo!=P.Links||!P.Pointer->DefaultValue.Equals(P.DefaultValue,ESearchCase::CaseSensitive)||!P.Pointer->AutogeneratedDefaultValue.Equals(P.AutoDefault,ESearchCase::CaseSensitive)||!P.Pointer->DefaultTextValue.ToString().Equals(P.DefaultText,ESearchCase::CaseSensitive)||P.Pointer->DefaultObject!=P.DefaultObject.Get()))return Fail(TEXT("niagara_input_path_pin_links_changed"),TEXT("Pin identity/type/links differ from expected admitted transition."));}
            }
        }if(RemovalReadAdmission&&!RemovalReadAdmission())return Fail(TEXT("niagara_unused_module_input_authored_state_changed"),TEXT("Captured HLSL body/signature changed outside the explicit removal plan; no subsequent SDK."));return true;
    }
    TSharedRef<FJsonObject> MutationRow(const TCHAR* Stage){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetBoolField(TEXT("SDK_returned"),false);MutationRows.Add(MakeShared<FJsonValueObject>(Row));return Row;}
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage))return false;if(!Budget(1,FCString::Strlen(Stage)))return false;if(++Calls>262144)return Fail(TEXT("niagara_input_path_sdk_budget"),TEXT("SDK call budget exceeded."));R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(Mutation)Prefix->SetBoolField(Stage,true);return true;}
    bool After(const TCHAR* Stage){Prefix->SetBoolField(FString(Stage)+TEXT("_SDK_returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
    template<typename F>bool Step(const TCHAR* Stage,bool Mutation,F&& Fnc){if(!SDK(Stage,Mutation))return false;Fnc();return After(Stage);}
    bool Modify(UObject* O){if(!Own(O))return false;Prefix->SetStringField(TEXT("last_modify_object"),O->GetPathName());return Step(TEXT("Modify"),true,[&]{O->Modify();});}
    bool Dirty(){return Step(TEXT("MarkPackageDirty"),true,[&]{System->MarkPackageDirty();});}
    bool CaptureScript(UNiagaraScript* O){if(!O)return true;if(!Pin(O))return false;for(const auto& I:ScriptBindings)if(I.Script.Get()==O)return true;ScriptBindings.Add({O,O->GetLatestSource(),O->GetExposedVersion().VersionGuid});return true;}
    bool CaptureSystem(UNiagaraSystem* O);
    bool CaptureGraph(UNiagaraScriptSource* S);
    bool GraphForWrite(UNiagaraGraph* G);
    bool PinForWrite(UEdGraphPin* P);
    bool BreakLinks(UEdGraphPin* P,bool Notify);
    bool LinkPins(UEdGraphPin* A,UEdGraphPin* B);
    bool RemoveNode(UNiagaraGraph* G,UEdGraphNode* N,bool BreakAllLinks=true,bool AlwaysMarkDirty=true,bool Destroy=false);
    bool Request(bool& Accepted);
    bool Save(bool Direct,FString* Filename=nullptr);
    bool WaitAdmission(bool Wait,bool WillRequest);
    TSharedPtr<FJsonObject> WaitSnapshot(bool Wait,double Timeout,double Poll);
    TSharedPtr<FJsonObject> Projection();
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("continuation"));}
#define GL_STEP(Stage,Mutation,Expr) do{if(!W.Step(TEXT(Stage),Mutation,[&]{Expr;}))return W.Error(W.R.Error);}while(false)

bool FWork::CaptureSystem(UNiagaraSystem* O)
{
    if(!Pin(O)||!Pin(O->GetOutermost()))return false;System=O;Spawn=O->GetSystemSpawnScript();Update=O->GetSystemUpdateScript();if(!CaptureScript(Spawn.Get())||!CaptureScript(Update.Get()))return false;
    for(UNiagaraScript* S:O->ScratchPadScripts){if(S&&!CaptureScript(S))return false;Scratch.Add(S);}
    if(!Budget(O->GetEmitterHandles().Num())||O->GetEmitterHandles().Num()>4096)return Fail(TEXT("niagara_input_path_emitter_bound"),TEXT("Emitter inventory exceeds bound."));
    for(const auto& H:O->GetEmitterHandles()){FEmitterIdentity E;E.Id=H.GetId();E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Emitter.IsValid()&&!Pin(E.Emitter.Get()))return false;if(E.Data){E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.ParentScratch=E.Data->ParentScratchPads;if(E.Source.IsValid()&&!Pin(E.Source.Get()))return false;
        auto CaptureScripts=[&](UNiagaraScratchPadContainer* C,TArray<TWeakObjectPtr<UNiagaraScript>>& A){if(!C)return true;if(!Pin(C)||!Budget(C->Scripts.Num()))return false;for(UNiagaraScript* S:C->Scripts){if(S&&!CaptureScript(S))return false;A.Add(S);}return true;};
        if(!CaptureScripts(E.Scratch.Get(),E.ScratchScripts)||!CaptureScripts(E.ParentScratch.Get(),E.ParentScripts))return false;
        TArray<UNiagaraScript*> OwnedScripts;E.Data->GetScripts(OwnedScripts,false,false);if(!Budget(OwnedScripts.Num()))return false;for(UNiagaraScript* P:OwnedScripts)if(!CaptureScript(P))return false;
        if(!Budget(E.Data->GetSimulationStages().Num()))return false;for(UNiagaraSimulationStageBase* S:E.Data->GetSimulationStages()){if(S&&(!Pin(S)||!CaptureScript(S->Script)))return false;if(S)StageBindings.Add({S,S->Script,S->SimulationStageName});E.Stages.Add(S);}
    }Emitters.Add(MoveTemp(E));}return true;
}
UNiagaraSystem* LoadSystem(const FString& Path)
{
    if(!Current->SDK(TEXT("LoadObject_System")))return nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* O=LoadObject<UNiagaraSystem>(nullptr,*Expected);Current->Prefix->SetBoolField(TEXT("load_returned_nonnull"),O!=nullptr);if(!Current->After(TEXT("LoadObject_System")))return nullptr;if(O&&(!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Current->CaptureSystem(O))){Current->Fail(TEXT("niagara_input_path_load_identity_unexpected"),TEXT("Loaded System differs from requested normalized exact path."));return nullptr;}return O;
}
bool FWork::CaptureGraph(UNiagaraScriptSource* S)
{
    if(!Admit(TEXT("capture_graph")))return false;if(!S||!S->NodeGraph)return true;if(!Pin(S)||!Pin(S->NodeGraph))return false;auto* G=S->NodeGraph.Get();
    bool Bound=false;for(const auto& E:Sources)if(E.Source.Get()==S){Bound=true;if(E.Graph.Get()!=G)return Fail(TEXT("niagara_input_path_source_graph_changed"),TEXT("Source graph changed."));}
    if(!Bound)Sources.Add({S,G});for(const auto& E:Graphs)if(E.Graph.Get()==G)return true;
    if(!Budget(G->Nodes.Num())||G->Nodes.Num()>16384)return Fail(TEXT("niagara_input_path_graph_bound"),TEXT("Graph node inventory exceeds bound."));
    FGraphIdentity E;E.Graph=G;E.Meta=GraphMetadataSignature(G);
    for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N)||N->GetGraph()!=G)return Fail(TEXT("niagara_input_path_node_owner_invalid"),TEXT("Null/foreign graph node cannot be safely inspected or mutated."));FNodeIdentity NE;NE.Node=N;NE.Id=N->NodeGuid;NE.X=N->NodePosX;NE.Y=N->NodePosY;if(!Budget(N->Pins.Num()))return false;
        for(UEdGraphPin* P:N->Pins){if(!P)return Fail(TEXT("niagara_input_path_null_pin"),TEXT("Null graph pin unsupported by Niagara public getters."));if(P->GetOwningNodeUnchecked()!=N||!Budget(P->LinkedTo.Num()))return Fail(TEXT("niagara_input_path_pin_owner_invalid"),TEXT("Pin owner/link inventory is invalid."));FPinIdentity PE;PE.Pointer=P;PE.Id=P->PinId;PE.PersistentId=P->PersistentGuid;PE.Name=P->PinName;PE.Direction=P->Direction;PE.Type=P->PinType;PE.Links=P->LinkedTo;PE.DefaultValue=P->DefaultValue;PE.AutoDefault=P->AutogeneratedDefaultValue;PE.DefaultText=P->DefaultTextValue.ToString();PE.DefaultObject=P->DefaultObject;if(P->DefaultObject&&!Pin(P->DefaultObject))return false;NE.Pins.Add(MoveTemp(PE));}
        E.Nodes.Add(MoveTemp(NE));}
    TSet<UEdGraphPin*> Known;for(const auto& N:E.Nodes)for(const auto& P:N.Pins)Known.Add(P.Pointer);for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links){if(!L||!Known.Contains(L))return Fail(TEXT("niagara_input_path_unresolved_link_identity"),TEXT("Null/cross-graph link is outside captured exact pin membership; no stale link dereference."));}
    Graphs.Add(MoveTemp(E));return true;
}
bool FWork::GraphForWrite(UNiagaraGraph* G)
{
    if(!Admit(TEXT("graph_writer_admission"))||!Own(G))return false;bool Captured=false;
    for(const auto& S:Sources){if(S.Graph.Get()==G){if(!Own(S.Source.Get()))return false;Captured=true;}}
    TSet<UNiagaraScriptSource*> Sharing;int32 EmitterOwners=0;bool SystemOwner=false;auto HasGraph=[&](UNiagaraScriptSourceBase* Base){auto* Source=Cast<UNiagaraScriptSource>(Base);if(!Source)return false;if(!Live(Source)){Fail(TEXT("niagara_input_path_shared_source_unresolved"),TEXT("Unloaded source prevents exact shared-graph admission."));return false;}if(Source->NodeGraph==G){Sharing.Add(Source);return true;}return false;};for(const auto& E:Emitters){if(HasGraph(E.Source.Get()))++EmitterOwners;}for(const auto& E:ScriptBindings){const bool Matches=HasGraph(E.Source.Get());if(Matches&&(E.Script.Get()==Spawn.Get()||E.Script.Get()==Update.Get()))SystemOwner=true;}if(!R.Error.IsEmpty())return false;if(Sharing.Num()>1||EmitterOwners>1||(EmitterOwners>0&&SystemOwner))return Fail(TEXT("niagara_input_path_shared_graph_writer_unsupported"),TEXT("The selected graph is shared by multiple source/owner roles. Refused before first Modify; no duplicate or unselected owner mutation."));
    if(!Captured)return Fail(TEXT("niagara_input_path_uncaptured_graph"),TEXT("Mutable graph has no captured exact source binding."));
    for(const auto& E:Graphs){if(E.Graph.Get()==G){for(const auto& N:E.Nodes)if(!Own(N.Node.Get()))return false;}}
    return true;
}
FPinIdentity* FindExpectedPin(FWork& W,UEdGraphPin* P)
{for(auto& G:W.Graphs)for(auto& N:G.Nodes)for(auto& E:N.Pins)if(E.Pointer==P)return &E;return nullptr;}
bool FWork::PinForWrite(UEdGraphPin* P)
{
    if(!Admit(TEXT("pin_writer_admission"))||!P)return Fail(TEXT("niagara_input_path_pin_unavailable"),TEXT("Pin unavailable."));
    auto* E=FindExpectedPin(*this,P);if(!E)return Fail(TEXT("niagara_input_path_uncaptured_pin"),TEXT("Pin is not in captured exact node membership; stale pointer not dereferenced."));
    auto* N=P->GetOwningNodeUnchecked();auto* G=Cast<UNiagaraGraph>(N->GetGraph());if(!GraphForWrite(G)||!Own(N))return false;
    for(auto* L:E->Links){if(!L||!FindExpectedPin(*this,L))return Fail(TEXT("niagara_input_path_external_pin_link_unsupported"),TEXT("Linked pin is outside captured mutable graph(s)."));auto* LN=L->GetOwningNodeUnchecked();if(LN->GetGraph()!=G||!Own(LN))return Fail(TEXT("niagara_input_path_external_pin_link_unsupported"),TEXT("Cross-graph linked writer is unsupported before mutation."));}
    return true;
}
bool FWork::BreakLinks(UEdGraphPin* P,bool Notify)
{
    if(!PinForWrite(P)||!SDK(TEXT("BreakAllPinLinks"),true))return false;auto* E=FindExpectedPin(*this,P);const TArray<UEdGraphPin*> Links=E->Links;
    for(auto* L:Links){auto* Other=FindExpectedPin(*this,L);if(!Other)return Fail(TEXT("niagara_input_path_link_plan_unavailable"),TEXT("Captured reciprocal pin unavailable."));Other->Links.Remove(P);}E->Links.Empty();
    auto Row=MutationRow(TEXT("BreakAllPinLinks"));Row->SetStringField(TEXT("node_guid"),P->GetOwningNodeUnchecked()->NodeGuid.ToString());Row->SetStringField(TEXT("pin_guid"),P->PinId.ToString());Row->SetStringField(TEXT("pin_name"),P->PinName.ToString());Row->SetNumberField(TEXT("links_before"),Links.Num());P->BreakAllPinLinks(Notify);Row->SetBoolField(TEXT("SDK_returned"),true);R.bChanged|=!Links.IsEmpty();return After(TEXT("BreakAllPinLinks"));
}
bool FWork::LinkPins(UEdGraphPin* A,UEdGraphPin* B)
{
    if(!PinForWrite(A)||!PinForWrite(B)||!SDK(TEXT("MakeLinkTo"),true))return false;
    auto* EA=FindExpectedPin(*this,A);auto* EB=FindExpectedPin(*this,B);EA->Links.AddUnique(B);EB->Links.AddUnique(A);auto Row=MutationRow(TEXT("MakeLinkTo"));Row->SetStringField(TEXT("pin_a"),A->PinId.ToString());Row->SetStringField(TEXT("pin_b"),B->PinId.ToString());A->MakeLinkTo(B);Row->SetBoolField(TEXT("SDK_returned"),true);R.bChanged=true;return After(TEXT("MakeLinkTo"));
}
bool ModifyPin(UEdGraphPin* P){if(!Current->PinForWrite(P))return false;return Current->Step(TEXT("Pin_Modify"),true,[&]{P->Modify();});}
bool FWork::RemoveNode(UNiagaraGraph* G,UEdGraphNode* N,bool BreakAllLinks,bool AlwaysMarkDirty,bool Destroy)
{
    if(!GraphForWrite(G)||!Own(N)||!SDK(Destroy?TEXT("DestroyNode"):TEXT("Graph_RemoveNode"),true))return false;
    FGraphIdentity* Expected=Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return Fail(TEXT("niagara_input_path_missing_graph_plan"),TEXT("Captured graph plan unavailable."));
    const FNodeIdentity* NE=Expected->Nodes.FindByPredicate([&](const auto& E){return E.Node.Get()==N;});if(!NE)return Fail(TEXT("niagara_input_path_node_already_removed"),TEXT("Exact captured node no longer belongs to graph."));TArray<UEdGraphPin*> Removing;for(const auto& P:NE->Pins)Removing.Add(P.Pointer);
    for(auto& GE:Graphs)for(auto& Node:GE.Nodes)for(auto& P:Node.Pins)for(auto* Deleted:Removing)P.Links.Remove(Deleted);
    Expected->Nodes.RemoveAll([&](const auto& E){return E.Node.Get()==N;});
    auto Row=MutationRow(Destroy?TEXT("DestroyNode"):TEXT("Graph_RemoveNode"));Row->SetStringField(TEXT("node_path"),N->GetPathName());Row->SetStringField(TEXT("node_guid"),N->NodeGuid.ToString());Row->SetStringField(TEXT("graph_path"),G->GetPathName());
    bool Removed=false;if(Destroy){N->DestroyNode();Removed=true;}else Removed=G->RemoveNode(N,BreakAllLinks,AlwaysMarkDirty);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_removed"),Removed);Row->SetBoolField(TEXT("return_is_void"),Destroy);Prefix->SetBoolField(TEXT("last_remove_node_returned_success"),Removed);R.bChanged|=Removed;return After(Destroy?TEXT("DestroyNode"):TEXT("Graph_RemoveNode"));
}



bool FWork::Request(bool& Accepted)
{
    R.bCompileRequested=true;if(!SDK(TEXT("RequestCompile"),true))return false;R.bCompileAttempted=true;R.Compiled.Reset();Accepted=System->RequestCompile(false);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("SDK_returned"),true);O->SetBoolField(TEXT("accepted"),Accepted);O->SetBoolField(TEXT("completion_verified"),false);Compiles.Add(MakeShared<FJsonValueObject>(O));return After(TEXT("RequestCompile"));
}
bool FWork::Save(bool Direct,FString* Filename)
{
    R.bSaveRequested=true;if(!Admit(TEXT("save_preflight")))return false;auto* O=System.Get();UPackage* P=O->GetOutermost();if(Filename)Filename->Reset();FString Path;
    if(Direct&&(!FPackageName::IsValidLongPackageName(P->GetName(),false)||!FPackageName::TryConvertLongPackageNameToFilename(P->GetName(),Path,FPackageName::GetAssetPackageExtension())))return Fail(TEXT("niagara_input_path_save_path_invalid"),TEXT("Exact package filename unavailable."));
    if(!SDK(Direct?TEXT("SavePackage"):TEXT("SaveLoadedAsset"),true))return false;R.bSaveAttempted=true;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_package"),P->GetName());Row->SetStringField(TEXT("requested_asset"),O->GetPathName());bool Saved=false;
    if(Direct){FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;Saved=UPackage::SavePackage(P,O,*Path,Args);}else Saved=UEditorAssetLibrary::SaveLoadedAsset(O,false);
    R.bSaved|=Saved;SaveHealth&=Saved;if(Saved&&Direct&&Filename)*Filename=Path;Row->SetStringField(TEXT("requested_filename"),Path);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Saved);Row->SetBoolField(TEXT("actual_saved_history"),R.bSaved);Saves.Add(MakeShared<FJsonValueObject>(Row));if(!After(Direct?TEXT("SavePackage"):TEXT("SaveLoadedAsset"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}
    const bool DirtyAfter=P->IsDirty();Row->SetStringField(TEXT("package_after"),P->GetName());Row->SetBoolField(TEXT("dirty_after"),DirtyAfter);if(Saved&&DirtyAfter){SaveHealth=false;Fail(TEXT("niagara_input_path_saved_package_dirty"),TEXT("Save returned true but captured package is dirty after SDK callbacks. Save history retained; dirty flag was not cleared or retried."));return false;}return Saved;
}
bool FWork::WaitAdmission(bool Wait,bool WillRequest)
{
    if(!Wait)return true;if(!SDK(TEXT("compile_wait_admission")))return false;bool Pending=System->HasOutstandingCompilationRequests(true)||System->HasActiveCompilations();if(!After(TEXT("compile_wait_admission")))return false;
    if(!DeferredMode&&(WillRequest||Pending))return Fail(TEXT("niagara_input_path_async_wait_unsupported"),TEXT("Actual requested/pending compile wait needs a deferred final result, exact lifetime and original save/absence continuation. This synchronous implementation rejects before writer; no GT sleep/flush, fake timeout or wait=False substitution."));return true;
}
TSharedPtr<FJsonObject> FWork::WaitSnapshot(bool Wait,double Timeout,double Poll)
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0.0);if(!System.IsValid()){O->SetBoolField(TEXT("outstanding_compilation_requests_after"),false);O->SetBoolField(TEXT("active_compilations_after"),false);return O;}
    if(!SDK(TEXT("compile_status_snapshot")))return O;const bool Pending=System->HasOutstandingCompilationRequests(true),Active=System->HasActiveCompilations();O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Pending);O->SetBoolField(TEXT("active_compilations_after"),Active);if(!After(TEXT("compile_status_snapshot")))return O;if(DeferredMode&&Wait){WaitRequested=true;WaitDeferred=Pending||Active;WaitIdle=!Pending&&!Active;WaitStarted=FPlatformTime::Seconds();WaitTimeout=Timeout;WaitPoll=Poll;WaitNext=WaitStarted;WaitObject=O;O->SetBoolField(TEXT("actual_wait_implemented"),true);return O;}if(Wait&&(Pending||Active)){Fail(TEXT("niagara_input_path_async_wait_unsupported"),TEXT("Compile became pending after actual mutation/save; partial history retained. No blocking or replay."));O->SetBoolField(TEXT("actual_wait_implemented"),false);}return O;
}

FString NormalizeNiagaraObjectPathForLoad(const FString& ObjectPath);
bool IsTempGeneratedNiagaraPath(const FString& ObjectPath);
TArray<FString> GetRawStringArrayParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName);
void ReadNiagaraCompileWaitParams(
    const TSharedPtr<FJsonObject>& Params,
    bool& bWaitForCompletion,
    double& TimeoutSeconds,
    double& PollIntervalSeconds);
TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values);
FString NiagaraTypeName(const FNiagaraTypeDefinition& TypeDef);
bool IsNiagaraDynamicAddPin(const UEdGraphPin* Pin);
UEdGraphPin* FindGraphPinByName(UEdGraphNode* Node, EEdGraphPinDirection Direction, const FName PinName, bool bSkipDynamicAddPins = true);
FString GetNiagaraCustomHlslString(const UNiagaraNodeCustomHlsl* CustomNode);
FNiagaraParameterHandle BuildAliasedModuleParameterHandleForInput(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraVariable& InputVariable);
bool NiagaraUserParameterNameMatches(const FNiagaraVariable& Candidate, const FString& UserParameterName);
UEdGraphPin* FindNiagaraStackFunctionInputOverridePin(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraParameterHandle& AliasedInputHandle);
bool ValidateLinkedNiagaraDataInterfaceInputNode(UEdGraphPin& OverridePin, FString& OutError);
bool NiagaraModuleInputNameMatches(const FNiagaraVariable& Candidate, const FString& InputName);
FString PackagePathFromObjectPath(const FString& ObjectPath);
bool JsonArrayToDoubles(const TSharedPtr<FJsonValue>& Value, TArray<double>& OutValues);
TSharedPtr<FJsonValue> NumberArrayToJson(std::initializer_list<double> Values);
bool SetNiagaraVariableDataFromJson(FNiagaraVariable& Variable, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError);
int32 CountNiagaraFunctionScriptReferences(const TArray<UNiagaraScriptSource*>& Sources, UNiagaraScript* FunctionScript);
TArray<UNiagaraScriptSource*> CollectTargetNiagaraScriptSources(UNiagaraSystem* System);
TArray<UNiagaraNodeFunctionCall*> CollectSortedNiagaraFunctionCalls(UNiagaraScriptSourceBase* SourceBase);
void AddNiagaraScriptSourceIfValid(
    UNiagaraScriptSourceBase* SourceBase,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources);
void AddNiagaraScriptLatestSourceIfValid(
    UNiagaraScript* Script,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources);
void AddScratchPadContainerScriptSources(
    UNiagaraScratchPadContainer* Container,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources);
bool SetNiagaraUserParameterValue(
    FNiagaraParameterStore& Store,
    const FNiagaraVariable& Parameter,
    const TSharedPtr<FJsonValue>& JsonValue,
    FString& OutError);
TSharedPtr<FJsonValue> NiagaraParameterValueToJson(const FNiagaraParameterStore& Store, const FNiagaraVariable& Parameter);
bool IsMcpSupportedNiagaraArrayDataInterfaceType(const FNiagaraTypeDefinition& TypeDef);
bool IsSettableNiagaraUserParameterType(const FNiagaraTypeDefinition& TypeDef);
bool IsMeshArrayClass(UClass* C);
FString FunctionNameForPath(const UNiagaraNodeFunctionCall* N);
FNiagaraTypeDefinition PinTypeForPath(UEdGraphPin* P);
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
        {const FString Key(Reader->GetIdentifier());for(TCHAR C:Key){if(C==0)return false;}if(Keys.Last().Contains(Key)){return false;}Keys.Last().Add(Key);}
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        {Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>65){return false;}}
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        {if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()||Values[0]->Type!=EJson::Object){return false;}
    int32 Visits=0;if(!FiniteValues(Values[0],0,Visits)){return false;}Out=Values[0]->AsObject();return Out.IsValid();
}



FString TypeIdentity(const FNiagaraTypeDefinition& T)
{
    auto Path=[](const UObject* O){return O?O->GetPathName():FString();};
    return FString::Printf(TEXT("%s|%s|%s|%u"),*Path(T.GetStruct()),*Path(T.GetClass()),*Path(T.GetEnum()),static_cast<uint32>(T.GetFlags()));
}
FString GraphMetadataWithout(UNiagaraGraph* G,const FNiagaraVariableBase* Exclude)
{
    TArray<FString> Rows;
    for(const auto& P:G->GetAllMetaData())
    {
        if(Exclude&&P.Key.GetName()==Exclude->GetName()&&P.Key.GetType()==Exclude->GetType())continue;
        const auto* V=P.Value.Get();
        Rows.Add(P.Key.GetName().ToString()+TEXT("|")+TypeIdentity(P.Key.GetType())+TEXT("|")+(V?V->GetPathName():FString())+TEXT("|")+(V?FString::FromInt(static_cast<int32>(V->DefaultMode)):TEXT("null")));
    }
    Rows.Sort([](const FString& A,const FString& B){return A.Compare(B,ESearchCase::CaseSensitive)<0;});
    return FString::Join(Rows,TEXT("\n"));
}
FString GraphMetadataSignature(UNiagaraGraph* G){return GraphMetadataWithout(G,nullptr);}
bool IsMeshArrayClass(UClass* C)
{
    UClass* Mesh=FindObject<UClass>(nullptr,TEXT("/Script/Niagara.NiagaraDataInterfaceArrayMesh"));
    return C&&Mesh&&Mesh->IsChildOf(UNiagaraDataInterfaceArray::StaticClass())&&C->IsChildOf(Mesh);
}
FString FunctionNameForPath(const UNiagaraNodeFunctionCall* N)
{
    FString Out;if(!N||!Current->Step(TEXT("GetFunctionName"),false,[&]{Out=N->GetFunctionName();}))return {};
    Current->Budget(0,Out.Len());return Out;
}
FNiagaraTypeDefinition PinTypeForPath(UEdGraphPin* P)
{
    FNiagaraTypeDefinition Out;
    if(!ContinueRead()||!FindExpectedPin(*Current,P)){Current->Fail(TEXT("niagara_input_path_pin_unresolved"),TEXT("Pin is outside captured exact graph membership."));return Out;}
    if(!Current->Step(TEXT("PinToTypeDefinition"),false,[&]{Out=UEdGraphSchema_Niagara::PinToTypeDefinition(P);}))return {};
    return Out;
}
TSharedPtr<FJsonObject> WaitForNiagaraCompileJson(UNiagaraSystem* S,bool Wait,double Timeout,double Poll)
{
    if(!S){auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0);O->SetBoolField(TEXT("outstanding_compilation_requests_after"),false);O->SetBoolField(TEXT("active_compilations_after"),false);return O;}
    return Current->WaitSnapshot(Wait,Timeout,Poll);
}
struct FUserValue
{FNiagaraVariable Variable;TArray<uint8> Bytes;TWeakObjectPtr<UNiagaraDataInterface> DI;TWeakObjectPtr<UObject> Object;FVector Position=FVector::ZeroVector;bool HasPosition=false;};
TArray<FUserValue> CaptureUserValues(FWork& W)
{
    TArray<FUserValue> Out;if(!W.Admit(TEXT("user_store_capture"))||!W.System.IsValid())return Out;
    const auto& Store=W.System->GetExposedParameters();TArray<FNiagaraVariable> Variables;Store.GetUserParameters(Variables);
    if(!W.Budget(Variables.Num())||Variables.Num()>16384){W.Fail(TEXT("niagara_input_path_store_bound"),TEXT("Exposed parameter inventory exceeds bound."));return Out;}
    for(const auto& V:Variables)
    {
        FUserValue E;E.Variable=V;if(!Store.RedirectUserVariable(E.Variable)){W.Fail(TEXT("niagara_input_path_user_alias_unresolved"),TEXT("Captured User alias could not be resolved to the exact full-name variable."));return Out;}if(V.GetType().IsDataInterface()){auto* DI=Store.GetDataInterface(V);E.DI=DI;if(DI&&!W.Pin(DI))return Out;}
        else if(V.GetType().IsUObject()){UObject* O=Store.GetUObject(V);E.Object=O;if(O&&!W.Pin(O))return Out;}
        else {const uint8* D=Store.GetParameterData(V);const int32 Size=V.GetSizeInBytes();if(Size<0||!W.Budget(0,Size)){return Out;}if(D&&Size)E.Bytes.Append(D,Size);}
        if(V.GetType()==FNiagaraTypeDefinition::GetPositionDef()){if(const FVector* P=Store.GetPositionParameterValue(V.GetName())){E.Position=*P;E.HasPosition=true;}}
        Out.Add(MoveTemp(E));
    }return Out;
}
bool SameUserValue(const FUserValue& A,const FUserValue& B)
{
    return A.Variable.GetName().ToString().Equals(B.Variable.GetName().ToString(),ESearchCase::CaseSensitive)&&A.Variable.GetType()==B.Variable.GetType()&&A.Bytes==B.Bytes&&A.DI==B.DI&&A.Object==B.Object&&A.HasPosition==B.HasPosition&&(!A.HasPosition||A.Position==B.Position);
}
bool VerifyCanonicalUserForPath(FWork& W,const FNiagaraVariable& User)
{
    if(!W.Admit(TEXT("canonical_user_parameter"))||!W.System.IsValid())return false;
    const auto& CanonicalStore=W.System->GetExposedParameters();
    const int32 FullParameterCount=CanonicalStore.Num();
    if(FullParameterCount<0||FullParameterCount>65536||!W.Budget(FullParameterCount))return W.Fail(TEXT("niagara_input_path_full_user_store_bound"),TEXT("Full exposed declaration inventory exceeds the bounded canonical User check."));
    TArray<FNiagaraVariable> FullParametersForUser;CanonicalStore.GetParameters(FullParametersForUser);
    int32 ExactFullMatches=0;for(const FNiagaraVariable& FullParameter:FullParametersForUser)if(FullParameter.GetName()==User.GetName()&&FullParameter.GetType()==User.GetType())++ExactFullMatches;
    FNiagaraVariable CanonicalProbe=User;const bool CanonicalRedirected=CanonicalStore.RedirectUserVariable(CanonicalProbe);
    W.Prefix->SetStringField(TEXT("last_user_full_name_checked"),User.GetName().ToString());
    W.Prefix->SetStringField(TEXT("last_user_full_name_redirect"),CanonicalProbe.GetName().ToString());
    if(ExactFullMatches!=1||!CanonicalRedirected||CanonicalProbe.GetName()!=User.GetName()||CanonicalProbe.GetType()!=User.GetType())return W.Fail(TEXT("niagara_input_path_user_canonical_redirect_unsupported"),TEXT("Selected full typed User declaration is absent, ambiguous, or redirects to another declaration; no next store or graph writer dispatched."));
    return true;
}
bool VerifyUnrelatedUserValues(FWork& W,const TArray<FUserValue>& Before,const FNiagaraVariable& Selected,bool AllowNew)
{
    if(!VerifyCanonicalUserForPath(W,Selected))return false;
    auto After=CaptureUserValues(W);if(!ContinueRead())return false;
    int32 OtherBefore=0,OtherAfter=0;
    auto SelectedMatch=[&](const FUserValue& E){return E.Variable.GetName()==Selected.GetName()&&E.Variable.GetType()==Selected.GetType();};
    for(const auto& E:Before){if(SelectedMatch(E))continue;++OtherBefore;const auto* A=After.FindByPredicate([&](const auto& V){return V.Variable.GetName()==E.Variable.GetName()&&V.Variable.GetType()==E.Variable.GetType();});if(!A||!SameUserValue(E,*A))return W.Fail(TEXT("niagara_input_path_unselected_store_changed"),TEXT("Unselected exposed parameter changed across SDK call."));}
    for(const auto& E:After)if(!SelectedMatch(E))++OtherAfter;
    const int32 SelectedCount=After.FilterByPredicate(SelectedMatch).Num();
    return (OtherBefore==OtherAfter&&SelectedCount==1&&(AllowNew||Before.ContainsByPredicate(SelectedMatch)))||W.Fail(TEXT("niagara_input_path_store_shape_changed"),TEXT("Exposed parameter set differs from the admitted selected transition."));
}
bool PreflightStackMap(FWork& W,UNiagaraNodeFunctionCall* Function)
{
    if(!ContinueRead())return false;auto* G=Cast<UNiagaraGraph>(Function->GetGraph());if(!G||!W.GraphForWrite(G)||!W.Own(Function))return false;
    TArray<UEdGraphPin*> Inputs;for(auto* P:Function->Pins)if(P->Direction==EGPD_Input&&PinTypeForPath(P)==FNiagaraTypeDefinition::GetParameterMapDef())Inputs.Add(P);
    if(!ContinueRead())return false;
    if(Inputs.Num()!=1||Inputs[0]->LinkedTo.Num()!=1||!FindExpectedPin(W,Inputs[0]->LinkedTo[0]))return W.Fail(TEXT("niagara_input_path_stack_map_shape_unsupported"),TEXT("The module must have one exact captured ParameterMap input and one upstream link before public stack SDK."));
    return true;
}
UEdGraphPin* MapPin(UEdGraphNode* N,EEdGraphPinDirection Direction)
{
    UEdGraphPin* Found=nullptr;for(auto* P:N->Pins)if(P&&P->Direction==Direction&&UEdGraphSchema_Niagara::PinToTypeDefinition(P)==FNiagaraTypeDefinition::GetParameterMapDef()){if(Found)return nullptr;Found=P;}return Found;
}
bool PinHeaderSame(const FPinIdentity& E,const UEdGraphPin* P)
{
    return P&&P->PinId==E.Id&&P->PinName.ToString().Equals(E.Name.ToString(),ESearchCase::CaseSensitive)&&P->Direction==E.Direction&&P->PinType==E.Type&&P->DefaultValue.Equals(E.DefaultValue,ESearchCase::CaseSensitive)&&P->AutogeneratedDefaultValue.Equals(E.AutoDefault,ESearchCase::CaseSensitive)&&P->DefaultTextValue.ToString().Equals(E.DefaultText,ESearchCase::CaseSensitive)&&P->DefaultObject==E.DefaultObject.Get();
}
bool CaptureTransitionResult(FWork& W,UNiagaraGraph* G,const FGraphIdentity& Before,UNiagaraNodeFunctionCall* Function,const FNiagaraParameterHandle& Alias,const FNiagaraTypeDefinition& Type,UEdGraphPin* Returned,UEdGraphPin* ExistingOverride,const FNiagaraVariableBase* LinkedUser,bool WasOverride,const FString& OtherMetadataBefore)
{
    // The public stack call has one admitted mutable graph. Retained graph/node/pin
    // identity is validated before adopting only the exact requested append/link plan.
    if(!W.Admit(TEXT("stack_transition_context")))return false;
    if(!GraphMetadataWithout(G,LinkedUser).Equals(OtherMetadataBefore,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_input_path_unselected_metadata_changed"),TEXT("Unselected graph metadata changed during the requested stack transition."));
    if(G->Nodes.Num()<Before.Nodes.Num()||G->Nodes.Num()>Before.Nodes.Num()+1)return W.Fail(TEXT("niagara_input_path_unexpected_node_delta"),TEXT("Public stack operation changed an unexpected number of nodes."));
    for(int32 I=0;I<Before.Nodes.Num();++I)if(G->Nodes[I]!=Before.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_input_path_retained_node_changed"),TEXT("Retained node order/identity changed during stack SDK."));
    UEdGraphNode* NewNode=G->Nodes.Num()>Before.Nodes.Num()?G->Nodes.Last().Get():nullptr;
    if(NewNode&&(!Live(NewNode)||!NewNode->IsIn(W.System.Get())||NewNode->GetOutermost()!=W.System->GetOutermost()||NewNode->GetGraph()!=G))return W.Fail(TEXT("niagara_input_path_generated_node_owner_invalid"),TEXT("New stack node is outside the requested graph/System."));
    const TCHAR* NewClass=WasOverride?TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"):TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet");
    if(NewNode&&!NewNode->GetClass()->GetPathName().Equals(NewClass,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_input_path_generated_node_class_invalid"),TEXT("Public stack SDK created an unexpected node class."));
    if(!WasOverride&&!NewNode)return W.Fail(TEXT("niagara_input_path_link_node_missing"),TEXT("User binding did not append its exact getter node."));
    TSet<UEdGraphPin*> LivePins;
    for(UEdGraphNode* N:G->Nodes){if(!Live(N)||!W.Pin(N))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_input_path_generated_pin_invalid"),TEXT("Post-SDK pin owner is invalid."));LivePins.Add(P);}}
    if(Returned&&!LivePins.Contains(Returned))return W.Fail(TEXT("niagara_input_path_returned_pin_expired"),TEXT("Returned override pin was removed during SDK callback; stale pointer not read."));
    if(ExistingOverride&&!LivePins.Contains(ExistingOverride))return W.Fail(TEXT("niagara_input_path_existing_override_expired"),TEXT("Captured existing override pin disappeared during stack SDK; stale pointer not dereferenced."));
    UEdGraphPin* FunctionInput=MapPin(Function,EGPD_Input);if(!FunctionInput||FunctionInput->LinkedTo.Num()!=1)return W.Fail(TEXT("niagara_input_path_stack_map_changed"),TEXT("Module map input no longer has exact upstream."));
    UEdGraphNode* OverrideNode=WasOverride?(Returned?Returned->GetOwningNodeUnchecked():nullptr):ExistingOverride->GetOwningNodeUnchecked();
    if(!OverrideNode||!OverrideNode->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_input_path_override_owner_invalid"),TEXT("Exact override owner is not the public reflected ParameterMapSet class."));
    UEdGraphPin* OverrideInput=MapPin(OverrideNode,EGPD_Input);UEdGraphPin* OverrideOutput=MapPin(OverrideNode,EGPD_Output);
    if(!OverrideInput||!OverrideOutput||OverrideInput->LinkedTo.Num()!=1)return W.Fail(TEXT("niagara_input_path_override_stack_invalid"),TEXT("Override ParameterMap input must have one captured upstream link."));
    UEdGraphPin* Prior=OverrideInput->LinkedTo[0];if(!LivePins.Contains(Prior))return W.Fail(TEXT("niagara_input_path_upstream_unresolved"),TEXT("Override upstream is outside current exact graph pins."));
    UEdGraphPin* NewGetInput=nullptr;UEdGraphPin* NewGetOutput=nullptr;
    if(!WasOverride)
    {
        NewGetInput=MapPin(NewNode,EGPD_Input);int32 Semantic=0;
        for(auto* P:NewNode->Pins)if(P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P)){++Semantic;NewGetOutput=P;}
        if(Semantic!=1||!NewGetInput||!NewGetOutput||!LinkedUser||NewGetOutput->PinName!=LinkedUser->GetName()||UEdGraphSchema_Niagara::PinToTypeDefinition(NewGetOutput)!=LinkedUser->GetType()||NewGetInput->LinkedTo!=TArray<UEdGraphPin*>{Prior}||NewGetOutput->LinkedTo!=TArray<UEdGraphPin*>{ExistingOverride})return W.Fail(TEXT("niagara_input_path_user_getter_shape_invalid"),TEXT("Generated getter does not match the exact requested typed User link."));
    }
    for(const auto& NE:Before.Nodes)
    {
        auto* N=NE.Node.Get();if(N->NodeGuid!=NE.Id||N->NodePosX!=NE.X||N->NodePosY!=NE.Y||N->GetGraph()!=G)return W.Fail(TEXT("niagara_input_path_retained_node_fields_changed"),TEXT("Retained node GUID/position/graph changed during stack SDK."));int32 AddedPins=N->Pins.Num()-NE.Pins.Num();const bool IsOldOverride=WasOverride&&N==OverrideNode;
        if(AddedPins!=(IsOldOverride&&!ExistingOverride?1:0))return W.Fail(TEXT("niagara_input_path_unexpected_pin_delta"),TEXT("Retained node pin count differs from requested override insertion."));
        TArray<UEdGraphPin*> Remaining;for(auto* P:N->Pins)if(!(IsOldOverride&&!ExistingOverride&&P==Returned))Remaining.Add(P);
        if(Remaining.Num()!=NE.Pins.Num())return W.Fail(TEXT("niagara_input_path_pin_plan_invalid"),TEXT("Override insertion did not yield exact retained pin set."));
        for(int32 J=0;J<NE.Pins.Num();++J)
        {
            const auto& PE=NE.Pins[J];auto* P=Remaining[J];if(P!=PE.Pointer||!PinHeaderSame(PE,P))return W.Fail(TEXT("niagara_input_path_retained_pin_changed"),TEXT("Retained exact pin/type/name changed during SDK."));
            TArray<UEdGraphPin*> Expected=PE.Links;
            if(WasOverride&&NewNode)
            {
                UEdGraphPin* OldFunctionInput=FunctionInput;
                const FPinIdentity* OldInput=FindExpectedPin(W,OldFunctionInput);
                if(!OldInput||OldInput->Links.Num()!=1)return W.Fail(TEXT("niagara_input_path_original_map_unresolved"),TEXT("Original module upstream no longer known."));
                UEdGraphPin* OldPrior=OldInput->Links[0];const auto* OldPriorExpected=FindExpectedPin(W,OldPrior);
                if(!OldPriorExpected)return false;
                if(P==OldPrior)Expected={OverrideInput};
                else if(P==OldFunctionInput){Expected.Remove(OldPrior);Expected.AddUnique(OverrideOutput);}
                else if(OldPriorExpected->Links.Contains(P)){Expected.Remove(OldPrior);Expected.AddUnique(OverrideOutput);}
            }
            if(!WasOverride){if(P==ExistingOverride)Expected={NewGetOutput};else if(P==Prior)Expected.AddUnique(NewGetInput);}
            if(P->LinkedTo!=Expected)return W.Fail(TEXT("niagara_input_path_unselected_links_changed"),TEXT("Retained links differ from the single requested public stack transition."));
        }
    }
    if(WasOverride&&Returned&&(Returned->PinName!=Alias.GetParameterHandleString()||Returned->Direction!=EGPD_Input||UEdGraphSchema_Niagara::PinToTypeDefinition(Returned)!=Type||(!ExistingOverride&&Returned->LinkedTo.Num()!=0)||(ExistingOverride&&Returned!=ExistingOverride)))return W.Fail(TEXT("niagara_input_path_override_pin_shape_invalid"),TEXT("Override pin is not exact typed empty requested alias."));
    if(WasOverride&&NewNode){for(auto* P:NewNode->Pins)if(P!=OverrideInput&&P!=OverrideOutput&&P!=Returned&&!IsNiagaraDynamicAddPin(P))return W.Fail(TEXT("niagara_input_path_override_extra_pin"),TEXT("Generated override contains an unexpected semantic pin."));}
    if(NewNode){if(!W.Own(NewNode))return false;W.R.bChanged=true;}
    auto* Source=W.Sources.FindByPredicate([&](const auto& S){return S.Graph.Get()==G;});if(!Source)return false;
    W.Graphs.RemoveAll([&](const auto& GE){return GE.Graph.Get()==G;});
    return W.CaptureGraph(Source->Source.Get());
}
UEdGraphPin* CreateOverrideForPath(FWork& W,UNiagaraNodeFunctionCall* Function,const FNiagaraParameterHandle& Alias,const FNiagaraTypeDefinition& Type)
{
    if(!PreflightStackMap(W,Function))return nullptr;auto* G=Cast<UNiagaraGraph>(Function->GetGraph());auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return nullptr;
    const FGraphIdentity Before=*Expected;const FString MetadataBefore=GraphMetadataSignature(G);UEdGraphPin* Existing=FindNiagaraStackFunctionInputOverridePin(Function,Alias);UEdGraphPin* Returned=nullptr;
    if(!W.SDK(TEXT("GetOrCreateStackFunctionInputOverridePin"),true))return nullptr;auto Row=W.MutationRow(TEXT("GetOrCreateStackFunctionInputOverridePin"));Row->SetStringField(TEXT("module_guid"),Function->NodeGuid.ToString());Row->SetStringField(TEXT("input_alias"),Alias.GetParameterHandleString().ToString());
    {TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);Returned=&FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Function,Alias,Type,FGuid(),FGuid());Row->SetBoolField(TEXT("SDK_returned"),true);if(!CaptureTransitionResult(W,G,Before,Function,Alias,Type,Returned,Existing,nullptr,true,MetadataBefore))return nullptr;}
    if(!W.After(TEXT("GetOrCreateStackFunctionInputOverridePin")))return nullptr;return Returned;
}
bool LinkUserForPath(FWork& W,UNiagaraNodeFunctionCall* Function,UEdGraphPin* Override,const FNiagaraVariableBase& User)
{
    if(!W.PinForWrite(Override)||Override->LinkedTo.Num()!=0||!PreflightStackMap(W,Function))return W.Fail(TEXT("niagara_input_path_override_not_empty"),TEXT("Exact override must be empty before User link SDK."));
    auto* G=Cast<UNiagaraGraph>(Function->GetGraph());auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return false;const FGraphIdentity Before=*Expected;const FString MetadataBefore=GraphMetadataWithout(G,&User);
    UNiagaraScriptVariable* ExistingMetadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&!W.Own(ExistingMetadata))return false;
    if(!W.SDK(TEXT("SetLinkedParameterValueForFunctionInput"),true))return false;auto Row=W.MutationRow(TEXT("SetLinkedParameterValueForFunctionInput"));Row->SetStringField(TEXT("module_guid"),Function->NodeGuid.ToString());Row->SetStringField(TEXT("user_parameter"),User.GetName().ToString());
    {TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);TSet<FNiagaraVariableBase> Known;Known.Add(User);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Override,User,Known);Row->SetBoolField(TEXT("SDK_returned"),true);if(!CaptureTransitionResult(W,G,Before,Function,FNiagaraParameterHandle(),User.GetType(),nullptr,Override,&User,false,MetadataBefore))return false;}
    UNiagaraScriptVariable* Metadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&Metadata!=ExistingMetadata)return W.Fail(TEXT("niagara_input_path_metadata_identity_changed"),TEXT("Selected existing metadata was replaced during binding SDK."));if(!Metadata||!W.Own(Metadata)||Metadata->DefaultMode!=ENiagaraDefaultMode::FailIfPreviouslyNotSet)return W.Fail(TEXT("niagara_input_path_binding_metadata_readback_failed"),TEXT("Exact selected User metadata was not observed after public binding."));
    W.R.bChanged=true;return W.After(TEXT("SetLinkedParameterValueForFunctionInput"));
}
bool ClearLinkedForPath(FWork& W,UEdGraphPin& Override,FString& Error)
{
    if(!ValidateLinkedNiagaraDataInterfaceInputNode(Override,Error))return false;if(Override.LinkedTo.IsEmpty())return true;
    auto* Node=Override.LinkedTo[0]->GetOwningNodeUnchecked();auto* G=Cast<UNiagaraGraph>(Node->GetGraph());
    return W.Modify(G)&&W.Modify(Node)&&W.BreakLinks(&Override,true)&&W.RemoveNode(G,Node);
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

TArray<FString> GetRawStringArrayParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName)
{
    TArray<FString> Values;
    const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(FieldName, JsonValues) || !JsonValues)
    {
        return Values;
    }

    for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValues)
    {
        FString Value;
        if (JsonValue.IsValid() && JsonValue->TryGetString(Value))
        {
            Value.TrimStartAndEndInline();
            Value.TrimQuotesInline();
            if (!Value.IsEmpty())
            {
                Values.AddUnique(Value);
            }
        }
    }
    return Values;
}

void ReadNiagaraCompileWaitParams(
    const TSharedPtr<FJsonObject>& Params,
    bool& bWaitForCompletion,
    double& TimeoutSeconds,
    double& PollIntervalSeconds)
{
    bWaitForCompletion = false;
    TimeoutSeconds = 10.0;
    PollIntervalSeconds = 0.1;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
        Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
        Params->TryGetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);
    }
    TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 0.0, 60.0);
    PollIntervalSeconds = FMath::Clamp(PollIntervalSeconds, 0.02, 1.0);
}

TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    return JsonValues;
}

FString NiagaraTypeName(const FNiagaraTypeDefinition& TypeDef)
{
    return TypeDef.GetNameText().ToString();
}

bool IsNiagaraDynamicAddPin(const UEdGraphPin* Pin)
{
    return Pin &&
        Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc &&
        Pin->PinType.PinSubCategory == TEXT("DynamicAddPin");
}

UEdGraphPin* FindGraphPinByName(UEdGraphNode* Node, EEdGraphPinDirection Direction, const FName PinName, bool bSkipDynamicAddPins)
{
    if (!Node)
    {
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->Direction != Direction)
        {
            continue;
        }
        if (bSkipDynamicAddPins && IsNiagaraDynamicAddPin(Pin))
        {
            continue;
        }
        if (Pin->PinName == PinName)
        {
            return Pin;
        }
    }
    return nullptr;
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

FNiagaraParameterHandle BuildAliasedModuleParameterHandleForInput(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraVariable& InputVariable)
{
    if (!FunctionCall)
    {
        return FNiagaraParameterHandle();
    }

    const FNiagaraParameterHandle InputHandle(InputVariable.GetName());
    const FNiagaraParameterHandle ModuleHandle = InputHandle.IsModuleHandle()
        ? InputHandle
        : FNiagaraParameterHandle::CreateModuleParameterHandle(InputVariable.GetName());
    return FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, FunctionCall);
}

bool NiagaraUserParameterNameMatches(const FNiagaraVariable& Candidate, const FString& UserParameterName)
{
    const FString CandidateName = Candidate.GetName().ToString();
    FString CandidateShortName = CandidateName;
    CandidateShortName.RemoveFromStart(TEXT("User."));
    FString RequestedShortName = UserParameterName;
    RequestedShortName.RemoveFromStart(TEXT("User."));
    return CandidateName.Equals(UserParameterName, ESearchCase::IgnoreCase) ||
        CandidateShortName.Equals(RequestedShortName, ESearchCase::IgnoreCase);
}

UEdGraphPin* FindNiagaraStackFunctionInputOverridePin(
    const UNiagaraNodeFunctionCall* FunctionCall,
    const FNiagaraParameterHandle& AliasedInputHandle)
{
    if (!FunctionCall)
    {
        return nullptr;
    }

    UEdGraph* Graph = FunctionCall->GetGraph();
    if (!Graph)
    {
        return nullptr;
    }

    const FName TargetPinName = AliasedInputHandle.GetParameterHandleString();
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || !Node->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive))
        {
            continue;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == TargetPinName)
            {
                return Pin;
            }
        }
    }

    return nullptr;
}

bool ValidateLinkedNiagaraDataInterfaceInputNode(UEdGraphPin& OverridePin, FString& OutError)
{
    if (OverridePin.LinkedTo.Num() == 0)
    {
        return true;
    }

    if (OverridePin.LinkedTo.Num() != 1)
    {
        OutError = FString::Printf(TEXT("Expected one linked override value node, found %d"), OverridePin.LinkedTo.Num());
        return false;
    }

    UEdGraphPin* LinkedPin = OverridePin.LinkedTo[0];
    UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
    UEdGraph* Graph = LinkedNode ? LinkedNode->GetGraph() : nullptr;
    if (!LinkedNode || !Graph)
    {
        OutError = TEXT("Linked override value node or graph is invalid");
        return false;
    }

    const bool bIsSupportedOverrideValueNode =
        LinkedNode->IsA<UNiagaraNodeInput>() ||
        LinkedNode->IsA<UNiagaraNodeFunctionCall>() ||
        LinkedNode->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive);
    if (!bIsSupportedOverrideValueNode)
    {
        OutError = FString::Printf(
            TEXT("Existing override value node '%s' is not a direct Niagara data input, parameter-map-get, or dynamic-input function node; refusing to rewrite an unsupported graph shape"),
            *LinkedNode->GetName());
        return false;
    }
    if (LinkedPin->LinkedTo.Num() != 1 || LinkedPin->LinkedTo[0] != &OverridePin)
    {
        OutError = FString::Printf(
            TEXT("Existing override value node '%s' output is shared by multiple graph links; refusing to remove it"),
            *LinkedNode->GetName());
        return false;
    }
    for (const UEdGraphPin* Pin : LinkedNode->Pins)
    {
        if (!Pin || Pin == LinkedPin || Pin->Direction != EGPD_Output)
        {
            continue;
        }
        if (Pin->LinkedTo.Num() > 0)
        {
            OutError = FString::Printf(
                TEXT("Existing override value node '%s' has additional linked outputs; refusing to remove it"),
                *LinkedNode->GetName());
            return false;
        }
    }

    return true;
}

bool NiagaraModuleInputNameMatches(const FNiagaraVariable& Candidate, const FString& InputName)
{
    const FString CandidateName = Candidate.GetName().ToString();
    FString ShortName = CandidateName;
    ShortName.RemoveFromStart(TEXT("Module."));
    return CandidateName.Equals(InputName, ESearchCase::IgnoreCase) ||
        ShortName.Equals(InputName, ESearchCase::IgnoreCase);
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

bool JsonArrayToDoubles(const TSharedPtr<FJsonValue>& Value, TArray<double>& OutValues)
{
    if (!Value.IsValid() || Value->Type != EJson::Array)
    {
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
    {
        double NumberValue = 0.0;
        if (!Item.IsValid() || !Item->TryGetNumber(NumberValue))
        {
            return false;
        }
        OutValues.Add(NumberValue);
    }
    return true;
}

TSharedPtr<FJsonValue> NumberArrayToJson(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (double Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return MakeShared<FJsonValueArray>(JsonValues);
}

bool SetNiagaraVariableDataFromJson(FNiagaraVariable& Variable, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
{
    const FNiagaraTypeDefinition& TypeDef = Variable.GetType();
    if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        double NumberValue = 0.0;
        if (!JsonValue.IsValid() || !JsonValue->TryGetNumber(NumberValue))
        {
            OutError = TEXT("Expected numeric value for float Niagara module input");
            return false;
        }
        Variable.SetValue<float>(static_cast<float>(NumberValue));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
    {
        double NumberValue = 0.0;
        if (!JsonValue.IsValid() || !JsonValue->TryGetNumber(NumberValue))
        {
            OutError = TEXT("Expected numeric value for int Niagara module input");
            return false;
        }
        Variable.SetValue<int32>(static_cast<int32>(NumberValue));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        bool BoolValue = false;
        if (!JsonValue.IsValid() || !JsonValue->TryGetBool(BoolValue))
        {
            OutError = TEXT("Expected boolean value for bool Niagara module input");
            return false;
        }
        Variable.SetValue<FNiagaraBool>(FNiagaraBool(BoolValue));
        return true;
    }
    if (TypeDef.IsEnum() && TypeDef.GetEnum())
    {
        const UEnum* Enum = TypeDef.GetEnum();
        int32 ResolvedIndex = INDEX_NONE;

        if (JsonValue.IsValid() && JsonValue->Type == EJson::Number)
        {
            double NumericValue = 0.0;
            if (!JsonValue->TryGetNumber(NumericValue) || !FMath::IsFinite(NumericValue))
            {
                OutError = TEXT("Expected a finite integer for Niagara enum module input");
                return false;
            }
            const double RoundedValue = FMath::RoundToDouble(NumericValue);
            if (NumericValue != RoundedValue || RoundedValue < static_cast<double>(MIN_int32) || RoundedValue > static_cast<double>(MAX_int32))
            {
                OutError = TEXT("Expected an exact int32 for Niagara enum module input");
                return false;
            }
            ResolvedIndex = Enum->GetIndexByValue(static_cast<int64>(RoundedValue));
        }
        else if (JsonValue.IsValid() && JsonValue->Type == EJson::String)
        {
            FString RequestedValue;
            if (!JsonValue->TryGetString(RequestedValue))
            {
                OutError = TEXT("Expected a string or integer for Niagara enum module input");
                return false;
            }
            RequestedValue.TrimStartAndEndInline();
            if (RequestedValue.IsEmpty())
            {
                OutError = TEXT("Niagara enum module input cannot be empty");
                return false;
            }

            TArray<int32> MatchingIndices;
            for (int32 EnumIndex = 0; EnumIndex < Enum->NumEnums(); ++EnumIndex)
            {
                if (Enum->GetNameStringByIndex(EnumIndex).Equals(RequestedValue, ESearchCase::IgnoreCase) ||
                    Enum->GetNameByIndex(EnumIndex).ToString().Equals(RequestedValue, ESearchCase::IgnoreCase) ||
                    Enum->GetAuthoredNameStringByIndex(EnumIndex).Equals(RequestedValue, ESearchCase::IgnoreCase) ||
                    Enum->GetDisplayNameTextByIndex(EnumIndex).ToString().Equals(RequestedValue, ESearchCase::IgnoreCase))
                {
                    MatchingIndices.Add(EnumIndex);
                }
            }
            if (MatchingIndices.Num() == 1)
            {
                ResolvedIndex = MatchingIndices[0];
            }
            else if (MatchingIndices.Num() > 1)
            {
                OutError = FString::Printf(TEXT("Ambiguous Niagara enum module input for %s: %s"), *Enum->GetPathName(), *RequestedValue);
                return false;
            }
        }
        else
        {
            OutError = TEXT("Expected a display/internal enum name string or integer for Niagara enum module input");
            return false;
        }

        if (ResolvedIndex == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("Invalid Niagara enum module input for %s"), *Enum->GetPathName());
            return false;
        }

        const FString InternalName = Enum->GetNameStringByIndex(ResolvedIndex);
        const bool bTerminalMax = InternalName.EndsWith(TEXT("_MAX"), ESearchCase::IgnoreCase) ||
            InternalName.Equals(TEXT("MAX"), ESearchCase::IgnoreCase);
        if (Enum->HasMetaData(TEXT("Hidden"), ResolvedIndex) || bTerminalMax)
        {
            OutError = FString::Printf(TEXT("Hidden or terminal Niagara enum module input is not selectable for %s"), *Enum->GetPathName());
            return false;
        }

        const int64 ResolvedValue = Enum->GetValueByIndex(ResolvedIndex);
        if (ResolvedValue < static_cast<int64>(MIN_int32) || ResolvedValue > static_cast<int64>(MAX_int32))
        {
            OutError = FString::Printf(TEXT("Niagara enum value is outside int32 range for %s"), *Enum->GetPathName());
            return false;
        }

        FNiagaraInt32 NiagaraEnumValue;
        NiagaraEnumValue.Value = static_cast<int32>(ResolvedValue);
        Variable.SetValue<FNiagaraInt32>(NiagaraEnumValue);
        return true;
    }

    TArray<double> Values;
    if (!JsonArrayToDoubles(JsonValue, Values))
    {
        OutError = TEXT("Expected numeric array value for vector/color Niagara module input");
        return false;
    }

    if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected color array with at least 3 values");
            return false;
        }
        const float Alpha = Values.Num() >= 4 ? static_cast<float>(Values[3]) : 1.0f;
        Variable.SetValue<FLinearColor>(FLinearColor(static_cast<float>(Values[0]), static_cast<float>(Values[1]), static_cast<float>(Values[2]), Alpha));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
    {
        if (Values.Num() < 2)
        {
            OutError = TEXT("Expected vec2 array with 2 values");
            return false;
        }
        Variable.SetValue<FVector2f>(FVector2f(Values[0], Values[1]));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected vec3 array with 3 values");
            return false;
        }
        Variable.SetValue<FVector3f>(FVector3f(Values[0], Values[1], Values[2]));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
    {
        if (Values.Num() < 4)
        {
            OutError = TEXT("Expected vec4 array with 4 values");
            return false;
        }
        Variable.SetValue<FVector4f>(FVector4f(Values[0], Values[1], Values[2], Values[3]));
        return true;
    }
    if (TypeDef == FNiagaraTypeDefinition::GetPositionDef())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected position array with 3 values");
            return false;
        }
        Variable.SetValue<FVector3f>(FVector3f(Values[0], Values[1], Values[2]));
        return true;
    }

    OutError = FString::Printf(TEXT("Unsupported Niagara module input type: %s"), *NiagaraTypeName(TypeDef));
    return false;
}

int32 CountNiagaraFunctionScriptReferences(const TArray<UNiagaraScriptSource*>& Sources, UNiagaraScript* FunctionScript)
{
    if (!FunctionScript)
    {
        return 0;
    }

    int32 ReferenceCount = 0;
    for (UNiagaraScriptSource* Source : Sources)
    {
        const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(Source);
        for (UNiagaraNodeFunctionCall* FunctionCall : FunctionCalls)
        {
            if (FunctionCall && FunctionCall->FunctionScript == FunctionScript)
            {
                ++ReferenceCount;
            }
        }
    }
    return ReferenceCount;
}

TArray<UNiagaraScriptSource*> CollectTargetNiagaraScriptSources(UNiagaraSystem* System)
{
    if(!ContinueRead())return {};
    TArray<UNiagaraScriptSource*> Sources;
    TSet<UNiagaraScriptSource*> SeenSources;
    if (!System)
    {
        return Sources;
    }

    AddNiagaraScriptLatestSourceIfValid(System->GetSystemSpawnScript(), Sources, SeenSources);
    if(!ContinueRead())return Sources;
    AddNiagaraScriptLatestSourceIfValid(System->GetSystemUpdateScript(), Sources, SeenSources);
    if(!ContinueRead())return Sources;

    for (const TObjectPtr<UNiagaraScript>& ScratchPadScript : System->ScratchPadScripts)
    {
        AddNiagaraScriptLatestSourceIfValid(ScratchPadScript.Get(), Sources, SeenSources);
    if(!ContinueRead())return Sources;
    }

    for (FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            AddNiagaraScriptSourceIfValid(EmitterData->GraphSource, Sources, SeenSources);
    if(!ContinueRead())return Sources;
            AddScratchPadContainerScriptSources(EmitterData->ScratchPads, Sources, SeenSources);
    if(!ContinueRead())return Sources;
            AddScratchPadContainerScriptSources(EmitterData->ParentScratchPads, Sources, SeenSources);
    if(!ContinueRead())return Sources;
        }
    }

    return Sources;
}

TArray<UNiagaraNodeFunctionCall*> CollectSortedNiagaraFunctionCalls(UNiagaraScriptSourceBase* SourceBase)
{
    TArray<UNiagaraNodeFunctionCall*> FunctionCalls;
    UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    if (!Graph)
    {
        return FunctionCalls;
    }

    if(!Current->CaptureGraph(Source))return FunctionCalls;
    for (UEdGraphNode* Node : TArray<TObjectPtr<UEdGraphNode>>(Graph->Nodes))
    {
        if (UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
        {
            FunctionCalls.Add(FunctionCall);
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
    return FunctionCalls;
}

void AddNiagaraScriptSourceIfValid(
    UNiagaraScriptSourceBase* SourceBase,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources)
{
    if(!ContinueRead())return ;
    UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
    if (Source && Source->NodeGraph && !SeenSources.Contains(Source))
    {
        if(!Current->CaptureGraph(Source))return;
        SeenSources.Add(Source);
        OutSources.Add(Source);
    }
}

void AddNiagaraScriptLatestSourceIfValid(
    UNiagaraScript* Script,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources)
{
    if(!ContinueRead())return ;
    if (Script)
    {
        AddNiagaraScriptSourceIfValid(Script->GetLatestSource(), OutSources, SeenSources);
    if(!ContinueRead())return ;
    }
}

void AddScratchPadContainerScriptSources(
    UNiagaraScratchPadContainer* Container,
    TArray<UNiagaraScriptSource*>& OutSources,
    TSet<UNiagaraScriptSource*>& SeenSources)
{
    if(!ContinueRead())return ;
    if (!Container)
    {
        return;
    }

    for (const TObjectPtr<UNiagaraScript>& Script : Container->Scripts)
    {
        AddNiagaraScriptLatestSourceIfValid(Script.Get(), OutSources, SeenSources);
    if(!ContinueRead())return ;
    }
}

bool SetNiagaraUserParameterValue(
    FNiagaraParameterStore& Store,
    const FNiagaraVariable& Parameter,
    const TSharedPtr<FJsonValue>& JsonValue,
    FString& OutError)
{
    const FNiagaraTypeDefinition& TypeDef = Parameter.GetType();
    if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        double NumberValue = 0.0;
        if (!JsonValue.IsValid() || !JsonValue->TryGetNumber(NumberValue))
        {
            OutError = TEXT("Expected numeric value for float Niagara user parameter");
            return false;
        }
        return Store.SetParameterValue<float>(static_cast<float>(NumberValue), Parameter, false);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
    {
        double NumberValue = 0.0;
        if (!JsonValue.IsValid() || !JsonValue->TryGetNumber(NumberValue))
        {
            OutError = TEXT("Expected numeric value for int Niagara user parameter");
            return false;
        }
        return Store.SetParameterValue<int32>(static_cast<int32>(NumberValue), Parameter, false);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        bool BoolValue = false;
        if (!JsonValue.IsValid() || !JsonValue->TryGetBool(BoolValue))
        {
            OutError = TEXT("Expected boolean value for bool Niagara user parameter");
            return false;
        }
        return Store.SetParameterValue<FNiagaraBool>(FNiagaraBool(BoolValue), Parameter, false);
    }
    TArray<double> Values;
    if (!JsonArrayToDoubles(JsonValue, Values))
    {
        OutError = TEXT("Expected numeric array value for vector/color Niagara user parameter");
        return false;
    }

    if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected color array with at least 3 values");
            return false;
        }
        const float Alpha = Values.Num() >= 4 ? static_cast<float>(Values[3]) : 1.0f;
        return Store.SetParameterValue<FLinearColor>(
            FLinearColor(static_cast<float>(Values[0]), static_cast<float>(Values[1]), static_cast<float>(Values[2]), Alpha),
            Parameter,
            false);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
    {
        if (Values.Num() < 2)
        {
            OutError = TEXT("Expected vec2 array with 2 values");
            return false;
        }
        return Store.SetParameterValue<FVector2f>(FVector2f(Values[0], Values[1]), Parameter, false);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected vec3 array with 3 values");
            return false;
        }
        return Store.SetParameterValue<FVector3f>(FVector3f(Values[0], Values[1], Values[2]), Parameter, false);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
    {
        if (Values.Num() < 4)
        {
            OutError = TEXT("Expected vec4 array with 4 values");
            return false;
        }
        return Store.SetParameterValue<FVector4f>(FVector4f(Values[0], Values[1], Values[2], Values[3]), Parameter, false);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetPositionDef())
    {
        if (Values.Num() < 3)
        {
            OutError = TEXT("Expected position array with 3 values");
            return false;
        }
        return Store.SetPositionParameterValue(FVector(Values[0], Values[1], Values[2]), Parameter.GetName(), false);
    }

    OutError = FString::Printf(TEXT("Unsupported Niagara user parameter type: %s"), *NiagaraTypeName(TypeDef));
    return false;
}

TSharedPtr<FJsonValue> NiagaraParameterValueToJson(const FNiagaraParameterStore& Store, const FNiagaraVariable& Parameter)
{
    const FNiagaraTypeDefinition& TypeDef = Parameter.GetType();
    if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        return MakeShared<FJsonValueNumber>(Store.GetParameterValue<float>(Parameter));
    }
    if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
    {
        return MakeShared<FJsonValueNumber>(Store.GetParameterValue<int32>(Parameter));
    }
    if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        const FNiagaraBool Value = Store.GetParameterValue<FNiagaraBool>(Parameter);
        return MakeShared<FJsonValueBoolean>(Value.GetValue());
    }
    if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
    {
        const FLinearColor Value = Store.GetParameterValue<FLinearColor>(Parameter);
        return NumberArrayToJson({ Value.R, Value.G, Value.B, Value.A });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
    {
        const FVector2f Value = Store.GetParameterValue<FVector2f>(Parameter);
        return NumberArrayToJson({ Value.X, Value.Y });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
    {
        const FVector3f Value = Store.GetParameterValue<FVector3f>(Parameter);
        return NumberArrayToJson({ Value.X, Value.Y, Value.Z });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
    {
        const FVector4f Value = Store.GetParameterValue<FVector4f>(Parameter);
        return NumberArrayToJson({ Value.X, Value.Y, Value.Z, Value.W });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetPositionDef())
    {
        const FVector* Value = Store.GetPositionParameterValue(Parameter.GetName());
        if (Value)
        {
            return NumberArrayToJson({ Value->X, Value->Y, Value->Z });
        }
    }
    return MakeShared<FJsonValueNull>();
}

bool IsMcpSupportedNiagaraArrayDataInterfaceType(const FNiagaraTypeDefinition& TypeDef)
{
    UClass* InputClass = TypeDef.GetClass();
    return TypeDef.IsDataInterface() &&
        InputClass &&
        (InputClass->IsChildOf(UNiagaraDataInterfaceArrayPosition::StaticClass()) ||
            InputClass->IsChildOf(UNiagaraDataInterfaceArrayFloat3::StaticClass()) ||
            InputClass->IsChildOf(UNiagaraDataInterfaceArrayFloat2::StaticClass()) ||
            InputClass->IsChildOf(UNiagaraDataInterfaceArrayFloat::StaticClass()) ||
            InputClass->IsChildOf(UNiagaraDataInterfaceArrayInt32::StaticClass()) ||
            IsMeshArrayClass(InputClass));
}

bool IsSettableNiagaraUserParameterType(const FNiagaraTypeDefinition& TypeDef)
{
    return TypeDef == FNiagaraTypeDefinition::GetFloatDef() ||
        TypeDef == FNiagaraTypeDefinition::GetIntDef() ||
        TypeDef == FNiagaraTypeDefinition::GetBoolDef() ||
        TypeDef == FNiagaraTypeDefinition::GetColorDef() ||
        TypeDef == FNiagaraTypeDefinition::GetVec2Def() ||
        TypeDef == FNiagaraTypeDefinition::GetVec3Def() ||
        TypeDef == FNiagaraTypeDefinition::GetVec4Def() ||
        TypeDef == FNiagaraTypeDefinition::GetPositionDef() ||
        IsMcpSupportedNiagaraArrayDataInterfaceType(TypeDef) ||
        (TypeDef.IsUObject() && !TypeDef.IsDataInterface());
}

bool AddUserForPath(FWork& W,const FNiagaraVariable& V)
{
    if(!W.SDK(TEXT("UserStore_AddParameter"),true))return false;
    auto Row=W.MutationRow(TEXT("UserStore_AddParameter"));Row->SetStringField(TEXT("parameter_name"),V.GetName().ToString());Row->SetStringField(TEXT("type_identity"),TypeIdentity(V.GetType()));
    const bool Added=W.System->GetExposedParameters().AddParameter(V,true,true);
    Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_added"),Added);W.Prefix->SetBoolField(TEXT("user_parameter_add_returned"),true);W.Prefix->SetBoolField(TEXT("user_parameter_add_success"),Added);W.R.bChanged|=Added;
    if(!W.After(TEXT("UserStore_AddParameter")))return false;return Added;
}
UNiagaraDataInterface* CreateUserDIForPath(FWork& W,UClass* Class)
{
    if(!W.Pin(Class)||!W.SDK(TEXT("NewObject_UserArrayDI"),true))return nullptr;
    W.Prefix->SetStringField(TEXT("DI_requested_class"),Class->GetPathName());W.Prefix->SetStringField(TEXT("DI_requested_owner"),W.System->GetPathName());
    UNiagaraDataInterface* DI=NewObject<UNiagaraDataInterface>(W.System.Get(),Class,NAME_None,UNiagaraDataInterface::BuildObjectFlagsForOwner(W.System.Get(),RF_Transactional));
    W.Prefix->SetBoolField(TEXT("DI_new_object_returned_nonnull"),DI!=nullptr);
    if(!W.After(TEXT("NewObject_UserArrayDI")))return nullptr;
    if(DI&&(!W.Own(DI)||DI->GetClass()!=Class)){W.Fail(TEXT("niagara_input_path_DI_created_identity_invalid"),TEXT("Returned DI differs from requested exact class/System owner."));return nullptr;}
    W.R.bChanged|=DI!=nullptr;return DI;
}
bool SetUserDIForPath(FWork& W,const FNiagaraVariable& V,UNiagaraDataInterface* DI)
{return VerifyCanonicalUserForPath(W,V)&&W.Own(DI)&&W.Step(TEXT("UserStore_SetDataInterface"),true,[&]{W.System->GetExposedParameters().SetDataInterface(DI,V);});}
bool SetDefaultForPath(FWork& W,FNiagaraUserRedirectionParameterStore& Store,const FNiagaraVariable& V,const TSharedPtr<FJsonValue>& Value,FString& Error)
{
    if(!VerifyCanonicalUserForPath(W,V))return false;
    bool Set=false;if(!W.Step(TEXT("UserStore_SetDefaultValue"),true,[&]{Set=SetNiagaraUserParameterValue(Store,V,Value,Error);W.Prefix->SetBoolField(TEXT("default_value_set_returned"),Set);}))return false;return Set;
}
bool SafeDefaultNumbers(const FNiagaraTypeDefinition& Type,const TSharedPtr<FJsonValue>& Value)
{
    if(!Value)return true;
    auto FloatSafe=[](double X){return FMath::IsFinite(X)&&X>=-static_cast<double>(MAX_flt)&&X<=static_cast<double>(MAX_flt);};
    if(Type==FNiagaraTypeDefinition::GetIntDef()){double X=0;if(Value->TryGetNumber(X)&&(!FMath::IsFinite(X)||X<MIN_int32||X>MAX_int32))return Current->Fail(TEXT("niagara_input_path_numeric_range_unsupported"),TEXT("Numeric default is outside the SDK int32 conversion range."));}
    else if(Type==FNiagaraTypeDefinition::GetFloatDef()){double X=0;if(Value->TryGetNumber(X)&&!FloatSafe(X))return Current->Fail(TEXT("niagara_input_path_numeric_range_unsupported"),TEXT("Numeric default is outside finite float range."));}
    else if(Value->Type==EJson::Array&&Type!=FNiagaraTypeDefinition::GetPositionDef()){for(const auto& V:Value->AsArray()){double X=0;if(V&&V->TryGetNumber(X)&&!FloatSafe(X))return Current->Fail(TEXT("niagara_input_path_numeric_range_unsupported"),TEXT("Vector/color default is outside finite float range."));}}
    return true;
}

TSharedPtr<FJsonValue> ExpectedScalarForPath(const FNiagaraVariable& User,const TSharedPtr<FJsonValue>& Value)
{
    // Scalar-only local storage. DI/default-object branches have already been
    // rejected, and no UObject/interface initialization or layout rebind is requested.
    FNiagaraParameterStore Preview;
    if(!Preview.AddParameter(User,false,false))return nullptr;
    FString Error;if(!SetNiagaraUserParameterValue(Preview,User,Value,Error))return nullptr;
    return NiagaraParameterValueToJson(Preview,User);
}
bool SameProjectedValue(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A||!B)return false;
    auto OA=MakeShared<FJsonObject>();OA->SetField(TEXT("value"),A);auto OB=MakeShared<FJsonObject>();OB->SetField(TEXT("value"),B);
    return Json(OA).Equals(Json(OB),ESearchCase::CaseSensitive);
}

bool VerifyBoundUser(FWork& W,UNiagaraNodeFunctionCall* Function,const FNiagaraParameterHandle& Alias,const FNiagaraVariable& User,const TArray<FUserValue>& Before,bool AllowNew,const TSharedPtr<FJsonValue>& ExpectedDefault)
{
    if(!VerifyUnrelatedUserValues(W,Before,User,AllowNew)||!ContinueRead())return false;
    if(ExpectedDefault&&!SameProjectedValue(ExpectedDefault,NiagaraParameterValueToJson(W.System->GetExposedParameters(),User)))return W.Fail(TEXT("niagara_input_path_default_readback_failed"),TEXT("Actual selected default differs from the requested typed scalar/vector/position projection."));
    UEdGraphPin* Pin=FindNiagaraStackFunctionInputOverridePin(Function,Alias);
    if(!Pin||!FindExpectedPin(W,Pin)||Pin->LinkedTo.Num()!=1||!FindExpectedPin(W,Pin->LinkedTo[0]))return W.Fail(TEXT("niagara_input_path_binding_readback_failed"),TEXT("Exact override link was not observed after binding."));
    UEdGraphPin* Linked=Pin->LinkedTo[0];auto* Node=Linked->GetOwningNodeUnchecked();
    if(!Node->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive)||!Linked->PinName.ToString().Equals(User.GetName().ToString(),ESearchCase::CaseSensitive)||UEdGraphSchema_Niagara::PinToTypeDefinition(Linked)!=User.GetType())return W.Fail(TEXT("niagara_input_path_binding_readback_failed"),TEXT("Binding source is not the requested typed User getter."));
    return true;
}

TSharedPtr<FJsonObject> FWork::Projection()
{
    auto O=MakeShared<FJsonObject>();
    auto Store=CaptureUserValues(*this);if(!ContinueRead())return O;O->SetBoolField(TEXT("source_projection_complete"),false);if(!Admit(TEXT("target_projection")))return O;
    TArray<TSharedPtr<FJsonValue>> ObjectRows,GraphRows;
    for(const auto& I:Objects){if(!Budget(1))return O;auto* P=I->Object.Get();auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),P->GetPathName());V->SetStringField(TEXT("class"),P->GetClass()->GetPathName());V->SetStringField(TEXT("outer"),P->GetOuter()?P->GetOuter()->GetPathName():FString());V->SetStringField(TEXT("package"),P->GetOutermost()->GetName());V->SetBoolField(TEXT("dirty"),P->GetOutermost()->IsDirty());V->SetBoolField(TEXT("retired_script_subtree"),I->Retired);ObjectRows.Add(MakeShared<FJsonValueObject>(V));}
    for(const auto& GE:Graphs){auto* G=GE.Graph.Get();auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),G->GetPathName());V->SetStringField(TEXT("typed_metadata"),GraphMetadataSignature(G));TArray<TSharedPtr<FJsonValue>> Nodes;
        for(UEdGraphNode* N:G->Nodes){if(!Budget(1))return O;auto NV=MakeShared<FJsonObject>();NV->SetStringField(TEXT("guid"),N->NodeGuid.ToString());NV->SetStringField(TEXT("path"),N->GetPathName());if(auto* H=Cast<UNiagaraNodeCustomHlsl>(N)){const FString Body=GetNiagaraCustomHlslString(H);if(!Budget(0,Body.Len()))return O;NV->SetStringField(TEXT("custom_hlsl_body"),Body);}TArray<TSharedPtr<FJsonValue>> Pins;
            for(auto* P:N->Pins){if(!Budget(1,P->DefaultValue.Len()))return O;auto PV=MakeShared<FJsonObject>();PV->SetStringField(TEXT("id"),P->PinId.ToString());PV->SetStringField(TEXT("name"),P->PinName.ToString());PV->SetNumberField(TEXT("direction"),P->Direction);PV->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());PV->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());PV->SetStringField(TEXT("default_value"),P->DefaultValue);PV->SetStringField(TEXT("autogenerated_default"),P->AutogeneratedDefaultValue);PV->SetStringField(TEXT("default_text"),P->DefaultTextValue.ToString());PV->SetStringField(TEXT("default_object"),P->DefaultObject?P->DefaultObject->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:P->LinkedTo){if(!Budget(1))return O;auto LV=MakeShared<FJsonObject>();LV->SetStringField(TEXT("owner"),L->GetOwningNodeUnchecked()->NodeGuid.ToString());LV->SetStringField(TEXT("pin"),L->PinId.ToString());Links.Add(MakeShared<FJsonValueObject>(LV));}PV->SetArrayField(TEXT("links"),Links);Pins.Add(MakeShared<FJsonValueObject>(PV));}
            NV->SetArrayField(TEXT("pins"),Pins);Nodes.Add(MakeShared<FJsonValueObject>(NV));}
        V->SetArrayField(TEXT("nodes"),Nodes);GraphRows.Add(MakeShared<FJsonValueObject>(V));}
    O->SetArrayField(TEXT("objects"),ObjectRows);O->SetArrayField(TEXT("graphs"),GraphRows);
    if(System.IsValid()){auto* S=System.Get();O->SetStringField(TEXT("system_path"),S->GetPathName());TArray<TSharedPtr<FJsonValue>> SystemScratch,EmitterRows;for(UNiagaraScript* P:S->ScratchPadScripts)SystemScratch.Add(MakeShared<FJsonValueString>(P?P->GetPathName():FString()));
        for(const auto& H:S->GetEmitterHandles()){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("id"),H.GetId().ToString());V->SetStringField(TEXT("version"),H.GetInstance().Version.ToString());TArray<TSharedPtr<FJsonValue>> Stages,Scripts;auto* Data=H.GetEmitterData();if(Data){for(UNiagaraSimulationStageBase* P:Data->GetSimulationStages())Stages.Add(MakeShared<FJsonValueString>(P?P->GetPathName():FString()));if(Data->ScratchPads){for(UNiagaraScript* P:Data->ScratchPads->Scripts)Scripts.Add(MakeShared<FJsonValueString>(P?P->GetPathName():FString()));}}V->SetArrayField(TEXT("stages"),Stages);V->SetArrayField(TEXT("scratch"),Scripts);EmitterRows.Add(MakeShared<FJsonValueObject>(V));}
        O->SetArrayField(TEXT("system_scratch"),SystemScratch);O->SetArrayField(TEXT("emitters"),EmitterRows);}
    TArray<TSharedPtr<FJsonValue>> UserRows;for(const auto& E:Store){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),E.Variable.GetName().ToString());V->SetStringField(TEXT("type_identity"),TypeIdentity(E.Variable.GetType()));V->SetNumberField(TEXT("byte_count"),E.Bytes.Num());V->SetStringField(TEXT("bytes_md5"),E.Bytes.IsEmpty()?FString():FMD5::HashBytes(E.Bytes.GetData(),E.Bytes.Num()));V->SetStringField(TEXT("DI_path"),E.DI.IsValid()?E.DI->GetPathName():FString());V->SetStringField(TEXT("object_path"),E.Object.IsValid()?E.Object->GetPathName():FString());V->SetBoolField(TEXT("has_position"),E.HasPosition);if(E.HasPosition){V->SetNumberField(TEXT("position_x"),E.Position.X);V->SetNumberField(TEXT("position_y"),E.Position.Y);V->SetNumberField(TEXT("position_z"),E.Position.Z);}UserRows.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("exposed_user_parameters"),UserRows);
    O->SetBoolField(TEXT("source_projection_complete"),true);return O;
}

struct FIDCompileSnapshot
{
 TWeakObjectPtr<UNiagaraScript> Script;FString OwnerKind,OwnerName,ScriptPath,Usage;
 FNiagaraVMExecutableDataId CachedId,ComputedId;ENiagaraScriptCompileStatus CompileStatus=ENiagaraScriptCompileStatus::NCS_Unknown;
};
bool IDReady(ENiagaraScriptCompileStatus S){return S==ENiagaraScriptCompileStatus::NCS_UpToDate||S==ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings||S==ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings;}
bool IDWarn(ENiagaraScriptCompileStatus S){return S==ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings||S==ENiagaraScriptCompileStatus::NCS_ComputeUpToDateWithWarnings;}
bool IDError(ENiagaraScriptCompileStatus S){return S==ENiagaraScriptCompileStatus::NCS_Error;}
FString IDFingerprint(const FNiagaraVMExecutableDataId& ID){if(!ID.IsValid())return FString();FString Key;ID.AppendKeyString(Key);return FMD5::HashAnsiString(*Key);}
TSharedPtr<FJsonObject> IDSnapshotJson(const FIDCompileSnapshot& S,int32 Index)
{
 auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("script_index"),Index);J->SetStringField(TEXT("owner_kind"),S.OwnerKind);J->SetStringField(TEXT("owner_name"),S.OwnerName);J->SetStringField(TEXT("script_path"),S.ScriptPath);J->SetStringField(TEXT("usage"),S.Usage);
 J->SetStringField(TEXT("compile_status"),StaticEnum<ENiagaraScriptCompileStatus>()->GetNameStringByValue(static_cast<int64>(S.CompileStatus)));
 J->SetBoolField(TEXT("has_error"),IDError(S.CompileStatus));J->SetBoolField(TEXT("has_warning"),IDWarn(S.CompileStatus));J->SetBoolField(TEXT("cached_id_valid"),S.CachedId.IsValid());J->SetBoolField(TEXT("computed_id_valid"),S.ComputedId.IsValid());
 J->SetStringField(TEXT("cached_id_fingerprint"),IDFingerprint(S.CachedId));J->SetStringField(TEXT("computed_id_fingerprint"),IDFingerprint(S.ComputedId));J->SetBoolField(TEXT("source_synchronized"),S.CachedId.IsValid()&&S.ComputedId.IsValid()&&S.CachedId==S.ComputedId);return J;
}
bool IDCaptureCompile(FWork& W,TArray<FIDCompileSnapshot>& Out)
{
 if(!W.Admit(TEXT("unused_compile_snapshot_roles")))return false;TSet<const UNiagaraScript*> Seen;
 auto Add=[&](UNiagaraScript* S,const FString& Kind,const FString& Name)->bool
 {
  if(!S||Seen.Contains(S))return true;if(!W.Pin(S)||!W.Budget(1))return false;Seen.Add(S);
  FIDCompileSnapshot V;V.Script=S;V.OwnerKind=Kind;V.OwnerName=Name;V.ScriptPath=S->GetPathName();
  V.Usage=StaticEnum<ENiagaraScriptUsage>()->GetNameStringByValue(static_cast<int64>(S->GetUsage()));
  if(!W.SDK(TEXT("unused_GetVMExecutableDataCompilationId")))return false;V.CachedId=S->GetVMExecutableDataCompilationId();if(!W.After(TEXT("unused_GetVMExecutableDataCompilationId")))return false;
  if(!W.SDK(TEXT("unused_ComputeVMCompilationId")))return false;S->ComputeVMCompilationId(V.ComputedId,FGuid());if(!W.After(TEXT("unused_ComputeVMCompilationId")))return false;
  if(!W.SDK(TEXT("unused_GetLastCompileStatus")))return false;V.CompileStatus=S->GetLastCompileStatus();if(!W.After(TEXT("unused_GetLastCompileStatus")))return false;
  Out.Add(MoveTemp(V));return true;
 };
 if(!Add(W.Spawn.Get(),TEXT("system"),TEXT("system_spawn"))||!Add(W.Update.Get(),TEXT("system"),TEXT("system_update")))return false;
 for(int32 I=0;I<W.Emitters.Num();++I)
 {
  if(!W.SDK(TEXT("unused_GetEmitterScripts")))return false;const auto& H=W.System->GetEmitterHandles()[I];auto* D=H.GetEmitterData();TArray<UNiagaraScript*> Scripts;if(D)D->GetScripts(Scripts,false,false);
  const FString Name=H.GetName().ToString();if(!W.After(TEXT("unused_GetEmitterScripts"))||!W.Budget(Scripts.Num()))return false;
  for(auto* S:Scripts)if(!Add(S,FString::Printf(TEXT("emitter:%d"),I),Name))return false;
 }
 return true;
}
struct FIDCompileCounts
{
 int32 Known=0,Errors=0,Warnings=0,NonReady=0,Unsync=0,Changed=0;TArray<TSharedPtr<FJsonValue>> Json;
};
FIDCompileCounts IDCount(const TArray<FIDCompileSnapshot>& Snapshots,const TArray<FIDCompileSnapshot>* Baseline=nullptr)
{
 FIDCompileCounts C;
 for(const auto& S:Snapshots)
 {
  C.Json.Add(MakeShared<FJsonValueObject>(IDSnapshotJson(S,C.Json.Num())));if(S.CompileStatus==ENiagaraScriptCompileStatus::NCS_Unknown)continue;
  ++C.Known;C.Errors+=IDError(S.CompileStatus)?1:0;C.Warnings+=IDWarn(S.CompileStatus)?1:0;C.NonReady+=IDReady(S.CompileStatus)?0:1;
  C.Unsync+=S.CachedId.IsValid()&&S.ComputedId.IsValid()&&S.CachedId==S.ComputedId?0:1;
  if(Baseline){const auto* B=Baseline->FindByPredicate([&](const auto& X){return X.ScriptPath.Equals(S.ScriptPath,ESearchCase::CaseSensitive);});if(B&&B->ComputedId.IsValid()&&S.ComputedId.IsValid()&&B->ComputedId!=S.ComputedId)++C.Changed;}
 }
 return C;
}
// Read the installed reflected UPROPERTY through exported reflection; the private
// unexported MapGet class/GetDefaultPin symbol is not linked or written.
bool IDDefaultPin(FWork& W,UEdGraphNode* N,UEdGraphPin* Output,UEdGraphPin*& Default)
{
 Default=nullptr;if(!W.Admit(TEXT("unused_default_pair")))return false;
 if(!N->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive))return true;
 const FMapProperty* Map=FindFProperty<FMapProperty>(N->GetClass(),TEXT("PinOutputToPinDefaultPersistentId"));
 const FStructProperty* K=Map?CastField<FStructProperty>(Map->KeyProp):nullptr;const FStructProperty* V=Map?CastField<FStructProperty>(Map->ValueProp):nullptr;
 if(!Map||!K||!V||Map->ArrayDim!=1||K->Struct!=TBaseStructure<FGuid>::Get()||V->Struct!=TBaseStructure<FGuid>::Get())return W.Fail(TEXT("niagara_unused_module_input_default_mapping_unavailable"),TEXT("Exact installed reflected Guid→Guid default-pair mapping unavailable."));
 FScriptMapHelper Values(Map,Map->ContainerPtrToValuePtr<void>(N));FGuid DefaultId;int32 Found=0;
 if(!W.Budget(Values.Num())||Values.Num()>16384)return false;
 for(int32 I=0;I<Values.GetMaxIndex();++I)if(Values.IsValidIndex(I)&&*reinterpret_cast<const FGuid*>(Values.GetKeyPtr(I))==Output->PersistentGuid){DefaultId=*reinterpret_cast<const FGuid*>(Values.GetValuePtr(I));++Found;}
 if(Found!=1||!DefaultId.IsValid())return W.Fail(TEXT("niagara_unused_module_input_default_mapping_ambiguous"),TEXT("Selected output lacks exact unique persistent default pair."));
 for(auto* P:N->Pins)if(P&&P->PersistentGuid==DefaultId){if(Default)return W.Fail(TEXT("niagara_unused_module_input_default_mapping_ambiguous"),TEXT("Default persistent pin ID is duplicated."));Default=P;}
 return (Default&&Default->Direction==EGPD_Input&&Default->LinkedTo.IsEmpty()&&FindExpectedPin(W,Default))||W.Fail(TEXT("niagara_unused_module_input_default_pair_not_ready"),TEXT("Exact captured unlinked default input pair required."));
}
bool IDRemovePin(FWork& W,UEdGraphNode* N,const FGuid& Id,bool Pair)
{
 if(!W.Admit(TEXT("unused_pin_removal_reacquire"))||!Live(N))return false;
 UEdGraphPin* P=nullptr;for(auto* C:N->Pins)if(C&&C->PinId==Id){if(P)return W.Fail(TEXT("niagara_unused_module_input_pin_id_ambiguous"),TEXT("Duplicate selected PinId."));P=C;}
 if(!P||!W.PinForWrite(P))return W.Fail(TEXT("niagara_unused_module_input_pin_expired"),TEXT("Selected pin is absent from current captured node."));
 UEdGraphPin* Default=nullptr;if(Pair&&!IDDefaultPin(W,N,P,Default))return false;
 if(!W.SDK(TEXT("unused_RemovePin"),true))return false;
 if(Pair&&W.RemovalPairPlan&&!W.RemovalPairPlan(N,P->PersistentGuid))return W.Fail(TEXT("niagara_unused_module_input_default_mapping_plan_failed"),TEXT("Selected persistent pair is not in its captured map plan."));
 for(auto& G:W.Graphs)for(auto& Node:G.Nodes)
 {
  for(auto& E:Node.Pins){E.Links.Remove(P);if(Default)E.Links.Remove(Default);}
  if(Node.Node.Get()==N)Node.Pins.RemoveAll([&](const auto& E){return E.Pointer==P||E.Pointer==Default;});
 }
 const bool Removed=N->RemovePin(P);W.Prefix->SetBoolField(TEXT("unused_RemovePin_returned"),Removed);W.R.bChanged|=Removed;
 if(!W.After(TEXT("unused_RemovePin")))return false;
 return Removed||W.Fail(TEXT("niagara_unused_module_input_pin_removal_failed"),TEXT("Public RemovePin returned false; retained actual prefix, no replay."));
}
bool IDRemoveMetadata(FWork& W,UNiagaraGraph* G,const FNiagaraVariable& Variable,int32& Removed)
{
 if(!W.GraphForWrite(G))return false;
 if(auto* V=G->GetAllMetaData().FindRef(Variable).Get()){if(!W.Modify(V))return false;}
 const FString Expected=GraphMetadataWithout(G,&Variable);
 if(!W.SDK(TEXT("unused_RemoveMetadata"),true))return false;
 auto* E=W.Graphs.FindByPredicate([&](const auto& X){return X.Graph.Get()==G;});if(!E)return false;E->Meta=Expected;
 Removed=G->GetAllMetaData().Remove(Variable);W.R.bChanged|=Removed>0;W.Prefix->SetNumberField(TEXT("unused_last_removed_metadata_count"),Removed);
 return W.After(TEXT("unused_RemoveMetadata"));
}


struct FIDCallRole{TWeakObjectPtr<UNiagaraNodeFunctionCall> Node;TWeakObjectPtr<UNiagaraScript> Script;FGuid Version;};
struct FIDMapRole{TWeakObjectPtr<UEdGraphNode> Node;TMap<FGuid,FGuid> Pairs;};
bool IDMapPairs(UEdGraphNode* N,TMap<FGuid,FGuid>& Out)
{
 const FMapProperty* P=FindFProperty<FMapProperty>(N->GetClass(),TEXT("PinOutputToPinDefaultPersistentId"));
 const FStructProperty* K=P?CastField<FStructProperty>(P->KeyProp):nullptr;const FStructProperty* V=P?CastField<FStructProperty>(P->ValueProp):nullptr;
 if(!P||!K||!V||P->ArrayDim!=1||K->Struct!=TBaseStructure<FGuid>::Get()||V->Struct!=TBaseStructure<FGuid>::Get())return false;
 FScriptMapHelper M(P,P->ContainerPtrToValuePtr<void>(N));if(M.Num()>16384)return false;
 for(int32 I=0;I<M.GetMaxIndex();++I)if(M.IsValidIndex(I))Out.Add(*reinterpret_cast<const FGuid*>(M.GetKeyPtr(I)),*reinterpret_cast<const FGuid*>(M.GetValuePtr(I)));return true;
}
struct FIDRemovalPlan
{
 TWeakObjectPtr<UNiagaraNodeCustomHlsl> Custom;FNiagaraFunctionSignature Signature;FString Body;
 TWeakObjectPtr<UNiagaraSystem> System;TArray<FVersionedNiagaraEmitterData*> EmitterData;TArray<UNiagaraScript*> CompileRoles;
 TArray<FIDCallRole> Calls;TArray<FIDMapRole> Maps;TArray<FUserValue> Users;FNiagaraVariable User;bool UserRemoved=false;
 bool Check()const
 {
  auto* H=Custom.Get();if(!Live(H)||!FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&H->Signature,&Signature,PPF_None)||!GetNiagaraCustomHlslString(H).Equals(Body,ESearchCase::CaseSensitive))return false;
  auto* Root=System.Get();if(!Live(Root))return false;TArray<UNiagaraScript*> CurrentRoles{Root->GetSystemSpawnScript(),Root->GetSystemUpdateScript()};for(auto* D:EmitterData)if(D)D->GetScripts(CurrentRoles,false,false);if(CurrentRoles!=CompileRoles)return false;
  for(const auto& C:Calls){auto* N=C.Node.Get();if(!Live(N)||N->FunctionScript!=C.Script.Get()||N->SelectedScriptVersion!=C.Version)return false;}
  for(const auto& M:Maps){if(!Live(M.Node.Get()))return false;TMap<FGuid,FGuid> Now;if(!IDMapPairs(M.Node.Get(),Now)||!Now.OrderIndependentCompareEqual(M.Pairs))return false;}return true;
 }
};
bool IDUsersPreserved(FWork& W,const FIDRemovalPlan& Plan)
{
 auto CurrentUsers=CaptureUserValues(W);if(!ContinueRead())return false;int32 BeforeOther=0,AfterOther=0,Selected=0;
 auto Match=[&](const FUserValue& V){return V.Variable.GetName()==Plan.User.GetName()&&V.Variable.GetType()==Plan.User.GetType();};
 for(const auto& V:Plan.Users){if(Match(V))continue;++BeforeOther;const auto* N=CurrentUsers.FindByPredicate([&](const auto& X){return X.Variable.GetName()==V.Variable.GetName()&&X.Variable.GetType()==V.Variable.GetType();});if(!N||!SameUserValue(V,*N))return W.Fail(TEXT("niagara_unused_module_input_unselected_user_changed"),TEXT("An unselected typed User declaration/value/DI/object changed."));}
 for(const auto& V:CurrentUsers){if(Match(V))++Selected;else ++AfterOther;}
 return (BeforeOther==AfterOther&&Selected==(Plan.UserRemoved?0:1))||W.Fail(TEXT("niagara_unused_module_input_user_store_shape_changed"),TEXT("User store differs from its one planned declaration removal."));
}
// This one SDK can reallocate exactly this retained node's pins. Match current
// pins by exact ordered name/direction/type/defaults and transform only reciprocal
// links from old physical endpoints; never inspect an old pin after the callback.
bool IDRefreshNode(FWork& W,UNiagaraNodeFunctionCall* Node)
{
 if(!W.Admit(TEXT("unused_refresh_plan"))||!W.Own(Node))return false;auto* G=Cast<UNiagaraGraph>(Node->GetGraph());
 auto* GE=W.Graphs.FindByPredicate([&](const auto& X){return X.Graph.Get()==G;});if(!GE)return false;
 const FGraphIdentity Before=*GE;const auto* Target=Before.Nodes.FindByPredicate([&](const auto& N){return N.Node.Get()==Node;});if(!Target)return false;
 if(!W.SDK(TEXT("unused_RefreshFromExternalChanges"),true))return false;
 {
  TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);
  const bool Reloaded=Node->RefreshFromExternalChanges();W.Prefix->SetBoolField(TEXT("last_refresh_returned_reloaded"),Reloaded);
  if(!W.Admit(TEXT("unused_refresh_returned_roles")))return false;
  if(G->Nodes.Num()!=Before.Nodes.Num()||!GraphMetadataSignature(G).Equals(Before.Meta,ESearchCase::CaseSensitive)||Node->Pins.Num()!=Target->Pins.Num())return W.Fail(TEXT("niagara_unused_module_input_refresh_shape_changed"),TEXT("Refresh changed an unplanned graph/node/pin inventory."));
  TMap<UEdGraphPin*,UEdGraphPin*> Remap;
  for(int32 I=0;I<Target->Pins.Num();++I)
  {
   const auto& P=Target->Pins[I];auto* N=Node->Pins[I];
   if(!N||N->GetOwningNodeUnchecked()!=Node||(P.PersistentId.IsValid()?N->PersistentGuid!=P.PersistentId:N->PinId!=P.Id)||N->Direction!=P.Direction||N->PinName!=P.Name||N->PinType!=P.Type||N->DefaultValue!=P.DefaultValue||N->AutogeneratedDefaultValue!=P.AutoDefault||N->DefaultTextValue.ToString()!=P.DefaultText||N->DefaultObject!=P.DefaultObject.Get())return W.Fail(TEXT("niagara_unused_module_input_refresh_retained_pin_changed"),TEXT("Retained current pin fields/defaults differ after exact node refresh."));
   Remap.Add(P.Pointer,N);
  }
  for(int32 I=0;I<Before.Nodes.Num();++I)
  {
   const auto& E=Before.Nodes[I];auto* N=E.Node.Get();
   if(G->Nodes[I]!=N||!Live(N)||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num())return W.Fail(TEXT("niagara_unused_module_input_refresh_other_node_changed"),TEXT("Refresh changed another retained node/header."));
   for(int32 J=0;J<E.Pins.Num();++J)
   {
    const auto& P=E.Pins[J];auto* Now=N->Pins[J];UEdGraphPin* Expected=N==Node?Remap.FindChecked(P.Pointer):P.Pointer;
    if(Now!=Expected||(!Remap.Contains(P.Pointer)&&!PinHeaderSame(P,Now)))return W.Fail(TEXT("niagara_unused_module_input_refresh_other_pin_changed"),TEXT("Refresh changed unrelated physical pin membership/header."));
    TArray<UEdGraphPin*> Links;for(auto* L:P.Links)Links.Add(Remap.Contains(L)?Remap.FindChecked(L):L);
    if(Now->LinkedTo!=Links)return W.Fail(TEXT("niagara_unused_module_input_refresh_links_changed"),TEXT("Refresh changed unplanned ordered reciprocal links."));
   }
  }
  auto* Source=W.Sources.FindByPredicate([&](const auto& S){return S.Graph.Get()==G;});if(!Source)return false;
  W.Graphs.RemoveAll([&](const auto& X){return X.Graph.Get()==G;});if(!W.CaptureGraph(Source->Source.Get()))return false;
 }
 return W.After(TEXT("unused_RefreshFromExternalChanges"));
}

TSharedPtr<FJsonObject> HandleRemoveUnusedPath(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    bool bMutationStarted = false;
    auto MakeSafetyError = [&W,&bMutationStarted](const FString& Message, const FString& ErrorCode)
    {
        W.Fail(*ErrorCode,Message);TSharedPtr<FJsonObject> Error=W.Error(Message);
        Error->SetStringField(TEXT("error_code"), ErrorCode);
        Error->SetBoolField(TEXT("mutation_applied_in_memory"), bMutationStarted);
        Error->SetBoolField(TEXT("requires_reload_or_undo"), bMutationStarted);
        Error->SetBoolField(TEXT("saved"), false);
        return Error;
    };

    if (!Params.IsValid())
    {
        return MakeSafetyError(TEXT("Missing parameters"), TEXT("niagara_unused_module_input_missing_parameters"));
    }

    FString SystemPath;
    FString ScratchPadName;
    FString ScratchPadScriptPath;
    FString ModuleName;
    FString ModuleNodeGuidString;
    FString InputName;
    FString LinkedUserParameterName;
    FString CustomHlslSignatureName;
    FString CustomHlslInputName;
    FString ExpectedHlslIdentifierAbsent;
    FString ExpectedHlslBodyHash;
    FString ExpectedCustomHlslNodeGuidString;
    FString ExpectedOverrideNodeGuidString;
    FString ExpectedInputType;
    FString ExpectedUserSourceNodeGuidString;
    Params->TryGetStringField(TEXT("system_path"), SystemPath);
    Params->TryGetStringField(TEXT("scratch_pad_name"), ScratchPadName);
    Params->TryGetStringField(TEXT("scratch_pad_script_path"), ScratchPadScriptPath);
    Params->TryGetStringField(TEXT("module_name"), ModuleName);
    Params->TryGetStringField(TEXT("module_node_guid"), ModuleNodeGuidString);
    Params->TryGetStringField(TEXT("input_name"), InputName);
    Params->TryGetStringField(TEXT("linked_user_parameter_name"), LinkedUserParameterName);
    Params->TryGetStringField(TEXT("custom_hlsl_signature_name"), CustomHlslSignatureName);
    Params->TryGetStringField(TEXT("custom_hlsl_input_name"), CustomHlslInputName);
    Params->TryGetStringField(TEXT("expected_hlsl_identifier_absent"), ExpectedHlslIdentifierAbsent);
    Params->TryGetStringField(TEXT("expected_hlsl_body_hash"), ExpectedHlslBodyHash);
    Params->TryGetStringField(TEXT("expected_custom_hlsl_node_guid"), ExpectedCustomHlslNodeGuidString);
    Params->TryGetStringField(TEXT("expected_override_node_guid"), ExpectedOverrideNodeGuidString);
    Params->TryGetStringField(TEXT("expected_input_type"), ExpectedInputType);
    Params->TryGetStringField(TEXT("expected_user_source_node_guid"), ExpectedUserSourceNodeGuidString);

    SystemPath.TrimStartAndEndInline();
    ScratchPadName.TrimStartAndEndInline();
    ScratchPadScriptPath.TrimStartAndEndInline();
    ModuleName.TrimStartAndEndInline();
    ModuleNodeGuidString.TrimStartAndEndInline();
    InputName.TrimStartAndEndInline();
    LinkedUserParameterName.TrimStartAndEndInline();
    CustomHlslSignatureName.TrimStartAndEndInline();
    CustomHlslInputName.TrimStartAndEndInline();
    ExpectedHlslIdentifierAbsent.TrimStartAndEndInline();
    ExpectedHlslBodyHash.TrimStartAndEndInline();
    ExpectedCustomHlslNodeGuidString.TrimStartAndEndInline();
    ExpectedOverrideNodeGuidString.TrimStartAndEndInline();
    ExpectedInputType.TrimStartAndEndInline();
    ExpectedUserSourceNodeGuidString.TrimStartAndEndInline();

    if (SystemPath.IsEmpty() || (ScratchPadName.IsEmpty() && ScratchPadScriptPath.IsEmpty()) ||
        ModuleName.IsEmpty() || ModuleNodeGuidString.IsEmpty() || InputName.IsEmpty() ||
        LinkedUserParameterName.IsEmpty() || CustomHlslSignatureName.IsEmpty() ||
        ExpectedHlslIdentifierAbsent.IsEmpty())
    {
        return MakeSafetyError(
            TEXT("system_path, a Scratch Pad selector, module_name, module_node_guid, input_name, linked_user_parameter_name, custom_hlsl_signature_name, and expected_hlsl_identifier_absent are required"),
            TEXT("niagara_unused_module_input_missing_selector"));
    }

    FString ModuleInputName = InputName;
    if (!ModuleInputName.StartsWith(TEXT("Module."), ESearchCase::IgnoreCase))
    {
        if (ModuleInputName.Contains(TEXT(".")))
        {
            return MakeSafetyError(
                TEXT("input_name must be unqualified or use the Module.* namespace"),
                TEXT("niagara_unused_module_input_invalid_namespace"));
        }
        ModuleInputName = FString::Printf(TEXT("Module.%s"), *ModuleInputName);
    }
    const FString ShortInputName = ModuleInputName.RightChop(7);
    if (CustomHlslInputName.IsEmpty())
    {
        CustomHlslInputName = ShortInputName;
    }
    if (!ExpectedHlslIdentifierAbsent.Equals(CustomHlslInputName, ESearchCase::CaseSensitive))
    {
        return MakeSafetyError(
            TEXT("expected_hlsl_identifier_absent must exactly match the selected Custom HLSL input name"),
            TEXT("niagara_unused_module_input_hlsl_guard_must_match_input"));
    }

    if (!LinkedUserParameterName.StartsWith(TEXT("User."), ESearchCase::IgnoreCase))
    {
        if (LinkedUserParameterName.Contains(TEXT(".")))
        {
            return MakeSafetyError(
                TEXT("linked_user_parameter_name must be unqualified or use the User.* namespace"),
                TEXT("niagara_unused_module_input_invalid_user_namespace"));
        }
        LinkedUserParameterName = FString::Printf(TEXT("User.%s"), *LinkedUserParameterName);
    }

    FGuid ModuleNodeGuid;
    if (!FGuid::Parse(ModuleNodeGuidString, ModuleNodeGuid) || !ModuleNodeGuid.IsValid())
    {
        return MakeSafetyError(
            TEXT("module_node_guid must be a valid non-zero GUID"),
            TEXT("niagara_unused_module_input_invalid_module_guid"));
    }

    bool bDryRun = true;
    bool bAllowSourceEdit = false;
    bool bSave = false;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("dry_run"), bDryRun);
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    Params->TryGetBoolField(TEXT("save"), bSave);
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bWaitForCompletion = true;
    double TimeoutSeconds = 10.0;
    double PollIntervalSeconds = 0.1;
    ReadNiagaraCompileWaitParams(Params, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds);

    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Refusing to edit Niagara system outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath),
            TEXT("niagara_unused_module_input_source_edit_not_allowed"));
    }
    if (!bDryRun && GEditor && GEditor->GetPIEWorldContext() != nullptr)
    {
        return MakeSafetyError(
            TEXT("Refusing to mutate a Niagara graph while a play session is active"),
            TEXT("niagara_unused_module_input_play_session_active"));
    }
    if (!bDryRun && (!bRequestCompile || !bWaitForCompletion))
    {
        return MakeSafetyError(
            TEXT("Apply mode requires request_compile=true and wait_for_completion=true"),
            TEXT("niagara_unused_module_input_compile_required"));
    }

    FGuid ExpectedCustomHlslNodeGuid;
    FGuid ExpectedOverrideNodeGuid;
    FGuid ExpectedUserSourceNodeGuid;
    if ((!ExpectedCustomHlslNodeGuidString.IsEmpty() &&
         (!FGuid::Parse(ExpectedCustomHlslNodeGuidString, ExpectedCustomHlslNodeGuid) || !ExpectedCustomHlslNodeGuid.IsValid())) ||
        (!ExpectedOverrideNodeGuidString.IsEmpty() &&
         (!FGuid::Parse(ExpectedOverrideNodeGuidString, ExpectedOverrideNodeGuid) || !ExpectedOverrideNodeGuid.IsValid())) ||
        (!ExpectedUserSourceNodeGuidString.IsEmpty() &&
         (!FGuid::Parse(ExpectedUserSourceNodeGuidString, ExpectedUserSourceNodeGuid) || !ExpectedUserSourceNodeGuid.IsValid())))
    {
        return MakeSafetyError(
            TEXT("Expected Custom HLSL, override, and User source node GUIDs must be valid non-zero GUIDs when provided"),
            TEXT("niagara_unused_module_input_invalid_expected_guid"));
    }

    TArray<FString> ExpectedMapGetGuidStrings = GetRawStringArrayParam(Params, TEXT("expected_scratch_map_get_node_guids"));
    TArray<FString> CanonicalExpectedMapGetGuids;
    TSet<FString> SeenExpectedMapGetGuids;
    for (const FString& GuidString : ExpectedMapGetGuidStrings)
    {
        FGuid Guid;
        if (!FGuid::Parse(GuidString, Guid) || !Guid.IsValid())
        {
            return MakeSafetyError(
                FString::Printf(TEXT("Invalid expected_scratch_map_get_node_guids entry: %s"), *GuidString),
                TEXT("niagara_unused_module_input_invalid_expected_map_get_guid"));
        }
        const FString CanonicalGuid = Guid.ToString(EGuidFormats::DigitsWithHyphens);
        if (SeenExpectedMapGetGuids.Contains(CanonicalGuid))
        {
            return MakeSafetyError(
                TEXT("expected_scratch_map_get_node_guids must not contain duplicates"),
                TEXT("niagara_unused_module_input_duplicate_expected_map_get_guid"));
        }
        SeenExpectedMapGetGuids.Add(CanonicalGuid);
        CanonicalExpectedMapGetGuids.Add(CanonicalGuid);
    }
    CanonicalExpectedMapGetGuids.Sort();

    if (!bDryRun &&
        (ExpectedHlslBodyHash.IsEmpty() || !ExpectedCustomHlslNodeGuid.IsValid() ||
         !ExpectedOverrideNodeGuid.IsValid() || ExpectedInputType.IsEmpty() ||
         !ExpectedUserSourceNodeGuid.IsValid() || CanonicalExpectedMapGetGuids.IsEmpty()))
    {
        return MakeSafetyError(
            TEXT("Apply mode requires expected_hlsl_body_hash, expected_custom_hlsl_node_guid, expected_override_node_guid, expected_input_type, expected_user_source_node_guid, and the non-empty expected_scratch_map_get_node_guids array returned by dry-run"),
            TEXT("niagara_unused_module_input_missing_dry_run_contract"));
    }

    W.R.bSaveRequested=!bDryRun&&bSave;W.R.bCompileRequested=!bDryRun&&bRequestCompile;
    UNiagaraSystem* System=LoadSystem(SystemPath);
    if (!System)
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath),
            TEXT("niagara_unused_module_input_system_not_found"));
    }
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(System->GetPathName()))
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Loaded Niagara system resolves outside %s: %s"), NiagaraTempGenerationRoot, *System->GetPathName()),
            TEXT("niagara_unused_module_input_resolved_source_edit_not_allowed"));
    }
    const auto AllSources=CollectTargetNiagaraScriptSources(System);if(!ContinueRead())return W.Error(W.R.Error);
    UPackage* SystemPackage = System->GetOutermost();
    const bool bPackageWasDirty = SystemPackage && SystemPackage->IsDirty();
    if (!bDryRun && bPackageWasDirty)
    {
        return MakeSafetyError(
            TEXT("Niagara package must be clean before this guarded multi-graph mutation"),
            TEXT("niagara_unused_module_input_dirty_package_refused"));
    }


    TArray<FIDCompileSnapshot> PreMutationCompileSnapshots;int32 PreflightCompileWarningCount=0;
    if(!bDryRun)
    {
        if(!W.DeferredMode){W.Fail(TEXT("niagara_input_path_async_wait_unsupported"),TEXT("Guarded apply requires its same-Job final continuation; no destructive writer dispatched."));return W.Error(W.R.Error);}
        const auto Idle=WaitForNiagaraCompileJson(System,false,TimeoutSeconds,PollIntervalSeconds);
        if(!ContinueRead()||!IDCaptureCompile(W,PreMutationCompileSnapshots))return W.Error(W.R.Error);
        const auto Counts=IDCount(PreMutationCompileSnapshots);PreflightCompileWarningCount=Counts.Warnings;
        if(Idle->GetBoolField(TEXT("outstanding_compilation_requests_after"))||Idle->GetBoolField(TEXT("active_compilations_after"))||Counts.Known==0||Counts.Errors||Counts.NonReady||Counts.Unsync)
        {
            auto Error=MakeSafetyError(TEXT("Niagara compile baseline must be idle, ready and synchronized before guarded mutation"),TEXT("niagara_unused_module_input_compile_baseline_not_ready"));
            Error->SetObjectField(TEXT("compile_wait"),Idle);Error->SetNumberField(TEXT("known_compile_script_count"),Counts.Known);Error->SetNumberField(TEXT("compile_error_count"),Counts.Errors);
            Error->SetNumberField(TEXT("compile_warning_count"),Counts.Warnings);Error->SetNumberField(TEXT("compile_non_ready_count"),Counts.NonReady);Error->SetNumberField(TEXT("compile_unsynchronized_count"),Counts.Unsync);Error->SetArrayField(TEXT("compile_snapshots"),Counts.Json);return Error;
        }
    }


    // This first pass intentionally resolves only system-owned Scratch Pads.
    // That keeps ownership and save behavior exact for the proven ReactiveSurface topology.
    UNiagaraScript* ScratchPadScript = nullptr;
    int32 ScratchPadMatchCount = 0;
    int32 ScratchPadScriptIndex = INDEX_NONE;
    const FString NormalizedScratchPadPath = NormalizeNiagaraObjectPathForLoad(ScratchPadScriptPath);
    for (int32 Index = 0; Index < System->ScratchPadScripts.Num(); ++Index)
    {
        UNiagaraScript* Candidate = System->ScratchPadScripts[Index].Get();
        if (!Candidate)
        {
            continue;
        }
        const bool bNameMatches = ScratchPadName.IsEmpty() || Candidate->GetName().Equals(ScratchPadName, ESearchCase::CaseSensitive);
        const bool bPathMatches = ScratchPadScriptPath.IsEmpty() || Candidate->GetPathName().Equals(NormalizedScratchPadPath, ESearchCase::CaseSensitive);
        if (bNameMatches && bPathMatches)
        {
            ScratchPadScript = Candidate;
            ScratchPadScriptIndex = Index;
            ++ScratchPadMatchCount;
        }
    }
    if (ScratchPadMatchCount != 1 || !ScratchPadScript)
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Scratch Pad selector must resolve to exactly one system-owned script; matched %d"), ScratchPadMatchCount),
            TEXT("niagara_unused_module_input_scratch_selector_ambiguous"));
    }
    if (ScratchPadScript->GetUsage() != ENiagaraScriptUsage::Module)
    {
        return MakeSafetyError(
            TEXT("Target Scratch Pad must be a Module script"),
            TEXT("niagara_unused_module_input_scratch_not_module"));
    }
    UNiagaraScriptSource* ScratchPadSource = Cast<UNiagaraScriptSource>(ScratchPadScript->GetLatestSource());
    UNiagaraGraph* ScratchGraph = ScratchPadSource ? ScratchPadSource->NodeGraph : nullptr;
    if (!ScratchGraph)
    {
        return MakeSafetyError(
            TEXT("Target Scratch Pad has no editable Niagara graph"),
            TEXT("niagara_unused_module_input_scratch_graph_missing"));
    }

    UNiagaraNodeFunctionCall* ReferencingModule = nullptr;
    FVersionedNiagaraEmitterData* ReferencingEmitterData = nullptr;
    FString ReferencingEmitterName;
    int32 ReferencingEmitterIndex = INDEX_NONE;
    int32 ReferencingModuleIndex = INDEX_NONE;
    int32 ReferencingModuleMatchCount = 0;
    int32 CurrentEmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : TArray<FNiagaraEmitterHandle>(System->GetEmitterHandles()))
    {
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(EmitterData->GraphSource);
            for (int32 ModuleIndex = 0; ModuleIndex < FunctionCalls.Num(); ++ModuleIndex)
            {
                UNiagaraNodeFunctionCall* Candidate = FunctionCalls[ModuleIndex];
                if (!Candidate || Candidate->NodeGuid != ModuleNodeGuid)
                {
                    continue;
                }
                FString CandidateName=FunctionNameForPath(Candidate);if(!ContinueRead())return W.Error(W.R.Error);
                if (CandidateName.IsEmpty())
                {
                    CandidateName = Candidate->Signature.GetNameString();
                }
                if (!CandidateName.Equals(ModuleName, ESearchCase::CaseSensitive))
                {
                    continue;
                }
                ReferencingModule = Candidate;
                ReferencingEmitterData = EmitterData;
                ReferencingEmitterName = EmitterHandle.GetName().ToString();
                ReferencingEmitterIndex = CurrentEmitterIndex;
                ReferencingModuleIndex = ModuleIndex;
                ++ReferencingModuleMatchCount;
            }
        }
        ++CurrentEmitterIndex;
    }
    if (ReferencingModuleMatchCount != 1 || !ReferencingModule || !ReferencingEmitterData)
    {
        return MakeSafetyError(
            FString::Printf(TEXT("module_name plus module_node_guid must resolve to exactly one emitter-stack module; matched %d"), ReferencingModuleMatchCount),
            TEXT("niagara_unused_module_input_module_selector_ambiguous"));
    }
    if (ReferencingModule->FunctionScript != ScratchPadScript)
    {
        return MakeSafetyError(
            TEXT("The selected emitter-stack module does not reference the selected Scratch Pad script"),
            TEXT("niagara_unused_module_input_script_reference_mismatch"));
    }
    const int32 ScratchPadReferenceCount = CountNiagaraFunctionScriptReferences(
        CollectTargetNiagaraScriptSources(System), ScratchPadScript);
    if(!ContinueRead())return W.Error(W.R.Error);
    if (ScratchPadReferenceCount != 1)
    {
        return MakeSafetyError(
            FString::Printf(
                TEXT("The selected Scratch Pad must have exactly one function-call reference; found %d"),
                ScratchPadReferenceCount),
            TEXT("niagara_unused_module_input_scratch_has_other_references"));
    }

    UNiagaraNodeCustomHlsl* CustomHlslNode = nullptr;
    UEdGraphPin* CustomHlslInputPin = nullptr;
    int32 CustomHlslMatchCount = 0;
    for (UEdGraphNode* Node : ScratchGraph->Nodes)
    {
        UNiagaraNodeCustomHlsl* Candidate = Cast<UNiagaraNodeCustomHlsl>(Node);
        if (!Candidate || !Candidate->Signature.GetNameString().Equals(CustomHlslSignatureName, ESearchCase::CaseSensitive))
        {
            continue;
        }
        UEdGraphPin* CandidateInput = FindGraphPinByName(Candidate, EGPD_Input, FName(*CustomHlslInputName), false);
        if (!CandidateInput)
        {
            continue;
        }
        CustomHlslNode = Candidate;
        CustomHlslInputPin = CandidateInput;
        ++CustomHlslMatchCount;
    }
    if (CustomHlslMatchCount != 1 || !CustomHlslNode || !CustomHlslInputPin)
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Custom HLSL signature '%s' with input '%s' must resolve exactly once; matched %d"), *CustomHlslSignatureName, *CustomHlslInputName, CustomHlslMatchCount),
            TEXT("niagara_unused_module_input_custom_hlsl_selector_ambiguous"));
    }
    if (!CustomHlslNode->NodeGuid.IsValid() || CustomHlslInputPin->bOrphanedPin)
    {
        return MakeSafetyError(
            TEXT("Selected Custom HLSL node/input lacks a stable non-orphaned graph identity"),
            TEXT("niagara_unused_module_input_custom_hlsl_identity_invalid"));
    }
    if (ExpectedCustomHlslNodeGuid.IsValid() && CustomHlslNode->NodeGuid != ExpectedCustomHlslNodeGuid)
    {
        return MakeSafetyError(
            TEXT("Custom HLSL node GUID no longer matches the dry-run contract"),
            TEXT("niagara_unused_module_input_custom_hlsl_guid_changed"));
    }
    const FString ExistingHlsl = GetNiagaraCustomHlslString(CustomHlslNode);
    const FString ExistingHlslHash = FMD5::HashAnsiString(*ExistingHlsl);
    if (!ExpectedHlslBodyHash.IsEmpty() && !ExistingHlslHash.Equals(ExpectedHlslBodyHash, ESearchCase::CaseSensitive))
    {
        return MakeSafetyError(
            TEXT("Custom HLSL body hash no longer matches the dry-run contract"),
            TEXT("niagara_unused_module_input_hlsl_hash_changed"));
    }
    if (ExistingHlsl.Contains(ExpectedHlslIdentifierAbsent, ESearchCase::CaseSensitive))
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Custom HLSL body still contains guarded identifier '%s'; refusing to remove its input path"), *ExpectedHlslIdentifierAbsent),
            TEXT("niagara_unused_module_input_hlsl_identifier_still_used"));
    }

    const FNiagaraTypeDefinition InputType = PinTypeForPath(CustomHlslInputPin);
    if (!InputType.IsValid() || InputType == FNiagaraTypeDefinition::GetParameterMapDef())
    {
        return MakeSafetyError(
            TEXT("Custom HLSL input has an invalid or ParameterMap type"),
            TEXT("niagara_unused_module_input_invalid_input_type"));
    }
    if (InputType.IsDataInterface() || InputType.IsUObject())
    {
        return MakeSafetyError(
            TEXT("Object and Data Interface inputs are not supported by this value-path removal command"),
            TEXT("niagara_unused_module_input_object_or_data_interface_not_supported"));
    }
    const FString InputTypeName = NiagaraTypeName(InputType);
    if (!ExpectedInputType.IsEmpty() && !ExpectedInputType.Equals(InputTypeName, ESearchCase::CaseSensitive))
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Input type changed since dry-run; expected %s, actual %s"), *ExpectedInputType, *InputTypeName),
            TEXT("niagara_unused_module_input_type_changed"));
    }
    int32 CustomSignatureInputCount = 0;
    for (const FNiagaraVariable& SignatureInput : CustomHlslNode->Signature.Inputs)
    {
        if (SignatureInput.GetName() == FName(*CustomHlslInputName) && SignatureInput.GetType() == InputType)
        {
            ++CustomSignatureInputCount;
        }
    }
    if (CustomSignatureInputCount != 1)
    {
        return MakeSafetyError(
            TEXT("Custom HLSL signature must contain exactly one typed target input"),
            TEXT("niagara_unused_module_input_custom_signature_mismatch"));
    }

    const FNiagaraVariable ModuleInputVariable(InputType, FName(*ModuleInputName));
    if (!ScratchGraph->GetAllMetaData().Contains(ModuleInputVariable))
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Scratch Pad metadata does not contain exact variable %s"), *ModuleInputName),
            TEXT("niagara_unused_module_input_metadata_missing"));
    }

    struct FMapGetPinRecord
    {
        TObjectPtr<UEdGraphNode> Node = nullptr;
        UEdGraphPin* Pin = nullptr;
        FString NodeGuid;
    };
    TArray<FMapGetPinRecord> MapGetPinRecords;
    TArray<FString> ActualMapGetNodeGuids;
    TSet<FString> SeenActualMapGetNodeGuids;
    for (UEdGraphNode* Node : ScratchGraph->Nodes)
    {
        if (!Node || !Node->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive))
        {
            continue;
        }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output || !Pin->PinName.ToString().Equals(ModuleInputName, ESearchCase::CaseSensitive))
            {
                continue;
            }
            if (Pin->bOrphanedPin)
            {
                return MakeSafetyError(
                    TEXT("A matching Scratch Pad ParameterMapGet output is already orphaned"),
                    TEXT("niagara_unused_module_input_orphaned_map_get_pin"));
            }
            if (PinTypeForPath(Pin) != InputType)
            {
                return MakeSafetyError(
                    TEXT("A matching Scratch Pad ParameterMapGet pin has a different type"),
                    TEXT("niagara_unused_module_input_map_get_type_mismatch"));
            }
            if (!Node->NodeGuid.IsValid())
            {
                return MakeSafetyError(
                    TEXT("A matching Scratch Pad ParameterMapGet node has no stable GUID"),
                    TEXT("niagara_unused_module_input_map_get_guid_missing"));
            }
            const FString NodeGuid = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
            if (SeenActualMapGetNodeGuids.Contains(NodeGuid))
            {
                return MakeSafetyError(
                    TEXT("A ParameterMapGet node contains duplicate target output pins; refusing the ambiguous shape"),
                    TEXT("niagara_unused_module_input_duplicate_map_get_pin"));
            }
            SeenActualMapGetNodeGuids.Add(NodeGuid);
            ActualMapGetNodeGuids.Add(NodeGuid);
            FMapGetPinRecord& Record = MapGetPinRecords.AddDefaulted_GetRef();
            Record.Node = Node;
            Record.Pin = Pin;
            Record.NodeGuid = NodeGuid;
        }
    }
    ActualMapGetNodeGuids.Sort();
    if (MapGetPinRecords.IsEmpty())
    {
        return MakeSafetyError(
            FString::Printf(TEXT("No Scratch Pad ParameterMapGet output exists for %s"), *ModuleInputName),
            TEXT("niagara_unused_module_input_map_get_pin_missing"));
    }
    for (const FMapGetPinRecord& Record : MapGetPinRecords)
    {
        if (!Record.Pin || Record.Pin->LinkedTo.Num() > 1 ||
            (Record.Pin->LinkedTo.Num() == 1 && Record.Pin->LinkedTo[0] != CustomHlslInputPin))
        {
            return MakeSafetyError(
                TEXT("Every target ParameterMapGet output must be unlinked or linked only to the selected Custom HLSL input"),
                TEXT("niagara_unused_module_input_map_get_has_other_consumer"));
        }
    }
    if (CustomHlslInputPin->LinkedTo.Num() != 1 ||
        !MapGetPinRecords.ContainsByPredicate(
            [CustomHlslInputPin](const FMapGetPinRecord& Record)
            {
                return Record.Pin && CustomHlslInputPin->LinkedTo[0] == Record.Pin;
            }))
    {
        return MakeSafetyError(
            TEXT("The selected Custom HLSL input must have exactly one link from the selected Module.* ParameterMapGet path"),
            TEXT("niagara_unused_module_input_custom_link_mismatch"));
    }
    if (!CanonicalExpectedMapGetGuids.IsEmpty() && CanonicalExpectedMapGetGuids != ActualMapGetNodeGuids)
    {
        TSharedPtr<FJsonObject> Error = MakeSafetyError(
            TEXT("Scratch Pad ParameterMapGet node GUID set no longer matches the dry-run contract"),
            TEXT("niagara_unused_module_input_map_get_guid_set_changed"));
        Error->SetArrayField(TEXT("expected_scratch_map_get_node_guids"), StringArrayToJsonValues(CanonicalExpectedMapGetGuids));
        Error->SetArrayField(TEXT("actual_scratch_map_get_node_guids"), StringArrayToJsonValues(ActualMapGetNodeGuids));
        return Error;
    }

    FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
    TArray<FNiagaraVariable> UserParameters;
    Store.GetUserParameters(UserParameters);
    FNiagaraVariable LinkedUserVariable;
    int32 LinkedUserVariableMatchCount = 0;
    for (const FNiagaraVariable& Candidate : UserParameters)
    {
        if (NiagaraUserParameterNameMatches(Candidate, LinkedUserParameterName) && Candidate.GetType() == InputType)
        {
            LinkedUserVariable = Candidate;
            ++LinkedUserVariableMatchCount;
        }
    }
    if (LinkedUserVariableMatchCount != 1)
    {
        return MakeSafetyError(
            FString::Printf(TEXT("Exposed parameter %s with the exact module-input type must exist once; matched %d"), *LinkedUserParameterName, LinkedUserVariableMatchCount),
            TEXT("niagara_unused_module_input_user_parameter_mismatch"));
    }

    const FNiagaraParameterHandle AliasedInputHandle =
        BuildAliasedModuleParameterHandleForInput(ReferencingModule, ModuleInputVariable);
    UEdGraphPin* OverridePin = FindNiagaraStackFunctionInputOverridePin(ReferencingModule, AliasedInputHandle);
    if (!OverridePin || OverridePin->bOrphanedPin || OverridePin->LinkedTo.Num() != 1)
    {
        return MakeSafetyError(
            TEXT("The selected module input must have exactly one stack override link"),
            TEXT("niagara_unused_module_input_override_link_mismatch"));
    }
    UEdGraphNode* OverrideNode = OverridePin->GetOwningNode();
    UEdGraphPin* LinkedUserSourcePin = OverridePin->LinkedTo[0];
    UEdGraphNode* LinkedUserSourceNode = LinkedUserSourcePin ? LinkedUserSourcePin->GetOwningNode() : nullptr;
    UNiagaraGraph* LinkedUserSourceGraph = LinkedUserSourceNode
        ? Cast<UNiagaraGraph>(LinkedUserSourceNode->GetGraph())
        : nullptr;
    if (!OverrideNode || !OverrideNode->NodeGuid.IsValid() || !LinkedUserSourcePin || !LinkedUserSourceNode ||
        !LinkedUserSourceNode->NodeGuid.IsValid() || !LinkedUserSourceGraph ||
        !LinkedUserSourceNode->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive) ||
        LinkedUserSourcePin->Direction != EGPD_Output ||
        !LinkedUserSourcePin->PinName.ToString().Equals(LinkedUserParameterName, ESearchCase::CaseSensitive) ||
        PinTypeForPath(LinkedUserSourcePin) != InputType)
    {
        return MakeSafetyError(
            TEXT("The module override is not the expected direct User.* ParameterMapGet link"),
            TEXT("niagara_unused_module_input_override_source_mismatch"));
    }
    if (ExpectedUserSourceNodeGuid.IsValid() && LinkedUserSourceNode->NodeGuid != ExpectedUserSourceNodeGuid)
    {
        return MakeSafetyError(
            TEXT("User ParameterMapGet source node GUID no longer matches the dry-run contract"),
            TEXT("niagara_unused_module_input_user_source_guid_changed"));
    }
    const FNiagaraVariable LinkedUserGraphVariable(InputType, FName(*LinkedUserParameterName));
    const bool bLinkedUserGraphMetadataPresent =
        LinkedUserSourceGraph->GetAllMetaData().Contains(LinkedUserGraphVariable);
    int32 LinkedUserSourceSemanticOutputCount = 0;
    for (const UEdGraphPin* Pin : LinkedUserSourceNode->Pins)
    {
        if (!Pin || Pin->Direction != EGPD_Output || IsNiagaraDynamicAddPin(Pin))
        {
            continue;
        }
        ++LinkedUserSourceSemanticOutputCount;
        if (Pin != LinkedUserSourcePin)
        {
            return MakeSafetyError(
                TEXT("The direct User.* ParameterMapGet contains another semantic output; refusing whole-node removal"),
                TEXT("niagara_unused_module_input_override_source_has_other_outputs"));
        }
    }
    if (LinkedUserSourceSemanticOutputCount != 1)
    {
        return MakeSafetyError(
            TEXT("The direct User.* ParameterMapGet must contain exactly one semantic output"),
            TEXT("niagara_unused_module_input_override_source_shape_unsupported"));
    }
    FString OverrideValidationError;
    if (!ValidateLinkedNiagaraDataInterfaceInputNode(*OverridePin, OverrideValidationError))
    {
        return MakeSafetyError(
            OverrideValidationError,
            TEXT("niagara_unused_module_input_override_source_shared"));
    }
    if (ExpectedOverrideNodeGuid.IsValid() && OverrideNode->NodeGuid != ExpectedOverrideNodeGuid)
    {
        return MakeSafetyError(
            TEXT("Override ParameterMapSet node GUID no longer matches the dry-run contract"),
            TEXT("niagara_unused_module_input_override_guid_changed"));
    }

    int32 ExactUserGraphPinCount = 0;
    int32 OtherUserGraphPinCount = 0;
    int32 ExactUserGraphMetadataCount = 0;
    int32 OtherUserGraphMetadataCount = 0;
    for (UNiagaraScriptSource* Source : CollectTargetNiagaraScriptSources(System))
    {
        if(!ContinueRead())return W.Error(W.R.Error);
        UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
        if (!Graph)
        {
            continue;
        }
        if (Graph->GetAllMetaData().Contains(LinkedUserGraphVariable))
        {
            ++ExactUserGraphMetadataCount;
            OtherUserGraphMetadataCount += Graph == LinkedUserSourceGraph ? 0 : 1;
        }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->PinName.ToString().Equals(LinkedUserParameterName, ESearchCase::CaseSensitive))
                {
                    ++ExactUserGraphPinCount;
                    OtherUserGraphPinCount += Pin == LinkedUserSourcePin ? 0 : 1;
                }
            }
        }
    }
    if (ExactUserGraphPinCount != 1 || OtherUserGraphPinCount != 0)
    {
        return MakeSafetyError(
            FString::Printf(TEXT("User parameter has graph references outside the selected direct override source; exact pins=%d, other pins=%d"), ExactUserGraphPinCount, OtherUserGraphPinCount),
            TEXT("niagara_unused_module_input_user_parameter_has_other_graph_references"));
    }
    if (OtherUserGraphMetadataCount != 0 ||
        ExactUserGraphMetadataCount != (bLinkedUserGraphMetadataPresent ? 1 : 0))
    {
        return MakeSafetyError(
            FString::Printf(TEXT("User parameter metadata exists outside the selected direct source graph; exact entries=%d, other entries=%d"), ExactUserGraphMetadataCount, OtherUserGraphMetadataCount),
            TEXT("niagara_unused_module_input_user_parameter_has_other_graph_metadata"));
    }

    const FString CustomHlslNodeGuidString = CustomHlslNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
    const FString OverrideNodeGuidString = OverrideNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
    const FString UserSourceNodeGuidString = LinkedUserSourceNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
    TSharedPtr<FJsonObject> Discovery = MakeShared<FJsonObject>();
    Discovery->SetStringField(TEXT("scratch_pad_script_path"), ScratchPadScript->GetPathName());
    Discovery->SetNumberField(TEXT("scratch_pad_script_index"), ScratchPadScriptIndex);
    Discovery->SetNumberField(TEXT("scratch_pad_reference_count"), ScratchPadReferenceCount);
    Discovery->SetStringField(TEXT("module_name"), ModuleName);
    Discovery->SetStringField(TEXT("module_node_guid"), ModuleNodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Discovery->SetStringField(TEXT("emitter_name"), ReferencingEmitterName);
    Discovery->SetNumberField(TEXT("emitter_index"), ReferencingEmitterIndex);
    Discovery->SetNumberField(TEXT("module_index"), ReferencingModuleIndex);
    Discovery->SetStringField(TEXT("module_input_name"), ModuleInputName);
    Discovery->SetStringField(TEXT("custom_hlsl_input_name"), CustomHlslInputName);
    Discovery->SetStringField(TEXT("input_type"), InputTypeName);
    Discovery->SetStringField(TEXT("expected_input_type"), InputTypeName);
    Discovery->SetStringField(TEXT("linked_user_parameter_name"), LinkedUserParameterName);
    Discovery->SetStringField(TEXT("expected_custom_hlsl_node_guid"), CustomHlslNodeGuidString);
    Discovery->SetStringField(TEXT("expected_override_node_guid"), OverrideNodeGuidString);
    Discovery->SetStringField(TEXT("expected_user_source_node_guid"), UserSourceNodeGuidString);
    Discovery->SetArrayField(TEXT("expected_scratch_map_get_node_guids"), StringArrayToJsonValues(ActualMapGetNodeGuids));
    Discovery->SetNumberField(TEXT("scratch_map_get_pin_count"), MapGetPinRecords.Num());
    Discovery->SetNumberField(TEXT("user_graph_pin_count"), ExactUserGraphPinCount);
    Discovery->SetNumberField(TEXT("user_graph_metadata_count"), ExactUserGraphMetadataCount);
    Discovery->SetBoolField(TEXT("user_source_graph_metadata_present"), bLinkedUserGraphMetadataPresent);
    Discovery->SetStringField(TEXT("user_source_graph_path"), LinkedUserSourceGraph->GetPathName());
    Discovery->SetNumberField(TEXT("user_source_semantic_output_count"), LinkedUserSourceSemanticOutputCount);
    Discovery->SetBoolField(TEXT("hlsl_identifier_absent"), true);
    Discovery->SetStringField(TEXT("guarded_hlsl_identifier"), ExpectedHlslIdentifierAbsent);
    Discovery->SetStringField(TEXT("expected_hlsl_body_hash"), ExistingHlslHash);

    if(!ContinueRead())return W.Error(W.R.Error);
    W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);
    if (bDryRun)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("dry_run"), true);
        Result->SetBoolField(TEXT("would_remove"), true);
        Result->SetBoolField(TEXT("changed"), false);
        Result->SetBoolField(TEXT("package_was_dirty"), bPackageWasDirty);
        Result->SetStringField(TEXT("system_path"), System->GetPathName());
        Result->SetObjectField(TEXT("discovery_contract"), Discovery);
        Result->SetStringField(TEXT("write_scope"), TEXT("niagara_unused_module_input_path_removal"));
        return Result;
    }


    // All selectors above are the original dry-run contract. Pin every actual
    // writer and freeze call/metadata/default routes before the first Modify.
    W.GuardedRemoval=true;
    auto Plan=MakeShared<FIDRemovalPlan>();Plan->Custom=CustomHlslNode;Plan->Signature=CustomHlslNode->Signature;Plan->Body=ExistingHlsl;Plan->User=LinkedUserVariable;
    Plan->System=System;Plan->CompileRoles={System->GetSystemSpawnScript(),System->GetSystemUpdateScript()};for(const auto& E:W.Emitters){Plan->EmitterData.Add(E.Data);if(E.Data)E.Data->GetScripts(Plan->CompileRoles,false,false);}
    Plan->Users=CaptureUserValues(W);if(!ContinueRead()||!VerifyCanonicalUserForPath(W,LinkedUserVariable))return W.Error(W.R.Error);
    for(const auto& G:W.Graphs)for(const auto& N:G.Nodes)
    {
        auto* Node=N.Node.Get();if(auto* Call=Cast<UNiagaraNodeFunctionCall>(Node)){if(Call->FunctionScript&&!W.CaptureScript(Call->FunctionScript))return W.Error(W.R.Error);Plan->Calls.Add({Call,Call->FunctionScript,Call->SelectedScriptVersion});}
        if(Node->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive)){FIDMapRole M;M.Node=Node;if(!IDMapPairs(Node,M.Pairs)){W.Fail(TEXT("niagara_unused_module_input_default_mapping_unavailable"),TEXT("Exact reflected MapGet default mapping is unavailable before first writer."));return W.Error(W.R.Error);}Plan->Maps.Add(MoveTemp(M));}
    }
    W.RemovalReadAdmission=[Plan](){return Plan->Check();};
    W.RemovalPairPlan=[Plan](UEdGraphNode* N,const FGuid& Id){auto* M=Plan->Maps.FindByPredicate([&](const auto& X){return X.Node.Get()==N;});return M&&M->Pairs.Remove(Id)==1;};
    if(!W.Admit(TEXT("unused_pre_removal_complete"))||!IDUsersPreserved(W,*Plan))return W.Error(W.R.Error);
    for(UObject* Object:{static_cast<UObject*>(System),static_cast<UObject*>(ScratchPadScript),static_cast<UObject*>(ScratchPadSource),static_cast<UObject*>(ScratchGraph),static_cast<UObject*>(CustomHlslNode),static_cast<UObject*>(ReferencingModule),static_cast<UObject*>(OverrideNode),static_cast<UObject*>(LinkedUserSourceGraph)})
        if(!W.Own(Object))return W.Error(W.R.Error);
    // Plain IDs are retained; deleted raw pins are not used after SDK callbacks.
    const FGuid OverridePinId=OverridePin->PinId,CustomPinId=CustomHlslInputPin->PinId;
    TArray<TPair<TWeakObjectPtr<UEdGraphNode>,FGuid>> MapPins;
    for(const auto& Record:MapGetPinRecords){UEdGraphPin* Default=nullptr;if(!IDDefaultPin(W,Record.Node,Record.Pin,Default))return W.Error(W.R.Error);MapPins.Add({Record.Node,Record.Pin->PinId});}
    bMutationStarted=true;W.RemovalMutationStarted=true;W.R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),true);W.R.Data->SetBoolField(TEXT("transient_creation_only"),false);
    if(!W.Modify(System)||!W.Dirty()||!W.Modify(ScratchPadScript)||!W.Modify(ScratchPadSource)||!W.Modify(ScratchGraph)||!W.Modify(CustomHlslNode)||!W.Modify(ReferencingModule)||!W.Modify(OverrideNode)||!W.Modify(LinkedUserSourceGraph))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
    // Original ClearLinked... performs graph Modify, source Modify, break then RemoveNode.
    if(!W.Modify(LinkedUserSourceGraph)||!W.Modify(LinkedUserSourceNode))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
    UEdGraphPin* CurrentOverride=nullptr;for(auto* P:OverrideNode->Pins)if(P&&P->PinId==OverridePinId)CurrentOverride=P;
    if(!CurrentOverride||!W.BreakLinks(CurrentOverride,true)||!W.RemoveNode(LinkedUserSourceGraph,LinkedUserSourceNode)||!IDRemovePin(W,OverrideNode,OverridePinId,false))
        return MakeSafetyError(W.R.Error,TEXT("niagara_unused_module_input_override_removal_failed"));
    int32 CustomSignatureInputIndex=INDEX_NONE;
    for(int32 I=0;I<CustomHlslNode->Signature.Inputs.Num();++I)if(CustomHlslNode->Signature.Inputs[I].GetName()==FName(*CustomHlslInputName)&&CustomHlslNode->Signature.Inputs[I].GetType()==InputType)CustomSignatureInputIndex=I;
    if(CustomSignatureInputIndex==INDEX_NONE)return MakeSafetyError(TEXT("Failed to remove exactly one Custom HLSL signature input"),TEXT("niagara_unused_module_input_signature_removal_failed"));
    UEdGraphPin* CurrentCustom=nullptr;for(auto* P:CustomHlslNode->Pins)if(P&&P->PinId==CustomPinId)CurrentCustom=P;
    if(!CurrentCustom||!W.BreakLinks(CurrentCustom,true))return MakeSafetyError(W.R.Error,TEXT("niagara_unused_module_input_custom_pin_removal_failed"));
    if(!W.SDK(TEXT("unused_RemoveCustomSignature"),true))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
    const FNiagaraVariable RemovedSignatureInput=CustomHlslNode->Signature.Inputs[CustomSignatureInputIndex];
    Plan->Signature.Inputs.RemoveAt(CustomSignatureInputIndex);Plan->Signature.InputDescriptions.Remove(RemovedSignatureInput);Plan->Signature.NoDefaultValueInputs.Remove(RemovedSignatureInput);
    CustomHlslNode->Signature=Plan->Signature;W.R.bChanged=true;
    if(!W.After(TEXT("unused_RemoveCustomSignature"))||!IDRemovePin(W,CustomHlslNode,CustomPinId,false)||!IDRefreshNode(W,CustomHlslNode))return MakeSafetyError(W.R.Error,TEXT("niagara_unused_module_input_custom_pin_removal_failed"));
    GL_STEP("unused_custom_synchronize",true,CustomHlslNode->MarkNodeRequiresSynchronization(TEXT("MCP: unused Custom HLSL input removed"),true));
    int32 RemovedMapGetPinCount=0;
    for(const auto& Record:MapPins)
    {
        auto* Node=Record.Key.Get();if(!W.Admit(TEXT("unused_map_pin_reacquire"))||!Live(Node)||!W.Modify(Node))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
        UEdGraphPin* P=nullptr;for(auto* Pin:Node->Pins)if(Pin&&Pin->PinId==Record.Value)P=Pin;
        if(!P||!W.BreakLinks(P,true))return MakeSafetyError(W.R.Error,TEXT("niagara_unused_module_input_map_get_removal_failed"));
        // Plan only the selected persistent mapping retirement; the SDK removes
        // its exact default pair via OnPinRemoved.
        const FGuid OutputPersistent=P->PersistentGuid;
        if(!W.SDK(TEXT("unused_map_pair_plan")))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
        auto* Mapping=Plan->Maps.FindByPredicate([&](const auto& M){return M.Node.Get()==Node;});if(!Mapping)return MakeSafetyError(TEXT("Missing captured default map"),TEXT("niagara_unused_module_input_default_mapping_unavailable"));
        // IDRemovePin admits before its SDK, so update the map expectation only
        // inside that SDK's explicit transition.
        W.Prefix->SetStringField(TEXT("selected_map_output_persistent_id"),OutputPersistent.ToString());
        if(!IDRemovePin(W,Node,Record.Value,true))return MakeSafetyError(W.R.Error,TEXT("niagara_unused_module_input_map_get_removal_failed"));
        ++RemovedMapGetPinCount;
    }
    int32 RemovedMetadataCount=0,RemovedUserGraphMetadataCount=0;
    if(!IDRemoveMetadata(W,ScratchGraph,ModuleInputVariable,RemovedMetadataCount)||RemovedMetadataCount!=1)return MakeSafetyError(W.R.Error.IsEmpty()?TEXT("Failed to remove exactly one Scratch Pad Module.* metadata entry"):W.R.Error,TEXT("niagara_unused_module_input_metadata_removal_failed"));
    GL_STEP("unused_scratch_NotifyGraphChanged",true,ScratchGraph->NotifyGraphChanged());
    if(!IDRefreshNode(W,ReferencingModule))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
    GL_STEP("unused_module_synchronize",true,ReferencingModule->MarkNodeRequiresSynchronization(TEXT("MCP: unused Scratch Pad module input removed"),true));
    if(bLinkedUserGraphMetadataPresent)
    {
        if(!IDRemoveMetadata(W,LinkedUserSourceGraph,LinkedUserGraphVariable,RemovedUserGraphMetadataCount))return MakeSafetyError(W.R.Error,TEXT("niagara_unused_module_input_user_graph_metadata_removal_failed"));
        GL_STEP("unused_user_graph_NotifyGraphChanged",true,LinkedUserSourceGraph->NotifyGraphChanged());
    }
    const bool bUserGraphMetadataAbsent=!LinkedUserSourceGraph->GetAllMetaData().Contains(LinkedUserGraphVariable);
    if(!bUserGraphMetadataAbsent||RemovedUserGraphMetadataCount!=(bLinkedUserGraphMetadataPresent?1:0))return MakeSafetyError(TEXT("Failed exact direct User metadata removal"),TEXT("niagara_unused_module_input_user_graph_metadata_removal_failed"));
    if(!IDUsersPreserved(W,*Plan)||!W.SDK(TEXT("unused_Store_RemoveParameter"),true))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
    const bool bRemovedUserParameter=Store.RemoveParameter(LinkedUserVariable);W.R.bChanged|=bRemovedUserParameter;Plan->UserRemoved=bRemovedUserParameter;
    if(!W.After(TEXT("unused_Store_RemoveParameter"))||!bRemovedUserParameter)return MakeSafetyError(W.R.Error.IsEmpty()?TEXT("Failed to remove the exact exposed User parameter"):W.R.Error,TEXT("niagara_unused_module_input_user_parameter_removal_failed"));
    if(!W.Dirty()||!IDUsersPreserved(W,*Plan))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
    // Immediate in-memory readback. These checks cover every location this
    // narrow command is authorized to remove.
    TArray<FNiagaraVariable> PostUserParameters;
    Store.GetUserParameters(PostUserParameters);
    const bool bUserParameterAbsent = !PostUserParameters.ContainsByPredicate(
        [&LinkedUserParameterName, &InputType](const FNiagaraVariable& Candidate)
        {
            return NiagaraUserParameterNameMatches(Candidate, LinkedUserParameterName) && Candidate.GetType() == InputType;
        });
    int32 PostCustomSignatureInputCount = 0;
    int32 PostCustomPinCount = 0;
    for (const FNiagaraVariable& SignatureInput : CustomHlslNode->Signature.Inputs)
    {
        PostCustomSignatureInputCount +=
            SignatureInput.GetName() == FName(*CustomHlslInputName) && SignatureInput.GetType() == InputType ? 1 : 0;
    }
    for (UEdGraphPin* Pin : CustomHlslNode->Pins)
    {
        PostCustomPinCount += Pin && Pin->Direction == EGPD_Input &&
            Pin->PinName.ToString().Equals(CustomHlslInputName, ESearchCase::CaseSensitive) ? 1 : 0;
    }
    int32 PostModuleGraphPinCount = 0;
    for (UEdGraphNode* Node : ScratchGraph->Nodes)
    {
        if (!Node)
        {
            continue;
        }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            PostModuleGraphPinCount += Pin && Pin->PinName.ToString().Equals(ModuleInputName, ESearchCase::CaseSensitive) ? 1 : 0;
        }
    }
    int32 PostUserGraphPinCount = 0;
    for (UNiagaraScriptSource* Source : CollectTargetNiagaraScriptSources(System))
    {
        UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
        if (!Graph)
        {
            continue;
        }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }
            for (UEdGraphPin* Pin : Node->Pins)
            {
                PostUserGraphPinCount += Pin && Pin->PinName.ToString().Equals(LinkedUserParameterName, ESearchCase::CaseSensitive) ? 1 : 0;
            }
        }
    }
    const bool bMetadataAbsent = !ScratchGraph->GetAllMetaData().Contains(ModuleInputVariable);
    const bool bOverrideAbsent = FindNiagaraStackFunctionInputOverridePin(ReferencingModule, AliasedInputHandle) == nullptr;
    const bool bHlslUnchanged = GetNiagaraCustomHlslString(CustomHlslNode).Equals(ExistingHlsl, ESearchCase::CaseSensitive);
    const bool bHlslHashUnchanged =
        FMD5::HashAnsiString(*GetNiagaraCustomHlslString(CustomHlslNode)).Equals(ExistingHlslHash, ESearchCase::CaseSensitive);
    const bool bReadbackVerified =
        bUserParameterAbsent && PostCustomSignatureInputCount == 0 && PostCustomPinCount == 0 &&
        PostModuleGraphPinCount == 0 && PostUserGraphPinCount == 0 && bMetadataAbsent &&
        bUserGraphMetadataAbsent &&
        bOverrideAbsent && bHlslUnchanged && bHlslHashUnchanged;
    if (!bReadbackVerified)
    {
        TSharedPtr<FJsonObject> Error = MakeSafetyError(
            TEXT("Post-mutation Niagara graph readback did not prove the complete selected path absent"),
            TEXT("niagara_unused_module_input_post_readback_failed"));
        Error->SetBoolField(TEXT("mutation_applied_in_memory"), true);
        Error->SetBoolField(TEXT("user_parameter_absent"), bUserParameterAbsent);
        Error->SetNumberField(TEXT("custom_signature_input_count"), PostCustomSignatureInputCount);
        Error->SetNumberField(TEXT("custom_input_pin_count"), PostCustomPinCount);
        Error->SetNumberField(TEXT("module_graph_pin_count"), PostModuleGraphPinCount);
        Error->SetNumberField(TEXT("user_graph_pin_count"), PostUserGraphPinCount);
        Error->SetBoolField(TEXT("metadata_absent"), bMetadataAbsent);
        Error->SetBoolField(TEXT("user_graph_metadata_absent"), bUserGraphMetadataAbsent);
        Error->SetBoolField(TEXT("override_absent"), bOverrideAbsent);
        Error->SetBoolField(TEXT("hlsl_unchanged"), bHlslUnchanged);
        Error->SetBoolField(TEXT("hlsl_hash_unchanged"), bHlslHashUnchanged);
        return Error;
    }

    bool bCompileLaunched=false;
    if(!W.Request(bCompileLaunched))return MakeSafetyError(W.R.Error,W.R.ErrorCode);
    const auto CompileWait=WaitForNiagaraCompileJson(System,bWaitForCompletion,TimeoutSeconds,PollIntervalSeconds);
    if(!ContinueRead())return MakeSafetyError(W.R.Error,W.R.ErrorCode);
    // All raw pin removals are complete. This one retained closure observes
    // current owners/graphs/roles again after the same Job's bounded wait.
    W.RemovalTail=[=,&W]()->TSharedPtr<FJsonObject>
    {
        auto MakeTerminalSafetyError=[&W](const FString& Message,const FString& Code)
        {W.Fail(*Code,Message);auto E=W.Error(Message);E->SetStringField(TEXT("error_code"),Code);E->SetBoolField(TEXT("mutation_applied_in_memory"),true);E->SetBoolField(TEXT("requires_reload_or_undo"),true);E->SetBoolField(TEXT("saved"),false);return E;};
        if(!W.Admit(TEXT("unused_postcompile_phase"))||!IDUsersPreserved(W,*Plan))return MakeTerminalSafetyError(W.R.Error,W.R.ErrorCode);
        TArray<FIDCompileSnapshot> CurrentSnapshots;if(!IDCaptureCompile(W,CurrentSnapshots))return MakeTerminalSafetyError(W.R.Error,W.R.ErrorCode);
        const auto Counts=IDCount(CurrentSnapshots,&PreMutationCompileSnapshots);
        bool SameCallers=CurrentSnapshots.Num()==PreMutationCompileSnapshots.Num();
        if(SameCallers)for(int32 I=0;I<CurrentSnapshots.Num();++I){const auto& A=CurrentSnapshots[I];const auto& B=PreMutationCompileSnapshots[I];if(A.Script.Get()!=B.Script.Get()||A.OwnerKind!=B.OwnerKind||A.OwnerName!=B.OwnerName||A.ScriptPath!=B.ScriptPath){SameCallers=false;break;}}
        const bool bCompileGatePass=SameCallers&&!W.WaitTimedOut&&W.WaitIdle&&Counts.Known>0&&Counts.Errors==0&&Counts.Warnings<=PreflightCompileWarningCount&&Counts.NonReady==0&&Counts.Unsync==0&&Counts.Changed>0;
        const auto CompileSnapshotsJson=Counts.Json;
        if(!bCompileGatePass)
        {
            auto Error=MakeTerminalSafetyError(TEXT("Compile gate did not prove idle, ready, synchronized Niagara scripts; mutation remains unsaved in memory"),TEXT("niagara_unused_module_input_compile_gate_failed"));
            Error->SetBoolField(TEXT("compile_requested"),bRequestCompile);Error->SetBoolField(TEXT("compile_launched"),bCompileLaunched);Error->SetObjectField(TEXT("compile_wait"),CompileWait);
            Error->SetNumberField(TEXT("known_compile_script_count"),Counts.Known);Error->SetNumberField(TEXT("compile_error_count"),Counts.Errors);Error->SetNumberField(TEXT("compile_warning_count"),Counts.Warnings);Error->SetNumberField(TEXT("preflight_compile_warning_count"),PreflightCompileWarningCount);
            Error->SetNumberField(TEXT("compile_non_ready_count"),Counts.NonReady);Error->SetNumberField(TEXT("compile_unsynchronized_count"),Counts.Unsync);Error->SetNumberField(TEXT("compile_input_changed_count"),Counts.Changed);Error->SetArrayField(TEXT("compile_snapshots"),CompileSnapshotsJson);Error->SetBoolField(TEXT("compile_caller_roles_preserved"),SameCallers);return Error;
        }
        W.R.Compiled=true;W.R.Data->SetBoolField(TEXT("original_removal_strict_compile_gate_passed"),true);
    // Compile can rebuild Niagara pins and exposed parameters. Repeat the full
    // target-absence check after compilation, before any optional save.
    TArray<FNiagaraVariable> PostCompileUserParameters;
    System->GetExposedParameters().GetUserParameters(PostCompileUserParameters);
    const bool bPostCompileUserParameterAbsent = !PostCompileUserParameters.ContainsByPredicate(
        [&LinkedUserParameterName, &InputType](const FNiagaraVariable& Candidate)
        {
            return NiagaraUserParameterNameMatches(Candidate, LinkedUserParameterName) && Candidate.GetType() == InputType;
        });
    UNiagaraNodeCustomHlsl* PostCompileCustomNode = nullptr;
    int32 PostCompileCustomNodeCount = 0;
    int32 PostCompileCustomSignatureInputCount = 0;
    int32 PostCompileCustomPinCount = 0;
    int32 PostCompileModuleGraphPinCount = 0;
    for (UEdGraphNode* Node : ScratchGraph->Nodes)
    {
        if (!Node)
        {
            continue;
        }
        if (UNiagaraNodeCustomHlsl* Candidate = Cast<UNiagaraNodeCustomHlsl>(Node))
        {
            if (Candidate->NodeGuid == ExpectedCustomHlslNodeGuid &&
                Candidate->Signature.GetNameString().Equals(CustomHlslSignatureName, ESearchCase::CaseSensitive))
            {
                PostCompileCustomNode = Candidate;
                ++PostCompileCustomNodeCount;
                for (const FNiagaraVariable& SignatureInput : Candidate->Signature.Inputs)
                {
                    PostCompileCustomSignatureInputCount +=
                        SignatureInput.GetName() == FName(*CustomHlslInputName) && SignatureInput.GetType() == InputType ? 1 : 0;
                }
                for (UEdGraphPin* Pin : Candidate->Pins)
                {
                    PostCompileCustomPinCount += Pin && Pin->Direction == EGPD_Input &&
                        Pin->PinName.ToString().Equals(CustomHlslInputName, ESearchCase::CaseSensitive) ? 1 : 0;
                }
            }
        }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            PostCompileModuleGraphPinCount += Pin &&
                Pin->PinName.ToString().Equals(ModuleInputName, ESearchCase::CaseSensitive) ? 1 : 0;
        }
    }
    int32 PostCompileUserGraphPinCount = 0;
    for (UNiagaraScriptSource* Source : CollectTargetNiagaraScriptSources(System))
    {
        UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
        if (!Graph)
        {
            continue;
        }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }
            for (UEdGraphPin* Pin : Node->Pins)
            {
                PostCompileUserGraphPinCount += Pin &&
                    Pin->PinName.ToString().Equals(LinkedUserParameterName, ESearchCase::CaseSensitive) ? 1 : 0;
            }
        }
    }
    const bool bPostCompileMetadataAbsent = !ScratchGraph->GetAllMetaData().Contains(ModuleInputVariable);
    const bool bPostCompileUserGraphMetadataAbsent =
        !LinkedUserSourceGraph->GetAllMetaData().Contains(LinkedUserGraphVariable);
    const bool bPostCompileOverrideAbsent =
        FindNiagaraStackFunctionInputOverridePin(ReferencingModule, AliasedInputHandle) == nullptr;
    const bool bPostCompileHlslUnchanged =
        PostCompileCustomNode &&
        GetNiagaraCustomHlslString(PostCompileCustomNode).Equals(ExistingHlsl, ESearchCase::CaseSensitive) &&
        FMD5::HashAnsiString(*GetNiagaraCustomHlslString(PostCompileCustomNode)).Equals(ExistingHlslHash, ESearchCase::CaseSensitive);
    const bool bPostCompileReadbackVerified =
        bPostCompileUserParameterAbsent && PostCompileCustomNodeCount == 1 &&
        PostCompileCustomSignatureInputCount == 0 && PostCompileCustomPinCount == 0 &&
        PostCompileModuleGraphPinCount == 0 && PostCompileUserGraphPinCount == 0 &&
        bPostCompileMetadataAbsent && bPostCompileUserGraphMetadataAbsent &&
        bPostCompileOverrideAbsent && bPostCompileHlslUnchanged;
    if (!bPostCompileReadbackVerified)
    {
        TSharedPtr<FJsonObject> Error = MakeTerminalSafetyError(
            TEXT("Post-compile readback recreated or changed part of the guarded Niagara input path; mutation remains unsaved in memory"),
            TEXT("niagara_unused_module_input_post_compile_readback_failed"));
        Error->SetBoolField(TEXT("mutation_applied_in_memory"), true);
        Error->SetBoolField(TEXT("user_parameter_absent"), bPostCompileUserParameterAbsent);
        Error->SetNumberField(TEXT("custom_node_count"), PostCompileCustomNodeCount);
        Error->SetNumberField(TEXT("custom_signature_input_count"), PostCompileCustomSignatureInputCount);
        Error->SetNumberField(TEXT("custom_input_pin_count"), PostCompileCustomPinCount);
        Error->SetNumberField(TEXT("module_graph_pin_count"), PostCompileModuleGraphPinCount);
        Error->SetNumberField(TEXT("user_graph_pin_count"), PostCompileUserGraphPinCount);
        Error->SetBoolField(TEXT("metadata_absent"), bPostCompileMetadataAbsent);
        Error->SetBoolField(TEXT("user_graph_metadata_absent"), bPostCompileUserGraphMetadataAbsent);
        Error->SetBoolField(TEXT("override_absent"), bPostCompileOverrideAbsent);
        Error->SetBoolField(TEXT("hlsl_unchanged"), bPostCompileHlslUnchanged);
        return Error;
    }
        if(!ContinueRead()||!IDUsersPreserved(W,*Plan))return MakeTerminalSafetyError(W.R.Error,W.R.ErrorCode);
        bool bSaved=false;
        if(bSave){bSaved=W.Save(false);if(!bSaved||!W.R.Error.IsEmpty())return MakeTerminalSafetyError(TEXT("Verified Niagara mutation could not be saved"),TEXT("niagara_unused_module_input_save_failed"));}
        if(!ContinueRead()||!IDUsersPreserved(W,*Plan))return MakeTerminalSafetyError(W.R.Error,W.R.ErrorCode);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("dry_run"), false);
    Result->SetBoolField(TEXT("changed"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetObjectField(TEXT("discovery_contract"), Discovery);
    Result->SetBoolField(TEXT("readback_verified"), bReadbackVerified);
    Result->SetBoolField(TEXT("post_compile_readback_verified"), bPostCompileReadbackVerified);
    Result->SetNumberField(TEXT("removed_custom_hlsl_signature_input_count"), 1);
    Result->SetNumberField(TEXT("removed_scratch_map_get_pin_count"), RemovedMapGetPinCount);
    Result->SetNumberField(TEXT("removed_metadata_count"), RemovedMetadataCount);
    Result->SetNumberField(TEXT("removed_user_graph_metadata_count"), RemovedUserGraphMetadataCount);
    Result->SetBoolField(TEXT("removed_user_parameter"), bRemovedUserParameter);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("compile_launched"), bCompileLaunched);
    Result->SetBoolField(TEXT("compile_gate_pass"), bCompileGatePass);
    Result->SetObjectField(TEXT("compile_wait"), CompileWait);
    Result->SetArrayField(TEXT("compile_snapshots"), CompileSnapshotsJson);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("niagara_unused_module_input_path_removal"));
    return Result;
    };
    if(W.WaitDeferred)
    {
        auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);Pending->SetBoolField(TEXT("mutation_applied_in_memory"),true);
        Pending->SetBoolField(TEXT("requires_reload_or_undo"),true);Pending->SetBoolField(TEXT("saved"),false);Pending->SetObjectField(TEXT("discovery_contract"),Discovery);return Pending;
    }
    auto Tail=MoveTemp(W.RemovalTail);return Tail();

#else
return W.Error(TEXT("remove_niagara_unused_module_input_path requires editor-only Niagara graph data"));
#endif
}

TSharedPtr<FJsonObject> HandleBindUser(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    FString InputName;
    FString UserParameterName;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("input_name"), InputName))
    {
        return W.Error(TEXT("Missing 'input_name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("user_parameter_name"), UserParameterName))
    {
        return W.Error(TEXT("Missing 'user_parameter_name' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara module input user parameter outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    UserParameterName.TrimStartAndEndInline();
    if (UserParameterName.IsEmpty())
    {
        return W.Error(TEXT("'user_parameter_name' cannot be empty"));
    }
    if (!UserParameterName.StartsWith(TEXT("User.")))
    {
        UserParameterName = FString::Printf(TEXT("User.%s"), *UserParameterName);
    }

    bool bRequireArrayDataInterface = false;
    Params->TryGetBoolField(TEXT("require_array_data_interface"), bRequireArrayDataInterface);

    bool bOverwriteExisting = false;
    Params->TryGetBoolField(TEXT("overwrite_existing"), bOverwriteExisting);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    bool bWaitForCompletion = false;
    double TimeoutSeconds = 10.0;
    double PollIntervalSeconds = 0.1;
    ReadNiagaraCompileWaitParams(Params, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds);

    int32 TargetEmitterIndex = INDEX_NONE;
    int32 TargetModuleIndex = INDEX_NONE;
    FString TargetEmitterName;
    FString TargetModuleName;
    FString TargetModuleGuid;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetNumberField(TEXT("module_index"), TargetModuleIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    Params->TryGetStringField(TEXT("module_name"), TargetModuleName);
    Params->TryGetStringField(TEXT("module_node_guid"), TargetModuleGuid);

    W.R.bSaveRequested=bSave;W.R.bCompileRequested=bRequestCompile;
    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    const auto AllSources=CollectTargetNiagaraScriptSources(System);if(!ContinueRead())return W.Error(W.R.Error);
    UNiagaraNodeFunctionCall* MatchedFunctionCall = nullptr;
    FVersionedNiagaraEmitterData* MatchedEmitterData = nullptr;
    FVersionedNiagaraEmitter MatchedOwningEmitter;
    FString MatchedEmitterName;
    FString MatchedModuleName;
    int32 MatchedEmitterIndex = INDEX_NONE;
    int32 MatchedModuleIndex = INDEX_NONE;

    int32 CurrentEmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : TArray<FNiagaraEmitterHandle>(System->GetEmitterHandles()))
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));

        if (bEmitterMatches)
        {
            if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
            {
                const TArray<UNiagaraNodeFunctionCall*> FunctionCalls = CollectSortedNiagaraFunctionCalls(EmitterData->GraphSource);
                for (int32 ModuleIndex = 0; ModuleIndex < FunctionCalls.Num(); ++ModuleIndex)
                {
                    UNiagaraNodeFunctionCall* FunctionCall = FunctionCalls[ModuleIndex];
                    if (!FunctionCall)
                    {
                        continue;
                    }

                    FString FunctionName = FunctionNameForPath(FunctionCall);if(!ContinueRead())return W.Error(W.R.Error);
                    if (FunctionName.IsEmpty())
                    {
                        FunctionName = FunctionCall->Signature.GetNameString();
                    }
                    const FString FunctionGuid = FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
                    const bool bModuleMatches =
                        (TargetModuleIndex == INDEX_NONE || TargetModuleIndex == ModuleIndex) &&
                        (TargetModuleName.IsEmpty() || TargetModuleName.Equals(FunctionName, ESearchCase::IgnoreCase)) &&
                        (TargetModuleGuid.IsEmpty() || TargetModuleGuid.Equals(FunctionGuid, ESearchCase::IgnoreCase));

                    if (bModuleMatches)
                    {
                        MatchedFunctionCall = FunctionCall;
                        MatchedEmitterData = EmitterData;
                        MatchedOwningEmitter = EmitterHandle.GetInstance();
                        MatchedEmitterName = EmitterName;
                        MatchedModuleName = FunctionName;
                        MatchedEmitterIndex = CurrentEmitterIndex;
                        MatchedModuleIndex = ModuleIndex;
                        break;
                    }
                }
            }
        }

        if (MatchedFunctionCall)
        {
            break;
        }
        ++CurrentEmitterIndex;
    }

    if (!MatchedFunctionCall || !MatchedEmitterData)
    {
        return W.Error(TEXT("No matching Niagara module was found"));
    }

    ENiagaraScriptUsage OutputUsage=ENiagaraScriptUsage::Function;GL_STEP("GetOutputNodeUsage",false,OutputUsage=FNiagaraStackGraphUtilities::GetOutputNodeUsage(*MatchedFunctionCall));
    FCompileConstantResolver ConstantResolver(MatchedOwningEmitter, OutputUsage, MatchedFunctionCall->DebugState);
    TArray<FNiagaraVariable> InputVariables;
    TSet<FNiagaraVariable> HiddenVariables;
    if(!W.Step(TEXT("GetStackFunctionInputs"),false,[&]{FNiagaraStackGraphUtilities::GetStackFunctionInputs(
        *MatchedFunctionCall,
        InputVariables,
        HiddenVariables,
        ConstantResolver,
        FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,
        true);}))return W.Error(W.R.Error);

    FNiagaraVariable* MatchedInputVariable = InputVariables.FindByPredicate(
        [&InputName](const FNiagaraVariable& Candidate)
        {
            return NiagaraModuleInputNameMatches(Candidate, InputName);
        });

    // Scratch-pad metadata fallback, matching HandleSetNiagaraModuleInputLinkedParameter:
    // Custom HLSL scratch inputs, especially Array DIs rewired from hidden User
    // nodes to Module.* ParameterMapGet pins, may not enumerate through
    // GetStackFunctionInputs even though the module variable exists in metadata.
    FNiagaraVariable ScratchPadMetadataFallbackInput;
    bool bUsedScratchPadMetadataFallback = false;
    if (!MatchedInputVariable && MatchedFunctionCall->FunctionScript)
    {
        UNiagaraScriptSource* FunctionScriptSource = Cast<UNiagaraScriptSource>(MatchedFunctionCall->FunctionScript->GetLatestSource());
        UNiagaraGraph* FunctionGraph = FunctionScriptSource ? FunctionScriptSource->NodeGraph : nullptr;
        if (FunctionGraph)
        {
            UNiagaraGraph::FScriptVariableMap& ScriptVariableMap = FunctionGraph->GetAllMetaData();
            for (const TPair<FNiagaraVariable, TObjectPtr<UNiagaraScriptVariable>>& Pair : ScriptVariableMap)
            {
                if (NiagaraModuleInputNameMatches(Pair.Key, InputName))
                {
                    ScratchPadMetadataFallbackInput = Pair.Key;
                    MatchedInputVariable = &ScratchPadMetadataFallbackInput;
                    bUsedScratchPadMetadataFallback = true;
                    break;
                }
            }
        }
    }

    if (!MatchedInputVariable)
    {
        return W.Error(FString::Printf(TEXT("No matching module input was found: %s"), *InputName));
    }

    const FNiagaraTypeDefinition InputType = MatchedInputVariable->GetType();
    const bool bIsSupportedArrayDataInterface = IsMcpSupportedNiagaraArrayDataInterfaceType(InputType);
    if (bRequireArrayDataInterface && !bIsSupportedArrayDataInterface)
    {
        return W.Error(
            FString::Printf(TEXT("Module input '%s' is not a supported Niagara Array Data Interface input. Actual type: %s"),
                *MatchedInputVariable->GetName().ToString(),
                *NiagaraTypeName(InputType)));
    }
    if (InputType.IsDataInterface() || InputType.IsUObject())
    {
        if (!(bRequireArrayDataInterface && bIsSupportedArrayDataInterface))
        {
            return W.Error(
                FString::Printf(TEXT("Module input '%s' cannot be linked by this command. Use a dedicated command for data interfaces or object inputs. Actual type: %s"),
                    *MatchedInputVariable->GetName().ToString(),
                    *NiagaraTypeName(InputType)));
        }
    }
    if (!IsSettableNiagaraUserParameterType(InputType))
    {
        return W.Error(
            FString::Printf(TEXT("Module input '%s' cannot be linked to a User parameter by this command. Actual type: %s"),
                *MatchedInputVariable->GetName().ToString(),
                *NiagaraTypeName(InputType)));
    }

    const TSharedPtr<FJsonValue> DefaultValue = Params->TryGetField(TEXT("default_value"));
    const bool bHasDefaultValue = DefaultValue.IsValid();
    if (bHasDefaultValue)
    {
        if (bIsSupportedArrayDataInterface)
        {
            return W.Error(
                TEXT("Array Data Interface User parameters do not support literal default_value in this command; set runtime array contents from Blueprint or create the user parameter with upsert_niagara_user_parameters"));
        }
        if(!SafeDefaultNumbers(InputType,DefaultValue))return W.Error(W.R.Error);
        FNiagaraVariable DefaultValidationVariable(InputType, FName(*UserParameterName));
        FString DefaultValidationError;
        if (!SetNiagaraVariableDataFromJson(DefaultValidationVariable, DefaultValue, DefaultValidationError))
        {
            return W.Error(DefaultValidationError);
        }
    }

    FNiagaraVariable UserParameter(InputType, FName(*UserParameterName));
    FNiagaraUserRedirectionParameterStore& UserParameterStore = System->GetExposedParameters();

    bool bUserParameterAlreadyExisted = false;
    int32 UserParameterMatchCount = 0;
    TArray<FNiagaraVariable> UserParameters;
    UserParameterStore.GetUserParameters(UserParameters);
    for (const FNiagaraVariable& Candidate : UserParameters)
    {
        if (NiagaraUserParameterNameMatches(Candidate, UserParameterName))
        {
            if(++UserParameterMatchCount>1)return W.Error(TEXT("User parameter name matches multiple aliases; refusing an ambiguous binding target."));
            bUserParameterAlreadyExisted = true;
            if (Candidate.GetType() != InputType)
            {
                return W.Error(
                    FString::Printf(TEXT("Existing user parameter '%s' has incompatible type: %s. Expected: %s"),
                        *UserParameterName,
                        *NiagaraTypeName(Candidate.GetType()),
                        *NiagaraTypeName(InputType)));
            }
            UserParameter = Candidate;
        }
    }
    const FString LegacyUserParameterName=UserParameter.GetName().ToString();
    if(bUserParameterAlreadyExisted)
    {
        if(!W.Admit(TEXT("resolve_selected_user_alias")))return W.Error(W.R.Error);
        W.Prefix->SetStringField(TEXT("selected_user_alias_name"),LegacyUserParameterName);
        if(!UserParameterStore.RedirectUserVariable(UserParameter))return W.Error(TEXT("Selected User alias could not be resolved to its full typed declaration."));
        if(!VerifyCanonicalUserForPath(W,UserParameter))return W.Error(W.R.Error);
    }
    W.Prefix->SetStringField(TEXT("selected_user_full_name"),UserParameter.GetName().ToString());

    TSharedPtr<FJsonValue> ExpectedDefault;if(bHasDefaultValue){ExpectedDefault=ExpectedScalarForPath(UserParameter,DefaultValue);if(!ExpectedDefault)return W.Error(TEXT("Failed to prepare typed expected User default"));}
    const FNiagaraParameterHandle AliasedInputHandle = BuildAliasedModuleParameterHandleForInput(MatchedFunctionCall, *MatchedInputVariable);

    UEdGraphPin* ExistingOverridePin = FindNiagaraStackFunctionInputOverridePin(MatchedFunctionCall, AliasedInputHandle);
    const int32 PreviousLinkCount = ExistingOverridePin ? ExistingOverridePin->LinkedTo.Num() : 0;
    if (PreviousLinkCount > 0 && !bOverwriteExisting)
    {
        return W.Error(
            FString::Printf(TEXT("Module input override already exists for %s. Use overwrite_existing=true to replace it."),
                *MatchedInputVariable->GetName().ToString()));
    }
    if (PreviousLinkCount > 1)
    {
        return W.Error(
            FString::Printf(TEXT("Module input override for %s has %d links; refusing to overwrite an unexpected graph shape."),
                *MatchedInputVariable->GetName().ToString(),
                PreviousLinkCount));
    }
    if (PreviousLinkCount == 1 && ExistingOverridePin)
    {
        FString ValidateError;
        if (!ValidateLinkedNiagaraDataInterfaceInputNode(*ExistingOverridePin, ValidateError))
        {
            return W.Error(ValidateError);
        }
    }

    if(!W.Own(MatchedOwningEmitter.Emitter)||!W.GraphForWrite(Cast<UNiagaraGraph>(MatchedFunctionCall->GetGraph()))||!W.WaitAdmission(bWaitForCompletion,bRequestCompile)||!PreflightStackMap(W,MatchedFunctionCall))return W.Error(W.R.Error);
    const auto StoreBefore=CaptureUserValues(W);if(!ContinueRead())return W.Error(W.R.Error);W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);
    if(!W.Modify(System))return W.Error(W.R.Error);
    if (!bUserParameterAlreadyExisted)
    {
        if (!AddUserForPath(W,UserParameter))
        {
            return W.Error(
                FString::Printf(TEXT("Failed to add Niagara user parameter: %s"), *UserParameter.GetName().ToString()));
        }
    }

    if(!VerifyCanonicalUserForPath(W,UserParameter))return W.Error(W.R.Error);
    bool bCreatedDataInterface = false;
    FString DataInterfaceClassName;
    FString DataInterfacePath;
    if (bIsSupportedArrayDataInterface)
    {
        UNiagaraDataInterface* ExistingDataInterface = UserParameterStore.GetDataInterface(UserParameter);
        UClass* DataInterfaceClass = UserParameter.GetType().GetClass();
        if (!ExistingDataInterface && DataInterfaceClass)
        {
            UNiagaraDataInterface* NewDataInterface=CreateUserDIForPath(W,DataInterfaceClass);
            if (!NewDataInterface)
            {
                return W.Error(
                    FString::Printf(TEXT("Failed to create Niagara Array Data Interface for user parameter: %s"), *UserParameter.GetName().ToString()));
            }

            if(!SetUserDIForPath(W,UserParameter,NewDataInterface))return W.Error(W.R.Error);
            ExistingDataInterface = NewDataInterface;
            bCreatedDataInterface = true;
        }

        if (ExistingDataInterface)
        {
            DataInterfaceClassName = ExistingDataInterface->GetClass()->GetName();
            DataInterfacePath = ExistingDataInterface->GetPathName();
        }
    }

    if(!VerifyUnrelatedUserValues(W,StoreBefore,UserParameter,!bUserParameterAlreadyExisted))return W.Error(W.R.Error);
    if (bHasDefaultValue)
    {
        FString DefaultSetError;
        if (!SetDefaultForPath(W,UserParameterStore,UserParameter,DefaultValue,DefaultSetError))
        {
            return W.Error(DefaultSetError);
        }
    }

    if(!W.Modify(MatchedFunctionCall))return W.Error(W.R.Error);
    UEdGraphPin* CreatedOverride=CreateOverrideForPath(W,MatchedFunctionCall,AliasedInputHandle,InputType);if(!CreatedOverride||!ContinueRead())return W.Error(W.R.Error);UEdGraphPin& OverridePin=*CreatedOverride;

    if (PreviousLinkCount == 1)
    {
        FString ClearError;
        if (!ClearLinkedForPath(W,OverridePin,ClearError))
        {
            return W.Error(ClearError);
        }
    }

    const FNiagaraVariable LinkUserParameter=UserParameter;
    if(!LinkUserForPath(W,MatchedFunctionCall,&OverridePin,LinkUserParameter)||!VerifyBoundUser(W,MatchedFunctionCall,AliasedInputHandle,UserParameter,StoreBefore,!bUserParameterAlreadyExisted,ExpectedDefault))return W.Error(W.R.Error);
    W.Prefix->SetBoolField(TEXT("binding_readback_verified"),true);
    if(!W.Dirty())return W.Error(W.R.Error);
    if (bRequestCompile)
    {
        bool Accepted=false;if(!W.Request(Accepted))return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved=W.Save(false);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after module input user parameter update"));
        }
    }

    if(!ContinueRead())return W.Error(W.R.Error);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), MatchedEmitterName);
    Result->SetNumberField(TEXT("emitter_index"), MatchedEmitterIndex);
    Result->SetStringField(TEXT("module_name"), MatchedModuleName);
    Result->SetStringField(TEXT("module_node_guid"), MatchedFunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("module_index"), MatchedModuleIndex);
    Result->SetStringField(TEXT("input_name"), MatchedInputVariable->GetName().ToString());
    Result->SetStringField(TEXT("input_type"), NiagaraTypeName(InputType));
    Result->SetStringField(TEXT("aliased_input_handle"), AliasedInputHandle.GetParameterHandleString().ToString());
    Result->SetStringField(TEXT("user_parameter_name"), LegacyUserParameterName);
    Result->SetStringField(TEXT("linked_parameter_name"), LinkUserParameter.GetName().ToString());
    Result->SetBoolField(TEXT("used_scratch_pad_metadata_fallback"), bUsedScratchPadMetadataFallback);
    Result->SetBoolField(TEXT("user_parameter_created"), !bUserParameterAlreadyExisted);
    Result->SetBoolField(TEXT("default_value_set"), bHasDefaultValue);
    Result->SetBoolField(TEXT("array_data_interface_user_parameter"), bIsSupportedArrayDataInterface);
    if (bIsSupportedArrayDataInterface)
    {
        Result->SetBoolField(TEXT("data_interface_created"), bCreatedDataInterface);
        if (!DataInterfaceClassName.IsEmpty())
        {
            Result->SetStringField(TEXT("data_interface_class"), DataInterfaceClassName);
            Result->SetStringField(TEXT("data_interface_path"), DataInterfacePath);
        }
    }
    if (bHasDefaultValue)
    {
        if(!VerifyCanonicalUserForPath(W,UserParameter))return W.Error(W.R.Error);
        Result->SetField(TEXT("default_value"), NiagaraParameterValueToJson(UserParameterStore, UserParameter));
    }
    Result->SetNumberField(TEXT("previous_override_link_count"), PreviousLinkCount);
    Result->SetBoolField(TEXT("overwrote_existing"), PreviousLinkCount > 0);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetObjectField(TEXT("compile_wait"), WaitForNiagaraCompileJson(System, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds));
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), bIsSupportedArrayDataInterface
        ? TEXT("array_data_interface_module_input_user_parameter_link")
        : TEXT("module_input_user_parameter_link"));
    return Result;
}

FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
    bool Success=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),Success);bool Complete=false;
    if(W.Before)W.R.Data->SetObjectField(TEXT("before"),W.Before);
    if(Success&&W.R.Error.IsEmpty()&&W.Before)
    {
        auto After=W.Projection();W.R.Data->SetObjectField(TEXT("after"),After);
        bool BeforeComplete=false,AfterComplete=false;W.Before->TryGetBoolField(TEXT("source_projection_complete"),BeforeComplete);After->TryGetBoolField(TEXT("source_projection_complete"),AfterComplete);
        if(BeforeComplete&&AfterComplete){W.R.bChanged|=!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive);Complete=W.Admit(TEXT("final_target_admission"));}
    }
    if(!Success&&W.R.Error.IsEmpty()){FString Message,Code;if(Legacy){Legacy->TryGetStringField(TEXT("error"),Message);Legacy->TryGetStringField(TEXT("error_code"),Code);}W.Fail(Code.IsEmpty()?TEXT("niagara_input_path_request_failed"):*Code,Message.IsEmpty()?TEXT("Original input-path request failed; see legacy_result."):Message);}
    if(!W.SaveHealth&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_input_path_save_failed"),TEXT("At least one current SDK save failed; preceding actual save history retained."));
    W.R.bSuccess=Success&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess&&Complete;
    const bool Scoped=W.R.bSuccess&&W.R.bPostconditionPassed&&Complete;if(Scoped)W.R.bChangeStateKnown=true;
    W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());
    W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);
    W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.Saves);W.R.Data->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);W.R.Data->SetArrayField(TEXT("actual_item_prefix"),W.Partial);W.R.Data->SetArrayField(TEXT("actual_sdk_prefix"),W.MutationRows);
    W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),Complete);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),Scoped);
    W.R.Data->SetStringField(TEXT("scoped_known_scope"),TEXT("Success/readback plus complete dedicated System/emitter/source/graph/physical pin/link/typed metadata/User declaration/value/DI identity projection and final admission. Every DI internal property, loader/cache/global purity and effective compilation remain outside this target scope."));
    W.R.Data->SetBoolField(TEXT("guarded_path_apply_implemented"),W.GuardedRemoval&&W.DeferredMode);W.R.Data->SetBoolField(TEXT("required_compile_wait_implemented"),W.DeferredMode);
    W.R.Data->SetStringField(TEXT("compile_scope"),W.GuardedRemoval?TEXT("Original guarded-removal idle/ready/synchronized baseline and final warning-nonincrease/computed-ID-change gate plus repeated complete absence are required before optional save; timeout fails this gate. Global GPU/shader and internal nonblocking behavior are not inferred."):(W.DeferredMode?TEXT("HN bounded GT observation resumes after the original compile/save suffix; timeout is an observed terminal outcome, not successful compilation. Shader/GPU/internal blocking remain unproved."):TEXT("RequestCompile acceptance is separate from CPU/shader/GPU completion. Genuine pending/requested wait remains explicit incomplete without deferred final continuation. No GT sleep/forced flush.")));
    W.R.Data->SetStringField(TEXT("save_scope"),W.GuardedRemoval?TEXT("Guarded removal saves only after its original strict compile/absence gate. Actual SaveLoadedAsset true is OR history before callback guard; later failure never erases it or clears dirty again."):TEXT("SDK SaveLoadedAsset returned true is OR history, preserved before post-SDK guard and actual dirty readback. A later dirty/callback/wait failure does not erase the earlier true or clear dirty again. Binding preserves original save-before-wait order."));
    W.R.Data->SetBoolField(TEXT("atomic_asset_or_undo_rollback_verified"),false);
    W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;
    return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,FHandler Handler,bool Guarded,bool ForceArray)
{
    FWork W;TSharedPtr<FJsonObject> Params;
    if(!ParseParams(Text,Params)){W.Fail(TEXT("niagara_input_path_invalid_params_json"),TEXT("Expected finite JSON object within1Mi UTF16/depth64/262144 tokens/visits; duplicate case-sensitive keys and NUL are rejected."));return Finish(W,W.Error(W.R.Error),Operation);}
    if(Running){W.Fail(TEXT("niagara_input_path_reentrant_request"),TEXT("A dedicated Niagara input-path request is already active; no replay."));return Finish(W,W.Error(W.R.Error),Operation);}
    TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);
    W.R.Data->SetObjectField(TEXT("requested_params"),MakeShared<FJsonObject>(*Params));
    if(Guarded){bool Dry=true;Params->TryGetBoolField(TEXT("dry_run"),Dry);W.WriterIntent=!Dry;}
    if(ForceArray){Params->SetBoolField(TEXT("require_array_data_interface"),true);W.Prefix->SetBoolField(TEXT("Array_dispatch_forced_require_array_data_interface"),true);}
    if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);
    return Finish(W,Handler(W,Params),Operation);
}
}
FString RemoveUnusedPath(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("remove_niagara_unused_module_input_path"),&HandleRemoveUnusedPath,true,false);}
FString BindUserParameter(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_module_input_user_parameter"),&HandleBindUser,false,false);}
FString BindArrayUserParameter(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_array_data_interface_module_input_user_parameter"),&HandleBindUser,false,true);}
// HN: this concrete controller resumes only this existing leaf. Core/handler/Finish above are shared.
namespace
{
using HNJob=UCubelessNiagaraScratchUserRecipeJob;
namespace HN=NiagaraScratchUserRecipeAsync;
bool HNGlobals(){return IsInGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
struct FHNLeafJob;
TSharedPtr<FHNLeafJob> HNPending;
bool HNStarting=false;
TArray<TSharedPtr<FHNLeafJob>> HNRetainedUnsafe;
struct FHNLeafJob:TSharedFromThis<FHNLeafJob>
{
    FWork W;
    TWeakObjectPtr<HNJob> Job;HNJob* Key=nullptr;FGuid Id=FGuid::NewGuid();FString JobPath,LastJson;TSharedPtr<FJsonObject> Params,Legacy;
    FTSTicker::FDelegateHandle Ticker;int64 Revision=0;uint64 LastFrame=MAX_uint64;bool Executing=false,Dispatched=false,Terminal=false,Quarantined=false;
    const TCHAR* Operation=TEXT("set_niagara_module_input_user_parameter");
    explicit FHNLeafJob(bool ArrayBinding,bool RemoveUnused=false){if(RemoveUnused){Operation=TEXT("remove_niagara_unused_module_input_path");W.GuardedRemoval=true;}else if(ArrayBinding)Operation=TEXT("set_niagara_array_data_interface_module_input_user_parameter");W.DeferredMode=true;W.R.Data->SetBoolField(TEXT("external_transaction_across_yield_allowed"),false);W.R.Data->SetBoolField(TEXT("all_SDK_calls_nonblocking_proven"),false);W.R.Data->SetBoolField(TEXT("compiler_cancel_attempted"),false);}
    void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
    bool PublicationSafe()const
    {
        // Central publication permits only its owned normal-pre-exit cancellation scope.
        if(!HN::CanPublishJobObject())return false;
        if(!W.WorldCaptured)return true;
        if(W.CapturedEditor.Get()!=GEditor)return false;
        auto* EditorWorld=GEditor->GetEditorWorldContext().World();
        return Live(EditorWorld)&&!EditorWorld->bIsTearingDown&&W.World.Get()==EditorWorld&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext();
    }
    bool JobExact()const{return PublicationSafe()&&Job.Get()==Key&&Live(Key)&&Key->GetClass()==HNJob::StaticClass()&&Key->GetOuter()==GetTransientPackage()&&Key->GetOutermost()==GetTransientPackage()&&Key->GetPackage()==GetTransientPackage()&&Key->HasAnyFlags(RF_Transient)&&Key->GetPathName().Equals(JobPath,ESearchCase::CaseSensitive);}
    void ReleaseExecutionHolds(){if(W.GuardedRemoval&&!Terminal)return;/* Removed source/metadata objects have no graph owner reference during the wait. */if(HN::CanPublishJobObject())for(auto& Identity:W.Objects)Identity->Hold.Reset();}
    bool AcquireExecutionHolds(){if(!W.Admit(TEXT("HN_resume_before_execution_pins")))return false;for(auto& Identity:W.Objects){auto* O=Identity->Object.Get();if(!Live(O))return W.Fail(TEXT("niagara_recipe_wait_object_expired"),TEXT("Captured weak target expired; no reload or retry."));Identity->Hold=TStrongObjectPtr<UObject>(O);}return W.Admit(TEXT("HN_resume_after_execution_pins"));}
    void Prefix(bool Final)
    {
        W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::Digits));W.R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
        W.R.Data->SetBoolField(TEXT("native_job_terminal"),Final);W.R.Data->SetBoolField(TEXT("operation_complete"),Final);W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);W.R.Data->SetBoolField(TEXT("single_core_dispatch"),Dispatched);
        W.R.Data->SetBoolField(TEXT("bounded_wait_policy_observation"),true);W.R.Data->SetBoolField(TEXT("compile_idle_observed_at_wait_end"),W.WaitRequested&&W.WaitIdle);W.R.Data->SetBoolField(TEXT("bounded_wait_timed_out"),W.WaitTimedOut);
        W.R.Data->SetBoolField(TEXT("compiler_may_continue_after_job"),W.R.bCompileAttempted||(W.WaitRequested&&!W.WaitIdle));W.R.Data->SetBoolField(TEXT("successful_compilation_proven"),false);W.R.Data->SetBoolField(TEXT("required_compile_wait_implemented"),true);
        if(Legacy)W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);if(W.Before)W.R.Data->SetObjectField(TEXT("before"),W.Before);
        W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.Saves);W.R.Data->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);W.R.Data->SetArrayField(TEXT("actual_item_prefix"),W.Partial);W.R.Data->SetArrayField(TEXT("actual_sdk_prefix"),W.MutationRows);
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
            if(W.WaitObject){W.WaitObject->SetBoolField(TEXT("wait_timed_out"),W.WaitTimedOut);W.WaitObject->SetNumberField(TEXT("wait_iterations"),W.WaitIterations);W.WaitObject->SetNumberField(TEXT("wait_elapsed_seconds"),Elapsed);W.WaitObject->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);W.WaitObject->SetBoolField(TEXT("active_compilations_after"),Active);}
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
        if(!Dispatched){Dispatched=true;Legacy=W.GuardedRemoval?HandleRemoveUnusedPath(W,Params):HandleBindUser(W,Params);}
        else if(W.WaitDeferred&&!WaitStep()){if(!HNGlobals())Quarantine(W.R.Error);else{Legacy=W.Error(W.R.Error);Final();}return false;}
        if(!HNGlobals()){Quarantine(TEXT("SDK returned across unsafe lifetime; actual prefix cached without publication/cleanup."));return false;}
        if(!W.R.Error.IsEmpty()||W.HNCancelRequested){Legacy=W.Error(W.HNCancelRequested?W.HNCancelReason:W.R.Error);Final();return false;}
        if(!W.WaitDeferred){if(W.RemovalTail){auto Tail=MoveTemp(W.RemovalTail);Legacy=Tail();}Final();return false;}PublishPending();ReleaseExecutionHolds();return !Quarantined;
    }
};
}
UCubelessNiagaraScratchUserRecipeJob* StartUserBindingContinuation(const FString& Text,bool ArrayBinding,bool RemoveUnused=false)
{
    // One concrete controller owns both modes, including an unsafe retained lease.
    if(!HNGlobals()||NiagaraScratchUserRecipeAsync::IsStopping()||HNStarting||HNPending.IsValid())return nullptr;
    TGuardValue<bool> StartingScope(HNStarting,true);
    auto State=MakeShared<FHNLeafJob>(ArrayBinding,RemoveUnused);bool Parsed=ParseParams(Text,State->Params);if(!Parsed)State->W.Fail(TEXT("niagara_recipe_wait_invalid_params_json"),TEXT("Expected original bounded finite leaf params object."));
    if(Parsed){State->W.R.Data->SetObjectField(TEXT("requested_params"),MakeShared<FJsonObject>(*State->Params));if(RemoveUnused){bool Dry=true;State->Params->TryGetBoolField(TEXT("dry_run"),Dry);State->W.WriterIntent=!Dry;}if(ArrayBinding){State->Params->SetBoolField(TEXT("require_array_data_interface"),true);State->W.Prefix->SetBoolField(TEXT("Array_dispatch_forced_require_array_data_interface"),true);}}
    if(Parsed){FString Path;State->Params->TryGetStringField(TEXT("system_path"),Path);const FString Expanded=NormalizeNiagaraObjectPathForLoad(Path);if(Path.Len()>=NAME_SIZE||Expanded.Len()>=NAME_SIZE)State->W.Fail(TEXT("niagara_recipe_wait_path_bound"),TEXT("Raw/expanded FName path must fit NAME_SIZE before any target load."));}
    if(Running)State->W.Fail(TEXT("niagara_recipe_wait_core_busy"),TEXT("Existing synchronous/deferred core lease is active; no nested dispatch."));
    if(HNPending.IsValid())State->W.Fail(TEXT("niagara_recipe_wait_leaf_busy"),TEXT("This exact leaf controller already has a pending/quarantined invocation; no replay."));
    if(State->W.R.Error.IsEmpty()&&!State->W.Admit(TEXT("HN_before_job_creation"))){}
    auto* Created=NewObject<UCubelessNiagaraScratchUserRecipeJob>(GetTransientPackage(),NAME_None,RF_Transient);State->Key=Created;State->Job=Created;if(!HNGlobals()||!Live(Created))return nullptr;State->JobPath=Created->GetPathName();NiagaraScratchUserRecipeAsync::RegisterJob(Created,RemoveUnused?NiagaraScratchUserRecipeAsync::EJobKind::RemoveUnusedPath:(ArrayBinding?NiagaraScratchUserRecipeAsync::EJobKind::ArrayBinding:NiagaraScratchUserRecipeAsync::EJobKind::ScalarBinding),State->Id);
    if(RemoveUnused){State->W.R.bMutationAttempted=true;State->W.R.Data->SetNumberField(TEXT("NewObject_NiagaraScratchUserRecipeJob"),1);State->W.R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);State->W.R.Data->SetBoolField(TEXT("transient_creation_only"),true);}
    if(!State->W.R.Error.IsEmpty()){State->Legacy=State->W.Error(State->W.R.Error);State->Final();return Created;}
    HNPending=State;State->PublishPending();State->Step();if(!State->Terminal&&!State->Quarantined){TWeakPtr<FHNLeafJob> Weak=State;State->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float){auto Strong=Weak.Pin();return Strong.IsValid()&&Strong->Step();}));}return Created;
}
UCubelessNiagaraScratchUserRecipeJob* StartRemoveUnusedPathContinuation(const FString& Text){return StartUserBindingContinuation(Text,false,true);}
UCubelessNiagaraScratchUserRecipeJob* StartScalarBindingContinuation(const FString& Text){return StartUserBindingContinuation(Text,false);}
UCubelessNiagaraScratchUserRecipeJob* StartArrayBindingContinuation(const FString& Text){return StartUserBindingContinuation(Text,true);}
bool CancelScalarBindingContinuation(UCubelessNiagaraScratchUserRecipeJob* Job,const FString& Reason)
{if(!IsInGameThread()||!HNPending.IsValid()||HNPending->Key!=Job)return false;auto State=HNPending;State->CancelPlain(Reason);return true;}
void ShutdownScalarBindingContinuation(const FString& Reason)
{if(!IsInGameThread())return;if(auto State=HNPending){State->Unhook();State->CancelPlain(Reason);}}

}
