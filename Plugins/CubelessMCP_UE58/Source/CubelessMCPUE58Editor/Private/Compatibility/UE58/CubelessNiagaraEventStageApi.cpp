
// GS dedicated Editor-only Niagara simulation-stage/event inspection and event authoring. Source-only candidate.
#include "Compatibility/UE58/CubelessNiagaraEventStageApi.h"
#include "CubelessNiagaraEmitterEventAuthoringJob.h"
#include "Compatibility/UE58/CubelessNiagaraEmitterEventAuthoringAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraAuthoringInspectionApi.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#include "UObject/SavePackage.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "NiagaraDataInterfaceRenderTarget2D.h"
#include "NiagaraDataInterfaceTexture.h"
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
#include "NiagaraNodeAssignment.h"
#include "UObject/PropertyAccessUtil.h"
#include "ToolsetRegistry/ToolsetLibrary.h"
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
namespace Cubeless::UE58::NiagaraEventStage
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Running=false;
bool DeferredLeaseActive=false;
bool Safe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage;}
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
{UEdGraphPin* Pointer=nullptr;FGuid Id,PersistentId;bool NotConnectable=false,DefaultReadOnly=false;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;TArray<UEdGraphPin*> Links;FString DefaultValue,AutoDefault,DefaultText;TWeakObjectPtr<UObject> DefaultObject;};
struct FNodeIdentity
{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;int32 X=0,Y=0;TArray<FPinIdentity> Pins;TWeakObjectPtr<UNiagaraScript> FunctionScript;FGuid FunctionVersion;FName FunctionAsset;FNiagaraFunctionSignature FunctionSignature;TMap<FName,FName> Specifiers;bool Function=false;};
FString GraphMetadataSignature(UNiagaraGraph* G);
struct FGraphIdentity
{TWeakObjectPtr<UNiagaraGraph> Graph;TWeakObjectPtr<UClass> Schema;TArray<FNodeIdentity> Nodes;FString Meta;};
struct FSourceIdentity
{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;};
FString EventStateKey(const FVersionedNiagaraEmitterData*,const FNiagaraEventScriptProperties* =nullptr,int32 =INDEX_NONE);
FString StageStateKey(UNiagaraSimulationStageBase*);
struct FEmitterIdentity
{FGuid Id,InstanceVersion,DataVersion;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,ParentScratch;TArray<TWeakObjectPtr<UNiagaraScript>> ScratchScripts,ParentScripts;TArray<TWeakObjectPtr<UNiagaraSimulationStageBase>> Stages;FString EventKey;FName Name;bool Enabled=false;ENiagaraSimTarget SimTarget=ENiagaraSimTarget::CPUSim;};
struct FScriptBinding{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;ENiagaraScriptUsage Usage=ENiagaraScriptUsage::Function;FGuid UsageId;};
struct FStageBinding{TWeakObjectPtr<UNiagaraSimulationStageBase> Stage;TWeakObjectPtr<UNiagaraScript> Script;FName Name;FGuid OuterVersion;FString State;};

UNiagaraDataInterface* ReflectedNodeDI(const UNiagaraNodeInput* Node)
{
 // GetDataInterface is public but not NIAGARAEDITOR_API on MinimalAPI NodeInput.
 // Read its exact reflected UPROPERTY with exported CoreUObject reflection; no private C++ getter/write or offset arithmetic.
 const FObjectPropertyBase* Field=Node?FindFProperty<FObjectPropertyBase>(Node->GetClass(),TEXT("DataInterface")):nullptr;
 return Field&&Field->ArrayDim==1&&Field->PropertyClass==UNiagaraDataInterface::StaticClass()?Cast<UNiagaraDataInterface>(Field->GetObjectPropertyValue_InContainer(Node)):nullptr;
}
struct FExpectedDI{TWeakObjectPtr<UNiagaraNodeInput> Node;TWeakObjectPtr<UNiagaraDataInterface> DI;FNiagaraVariableBase User;TWeakObjectPtr<UTexture> Texture;bool RT=false,Inherit=false;FNiagaraVariableBase Input;int32 Sort=0;ENiagaraInputNodeUsage Usage=ENiagaraInputNodeUsage::Parameter;};

struct FWork
{
    FResult R;bool DeferredMode=false,CancelRequested=false,PersistentMutationAttempted=false;FString CancelReason;TWeakObjectPtr<UEditorEngine> CapturedEditor;TFunction<bool()> DeferredStep;FString FailedStage;TWeakObjectPtr<UWorld> World;bool WorldCaptured=false,SaveHealth=true,WriterIntent=true;
    TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<TWeakObjectPtr<UNiagaraScript>> Scratch;TArray<FEmitterIdentity> Emitters;
    TArray<FScriptBinding> ScriptBindings;TArray<FStageBinding> StageBindings;TArray<TUniquePtr<FIdentity>> Objects;TArray<FGraphIdentity> Graphs;TArray<FSourceIdentity> Sources;
    UNiagaraGraph* TransitionGraph=nullptr;TArray<FExpectedDI> ExpectedDI;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Saves,Compiles,Partial,MutationRows;TSharedPtr<FJsonObject> Before;int64 Slots=0,TextUnits=0,Calls=0;
    FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty())Fail(TEXT("niagara_event_stage_request_failed"),Message.IsEmpty()?TEXT("Dedicated graph operation failed; actual prefix retained."):Message);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
    bool Budget(int64 N,int64 Text=0){Slots+=N;TextUnits+=Text;Prefix->SetNumberField(TEXT("captured_slots"),static_cast<double>(Slots));Prefix->SetNumberField(TEXT("observed_text_utf16_units"),static_cast<double>(TextUnits));return (Slots<=262144&&TextUnits<=32ll*1024*1024)||Fail(TEXT("niagara_event_stage_budget_exceeded"),TEXT("Bounded capture/output budget exceeded; partial prefix retained."));}
    bool Pin(UObject* O){if(!Live(O))return Fail(TEXT("niagara_event_stage_object_unavailable"),TEXT("Expected live loaded object."));for(const auto& I:Objects){if(I->Object.Get()==O)return I->Exact()||Fail(TEXT("niagara_event_stage_identity_changed"),TEXT("Captured object changed."));}if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool Own(UObject* O)
    {auto* Root=System.Get();if(!Admit(TEXT("writer_ownership"))||!Live(O)||!Live(Root)||(O!=Root&&!O->IsIn(Root))||O->GetOutermost()!=Root->GetOutermost()||O->GetPackage()!=Root->GetPackage())return Fail(TEXT("niagara_event_stage_external_writer_unsupported"),TEXT("Actual writer is outside the specified System outer chain/package. No external emitter/shared graph/source was changed."));if(!Pin(O))return false;for(auto& I:Objects){if(I->Object.Get()==O){I->Writer=true;I->Root=Root;}}return true;}
    bool Admit(const FString& Stage)
    {
        R.Stage=Stage;if(DeferredMode&&(CancelRequested||NiagaraEmitterEventAuthoringAsync::IsStopping()))return Fail(TEXT("niagara_event_stage_async_cancelled"),CancelReason.IsEmpty()?TEXT("Owned continuation stopped; no further SDK."):CancelReason);if(!R.Error.IsEmpty())return false;
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||GIsSavingPackage)return Fail(TEXT("niagara_event_stage_context_unavailable"),TEXT("Live Editor game thread without GC/exit/save required; no replay."));
        if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_event_stage_editor_changed"),TEXT("Captured Editor changed before getter."));
        if(WriterIntent&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext()))return Fail(TEXT("niagara_event_stage_play_session_active"),TEXT("Niagara binding authoring requires stopped PIE/SIE."));
        UWorld* CurrentWorld=GEditor->GetEditorWorldContext().World();if(!Live(CurrentWorld)||CurrentWorld->bIsTearingDown)return Fail(TEXT("niagara_event_stage_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){World=CurrentWorld;if(DeferredMode)CapturedEditor=GEditor;WorldCaptured=true;}else if(World.Get()!=CurrentWorld)return Fail(TEXT("niagara_event_stage_world_changed"),TEXT("Editor world changed across SDK dispatch."));
        for(const auto& I:Objects){if(!I->Exact())return Fail(TEXT("niagara_event_stage_identity_changed"),TEXT("Captured class/outer/package/path/ownership changed; no replacement adopted."));}
        if(System.IsValid())
        {auto* O=System.Get();if(O->GetSystemSpawnScript()!=Spawn.Get()||O->GetSystemUpdateScript()!=Update.Get()||O->ScratchPadScripts.Num()!=Scratch.Num()||O->GetEmitterHandles().Num()!=Emitters.Num())return Fail(TEXT("niagara_event_stage_system_topology_changed"),TEXT("Captured System emitter/script topology changed."));
            for(int32 I=0;I<Scratch.Num();++I)if(O->ScratchPadScripts[I]!=Scratch[I].Get())return Fail(TEXT("niagara_event_stage_scratch_membership_changed"),TEXT("System scratch membership changed."));
            for(int32 I=0;I<Emitters.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=Emitters[I];if(H.GetId()!=E.Id||H.GetName()!=E.Name||H.GetIsEnabled()!=E.Enabled||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.InstanceVersion||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion))return Fail(TEXT("niagara_event_stage_emitter_version_changed"),TEXT("Physical emitter/ref/version changed."));
                if(E.Data){if(E.Data->SimTarget!=E.SimTarget||!EventStateKey(E.Data).Equals(E.EventKey,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_event_stage_event_topology_changed"),TEXT("Captured event settings/script/source generator identity changed outside the admitted operation."));if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.ParentScratch.Get())return Fail(TEXT("niagara_event_stage_emitter_routing_changed"),TEXT("Emitter source/scratch binding changed."));
                    auto Same=[](const UNiagaraScratchPadContainer* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& P){if(!C)return P.IsEmpty();if(C->Scripts.Num()!=P.Num())return false;for(int32 J=0;J<P.Num();++J)if(C->Scripts[J]!=P[J].Get())return false;return true;};
                    if(!Same(E.Scratch.Get(),E.ScratchScripts)||!Same(E.ParentScratch.Get(),E.ParentScripts))return Fail(TEXT("niagara_event_stage_scratch_membership_changed"),TEXT("Emitter scratch membership changed."));
                    const auto& A=E.Data->GetSimulationStages();if(A.Num()!=E.Stages.Num())return Fail(TEXT("niagara_event_stage_stage_membership_changed"),TEXT("Simulation stage count changed."));for(int32 J=0;J<A.Num();++J)if(A[J]!=E.Stages[J].Get())return Fail(TEXT("niagara_event_stage_stage_membership_changed"),TEXT("Simulation stage order changed."));
                }
            }
        }
        for(const auto& S:ScriptBindings){auto* O=S.Script.Get();if(!Live(O)||O->GetLatestSource()!=S.Source.Get()||O->GetExposedVersion().VersionGuid!=S.Version||O->GetUsage()!=S.Usage||O->GetUsageId()!=S.UsageId)return Fail(TEXT("niagara_event_stage_script_source_changed"),TEXT("Script source/exposed version changed."));}
        for(const auto& S:StageBindings){auto* O=S.Stage.Get();if(!Live(O)||O->Script!=S.Script.Get()||O->SimulationStageName!=S.Name||O->OuterEmitterVersion!=S.OuterVersion||!StageStateKey(O).Equals(S.State,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_event_stage_stage_script_changed"),TEXT("Stage name/script binding changed."));}
        for(const auto& S:Sources){if(!Live(S.Source.Get())||S.Source->NodeGraph!=S.Graph.Get())return Fail(TEXT("niagara_event_stage_source_graph_changed"),TEXT("Captured source graph binding changed."));}
        for(const auto& G:Graphs){if(G.Graph.Get()==TransitionGraph)continue;auto* O=G.Graph.Get();if(!Live(O)||O->Schema!=G.Schema.Get()||O->Nodes.Num()!=G.Nodes.Num()||!GraphMetadataSignature(O).Equals(G.Meta,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_event_stage_graph_topology_changed"),TEXT("Graph node count changed outside the planned removal."));
            for(int32 I=0;I<G.Nodes.Num();++I){const auto& E=G.Nodes[I];auto* N=E.Node.Get();if(!Live(N)||O->Nodes[I]!=N||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num())return Fail(TEXT("niagara_event_stage_node_identity_changed"),TEXT("Captured node/pin membership changed."));
                if(E.Function){auto* F=Cast<UNiagaraNodeFunctionCall>(N);if(!F||F->FunctionScript!=E.FunctionScript.Get()||F->SelectedScriptVersion!=E.FunctionVersion||F->FunctionScriptAssetObjectPath!=E.FunctionAsset||F->FunctionSpecifiers.OrderIndependentCompareEqual(E.Specifiers)==false||!FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&F->Signature,&E.FunctionSignature,0))return Fail(TEXT("niagara_event_stage_function_binding_changed"),TEXT("Selected function script/version/signature/specifiers changed across SDK callback."));}
                for(int32 J=0;J<E.Pins.Num();++J){const auto& P=E.Pins[J];if(N->Pins[J]!=P.Pointer)return Fail(TEXT("niagara_event_stage_pin_identity_changed"),TEXT("Captured pin disappeared; stale pointer not dereferenced."));if(P.Pointer&&(P.Pointer->PinId!=P.Id||P.Pointer->PersistentGuid!=P.PersistentId||P.Pointer->bNotConnectable!=P.NotConnectable||P.Pointer->bDefaultValueIsReadOnly!=P.DefaultReadOnly||P.Pointer->GetOwningNodeUnchecked()!=N||!P.Pointer->PinName.ToString().Equals(P.Name.ToString(),ESearchCase::CaseSensitive)||P.Pointer->Direction!=P.Direction||P.Pointer->PinType!=P.Type||P.Pointer->LinkedTo!=P.Links||!P.Pointer->DefaultValue.Equals(P.DefaultValue,ESearchCase::CaseSensitive)||!P.Pointer->AutogeneratedDefaultValue.Equals(P.AutoDefault,ESearchCase::CaseSensitive)||!P.Pointer->DefaultTextValue.ToString().Equals(P.DefaultText,ESearchCase::CaseSensitive)||P.Pointer->DefaultObject!=P.DefaultObject.Get()))return Fail(TEXT("niagara_event_stage_pin_links_changed"),TEXT("Pin identity/type/links differ from expected admitted transition."));}
            }
        }for(const auto& E:ExpectedDI){auto* Node=E.Node.Get();auto* DI=E.DI.Get();if(!Live(Node)||!Live(DI)||ReflectedNodeDI(Node)!=DI||Node->Input!=E.Input||Node->CallSortPriority!=E.Sort||Node->Usage!=E.Usage||DI->GetOuter()!=Node||DI->GetPackage()!=System->GetPackage())return Fail(TEXT("niagara_event_stage_DI_replaced"),TEXT("Captured DI input-node binding/owner changed."));if(E.RT){auto* D=Cast<UNiagaraDataInterfaceRenderTarget2D>(DI);if(!D||D->RenderTargetUserParameter.Parameter!=E.User||D->bInheritUserParameterSettings!=E.Inherit)return Fail(TEXT("niagara_event_stage_DI_fields_changed"),TEXT("RT public binding fields changed outside requested assignment."));}else{auto* D=Cast<UNiagaraDataInterfaceTexture>(DI);if(!D||D->TextureUserParameter.Parameter!=E.User||D->Texture!=E.Texture.Get())return Fail(TEXT("niagara_event_stage_DI_fields_changed"),TEXT("Texture public binding fields changed outside requested assignment."));}}return true;
    }
    TSharedRef<FJsonObject> MutationRow(const TCHAR* Stage){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetBoolField(TEXT("SDK_returned"),false);MutationRows.Add(MakeShared<FJsonValueObject>(Row));return Row;}
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage))return false;if(!Budget(1,FCString::Strlen(Stage)))return false;if(++Calls>262144)return Fail(TEXT("niagara_event_stage_sdk_budget"),TEXT("SDK call budget exceeded."));R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;if(DeferredMode&&Mutation&&FCString::Strcmp(Stage,TEXT("NewObject_EmitterEventJob"))!=0)PersistentMutationAttempted=true;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(Mutation)Prefix->SetBoolField(Stage,true);return true;}
    bool After(const TCHAR* Stage){Prefix->SetBoolField(FString(Stage)+TEXT("_SDK_returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
    template<typename F>bool Step(const TCHAR* Stage,bool Mutation,F&& Fnc){if(!SDK(Stage,Mutation))return false;Fnc();return After(Stage);}
    bool Modify(UObject* O){if(!Own(O))return false;Prefix->SetStringField(TEXT("last_modify_object"),O->GetPathName());return Step(TEXT("Modify"),true,[&]{O->Modify();});}
    bool Dirty(){return Step(TEXT("MarkPackageDirty"),true,[&]{System->MarkPackageDirty();});}
    bool CaptureScript(UNiagaraScript* O){if(!O)return true;if(!Pin(O))return false;for(const auto& I:ScriptBindings)if(I.Script.Get()==O)return true;ScriptBindings.Add({O,O->GetLatestSource(),O->GetExposedVersion().VersionGuid,O->GetUsage(),O->GetUsageId()});return true;}
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
#define GS_STEP(Stage,Mutation,Expr) do{if(!W.Step(TEXT(Stage),Mutation,[&]{Expr;}))return W.Error(W.R.Error);}while(false)

bool FWork::CaptureSystem(UNiagaraSystem* O)
{
    if(!Pin(O)||!Pin(O->GetOutermost()))return false;System=O;Spawn=O->GetSystemSpawnScript();Update=O->GetSystemUpdateScript();if(!CaptureScript(Spawn.Get())||!CaptureScript(Update.Get()))return false;
    for(UNiagaraScript* S:O->ScratchPadScripts){if(S&&!CaptureScript(S))return false;Scratch.Add(S);}
    if(!Budget(O->GetEmitterHandles().Num())||O->GetEmitterHandles().Num()>4096)return Fail(TEXT("niagara_event_stage_emitter_bound"),TEXT("Emitter inventory exceeds bound."));
    for(const auto& H:O->GetEmitterHandles()){FEmitterIdentity E;E.Id=H.GetId();E.Name=H.GetName();E.Enabled=H.GetIsEnabled();E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Emitter.IsValid()&&!Pin(E.Emitter.Get()))return false;if(E.Data){E.SimTarget=E.Data->SimTarget;if(E.Data->GetEventHandlers().Num()>4096||E.Data->SpawnScriptProps.EventGenerators.Num()>4096||E.Data->UpdateScriptProps.EventGenerators.Num()>4096||!Budget(E.Data->GetEventHandlers().Num()+E.Data->SpawnScriptProps.EventGenerators.Num()+E.Data->UpdateScriptProps.EventGenerators.Num()))return Fail(TEXT("niagara_event_stage_event_inventory_bound"),TEXT("Event inventory exceeds bounded projection."));for(const auto& Handler:E.Data->GetEventHandlers())if(Handler.Script&&!CaptureScript(Handler.Script))return false;E.EventKey=EventStateKey(E.Data);E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.ParentScratch=E.Data->ParentScratchPads;if(E.Source.IsValid()&&!Pin(E.Source.Get()))return false;
        auto CaptureScripts=[&](UNiagaraScratchPadContainer* C,TArray<TWeakObjectPtr<UNiagaraScript>>& A){if(!C)return true;if(!Pin(C)||!Budget(C->Scripts.Num()))return false;for(UNiagaraScript* S:C->Scripts){if(S&&!CaptureScript(S))return false;A.Add(S);}return true;};
        if(!CaptureScripts(E.Scratch.Get(),E.ScratchScripts)||!CaptureScripts(E.ParentScratch.Get(),E.ParentScripts))return false;
        TArray<UNiagaraScript*> OwnedScripts;E.Data->GetScripts(OwnedScripts,false,false);if(!Budget(OwnedScripts.Num()))return false;for(UNiagaraScript* P:OwnedScripts)if(!CaptureScript(P))return false;
        if(!Budget(E.Data->GetSimulationStages().Num()))return false;for(UNiagaraSimulationStageBase* S:E.Data->GetSimulationStages()){if(S&&(!Pin(S)||!CaptureScript(S->Script)))return false;if(S){const FString State=StageStateKey(S);if(State.Len()>1024*1024||!Budget(1,State.Len()))return Fail(TEXT("niagara_event_stage_stage_settings_bound"),TEXT("Stage public settings exceed bounded projection."));StageBindings.Add({S,S->Script,S->SimulationStageName,S->OuterEmitterVersion,State});}E.Stages.Add(S);}
    }Emitters.Add(MoveTemp(E));}return true;
}
UNiagaraSystem* LoadSystem(const FString& Path)
{
    if(!Current->SDK(TEXT("LoadObject_System")))return nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* O=LoadObject<UNiagaraSystem>(nullptr,*Expected);Current->Prefix->SetBoolField(TEXT("load_returned_nonnull"),O!=nullptr);if(!Current->After(TEXT("LoadObject_System")))return nullptr;if(O&&(!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Current->CaptureSystem(O))){Current->Fail(TEXT("niagara_event_stage_load_identity_unexpected"),TEXT("Loaded System differs from requested normalized exact path."));return nullptr;}return O;
}
bool FWork::CaptureGraph(UNiagaraScriptSource* S)
{
    if(!Admit(TEXT("capture_graph")))return false;if(!S||!S->NodeGraph)return true;if(!Pin(S)||!Pin(S->NodeGraph))return false;auto* G=S->NodeGraph.Get();
    bool Bound=false;for(const auto& E:Sources)if(E.Source.Get()==S){Bound=true;if(E.Graph.Get()!=G)return Fail(TEXT("niagara_event_stage_source_graph_changed"),TEXT("Source graph changed."));}
    if(!Bound)Sources.Add({S,G});for(const auto& E:Graphs)if(E.Graph.Get()==G)return true;
    if(!Budget(G->Nodes.Num())||G->Nodes.Num()>16384)return Fail(TEXT("niagara_event_stage_graph_bound"),TEXT("Graph node inventory exceeds bound."));
    if(G->GetAllMetaData().Num()>65536||!Budget(G->GetAllMetaData().Num()))return false;for(const auto& M:G->GetAllMetaData()){if(!Live(M.Value.Get())||!Pin(M.Value.Get()))return Fail(TEXT("niagara_event_stage_metadata_invalid"),TEXT("Live typed metadata required."));}
    FGraphIdentity E;E.Graph=G;E.Schema=G->Schema.Get();E.Meta=GraphMetadataSignature(G);
    for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N)||N->GetGraph()!=G)return Fail(TEXT("niagara_event_stage_node_owner_invalid"),TEXT("Null/foreign graph node cannot be safely inspected or mutated."));FNodeIdentity NE;NE.Node=N;NE.Id=N->NodeGuid;NE.X=N->NodePosX;NE.Y=N->NodePosY;if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()>65536||!Budget(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()))return Fail(TEXT("niagara_event_stage_signature_bound"),TEXT("Function signature inventory exceeds bound."));NE.Function=true;NE.FunctionScript=F->FunctionScript;NE.FunctionVersion=F->SelectedScriptVersion;NE.FunctionAsset=F->FunctionScriptAssetObjectPath;NE.FunctionSignature=F->Signature;NE.Specifiers=F->FunctionSpecifiers;if(F->FunctionScript&&!CaptureScript(F->FunctionScript))return false;}if(!Budget(N->Pins.Num()))return false;
        for(UEdGraphPin* P:N->Pins){if(!P)return Fail(TEXT("niagara_event_stage_null_pin"),TEXT("Null graph pin unsupported by Niagara public getters."));if(P->GetOwningNodeUnchecked()!=N||!Budget(P->LinkedTo.Num()))return Fail(TEXT("niagara_event_stage_pin_owner_invalid"),TEXT("Pin owner/link inventory is invalid."));FPinIdentity PE;PE.Pointer=P;PE.Id=P->PinId;PE.PersistentId=P->PersistentGuid;PE.NotConnectable=P->bNotConnectable;PE.DefaultReadOnly=P->bDefaultValueIsReadOnly;PE.Name=P->PinName;PE.Direction=P->Direction;PE.Type=P->PinType;PE.Links=P->LinkedTo;PE.DefaultValue=P->DefaultValue;PE.AutoDefault=P->AutogeneratedDefaultValue;PE.DefaultText=P->DefaultTextValue.ToString();PE.DefaultObject=P->DefaultObject;if(P->DefaultObject&&!Pin(P->DefaultObject))return false;if(P->PinType.PinSubCategoryObject.IsValid()&&!Pin(P->PinType.PinSubCategoryObject.Get()))return false;NE.Pins.Add(MoveTemp(PE));}
        E.Nodes.Add(MoveTemp(NE));}
    TSet<UEdGraphPin*> Known;for(const auto& N:E.Nodes)for(const auto& P:N.Pins)Known.Add(P.Pointer);for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links){if(!L||!Known.Contains(L))return Fail(TEXT("niagara_event_stage_unresolved_link_identity"),TEXT("Null/cross-graph link is outside captured exact pin membership; no stale link dereference."));}
    Graphs.Add(MoveTemp(E));return true;
}
bool FWork::GraphForWrite(UNiagaraGraph* G)
{
    if(!Admit(TEXT("graph_writer_admission"))||!Own(G))return false;bool Captured=false;
    for(const auto& S:Sources){if(S.Graph.Get()==G){if(!Own(S.Source.Get()))return false;Captured=true;}}
    TSet<UNiagaraScriptSource*> Sharing;int32 EmitterOwners=0;bool SystemOwner=false;auto HasGraph=[&](UNiagaraScriptSourceBase* Base){auto* Source=Cast<UNiagaraScriptSource>(Base);if(!Source)return false;if(!Live(Source)){Fail(TEXT("niagara_event_stage_shared_source_unresolved"),TEXT("Unloaded source prevents exact shared-graph admission."));return false;}if(Source->NodeGraph==G){Sharing.Add(Source);return true;}return false;};for(const auto& E:Emitters){if(HasGraph(E.Source.Get()))++EmitterOwners;}for(const auto& E:ScriptBindings){const bool Matches=HasGraph(E.Source.Get());if(Matches&&(E.Script.Get()==Spawn.Get()||E.Script.Get()==Update.Get()))SystemOwner=true;}if(!R.Error.IsEmpty())return false;if(Sharing.Num()>1||EmitterOwners>1||(EmitterOwners>0&&SystemOwner))return Fail(TEXT("niagara_event_stage_shared_graph_writer_unsupported"),TEXT("The selected graph is shared by multiple source/owner roles. Refused before first Modify; no duplicate or unselected owner mutation."));
    int32 ScratchRoles=0,RegularRoles=EmitterOwners+(SystemOwner?1:0);auto ScratchUses=[&](UNiagaraScript* P){auto* S=P?Cast<UNiagaraScriptSource>(P->GetLatestSource()):nullptr;return S&&S->NodeGraph==G;};for(const auto& P:Scratch)if(ScratchUses(P.Get()))++ScratchRoles;for(const auto& E:Emitters){for(const auto& P:E.ScratchScripts)if(ScratchUses(P.Get()))++ScratchRoles;for(const auto& P:E.ParentScripts)if(ScratchUses(P.Get()))++ScratchRoles;}if(ScratchRoles>1||(ScratchRoles>0&&RegularRoles>0))return Fail(TEXT("niagara_event_stage_shared_scratch_graph_unsupported"),TEXT("Mutable scratch graph is shared with another scratch or regular owner role."));
    if(!Captured)return Fail(TEXT("niagara_event_stage_uncaptured_graph"),TEXT("Mutable graph has no captured exact source binding."));
    for(const auto& E:Graphs){if(E.Graph.Get()==G){for(const auto& N:E.Nodes)if(!Own(N.Node.Get()))return false;}}
    return true;
}
FPinIdentity* FindExpectedPin(FWork& W,UEdGraphPin* P)
{for(auto& G:W.Graphs)for(auto& N:G.Nodes)for(auto& E:N.Pins)if(E.Pointer==P)return &E;return nullptr;}
bool FWork::PinForWrite(UEdGraphPin* P)
{
    if(!Admit(TEXT("pin_writer_admission"))||!P)return Fail(TEXT("niagara_event_stage_pin_unavailable"),TEXT("Pin unavailable."));
    auto* E=FindExpectedPin(*this,P);if(!E)return Fail(TEXT("niagara_event_stage_uncaptured_pin"),TEXT("Pin is not in captured exact node membership; stale pointer not dereferenced."));
    auto* N=P->GetOwningNodeUnchecked();auto* G=Cast<UNiagaraGraph>(N->GetGraph());if(!GraphForWrite(G)||!Own(N))return false;
    for(auto* L:E->Links){if(!L||!FindExpectedPin(*this,L))return Fail(TEXT("niagara_event_stage_external_pin_link_unsupported"),TEXT("Linked pin is outside captured mutable graph(s)."));auto* LN=L->GetOwningNodeUnchecked();if(LN->GetGraph()!=G||!Own(LN))return Fail(TEXT("niagara_event_stage_external_pin_link_unsupported"),TEXT("Cross-graph linked writer is unsupported before mutation."));}
    return true;
}
bool FWork::BreakLinks(UEdGraphPin* P,bool Notify)
{
    if(!PinForWrite(P)||!SDK(TEXT("BreakAllPinLinks"),true))return false;auto* E=FindExpectedPin(*this,P);const TArray<UEdGraphPin*> Links=E->Links;
    for(auto* L:Links){auto* Other=FindExpectedPin(*this,L);if(!Other)return Fail(TEXT("niagara_event_stage_link_plan_unavailable"),TEXT("Captured reciprocal pin unavailable."));Other->Links.Remove(P);}E->Links.Empty();
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
    FGraphIdentity* Expected=Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return Fail(TEXT("niagara_event_stage_missing_graph_plan"),TEXT("Captured graph plan unavailable."));
    const FNodeIdentity* NE=Expected->Nodes.FindByPredicate([&](const auto& E){return E.Node.Get()==N;});if(!NE)return Fail(TEXT("niagara_event_stage_node_already_removed"),TEXT("Exact captured node no longer belongs to graph."));TArray<UEdGraphPin*> Removing;for(const auto& P:NE->Pins)Removing.Add(P.Pointer);
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
    if(Direct&&(!FPackageName::IsValidLongPackageName(P->GetName(),false)||!FPackageName::TryConvertLongPackageNameToFilename(P->GetName(),Path,FPackageName::GetAssetPackageExtension())))return Fail(TEXT("niagara_event_stage_save_path_invalid"),TEXT("Exact package filename unavailable."));
    if(!SDK(Direct?TEXT("SavePackage"):TEXT("SaveLoadedAsset"),true))return false;R.bSaveAttempted=true;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_package"),P->GetName());Row->SetStringField(TEXT("requested_asset"),O->GetPathName());bool Saved=false;
    if(Direct){FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;Saved=UPackage::SavePackage(P,O,*Path,Args);}else Saved=UEditorAssetLibrary::SaveLoadedAsset(O,false);
    R.bSaved|=Saved;SaveHealth&=Saved;if(Saved&&Direct&&Filename)*Filename=Path;Row->SetStringField(TEXT("requested_filename"),Path);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Saved);Row->SetBoolField(TEXT("actual_saved_history"),R.bSaved);Saves.Add(MakeShared<FJsonValueObject>(Row));if(!After(Direct?TEXT("SavePackage"):TEXT("SaveLoadedAsset"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}
    const bool DirtyAfter=P->IsDirty();Row->SetStringField(TEXT("package_after"),P->GetName());Row->SetBoolField(TEXT("dirty_after"),DirtyAfter);if(Saved&&DirtyAfter){SaveHealth=false;Fail(TEXT("niagara_event_stage_saved_package_dirty"),TEXT("Save returned true but captured package is dirty after SDK callbacks. Save history retained; dirty flag was not cleared or retried."));return false;}return Saved;
}
bool FWork::WaitAdmission(bool Wait,bool WillRequest)
{
    if(!Wait)return true;if(!SDK(TEXT("compile_wait_admission")))return false;bool Pending=System->HasOutstandingCompilationRequests(true)||System->HasActiveCompilations();if(!After(TEXT("compile_wait_admission")))return false;
    if(DeferredMode)return true;
    if(WillRequest||Pending)return Fail(TEXT("niagara_event_stage_async_wait_unsupported"),TEXT("Actual requested/pending compile wait needs a deferred final result, exact lifetime and original save/absence continuation. This synchronous implementation rejects before writer; no GT sleep/flush, fake timeout or wait=False substitution."));return true;
}
TSharedPtr<FJsonObject> FWork::WaitSnapshot(bool Wait,double Timeout,double Poll)
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0.0);if(!System.IsValid()){O->SetBoolField(TEXT("outstanding_compilation_requests_after"),false);O->SetBoolField(TEXT("active_compilations_after"),false);return O;}
    if(!SDK(TEXT("compile_status_snapshot")))return O;const bool Pending=System->HasOutstandingCompilationRequests(true),Active=System->HasActiveCompilations();O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Pending);O->SetBoolField(TEXT("active_compilations_after"),Active);if(!After(TEXT("compile_status_snapshot")))return O;if(Wait&&(Pending||Active)){Fail(TEXT("niagara_event_stage_async_wait_unsupported"),TEXT("Compile became pending after actual mutation/save; partial history retained. No blocking or replay."));O->SetBoolField(TEXT("actual_wait_implemented"),false);}return O;
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



bool FunctionFieldsExact(const FNodeIdentity& E,UEdGraphNode* N);
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
FString GraphMetadataSignature(UNiagaraGraph* G)
{
 TArray<FString> Rows;for(const auto& P:G->GetAllMetaData()){const auto* V=P.Value.Get();if(!Live(V))return TEXT("invalid_metadata");const int32 N=V->GetDefaultValueVariant().GetNumBytes();if(N<0||N>65536)return TEXT("metadata_bytes_bound");FString DefaultHash;if(V->GetDefaultValueData()&&N)DefaultHash=FMD5::HashBytes(V->GetDefaultValueData(),N);Rows.Add(P.Key.GetName().ToString()+TEXT("|")+TypeIdentity(P.Key.GetType())+TEXT("|")+V->GetPathName()+TEXT("|")+V->Metadata.GetVariableGuid().ToString()+TEXT("|")+V->GetChangeId().ToString()+TEXT("|")+FString::FromInt(static_cast<int32>(V->DefaultMode))+TEXT("|")+V->DefaultBinding.GetName().ToString()+TEXT("|")+DefaultHash);}Rows.Sort([](const FString& A,const FString& B){return A.Compare(B,ESearchCase::CaseSensitive)<0;});return FString::Join(Rows,TEXT("\n"));
}

FString FunctionNameForPath(const UNiagaraNodeFunctionCall* N)
{
    FString Out;if(!N||!Current->Step(TEXT("GetFunctionName"),false,[&]{Out=N->GetFunctionName();}))return {};
    Current->Budget(0,Out.Len());return Out;
}
FNiagaraTypeDefinition PinTypeForPath(UEdGraphPin* P)
{
    FNiagaraTypeDefinition Out;
    if(!ContinueRead()||!FindExpectedPin(*Current,P)){Current->Fail(TEXT("niagara_event_stage_pin_unresolved"),TEXT("Pin is outside captured exact graph membership."));return Out;}
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
    if(!W.Budget(Variables.Num())||Variables.Num()>16384){W.Fail(TEXT("niagara_event_stage_store_bound"),TEXT("Exposed parameter inventory exceeds bound."));return Out;}
    for(const auto& V:Variables)
    {
        FUserValue E;E.Variable=V;if(!Store.RedirectUserVariable(E.Variable)){W.Fail(TEXT("niagara_event_stage_user_alias_unresolved"),TEXT("Captured User alias could not be resolved to the exact full-name variable."));return Out;}if(V.GetType().IsDataInterface()){auto* DI=Store.GetDataInterface(V);E.DI=DI;if(DI&&!W.Pin(DI))return Out;}
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
    if(FullParameterCount<0||FullParameterCount>65536||!W.Budget(FullParameterCount))return W.Fail(TEXT("niagara_event_stage_full_user_store_bound"),TEXT("Full exposed declaration inventory exceeds the bounded canonical User check."));
    TArray<FNiagaraVariable> FullParametersForUser;CanonicalStore.GetParameters(FullParametersForUser);
    int32 ExactFullMatches=0;for(const FNiagaraVariable& FullParameter:FullParametersForUser)if(FullParameter.GetName()==User.GetName()&&FullParameter.GetType()==User.GetType())++ExactFullMatches;
    FNiagaraVariable CanonicalProbe=User;const bool CanonicalRedirected=CanonicalStore.RedirectUserVariable(CanonicalProbe);
    W.Prefix->SetStringField(TEXT("last_user_full_name_checked"),User.GetName().ToString());
    W.Prefix->SetStringField(TEXT("last_user_full_name_redirect"),CanonicalProbe.GetName().ToString());
    if(ExactFullMatches!=1||!CanonicalRedirected||CanonicalProbe.GetName()!=User.GetName()||CanonicalProbe.GetType()!=User.GetType())return W.Fail(TEXT("niagara_event_stage_user_canonical_redirect_unsupported"),TEXT("Selected full typed User declaration is absent, ambiguous, or redirects to another declaration; no next store or graph writer dispatched."));
    return true;
}
bool VerifyUnrelatedUserValues(FWork& W,const TArray<FUserValue>& Before,const FNiagaraVariable& Selected,bool AllowNew)
{
    if(!VerifyCanonicalUserForPath(W,Selected))return false;
    auto After=CaptureUserValues(W);if(!ContinueRead())return false;
    int32 OtherBefore=0,OtherAfter=0;
    auto SelectedMatch=[&](const FUserValue& E){return E.Variable.GetName()==Selected.GetName()&&E.Variable.GetType()==Selected.GetType();};
    for(const auto& E:Before){if(SelectedMatch(E))continue;++OtherBefore;const auto* A=After.FindByPredicate([&](const auto& V){return V.Variable.GetName()==E.Variable.GetName()&&V.Variable.GetType()==E.Variable.GetType();});if(!A||!SameUserValue(E,*A))return W.Fail(TEXT("niagara_event_stage_unselected_store_changed"),TEXT("Unselected exposed parameter changed across SDK call."));}
    for(const auto& E:After)if(!SelectedMatch(E))++OtherAfter;
    const int32 SelectedCount=After.FilterByPredicate(SelectedMatch).Num();
    return (OtherBefore==OtherAfter&&SelectedCount==1&&(AllowNew||Before.ContainsByPredicate(SelectedMatch)))||W.Fail(TEXT("niagara_event_stage_store_shape_changed"),TEXT("Exposed parameter set differs from the admitted selected transition."));
}
bool PreflightStackMap(FWork& W,UNiagaraNodeFunctionCall* Function)
{
    if(!ContinueRead())return false;auto* G=Cast<UNiagaraGraph>(Function->GetGraph());if(!G||!W.GraphForWrite(G)||!W.Own(Function))return false;
    TArray<UEdGraphPin*> Inputs;for(auto* P:Function->Pins)if(P->Direction==EGPD_Input&&PinTypeForPath(P)==FNiagaraTypeDefinition::GetParameterMapDef())Inputs.Add(P);
    if(!ContinueRead())return false;
    if(Inputs.Num()!=1||Inputs[0]->LinkedTo.Num()!=1||!FindExpectedPin(W,Inputs[0]->LinkedTo[0]))return W.Fail(TEXT("niagara_event_stage_stack_map_shape_unsupported"),TEXT("The module must have one exact captured ParameterMap input and one upstream link before public stack SDK."));
    return true;
}
UEdGraphPin* MapPin(UEdGraphNode* N,EEdGraphPinDirection Direction)
{
    UEdGraphPin* Found=nullptr;for(auto* P:N->Pins)if(P&&P->Direction==Direction&&UEdGraphSchema_Niagara::PinToTypeDefinition(P)==FNiagaraTypeDefinition::GetParameterMapDef()){if(Found)return nullptr;Found=P;}return Found;
}
bool PinHeaderSame(const FPinIdentity& E,const UEdGraphPin* P)
{
    return P&&P->PinId==E.Id&&P->PersistentGuid==E.PersistentId&&P->bNotConnectable==E.NotConnectable&&P->bDefaultValueIsReadOnly==E.DefaultReadOnly&&P->PinName.ToString().Equals(E.Name.ToString(),ESearchCase::CaseSensitive)&&P->Direction==E.Direction&&P->PinType==E.Type&&P->DefaultValue.Equals(E.DefaultValue,ESearchCase::CaseSensitive)&&P->AutogeneratedDefaultValue.Equals(E.AutoDefault,ESearchCase::CaseSensitive)&&P->DefaultTextValue.ToString().Equals(E.DefaultText,ESearchCase::CaseSensitive)&&P->DefaultObject==E.DefaultObject.Get();
}
bool CaptureTransitionResult(FWork& W,UNiagaraGraph* G,const FGraphIdentity& Before,UNiagaraNodeFunctionCall* Function,const FNiagaraParameterHandle& Alias,const FNiagaraTypeDefinition& Type,UEdGraphPin* Returned,UEdGraphPin* ExistingOverride,const FNiagaraVariableBase* LinkedUser,bool WasOverride,const FString& OtherMetadataBefore)
{
    // The public stack call has one admitted mutable graph. Retained graph/node/pin
    // identity is validated before adopting only the exact requested append/link plan.
    if(!W.Admit(TEXT("stack_transition_context")))return false;
    if(!GraphMetadataWithout(G,LinkedUser).Equals(OtherMetadataBefore,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_event_stage_unselected_metadata_changed"),TEXT("Unselected graph metadata changed during the requested stack transition."));
    if(G->Schema!=Before.Schema.Get()||G->Nodes.Num()<Before.Nodes.Num()||G->Nodes.Num()>Before.Nodes.Num()+1)return W.Fail(TEXT("niagara_event_stage_unexpected_node_delta"),TEXT("Public stack operation changed an unexpected number of nodes."));
    for(int32 I=0;I<Before.Nodes.Num();++I)if(G->Nodes[I]!=Before.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_event_stage_retained_node_changed"),TEXT("Retained node order/identity changed during stack SDK."));
    UEdGraphNode* NewNode=G->Nodes.Num()>Before.Nodes.Num()?G->Nodes.Last().Get():nullptr;
    if(NewNode&&(!Live(NewNode)||!NewNode->IsIn(W.System.Get())||NewNode->GetOutermost()!=W.System->GetOutermost()||NewNode->GetGraph()!=G))return W.Fail(TEXT("niagara_event_stage_generated_node_owner_invalid"),TEXT("New stack node is outside the requested graph/System."));
    const TCHAR* NewClass=WasOverride?TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"):TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet");
    if(NewNode&&!NewNode->GetClass()->GetPathName().Equals(NewClass,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_event_stage_generated_node_class_invalid"),TEXT("Public stack SDK created an unexpected node class."));
    if(!WasOverride&&!NewNode)return W.Fail(TEXT("niagara_event_stage_link_node_missing"),TEXT("User binding did not append its exact getter node."));
    TSet<UEdGraphPin*> LivePins;
    for(UEdGraphNode* N:G->Nodes){if(!Live(N)||!W.Pin(N))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_event_stage_generated_pin_invalid"),TEXT("Post-SDK pin owner is invalid."));LivePins.Add(P);}}
    if(Returned&&!LivePins.Contains(Returned))return W.Fail(TEXT("niagara_event_stage_returned_pin_expired"),TEXT("Returned override pin was removed during SDK callback; stale pointer not read."));
    if(ExistingOverride&&!LivePins.Contains(ExistingOverride))return W.Fail(TEXT("niagara_event_stage_existing_override_expired"),TEXT("Captured existing override pin disappeared during stack SDK; stale pointer not dereferenced."));
    UEdGraphPin* FunctionInput=Function?MapPin(Function,EGPD_Input):nullptr;if(Function&&(!FunctionInput||FunctionInput->LinkedTo.Num()!=1))return W.Fail(TEXT("niagara_event_stage_stack_map_changed"),TEXT("Module map input no longer has exact upstream."));
    UEdGraphNode* OverrideNode=WasOverride?(Returned?Returned->GetOwningNodeUnchecked():nullptr):ExistingOverride->GetOwningNodeUnchecked();
    if(!OverrideNode||!OverrideNode->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_event_stage_override_owner_invalid"),TEXT("Exact override owner is not the public reflected ParameterMapSet class."));
    UEdGraphPin* OverrideInput=MapPin(OverrideNode,EGPD_Input);UEdGraphPin* OverrideOutput=MapPin(OverrideNode,EGPD_Output);
    if(!OverrideInput||!OverrideOutput||OverrideInput->LinkedTo.Num()!=1)return W.Fail(TEXT("niagara_event_stage_override_stack_invalid"),TEXT("Override ParameterMap input must have one captured upstream link."));
    UEdGraphPin* Prior=OverrideInput->LinkedTo[0];if(!LivePins.Contains(Prior))return W.Fail(TEXT("niagara_event_stage_upstream_unresolved"),TEXT("Override upstream is outside current exact graph pins."));
    UEdGraphPin* NewGetInput=nullptr;UEdGraphPin* NewGetOutput=nullptr;
    if(!WasOverride)
    {
        NewGetInput=MapPin(NewNode,EGPD_Input);int32 Semantic=0;
        for(auto* P:NewNode->Pins)if(P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P)){++Semantic;NewGetOutput=P;}
        if(Semantic!=1||!NewGetInput||!NewGetOutput||!LinkedUser||NewGetOutput->PinName!=LinkedUser->GetName()||UEdGraphSchema_Niagara::PinToTypeDefinition(NewGetOutput)!=LinkedUser->GetType()||NewGetInput->LinkedTo!=TArray<UEdGraphPin*>{Prior}||NewGetOutput->LinkedTo!=TArray<UEdGraphPin*>{ExistingOverride})return W.Fail(TEXT("niagara_event_stage_user_getter_shape_invalid"),TEXT("Generated getter does not match the exact requested typed User link."));
    }
    for(const auto& NE:Before.Nodes)
    {
        auto* N=NE.Node.Get();if(N->NodeGuid!=NE.Id||N->NodePosX!=NE.X||N->NodePosY!=NE.Y||N->GetGraph()!=G||!FunctionFieldsExact(NE,N))return W.Fail(TEXT("niagara_event_stage_retained_node_fields_changed"),TEXT("Retained node GUID/position/graph changed during stack SDK."));int32 AddedPins=N->Pins.Num()-NE.Pins.Num();const bool IsOldOverride=WasOverride&&N==OverrideNode;
        if(AddedPins!=(IsOldOverride&&!ExistingOverride?1:0))return W.Fail(TEXT("niagara_event_stage_unexpected_pin_delta"),TEXT("Retained node pin count differs from requested override insertion."));
        TArray<UEdGraphPin*> Remaining;for(auto* P:N->Pins)if(!(IsOldOverride&&!ExistingOverride&&P==Returned))Remaining.Add(P);
        if(Remaining.Num()!=NE.Pins.Num())return W.Fail(TEXT("niagara_event_stage_pin_plan_invalid"),TEXT("Override insertion did not yield exact retained pin set."));
        for(int32 J=0;J<NE.Pins.Num();++J)
        {
            const auto& PE=NE.Pins[J];auto* P=Remaining[J];if(P!=PE.Pointer||!PinHeaderSame(PE,P))return W.Fail(TEXT("niagara_event_stage_retained_pin_changed"),TEXT("Retained exact pin/type/name changed during SDK."));
            TArray<UEdGraphPin*> Expected=PE.Links;
            if(WasOverride&&NewNode)
            {
                UEdGraphPin* OldFunctionInput=FunctionInput;
                const FPinIdentity* OldInput=FindExpectedPin(W,OldFunctionInput);
                if(!OldInput||OldInput->Links.Num()!=1)return W.Fail(TEXT("niagara_event_stage_original_map_unresolved"),TEXT("Original module upstream no longer known."));
                UEdGraphPin* OldPrior=OldInput->Links[0];const auto* OldPriorExpected=FindExpectedPin(W,OldPrior);
                if(!OldPriorExpected)return false;
                if(P==OldPrior)Expected={OverrideInput};
                else if(P==OldFunctionInput){Expected.Remove(OldPrior);Expected.AddUnique(OverrideOutput);}
                else if(OldPriorExpected->Links.Contains(P)){Expected.Remove(OldPrior);Expected.AddUnique(OverrideOutput);}
            }
            if(!WasOverride){if(P==ExistingOverride)Expected={NewGetOutput};else if(P==Prior)Expected.AddUnique(NewGetInput);}
            if(P->LinkedTo!=Expected)return W.Fail(TEXT("niagara_event_stage_unselected_links_changed"),TEXT("Retained links differ from the single requested public stack transition."));
        }
    }
    if(WasOverride&&Returned&&(Returned->PinName!=Alias.GetParameterHandleString()||Returned->Direction!=EGPD_Input||UEdGraphSchema_Niagara::PinToTypeDefinition(Returned)!=Type||(!ExistingOverride&&Returned->LinkedTo.Num()!=0)||(ExistingOverride&&Returned!=ExistingOverride)))return W.Fail(TEXT("niagara_event_stage_override_pin_shape_invalid"),TEXT("Override pin is not exact typed empty requested alias."));
    if(WasOverride&&NewNode){for(auto* P:NewNode->Pins)if(P!=OverrideInput&&P!=OverrideOutput&&P!=Returned&&!IsNiagaraDynamicAddPin(P))return W.Fail(TEXT("niagara_event_stage_override_extra_pin"),TEXT("Generated override contains an unexpected semantic pin."));}
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
    if(!W.PinForWrite(Override)||Override->LinkedTo.Num()!=0||!PreflightStackMap(W,Function))return W.Fail(TEXT("niagara_event_stage_override_not_empty"),TEXT("Exact override must be empty before User link SDK."));
    auto* G=Cast<UNiagaraGraph>(Function->GetGraph());auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return false;const FGraphIdentity Before=*Expected;const FString MetadataBefore=GraphMetadataWithout(G,&User);
    UNiagaraScriptVariable* ExistingMetadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&!W.Own(ExistingMetadata))return false;
    if(!W.SDK(TEXT("SetLinkedParameterValueForFunctionInput"),true))return false;auto Row=W.MutationRow(TEXT("SetLinkedParameterValueForFunctionInput"));Row->SetStringField(TEXT("module_guid"),Function->NodeGuid.ToString());Row->SetStringField(TEXT("user_parameter"),User.GetName().ToString());
    {TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);TSet<FNiagaraVariableBase> Known;Known.Add(User);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Override,User,Known);Row->SetBoolField(TEXT("SDK_returned"),true);if(!CaptureTransitionResult(W,G,Before,Function,FNiagaraParameterHandle(),User.GetType(),nullptr,Override,&User,false,MetadataBefore))return false;}
    UNiagaraScriptVariable* Metadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&Metadata!=ExistingMetadata)return W.Fail(TEXT("niagara_event_stage_metadata_identity_changed"),TEXT("Selected existing metadata was replaced during binding SDK."));if(!Metadata||!W.Own(Metadata)||Metadata->DefaultMode!=ENiagaraDefaultMode::FailIfPreviouslyNotSet)return W.Fail(TEXT("niagara_event_stage_binding_metadata_readback_failed"),TEXT("Exact selected User metadata was not observed after public binding."));
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

FString GetNiagaraCustomHlslString(const UNiagaraNodeCustomHlsl* Node)
{
 if(!Current||!Current->Admit(TEXT("HLSL_property_read_preflight"))||!Live(Node)||Node->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass())return FString();FProperty* P=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));if(!CastField<FStrProperty>(P)||P->ArrayDim!=1||EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(P),EPropertyAccessResultFlags::PermissionDenied)){Current->Fail(TEXT("niagara_event_stage_HLSL_property_denied"),TEXT("Public CustomHlsl property is not readable."));return FString();}FString Encoded;if(!Current->Step(TEXT("GetObjectProperties_CustomHlsl"),false,[&]{Encoded=UToolsetLibrary::GetObjectProperties(Node,{FName(TEXT("CustomHlsl"))});}))return FString();TSharedPtr<FJsonObject> O;FString Body;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Encoded),O)||!O||!O->TryGetStringField(TEXT("CustomHlsl"),Body)){Current->Fail(TEXT("niagara_event_stage_HLSL_read_failed"),TEXT("Public HLSL property readback unavailable."));return FString();}if(!Current->Budget(0,Body.Len()))return FString();return Body;
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

UEdGraphPin* FindNiagaraStackFunctionInputOverridePin(const UNiagaraNodeFunctionCall* FunctionCall,const FNiagaraParameterHandle& Alias)
{
 if(!FunctionCall||!ContinueRead())return nullptr;UEdGraphPin* ModuleMap=nullptr;for(auto* P:FunctionCall->Pins){if(P->Direction==EGPD_Input&&PinTypeForPath(P)==FNiagaraTypeDefinition::GetParameterMapDef()){if(ModuleMap)return nullptr;ModuleMap=P;}}if(!ContinueRead()||!ModuleMap||ModuleMap->LinkedTo.Num()!=1)return nullptr;auto* Upstream=ModuleMap->LinkedTo[0];if(!FindExpectedPin(*Current,Upstream))return nullptr;auto* Node=Upstream->GetOwningNodeUnchecked();if(Node->GetGraph()!=FunctionCall->GetGraph()||!Node->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive))return nullptr;for(auto* P:Node->Pins)if(P->Direction==EGPD_Input&&P->PinName==Alias.GetParameterHandleString())return P;return nullptr;
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





bool AddUserForPath(FWork& W,const FNiagaraVariable& V)
{
    if(!W.SDK(TEXT("UserStore_AddParameter"),true))return false;
    auto Row=W.MutationRow(TEXT("UserStore_AddParameter"));Row->SetStringField(TEXT("parameter_name"),V.GetName().ToString());Row->SetStringField(TEXT("type_identity"),TypeIdentity(V.GetType()));
    const bool Added=W.System->GetExposedParameters().AddParameter(V,true,true);
    Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_added"),Added);W.Prefix->SetBoolField(TEXT("user_parameter_add_returned"),true);W.Prefix->SetBoolField(TEXT("user_parameter_add_success"),Added);W.R.bChanged|=Added;
    if(!W.After(TEXT("UserStore_AddParameter")))return false;return Added;
}


bool SetDefaultForPath(FWork& W,FNiagaraUserRedirectionParameterStore& Store,const FNiagaraVariable& V,const TSharedPtr<FJsonValue>& Value,FString& Error)
{
    if(!VerifyCanonicalUserForPath(W,V))return false;
    bool Set=false;if(!W.Step(TEXT("UserStore_SetDefaultValue"),true,[&]{Set=SetNiagaraUserParameterValue(Store,V,Value,Error);W.Prefix->SetBoolField(TEXT("default_value_set_returned"),Set);}))return false;return Set;
}
bool SafeDefaultNumbers(const FNiagaraTypeDefinition& Type,const TSharedPtr<FJsonValue>& Value)
{
    if(!Value)return true;
    auto FloatSafe=[](double X){return FMath::IsFinite(X)&&X>=-static_cast<double>(MAX_flt)&&X<=static_cast<double>(MAX_flt);};
    if(Type==FNiagaraTypeDefinition::GetIntDef()){double X=0;if(Value->TryGetNumber(X)&&(!FMath::IsFinite(X)||X<MIN_int32||X>MAX_int32))return Current->Fail(TEXT("niagara_event_stage_numeric_range_unsupported"),TEXT("Numeric default is outside the SDK int32 conversion range."));}
    else if(Type==FNiagaraTypeDefinition::GetFloatDef()){double X=0;if(Value->TryGetNumber(X)&&!FloatSafe(X))return Current->Fail(TEXT("niagara_event_stage_numeric_range_unsupported"),TEXT("Numeric default is outside finite float range."));}
    else if(Value->Type==EJson::Array&&Type!=FNiagaraTypeDefinition::GetPositionDef()){for(const auto& V:Value->AsArray()){double X=0;if(V&&V->TryGetNumber(X)&&!FloatSafe(X))return Current->Fail(TEXT("niagara_event_stage_numeric_range_unsupported"),TEXT("Vector/color default is outside finite float range."));}}
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



TSharedPtr<FJsonObject> FWork::Projection()
{
    auto O=MakeShared<FJsonObject>();
    auto Store=CaptureUserValues(*this);if(!ContinueRead())return O;O->SetBoolField(TEXT("source_projection_complete"),false);if(!Admit(TEXT("target_projection")))return O;
    TArray<TSharedPtr<FJsonValue>> ObjectRows,GraphRows;
    for(const auto& I:Objects){if(!Budget(1))return O;auto* P=I->Object.Get();auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),P->GetPathName());V->SetStringField(TEXT("class"),P->GetClass()->GetPathName());V->SetStringField(TEXT("outer"),P->GetOuter()?P->GetOuter()->GetPathName():FString());V->SetStringField(TEXT("package"),P->GetOutermost()->GetName());V->SetBoolField(TEXT("dirty"),P->GetOutermost()->IsDirty());V->SetBoolField(TEXT("retired_script_subtree"),I->Retired);ObjectRows.Add(MakeShared<FJsonValueObject>(V));}
    for(const auto& GE:Graphs){auto* G=GE.Graph.Get();auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("path"),G->GetPathName());V->SetStringField(TEXT("typed_metadata"),GraphMetadataSignature(G));TArray<TSharedPtr<FJsonValue>> Nodes;
        for(UEdGraphNode* N:G->Nodes){if(!Budget(1))return O;auto NV=MakeShared<FJsonObject>();NV->SetStringField(TEXT("guid"),N->NodeGuid.ToString());NV->SetStringField(TEXT("path"),N->GetPathName());if(auto* H=Cast<UNiagaraNodeCustomHlsl>(N)){const FString Body=GetNiagaraCustomHlslString(H);if(!ContinueRead()||!Budget(0,Body.Len()))return O;NV->SetStringField(TEXT("custom_hlsl_body"),Body);}TArray<TSharedPtr<FJsonValue>> Pins;
            for(auto* P:N->Pins){if(!Budget(1,P->DefaultValue.Len()))return O;auto PV=MakeShared<FJsonObject>();PV->SetStringField(TEXT("id"),P->PinId.ToString());PV->SetStringField(TEXT("persistent_guid"),P->PersistentGuid.ToString());PV->SetBoolField(TEXT("not_connectable"),P->bNotConnectable);PV->SetBoolField(TEXT("default_readonly"),P->bDefaultValueIsReadOnly);PV->SetStringField(TEXT("name"),P->PinName.ToString());PV->SetNumberField(TEXT("direction"),P->Direction);PV->SetStringField(TEXT("category"),P->PinType.PinCategory.ToString());PV->SetStringField(TEXT("subcategory"),P->PinType.PinSubCategory.ToString());PV->SetStringField(TEXT("default_value"),P->DefaultValue);PV->SetStringField(TEXT("autogenerated_default"),P->AutogeneratedDefaultValue);PV->SetStringField(TEXT("default_text"),P->DefaultTextValue.ToString());PV->SetStringField(TEXT("default_object"),P->DefaultObject?P->DefaultObject->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> Links;for(auto* L:P->LinkedTo){if(!Budget(1))return O;auto LV=MakeShared<FJsonObject>();LV->SetStringField(TEXT("owner"),L->GetOwningNodeUnchecked()->NodeGuid.ToString());LV->SetStringField(TEXT("pin"),L->PinId.ToString());Links.Add(MakeShared<FJsonValueObject>(LV));}PV->SetArrayField(TEXT("links"),Links);Pins.Add(MakeShared<FJsonValueObject>(PV));}
            NV->SetArrayField(TEXT("pins"),Pins);Nodes.Add(MakeShared<FJsonValueObject>(NV));}
        V->SetArrayField(TEXT("nodes"),Nodes);GraphRows.Add(MakeShared<FJsonValueObject>(V));}
    O->SetArrayField(TEXT("objects"),ObjectRows);O->SetArrayField(TEXT("graphs"),GraphRows);
    if(System.IsValid()){auto* S=System.Get();O->SetStringField(TEXT("system_path"),S->GetPathName());TArray<TSharedPtr<FJsonValue>> SystemScratch,EmitterRows;for(UNiagaraScript* P:S->ScratchPadScripts)SystemScratch.Add(MakeShared<FJsonValueString>(P?P->GetPathName():FString()));
        for(const auto& H:S->GetEmitterHandles()){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("id"),H.GetId().ToString());V->SetStringField(TEXT("version"),H.GetInstance().Version.ToString());TArray<TSharedPtr<FJsonValue>> Stages,Scripts;auto* Data=H.GetEmitterData();if(Data){for(UNiagaraSimulationStageBase* P:Data->GetSimulationStages())Stages.Add(MakeShared<FJsonValueString>(P?P->GetPathName():FString()));if(Data->ScratchPads){for(UNiagaraScript* P:Data->ScratchPads->Scripts)Scripts.Add(MakeShared<FJsonValueString>(P?P->GetPathName():FString()));}}V->SetArrayField(TEXT("stages"),Stages);V->SetArrayField(TEXT("scratch"),Scripts);EmitterRows.Add(MakeShared<FJsonValueObject>(V));}
        O->SetArrayField(TEXT("system_scratch"),SystemScratch);O->SetArrayField(TEXT("emitters"),EmitterRows);}
    TArray<TSharedPtr<FJsonValue>> UserRows;for(const auto& E:Store){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),E.Variable.GetName().ToString());V->SetStringField(TEXT("type_identity"),TypeIdentity(E.Variable.GetType()));V->SetNumberField(TEXT("byte_count"),E.Bytes.Num());V->SetStringField(TEXT("bytes_md5"),E.Bytes.IsEmpty()?FString():FMD5::HashBytes(E.Bytes.GetData(),E.Bytes.Num()));V->SetStringField(TEXT("DI_path"),E.DI.IsValid()?E.DI->GetPathName():FString());V->SetStringField(TEXT("object_path"),E.Object.IsValid()?E.Object->GetPathName():FString());V->SetBoolField(TEXT("has_position"),E.HasPosition);if(E.HasPosition){V->SetNumberField(TEXT("position_x"),E.Position.X);V->SetNumberField(TEXT("position_y"),E.Position.Y);V->SetNumberField(TEXT("position_z"),E.Position.Z);}UserRows.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("exposed_user_parameters"),UserRows);
    O->SetBoolField(TEXT("source_projection_complete"),true);return O;
}

int32 IndexParam(const TSharedPtr<FJsonObject>& P,const TCHAR* K,int32 Default=INDEX_NONE){double N=Default;if(!P->TryGetNumberField(K,N))return Default;if(!FMath::IsFinite(N)||N<MIN_int32||N>MAX_int32){Current->Fail(TEXT("niagara_event_stage_index_invalid"),FString(K)+TEXT(" requires a safe finite int32 conversion."));return Default;}int32 Out=Default;P->TryGetNumberField(K,Out);return Out;}
int32 IndexParam(FWork&,const TSharedPtr<FJsonObject>& P,const TCHAR* K,int32 Default=INDEX_NONE){return IndexParam(P,K,Default);}



bool FunctionFieldsExact(const FNodeIdentity& E,UEdGraphNode* N)
{if(!E.Function)return true;auto* F=Cast<UNiagaraNodeFunctionCall>(N);return F&&F->FunctionScript==E.FunctionScript.Get()&&F->SelectedScriptVersion==E.FunctionVersion&&F->FunctionScriptAssetObjectPath==E.FunctionAsset&&F->FunctionSpecifiers.OrderIndependentCompareEqual(E.Specifiers)&&FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&F->Signature,&E.FunctionSignature,0);}
bool ResolveUser(FWork& W,const FString& Requested,const FNiagaraTypeDefinition& Type,FNiagaraVariable& User,bool& Existed)
{
 if(!W.Admit(TEXT("resolve_user_parameter")))return false;User=FNiagaraVariable(Type,FName(*Requested));Existed=false;const auto& Store=W.System->GetExposedParameters();TArray<FNiagaraVariable> Full;Store.GetParameters(Full);if(Full.Num()>65536||!W.Budget(Full.Num()))return W.Fail(TEXT("niagara_event_stage_user_store_bound"),TEXT("Full typed declaration inventory exceeds bound."));int32 Count=0;for(const auto& V:Full){if(V.GetName()!=User.GetName())continue;if(V.GetType()!=Type)return W.Fail(TEXT("niagara_event_stage_user_type_conflict"),TEXT("Requested full User name has an existing incompatible type."));++Count;}
 FNiagaraVariable Probe=User;const bool Redirected=Store.RedirectUserVariable(Probe);if(Redirected&&(Probe.GetName()!=User.GetName()||Probe.GetType()!=User.GetType()))return W.Fail(TEXT("niagara_event_stage_user_alias_collision"),TEXT("Requested normalized full typed User name redirects to another declaration; no store/graph writer."));if(Count>1)return W.Fail(TEXT("niagara_event_stage_user_declaration_ambiguous"),TEXT("Requested full typed declaration appears more than once."));Existed=Count==1;if(Existed)return VerifyCanonicalUserForPath(W,User);return true;
}
bool PreflightMapValue(FWork& W,UEdGraphPin* P)
{
 if(!W.PinForWrite(P))return false;auto* N=P->GetOwningNodeUnchecked();if(!N->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive)||P->Direction!=EGPD_Input||IsNiagaraDynamicAddPin(P))return W.Fail(TEXT("niagara_event_stage_map_owner_unsupported"),TEXT("Value target must be a captured exact ParameterMapSet input."));auto* Source=MapPin(N,EGPD_Input);if(!Source||Source==P||Source->LinkedTo.Num()!=1||!FindExpectedPin(W,Source->LinkedTo[0])||Source->LinkedTo[0]->Direction!=EGPD_Output||Source->LinkedTo[0]->GetOwningNodeUnchecked()->GetGraph()!=N->GetGraph())return W.Fail(TEXT("niagara_event_stage_map_upstream_invalid"),TEXT("ParameterMapSet requires one captured exact upstream before public linked/DI SDK."));return true;
}
bool AdoptDITransition(FWork& W,UNiagaraGraph* G,const FGraphIdentity& Before,UEdGraphPin* Override,UClass* Class,UNiagaraDataInterface* DI,UNiagaraNodeInput*& CreatedNode)
{
 if(!W.Admit(TEXT("DI_transition_context")))return false;if(G->Schema!=Before.Schema.Get()||G->Nodes.Num()!=Before.Nodes.Num()+1||!GraphMetadataSignature(G).Equals(Before.Meta,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_event_stage_DI_graph_delta_invalid"),TEXT("DI creation changed more than one node or unrelated metadata."));for(int32 I=0;I<Before.Nodes.Num();++I)if(G->Nodes[I]!=Before.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_event_stage_retained_node_changed"),TEXT("DI creation replaced/reordered a retained node."));auto* New=G->Nodes.Last().Get();if(!Live(New)||New->GetClass()!=UNiagaraNodeInput::StaticClass()||New->GetGraph()!=G||!New->IsIn(W.System.Get())||New->GetPackage()!=W.System->GetPackage())return W.Fail(TEXT("niagara_event_stage_DI_node_owner_invalid"),TEXT("New DI node must be exact stock NodeInput in the requested System/graph/package."));TSet<UEdGraphPin*> Pins;for(UEdGraphNode* N:G->Nodes){if(!Live(N)||!W.Pin(N))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_event_stage_DI_pin_invalid"),TEXT("New graph pin membership is malformed."));Pins.Add(P);}}if(!Pins.Contains(Override))return W.Fail(TEXT("niagara_event_stage_override_expired"),TEXT("Returned exact override disappeared; stale pointer not dereferenced."));UEdGraphPin* Output=nullptr;int32 OutputCount=0;for(auto* P:New->Pins){if(P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P)){++OutputCount;Output=P;}else if(!P->LinkedTo.IsEmpty())return W.Fail(TEXT("niagara_event_stage_DI_extra_link"),TEXT("DI node contains an unexpected linked pin."));}if(OutputCount!=1||!Output)return W.Fail(TEXT("niagara_event_stage_DI_output_invalid"),TEXT("Expected one actual typed DI output."));
 for(const auto& E:Before.Nodes){auto* N=E.Node.Get();if(N->GetGraph()!=G||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num()||!FunctionFieldsExact(E,N))return W.Fail(TEXT("niagara_event_stage_retained_node_changed"),TEXT("Retained node/function binding changed during DI SDK."));for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& PE=E.Pins[J];if(P!=PE.Pointer||!PinHeaderSame(PE,P))return W.Fail(TEXT("niagara_event_stage_retained_pin_changed"),TEXT("Retained pin/default/type changed during DI SDK."));const TArray<UEdGraphPin*> Expected=P==Override?TArray<UEdGraphPin*>{Output}:PE.Links;if(P->LinkedTo!=Expected)return W.Fail(TEXT("niagara_event_stage_DI_unselected_links_changed"),TEXT("Only exact override-to-new-DI edge was admitted."));}}
 auto* Input=static_cast<UNiagaraNodeInput*>(New);if(!Live(DI)||DI->GetClass()!=Class||DI->GetOuter()!=Input||DI->GetPackage()!=W.System->GetPackage()||ReflectedNodeDI(Input)!=DI||Input->Input.GetType()!=FNiagaraTypeDefinition(Class)||UEdGraphSchema_Niagara::PinToTypeDefinition(Output)!=FNiagaraTypeDefinition(Class)||Output->LinkedTo!=TArray<UEdGraphPin*>{Override}||Override->LinkedTo!=TArray<UEdGraphPin*>{Output})return W.Fail(TEXT("niagara_event_stage_DI_typed_readback_failed"),TEXT("Created exact DI/owner/public output does not match requested type and reciprocal edge."));if(!W.Own(Input)||!W.Own(DI))return false;CreatedNode=Input;auto* Source=W.Sources.FindByPredicate([&](const auto& S){return S.Graph.Get()==G;});if(!Source)return false;W.Graphs.RemoveAll([&](const auto& E){return E.Graph.Get()==G;});return W.CaptureGraph(Source->Source.Get());
}
UNiagaraDataInterface* CreateDI(FWork& W,UEdGraphPin* Override,UClass* Class,const FString& Name,bool RT)
{
 if(!PreflightMapValue(W,Override)||!Override->LinkedTo.IsEmpty()||!Class||!Class->IsChildOf(UNiagaraDataInterface::StaticClass())||!W.Pin(Class))return W.Fail(TEXT("niagara_event_stage_DI_precondition_failed"),TEXT("DI SDK needs an empty exact value override and public DI class.")),nullptr;auto* G=Cast<UNiagaraGraph>(Override->GetOwningNodeUnchecked()->GetGraph());const auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return nullptr;const FGraphIdentity Before=*Expected;UNiagaraDataInterface* DI=nullptr;UNiagaraNodeInput* Node=nullptr;if(!W.SDK(TEXT("SetDataInterfaceValueForFunctionInput"),true))return nullptr;auto Row=W.MutationRow(TEXT("SetDataInterfaceValueForFunctionInput"));Row->SetStringField(TEXT("input_alias"),Name);Row->SetStringField(TEXT("class"),Class->GetPathName());{TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput(*Override,Class,Name,DI);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),DI!=nullptr);W.R.bChanged|=DI!=nullptr;if(!AdoptDITransition(W,G,Before,Override,Class,DI,Node))return nullptr;}if(!W.After(TEXT("SetDataInterfaceValueForFunctionInput")))return nullptr;Row->SetStringField(TEXT("actual_DI_path"),DI->GetPathName());Row->SetStringField(TEXT("actual_input_node_path"),Node->GetPathName());FExpectedDI E;E.Node=Node;E.DI=DI;E.RT=RT;E.Input=Node->Input;E.Sort=Node->CallSortPriority;E.Usage=Node->Usage;if(RT){auto* D=Cast<UNiagaraDataInterfaceRenderTarget2D>(DI);E.User=D->RenderTargetUserParameter.Parameter;E.Inherit=D->bInheritUserParameterSettings;}else{auto* D=Cast<UNiagaraDataInterfaceTexture>(DI);E.User=D->TextureUserParameter.Parameter;E.Texture=D->Texture;}W.ExpectedDI.Add(E);return DI;
}
bool LinkMapUser(FWork& W,UEdGraphPin* Target,const FNiagaraVariable& User)
{
 if(!PreflightMapValue(W,Target)||!Target->LinkedTo.IsEmpty()||!VerifyCanonicalUserForPath(W,User))return false;auto* G=Cast<UNiagaraGraph>(Target->GetOwningNodeUnchecked()->GetGraph());auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return false;const FGraphIdentity Before=*Expected;const FString Meta=GraphMetadataWithout(G,&User);UNiagaraScriptVariable* ExistingMetadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&!W.Own(ExistingMetadata))return false;if(!W.SDK(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"),true))return false;auto Row=W.MutationRow(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"));Row->SetStringField(TEXT("target_node"),Target->GetOwningNodeUnchecked()->NodeGuid.ToString());Row->SetStringField(TEXT("target_pin"),Target->PinId.ToString());Row->SetStringField(TEXT("full_user_parameter"),User.GetName().ToString());{TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);TSet<FNiagaraVariableBase> Known;Known.Add(User);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Target,User,Known);Row->SetBoolField(TEXT("SDK_returned"),true);W.R.bChanged=true;if(!CaptureTransitionResult(W,G,Before,nullptr,FNiagaraParameterHandle(),User.GetType(),nullptr,Target,&User,false,Meta))return false;}UNiagaraScriptVariable* Metadata=G->GetAllMetaData().FindRef(User);if((ExistingMetadata&&Metadata!=ExistingMetadata)||!Metadata||!W.Own(Metadata)||Metadata->DefaultMode!=ENiagaraDefaultMode::FailIfPreviouslyNotSet)return W.Fail(TEXT("niagara_event_stage_user_metadata_readback_failed"),TEXT("Selected metadata identity/default mode changed unexpectedly."));return W.After(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"));
}
bool SetPinDefault(FWork& W,UEdGraphPin* P,const FString& Value)
{
 if(!W.PinForWrite(P))return false;const UEdGraphSchema* Schema=P->GetSchema();if(Schema&&Schema->GetClass()!=UEdGraphSchema_Niagara::StaticClass())return W.Fail(TEXT("niagara_event_stage_schema_unsupported"),TEXT("Only exact public Niagara default setter is supported."));if(Schema&&!W.Pin(Schema->GetClass()))return false;auto* E=FindExpectedPin(W,P);if(!W.Step(TEXT("ClearPinDefaultObjectAndText"),true,[&]{P->DefaultObject=nullptr;P->DefaultTextValue=FText::GetEmpty();E->DefaultObject=nullptr;E->DefaultText=FString();}))return false;if(!W.SDK(TEXT("Schema_TrySetDefaultValue"),true))return false;E->DefaultValue=Value;if(Schema)Schema->TrySetDefaultValue(*P,Value);else P->DefaultValue=Value;W.Prefix->SetStringField(TEXT("last_default_value_assigned"),Value);W.R.bChanged=true;return W.After(TEXT("Schema_TrySetDefaultValue"));
}


template<typename T>T* LoadResource(FWork& W,const FString& Path)
{
 if(Path.IsEmpty())return nullptr;if(Path.ToLower().Contains(TEXT("unrealmcp"))||Path.ToLower().Contains(TEXT("cubelessmcp"))||Path.ToLower().Contains(TEXT("toolsetregistry")))return W.Fail(TEXT("niagara_event_stage_dependency_rejected"),TEXT("MCP/Cubeless/Toolset resource dependency rejected.")),nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);T* O=nullptr;if(!W.Step(TEXT("LoadObject_Resource"),false,[&]{O=LoadObject<T>(nullptr,*Expected);W.Prefix->SetBoolField(TEXT("resource_load_returned_nonnull"),O!=nullptr);}))return nullptr;if(!O||!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!W.Pin(O))return W.Fail(TEXT("niagara_event_stage_resource_load_failed"),TEXT("Exact requested resource could not be loaded.")),nullptr;return O;
}
bool VerifyResourceBinding(FWork& W,UNiagaraNodeFunctionCall* Function,const FNiagaraParameterHandle& Alias,const FNiagaraVariable& User,const TArray<FUserValue>& Before,bool New,UNiagaraDataInterface* DI,UObject* ExpectedObject)
{
 if(!W.Admit(TEXT("resource_binding_typed_readback"))||!VerifyUnrelatedUserValues(W,Before,User,New))return false;auto* Override=FindNiagaraStackFunctionInputOverridePin(Function,Alias);if(!ContinueRead())return false;if(!Override||!FindExpectedPin(W,Override)||Override->LinkedTo.Num()!=1||!FindExpectedPin(W,Override->LinkedTo[0]))return W.Fail(TEXT("niagara_event_stage_override_readback_failed"),TEXT("Exact module override-to-DI edge not observed after compile/save."));auto* Node=Cast<UNiagaraNodeInput>(Override->LinkedTo[0]->GetOwningNodeUnchecked());if(!Node||ReflectedNodeDI(Node)!=DI||DI->GetOuter()!=Node||W.System->GetExposedParameters().GetUObject(User).Get()!=ExpectedObject)return W.Fail(TEXT("niagara_event_stage_resource_readback_failed"),TEXT("Actual selected User object/DI node binding differs from requested typed target."));return W.Admit(TEXT("resource_binding_verified"));
}

TArray<TSharedPtr<FJsonValue>> IntArrayToJsonValues(std::initializer_list<int32> Values);
TSharedPtr<FJsonObject> NiagaraVariableWithDataToJsonObject(const FNiagaraVariable&);
TSharedPtr<FJsonValue> NiagaraVariableDataToJsonValue(const FNiagaraVariable&);

// Dedicated original JSON projection helpers; no mutation/compile/save calls.
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
template<typename TEnum>
FString NiagaraEnumValueName(TEnum Value)
{
    if (const UEnum* Enum = StaticEnum<TEnum>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Value));
    }
    return FString::FromInt(static_cast<int32>(Value));
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
TSharedPtr<FJsonValue> NiagaraVariableDataToJsonValue(const FNiagaraVariable& Variable)
{
    if (!Variable.IsDataAllocated())
    {
        return MakeShared<FJsonValueNull>();
    }

    const FNiagaraTypeDefinition& TypeDef = Variable.GetType();
    if(!TypeDef.IsValid()||Variable.GetAllocatedSizeInBytes()<TypeDef.GetSize())return MakeShared<FJsonValueNull>();
    if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        return MakeShared<FJsonValueNumber>(Variable.GetValue<float>());
    }
    if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
    {
        return MakeShared<FJsonValueNumber>(Variable.GetValue<int32>());
    }
    if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        const FNiagaraBool Value = Variable.GetValue<FNiagaraBool>();
        return MakeShared<FJsonValueBoolean>(Value.GetValue());
    }
    if (TypeDef.IsEnum() && TypeDef.GetEnum())
    {
        const UEnum* Enum = TypeDef.GetEnum();
        const FNiagaraInt32 Value = Variable.GetValue<FNiagaraInt32>();
        const int32 EnumIndex = Enum->GetIndexByValue(Value.Value);
        if (EnumIndex != INDEX_NONE)
        {
            return MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(EnumIndex));
        }
        return MakeShared<FJsonValueNumber>(Value.Value);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
    {
        const FLinearColor Value = Variable.GetValue<FLinearColor>();
        return NumberArrayToJson({ Value.R, Value.G, Value.B, Value.A });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
    {
        const FVector2f Value = Variable.GetValue<FVector2f>();
        return NumberArrayToJson({ Value.X, Value.Y });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
    {
        const FVector3f Value = Variable.GetValue<FVector3f>();
        return NumberArrayToJson({ Value.X, Value.Y, Value.Z });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
    {
        const FVector4f Value = Variable.GetValue<FVector4f>();
        return NumberArrayToJson({ Value.X, Value.Y, Value.Z, Value.W });
    }
    if (TypeDef == FNiagaraTypeDefinition::GetPositionDef())
    {
        const FVector3f Value = Variable.GetValue<FVector3f>();
        return NumberArrayToJson({ Value.X, Value.Y, Value.Z });
    }

    return MakeShared<FJsonValueString>(Variable.ToString());
}
TSharedPtr<FJsonObject> NiagaraVariableWithDataToJsonObject(const FNiagaraVariable& Variable)
{
    TSharedPtr<FJsonObject> VariableObject = NiagaraVariableToJsonObject(Variable);
    VariableObject->SetBoolField(TEXT("has_data"), Variable.IsDataAllocated());
    VariableObject->SetNumberField(TEXT("data_size_bytes"), Variable.GetAllocatedSizeInBytes());
    VariableObject->SetField(TEXT("value"), NiagaraVariableDataToJsonValue(Variable));
    return VariableObject;
}
TSharedPtr<FJsonObject> NiagaraParameterBindingToJsonObject(FNiagaraParameterBinding& Binding)
{
    TSharedPtr<FJsonObject> BindingObject = MakeShared<FJsonObject>();
    BindingObject->SetObjectField(TEXT("resolved_parameter"), NiagaraVariableToJsonObject(Binding.ResolvedParameter));
#if WITH_EDITORONLY_DATA
    BindingObject->SetObjectField(TEXT("aliased_parameter"), NiagaraVariableToJsonObject(Binding.AliasedParameter));
    BindingObject->SetObjectField(TEXT("default_aliased_parameter"), NiagaraVariableWithDataToJsonObject(Binding.GetDefaultAliasedParameter()));
    BindingObject->SetObjectField(TEXT("default_resolved_parameter"), NiagaraVariableWithDataToJsonObject(Binding.GetDefaultResolvedParameter()));
    BindingObject->SetBoolField(TEXT("is_set_to_default"), Binding.IsSetToDefault());
    BindingObject->SetStringField(TEXT("display_string"), Binding.ToString());
    BindingObject->SetBoolField(TEXT("allow_user_parameters"), Binding.AllowUserParameters());
    BindingObject->SetBoolField(TEXT("allow_system_parameters"), Binding.AllowSystemParameters());
    BindingObject->SetBoolField(TEXT("allow_emitter_parameters"), Binding.AllowEmitterParameters());
    BindingObject->SetBoolField(TEXT("allow_particle_parameters"), Binding.AllowParticleParameters());
    BindingObject->SetBoolField(TEXT("allow_static_variables"), Binding.AllowStaticVariables());
#endif
    return BindingObject;
}
TSharedPtr<FJsonObject> NiagaraSimStageCompileDataToJsonObject(const FNiagaraSimulationStageCompilationData& CompileData)
{
    TSharedPtr<FJsonObject> CompileObject = MakeShared<FJsonObject>();
    CompileObject->SetStringField(TEXT("stage_guid"), CompileData.StageGuid.ToString(EGuidFormats::DigitsWithHyphens));
    CompileObject->SetStringField(TEXT("stage_name"), CompileData.StageName.ToString());
    CompileObject->SetStringField(TEXT("enabled_binding"), CompileData.EnabledBinding.ToString());
    CompileObject->SetArrayField(TEXT("element_count"), IntArrayToJsonValues({
        CompileData.ElementCount.X,
        CompileData.ElementCount.Y,
        CompileData.ElementCount.Z
    }));
    CompileObject->SetStringField(TEXT("element_count_x_binding"), CompileData.ElementCountXBinding.ToString());
    CompileObject->SetStringField(TEXT("element_count_y_binding"), CompileData.ElementCountYBinding.ToString());
    CompileObject->SetStringField(TEXT("element_count_z_binding"), CompileData.ElementCountZBinding.ToString());
    CompileObject->SetNumberField(TEXT("num_iterations"), CompileData.NumIterations);
    CompileObject->SetStringField(TEXT("num_iterations_binding"), CompileData.NumIterationsBinding.ToString());
    CompileObject->SetStringField(TEXT("iteration_source_type"), NiagaraEnumValueName(CompileData.IterationSourceType));
    CompileObject->SetStringField(TEXT("iteration_data_interface"), CompileData.IterationDataInterface.ToString());
    CompileObject->SetStringField(TEXT("iteration_direct_binding"), CompileData.IterationDirectBinding.ToString());
    CompileObject->SetStringField(TEXT("execute_behavior"), NiagaraEnumValueName(CompileData.ExecuteBehavior));
    CompileObject->SetBoolField(TEXT("partial_particle_update"), CompileData.PartialParticleUpdate);
    CompileObject->SetBoolField(TEXT("particle_iteration_state_enabled"), CompileData.bParticleIterationStateEnabled);
    CompileObject->SetStringField(TEXT("particle_iteration_state_binding"), CompileData.ParticleIterationStateBinding.ToString());
    CompileObject->SetArrayField(TEXT("particle_iteration_state_range"), IntArrayToJsonValues({
        CompileData.ParticleIterationStateRange.X,
        CompileData.ParticleIterationStateRange.Y
    }));
    CompileObject->SetBoolField(TEXT("gpu_dispatch_force_linear"), CompileData.bGpuDispatchForceLinear);
    CompileObject->SetStringField(TEXT("direct_dispatch_type"), NiagaraEnumValueName(CompileData.DirectDispatchType));
    CompileObject->SetStringField(TEXT("direct_dispatch_element_type"), NiagaraEnumValueName(CompileData.DirectDispatchElementType));
    CompileObject->SetBoolField(TEXT("override_gpu_dispatch_num_threads"), CompileData.bOverrideGpuDispatchNumThreads);
    CompileObject->SetArrayField(TEXT("override_gpu_dispatch_num_threads_value"), IntArrayToJsonValues({
        CompileData.OverrideGpuDispatchNumThreads.X,
        CompileData.OverrideGpuDispatchNumThreads.Y,
        CompileData.OverrideGpuDispatchNumThreads.Z
    }));
    CompileObject->SetObjectField(TEXT("override_gpu_dispatch_num_threads_x_binding"), NiagaraVariableToJsonObject(CompileData.OverrideGpuDispatchNumThreadsXBinding));
    CompileObject->SetObjectField(TEXT("override_gpu_dispatch_num_threads_y_binding"), NiagaraVariableToJsonObject(CompileData.OverrideGpuDispatchNumThreadsYBinding));
    CompileObject->SetObjectField(TEXT("override_gpu_dispatch_num_threads_z_binding"), NiagaraVariableToJsonObject(CompileData.OverrideGpuDispatchNumThreadsZBinding));
    return CompileObject;
}
TSharedPtr<FJsonObject> NiagaraAttributeBindingToJsonObject(const FNiagaraVariableAttributeBinding& Binding)
{
    TSharedPtr<FJsonObject> BindingObject = MakeShared<FJsonObject>();
    BindingObject->SetBoolField(TEXT("is_valid"), Binding.IsValid());
    BindingObject->SetBoolField(TEXT("is_particle_binding"), Binding.IsParticleBinding());
    BindingObject->SetBoolField(TEXT("binding_exists_on_source"), Binding.DoesBindingExistOnSource());
    BindingObject->SetBoolField(TEXT("can_bind_to_host_parameter_map"), Binding.CanBindToHostParameterMap());
    BindingObject->SetStringField(TEXT("binding_source_mode"), NiagaraEnumValueName(Binding.GetBindingSourceMode()));
    BindingObject->SetObjectField(TEXT("param_map_bindable_variable"), NiagaraVariableToJsonObject(Binding.GetParamMapBindableVariable()));
    BindingObject->SetObjectField(TEXT("data_set_bindable_variable"), NiagaraVariableToJsonObject(Binding.GetDataSetBindableVariable()));
#if WITH_EDITORONLY_DATA
    BindingObject->SetStringField(TEXT("display_name"), Binding.GetName().ToString());
    BindingObject->SetStringField(TEXT("default_value_string"), Binding.GetDefaultValueString());
#endif
    return BindingObject;
}
TSharedPtr<FJsonObject> NiagaraDataInterfaceBindingToJsonObject(const FNiagaraVariableDataInterfaceBinding& Binding)
{
    TSharedPtr<FJsonObject> BindingObject = MakeShared<FJsonObject>();
    BindingObject->SetObjectField(TEXT("bound_variable"), NiagaraVariableWithDataToJsonObject(Binding.BoundVariable));
    BindingObject->SetBoolField(TEXT("is_valid"), Binding.BoundVariable.GetType().IsDataInterface());
    return BindingObject;
}
TSharedPtr<FJsonObject> NiagaraParameterBindingWithValueToJsonObject(FNiagaraParameterBindingWithValue& Binding)
{
    TSharedPtr<FJsonObject> BindingObject = NiagaraParameterBindingToJsonObject(Binding);
    const TConstArrayView<uint8> DefaultValueBytes = Binding.GetDefaultValueArray();
    BindingObject->SetNumberField(TEXT("default_value_byte_count"), DefaultValueBytes.Num());

    if(DefaultValueBytes.Num()>65536){BindingObject->SetStringField(TEXT("default_value_decode_status"),TEXT("bounded_value_unavailable"));return BindingObject;}
    if (DefaultValueBytes.Num() > 0)
    {
        FNiagaraVariable DefaultValueVariable(Binding.ResolvedParameter);
#if WITH_EDITORONLY_DATA
        const FNiagaraVariable& DefaultAliasedParameter = Binding.GetDefaultAliasedParameter();
        if (DefaultAliasedParameter.GetType().IsValid())
        {
            DefaultValueVariable = DefaultAliasedParameter;
        }
#endif
        if (DefaultValueVariable.GetType().IsValid() &&
            DefaultValueVariable.GetType().GetSize() == DefaultValueBytes.Num())
        {
            DefaultValueVariable.SetData(DefaultValueBytes.GetData());
            BindingObject->SetField(TEXT("default_value"), NiagaraVariableDataToJsonValue(DefaultValueVariable));
            BindingObject->SetStringField(TEXT("default_value_decode_status"), TEXT("decoded"));
        }
        else
        {
            BindingObject->SetStringField(TEXT("default_value_decode_status"), TEXT("type_size_mismatch"));
        }
    }
    else
    {
        BindingObject->SetStringField(TEXT("default_value_decode_status"), TEXT("no_default_value_bytes"));
    }

    return BindingObject;
}
TArray<TSharedPtr<FJsonValue>> IntArrayToJsonValues(std::initializer_list<int32> Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (int32 Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return JsonValues;
}
bool TryGetJsonIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue, FString& OutError)
{
    if (!Object.IsValid() || !Object->HasField(FieldName))
    {
        return false;
    }

    double NumberValue = 0.0;
    if (!Object->TryGetNumberField(FieldName, NumberValue))
    {
        OutError = FString::Printf(TEXT("'%s' must be a number"), FieldName);
        return false;
    }
    if (!FMath::IsFinite(NumberValue))
    {
        OutError = FString::Printf(TEXT("'%s' must be finite"), FieldName);
        return false;
    }

    const double RoundedValue = FMath::RoundToDouble(NumberValue);
    if (!FMath::IsNearlyEqual(NumberValue, RoundedValue, KINDA_SMALL_NUMBER))
    {
        OutError = FString::Printf(TEXT("'%s' must be an integer"), FieldName);
        return false;
    }

    if(RoundedValue<MIN_int32||RoundedValue>MAX_int32){OutError=FString(FieldName)+TEXT(" must fit int32");return false;}
    OutValue = static_cast<int32>(RoundedValue);
    return true;
}

TSharedPtr<FJsonObject> StagePublicFields(UNiagaraSimulationStageBase* Stage)
{
 auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Stage->SimulationStageName.ToString());O->SetBoolField(TEXT("enabled"),Stage->bEnabled!=0);O->SetStringField(TEXT("outer_emitter_version"),Stage->OuterEmitterVersion.ToString());
 if(auto* GenericStage=Cast<UNiagaraSimulationStageGeneric>(Stage)){
                     O->SetStringField(TEXT("iteration_source"), NiagaraEnumValueName(GenericStage->IterationSource));
                    O->SetStringField(TEXT("execute_behavior"), NiagaraEnumValueName(GenericStage->ExecuteBehavior));
                    O->SetBoolField(TEXT("disable_partial_particle_update"), GenericStage->bDisablePartialParticleUpdate != 0);
                    O->SetBoolField(TEXT("particle_iteration_state_enabled"), GenericStage->bParticleIterationStateEnabled != 0);
                    O->SetArrayField(TEXT("particle_iteration_state_range"), IntArrayToJsonValues({
                        GenericStage->ParticleIterationStateRange.X,
                        GenericStage->ParticleIterationStateRange.Y
                    }));
                    O->SetBoolField(TEXT("gpu_dispatch_force_linear"), GenericStage->bGpuDispatchForceLinear != 0);
                    O->SetBoolField(TEXT("override_gpu_dispatch_num_threads"), GenericStage->bOverrideGpuDispatchNumThreads != 0);
                    O->SetStringField(TEXT("direct_dispatch_type"), NiagaraEnumValueName(GenericStage->DirectDispatchType));
                    O->SetStringField(TEXT("direct_dispatch_element_type"), NiagaraEnumValueName(GenericStage->DirectDispatchElementType));

                    O->SetObjectField(TEXT("enabled_binding"), NiagaraAttributeBindingToJsonObject(GenericStage->EnabledBinding));
                    O->SetObjectField(TEXT("data_interface"), NiagaraDataInterfaceBindingToJsonObject(GenericStage->DataInterface));
                    O->SetObjectField(TEXT("particle_iteration_state_binding"), NiagaraAttributeBindingToJsonObject(GenericStage->ParticleIterationStateBinding));
                    O->SetObjectField(TEXT("num_iterations"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->NumIterations));
                    O->SetObjectField(TEXT("element_count_x"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->ElementCountX));
                    O->SetObjectField(TEXT("element_count_y"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->ElementCountY));
                    O->SetObjectField(TEXT("element_count_z"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->ElementCountZ));
                    O->SetObjectField(TEXT("override_gpu_dispatch_num_threads_x"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->OverrideGpuDispatchNumThreadsX));
                    O->SetObjectField(TEXT("override_gpu_dispatch_num_threads_y"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->OverrideGpuDispatchNumThreadsY));
                    O->SetObjectField(TEXT("override_gpu_dispatch_num_threads_z"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->OverrideGpuDispatchNumThreadsZ));

 }return O;
}
FString StageStateKey(UNiagaraSimulationStageBase* Stage){return Json(StagePublicFields(Stage));}
FString EventStateKey(const FVersionedNiagaraEmitterData* Data,const FNiagaraEventScriptProperties* Append,int32 Skip)
{
 if(!Data)return FString();auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows,SpawnGenerators,UpdateGenerators;
 auto Add=[&](const FNiagaraEventScriptProperties& E){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("script"),E.Script?E.Script->GetPathName():FString());V->SetNumberField(TEXT("mode"),static_cast<int32>(E.ExecutionMode));V->SetNumberField(TEXT("spawn"),E.SpawnNumber);V->SetNumberField(TEXT("max"),E.MaxEventsPerFrame);V->SetStringField(TEXT("source"),E.SourceEmitterID.ToString());V->SetStringField(TEXT("event"),E.SourceEventName.ToString());V->SetBoolField(TEXT("random"),E.bRandomSpawnNumber);V->SetNumberField(TEXT("min"),E.MinSpawnNumber);V->SetBoolField(TEXT("initial"),E.UpdateAttributeInitialValues);Rows.Add(MakeShared<FJsonValueObject>(V));};
 const auto& A=Data->GetEventHandlers();for(int32 I=0;I<A.Num();++I)if(I!=Skip)Add(A[I]);if(Append)Add(*Append);
 for(const auto& G:Data->SpawnScriptProps.EventGenerators)SpawnGenerators.Add(MakeShared<FJsonValueString>(G.ID.ToString()));for(const auto& G:Data->UpdateScriptProps.EventGenerators)UpdateGenerators.Add(MakeShared<FJsonValueString>(G.ID.ToString()));O->SetArrayField(TEXT("events"),Rows);O->SetArrayField(TEXT("spawn_generators"),SpawnGenerators);O->SetArrayField(TEXT("update_generators"),UpdateGenerators);return Json(O);
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

    auto& W=*Current;if(!W.Admit(TEXT("script_compile_status")))return ScriptObject;ENiagaraScriptCompileStatus Status;
    if(!W.Step(TEXT("GetLastCompileStatus"),false,[&]{Status=Script->GetLastCompileStatus();}))return ScriptObject;
    ScriptObject->SetStringField(TEXT("script_name"), Script->GetName());
    ScriptObject->SetStringField(TEXT("script_path"), Script->GetPathName());
    ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
    ScriptObject->SetStringField(TEXT("usage_id"), Script->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
    ScriptObject->SetStringField(TEXT("compile_status"), NiagaraCompileStatusName(Status));
    ScriptObject->SetBoolField(TEXT("has_error"), IsNiagaraCompileErrorStatus(Status));
    ScriptObject->SetBoolField(TEXT("has_warning"), IsNiagaraCompileWarningStatus(Status));
    bool CPU=false,GPU=false;if(!W.Step(TEXT("ScriptIsReadyCPU"),false,[&]{CPU=Script->IsReadyToRun(ENiagaraSimTarget::CPUSim);}))return ScriptObject;
    if(!W.Step(TEXT("ScriptIsReadyGPU"),false,[&]{GPU=Script->IsReadyToRun(ENiagaraSimTarget::GPUComputeSim);}))return ScriptObject;ScriptObject->SetBoolField(TEXT("is_ready_cpu"),CPU);ScriptObject->SetBoolField(TEXT("is_ready_gpu"),GPU);
    return ScriptObject;
}
struct FResolvedEventSourceEmitter{FGuid HandleId;FString Name;int32 Index=INDEX_NONE;FVersionedNiagaraEmitterData* Data=nullptr;};
struct FResolvedNiagaraEmitterTarget{UNiagaraEmitter* Emitter=nullptr;FVersionedNiagaraEmitterData* Data=nullptr;FGuid Version;FString Name;int32 Index=INDEX_NONE;};
FString NormalizeEventHandlerToken(FString Value)
{
    Value.TrimStartAndEndInline();
    Value.ToLowerInline();
    Value.ReplaceInline(TEXT("_"), TEXT(""));
    Value.ReplaceInline(TEXT("-"), TEXT(""));
    Value.ReplaceInline(TEXT(" "), TEXT(""));
    return Value;
}
bool TryParseEventHandlerExecutionMode(const FString& Value, EScriptExecutionMode& OutMode)
{
    const FString Token = NormalizeEventHandlerToken(Value);
    if (Token == TEXT("everyparticle") || Token == TEXT("every"))
    {
        OutMode = EScriptExecutionMode::EveryParticle;
        return true;
    }
    if (Token == TEXT("spawnedparticles") || Token == TEXT("spawned") || Token == TEXT("eventspawn"))
    {
        OutMode = EScriptExecutionMode::SpawnedParticles;
        return true;
    }
    return false;
}
bool ResolveEventSourceEmitter(
    UNiagaraSystem* System,
    int32 SourceEmitterIndex,
    const FString& SourceEmitterName,
    const FString& SourceEmitterIdText,
    FResolvedEventSourceEmitter& OutResolved,
    FString& OutError)
{
    FGuid SourceEmitterId;
    const bool bHasIdSelector = !SourceEmitterIdText.IsEmpty();
    if (bHasIdSelector && !FGuid::Parse(SourceEmitterIdText, SourceEmitterId))
    {
        OutError = FString::Printf(TEXT("Invalid source_emitter_id GUID: %s"), *SourceEmitterIdText);
        return false;
    }
    if (bHasIdSelector && !SourceEmitterId.IsValid())
    {
        OutError = TEXT("source_emitter_id cannot be the all-zero GUID; omit source selectors to use the target emitter itself");
        return false;
    }

    int32 MatchCount = 0;
    int32 CurrentIndex = 0;
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        const FString HandleName = Handle.GetName().ToString();
        const bool bMatches =
            (SourceEmitterIndex == INDEX_NONE || SourceEmitterIndex == CurrentIndex) &&
            (SourceEmitterName.IsEmpty() || SourceEmitterName.Equals(HandleName, ESearchCase::IgnoreCase)) &&
            (!bHasIdSelector || SourceEmitterId == Handle.GetId());
        if (bMatches)
        {
            ++MatchCount;
            if (MatchCount == 1)
            {
                OutResolved.HandleId = Handle.GetId();
                OutResolved.Name = HandleName;
                OutResolved.Index = CurrentIndex;
                OutResolved.Data = Handle.GetEmitterData();
            }
        }
        ++CurrentIndex;
    }

    if (MatchCount == 0)
    {
        OutError = TEXT("No matching source Niagara emitter was found");
        return false;
    }
    if (MatchCount > 1)
    {
        OutError = TEXT("Source emitter selector matched more than one emitter; provide source_emitter_index, source_emitter_name, or source_emitter_id");
        return false;
    }
    if (!OutResolved.Data)
    {
        OutError = TEXT("Matched source Niagara emitter is invalid");
        return false;
    }
    return true;
}
FGuid GetEmitterHandleId(const UNiagaraSystem* System, int32 EmitterIndex)
{
    const TArray<FNiagaraEmitterHandle>& Handles = System->GetEmitterHandles();
    return Handles.IsValidIndex(EmitterIndex) ? Handles[EmitterIndex].GetId() : FGuid();
}
void ResolveEventSourceForReport(
    const UNiagaraSystem* System,
    int32 TargetEmitterIndex,
    const FGuid& StoredSourceEmitterId,
    FGuid& OutResolvedSourceEmitterId,
    FString& OutSourceEmitterName,
    int32& OutSourceEmitterIndex,
    bool& bOutSourceIsSelf)
{
    const FGuid TargetEmitterId = GetEmitterHandleId(System, TargetEmitterIndex);
    OutResolvedSourceEmitterId = StoredSourceEmitterId.IsValid() ? StoredSourceEmitterId : TargetEmitterId;
    OutSourceEmitterName.Reset();
    OutSourceEmitterIndex = INDEX_NONE;
    bOutSourceIsSelf = OutResolvedSourceEmitterId == TargetEmitterId;

    int32 CurrentIndex = 0;
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (Handle.GetId() == OutResolvedSourceEmitterId)
        {
            OutSourceEmitterName = Handle.GetName().ToString();
            OutSourceEmitterIndex = CurrentIndex;
            return;
        }
        ++CurrentIndex;
    }
}
FGuid CanonicalEventSourceId(const FGuid& StoredSourceId, const FGuid& TargetEmitterId)
{
    return StoredSourceId.IsValid() ? StoredSourceId : TargetEmitterId;
}
bool ResolveSingleNiagaraEmitterTarget(UNiagaraSystem* System, int32 TargetIndex, const FString& TargetName,
    FResolvedNiagaraEmitterTarget& OutResolved, int32& OutMatchCount)
{
    OutMatchCount = 0;
    int32 CurrentIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bMatches =
            (TargetIndex == INDEX_NONE || TargetIndex == CurrentIndex) &&
            (TargetName.IsEmpty() || TargetName.Equals(EmitterName, ESearchCase::IgnoreCase));
        if (bMatches)
        {
            ++OutMatchCount;
            if (OutMatchCount == 1)
            {
                const FVersionedNiagaraEmitter VersionedEmitter = EmitterHandle.GetInstance();
                OutResolved.Data = EmitterHandle.GetEmitterData();
                OutResolved.Emitter = VersionedEmitter.Emitter;
                OutResolved.Version = VersionedEmitter.Version;
                OutResolved.Name = EmitterName;
                OutResolved.Index = CurrentIndex;
            }
        }
        ++CurrentIndex;
    }
    return OutMatchCount == 1 && OutResolved.Data != nullptr;
}

TSharedPtr<FJsonObject> BuildNiagaraEventHandlerJson(
    const UNiagaraSystem* System,
    int32 TargetEmitterIndex,
    const FString& TargetEmitterName,
    const FNiagaraEventScriptProperties& Handler,
    int32 HandlerIndex)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();auto& W=*Current;if(!W.Admit(TEXT("event_handler_report")))return Object;
    const FGuid TargetEmitterId = GetEmitterHandleId(System, TargetEmitterIndex);
    const FGuid UsageId = Handler.Script ? Handler.Script->GetUsageId() : FGuid();

    FGuid ResolvedSourceEmitterId;
    FString SourceEmitterName;
    int32 SourceEmitterIndex = INDEX_NONE;
    bool bSourceIsSelf = false;
    ResolveEventSourceForReport(
        System,
        TargetEmitterIndex,
        Handler.SourceEmitterID,
        ResolvedSourceEmitterId,
        SourceEmitterName,
        SourceEmitterIndex,
        bSourceIsSelf);

    const FVersionedNiagaraEmitterData* SourceEmitterData = nullptr;
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (Handle.GetId() == ResolvedSourceEmitterId)
        {
            SourceEmitterData = Handle.GetEmitterData();
            break;
        }
    }

    TArray<TSharedPtr<FJsonValue>> SourceSpawnGeneratorValues;
    TArray<TSharedPtr<FJsonValue>> SourceUpdateGeneratorValues;
    bool bSourceEventGeneratorFound = false;
    auto AppendGenerators = [&Handler, &bSourceEventGeneratorFound](
        const TArray<FNiagaraEventGeneratorProperties>& Generators,
        TArray<TSharedPtr<FJsonValue>>& Values)
    {
        for (const FNiagaraEventGeneratorProperties& Generator : Generators)
        {
            Values.Add(MakeShared<FJsonValueString>(Generator.ID.ToString()));
            bSourceEventGeneratorFound |= Generator.ID == Handler.SourceEventName;
        }
    };
    if (SourceEmitterData)
    {
        AppendGenerators(SourceEmitterData->SpawnScriptProps.EventGenerators, SourceSpawnGeneratorValues);
        AppendGenerators(SourceEmitterData->UpdateScriptProps.EventGenerators, SourceUpdateGeneratorValues);
    }

    UNiagaraScriptSource* ScriptSource = Handler.Script
        ? Cast<UNiagaraScriptSource>(Handler.Script->GetLatestSource())
        : nullptr;
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if(ScriptSource&&!W.CaptureGraph(ScriptSource))return Object;UNiagaraNodeOutput* OutputNode=nullptr;
    if(Graph&&!W.Step(TEXT("FindEventOutputNode"),false,[&]{OutputNode=Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleEventScript,UsageId);}))return Object;
    if(OutputNode&&(!W.Pin(OutputNode)||OutputNode->GetGraph()!=Graph))return Object;

    Object->SetNumberField(TEXT("handler_index"), HandlerIndex);
    Object->SetStringField(TEXT("target_emitter_name"), TargetEmitterName);
    Object->SetNumberField(TEXT("target_emitter_index"), TargetEmitterIndex);
    Object->SetStringField(TEXT("target_emitter_id"), TargetEmitterId.ToString(EGuidFormats::DigitsWithHyphens));
    Object->SetStringField(TEXT("usage"), NiagaraScriptUsageName(ENiagaraScriptUsage::ParticleEventScript));
    Object->SetStringField(TEXT("usage_id"), UsageId.ToString(EGuidFormats::DigitsWithHyphens));
    Object->SetStringField(TEXT("execution_mode"), NiagaraEnumValueName(Handler.ExecutionMode));
    Object->SetNumberField(TEXT("spawn_number"), static_cast<double>(Handler.SpawnNumber));
    Object->SetNumberField(TEXT("max_events_per_frame"), static_cast<double>(Handler.MaxEventsPerFrame));
    Object->SetStringField(TEXT("source_emitter_id"), Handler.SourceEmitterID.ToString(EGuidFormats::DigitsWithHyphens));
    Object->SetStringField(TEXT("resolved_source_emitter_id"), ResolvedSourceEmitterId.ToString(EGuidFormats::DigitsWithHyphens));
    Object->SetStringField(TEXT("source_emitter_name"), SourceEmitterName);
    Object->SetNumberField(TEXT("source_emitter_index"), SourceEmitterIndex);
    Object->SetBoolField(TEXT("source_is_self"), bSourceIsSelf);
    Object->SetStringField(TEXT("source_event_name"), Handler.SourceEventName.ToString());
    Object->SetArrayField(TEXT("source_spawn_event_generators"), SourceSpawnGeneratorValues);
    Object->SetArrayField(TEXT("source_update_event_generators"), SourceUpdateGeneratorValues);
    Object->SetBoolField(TEXT("source_event_generator_found"), bSourceEventGeneratorFound);
    Object->SetBoolField(TEXT("random_spawn_number"), Handler.bRandomSpawnNumber);
    Object->SetNumberField(TEXT("min_spawn_number"), static_cast<double>(Handler.MinSpawnNumber));
    Object->SetBoolField(TEXT("update_attribute_initial_values"), Handler.UpdateAttributeInitialValues);
    Object->SetStringField(TEXT("script_path"), Handler.Script ? Handler.Script->GetPathName() : FString());
    bool Ready=false,Compiled=false;int32 Attributes=0;
    if(Handler.Script){if(!W.Step(TEXT("EventScriptReadyCPU"),false,[&]{Ready=Handler.Script->IsReadyToRun(ENiagaraSimTarget::CPUSim);}))return Object;if(!W.Step(TEXT("EventScriptCompiledCPU"),false,[&]{Compiled=Handler.Script->DidScriptCompilationSucceed(false);}))return Object;if(!W.Step(TEXT("EventScriptAttributes"),false,[&]{Attributes=Handler.Script->GetVMExecutableData().Attributes.Num();}))return Object;}
    Object->SetBoolField(TEXT("script_ready_to_run_cpu"),Ready);Object->SetBoolField(TEXT("script_compile_succeeded_cpu"),Compiled);Object->SetNumberField(TEXT("script_compiled_attribute_count"),Attributes);
    Object->SetStringField(TEXT("graph_path"), Graph ? Graph->GetPathName() : FString());
    Object->SetBoolField(TEXT("output_node_exists"), OutputNode != nullptr);
    Object->SetStringField(
        TEXT("output_node_guid"),
        OutputNode ? OutputNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString());
    return Object;
}
// Domain-local expected graph transitions; unchanged captured nodes/pins/metadata remain exact.
struct FMetaObservation{FNiagaraVariableBase Variable;TWeakObjectPtr<UNiagaraScriptVariable> Object;FString State;};
bool MetadataSnapshot(FWork& W,UNiagaraGraph* G,TArray<FMetaObservation>& Rows)
{
 if(G->GetAllMetaData().Num()>65536||!W.Budget(G->GetAllMetaData().Num()))return W.Fail(TEXT("niagara_event_stage_metadata_bound"),TEXT("Metadata inventory exceeds bound."));
 for(const auto& Pair:G->GetAllMetaData()){UNiagaraScriptVariable* V=Pair.Value.Get();if(!Live(V)||V->Variable!=Pair.Key||!W.Pin(V))return W.Fail(TEXT("niagara_event_stage_metadata_invalid"),TEXT("Captured typed metadata must be live and exact."));const auto& Bytes=V->GetDefaultValueVariant();const int32 N=Bytes.GetNumBytes();if(N<0||N>65536||!W.Budget(N))return false;FString ValueHash;if(V->GetDefaultValueData()&&N)ValueHash=FMD5::HashBytes(V->GetDefaultValueData(),N);FString State=V->GetPathName()+TEXT("|")+V->Metadata.GetVariableGuid().ToString()+TEXT("|")+V->GetChangeId().ToString()+TEXT("|")+FString::FromInt(static_cast<int32>(V->DefaultMode))+TEXT("|")+V->DefaultBinding.GetName().ToString()+TEXT("|")+ValueHash;Rows.Add({Pair.Key,V,State});}return true;
}
struct FGraphEditPlan
{
 UEdGraphNode* MutableNode=nullptr;UEdGraphNode* AddedNode=nullptr;TSet<UEdGraphPin*> MutablePins,InitialDynamicPins;TMap<UEdGraphPin*,TArray<UEdGraphPin*>> Links;
 TArray<FNiagaraVariableBase> MutableMetadata;bool AllowOneAddedMetadata=false;FNiagaraTypeDefinition AddedMetadataType;
 bool NodeMutable(const UEdGraphNode* N)const{return N==MutableNode;}
 bool MetaMutable(const FNiagaraVariableBase& V)const{return MutableMetadata.Contains(V);}
};
bool ObserveGraphTransition(FWork& W,UNiagaraGraph* G,const FGraphIdentity& Old,const TArray<FMetaObservation>& OldMeta,const FGraphEditPlan& Plan)
{
 if(!W.Admit(TEXT("graph_edit_context")))return false;if(G->Schema!=Old.Schema.Get()||G->Nodes.Num()!=Old.Nodes.Num()+(Plan.AddedNode?1:0)||G->Nodes.Num()>16384)return W.Fail(TEXT("niagara_event_stage_unexpected_graph_delta"),TEXT("Unexpected schema/node count during dedicated edit."));
 TSet<UEdGraphPin*> LivePins;for(int32 I=0;I<G->Nodes.Num();++I){UEdGraphNode* N=G->Nodes[I];if(!Live(N)||N->GetGraph()!=G||!N->IsIn(W.System.Get())||N->GetPackage()!=W.System->GetPackage()||N->Pins.Num()>65536)return W.Fail(TEXT("niagara_event_stage_generated_node_invalid"),TEXT("Current node ownership/pin bound invalid."));if(I<Old.Nodes.Num()&&N!=Old.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_event_stage_retained_node_replaced"),TEXT("Retained physical node order changed."));if(I==Old.Nodes.Num()&&N!=Plan.AddedNode)return false;if(!W.Pin(N)||!W.Budget(N->Pins.Num()))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_event_stage_pin_owner_invalid"),TEXT("Malformed current pin owner."));LivePins.Add(P);}}
 for(const auto& E:Old.Nodes){auto* N=E.Node.Get();if(!Plan.NodeMutable(N)&&(N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||!FunctionFieldsExact(E,N)||N->Pins.Num()!=E.Pins.Num()))return W.Fail(TEXT("niagara_event_stage_retained_node_fields_changed"),TEXT("Unselected node fields or pin count changed."));if(Plan.NodeMutable(N)){for(const auto& P:E.Pins)if(!LivePins.Contains(P.Pointer))return W.Fail(TEXT("niagara_event_stage_mutable_pin_retired"),TEXT("Allowed node edit unexpectedly removed an existing pin."));}
  for(int32 J=0;J<E.Pins.Num();++J){const auto& P=E.Pins[J];if(!LivePins.Contains(P.Pointer)||(!Plan.NodeMutable(N)&&N->Pins[J]!=P.Pointer))return W.Fail(TEXT("niagara_event_stage_retained_pin_replaced"),TEXT("Old exact pin retired/reordered."));if((P.PersistentId.IsValid()||!Plan.InitialDynamicPins.Contains(P.Pointer))&&P.Pointer->PersistentGuid!=P.PersistentId)return W.Fail(TEXT("niagara_event_stage_pair_guid_changed"),TEXT("Existing public pair PersistentGuid changed during callback."));if(P.Pointer->bNotConnectable!=P.NotConnectable||P.Pointer->bDefaultValueIsReadOnly!=P.DefaultReadOnly)return W.Fail(TEXT("niagara_event_stage_pair_flags_changed"),TEXT("Existing public default-pair flags changed."));if(!Plan.MutablePins.Contains(P.Pointer)&&!PinHeaderSame(P,P.Pointer))return W.Fail(TEXT("niagara_event_stage_unselected_pin_changed"),TEXT("Unselected pin header/default changed."));const auto* Expected=Plan.Links.Find(P.Pointer);if(P.Pointer->LinkedTo!=(Expected?*Expected:P.Links))return W.Fail(TEXT("niagara_event_stage_unselected_link_changed"),TEXT("Links differ from the admitted exact edge plan."));}}
 for(auto* P:LivePins)for(auto* L:P->LinkedTo)if(!L||!LivePins.Contains(L)||!L->LinkedTo.Contains(P))return W.Fail(TEXT("niagara_event_stage_foreign_link"),TEXT("Every resulting link must be same-graph reciprocal and live."));
 TArray<FMetaObservation> Now;if(!MetadataSnapshot(W,G,Now))return false;int32 Added=0;for(const auto& N:Now){const auto* Prior=OldMeta.FindByPredicate([&](const auto& M){return M.Variable==N.Variable;});if(!Prior){++Added;if(!Plan.MetaMutable(N.Variable)&&!(Plan.AllowOneAddedMetadata&&N.Variable.GetType()==Plan.AddedMetadataType))return W.Fail(TEXT("niagara_event_stage_extra_metadata"),TEXT("Unexpected typed metadata created."));}else if(!Plan.MetaMutable(N.Variable)&&(Prior->Object!=N.Object||!Prior->State.Equals(N.State,ESearchCase::CaseSensitive)))return W.Fail(TEXT("niagara_event_stage_unselected_metadata_changed"),TEXT("Unselected metadata/default changed."));}
 for(const auto& M:OldMeta)if(!Plan.MetaMutable(M.Variable)&&!Now.ContainsByPredicate([&](const auto& N){return N.Variable==M.Variable;}))return W.Fail(TEXT("niagara_event_stage_metadata_retired"),TEXT("Unselected typed metadata removed."));if(Added>(Plan.AllowOneAddedMetadata?1:Plan.MutableMetadata.Num()))return W.Fail(TEXT("niagara_event_stage_metadata_delta"),TEXT("Unexpected metadata insertion count."));
 auto* Source=W.Sources.FindByPredicate([&](const auto& S){return S.Graph.Get()==G;});if(!Source)return false;auto SourcePin=Source->Source;W.Graphs.RemoveAll([&](const auto& E){return E.Graph.Get()==G;});if(!W.CaptureGraph(SourcePin.Get()))return false;return true;
}
template<typename F>bool GraphEdit(FWork& W,UNiagaraGraph* G,const TCHAR* Stage,FGraphEditPlan& Plan,F&& Fn)
{
 if(!W.GraphForWrite(G))return false;const auto* Found=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Found)return false;const FGraphIdentity Old=*Found;TArray<FMetaObservation> Meta;if(!MetadataSnapshot(W,G,Meta)||!W.SDK(Stage,true))return false;auto Row=W.MutationRow(Stage);{TGuardValue<UNiagaraGraph*> Mutable(W.TransitionGraph,G);Fn();Row->SetBoolField(TEXT("SDK_returned"),true);if(!ObserveGraphTransition(W,G,Old,Meta,Plan))return false;}return W.After(Stage);
}
UEdGraphPin* NamedGraphPin(FWork& W,UEdGraphNode* N,EEdGraphPinDirection Direction,const FName& Name)
{if(!W.Admit(TEXT("named_pin"))||!Live(N))return nullptr;UEdGraphPin* Found=nullptr;for(auto* P:N->Pins)if(P&&P->Direction==Direction&&P->PinName==Name){if(Found)return nullptr;Found=P;}return Found;}
UEdGraphPin* DynamicOutput(FWork& W,UEdGraphNode* N)
{if(!W.Admit(TEXT("dynamic_output")))return nullptr;UEdGraphPin* Found=nullptr;for(auto* P:N->Pins)if(P&&P->Direction==EGPD_Output&&IsNiagaraDynamicAddPin(P)){if(Found)return nullptr;Found=P;}return Found;}
UEdGraphNode* CreateNode(FWork& W,UNiagaraGraph* G,UClass* Class,int32 X,int32 Y,const FNiagaraFunctionSignature* Signature=nullptr,const FGuid* EventUsage=nullptr)
{
 if(!Class||!Class->IsChildOf(UEdGraphNode::StaticClass())||!W.Pin(Class)||!W.GraphForWrite(G))return nullptr;UEdGraphNode* N=nullptr;if(!W.Step(TEXT("NewObject_GraphNode"),true,[&]{N=NewObject<UEdGraphNode>(G,Class,NAME_None,RF_Transactional);W.Prefix->SetBoolField(TEXT("new_node_returned_nonnull"),N!=nullptr);W.R.bChanged|=N!=nullptr;}))return nullptr;if(!Live(N)||N->GetClass()!=Class||N->GetOuter()!=G||!W.Own(N))return nullptr;auto Row=W.MutationRow(TEXT("created_node_identity"));Row->SetStringField(TEXT("path"),N->GetPathName());Row->SetStringField(TEXT("class"),Class->GetPathName());
 N->NodePosX=X;N->NodePosY=Y;N->CreateNewGuid();if(EventUsage){if(auto* Output=Cast<UNiagaraNodeOutput>(N)){Output->SetUsage(ENiagaraScriptUsage::ParticleEventScript);Output->SetUsageId(*EventUsage);Output->Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(),TEXT("Out")));}else if(auto* Input=Cast<UNiagaraNodeInput>(N)){Input->Input=FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(),TEXT("InputMap"));Input->Usage=ENiagaraInputNodeUsage::Parameter;}}if(Signature){auto* Custom=Cast<UNiagaraNodeCustomHlsl>(N);if(!Custom)return nullptr;Custom->ScriptUsage=ENiagaraScriptUsage::DynamicInput;Custom->Signature=*Signature;}FGraphEditPlan Add;Add.AddedNode=N;if(!GraphEdit(W,G,TEXT("Graph_AddNode"),Add,[&]{G->AddNode(N,false,false);}))return nullptr;FGraphEditPlan Init;Init.MutableNode=N;if(!GraphEdit(W,G,TEXT("Node_PostPlacedNewNode"),Init,[&]{N->PostPlacedNewNode();}))return nullptr;if(!GraphEdit(W,G,TEXT("Node_AllocateDefaultPins"),Init,[&]{N->AllocateDefaultPins();}))return nullptr;return N;
}
bool SchemaLink(FWork& W,UNiagaraGraph* G,UEdGraphPin* Out,UEdGraphPin* In,FGraphEditPlan& Plan,bool Replace=false)
{
 if(!W.PinForWrite(Out)||!W.PinForWrite(In)||Out->Direction!=EGPD_Output||In->Direction!=EGPD_Input||Out->GetOwningNodeUnchecked()->GetGraph()!=G||In->GetOwningNodeUnchecked()->GetGraph()!=G)return false;const UEdGraphSchema_Niagara* Schema=Cast<UEdGraphSchema_Niagara>(G->GetSchema());if(!Schema||Schema->GetClass()!=UEdGraphSchema_Niagara::StaticClass()||!W.Pin(Schema->GetClass()))return W.Fail(TEXT("niagara_event_stage_schema_unsupported"),TEXT("Exact stock Niagara schema required."));FPinConnectionResponse Response(CONNECT_RESPONSE_DISALLOW,TEXT(""));if(!W.Step(TEXT("CanCreateConnection"),false,[&]{Response=Schema->CanCreateConnection(Out,In);}))return false;if(Response.Response!=CONNECT_RESPONSE_MAKE&&!(Replace&&Response.Response==CONNECT_RESPONSE_BREAK_OTHERS_B))return W.Fail(TEXT("niagara_event_stage_connection_unsupported"),TEXT("Only direct MAKE or selected input replacement is supported."));
 Plan.Links.Add(Out,Out->LinkedTo);Plan.Links[Out].AddUnique(In);Plan.Links.Add(In,TArray<UEdGraphPin*>{Out});if(Replace)for(auto* Previous:In->LinkedTo)if(Previous!=Out){Plan.Links.Add(Previous,Previous->LinkedTo);Plan.Links[Previous].Remove(In);}bool Linked=false;if(!GraphEdit(W,G,TEXT("TryCreateConnection"),Plan,[&]{Linked=Schema->TryCreateConnection(Out,In);W.Prefix->SetBoolField(TEXT("schema_connection_returned"),Linked);}))return false;return Linked&&Out->LinkedTo.Contains(In)&&In->LinkedTo.Contains(Out)||W.Fail(TEXT("niagara_event_stage_connection_failed"),TEXT("Schema did not retain exact reciprocal edge."));
}


int32 FindEventHandler(FWork& W,int32 EmitterIndex,UNiagaraScript* Script,const FGuid& Id)
{
 if(!W.Admit(TEXT("find_exact_event"))||!W.Emitters.IsValidIndex(EmitterIndex))return INDEX_NONE;auto* Data=W.Emitters[EmitterIndex].Data;if(!Data)return INDEX_NONE;int32 Found=INDEX_NONE;const auto& Events=Data->GetEventHandlers();for(int32 I=0;I<Events.Num();++I)if(Events[I].Script==Script&&Script->GetUsage()==ENiagaraScriptUsage::ParticleEventScript&&Script->GetUsageId()==Id){if(Found!=INDEX_NONE){W.Fail(TEXT("niagara_event_stage_event_ambiguous"),TEXT("Exact event script/id occurs more than once."));return INDEX_NONE;}Found=I;}return Found;
}
TSharedPtr<FJsonObject> EventRecoveryError(FWork& W,const FString& Message,const FString& Code,const TSharedPtr<FJsonObject>& Wait=nullptr,bool SaveUnknown=false)
{
 if(W.R.Error.IsEmpty())W.Fail(*Code,Message);auto O=W.Error(Message);O->SetStringField(TEXT("error_code"),Code);O->SetBoolField(TEXT("saved"),W.R.bSaved);O->SetBoolField(TEXT("mutation_performed"),W.R.bChanged);O->SetBoolField(TEXT("mutation_applied_in_memory"),W.R.bMutationAttempted);O->SetBoolField(TEXT("save_state_unknown"),SaveUnknown);O->SetBoolField(TEXT("requires_package_reload"),true);O->SetBoolField(TEXT("requires_reload_or_undo"),true);O->SetBoolField(TEXT("undo_recovery_unproven"),true);O->SetBoolField(TEXT("do_not_retry"),true);O->SetStringField(TEXT("recovery_policy"),TEXT("reload_exact_package_before_retry"));if(Wait)O->SetObjectField(TEXT("compile_wait"),Wait);return O;
}
void TryRemoveOwnedEvent(FWork& W,const FResolvedNiagaraEmitterTarget& Target,UNiagaraScript* Script,const FGuid& Id)
{
 W.Prefix->SetBoolField(TEXT("event_registration_cleanup_attempted"),false);
 // An unsafe/failed admission is not temporarily cleared to perform SDK cleanup.
 if(!W.R.Error.IsEmpty()||!W.Admit(TEXT("event_cleanup_admission")))return;int32 Index=FindEventHandler(W,Target.Index,Script,Id);if(Index==INDEX_NONE)return;
 int32 ExactMatches=0;for(const auto& Handler:Target.Data->GetEventHandlers()){auto* Candidate=Handler.Script.Get();if(!Live(Candidate)){W.Prefix->SetBoolField(TEXT("event_registration_cleanup_skipped_unsafe_handler"),true);return;}const FGuid CandidateId=Candidate->GetUsageId();if(CandidateId==Id){if(Candidate!=Script){W.Prefix->SetBoolField(TEXT("event_registration_cleanup_skipped_ambiguous_usage"),true);return;}++ExactMatches;}}if(ExactMatches!=1){W.Prefix->SetBoolField(TEXT("event_registration_cleanup_skipped_ambiguous_usage"),true);return;}
 const FString Expected=EventStateKey(Target.Data,nullptr,Index);if(!W.SDK(TEXT("RemoveEventHandlerByUsageId"),true))return;W.Emitters[Target.Index].EventKey=Expected;W.Prefix->SetBoolField(TEXT("event_registration_cleanup_attempted"),true);auto Row=W.MutationRow(TEXT("RemoveEventHandlerByUsageId"));Row->SetStringField(TEXT("usage_id"),Id.ToString());Row->SetStringField(TEXT("script"),Script->GetPathName());Target.Emitter->RemoveEventHandlerByUsageId(Id,Target.Version);Row->SetBoolField(TEXT("SDK_returned"),true);if(!W.After(TEXT("RemoveEventHandlerByUsageId")))return;W.Prefix->SetBoolField(TEXT("event_registration_cleanup_verified"),FindEventHandler(W,Target.Index,Script,Id)==INDEX_NONE);
}
UNiagaraNodeOutput* CreateEventGraph(FWork& W,UNiagaraGraph* Graph,const FGuid& Id)
{
 auto* Output=Cast<UNiagaraNodeOutput>(CreateNode(W,Graph,UNiagaraNodeOutput::StaticClass(),400,0,nullptr,&Id));if(!Output)return nullptr;
 auto* Input=Cast<UNiagaraNodeInput>(CreateNode(W,Graph,UNiagaraNodeInput::StaticClass(),0,0,nullptr,&Id));if(!Input)return nullptr;
 UEdGraphPin* OutPin=nullptr;UEdGraphPin* InPin=nullptr;if(!W.Admit(TEXT("event_graph_pins")))return nullptr;for(auto* P:Output->Pins)if(P&&P->Direction==EGPD_Input){InPin=P;break;}for(auto* P:Input->Pins)if(P&&P->Direction==EGPD_Output){OutPin=P;break;}if(!InPin||!OutPin)return nullptr;
 FGraphEditPlan Plan;if(!SchemaLink(W,Graph,OutPin,InPin,Plan))return nullptr;return Output;
}
bool VerifyAuthoredEvent(FWork& W,UNiagaraGraph* Graph,UNiagaraNodeOutput* Output,UNiagaraScript* Script,const FGuid& Id,const FNiagaraEventScriptProperties& Actual,const FNiagaraEventScriptProperties& Expected)
{
 if(!W.Admit(TEXT("event_typed_readback"))||!Live(Script)||!Live(Output)||Output->GetGraph()!=Graph||!Graph->Nodes.Contains(Output)||Output->GetUsage()!=ENiagaraScriptUsage::ParticleEventScript||Output->GetUsageId()!=Id||Script->GetUsage()!=ENiagaraScriptUsage::ParticleEventScript||Script->GetUsageId()!=Id||Actual.Script!=Expected.Script||Actual.ExecutionMode!=Expected.ExecutionMode||Actual.SpawnNumber!=Expected.SpawnNumber||Actual.MaxEventsPerFrame!=Expected.MaxEventsPerFrame||Actual.SourceEmitterID!=Expected.SourceEmitterID||!Actual.SourceEventName.ToString().Equals(Expected.SourceEventName.ToString(),ESearchCase::CaseSensitive)||Actual.bRandomSpawnNumber!=Expected.bRandomSpawnNumber||Actual.MinSpawnNumber!=Expected.MinSpawnNumber||Actual.UpdateAttributeInitialValues!=Expected.UpdateAttributeInitialValues)return false;
 if(Output->Outputs.Num()!=1||Output->Outputs[0].GetType()!=FNiagaraTypeDefinition::GetParameterMapDef())return false;UEdGraphPin* InputPin=nullptr;for(auto* P:Output->Pins)if(P&&P->Direction==EGPD_Input){InputPin=P;break;}if(!InputPin||!FindExpectedPin(W,InputPin)||InputPin->LinkedTo.Num()!=1)return false;auto* Other=InputPin->LinkedTo[0];if(!FindExpectedPin(W,Other))return false;auto* Input=Cast<UNiagaraNodeInput>(Other->GetOwningNodeUnchecked());return Input&&Input->GetGraph()==Graph&&Input->Input.GetType()==FNiagaraTypeDefinition::GetParameterMapDef()&&Input->Usage==ENiagaraInputNodeUsage::Parameter&&Other->LinkedTo.Contains(InputPin)&&W.Admit(TEXT("event_readback_verified"));
}

bool NiagaraCompileWaitAllowsSave(
    const TSharedPtr<FJsonObject>& CompileWait,
    FString& OutFailureReason)
{
    if (!CompileWait.IsValid())
    {
        OutFailureReason = TEXT("compile wait result is missing");
        return false;
    }

    bool bWaitTimedOut = true;
    bool bOutstanding = true;
    bool bActiveCompilations = true;
    CompileWait->TryGetBoolField(TEXT("wait_timed_out"), bWaitTimedOut);
    CompileWait->TryGetBoolField(TEXT("outstanding_compilation_requests_after"), bOutstanding);
    CompileWait->TryGetBoolField(TEXT("active_compilations_after"), bActiveCompilations);

    TArray<FString> Reasons;
    if (bWaitTimedOut)
    {
        Reasons.Add(TEXT("compile wait timed out"));
    }
    if (bOutstanding)
    {
        Reasons.Add(TEXT("outstanding compile requests remain"));
    }
    if (bActiveCompilations)
    {
        Reasons.Add(TEXT("active compilations remain"));
    }

    const TSharedPtr<FJsonObject>* CompileStatus = nullptr;
    if (!CompileWait->TryGetObjectField(TEXT("compile_status"), CompileStatus) ||
        !CompileStatus || !CompileStatus->IsValid())
    {
        Reasons.Add(TEXT("compile status snapshot is missing"));
    }
    else
    {
        bool bCompileStatusSuccess = false;
        double ErrorCount = 0.0;
        double DirtyCount = 0.0;
        double MissingCount = 0.0;
        const bool bHasRequiredFields =
            (*CompileStatus)->TryGetBoolField(TEXT("success"), bCompileStatusSuccess) &&
            (*CompileStatus)->TryGetNumberField(TEXT("error_count"), ErrorCount) &&
            (*CompileStatus)->TryGetNumberField(TEXT("dirty_count"), DirtyCount) &&
            (*CompileStatus)->TryGetNumberField(TEXT("missing_count"), MissingCount);
        if (!bHasRequiredFields || !bCompileStatusSuccess)
        {
            Reasons.Add(TEXT("compile status snapshot is incomplete or unsuccessful"));
        }
        else
        {
            if (ErrorCount > 0.0)
            {
                Reasons.Add(FString::Printf(TEXT("compile error count is %.0f"), ErrorCount));
            }
            if (DirtyCount > 0.0)
            {
                Reasons.Add(FString::Printf(TEXT("dirty script count is %.0f"), DirtyCount));
            }
            if (MissingCount > 0.0)
            {
                Reasons.Add(FString::Printf(TEXT("missing script count is %.0f"), MissingCount));
            }
        }
    }
    OutFailureReason = FString::Join(Reasons, TEXT("; "));
    return Reasons.IsEmpty();
}

TSharedPtr<FJsonObject> IndependentCompileStatus(FWork& W,UNiagaraSystem* System)
{
 auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("system_path"),System->GetPathName());P->SetBoolField(TEXT("request_compile"),false);P->SetBoolField(TEXT("wait_for_completion"),false);P->SetBoolField(TEXT("force"),false);P->SetBoolField(TEXT("allow_source_compile"),false);
 if(!W.SDK(TEXT("independent_compile_status")))return nullptr;
 const FString Encoded=NiagaraAuthoringInspection::InspectCompileStatus(Json(P));
 TSharedPtr<FJsonObject> Typed;const bool Parsed=ParseParams(Encoded,Typed);
 if(!Parsed){W.R.Data->SetStringField(TEXT("independent_compile_status_invalid_raw"),Encoded.Left(1024*1024));W.R.Data->SetBoolField(TEXT("independent_compile_status_raw_truncated"),Encoded.Len()>1024*1024);}
 const TSharedPtr<FJsonObject>* ChildData=nullptr;const TSharedPtr<FJsonObject>* Legacy=nullptr;
 if(Parsed)
 {
  auto Receipt=MakeShared<FJsonObject>(*Typed);
  if(Typed->TryGetObjectField(TEXT("data"),ChildData)&&ChildData&&ChildData->IsValid())
  {
   auto Data=MakeShared<FJsonObject>(**ChildData);Data->RemoveField(TEXT("legacy_result"));Data->SetStringField(TEXT("legacy_result_reference"),TEXT("legacy_result.compile_wait.compile_status"));Receipt->SetObjectField(TEXT("data"),Data);
   (*ChildData)->TryGetObjectField(TEXT("legacy_result"),Legacy);
  }
  W.R.Data->SetObjectField(TEXT("independent_compile_status_receipt"),Receipt);
  bool V=false;if(Typed->TryGetBoolField(TEXT("mutation_attempted"),V))W.R.bMutationAttempted|=V;
  V=false;if(Typed->TryGetBoolField(TEXT("compile_attempted"),V))W.R.bCompileAttempted|=V;
  V=false;if(Typed->TryGetBoolField(TEXT("save_attempted"),V))W.R.bSaveAttempted|=V;
  V=false;if(Typed->TryGetBoolField(TEXT("saved"),V))W.R.bSaved|=V;
 }
 if(!W.After(TEXT("independent_compile_status")))return nullptr;
 if(!Parsed||!Legacy||!Legacy->IsValid()){W.Fail(TEXT("niagara_event_stage_compile_status_invalid"),TEXT("Dedicated status did not return a bounded typed legacy result; actual prefix retained."));return nullptr;}
 bool Health=false;Typed->TryGetBoolField(TEXT("success"),Health);
 if(!Health){auto Failure=MakeShared<FJsonObject>(**Legacy);Failure->SetBoolField(TEXT("success"),false);return Failure;}
 return *Legacy;
}

struct FEventCompileWait
{
 TSharedPtr<FJsonObject> Result=MakeShared<FJsonObject>();
 double Started=0,Next=0,Timeout=0,Poll=.1;int32 Iterations=0;bool Done=false;
 FEventCompileWait(double Seconds,double Interval):Started(FPlatformTime::Seconds()),Timeout(FMath::Clamp(Seconds,0.0,60.0)),Poll(FMath::Clamp(Interval,.02,1.0))
 {Result->SetBoolField(TEXT("wait_for_completion"),true);Result->SetNumberField(TEXT("timeout_seconds"),Timeout);Result->SetNumberField(TEXT("poll_interval_seconds"),Poll);Result->SetBoolField(TEXT("wait_timed_out"),false);Result->SetBoolField(TEXT("blocking_poll_or_pump_invoked"),false);Result->SetBoolField(TEXT("finite_GT_ticker_wait"),true);}
 bool Tick(FWork& W,UNiagaraSystem* System)
 {
  if(Done)return true;
  const double Now=FPlatformTime::Seconds();if(Now<Next)return false;
  bool Outstanding=false,Active=false;
  auto Observe=[&]()
  {
   if(!W.SDK(TEXT("async_wait_HasOutstandingCompilationRequests")))return false;
   Outstanding=System->HasOutstandingCompilationRequests(true);Result->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);
   if(!W.After(TEXT("async_wait_HasOutstandingCompilationRequests")))return false;
   if(!W.SDK(TEXT("async_wait_HasActiveCompilations")))return false;
   Active=System->HasActiveCompilations();Result->SetBoolField(TEXT("active_compilations_after"),Active);
   return W.After(TEXT("async_wait_HasActiveCompilations"));
  };
  if(!Observe()){Done=true;return true;}
  if(Outstanding||Active)
  {
   if(!W.SDK(TEXT("async_wait_PollForCompilationComplete"))){Done=true;return true;}
   System->PollForCompilationComplete();
   if(!W.After(TEXT("async_wait_PollForCompilationComplete"))||!Observe()){Done=true;return true;}
  }
  const double Elapsed=FPlatformTime::Seconds()-Started;
  const bool TimedOut=(Outstanding||Active)&&Elapsed>=Timeout;
  Result->SetNumberField(TEXT("wait_elapsed_seconds"),Elapsed);Result->SetBoolField(TEXT("wait_timed_out"),TimedOut);
  if((!Outstanding&&!Active)||TimedOut){Done=true;Result->SetNumberField(TEXT("wait_iterations"),Iterations);Result->SetStringField(TEXT("wait_status"),TimedOut?TEXT("bounded_timeout_observed"):TEXT("idle_observed"));return true;}
  ++Iterations;Result->SetNumberField(TEXT("wait_iterations"),Iterations);
  Next=FPlatformTime::Seconds()+Poll;return false;
 }
};
void BeginEventWait(FWork& W,UNiagaraSystem* System,double Timeout,double Poll,TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Completion)
{
 auto Wait=MakeShared<FEventCompileWait>(Timeout,Poll);
 W.R.Data->SetObjectField(TEXT("compile_wait_partial"),Wait->Result);
 W.DeferredStep=[&W,System,Wait,Completion=MoveTemp(Completion)]()mutable
 {
  if(!Wait->Tick(W,System))return false;
  if(!W.Admit(TEXT("event_wait_completion_admission")))return true;
  TSharedPtr<FJsonObject> Legacy=Completion(Wait->Result);
  if(Legacy.IsValid())W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
  return true;
 };
}

TSharedPtr<FJsonObject> HandleStages(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bIncludeCompileData = true;
    Params->TryGetBoolField(TEXT("include_compile_data"), bIncludeCompileData);

    bool bIncludeScriptCompileStatus = true;
    Params->TryGetBoolField(TEXT("include_script_compile_status"), bIncludeScriptCompileStatus);

    int32 MaxStages = 128;
    MaxStages=IndexParam(Params,TEXT("max_stages"),128);if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);
    MaxStages = FMath::Clamp(MaxStages, 0, 1024);

    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    int32 TotalStageCount = 0;
    int32 IncludedStageCount = 0;
    bool bStagesTruncated = false;

    int32 EmitterIndex = 0;
    const TArray<FNiagaraEmitterHandle> Handles=System->GetEmitterHandles();
    for (const FNiagaraEmitterHandle& EmitterHandle : Handles)
    {
        TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
        const FString EmitterName = EmitterHandle.GetName().ToString();
        EmitterObject->SetStringField(TEXT("name"), EmitterName);
        EmitterObject->SetNumberField(TEXT("emitter_index"), EmitterIndex);
        EmitterObject->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());

        TArray<TSharedPtr<FJsonValue>> StageValues;
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            EmitterObject->SetStringField(TEXT("sim_target"), NiagaraEnumValueName(EmitterData->SimTarget));
            const TArray<UNiagaraSimulationStageBase*> SimulationStages = EmitterData->GetSimulationStages();
            EmitterObject->SetNumberField(TEXT("simulation_stage_count"), SimulationStages.Num());
            TotalStageCount += SimulationStages.Num();

            for (int32 StageIndex = 0; StageIndex < SimulationStages.Num(); ++StageIndex)
            {
                UNiagaraSimulationStageBase* Stage = SimulationStages[StageIndex];
                if (!Stage)
                {
                    continue;
                }
                if (IncludedStageCount >= MaxStages)
                {
                    bStagesTruncated = true;
                    break;
                }

                TSharedPtr<FJsonObject> StageObject = MakeShared<FJsonObject>();
                StageObject->SetNumberField(TEXT("stage_index"), StageIndex);
                StageObject->SetStringField(TEXT("stage_name"), Stage->SimulationStageName.ToString());
                StageObject->SetStringField(TEXT("stage_path"), Stage->GetPathName());
                StageObject->SetStringField(TEXT("stage_class"), Stage->GetClass() ? Stage->GetClass()->GetName() : FString());
                StageObject->SetBoolField(TEXT("enabled"), Stage->bEnabled != 0);
                StageObject->SetStringField(TEXT("script_path"), Stage->Script ? Stage->Script->GetPathName() : FString());

                if (bIncludeScriptCompileStatus)
                {
                    StageObject->SetObjectField(
                        TEXT("script_compile_status"),
                        BuildNiagaraScriptCompileStatusJson(
                            Stage->Script,
                            FString::Printf(TEXT("emitter:%d:stage:%d"), EmitterIndex, StageIndex),
                            EmitterName,
                            StageIndex));
                }

                if(!ContinueRead())return W.Error(W.R.Error);
                if (UNiagaraSimulationStageGeneric* GenericStage = Cast<UNiagaraSimulationStageGeneric>(Stage))
                {
                    StageObject->SetBoolField(TEXT("is_generic_stage"), true);
                    StageObject->SetStringField(TEXT("iteration_source"), NiagaraEnumValueName(GenericStage->IterationSource));
                    StageObject->SetStringField(TEXT("execute_behavior"), NiagaraEnumValueName(GenericStage->ExecuteBehavior));
                    StageObject->SetBoolField(TEXT("disable_partial_particle_update"), GenericStage->bDisablePartialParticleUpdate != 0);
                    StageObject->SetBoolField(TEXT("particle_iteration_state_enabled"), GenericStage->bParticleIterationStateEnabled != 0);
                    StageObject->SetArrayField(TEXT("particle_iteration_state_range"), IntArrayToJsonValues({
                        GenericStage->ParticleIterationStateRange.X,
                        GenericStage->ParticleIterationStateRange.Y
                    }));
                    StageObject->SetBoolField(TEXT("gpu_dispatch_force_linear"), GenericStage->bGpuDispatchForceLinear != 0);
                    StageObject->SetBoolField(TEXT("override_gpu_dispatch_num_threads"), GenericStage->bOverrideGpuDispatchNumThreads != 0);
                    StageObject->SetStringField(TEXT("direct_dispatch_type"), NiagaraEnumValueName(GenericStage->DirectDispatchType));
                    StageObject->SetStringField(TEXT("direct_dispatch_element_type"), NiagaraEnumValueName(GenericStage->DirectDispatchElementType));

                    StageObject->SetObjectField(TEXT("enabled_binding"), NiagaraAttributeBindingToJsonObject(GenericStage->EnabledBinding));
                    StageObject->SetObjectField(TEXT("data_interface"), NiagaraDataInterfaceBindingToJsonObject(GenericStage->DataInterface));
                    StageObject->SetObjectField(TEXT("particle_iteration_state_binding"), NiagaraAttributeBindingToJsonObject(GenericStage->ParticleIterationStateBinding));
                    StageObject->SetObjectField(TEXT("num_iterations"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->NumIterations));
                    StageObject->SetObjectField(TEXT("element_count_x"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->ElementCountX));
                    StageObject->SetObjectField(TEXT("element_count_y"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->ElementCountY));
                    StageObject->SetObjectField(TEXT("element_count_z"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->ElementCountZ));
                    StageObject->SetObjectField(TEXT("override_gpu_dispatch_num_threads_x"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->OverrideGpuDispatchNumThreadsX));
                    StageObject->SetObjectField(TEXT("override_gpu_dispatch_num_threads_y"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->OverrideGpuDispatchNumThreadsY));
                    StageObject->SetObjectField(TEXT("override_gpu_dispatch_num_threads_z"), NiagaraParameterBindingWithValueToJsonObject(GenericStage->OverrideGpuDispatchNumThreadsZ));

#if WITH_EDITORONLY_DATA
                    if (bIncludeCompileData)
                    {
                        TArray<FNiagaraSimulationStageCompilationData> CompilationData;
                        if(!GenericStage->Script||!Live(GenericStage->Script))return W.Error(TEXT("Generic stage compile-data requires an exact live Script; public virtual dereferences it."));
                        bool bFilledCompilationData=false;if(!W.Step(TEXT("FillStageCompilationData"),false,[&]{bFilledCompilationData=GenericStage->FillCompilationData(CompilationData);}))return W.Error(W.R.Error);if(CompilationData.Num()>4096||!W.Budget(CompilationData.Num()))return W.Error(TEXT("Compile data exceeds bound."));
                        TArray<TSharedPtr<FJsonValue>> CompileDataValues;
                        for (const FNiagaraSimulationStageCompilationData& CompileData : CompilationData)
                        {
                            CompileDataValues.Add(MakeShared<FJsonValueObject>(NiagaraSimStageCompileDataToJsonObject(CompileData)));
                        }
                        StageObject->SetBoolField(TEXT("filled_compile_data"), bFilledCompilationData);
                        StageObject->SetNumberField(TEXT("compile_data_count"), CompileDataValues.Num());
                        StageObject->SetArrayField(TEXT("compile_data"), CompileDataValues);
                    }
#else
                    StageObject->SetBoolField(TEXT("compile_data_unavailable"), bIncludeCompileData);
#endif
                }
                else
                {
                    StageObject->SetBoolField(TEXT("is_generic_stage"), false);
                    StageObject->SetStringField(TEXT("inspection_note"), TEXT("Only generic simulation-stage fields are expanded"));
                }

                if(!ContinueRead()||!W.Budget(1,Json(StageObject).Len()))return W.Error(W.R.Error);StageValues.Add(MakeShared<FJsonValueObject>(StageObject));
                ++IncludedStageCount;
            }
        }
        else
        {
            EmitterObject->SetNumberField(TEXT("simulation_stage_count"), 0);
            EmitterObject->SetStringField(TEXT("inspection_error"), TEXT("Emitter data unavailable"));
        }

        EmitterObject->SetArrayField(TEXT("simulation_stages"), StageValues);
        EmitterObject->SetNumberField(TEXT("included_simulation_stage_count"), StageValues.Num());
        EmitterValues.Add(MakeShared<FJsonValueObject>(EmitterObject));
        ++EmitterIndex;

        if (bStagesTruncated)
        {
            break;
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetBoolField(TEXT("include_compile_data"), bIncludeCompileData);
    Result->SetBoolField(TEXT("include_script_compile_status"), bIncludeScriptCompileStatus);
    Result->SetNumberField(TEXT("max_stages"), MaxStages);
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("simulation_stage_count"), TotalStageCount);
    Result->SetNumberField(TEXT("included_simulation_stage_count"), IncludedStageCount);
    Result->SetBoolField(TEXT("stages_truncated"), bStagesTruncated);
    Result->SetArrayField(TEXT("emitters"), EmitterValues);
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),W.Admit(TEXT("stage_query_complete")));return Result;
}
TSharedPtr<FJsonObject> HandleHandlers(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    TargetEmitterIndex=IndexParam(Params,TEXT("emitter_index"));if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    TargetEmitterName.TrimStartAndEndInline();

    int32 MaxHandlers = 256;
    FString MaxHandlersError;
    if (TryGetJsonIntField(Params, TEXT("max_handlers"), MaxHandlers, MaxHandlersError))
    {
        MaxHandlers = FMath::Clamp(MaxHandlers, 1, 1024);
    }
    if (!MaxHandlersError.IsEmpty())
    {
        return W.Error(MaxHandlersError);
    }

    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    int32 MatchedEmitterCount = 0;
    int32 TotalHandlerCount = 0;
    int32 ReportedHandlerCount = 0;
    int32 CurrentEmitterIndex = 0;
    const TArray<FNiagaraEmitterHandle> Handles=System->GetEmitterHandles();
    for (const FNiagaraEmitterHandle& Handle : Handles)
    {
        const FString EmitterName = Handle.GetName().ToString();
        const bool bMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));
        if (!bMatches)
        {
            ++CurrentEmitterIndex;
            continue;
        }

        ++MatchedEmitterCount;
        TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> HandlerValues;
        const FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
        const TArray<FNiagaraEventScriptProperties>* EventHandlers = EmitterData ? &EmitterData->GetEventHandlers() : nullptr;
        const int32 EmitterHandlerCount = EventHandlers ? EventHandlers->Num() : 0;
        TotalHandlerCount += EmitterHandlerCount;

        if (EventHandlers)
        {
            for (int32 HandlerIndex = 0; HandlerIndex < EventHandlers->Num() && ReportedHandlerCount < MaxHandlers; ++HandlerIndex)
            {
                HandlerValues.Add(MakeShared<FJsonValueObject>(BuildNiagaraEventHandlerJson(
                    System,
                    CurrentEmitterIndex,
                    EmitterName,
                    FNiagaraEventScriptProperties((*EventHandlers)[HandlerIndex]),
                    HandlerIndex)));
                if(!ContinueRead()||!W.Budget(1,Json(HandlerValues.Last()->AsObject()).Len()))return W.Error(W.R.Error);++ReportedHandlerCount;
            }
        }

        EmitterObject->SetStringField(TEXT("name"), EmitterName);
        EmitterObject->SetNumberField(TEXT("emitter_index"), CurrentEmitterIndex);
        EmitterObject->SetStringField(TEXT("emitter_id"), Handle.GetId().ToString(EGuidFormats::DigitsWithHyphens));
        EmitterObject->SetStringField(TEXT("sim_target"), EmitterData ? NiagaraEnumValueName(EmitterData->SimTarget) : FString());
        EmitterObject->SetBoolField(TEXT("is_allowed_to_execute"), EmitterData && EmitterData->IsAllowedToExecute());
        EmitterObject->SetNumberField(TEXT("handler_count"), EmitterHandlerCount);
        EmitterObject->SetArrayField(TEXT("handlers"), HandlerValues);
        EmitterValues.Add(MakeShared<FJsonValueObject>(EmitterObject));
        ++CurrentEmitterIndex;
    }

    if ((TargetEmitterIndex != INDEX_NONE || !TargetEmitterName.IsEmpty()) && MatchedEmitterCount == 0)
    {
        return W.Error(TEXT("No matching Niagara emitter was found"));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetNumberField(TEXT("matched_emitter_count"), MatchedEmitterCount);
    Result->SetNumberField(TEXT("total_handler_count"), TotalHandlerCount);
    Result->SetNumberField(TEXT("reported_handler_count"), ReportedHandlerCount);
    Result->SetNumberField(TEXT("max_handlers"), MaxHandlers);
    Result->SetBoolField(TEXT("truncated"), ReportedHandlerCount < TotalHandlerCount);
    Result->SetArrayField(TEXT("emitters"), EmitterValues);
    TArray<TSharedPtr<FJsonValue>> ExecutionOrderValues;
    const TArray<FNiagaraEmitterExecutionIndex> ExecutionOrder(System->GetEmitterExecutionOrder());if(!W.Budget(ExecutionOrder.Num()))return W.Error(W.R.Error);
    for (const FNiagaraEmitterExecutionIndex& ExecutionIndex : ExecutionOrder)
    {
        TSharedPtr<FJsonObject> ExecutionObject = MakeShared<FJsonObject>();
        ExecutionObject->SetNumberField(TEXT("emitter_index"), ExecutionIndex.EmitterIndex);
        ExecutionObject->SetBoolField(TEXT("start_new_overlap_group"), ExecutionIndex.bStartNewOverlapGroup);
        ExecutionOrderValues.Add(MakeShared<FJsonValueObject>(ExecutionObject));
    }
    Result->SetArrayField(TEXT("emitter_execution_order"), ExecutionOrderValues);
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetStringField(TEXT("source_policy"), TEXT("read_only_any_niagara_system"));
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),W.Admit(TEXT("handler_query_complete")));return Result;
#else
    return W.Error(TEXT("inspect_niagara_event_handlers requires editor-only Niagara data"));
#endif
}
TSharedPtr<FJsonObject> HandleAdd(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to add a Niagara Event Handler outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    TargetEmitterIndex=IndexParam(Params,TEXT("emitter_index"));if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    TargetEmitterName.TrimStartAndEndInline();

    int32 SourceEmitterIndex = INDEX_NONE;
    FString SourceEmitterName;
    FString SourceEmitterIdText;
    SourceEmitterIndex=IndexParam(Params,TEXT("source_emitter_index"));if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);
    Params->TryGetStringField(TEXT("source_emitter_name"), SourceEmitterName);
    Params->TryGetStringField(TEXT("source_emitter_id"), SourceEmitterIdText);
    SourceEmitterName.TrimStartAndEndInline();
    SourceEmitterIdText.TrimStartAndEndInline();
    const bool bHasSourceSelector =
        SourceEmitterIndex != INDEX_NONE || !SourceEmitterName.IsEmpty() || !SourceEmitterIdText.IsEmpty();

    FString SourceEventName = TEXT("CollisionEvent");
    Params->TryGetStringField(TEXT("source_event_name"), SourceEventName);
    SourceEventName.TrimStartAndEndInline();
    if (SourceEventName.IsEmpty())
    {
        return W.Error(TEXT("source_event_name cannot be empty"));
    }

    FString ExecutionModeText = TEXT("SpawnedParticles");
    Params->TryGetStringField(TEXT("execution_mode"), ExecutionModeText);
    EScriptExecutionMode ExecutionMode = EScriptExecutionMode::SpawnedParticles;
    if (!TryParseEventHandlerExecutionMode(ExecutionModeText, ExecutionMode))
    {
        return W.Error(
            FString::Printf(TEXT("Unsupported execution_mode '%s'; use EveryParticle or SpawnedParticles"), *ExecutionModeText));
    }

    FString IntegerError;
    int32 SpawnNumber = ExecutionMode == EScriptExecutionMode::SpawnedParticles ? 1 : 0;
    TryGetJsonIntField(Params, TEXT("spawn_number"), SpawnNumber, IntegerError);
    if (!IntegerError.IsEmpty())
    {
        return W.Error(IntegerError);
    }
    int32 MaxEventsPerFrame = 64;
    TryGetJsonIntField(Params, TEXT("max_events_per_frame"), MaxEventsPerFrame, IntegerError);
    if (!IntegerError.IsEmpty())
    {
        return W.Error(IntegerError);
    }
    int32 MinSpawnNumber = 0;
    TryGetJsonIntField(Params, TEXT("min_spawn_number"), MinSpawnNumber, IntegerError);
    if (!IntegerError.IsEmpty())
    {
        return W.Error(IntegerError);
    }
    if (SpawnNumber < 0 || SpawnNumber > 100000)
    {
        return W.Error(TEXT("spawn_number must be between 0 and 100000"));
    }
    if (ExecutionMode == EScriptExecutionMode::SpawnedParticles && SpawnNumber < 1)
    {
        return W.Error(TEXT("SpawnedParticles execution requires spawn_number >= 1"));
    }
    if (ExecutionMode == EScriptExecutionMode::EveryParticle && SpawnNumber != 0)
    {
        return W.Error(TEXT("EveryParticle execution requires spawn_number=0"));
    }
    if (MaxEventsPerFrame < 0 || MaxEventsPerFrame > 1000000)
    {
        return W.Error(TEXT("max_events_per_frame must be between 0 and 1000000"));
    }
    if (MinSpawnNumber < 0 || MinSpawnNumber > SpawnNumber)
    {
        return W.Error(TEXT("min_spawn_number must be between 0 and spawn_number"));
    }

    bool bRandomSpawnNumber = false;
    Params->TryGetBoolField(TEXT("random_spawn_number"), bRandomSpawnNumber);
    bool bUpdateAttributeInitialValues = true;
    Params->TryGetBoolField(TEXT("update_attribute_initial_values"), bUpdateAttributeInitialValues);
    bool bSkipIfDuplicate = true;
    Params->TryGetBoolField(TEXT("skip_if_duplicate"), bSkipIfDuplicate);
    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bWaitForCompletion = false;
    double TimeoutSeconds = 10.0;
    double PollIntervalSeconds = 0.1;
    ReadNiagaraCompileWaitParams(Params, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds);
    if (bSave && (!bRequestCompile || !bWaitForCompletion))
    {
        return W.Error(
            TEXT("save=true requires request_compile=true and wait_for_completion=true for Event Handler creation"));
    }

    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(System->GetPathName()))
    {
        return W.Error(
            FString::Printf(TEXT("Resolved Niagara system is outside %s: %s"), NiagaraTempGenerationRoot, *System->GetPathName()));
    }

    FResolvedNiagaraEmitterTarget Target;
    int32 TargetMatchCount = 0;
    if (!ResolveSingleNiagaraEmitterTarget(System, TargetEmitterIndex, TargetEmitterName, Target, TargetMatchCount))
    {
        if (TargetMatchCount == 0)
        {
            return W.Error(TEXT("No matching target Niagara emitter was found"));
        }
        return W.Error(TEXT("Target emitter selector matched more than one emitter; provide emitter_index or emitter_name"));
    }
    if (!Target.Emitter || !Target.Data)
    {
        return W.Error(TEXT("Matched target Niagara emitter is invalid"));
    }
    if (Target.Data->SimTarget != ENiagaraSimTarget::CPUSim)
    {
        return W.Error(
            FString::Printf(TEXT("Niagara Event Handlers require a CPU target emitter; '%s' is %s"), *Target.Name, *NiagaraEnumValueName(Target.Data->SimTarget)));
    }

    const FGuid TargetEmitterId = GetEmitterHandleId(System, Target.Index);
    FGuid StoredSourceEmitterId;
    FGuid CanonicalSourceEmitterId = TargetEmitterId;
    FString ResolvedSourceEmitterName = Target.Name;
    int32 ResolvedSourceEmitterIndex = Target.Index;
    if (bHasSourceSelector)
    {
        FResolvedEventSourceEmitter Source;
        FString SourceResolveError;
        if (!ResolveEventSourceEmitter(
                System,
                SourceEmitterIndex,
                SourceEmitterName,
                SourceEmitterIdText,
                Source,
                SourceResolveError))
        {
            return W.Error(SourceResolveError);
        }
        if (Source.Data->SimTarget != ENiagaraSimTarget::CPUSim)
        {
            return W.Error(
                FString::Printf(TEXT("Niagara event generation requires a CPU source emitter; '%s' is %s"), *Source.Name, *NiagaraEnumValueName(Source.Data->SimTarget)));
        }
        CanonicalSourceEmitterId = Source.HandleId;
        StoredSourceEmitterId = Source.HandleId == TargetEmitterId ? FGuid() : Source.HandleId;
        ResolvedSourceEmitterName = Source.Name;
        ResolvedSourceEmitterIndex = Source.Index;
    }

    UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(Target.Data->GraphSource);
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if (!ScriptSource || !Graph)
    {
        return W.Error(TEXT("Matched target Niagara emitter has no editable script source graph"));
    }

    const FName EventName(*SourceEventName);
    const int32 BeforeHandlerCount = Target.Data->GetEventHandlers().Num();
    for (int32 HandlerIndex = 0; HandlerIndex < Target.Data->GetEventHandlers().Num(); ++HandlerIndex)
    {
        const FNiagaraEventScriptProperties Existing = Target.Data->GetEventHandlers()[HandlerIndex];
        const bool bSameSource = CanonicalEventSourceId(Existing.SourceEmitterID, TargetEmitterId) == CanonicalSourceEmitterId;
        const bool bSameEvent = Existing.SourceEventName.ToString().Equals(SourceEventName, ESearchCase::IgnoreCase);
        if (!bSameSource || !bSameEvent)
        {
            continue;
        }

        if (!bSkipIfDuplicate)
        {
            return W.Error(
                FString::Printf(TEXT("An Event Handler already consumes '%s' from source emitter '%s'"), *SourceEventName, *ResolvedSourceEmitterName));
        }

        const bool bConfigurationMatches =
            Existing.ExecutionMode == ExecutionMode &&
            Existing.SpawnNumber == static_cast<uint32>(SpawnNumber) &&
            Existing.MaxEventsPerFrame == static_cast<uint32>(MaxEventsPerFrame) &&
            Existing.bRandomSpawnNumber == bRandomSpawnNumber &&
            Existing.MinSpawnNumber == static_cast<uint32>(MinSpawnNumber) &&
            Existing.UpdateAttributeInitialValues == bUpdateAttributeInitialValues;
        if (!bConfigurationMatches)
        {
            return W.Error(
                TEXT("A matching Event Handler already exists with different settings; inspect it before applying a separate explicit update"));
        }

        TSharedPtr<FJsonObject> ExistingReadback = BuildNiagaraEventHandlerJson(
            System,
            Target.Index,
            Target.Name,
            Existing,
            HandlerIndex);
        if(!ContinueRead())return W.Error(W.R.Error);
        const bool bExistingHealthy =
            ExistingReadback->GetBoolField(TEXT("output_node_exists")) &&
            ExistingReadback->GetBoolField(TEXT("script_ready_to_run_cpu")) &&
            ExistingReadback->GetBoolField(TEXT("script_compile_succeeded_cpu")) &&
            ExistingReadback->GetBoolField(TEXT("source_event_generator_found"));
        if (!bExistingHealthy)
        {
            TSharedPtr<FJsonObject> Error = W.Error(
                TEXT("A matching Event Handler exists but its graph, compiled CPU script, or source event generator readback is incomplete"));
            Error->SetStringField(TEXT("error_code"), TEXT("niagara_event_handler_duplicate_unhealthy"));W.R.ErrorCode=TEXT("niagara_event_handler_duplicate_unhealthy");
            Error->SetBoolField(TEXT("mutation_performed"), false);
            Error->SetBoolField(TEXT("mutation_applied_in_memory"), false);
            Error->SetBoolField(TEXT("saved"), false);
            Error->SetObjectField(TEXT("event_handler"), ExistingReadback);
            return Error;
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("skipped_duplicate"), true);
        Result->SetStringField(TEXT("skip_reason"), TEXT("event_handler_already_exists_with_matching_configuration"));
        Result->SetStringField(TEXT("system_path"), System->GetPathName());
        Result->SetNumberField(TEXT("handler_count_before"), BeforeHandlerCount);
        Result->SetNumberField(TEXT("handler_count_after"), BeforeHandlerCount);
        Result->SetBoolField(TEXT("compile_requested"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetStringField(TEXT("write_scope"), TEXT("target_temp_event_handler_create_duplicate_skip"));
        Result->SetObjectField(TEXT("event_handler"), ExistingReadback);
        W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;
    }


    // Actual wait is required by a requested compile, not replaced with a zero-work success.
    // A healthy duplicate returned above performs no write/compile/save.
    if(!W.WaitAdmission(bRequestCompile&&bWaitForCompletion,bRequestCompile))return W.Error(W.R.Error);
    if(!W.CaptureGraph(ScriptSource)||!W.GraphForWrite(Graph)||!W.Own(System)||!W.Own(Target.Emitter)||!W.Own(ScriptSource))return W.Error(W.R.Error);
    W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);
    if(!W.Modify(System)||!W.Modify(Target.Emitter)||!W.Modify(ScriptSource)||!W.Modify(Graph))return W.Error(W.R.Error);
    FNiagaraEventScriptProperties EventHandler;EventHandler.ExecutionMode=ExecutionMode;EventHandler.SpawnNumber=static_cast<uint32>(SpawnNumber);EventHandler.MaxEventsPerFrame=static_cast<uint32>(MaxEventsPerFrame);EventHandler.SourceEmitterID=StoredSourceEmitterId;EventHandler.SourceEventName=EventName;EventHandler.bRandomSpawnNumber=bRandomSpawnNumber;EventHandler.MinSpawnNumber=static_cast<uint32>(MinSpawnNumber);EventHandler.UpdateAttributeInitialValues=bUpdateAttributeInitialValues;
    UNiagaraScript* NewScript=nullptr;FName NewName=MakeUniqueObjectName(Target.Emitter,UNiagaraScript::StaticClass(),TEXT("EventScript"));
    if(!W.Step(TEXT("NewObject_EventScript"),true,[&]{NewScript=NewObject<UNiagaraScript>(Target.Emitter,NewName,RF_Transactional);W.Prefix->SetBoolField(TEXT("event_script_returned_nonnull"),NewScript!=nullptr);W.R.bChanged|=NewScript!=nullptr;}))return W.Error(W.R.Error);
    if(!Live(NewScript)||NewScript->GetClass()!=UNiagaraScript::StaticClass()||NewScript->GetOuter()!=Target.Emitter||NewScript->GetFName()!=NewName||!W.Own(NewScript))return W.Error(TEXT("Exact newly allocated event script ownership mismatch."));
    EventHandler.Script=NewScript;const FGuid EventUsageId=FGuid::NewGuid();
    if(!W.Step(TEXT("EventScript_SetUsage"),true,[&]{NewScript->SetUsage(ENiagaraScriptUsage::ParticleEventScript);})||!W.Step(TEXT("EventScript_SetUsageId"),true,[&]{NewScript->SetUsageId(EventUsageId);})||!W.Step(TEXT("EventScript_SetLatestSource"),true,[&]{NewScript->SetLatestSource(ScriptSource);}))return W.Error(W.R.Error);
    if(NewScript->GetUsage()!=ENiagaraScriptUsage::ParticleEventScript||NewScript->GetUsageId()!=EventUsageId||NewScript->GetLatestSource()!=ScriptSource||!W.CaptureScript(NewScript))return W.Error(TEXT("Exact event script usage/id/latest source readback failed."));
    const FString ExpectedEvents=EventStateKey(Target.Data,&EventHandler);if(!W.SDK(TEXT("AddEventHandler"),true))return W.Error(W.R.Error);
    W.Emitters[Target.Index].EventKey=ExpectedEvents;auto AddRow=W.MutationRow(TEXT("AddEventHandler"));AddRow->SetStringField(TEXT("usage_id"),EventUsageId.ToString());AddRow->SetStringField(TEXT("script"),NewScript->GetPathName());
    Target.Emitter->AddEventHandler(EventHandler,Target.Version);AddRow->SetBoolField(TEXT("SDK_returned"),true);W.R.bChanged=true;
    if(!W.After(TEXT("AddEventHandler")))return W.Error(W.R.Error);
    int32 RegisteredIndex=FindEventHandler(W,Target.Index,NewScript,EventUsageId);if(RegisteredIndex==INDEX_NONE)return W.Error(TEXT("Exact registered event script not present after AddEventHandler callbacks."));
    UNiagaraNodeOutput* OutputNode=CreateEventGraph(W,Graph,EventUsageId);
    if(!OutputNode){TryRemoveOwnedEvent(W,Target,NewScript,EventUsageId);return EventRecoveryError(W,TEXT("Event graph initialization failed; actual registration/graph prefix retained."),TEXT("niagara_event_handler_graph_initialization_failed"));}
    if(!W.Step(TEXT("Emitter_PostEditChange"),true,[&]{Target.Emitter->PostEditChange();}))return W.Error(W.R.Error);
    // Removed RefreshSystemParametersFromEmitter has no verified 5.8 public equivalent.
    // Use only the exported execution-order and overview paths; do not claim parameter-sync equivalence.
    if(!W.Step(TEXT("ComputeEmittersExecutionOrder"),true,[&]{System->ComputeEmittersExecutionOrder();}))return W.Error(W.R.Error);
    if(auto* EditorData=Cast<UNiagaraSystemEditorData>(System->GetEditorData())){if(!W.Own(EditorData)||!W.Step(TEXT("SynchronizeOverviewGraphWithSystem"),true,[&]{EditorData->SynchronizeOverviewGraphWithSystem(*System);}))return W.Error(W.R.Error);}
    if(!W.Dirty())return W.Error(W.R.Error);bool bCompileRequested=false;if(bRequestCompile&&!W.Request(bCompileRequested))return W.Error(W.R.Error);
    auto CompleteEvent=[=,&W](const TSharedPtr<FJsonObject>& CompileWait)->TSharedPtr<FJsonObject>
    {
     if(!W.Admit(TEXT("event_post_wait_admission")))return W.Error(W.R.Error);
     if(bRequestCompile&&bWaitForCompletion)
     {
      auto Status=IndependentCompileStatus(W,System);if(!ContinueRead())return W.Error(W.R.Error);if(Status)CompileWait->SetObjectField(TEXT("compile_status"),Status);
      FString Reason;if(!NiagaraCompileWaitAllowsSave(CompileWait,Reason))return EventRecoveryError(W,TEXT("Event Handler compile gate failed; mutation remains unsaved in memory: ")+Reason,TEXT("niagara_event_handler_compile_gate_failed"),CompileWait);
     }
    const int32 AddedHandlerIndex=FindEventHandler(W,Target.Index,NewScript,EventUsageId);if(AddedHandlerIndex==INDEX_NONE)return EventRecoveryError(W,TEXT("Exact event handler absent after compile dispatch."),TEXT("niagara_event_handler_readback_failed"),CompileWait);
    const FNiagaraEventScriptProperties Added=Target.Data->GetEventHandlers()[AddedHandlerIndex];auto EventHandlerReadback=BuildNiagaraEventHandlerJson(System,Target.Index,Target.Name,Added,AddedHandlerIndex);if(!ContinueRead())return W.Error(W.R.Error);
    if(!VerifyAuthoredEvent(W,Graph,OutputNode,NewScript,EventUsageId,Added,EventHandler))return EventRecoveryError(W,TEXT("Typed handler/output/schema edge differs from requested exact target."),TEXT("niagara_event_handler_readback_failed"),CompileWait);
    if(bSave){if(!EventHandlerReadback->GetBoolField(TEXT("output_node_exists"))||!EventHandlerReadback->GetBoolField(TEXT("script_ready_to_run_cpu"))||!EventHandlerReadback->GetBoolField(TEXT("script_compile_succeeded_cpu"))||!EventHandlerReadback->GetBoolField(TEXT("source_event_generator_found")))return EventRecoveryError(W,TEXT("Output/CPU compile/source generator save gate is incomplete."),TEXT("niagara_event_handler_save_readback_failed"),CompileWait);if(!W.Save(false))return EventRecoveryError(W,TEXT("Save failed or dirty/identity readback incomplete; actual true history retained."),TEXT("niagara_event_handler_save_state_unknown"),CompileWait,true);}
    auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("success"),true);Result->SetBoolField(TEXT("skipped_duplicate"),false);Result->SetStringField(TEXT("system_path"),System->GetPathName());Result->SetStringField(TEXT("source_emitter_name"),ResolvedSourceEmitterName);Result->SetNumberField(TEXT("source_emitter_index"),ResolvedSourceEmitterIndex);Result->SetNumberField(TEXT("handler_count_before"),BeforeHandlerCount);Result->SetNumberField(TEXT("handler_count_after"),Target.Data->GetEventHandlers().Num());Result->SetStringField(TEXT("target_usage"),NiagaraScriptUsageName(ENiagaraScriptUsage::ParticleEventScript));Result->SetStringField(TEXT("target_usage_id"),EventUsageId.ToString(EGuidFormats::DigitsWithHyphens));Result->SetBoolField(TEXT("compile_requested"),bCompileRequested);Result->SetObjectField(TEXT("compile_wait"),CompileWait);Result->SetBoolField(TEXT("saved"),W.R.bSaved);Result->SetStringField(TEXT("write_scope"),TEXT("target_temp_event_handler_create"));Result->SetStringField(TEXT("authoring_dependency"),TEXT("engine_native_niagara_only"));Result->SetObjectField(TEXT("event_handler"),EventHandlerReadback);W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;
    };
    if(W.DeferredMode&&bRequestCompile&&bWaitForCompletion){BeginEventWait(W,System,TimeoutSeconds,PollIntervalSeconds,MoveTemp(CompleteEvent));auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);return Pending;}
    auto CompileWait=W.WaitSnapshot(bRequestCompile&&bWaitForCompletion,TimeoutSeconds,PollIntervalSeconds);if(!ContinueRead())return W.Error(W.R.Error);return CompleteEvent(CompileWait);
#else
    return W.Error(TEXT("add_niagara_event_handler requires editor-only Niagara data"));
#endif
}


FString FinishGS(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation,bool Observer)
{
 bool Leaf=false,Typed=false,Complete=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),Leaf);W.R.Data->TryGetBoolField(TEXT("typed_target_postcondition_passed"),Typed);
 if(Leaf&&Typed&&W.R.Error.IsEmpty()){
  if(Observer)Complete=W.Admit(TEXT("final_observer_admission"));else if(W.Before){auto After=W.Projection();W.R.Data->SetObjectField(TEXT("after"),After);bool B=false,A=false;W.Before->TryGetBoolField(TEXT("source_projection_complete"),B);After->TryGetBoolField(TEXT("source_projection_complete"),A);if(B&&A&&W.R.Error.IsEmpty()){W.R.bChanged|=!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive);Complete=W.Admit(TEXT("final_target_admission"));}}
 }
 if(W.Before)W.R.Data->SetObjectField(TEXT("before"),W.Before);
 if(!Leaf&&W.R.Error.IsEmpty()){FString Message,Code;if(Legacy){Legacy->TryGetStringField(TEXT("error"),Message);Legacy->TryGetStringField(TEXT("error_code"),Code);}W.Fail(Code.IsEmpty()?TEXT("niagara_event_stage_request_failed"):*Code,Message.IsEmpty()?TEXT("Original request failed; see legacy_result and actual prefix."):Message);}
 if(Leaf&&(!Typed||!Complete)&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_event_stage_postcondition_incomplete"),TEXT("Actual typed target and safe final projection/admission are required."));
 if(!W.SaveHealth&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_event_stage_save_failed"),TEXT("Current save/dirty verification failed; actual SDK true save history retained."));
 W.R.bSuccess=Leaf&&Typed&&Complete&&W.SaveHealth&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;if(W.R.bSuccess&&!Observer)W.R.bChangeStateKnown=true;
 W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.Saves);W.R.Data->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);W.R.Data->SetArrayField(TEXT("actual_item_prefix"),W.Partial);W.R.Data->SetArrayField(TEXT("actual_sdk_prefix"),W.MutationRows);
 W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),Complete);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),W.R.bSuccess&&!Observer);W.R.Data->SetBoolField(TEXT("required_compile_wait_implemented"),W.DeferredMode);W.R.Data->SetBoolField(TEXT("removed_parameter_sync_equivalence_verified"),false);W.R.Data->SetBoolField(TEXT("actual_CPU_or_GPU_execution_verified"),false);
 W.R.Data->SetStringField(TEXT("compile_scope"),W.DeferredMode?TEXT("Event registration/RI delegate -> output graph -> compile -> genuine bounded wait -> independent strict status -> reacquired usageId/output/CPUready/CPUcompiled/sourcegenerator gate -> optional save. Timeout is original mutation recovery failure; compiled stays null after request. No global pump/Sleep/flush; removed SystemRefresh equivalence remains unverified."):TEXT("Synchronous requested wait is refused before writer; no wait=False substitution or fake completion. RequestCompile acceptance is not completion."));
 W.R.Data->SetStringField(TEXT("query_scope"),TEXT("Read-only means explicit authoring/compile/save zero. FillCompilationData is public virtual derived compile configuration with live exact Script admission, not live GPU execution/performance. SDK getters/cache/global side effects are unverified."));
 W.R.Data->SetStringField(TEXT("save_scope"),TEXT("Normalized requested save/compile intent is retained at valid input admission. Actual attempts/acceptance and OR SaveLoadedAsset true history are separate; no SetDirtyFlag(false). Later dirty/identity failures do not erase prior true return. Cold persistence and callback atomicity unverified."));
 W.R.Data->SetBoolField(TEXT("partial_event_or_graph_residual_possible"),!W.R.bSuccess&&W.R.bMutationAttempted);W.R.Data->SetStringField(TEXT("cleanup_scope"),TEXT("Safe graph-initialization failure may remove only the exact registered usageId/script through public RemoveEventHandlerByUsageId, with prefix/post-admission. Unsafe or already failed admission skips new cleanup SDK. New script/nodes/edges may remain; no force delete, reload, Undo reset, semantic rollback or replay is claimed."));
 W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
using FGSHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunGS(const FString& Text,const TCHAR* Operation,FGSHandler Handler,bool Observer)
{
 FWork W;W.WriterIntent=!Observer;if(!IsInGameThread()){W.Fail(TEXT("niagara_event_stage_game_thread_required"),TEXT("Editor game thread required."));return FinishGS(W,W.Error(W.R.Error),Operation,Observer);}if(Running||DeferredLeaseActive){W.Fail(TEXT("niagara_event_stage_reentrant_request"),TEXT("Dedicated request active; nested Native dispatch is rejected without changing Current."));return FinishGS(W,W.Error(W.R.Error),Operation,Observer);}
 TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);TSharedPtr<FJsonObject> Params;if(!ParseParams(Text,Params)){W.Fail(TEXT("niagara_event_stage_invalid_params_json"),TEXT("Finite bounded JSON object with exact keys and no embedded NUL required."));return FinishGS(W,W.Error(W.R.Error),Operation,Observer);}
 if(!Observer){bool Save=true,Compile=true;Params->TryGetBoolField(TEXT("save"),Save);Params->TryGetBoolField(TEXT("request_compile"),Compile);W.R.bSaveRequested=Save;W.R.bCompileRequested=Compile;}W.R.Data->SetObjectField(TEXT("requested_params"),MakeShared<FJsonObject>(*Params));if(!W.Admit(TEXT("preflight")))return FinishGS(W,W.Error(W.R.Error),Operation,Observer);return FinishGS(W,Handler(W,Params),Operation,Observer);
}
}
FString InspectSimulationStages(const FString& ParamsJson){return RunGS(ParamsJson,TEXT("inspect_niagara_simulation_stages"),&HandleStages,true);}
FString InspectEventHandlers(const FString& ParamsJson){return RunGS(ParamsJson,TEXT("inspect_niagara_event_handlers"),&HandleHandlers,true);}
FString AddEventHandler(const FString& ParamsJson){return RunGS(ParamsJson,TEXT("add_niagara_event_handler"),&HandleAdd,false);}
}

namespace Cubeless::UE58::NiagaraEventStage
{
namespace EventHandlerAsync
{
struct FJobState;
namespace
{

TSharedPtr<FJobState> ActiveJob;

// Unsafe records retain exact target/Job holds; Shutdown never drops them inside SDK calls or GC.
TArray<TSharedPtr<FJobState>>& UnsafeRecords(){static auto* Records=new TArray<TSharedPtr<FJobState>>();return *Records;}
enum class EPhase:uint8 { Prepare, Continue, Final, Terminal, Quarantined };
}
struct FJobState:public TSharedFromThis<FJobState>
{
 FWork Work;
 TStrongObjectPtr<UCubelessNiagaraEmitterEventAuthoringJob> Job{nullptr};
 UCubelessNiagaraEmitterEventAuthoringJob* RawJob=nullptr;
 TSharedPtr<FJsonObject> Params,Legacy;
 FString Operation,Id,LastSnapshot;
 EPhase Phase=EPhase::Prepare;
 FTSTicker::FDelegateHandle Ticker;
 FDelegateHandle PreExit;
 uint64 LastFrame=MAX_uint64;
 int64 Revision=0;
 bool Executing=false,Dispatched=false,FinalEncoded=false;
 explicit FJobState(const TCHAR* Op):Operation(Op),Id(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens))
 {Work.DeferredMode=true;Work.R.Data->SetBoolField(TEXT("async_emitter_event_continuation"),true);}
 bool PublicationSafe()const
 {
  if(!Safe()||Work.CapturedEditor.Get()!=GEditor)return false;
  if(Work.WriterIntent&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld))return false;
  UWorld* EditorWorld=GEditor->GetEditorWorldContext().World();
  return Work.WorldCaptured&&Live(EditorWorld)&&!EditorWorld->bIsTearingDown&&Work.World.Get()==EditorWorld;
 }
 void Unhook()
 {
  if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}
  if(PreExit.IsValid()){FCoreDelegates::OnEnginePreExit.Remove(PreExit);PreExit.Reset();}
 }
 void Metadata(bool Terminal)
 {
  auto& D=Work.R.Data;D->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),Work.PersistentMutationAttempted);D->SetBoolField(TEXT("transient_creation_only"),!Work.PersistentMutationAttempted);
  D->SetStringField(TEXT("native_job_id"),Id);
  D->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
  D->SetBoolField(TEXT("native_job_terminal"),Terminal);
  D->SetBoolField(TEXT("operation_complete"),Terminal);
  D->SetStringField(TEXT("operation"),Operation);
  D->SetBoolField(TEXT("automatic_retry_permitted"),false);
  D->SetBoolField(TEXT("async_core_dispatched"),Dispatched);
  
  
  
  
  
  D->SetBoolField(TEXT("timeout_does_not_imply_compile_success"),true);
  D->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);
  D->SetStringField(TEXT("continuation_scope"),TEXT("Exact existing EventHandler core on finite GT ticks. Compile then genuine bounded wait then independent original strict status gate then optional save. Timeout/pending/status gate failure remains original mutation recovery failure, including save=false. No global pump/Sleep/replay; SDK calls may block internally."));
  D->SetObjectField(TEXT("native_call_counts"),Work.Counts);
  
  
  D->SetArrayField(TEXT("actual_save_prefix"),Work.Saves);D->SetArrayField(TEXT("actual_compile_prefix"),Work.Compiles);D->SetArrayField(TEXT("actual_sdk_prefix"),Work.MutationRows);D->SetArrayField(TEXT("actual_item_prefix"),Work.Partial);
  D->SetObjectField(TEXT("legacy_result"),Legacy.IsValid()?Legacy:MakeShared<FJsonObject>());
 }
 void Cache(const FString& Encoded,bool Terminal,bool ObjectSafe)
 {
  LastSnapshot=Encoded;
  NiagaraEmitterEventAuthoringAsync::Publish(RawJob,Encoded,Terminal,ObjectSafe);
 }
 void Quarantine(const FString& Reason)
 {
  if(Phase==EPhase::Quarantined)return;
  Unhook();
  Work.CancelRequested=true;Work.CancelReason=Reason;RefreshLegacy();
  if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_event_stage_async_context_quarantined"),Reason);
  Work.R.bSuccess=false;Work.R.bPostconditionPassed=false;Work.R.bChangeStateKnown=false;
  Work.R.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);
  Work.R.Data->SetBoolField(TEXT("native_writers_stopped"),true);
  Work.R.Data->SetBoolField(TEXT("do_not_retry"),true);
  if(!Legacy)Legacy=MakeShared<FJsonObject>();
  Legacy->SetBoolField(TEXT("success"),false);Legacy->SetBoolField(TEXT("operation_complete"),false);
  if(!Legacy->HasField(TEXT("error")))Legacy->SetStringField(TEXT("error"),Reason);
  Metadata(false);
  Phase=EPhase::Quarantined;
  Cache(Operations::EncodeVariableResult(Work.R,*Operation),false,false);
  // No Job-property/target access or strong-reference release in unsafe context.
  UnsafeRecords().AddUnique(AsShared());
 }
 void PublishPending()
 {
  Work.R.bSuccess=false;Work.R.bPostconditionPassed=false;
  Metadata(false);
  if(!PublicationSafe()){Quarantine(TEXT("Editor/world/GC/exit unavailable while publishing pending emitter/event authoring snapshot."));return;}
  Cache(Operations::EncodeVariableResult(Work.R,*Operation),false,true);
 }
 void SetFailureLegacy()
 {
  if(!Legacy)Legacy=MakeShared<FJsonObject>();
  Legacy->SetBoolField(TEXT("success"),false);
  Legacy->SetStringField(TEXT("error"),Work.R.Error.IsEmpty()?TEXT("EmitterEvent job stopped; actual prefix retained."):Work.R.Error);
 }
 void Finalize()
 {
  if(FinalEncoded||Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("EmitterEvent terminal publication lost its exact Editor world; no recovery or replay."));return;}
  if(!Work.R.Error.IsEmpty()){RefreshLegacy();SetFailureLegacy();}
  Metadata(true);
  LastSnapshot=FinishGS(Work,Legacy,*Operation,false);
  FinalEncoded=true;
  if(!PublicationSafe()){FinalEncoded=false;Quarantine(TEXT("EmitterEvent final guard lost publication context."));return;}
  Cache(LastSnapshot,true,true);
  Phase=EPhase::Terminal;Unhook();
 }
 void ObserveChangedPrefix()
 {
  if(!Work.R.Error.IsEmpty()||!Work.Before.IsValid()||!Work.Admit(TEXT("async_changed_prefix")))return;
  auto Projection=Work.Projection();bool Complete=false;Projection->TryGetBoolField(TEXT("source_projection_complete"),Complete);
  if(Complete){Work.R.bChanged|=!Json(Work.Before).Equals(Json(Projection),ESearchCase::CaseSensitive);Work.R.Data->SetBoolField(TEXT("current_partial_projection_complete"),true);}
 }
 void RefreshLegacy()
 {
  const TSharedPtr<FJsonObject>* Stored=nullptr;if(Work.R.Data->TryGetObjectField(TEXT("legacy_result"),Stored)&&Stored&&Stored->IsValid())Legacy=*Stored;
 }
 void ExecutePhase()
 {
  if(Work.CancelRequested||NiagaraEmitterEventAuthoringAsync::IsStopping())
  {
   if(!Work.CancelRequested){Work.CancelRequested=true;Work.CancelReason=TEXT("Module shutdown stopped the owned emitter/event authoring job.");}
   Work.Fail(TEXT("niagara_event_stage_cancelled"),Work.CancelReason);
  }
  if(!Work.R.Error.IsEmpty())
  {
   RefreshLegacy();Phase=EPhase::Final;Finalize();return;
  }
  if(!Work.Admit(TEXT("async_conversion_phase_admission")))
  {
   RefreshLegacy();Phase=EPhase::Final;Finalize();return;
  }
  if(Phase==EPhase::Prepare)
  {
   if(Dispatched){Work.Fail(TEXT("niagara_event_stage_async_replay_forbidden"),TEXT("Original operation already dispatched."));Phase=EPhase::Final;Finalize();return;}
   Dispatched=true;Legacy=HandleAdd(Work,Params);if(Legacy.IsValid())Work.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
   ObserveChangedPrefix();Phase=Work.DeferredStep?EPhase::Continue:EPhase::Final;
  }
  else if(Phase==EPhase::Continue)
  {
   const bool Complete=Work.DeferredStep();RefreshLegacy();if(Complete){ObserveChangedPrefix();Phase=EPhase::Final;}
  }
  else if(Phase==EPhase::Final){Finalize();return;}
  if(!Work.R.Error.IsEmpty()){RefreshLegacy();Phase=EPhase::Final;Finalize();return;}
  PublishPending();
 }
 void ReleaseSafe()
 {
  if(Executing||Phase!=EPhase::Terminal||!PublicationSafe())return;
  Work.DeferredStep=nullptr;
  Work.Objects.Reset();Job.Reset();
  if(ActiveJob.Get()==this){ActiveJob.Reset();DeferredLeaseActive=false;}
 }
 bool Tick(float)
 {
  TSharedRef<FJobState> Hold=AsShared();
  if(Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return false;
  if(Executing||Running||LastFrame==GFrameCounter)return true;
  LastFrame=GFrameCounter;
  if(!PublicationSafe()){Quarantine(TEXT("EmitterEvent ticker lost live Editor/game-thread/world context."));return false;}
  {
   TGuardValue<bool> Busy(Executing,true);
   TGuardValue<bool> CoreLease(Running,true);
   TGuardValue<FWork*> Context(Current,&Work);
   ExecutePhase();
  }
  ReleaseSafe();
  return Phase!=EPhase::Terminal&&Phase!=EPhase::Quarantined;
 }
 void Hook()
 {
  const TWeakPtr<FJobState> Weak=AsShared();
  Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){auto State=Weak.Pin();return State.IsValid()&&State->Tick(Delta);}),0.0f);

 }
};
namespace
{
UCubelessNiagaraEmitterEventAuthoringJob* Start(const FString& Text,const TCHAR* Operation)
{
 if(!IsInGameThread()||NiagaraEmitterEventAuthoringAsync::IsStopping()||Running||DeferredLeaseActive||!Safe())return nullptr;
 auto State=MakeShared<FJobState>(Operation);
 State->Work.R.bSaveRequested=true;
 if(!State->Work.Admit(TEXT("async_emitter_event_start_admission")))return nullptr;

 ActiveJob=State;DeferredLeaseActive=true;
 UCubelessNiagaraEmitterEventAuthoringJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(State->Executing,true);
  TGuardValue<bool> CoreLease(Running,true);
  TGuardValue<FWork*> Context(Current,&State->Work);
  if(State->Work.SDK(TEXT("NewObject_EmitterEventJob"),true))
  {
   Created=NewObject<UCubelessNiagaraEmitterEventAuthoringJob>(GetTransientPackage());
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_returned"),true);
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
   State->RawJob=Created;
   if(NiagaraEmitterEventAuthoringAsync::IsStopping()||State->Work.CancelRequested||!State->PublicationSafe())
   {
    State->Quarantine(TEXT("EmitterEvent Job allocation interrupted; persistent emitter/event authoring dispatch not entered."));
    return nullptr;
   }
   if(!State->Work.After(TEXT("NewObject_EmitterEventJob"))||!Created)
   {
    State->Quarantine(TEXT("EmitterEvent Job allocation could not be safely adopted."));
    return nullptr;
   }
   State->Job.Reset(Created);FGuid RegisteredId;FGuid::Parse(State->Id,RegisteredId);NiagaraEmitterEventAuthoringAsync::RegisterJob(Created,NiagaraEmitterEventAuthoringAsync::EJobKind::EventHandler,RegisteredId);
   if(!ParseParams(Text,State->Params))
   {
    State->Work.Fail(TEXT("niagara_event_stage_invalid_params_json"),TEXT("Finite JSON object within1Mi/depth64/262144 slots required; raw presence/defaults are preserved."));
    State->Legacy=State->Work.Error(State->Work.R.Error);State->Phase=EPhase::Final;State->Finalize();
   }
   else
   {
    State->Work.R.Data->SetObjectField(TEXT("requested_params"),State->Params);State->Work.R.bSaveRequested=true;State->Work.R.bCompileRequested=true;State->Params->TryGetBoolField(TEXT("save"),State->Work.R.bSaveRequested);State->Params->TryGetBoolField(TEXT("request_compile"),State->Work.R.bCompileRequested);
    State->Legacy=MakeShared<FJsonObject>();State->Legacy->SetBoolField(TEXT("success"),false);State->Legacy->SetBoolField(TEXT("operation_complete"),false);
    State->PublishPending();
   }
  }
 }
 if(!Created)
 {
  if(State->Phase!=EPhase::Quarantined){ActiveJob.Reset();DeferredLeaseActive=false;}
  return nullptr;
 }
 if(State->Phase==EPhase::Terminal){State->ReleaseSafe();return Created;}
 if(State->Phase==EPhase::Quarantined)return Created;
 State->Hook();return Created;
}
}
bool Cancel(UCubelessNiagaraEmitterEventAuthoringJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!Job||!ActiveJob.IsValid()||ActiveJob->RawJob!=Job)return false;
 auto State=ActiveJob;
 if(State->Phase==EPhase::Terminal||State->Phase==EPhase::Quarantined)return false;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return true;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Cancellation lost emitter/event authoring publication context; actual prefix retained."));return true;}
 {
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> CoreLease(Running,true);TGuardValue<FWork*> Context(Current,&State->Work);
  State->Work.Fail(TEXT("niagara_event_stage_async_cancelled"),State->Work.CancelReason);
  State->Phase=EPhase::Final;State->Finalize();
 }
 State->ReleaseSafe();return true;
}
void Shutdown(const FString& Reason)
{
 if(!IsInGameThread())return;
 auto State=ActiveJob;if(!State.IsValid())return;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe emitter/event authoring state without touching UObject publication."));return;}
 Cancel(State->RawJob,State->Work.CancelReason);
}
}
bool HasEventHandlerContinuationLease(){return DeferredLeaseActive;}
UCubelessNiagaraEmitterEventAuthoringJob* StartEventHandlerContinuation(const FString& P){return EventHandlerAsync::Start(P,TEXT("add_niagara_event_handler"));}
bool CancelEventHandlerContinuation(UCubelessNiagaraEmitterEventAuthoringJob* J,const FString& R){return EventHandlerAsync::Cancel(J,R);}
void ShutdownEventHandlerContinuation(const FString& R){EventHandlerAsync::Shutdown(R);}
}
