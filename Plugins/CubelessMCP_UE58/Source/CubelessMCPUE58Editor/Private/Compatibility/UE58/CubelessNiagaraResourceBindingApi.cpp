
// GQ dedicated Editor-only Niagara resource/scratch bindings. Source-only candidate.
#include "Compatibility/UE58/CubelessNiagaraResourceBindingApi.h"
#include "CubelessNiagaraResourceBindingJob.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#include <atomic>
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
namespace Cubeless::UE58::NiagaraResourceBindings
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Running=false;
bool DeferredLeaseActive=false;std::atomic<bool> DeferredStopping{false};
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
{UEdGraphPin* Pointer=nullptr;FGuid Id;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;TArray<UEdGraphPin*> Links;FString DefaultValue,AutoDefault,DefaultText;TWeakObjectPtr<UObject> DefaultObject;};
struct FNodeIdentity
{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;int32 X=0,Y=0;TArray<FPinIdentity> Pins;TWeakObjectPtr<UNiagaraScript> FunctionScript;FGuid FunctionVersion;FName FunctionAsset;FNiagaraFunctionSignature FunctionSignature;TMap<FName,FName> Specifiers;bool Function=false;};
FString GraphMetadataSignature(UNiagaraGraph* G);
struct FGraphIdentity
{TWeakObjectPtr<UNiagaraGraph> Graph;TWeakObjectPtr<UClass> Schema;TArray<FNodeIdentity> Nodes;FString Meta;};
struct FSourceIdentity
{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;};
struct FEmitterIdentity
{FGuid Id,InstanceVersion,DataVersion;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,ParentScratch;TArray<TWeakObjectPtr<UNiagaraScript>> ScratchScripts,ParentScripts;TArray<TWeakObjectPtr<UNiagaraSimulationStageBase>> Stages;};
struct FScriptBinding{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;};
struct FStageBinding{TWeakObjectPtr<UNiagaraSimulationStageBase> Stage;TWeakObjectPtr<UNiagaraScript> Script;FName Name;};

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
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty())Fail(TEXT("niagara_resource_bindings_request_failed"),Message);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
    bool Budget(int64 N,int64 Text=0){Slots+=N;TextUnits+=Text;Prefix->SetNumberField(TEXT("captured_slots"),static_cast<double>(Slots));Prefix->SetNumberField(TEXT("observed_text_utf16_units"),static_cast<double>(TextUnits));return (Slots<=262144&&TextUnits<=32ll*1024*1024)||Fail(TEXT("niagara_resource_bindings_budget_exceeded"),TEXT("Bounded capture/output budget exceeded; partial prefix retained."));}
    bool Pin(UObject* O){if(!Live(O))return Fail(TEXT("niagara_resource_bindings_object_unavailable"),TEXT("Expected live loaded object."));for(const auto& I:Objects){if(I->Object.Get()==O)return I->Exact()||Fail(TEXT("niagara_resource_bindings_identity_changed"),TEXT("Captured object changed."));}if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool Own(UObject* O)
    {auto* Root=System.Get();if(!Admit(TEXT("writer_ownership"))||!Live(O)||!Live(Root)||(O!=Root&&!O->IsIn(Root))||O->GetOutermost()!=Root->GetOutermost()||O->GetPackage()!=Root->GetPackage())return Fail(TEXT("niagara_resource_bindings_external_writer_unsupported"),TEXT("Actual writer is outside the specified System outer chain/package. No external emitter/shared graph/source was changed."));if(!Pin(O))return false;for(auto& I:Objects){if(I->Object.Get()==O){I->Writer=true;I->Root=Root;}}return true;}
    bool Admit(const FString& Stage)
    {
        R.Stage=Stage;if(DeferredMode&&(DeferredStopping.load()||CancelRequested))return Fail(TEXT("niagara_resource_bindings_async_cancelled"),CancelReason.IsEmpty()?TEXT("Owned resource binding continuation stopped; no further SDK."):CancelReason);if(!R.Error.IsEmpty())return false;
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||GIsSavingPackage)return Fail(TEXT("niagara_resource_bindings_context_unavailable"),TEXT("Live Editor game thread without GC/exit/save required; no replay."));
        if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_resource_bindings_editor_changed"),TEXT("Captured Editor changed before getter; no replay."));
        if(WriterIntent&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext()))return Fail(TEXT("niagara_resource_bindings_play_session_active"),TEXT("Niagara binding authoring requires stopped PIE/SIE."));
        UWorld* CurrentWorld=GEditor->GetEditorWorldContext().World();if(!Live(CurrentWorld)||CurrentWorld->bIsTearingDown)return Fail(TEXT("niagara_resource_bindings_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){World=CurrentWorld;if(DeferredMode)CapturedEditor=GEditor;WorldCaptured=true;}else if(World.Get()!=CurrentWorld)return Fail(TEXT("niagara_resource_bindings_world_changed"),TEXT("Editor world changed across SDK dispatch."));
        for(const auto& I:Objects){if(!I->Exact())return Fail(TEXT("niagara_resource_bindings_identity_changed"),TEXT("Captured class/outer/package/path/ownership changed; no replacement adopted."));}
        if(System.IsValid())
        {auto* O=System.Get();if(O->GetSystemSpawnScript()!=Spawn.Get()||O->GetSystemUpdateScript()!=Update.Get()||O->ScratchPadScripts.Num()!=Scratch.Num()||O->GetEmitterHandles().Num()!=Emitters.Num())return Fail(TEXT("niagara_resource_bindings_system_topology_changed"),TEXT("Captured System emitter/script topology changed."));
            for(int32 I=0;I<Scratch.Num();++I)if(O->ScratchPadScripts[I]!=Scratch[I].Get())return Fail(TEXT("niagara_resource_bindings_scratch_membership_changed"),TEXT("System scratch membership changed."));
            for(int32 I=0;I<Emitters.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=Emitters[I];if(H.GetId()!=E.Id||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.InstanceVersion||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion))return Fail(TEXT("niagara_resource_bindings_emitter_version_changed"),TEXT("Physical emitter/ref/version changed."));
                if(E.Data){if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.ParentScratch.Get())return Fail(TEXT("niagara_resource_bindings_emitter_routing_changed"),TEXT("Emitter source/scratch binding changed."));
                    auto Same=[](const UNiagaraScratchPadContainer* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& P){if(!C)return P.IsEmpty();if(C->Scripts.Num()!=P.Num())return false;for(int32 J=0;J<P.Num();++J)if(C->Scripts[J]!=P[J].Get())return false;return true;};
                    if(!Same(E.Scratch.Get(),E.ScratchScripts)||!Same(E.ParentScratch.Get(),E.ParentScripts))return Fail(TEXT("niagara_resource_bindings_scratch_membership_changed"),TEXT("Emitter scratch membership changed."));
                    const auto& A=E.Data->GetSimulationStages();if(A.Num()!=E.Stages.Num())return Fail(TEXT("niagara_resource_bindings_stage_membership_changed"),TEXT("Simulation stage count changed."));for(int32 J=0;J<A.Num();++J)if(A[J]!=E.Stages[J].Get())return Fail(TEXT("niagara_resource_bindings_stage_membership_changed"),TEXT("Simulation stage order changed."));
                }
            }
        }
        for(const auto& S:ScriptBindings){auto* O=S.Script.Get();if(!Live(O)||O->GetLatestSource()!=S.Source.Get()||O->GetExposedVersion().VersionGuid!=S.Version)return Fail(TEXT("niagara_resource_bindings_script_source_changed"),TEXT("Script source/exposed version changed."));}
        for(const auto& S:StageBindings){auto* O=S.Stage.Get();if(!Live(O)||O->Script!=S.Script.Get()||O->SimulationStageName!=S.Name)return Fail(TEXT("niagara_resource_bindings_stage_script_changed"),TEXT("Stage name/script binding changed."));}
        for(const auto& S:Sources){if(!Live(S.Source.Get())||S.Source->NodeGraph!=S.Graph.Get())return Fail(TEXT("niagara_resource_bindings_source_graph_changed"),TEXT("Captured source graph binding changed."));}
        for(const auto& G:Graphs){if(G.Graph.Get()==TransitionGraph)continue;auto* O=G.Graph.Get();if(!Live(O)||O->Schema!=G.Schema.Get()||O->Nodes.Num()!=G.Nodes.Num()||!GraphMetadataSignature(O).Equals(G.Meta,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_resource_bindings_graph_topology_changed"),TEXT("Graph node count changed outside the planned removal."));
            for(int32 I=0;I<G.Nodes.Num();++I){const auto& E=G.Nodes[I];auto* N=E.Node.Get();if(!Live(N)||O->Nodes[I]!=N||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num())return Fail(TEXT("niagara_resource_bindings_node_identity_changed"),TEXT("Captured node/pin membership changed."));
                if(E.Function){auto* F=Cast<UNiagaraNodeFunctionCall>(N);if(!F||F->FunctionScript!=E.FunctionScript.Get()||F->SelectedScriptVersion!=E.FunctionVersion||F->FunctionScriptAssetObjectPath!=E.FunctionAsset||F->FunctionSpecifiers.OrderIndependentCompareEqual(E.Specifiers)==false||!FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&F->Signature,&E.FunctionSignature,0))return Fail(TEXT("niagara_resource_bindings_function_binding_changed"),TEXT("Selected function script/version/signature/specifiers changed across SDK callback."));}
                for(int32 J=0;J<E.Pins.Num();++J){const auto& P=E.Pins[J];if(N->Pins[J]!=P.Pointer)return Fail(TEXT("niagara_resource_bindings_pin_identity_changed"),TEXT("Captured pin disappeared; stale pointer not dereferenced."));if(P.Pointer&&(P.Pointer->PinId!=P.Id||P.Pointer->GetOwningNodeUnchecked()!=N||!P.Pointer->PinName.ToString().Equals(P.Name.ToString(),ESearchCase::CaseSensitive)||P.Pointer->Direction!=P.Direction||P.Pointer->PinType!=P.Type||P.Pointer->LinkedTo!=P.Links||!P.Pointer->DefaultValue.Equals(P.DefaultValue,ESearchCase::CaseSensitive)||!P.Pointer->AutogeneratedDefaultValue.Equals(P.AutoDefault,ESearchCase::CaseSensitive)||!P.Pointer->DefaultTextValue.ToString().Equals(P.DefaultText,ESearchCase::CaseSensitive)||P.Pointer->DefaultObject!=P.DefaultObject.Get()))return Fail(TEXT("niagara_resource_bindings_pin_links_changed"),TEXT("Pin identity/type/links differ from expected admitted transition."));}
            }
        }for(const auto& E:ExpectedDI){auto* Node=E.Node.Get();auto* DI=E.DI.Get();if(!Live(Node)||!Live(DI)||ReflectedNodeDI(Node)!=DI||Node->Input!=E.Input||Node->CallSortPriority!=E.Sort||Node->Usage!=E.Usage||DI->GetOuter()!=Node||DI->GetPackage()!=System->GetPackage())return Fail(TEXT("niagara_resource_bindings_DI_replaced"),TEXT("Captured DI input-node binding/owner changed."));if(E.RT){auto* D=Cast<UNiagaraDataInterfaceRenderTarget2D>(DI);if(!D||D->RenderTargetUserParameter.Parameter!=E.User||D->bInheritUserParameterSettings!=E.Inherit)return Fail(TEXT("niagara_resource_bindings_DI_fields_changed"),TEXT("RT public binding fields changed outside requested assignment."));}else{auto* D=Cast<UNiagaraDataInterfaceTexture>(DI);if(!D||D->TextureUserParameter.Parameter!=E.User||D->Texture!=E.Texture.Get())return Fail(TEXT("niagara_resource_bindings_DI_fields_changed"),TEXT("Texture public binding fields changed outside requested assignment."));}}return true;
    }
    TSharedRef<FJsonObject> MutationRow(const TCHAR* Stage){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetBoolField(TEXT("SDK_returned"),false);MutationRows.Add(MakeShared<FJsonValueObject>(Row));return Row;}
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage))return false;if(!Budget(1,FCString::Strlen(Stage)))return false;if(++Calls>262144)return Fail(TEXT("niagara_resource_bindings_sdk_budget"),TEXT("SDK call budget exceeded."));R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;if(DeferredMode&&Mutation&&FCString::Strcmp(Stage,TEXT("NewObject_ResourceBindingJob"))!=0)PersistentMutationAttempted=true;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(Mutation)Prefix->SetBoolField(Stage,true);return true;}
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
#define GQ_STEP(Stage,Mutation,Expr) do{if(!W.Step(TEXT(Stage),Mutation,[&]{Expr;}))return W.Error(W.R.Error);}while(false)

bool FWork::CaptureSystem(UNiagaraSystem* O)
{
    if(!Pin(O)||!Pin(O->GetOutermost()))return false;System=O;Spawn=O->GetSystemSpawnScript();Update=O->GetSystemUpdateScript();if(!CaptureScript(Spawn.Get())||!CaptureScript(Update.Get()))return false;
    for(UNiagaraScript* S:O->ScratchPadScripts){if(S&&!CaptureScript(S))return false;Scratch.Add(S);}
    if(!Budget(O->GetEmitterHandles().Num())||O->GetEmitterHandles().Num()>4096)return Fail(TEXT("niagara_resource_bindings_emitter_bound"),TEXT("Emitter inventory exceeds bound."));
    for(const auto& H:O->GetEmitterHandles()){FEmitterIdentity E;E.Id=H.GetId();E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Emitter.IsValid()&&!Pin(E.Emitter.Get()))return false;if(E.Data){E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.ParentScratch=E.Data->ParentScratchPads;if(E.Source.IsValid()&&!Pin(E.Source.Get()))return false;
        auto CaptureScripts=[&](UNiagaraScratchPadContainer* C,TArray<TWeakObjectPtr<UNiagaraScript>>& A){if(!C)return true;if(!Pin(C)||!Budget(C->Scripts.Num()))return false;for(UNiagaraScript* S:C->Scripts){if(S&&!CaptureScript(S))return false;A.Add(S);}return true;};
        if(!CaptureScripts(E.Scratch.Get(),E.ScratchScripts)||!CaptureScripts(E.ParentScratch.Get(),E.ParentScripts))return false;
        TArray<UNiagaraScript*> OwnedScripts;E.Data->GetScripts(OwnedScripts,false,false);if(!Budget(OwnedScripts.Num()))return false;for(UNiagaraScript* P:OwnedScripts)if(!CaptureScript(P))return false;
        if(!Budget(E.Data->GetSimulationStages().Num()))return false;for(UNiagaraSimulationStageBase* S:E.Data->GetSimulationStages()){if(S&&(!Pin(S)||!CaptureScript(S->Script)))return false;if(S)StageBindings.Add({S,S->Script,S->SimulationStageName});E.Stages.Add(S);}
    }Emitters.Add(MoveTemp(E));}return true;
}
UNiagaraSystem* LoadSystem(const FString& Path)
{
    if(!Current->SDK(TEXT("LoadObject_System")))return nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* O=LoadObject<UNiagaraSystem>(nullptr,*Expected);Current->Prefix->SetBoolField(TEXT("load_returned_nonnull"),O!=nullptr);if(!Current->After(TEXT("LoadObject_System")))return nullptr;if(O&&(!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Current->CaptureSystem(O))){Current->Fail(TEXT("niagara_resource_bindings_load_identity_unexpected"),TEXT("Loaded System differs from requested normalized exact path."));return nullptr;}return O;
}
bool FWork::CaptureGraph(UNiagaraScriptSource* S)
{
    if(!Admit(TEXT("capture_graph")))return false;if(!S||!S->NodeGraph)return true;if(!Pin(S)||!Pin(S->NodeGraph))return false;auto* G=S->NodeGraph.Get();
    bool Bound=false;for(const auto& E:Sources)if(E.Source.Get()==S){Bound=true;if(E.Graph.Get()!=G)return Fail(TEXT("niagara_resource_bindings_source_graph_changed"),TEXT("Source graph changed."));}
    if(!Bound)Sources.Add({S,G});for(const auto& E:Graphs)if(E.Graph.Get()==G)return true;
    if(!Budget(G->Nodes.Num())||G->Nodes.Num()>16384)return Fail(TEXT("niagara_resource_bindings_graph_bound"),TEXT("Graph node inventory exceeds bound."));
    FGraphIdentity E;E.Graph=G;E.Schema=G->Schema.Get();E.Meta=GraphMetadataSignature(G);
    for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N)||N->GetGraph()!=G)return Fail(TEXT("niagara_resource_bindings_node_owner_invalid"),TEXT("Null/foreign graph node cannot be safely inspected or mutated."));FNodeIdentity NE;NE.Node=N;NE.Id=N->NodeGuid;NE.X=N->NodePosX;NE.Y=N->NodePosY;if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()>65536||!Budget(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()))return Fail(TEXT("niagara_resource_bindings_signature_bound"),TEXT("Function signature inventory exceeds bound."));NE.Function=true;NE.FunctionScript=F->FunctionScript;NE.FunctionVersion=F->SelectedScriptVersion;NE.FunctionAsset=F->FunctionScriptAssetObjectPath;NE.FunctionSignature=F->Signature;NE.Specifiers=F->FunctionSpecifiers;if(F->FunctionScript&&!CaptureScript(F->FunctionScript))return false;}if(!Budget(N->Pins.Num()))return false;
        for(UEdGraphPin* P:N->Pins){if(!P)return Fail(TEXT("niagara_resource_bindings_null_pin"),TEXT("Null graph pin unsupported by Niagara public getters."));if(P->GetOwningNodeUnchecked()!=N||!Budget(P->LinkedTo.Num()))return Fail(TEXT("niagara_resource_bindings_pin_owner_invalid"),TEXT("Pin owner/link inventory is invalid."));FPinIdentity PE;PE.Pointer=P;PE.Id=P->PinId;PE.Name=P->PinName;PE.Direction=P->Direction;PE.Type=P->PinType;PE.Links=P->LinkedTo;PE.DefaultValue=P->DefaultValue;PE.AutoDefault=P->AutogeneratedDefaultValue;PE.DefaultText=P->DefaultTextValue.ToString();PE.DefaultObject=P->DefaultObject;if(P->DefaultObject&&!Pin(P->DefaultObject))return false;if(P->PinType.PinSubCategoryObject.IsValid()&&!Pin(P->PinType.PinSubCategoryObject.Get()))return false;NE.Pins.Add(MoveTemp(PE));}
        E.Nodes.Add(MoveTemp(NE));}
    TSet<UEdGraphPin*> Known;for(const auto& N:E.Nodes)for(const auto& P:N.Pins)Known.Add(P.Pointer);for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links){if(!L||!Known.Contains(L))return Fail(TEXT("niagara_resource_bindings_unresolved_link_identity"),TEXT("Null/cross-graph link is outside captured exact pin membership; no stale link dereference."));}
    Graphs.Add(MoveTemp(E));return true;
}
bool FWork::GraphForWrite(UNiagaraGraph* G)
{
    if(!Admit(TEXT("graph_writer_admission"))||!Own(G))return false;bool Captured=false;
    for(const auto& S:Sources){if(S.Graph.Get()==G){if(!Own(S.Source.Get()))return false;Captured=true;}}
    TSet<UNiagaraScriptSource*> Sharing;int32 EmitterOwners=0;bool SystemOwner=false;auto HasGraph=[&](UNiagaraScriptSourceBase* Base){auto* Source=Cast<UNiagaraScriptSource>(Base);if(!Source)return false;if(!Live(Source)){Fail(TEXT("niagara_resource_bindings_shared_source_unresolved"),TEXT("Unloaded source prevents exact shared-graph admission."));return false;}if(Source->NodeGraph==G){Sharing.Add(Source);return true;}return false;};for(const auto& E:Emitters){if(HasGraph(E.Source.Get()))++EmitterOwners;}for(const auto& E:ScriptBindings){const bool Matches=HasGraph(E.Source.Get());if(Matches&&(E.Script.Get()==Spawn.Get()||E.Script.Get()==Update.Get()))SystemOwner=true;}if(!R.Error.IsEmpty())return false;if(Sharing.Num()>1||EmitterOwners>1||(EmitterOwners>0&&SystemOwner))return Fail(TEXT("niagara_resource_bindings_shared_graph_writer_unsupported"),TEXT("The selected graph is shared by multiple source/owner roles. Refused before first Modify; no duplicate or unselected owner mutation."));
    int32 ScratchRoles=0,RegularRoles=EmitterOwners+(SystemOwner?1:0);auto ScratchUses=[&](UNiagaraScript* P){auto* S=P?Cast<UNiagaraScriptSource>(P->GetLatestSource()):nullptr;return S&&S->NodeGraph==G;};for(const auto& P:Scratch)if(ScratchUses(P.Get()))++ScratchRoles;for(const auto& E:Emitters){for(const auto& P:E.ScratchScripts)if(ScratchUses(P.Get()))++ScratchRoles;for(const auto& P:E.ParentScripts)if(ScratchUses(P.Get()))++ScratchRoles;}if(ScratchRoles>1||(ScratchRoles>0&&RegularRoles>0))return Fail(TEXT("niagara_resource_bindings_shared_scratch_graph_unsupported"),TEXT("Mutable scratch graph is shared with another scratch or regular owner role."));
    if(!Captured)return Fail(TEXT("niagara_resource_bindings_uncaptured_graph"),TEXT("Mutable graph has no captured exact source binding."));
    for(const auto& E:Graphs){if(E.Graph.Get()==G){for(const auto& N:E.Nodes)if(!Own(N.Node.Get()))return false;}}
    return true;
}
FPinIdentity* FindExpectedPin(FWork& W,UEdGraphPin* P)
{for(auto& G:W.Graphs)for(auto& N:G.Nodes)for(auto& E:N.Pins)if(E.Pointer==P)return &E;return nullptr;}
bool FWork::PinForWrite(UEdGraphPin* P)
{
    if(!Admit(TEXT("pin_writer_admission"))||!P)return Fail(TEXT("niagara_resource_bindings_pin_unavailable"),TEXT("Pin unavailable."));
    auto* E=FindExpectedPin(*this,P);if(!E)return Fail(TEXT("niagara_resource_bindings_uncaptured_pin"),TEXT("Pin is not in captured exact node membership; stale pointer not dereferenced."));
    auto* N=P->GetOwningNodeUnchecked();auto* G=Cast<UNiagaraGraph>(N->GetGraph());if(!GraphForWrite(G)||!Own(N))return false;
    for(auto* L:E->Links){if(!L||!FindExpectedPin(*this,L))return Fail(TEXT("niagara_resource_bindings_external_pin_link_unsupported"),TEXT("Linked pin is outside captured mutable graph(s)."));auto* LN=L->GetOwningNodeUnchecked();if(LN->GetGraph()!=G||!Own(LN))return Fail(TEXT("niagara_resource_bindings_external_pin_link_unsupported"),TEXT("Cross-graph linked writer is unsupported before mutation."));}
    return true;
}
bool FWork::BreakLinks(UEdGraphPin* P,bool Notify)
{
    if(!PinForWrite(P)||!SDK(TEXT("BreakAllPinLinks"),true))return false;auto* E=FindExpectedPin(*this,P);const TArray<UEdGraphPin*> Links=E->Links;
    for(auto* L:Links){auto* Other=FindExpectedPin(*this,L);if(!Other)return Fail(TEXT("niagara_resource_bindings_link_plan_unavailable"),TEXT("Captured reciprocal pin unavailable."));Other->Links.Remove(P);}E->Links.Empty();
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
    FGraphIdentity* Expected=Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return Fail(TEXT("niagara_resource_bindings_missing_graph_plan"),TEXT("Captured graph plan unavailable."));
    const FNodeIdentity* NE=Expected->Nodes.FindByPredicate([&](const auto& E){return E.Node.Get()==N;});if(!NE)return Fail(TEXT("niagara_resource_bindings_node_already_removed"),TEXT("Exact captured node no longer belongs to graph."));TArray<UEdGraphPin*> Removing;for(const auto& P:NE->Pins)Removing.Add(P.Pointer);
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
    if(Direct&&(!FPackageName::IsValidLongPackageName(P->GetName(),false)||!FPackageName::TryConvertLongPackageNameToFilename(P->GetName(),Path,FPackageName::GetAssetPackageExtension())))return Fail(TEXT("niagara_resource_bindings_save_path_invalid"),TEXT("Exact package filename unavailable."));
    if(!SDK(Direct?TEXT("SavePackage"):TEXT("SaveLoadedAsset"),true))return false;R.bSaveAttempted=true;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_package"),P->GetName());Row->SetStringField(TEXT("requested_asset"),O->GetPathName());bool Saved=false;
    if(Direct){FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;Saved=UPackage::SavePackage(P,O,*Path,Args);}else Saved=UEditorAssetLibrary::SaveLoadedAsset(O,false);
    R.bSaved|=Saved;SaveHealth&=Saved;if(Saved&&Direct&&Filename)*Filename=Path;Row->SetStringField(TEXT("requested_filename"),Path);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Saved);Row->SetBoolField(TEXT("actual_saved_history"),R.bSaved);Saves.Add(MakeShared<FJsonValueObject>(Row));if(!After(Direct?TEXT("SavePackage"):TEXT("SaveLoadedAsset"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}
    const bool DirtyAfter=P->IsDirty();Row->SetStringField(TEXT("package_after"),P->GetName());Row->SetBoolField(TEXT("dirty_after"),DirtyAfter);if(Saved&&DirtyAfter){SaveHealth=false;Fail(TEXT("niagara_resource_bindings_saved_package_dirty"),TEXT("Save returned true but captured package is dirty after SDK callbacks. Save history retained; dirty flag was not cleared or retried."));return false;}return Saved;
}
bool FWork::WaitAdmission(bool Wait,bool WillRequest)
{
    if(!Wait)return true;if(!SDK(TEXT("compile_wait_admission")))return false;bool Pending=System->HasOutstandingCompilationRequests(true)||System->HasActiveCompilations();if(!After(TEXT("compile_wait_admission")))return false;
    if(DeferredMode)return true;
    if(WillRequest||Pending)return Fail(TEXT("niagara_resource_bindings_async_wait_unsupported"),TEXT("Actual requested/pending compile wait needs a deferred final result, exact lifetime and original save/absence continuation. This synchronous implementation rejects before writer; no GT sleep/flush, fake timeout or wait=False substitution."));return true;
}
TSharedPtr<FJsonObject> FWork::WaitSnapshot(bool Wait,double Timeout,double Poll)
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0.0);if(!System.IsValid()){O->SetBoolField(TEXT("outstanding_compilation_requests_after"),false);O->SetBoolField(TEXT("active_compilations_after"),false);return O;}
    if(!SDK(TEXT("compile_status_snapshot")))return O;const bool Pending=System->HasOutstandingCompilationRequests(true),Active=System->HasActiveCompilations();O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Pending);O->SetBoolField(TEXT("active_compilations_after"),Active);if(!After(TEXT("compile_status_snapshot")))return O;if(Wait&&(Pending||Active)){Fail(TEXT("niagara_resource_bindings_async_wait_unsupported"),TEXT("Compile became pending after actual mutation/save; partial history retained. No blocking or replay."));O->SetBoolField(TEXT("actual_wait_implemented"),false);}return O;
}

struct FResourceCompileWait
{
 TSharedPtr<FJsonObject> Result=MakeShared<FJsonObject>();
 double Started=0,Next=0,Timeout=0,Poll=.1;int32 Iterations=0;bool Done=false;
 FResourceCompileWait(double Seconds,double Interval):Started(FPlatformTime::Seconds()),Timeout(FMath::Clamp(Seconds,0.0,60.0)),Poll(FMath::Clamp(Interval,.02,1.0))
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
void BeginResourceWait(FWork& W,UNiagaraSystem* System,double Timeout,double Poll,TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Completion)
{
 auto Wait=MakeShared<FResourceCompileWait>(Timeout,Poll);
 W.R.Data->SetObjectField(TEXT("compile_wait_partial"),Wait->Result);
 W.DeferredStep=[&W,System,Wait,Completion=MoveTemp(Completion)]()mutable
 {
  if(!Wait->Tick(W,System))return false;
  if(!W.Admit(TEXT("resource_wait_completion_admission")))return true;
  TSharedPtr<FJsonObject> Legacy=Completion(Wait->Result);
  if(Legacy.IsValid())W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
  return true;
 };
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
FString GraphMetadataSignature(UNiagaraGraph* G){return GraphMetadataWithout(G,nullptr);}

FString FunctionNameForPath(const UNiagaraNodeFunctionCall* N)
{
    FString Out;if(!N||!Current->Step(TEXT("GetFunctionName"),false,[&]{Out=N->GetFunctionName();}))return {};
    Current->Budget(0,Out.Len());return Out;
}
FNiagaraTypeDefinition PinTypeForPath(UEdGraphPin* P)
{
    FNiagaraTypeDefinition Out;
    if(!ContinueRead()||!FindExpectedPin(*Current,P)){Current->Fail(TEXT("niagara_resource_bindings_pin_unresolved"),TEXT("Pin is outside captured exact graph membership."));return Out;}
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
    if(!W.Budget(Variables.Num())||Variables.Num()>16384){W.Fail(TEXT("niagara_resource_bindings_store_bound"),TEXT("Exposed parameter inventory exceeds bound."));return Out;}
    for(const auto& V:Variables)
    {
        FUserValue E;E.Variable=V;if(!Store.RedirectUserVariable(E.Variable)){W.Fail(TEXT("niagara_resource_bindings_user_alias_unresolved"),TEXT("Captured User alias could not be resolved to the exact full-name variable."));return Out;}if(V.GetType().IsDataInterface()){auto* DI=Store.GetDataInterface(V);E.DI=DI;if(DI&&!W.Pin(DI))return Out;}
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
    if(FullParameterCount<0||FullParameterCount>65536||!W.Budget(FullParameterCount))return W.Fail(TEXT("niagara_resource_bindings_full_user_store_bound"),TEXT("Full exposed declaration inventory exceeds the bounded canonical User check."));
    TArray<FNiagaraVariable> FullParametersForUser;CanonicalStore.GetParameters(FullParametersForUser);
    int32 ExactFullMatches=0;for(const FNiagaraVariable& FullParameter:FullParametersForUser)if(FullParameter.GetName()==User.GetName()&&FullParameter.GetType()==User.GetType())++ExactFullMatches;
    FNiagaraVariable CanonicalProbe=User;const bool CanonicalRedirected=CanonicalStore.RedirectUserVariable(CanonicalProbe);
    W.Prefix->SetStringField(TEXT("last_user_full_name_checked"),User.GetName().ToString());
    W.Prefix->SetStringField(TEXT("last_user_full_name_redirect"),CanonicalProbe.GetName().ToString());
    if(ExactFullMatches!=1||!CanonicalRedirected||CanonicalProbe.GetName()!=User.GetName()||CanonicalProbe.GetType()!=User.GetType())return W.Fail(TEXT("niagara_resource_bindings_user_canonical_redirect_unsupported"),TEXT("Selected full typed User declaration is absent, ambiguous, or redirects to another declaration; no next store or graph writer dispatched."));
    return true;
}
bool VerifyUnrelatedUserValues(FWork& W,const TArray<FUserValue>& Before,const FNiagaraVariable& Selected,bool AllowNew)
{
    if(!VerifyCanonicalUserForPath(W,Selected))return false;
    auto After=CaptureUserValues(W);if(!ContinueRead())return false;
    int32 OtherBefore=0,OtherAfter=0;
    auto SelectedMatch=[&](const FUserValue& E){return E.Variable.GetName()==Selected.GetName()&&E.Variable.GetType()==Selected.GetType();};
    for(const auto& E:Before){if(SelectedMatch(E))continue;++OtherBefore;const auto* A=After.FindByPredicate([&](const auto& V){return V.Variable.GetName()==E.Variable.GetName()&&V.Variable.GetType()==E.Variable.GetType();});if(!A||!SameUserValue(E,*A))return W.Fail(TEXT("niagara_resource_bindings_unselected_store_changed"),TEXT("Unselected exposed parameter changed across SDK call."));}
    for(const auto& E:After)if(!SelectedMatch(E))++OtherAfter;
    const int32 SelectedCount=After.FilterByPredicate(SelectedMatch).Num();
    return (OtherBefore==OtherAfter&&SelectedCount==1&&(AllowNew||Before.ContainsByPredicate(SelectedMatch)))||W.Fail(TEXT("niagara_resource_bindings_store_shape_changed"),TEXT("Exposed parameter set differs from the admitted selected transition."));
}
bool PreflightStackMap(FWork& W,UNiagaraNodeFunctionCall* Function)
{
    if(!ContinueRead())return false;auto* G=Cast<UNiagaraGraph>(Function->GetGraph());if(!G||!W.GraphForWrite(G)||!W.Own(Function))return false;
    TArray<UEdGraphPin*> Inputs;for(auto* P:Function->Pins)if(P->Direction==EGPD_Input&&PinTypeForPath(P)==FNiagaraTypeDefinition::GetParameterMapDef())Inputs.Add(P);
    if(!ContinueRead())return false;
    if(Inputs.Num()!=1||Inputs[0]->LinkedTo.Num()!=1||!FindExpectedPin(W,Inputs[0]->LinkedTo[0]))return W.Fail(TEXT("niagara_resource_bindings_stack_map_shape_unsupported"),TEXT("The module must have one exact captured ParameterMap input and one upstream link before public stack SDK."));
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
    if(!GraphMetadataWithout(G,LinkedUser).Equals(OtherMetadataBefore,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_resource_bindings_unselected_metadata_changed"),TEXT("Unselected graph metadata changed during the requested stack transition."));
    if(G->Schema!=Before.Schema.Get()||G->Nodes.Num()<Before.Nodes.Num()||G->Nodes.Num()>Before.Nodes.Num()+1)return W.Fail(TEXT("niagara_resource_bindings_unexpected_node_delta"),TEXT("Public stack operation changed an unexpected number of nodes."));
    for(int32 I=0;I<Before.Nodes.Num();++I)if(G->Nodes[I]!=Before.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_resource_bindings_retained_node_changed"),TEXT("Retained node order/identity changed during stack SDK."));
    UEdGraphNode* NewNode=G->Nodes.Num()>Before.Nodes.Num()?G->Nodes.Last().Get():nullptr;
    if(NewNode&&(!Live(NewNode)||!NewNode->IsIn(W.System.Get())||NewNode->GetOutermost()!=W.System->GetOutermost()||NewNode->GetGraph()!=G))return W.Fail(TEXT("niagara_resource_bindings_generated_node_owner_invalid"),TEXT("New stack node is outside the requested graph/System."));
    const TCHAR* NewClass=WasOverride?TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"):TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet");
    if(NewNode&&!NewNode->GetClass()->GetPathName().Equals(NewClass,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_resource_bindings_generated_node_class_invalid"),TEXT("Public stack SDK created an unexpected node class."));
    if(!WasOverride&&!NewNode)return W.Fail(TEXT("niagara_resource_bindings_link_node_missing"),TEXT("User binding did not append its exact getter node."));
    TSet<UEdGraphPin*> LivePins;
    for(UEdGraphNode* N:G->Nodes){if(!Live(N)||!W.Pin(N))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_resource_bindings_generated_pin_invalid"),TEXT("Post-SDK pin owner is invalid."));LivePins.Add(P);}}
    if(Returned&&!LivePins.Contains(Returned))return W.Fail(TEXT("niagara_resource_bindings_returned_pin_expired"),TEXT("Returned override pin was removed during SDK callback; stale pointer not read."));
    if(ExistingOverride&&!LivePins.Contains(ExistingOverride))return W.Fail(TEXT("niagara_resource_bindings_existing_override_expired"),TEXT("Captured existing override pin disappeared during stack SDK; stale pointer not dereferenced."));
    UEdGraphPin* FunctionInput=Function?MapPin(Function,EGPD_Input):nullptr;if(Function&&(!FunctionInput||FunctionInput->LinkedTo.Num()!=1))return W.Fail(TEXT("niagara_resource_bindings_stack_map_changed"),TEXT("Module map input no longer has exact upstream."));
    UEdGraphNode* OverrideNode=WasOverride?(Returned?Returned->GetOwningNodeUnchecked():nullptr):ExistingOverride->GetOwningNodeUnchecked();
    if(!OverrideNode||!OverrideNode->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_resource_bindings_override_owner_invalid"),TEXT("Exact override owner is not the public reflected ParameterMapSet class."));
    UEdGraphPin* OverrideInput=MapPin(OverrideNode,EGPD_Input);UEdGraphPin* OverrideOutput=MapPin(OverrideNode,EGPD_Output);
    if(!OverrideInput||!OverrideOutput||OverrideInput->LinkedTo.Num()!=1)return W.Fail(TEXT("niagara_resource_bindings_override_stack_invalid"),TEXT("Override ParameterMap input must have one captured upstream link."));
    UEdGraphPin* Prior=OverrideInput->LinkedTo[0];if(!LivePins.Contains(Prior))return W.Fail(TEXT("niagara_resource_bindings_upstream_unresolved"),TEXT("Override upstream is outside current exact graph pins."));
    UEdGraphPin* NewGetInput=nullptr;UEdGraphPin* NewGetOutput=nullptr;
    if(!WasOverride)
    {
        NewGetInput=MapPin(NewNode,EGPD_Input);int32 Semantic=0;
        for(auto* P:NewNode->Pins)if(P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P)){++Semantic;NewGetOutput=P;}
        if(Semantic!=1||!NewGetInput||!NewGetOutput||!LinkedUser||NewGetOutput->PinName!=LinkedUser->GetName()||UEdGraphSchema_Niagara::PinToTypeDefinition(NewGetOutput)!=LinkedUser->GetType()||NewGetInput->LinkedTo!=TArray<UEdGraphPin*>{Prior}||NewGetOutput->LinkedTo!=TArray<UEdGraphPin*>{ExistingOverride})return W.Fail(TEXT("niagara_resource_bindings_user_getter_shape_invalid"),TEXT("Generated getter does not match the exact requested typed User link."));
    }
    for(const auto& NE:Before.Nodes)
    {
        auto* N=NE.Node.Get();if(N->NodeGuid!=NE.Id||N->NodePosX!=NE.X||N->NodePosY!=NE.Y||N->GetGraph()!=G||!FunctionFieldsExact(NE,N))return W.Fail(TEXT("niagara_resource_bindings_retained_node_fields_changed"),TEXT("Retained node GUID/position/graph changed during stack SDK."));int32 AddedPins=N->Pins.Num()-NE.Pins.Num();const bool IsOldOverride=WasOverride&&N==OverrideNode;
        if(AddedPins!=(IsOldOverride&&!ExistingOverride?1:0))return W.Fail(TEXT("niagara_resource_bindings_unexpected_pin_delta"),TEXT("Retained node pin count differs from requested override insertion."));
        TArray<UEdGraphPin*> Remaining;for(auto* P:N->Pins)if(!(IsOldOverride&&!ExistingOverride&&P==Returned))Remaining.Add(P);
        if(Remaining.Num()!=NE.Pins.Num())return W.Fail(TEXT("niagara_resource_bindings_pin_plan_invalid"),TEXT("Override insertion did not yield exact retained pin set."));
        for(int32 J=0;J<NE.Pins.Num();++J)
        {
            const auto& PE=NE.Pins[J];auto* P=Remaining[J];if(P!=PE.Pointer||!PinHeaderSame(PE,P))return W.Fail(TEXT("niagara_resource_bindings_retained_pin_changed"),TEXT("Retained exact pin/type/name changed during SDK."));
            TArray<UEdGraphPin*> Expected=PE.Links;
            if(WasOverride&&NewNode)
            {
                UEdGraphPin* OldFunctionInput=FunctionInput;
                const FPinIdentity* OldInput=FindExpectedPin(W,OldFunctionInput);
                if(!OldInput||OldInput->Links.Num()!=1)return W.Fail(TEXT("niagara_resource_bindings_original_map_unresolved"),TEXT("Original module upstream no longer known."));
                UEdGraphPin* OldPrior=OldInput->Links[0];const auto* OldPriorExpected=FindExpectedPin(W,OldPrior);
                if(!OldPriorExpected)return false;
                if(P==OldPrior)Expected={OverrideInput};
                else if(P==OldFunctionInput){Expected.Remove(OldPrior);Expected.AddUnique(OverrideOutput);}
                else if(OldPriorExpected->Links.Contains(P)){Expected.Remove(OldPrior);Expected.AddUnique(OverrideOutput);}
            }
            if(!WasOverride){if(P==ExistingOverride)Expected={NewGetOutput};else if(P==Prior)Expected.AddUnique(NewGetInput);}
            if(P->LinkedTo!=Expected)return W.Fail(TEXT("niagara_resource_bindings_unselected_links_changed"),TEXT("Retained links differ from the single requested public stack transition."));
        }
    }
    if(WasOverride&&Returned&&(Returned->PinName!=Alias.GetParameterHandleString()||Returned->Direction!=EGPD_Input||UEdGraphSchema_Niagara::PinToTypeDefinition(Returned)!=Type||(!ExistingOverride&&Returned->LinkedTo.Num()!=0)||(ExistingOverride&&Returned!=ExistingOverride)))return W.Fail(TEXT("niagara_resource_bindings_override_pin_shape_invalid"),TEXT("Override pin is not exact typed empty requested alias."));
    if(WasOverride&&NewNode){for(auto* P:NewNode->Pins)if(P!=OverrideInput&&P!=OverrideOutput&&P!=Returned&&!IsNiagaraDynamicAddPin(P))return W.Fail(TEXT("niagara_resource_bindings_override_extra_pin"),TEXT("Generated override contains an unexpected semantic pin."));}
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
    if(!W.PinForWrite(Override)||Override->LinkedTo.Num()!=0||!PreflightStackMap(W,Function))return W.Fail(TEXT("niagara_resource_bindings_override_not_empty"),TEXT("Exact override must be empty before User link SDK."));
    auto* G=Cast<UNiagaraGraph>(Function->GetGraph());auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return false;const FGraphIdentity Before=*Expected;const FString MetadataBefore=GraphMetadataWithout(G,&User);
    UNiagaraScriptVariable* ExistingMetadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&!W.Own(ExistingMetadata))return false;
    if(!W.SDK(TEXT("SetLinkedParameterValueForFunctionInput"),true))return false;auto Row=W.MutationRow(TEXT("SetLinkedParameterValueForFunctionInput"));Row->SetStringField(TEXT("module_guid"),Function->NodeGuid.ToString());Row->SetStringField(TEXT("user_parameter"),User.GetName().ToString());
    {TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);TSet<FNiagaraVariableBase> Known;Known.Add(User);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Override,User,Known);Row->SetBoolField(TEXT("SDK_returned"),true);if(!CaptureTransitionResult(W,G,Before,Function,FNiagaraParameterHandle(),User.GetType(),nullptr,Override,&User,false,MetadataBefore))return false;}
    UNiagaraScriptVariable* Metadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&Metadata!=ExistingMetadata)return W.Fail(TEXT("niagara_resource_bindings_metadata_identity_changed"),TEXT("Selected existing metadata was replaced during binding SDK."));if(!Metadata||!W.Own(Metadata)||Metadata->DefaultMode!=ENiagaraDefaultMode::FailIfPreviouslyNotSet)return W.Fail(TEXT("niagara_resource_bindings_binding_metadata_readback_failed"),TEXT("Exact selected User metadata was not observed after public binding."));
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
    if(Type==FNiagaraTypeDefinition::GetIntDef()){double X=0;if(Value->TryGetNumber(X)&&(!FMath::IsFinite(X)||X<MIN_int32||X>MAX_int32))return Current->Fail(TEXT("niagara_resource_bindings_numeric_range_unsupported"),TEXT("Numeric default is outside the SDK int32 conversion range."));}
    else if(Type==FNiagaraTypeDefinition::GetFloatDef()){double X=0;if(Value->TryGetNumber(X)&&!FloatSafe(X))return Current->Fail(TEXT("niagara_resource_bindings_numeric_range_unsupported"),TEXT("Numeric default is outside finite float range."));}
    else if(Value->Type==EJson::Array&&Type!=FNiagaraTypeDefinition::GetPositionDef()){for(const auto& V:Value->AsArray()){double X=0;if(V&&V->TryGetNumber(X)&&!FloatSafe(X))return Current->Fail(TEXT("niagara_resource_bindings_numeric_range_unsupported"),TEXT("Vector/color default is outside finite float range."));}}
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

int32 IndexParam(const TSharedPtr<FJsonObject>& P,const TCHAR* K,int32 Default=INDEX_NONE){double N=Default;if(!P->TryGetNumberField(K,N))return Default;if(!FMath::IsFinite(N)||N<MIN_int32||N>MAX_int32){Current->Fail(TEXT("niagara_resource_bindings_index_invalid"),FString(K)+TEXT(" requires a safe finite int32 conversion."));return Default;}int32 Out=Default;P->TryGetNumberField(K,Out);return Out;}


bool FunctionFieldsExact(const FNodeIdentity& E,UEdGraphNode* N)
{if(!E.Function)return true;auto* F=Cast<UNiagaraNodeFunctionCall>(N);return F&&F->FunctionScript==E.FunctionScript.Get()&&F->SelectedScriptVersion==E.FunctionVersion&&F->FunctionScriptAssetObjectPath==E.FunctionAsset&&F->FunctionSpecifiers.OrderIndependentCompareEqual(E.Specifiers)&&FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&F->Signature,&E.FunctionSignature,0);}
bool ResolveUser(FWork& W,const FString& Requested,const FNiagaraTypeDefinition& Type,FNiagaraVariable& User,bool& Existed)
{
 if(!W.Admit(TEXT("resolve_user_parameter")))return false;User=FNiagaraVariable(Type,FName(*Requested));Existed=false;const auto& Store=W.System->GetExposedParameters();TArray<FNiagaraVariable> Full;Store.GetParameters(Full);if(Full.Num()>65536||!W.Budget(Full.Num()))return W.Fail(TEXT("niagara_resource_bindings_user_store_bound"),TEXT("Full typed declaration inventory exceeds bound."));int32 Count=0;for(const auto& V:Full){if(V.GetName()!=User.GetName())continue;if(V.GetType()!=Type)return W.Fail(TEXT("niagara_resource_bindings_user_type_conflict"),TEXT("Requested full User name has an existing incompatible type."));++Count;}
 FNiagaraVariable Probe=User;const bool Redirected=Store.RedirectUserVariable(Probe);if(Redirected&&(Probe.GetName()!=User.GetName()||Probe.GetType()!=User.GetType()))return W.Fail(TEXT("niagara_resource_bindings_user_alias_collision"),TEXT("Requested normalized full typed User name redirects to another declaration; no store/graph writer."));if(Count>1)return W.Fail(TEXT("niagara_resource_bindings_user_declaration_ambiguous"),TEXT("Requested full typed declaration appears more than once."));Existed=Count==1;if(Existed)return VerifyCanonicalUserForPath(W,User);return true;
}
bool PreflightMapValue(FWork& W,UEdGraphPin* P)
{
 if(!W.PinForWrite(P))return false;auto* N=P->GetOwningNodeUnchecked();if(!N->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive)||P->Direction!=EGPD_Input||IsNiagaraDynamicAddPin(P))return W.Fail(TEXT("niagara_resource_bindings_map_owner_unsupported"),TEXT("Value target must be a captured exact ParameterMapSet input."));auto* Source=MapPin(N,EGPD_Input);if(!Source||Source==P||Source->LinkedTo.Num()!=1||!FindExpectedPin(W,Source->LinkedTo[0])||Source->LinkedTo[0]->Direction!=EGPD_Output||Source->LinkedTo[0]->GetOwningNodeUnchecked()->GetGraph()!=N->GetGraph())return W.Fail(TEXT("niagara_resource_bindings_map_upstream_invalid"),TEXT("ParameterMapSet requires one captured exact upstream before public linked/DI SDK."));return true;
}
bool AdoptDITransition(FWork& W,UNiagaraGraph* G,const FGraphIdentity& Before,UEdGraphPin* Override,UClass* Class,UNiagaraDataInterface* DI,UNiagaraNodeInput*& CreatedNode)
{
 if(!W.Admit(TEXT("DI_transition_context")))return false;if(G->Schema!=Before.Schema.Get()||G->Nodes.Num()!=Before.Nodes.Num()+1||!GraphMetadataSignature(G).Equals(Before.Meta,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_resource_bindings_DI_graph_delta_invalid"),TEXT("DI creation changed more than one node or unrelated metadata."));for(int32 I=0;I<Before.Nodes.Num();++I)if(G->Nodes[I]!=Before.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_resource_bindings_retained_node_changed"),TEXT("DI creation replaced/reordered a retained node."));auto* New=G->Nodes.Last().Get();if(!Live(New)||New->GetClass()!=UNiagaraNodeInput::StaticClass()||New->GetGraph()!=G||!New->IsIn(W.System.Get())||New->GetPackage()!=W.System->GetPackage())return W.Fail(TEXT("niagara_resource_bindings_DI_node_owner_invalid"),TEXT("New DI node must be exact stock NodeInput in the requested System/graph/package."));TSet<UEdGraphPin*> Pins;for(UEdGraphNode* N:G->Nodes){if(!Live(N)||!W.Pin(N))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_resource_bindings_DI_pin_invalid"),TEXT("New graph pin membership is malformed."));Pins.Add(P);}}if(!Pins.Contains(Override))return W.Fail(TEXT("niagara_resource_bindings_override_expired"),TEXT("Returned exact override disappeared; stale pointer not dereferenced."));UEdGraphPin* Output=nullptr;int32 OutputCount=0;for(auto* P:New->Pins){if(P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P)){++OutputCount;Output=P;}else if(!P->LinkedTo.IsEmpty())return W.Fail(TEXT("niagara_resource_bindings_DI_extra_link"),TEXT("DI node contains an unexpected linked pin."));}if(OutputCount!=1||!Output)return W.Fail(TEXT("niagara_resource_bindings_DI_output_invalid"),TEXT("Expected one actual typed DI output."));
 for(const auto& E:Before.Nodes){auto* N=E.Node.Get();if(N->GetGraph()!=G||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num()||!FunctionFieldsExact(E,N))return W.Fail(TEXT("niagara_resource_bindings_retained_node_changed"),TEXT("Retained node/function binding changed during DI SDK."));for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& PE=E.Pins[J];if(P!=PE.Pointer||!PinHeaderSame(PE,P))return W.Fail(TEXT("niagara_resource_bindings_retained_pin_changed"),TEXT("Retained pin/default/type changed during DI SDK."));const TArray<UEdGraphPin*> Expected=P==Override?TArray<UEdGraphPin*>{Output}:PE.Links;if(P->LinkedTo!=Expected)return W.Fail(TEXT("niagara_resource_bindings_DI_unselected_links_changed"),TEXT("Only exact override-to-new-DI edge was admitted."));}}
 auto* Input=static_cast<UNiagaraNodeInput*>(New);if(!Live(DI)||DI->GetClass()!=Class||DI->GetOuter()!=Input||DI->GetPackage()!=W.System->GetPackage()||ReflectedNodeDI(Input)!=DI||Input->Input.GetType()!=FNiagaraTypeDefinition(Class)||UEdGraphSchema_Niagara::PinToTypeDefinition(Output)!=FNiagaraTypeDefinition(Class)||Output->LinkedTo!=TArray<UEdGraphPin*>{Override}||Override->LinkedTo!=TArray<UEdGraphPin*>{Output})return W.Fail(TEXT("niagara_resource_bindings_DI_typed_readback_failed"),TEXT("Created exact DI/owner/public output does not match requested type and reciprocal edge."));if(!W.Own(Input)||!W.Own(DI))return false;CreatedNode=Input;auto* Source=W.Sources.FindByPredicate([&](const auto& S){return S.Graph.Get()==G;});if(!Source)return false;W.Graphs.RemoveAll([&](const auto& E){return E.Graph.Get()==G;});return W.CaptureGraph(Source->Source.Get());
}
UNiagaraDataInterface* CreateDI(FWork& W,UEdGraphPin* Override,UClass* Class,const FString& Name,bool RT)
{
 if(!PreflightMapValue(W,Override)||!Override->LinkedTo.IsEmpty()||!Class||!Class->IsChildOf(UNiagaraDataInterface::StaticClass())||!W.Pin(Class))return W.Fail(TEXT("niagara_resource_bindings_DI_precondition_failed"),TEXT("DI SDK needs an empty exact value override and public DI class.")),nullptr;auto* G=Cast<UNiagaraGraph>(Override->GetOwningNodeUnchecked()->GetGraph());const auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return nullptr;const FGraphIdentity Before=*Expected;UNiagaraDataInterface* DI=nullptr;UNiagaraNodeInput* Node=nullptr;if(!W.SDK(TEXT("SetDataInterfaceValueForFunctionInput"),true))return nullptr;auto Row=W.MutationRow(TEXT("SetDataInterfaceValueForFunctionInput"));Row->SetStringField(TEXT("input_alias"),Name);Row->SetStringField(TEXT("class"),Class->GetPathName());{TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput(*Override,Class,Name,DI);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),DI!=nullptr);W.R.bChanged|=DI!=nullptr;if(!AdoptDITransition(W,G,Before,Override,Class,DI,Node))return nullptr;}if(!W.After(TEXT("SetDataInterfaceValueForFunctionInput")))return nullptr;Row->SetStringField(TEXT("actual_DI_path"),DI->GetPathName());Row->SetStringField(TEXT("actual_input_node_path"),Node->GetPathName());FExpectedDI E;E.Node=Node;E.DI=DI;E.RT=RT;E.Input=Node->Input;E.Sort=Node->CallSortPriority;E.Usage=Node->Usage;if(RT){auto* D=Cast<UNiagaraDataInterfaceRenderTarget2D>(DI);E.User=D->RenderTargetUserParameter.Parameter;E.Inherit=D->bInheritUserParameterSettings;}else{auto* D=Cast<UNiagaraDataInterfaceTexture>(DI);E.User=D->TextureUserParameter.Parameter;E.Texture=D->Texture;}W.ExpectedDI.Add(E);return DI;
}
bool LinkMapUser(FWork& W,UEdGraphPin* Target,const FNiagaraVariable& User)
{
 if(!PreflightMapValue(W,Target)||!Target->LinkedTo.IsEmpty()||!VerifyCanonicalUserForPath(W,User))return false;auto* G=Cast<UNiagaraGraph>(Target->GetOwningNodeUnchecked()->GetGraph());auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return false;const FGraphIdentity Before=*Expected;const FString Meta=GraphMetadataWithout(G,&User);UNiagaraScriptVariable* ExistingMetadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&!W.Own(ExistingMetadata))return false;if(!W.SDK(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"),true))return false;auto Row=W.MutationRow(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"));Row->SetStringField(TEXT("target_node"),Target->GetOwningNodeUnchecked()->NodeGuid.ToString());Row->SetStringField(TEXT("target_pin"),Target->PinId.ToString());Row->SetStringField(TEXT("full_user_parameter"),User.GetName().ToString());{TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);TSet<FNiagaraVariableBase> Known;Known.Add(User);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Target,User,Known);Row->SetBoolField(TEXT("SDK_returned"),true);W.R.bChanged=true;if(!CaptureTransitionResult(W,G,Before,nullptr,FNiagaraParameterHandle(),User.GetType(),nullptr,Target,&User,false,Meta))return false;}UNiagaraScriptVariable* Metadata=G->GetAllMetaData().FindRef(User);if((ExistingMetadata&&Metadata!=ExistingMetadata)||!Metadata||!W.Own(Metadata)||Metadata->DefaultMode!=ENiagaraDefaultMode::FailIfPreviouslyNotSet)return W.Fail(TEXT("niagara_resource_bindings_user_metadata_readback_failed"),TEXT("Selected metadata identity/default mode changed unexpectedly."));return W.After(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"));
}
bool SetPinDefault(FWork& W,UEdGraphPin* P,const FString& Value)
{
 if(!W.PinForWrite(P))return false;const UEdGraphSchema* Schema=P->GetSchema();if(Schema&&Schema->GetClass()!=UEdGraphSchema_Niagara::StaticClass())return W.Fail(TEXT("niagara_resource_bindings_schema_unsupported"),TEXT("Only exact public Niagara default setter is supported."));if(Schema&&!W.Pin(Schema->GetClass()))return false;auto* E=FindExpectedPin(W,P);if(!W.Step(TEXT("ClearPinDefaultObjectAndText"),true,[&]{P->DefaultObject=nullptr;P->DefaultTextValue=FText::GetEmpty();E->DefaultObject=nullptr;E->DefaultText=FString();}))return false;if(!W.SDK(TEXT("Schema_TrySetDefaultValue"),true))return false;E->DefaultValue=Value;if(Schema)Schema->TrySetDefaultValue(*P,Value);else P->DefaultValue=Value;W.Prefix->SetStringField(TEXT("last_default_value_assigned"),Value);W.R.bChanged=true;return W.After(TEXT("Schema_TrySetDefaultValue"));
}


template<typename T>T* LoadResource(FWork& W,const FString& Path)
{
 if(Path.IsEmpty())return nullptr;if(Path.ToLower().Contains(TEXT("unrealmcp"))||Path.ToLower().Contains(TEXT("cubelessmcp"))||Path.ToLower().Contains(TEXT("toolsetregistry")))return W.Fail(TEXT("niagara_resource_bindings_dependency_rejected"),TEXT("MCP/Cubeless/Toolset resource dependency rejected.")),nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);T* O=nullptr;if(!W.Step(TEXT("LoadObject_Resource"),false,[&]{O=LoadObject<T>(nullptr,*Expected);W.Prefix->SetBoolField(TEXT("resource_load_returned_nonnull"),O!=nullptr);}))return nullptr;if(!O||!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!W.Pin(O))return W.Fail(TEXT("niagara_resource_bindings_resource_load_failed"),TEXT("Exact requested resource could not be loaded.")),nullptr;return O;
}
bool VerifyResourceBinding(FWork& W,UNiagaraNodeFunctionCall* Function,const FNiagaraParameterHandle& Alias,const FNiagaraVariable& User,const TArray<FUserValue>& Before,bool New,UNiagaraDataInterface* DI,UObject* ExpectedObject)
{
 if(!W.Admit(TEXT("resource_binding_typed_readback"))||!VerifyUnrelatedUserValues(W,Before,User,New))return false;auto* Override=FindNiagaraStackFunctionInputOverridePin(Function,Alias);if(!ContinueRead())return false;if(!Override||!FindExpectedPin(W,Override)||Override->LinkedTo.Num()!=1||!FindExpectedPin(W,Override->LinkedTo[0]))return W.Fail(TEXT("niagara_resource_bindings_override_readback_failed"),TEXT("Exact module override-to-DI edge not observed after compile/save."));auto* Node=Cast<UNiagaraNodeInput>(Override->LinkedTo[0]->GetOwningNodeUnchecked());if(!Node||ReflectedNodeDI(Node)!=DI||DI->GetOuter()!=Node||W.System->GetExposedParameters().GetUObject(User).Get()!=ExpectedObject)return W.Fail(TEXT("niagara_resource_bindings_resource_readback_failed"),TEXT("Actual selected User object/DI node binding differs from requested typed target."));return W.Admit(TEXT("resource_binding_verified"));
}
TSharedPtr<FJsonObject> HandleResource(FWork& W,const TSharedPtr<FJsonObject>& Params,bool RT)
{
 FString SystemPath,InputName;if(!Params->TryGetStringField(TEXT("system_path"),SystemPath))return W.Error(TEXT("Missing 'system_path' parameter"));if(!Params->TryGetStringField(TEXT("input_name"),InputName))return W.Error(TEXT("Missing 'input_name' parameter"));bool Allow=false;Params->TryGetBoolField(TEXT("allow_source_edit"),Allow);if(!Allow&&!IsTempGeneratedNiagaraPath(SystemPath))return W.Error(TEXT("Refusing to edit Niagara resource module input outside generated temp root."));const FString LowerPath=SystemPath.ToLower();if(LowerPath.Contains(TEXT("unrealmcp"))||LowerPath.Contains(TEXT("cubelessmcp"))||LowerPath.Contains(TEXT("toolsetregistry")))return W.Error(TEXT("MCP/Cubeless/Toolset System dependency rejected."));
 FString UserName=RT?TEXT("User.RT_IF_Deform"):TEXT("User.T_Mask");Params->TryGetStringField(TEXT("user_parameter_name"),UserName);UserName.TrimStartAndEndInline();if(UserName.IsEmpty())return W.Error(TEXT("'user_parameter_name' cannot be empty"));if(!UserName.StartsWith(TEXT("User.")))UserName=TEXT("User.")+UserName;FString AssetPath;Params->TryGetStringField(RT?TEXT("render_target_asset_path"):TEXT("texture_asset_path"),AssetPath);if(!RT)Params->TryGetStringField(TEXT("texture2d_asset_path"),AssetPath);AssetPath.TrimStartAndEndInline();bool Inherit=true,Overwrite=false,Save=true,Compile=true;Params->TryGetBoolField(TEXT("inherit_user_parameter_settings"),Inherit);Params->TryGetBoolField(TEXT("overwrite_existing"),Overwrite);Params->TryGetBoolField(TEXT("save"),Save);Params->TryGetBoolField(TEXT("request_compile"),Compile);bool Wait=false;double Timeout=10,Poll=.1;ReadNiagaraCompileWaitParams(Params,Wait,Timeout,Poll);W.R.bSaveRequested=Save;W.R.bCompileRequested=Compile;
 int32 EmitterIndex=IndexParam(Params,TEXT("emitter_index")),ModuleIndex=IndexParam(Params,TEXT("module_index"));FString EmitterName,ModuleName,ModuleGuid;Params->TryGetStringField(TEXT("emitter_name"),EmitterName);Params->TryGetStringField(TEXT("module_name"),ModuleName);Params->TryGetStringField(TEXT("module_node_guid"),ModuleGuid);if(!ContinueRead())return W.Error(W.R.Error);auto* System=LoadSystem(SystemPath);if(!System)return W.Error(TEXT("Failed to load Niagara system."));UTexture* Asset=nullptr;if(!AssetPath.IsEmpty()){Asset=RT?static_cast<UTexture*>(LoadResource<UTextureRenderTarget2D>(W,AssetPath)):static_cast<UTexture*>(LoadResource<UTexture2D>(W,AssetPath));if(!Asset)return W.Error(W.R.Error);}
 UNiagaraNodeFunctionCall* Function=nullptr;FVersionedNiagaraEmitterData* EmitterData=nullptr;FVersionedNiagaraEmitter Owner;FString ActualEmitterName,ActualModuleName;int32 ActualEmitterIndex=INDEX_NONE,ActualModuleIndex=INDEX_NONE;const TArray<FNiagaraEmitterHandle> Handles=System->GetEmitterHandles();for(int32 I=0;I<Handles.Num()&&!Function;++I){const auto& H=Handles[I];const FString Name=H.GetName().ToString();if((EmitterIndex!=INDEX_NONE&&EmitterIndex!=I)||(!EmitterName.IsEmpty()&&!EmitterName.Equals(Name,ESearchCase::IgnoreCase)))continue;auto* Data=H.GetEmitterData();if(!Data)continue;const auto Functions=CollectSortedNiagaraFunctionCalls(Data->GraphSource);if(!ContinueRead())return W.Error(W.R.Error);for(int32 J=0;J<Functions.Num();++J){auto* F=Functions[J];const FString FNameString=FunctionNameForPath(F);if(!ContinueRead())return W.Error(W.R.Error);const FString Guid=F->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);if((ModuleIndex==INDEX_NONE||ModuleIndex==J)&&(ModuleName.IsEmpty()||ModuleName.Equals(FNameString,ESearchCase::IgnoreCase))&&(ModuleGuid.IsEmpty()||ModuleGuid.Equals(Guid,ESearchCase::IgnoreCase))){Function=F;EmitterData=Data;Owner=H.GetInstance();ActualEmitterName=Name;ActualModuleName=FNameString;ActualEmitterIndex=I;ActualModuleIndex=J;break;}}}
 if(!Function||!EmitterData)return W.Error(TEXT("No matching Niagara module was found"));if(!PreflightStackMap(W,Function))return W.Error(W.R.Error);ENiagaraScriptUsage Usage=ENiagaraScriptUsage::Function;if(!W.Step(TEXT("GetOutputNodeUsage"),false,[&]{Usage=FNiagaraStackGraphUtilities::GetOutputNodeUsage(*Function);}))return W.Error(W.R.Error);FCompileConstantResolver Resolver(Owner,Usage,Function->DebugState);TArray<FNiagaraVariable> Inputs;TSet<FNiagaraVariable> Hidden;if(!W.Step(TEXT("GetStackFunctionInputs_Resolver"),false,[&]{FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Function,Inputs,Hidden,Resolver,FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly,true);}))return W.Error(W.R.Error);if(Inputs.Num()>65536||!W.Budget(Inputs.Num()))return W.Error(TEXT("Resolved stack input bound exceeded."));FNiagaraVariable Input;bool Found=false,Fallback=false;for(const auto& V:Inputs)if(NiagaraModuleInputNameMatches(V,InputName)){Input=V;Found=true;break;}
 if(!Found&&!RT&&Function->FunctionScript){auto* Source=Cast<UNiagaraScriptSource>(Function->FunctionScript->GetLatestSource());if(Source&&Source->NodeGraph){if(!W.CaptureGraph(Source))return W.Error(W.R.Error);for(const auto& Pair:Source->NodeGraph->GetAllMetaData()){const auto& T=Pair.Key.GetType();if(NiagaraModuleInputNameMatches(Pair.Key,InputName)&&T.IsDataInterface()&&T.GetClass()&&T.GetClass()->IsChildOf(UNiagaraDataInterfaceTexture::StaticClass())){Input=Pair.Key;Found=true;Fallback=true;break;}}}}
 if(!Found)return W.Error(TEXT("No matching module input was found: ")+InputName);const FNiagaraTypeDefinition Type=Input.GetType();UClass* DIClass=RT?UNiagaraDataInterfaceRenderTarget2D::StaticClass():UNiagaraDataInterfaceTexture::StaticClass();if(!Type.IsDataInterface()||!Type.GetClass()||!Type.GetClass()->IsChildOf(DIClass))return W.Error(TEXT("Matched module input is not the requested RenderTarget2D/Texture2D data interface type."));FNiagaraVariable User;bool Existed=false;if(!ResolveUser(W,UserName,RT?FNiagaraTypeDefinition::GetUTextureRenderTargetDef():FNiagaraTypeDefinition::GetUTextureDef(),User,Existed))return W.Error(W.R.Error);const auto BeforeUsers=CaptureUserValues(W);if(!ContinueRead())return W.Error(W.R.Error);UObject* ExpectedObject=Existed?System->GetExposedParameters().GetUObject(User).Get():nullptr;if(Asset)ExpectedObject=Asset;
 const FNiagaraParameterHandle Alias=BuildAliasedModuleParameterHandleForInput(Function,Input);auto* Existing=FindNiagaraStackFunctionInputOverridePin(Function,Alias);if(!ContinueRead())return W.Error(W.R.Error);const int32 PreviousLinks=Existing?Existing->LinkedTo.Num():0;if(PreviousLinks>0&&!Overwrite)return W.Error(TEXT("Module override already has links; overwrite_existing=True is required."));if(PreviousLinks>1)return W.Error(TEXT("Refusing to overwrite multiple resource override links."));if(Existing&&PreviousLinks==1){FString E;if(!W.PinForWrite(Existing)||!ValidateLinkedNiagaraDataInterfaceInputNode(*Existing,E))return W.Error(E.IsEmpty()?W.R.Error:E);}if(!W.WaitAdmission(Wait,Compile))return W.Error(W.R.Error);W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);if(!W.Modify(System)||!W.Modify(Function))return W.Error(W.R.Error);auto* Override=CreateOverrideForPath(W,Function,Alias,Type);if(!Override)return W.Error(W.R.Error);if(PreviousLinks==1){FString E;if(!ClearLinkedForPath(W,*Override,E))return W.Error(E.IsEmpty()?W.R.Error:E);}auto* DI=CreateDI(W,Override,DIClass,Alias.GetParameterHandleString().ToString(),RT);if(!DI)return W.Error(W.R.Error);if(!Existed&&!AddUserForPath(W,User))return W.Error(W.R.Error.IsEmpty()?TEXT("Failed to add Niagara user parameter."):W.R.Error);if(!VerifyUnrelatedUserValues(W,BeforeUsers,User,!Existed)||!W.Modify(DI))return W.Error(W.R.Error);
 if(!W.Step(TEXT("SetDI_PublicBindingFields"),true,[&]{auto* E=W.ExpectedDI.FindByPredicate([&](const auto& V){return V.DI.Get()==DI;});if(RT){auto* D=static_cast<UNiagaraDataInterfaceRenderTarget2D*>(DI);D->bInheritUserParameterSettings=Inherit;D->RenderTargetUserParameter.Parameter=User;E->Inherit=Inherit;E->User=User;}else{auto* D=static_cast<UNiagaraDataInterfaceTexture*>(DI);D->TextureUserParameter.Parameter=User;if(Asset)D->Texture=Asset;E->User=User;E->Texture=D->Texture;}W.R.bChanged=true;}))return W.Error(W.R.Error);if(Asset){if(!VerifyCanonicalUserForPath(W,User)||!W.Step(TEXT("UserStore_SetUObject"),true,[&]{System->GetExposedParameters().SetUObject(Asset,User);}))return W.Error(W.R.Error);}if(!VerifyResourceBinding(W,Function,Alias,User,BeforeUsers,!Existed,DI,ExpectedObject)||!W.Dirty())return W.Error(W.R.Error);if(Compile){bool Accepted=false;if(!W.Request(Accepted))return W.Error(W.R.Error);}if(Save&&!W.Save(false))return W.Error(W.R.Error);auto CompleteBinding=[=,&W](const TSharedPtr<FJsonObject>& WaitResult)->TSharedPtr<FJsonObject>
 {
 if(!ContinueRead()||!VerifyResourceBinding(W,Function,Alias,User,BeforeUsers,!Existed,DI,ExpectedObject))return W.Error(W.R.Error);
 auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("success"),true);Result->SetStringField(TEXT("system_path"),System->GetPathName());Result->SetStringField(TEXT("emitter_name"),ActualEmitterName);Result->SetNumberField(TEXT("emitter_index"),ActualEmitterIndex);Result->SetStringField(TEXT("module_name"),ActualModuleName);Result->SetStringField(TEXT("module_node_guid"),Function->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));Result->SetNumberField(TEXT("module_index"),ActualModuleIndex);Result->SetStringField(TEXT("input_name"),Input.GetName().ToString());Result->SetStringField(TEXT("input_type"),NiagaraTypeName(Type));Result->SetStringField(TEXT("aliased_input_handle"),Alias.GetParameterHandleString().ToString());Result->SetStringField(TEXT("user_parameter_name"),User.GetName().ToString());Result->SetBoolField(TEXT("user_parameter_created"),!Existed);Result->SetStringField(RT?TEXT("render_target_asset"):TEXT("texture_asset"),Asset?Asset->GetPathName():FString());Result->SetStringField(TEXT("data_interface_class"),DI->GetClass()->GetName());if(RT){auto* D=static_cast<UNiagaraDataInterfaceRenderTarget2D*>(DI);Result->SetBoolField(TEXT("inherit_user_parameter_settings"),Inherit);Result->SetStringField(TEXT("data_interface_user_parameter_name"),D->RenderTargetUserParameter.Parameter.GetName().ToString());Result->SetBoolField(TEXT("data_interface_inherit_user_parameter_settings"),D->bInheritUserParameterSettings);}else{auto* D=static_cast<UNiagaraDataInterfaceTexture*>(DI);Result->SetBoolField(TEXT("used_scratch_pad_metadata_fallback"),Fallback);Result->SetStringField(TEXT("data_interface_user_parameter_name"),D->TextureUserParameter.Parameter.GetName().ToString());Result->SetStringField(TEXT("data_interface_texture"),D->Texture?D->Texture->GetPathName():FString());}Result->SetNumberField(TEXT("previous_override_link_count"),PreviousLinks);Result->SetBoolField(TEXT("overwrote_existing"),PreviousLinks>0);Result->SetBoolField(TEXT("compile_requested"),Compile);Result->SetObjectField(TEXT("compile_wait"),WaitResult);Result->SetBoolField(TEXT("saved"),W.R.bSaved);Result->SetStringField(TEXT("write_scope"),RT?TEXT("render_target2d_data_interface_module_input"):TEXT("texture2d_data_interface_module_input"));W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;
 };
 if(W.DeferredMode&&Wait)
 {
  auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);Pending->SetBoolField(TEXT("saved"),W.R.bSaved);
  BeginResourceWait(W,System,Timeout,Poll,MoveTemp(CompleteBinding));return Pending;
 }
 return CompleteBinding(W.WaitSnapshot(Wait,Timeout,Poll));
}
TSharedPtr<FJsonObject> HandleRenderTarget2D(FWork& W,const TSharedPtr<FJsonObject>& P){return HandleResource(W,P,true);}
TSharedPtr<FJsonObject> HandleTexture2D(FWork& W,const TSharedPtr<FJsonObject>& P){return HandleResource(W,P,false);}

FString NiagaraObjectPathOrEmpty(const UObject* O){return O?O->GetPathName():FString();}
FString FormatNiagaraPinNumber(double Value)
{
    if(!FMath::IsFinite(Value)){Current->Fail(TEXT("niagara_resource_bindings_nonfinite_default"),TEXT("Numeric pin default conversion must remain finite."));return FString();}
    return FString::Printf(TEXT("%.6f"), Value);
}

bool TryGetJsonNumberFieldCaseInsensitive(const TSharedPtr<FJsonObject>& Object, const FString& LowerName, double& OutValue)
{
    if (!Object.IsValid())
    {
        return false;
    }

    if (Object->TryGetNumberField(LowerName, OutValue))
    {
        return true;
    }

    FString UpperName = LowerName.ToUpper();
    return Object->TryGetNumberField(UpperName, OutValue);
}

bool BuildNiagaraPinDefaultValueFromJson(const TSharedPtr<FJsonValue>& Value, FString& OutDefaultValue, FString& OutError)
{
    if (!Value.IsValid())
    {
        OutError = TEXT("Missing 'default_value' or 'value' parameter");
        return false;
    }

    if (Value->Type == EJson::String)
    {
        OutDefaultValue = Value->AsString();
        return true;
    }

    if (Value->Type == EJson::Number)
    {
        double NumberValue = 0.0;
        if (!Value->TryGetNumber(NumberValue))
        {
            OutError = TEXT("Expected numeric Niagara pin default value");
            return false;
        }
        OutDefaultValue = FormatNiagaraPinNumber(NumberValue);
        return true;
    }

    if (Value->Type == EJson::Boolean)
    {
        bool BoolValue = false;
        if (!Value->TryGetBool(BoolValue))
        {
            OutError = TEXT("Expected boolean Niagara pin default value");
            return false;
        }
        OutDefaultValue = BoolValue ? TEXT("true") : TEXT("false");
        return true;
    }

    if (Value->Type == EJson::Array)
    {
        TArray<double> Values;
        if (!JsonArrayToDoubles(Value, Values))
        {
            OutError = TEXT("Expected numeric array Niagara pin default value");
            return false;
        }

        if (Values.Num() == 2)
        {
            OutDefaultValue = FString::Printf(
                TEXT("X=%s Y=%s"),
                *FormatNiagaraPinNumber(Values[0]),
                *FormatNiagaraPinNumber(Values[1]));
            return true;
        }
        if (Values.Num() == 3)
        {
            OutDefaultValue = FString::Printf(
                TEXT("(X=%s,Y=%s,Z=%s)"),
                *FormatNiagaraPinNumber(Values[0]),
                *FormatNiagaraPinNumber(Values[1]),
                *FormatNiagaraPinNumber(Values[2]));
            return true;
        }
        if (Values.Num() == 4)
        {
            OutDefaultValue = FString::Printf(
                TEXT("(R=%s,G=%s,B=%s,A=%s)"),
                *FormatNiagaraPinNumber(Values[0]),
                *FormatNiagaraPinNumber(Values[1]),
                *FormatNiagaraPinNumber(Values[2]),
                *FormatNiagaraPinNumber(Values[3]));
            return true;
        }

        OutError = FString::Printf(TEXT("Unsupported Niagara pin default array length: %d"), Values.Num());
        return false;
    }

    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        double R = 0.0;
        double G = 0.0;
        double B = 0.0;
        double A = 1.0;
        if (TryGetJsonNumberFieldCaseInsensitive(Object, TEXT("r"), R) &&
            TryGetJsonNumberFieldCaseInsensitive(Object, TEXT("g"), G) &&
            TryGetJsonNumberFieldCaseInsensitive(Object, TEXT("b"), B))
        {
            TryGetJsonNumberFieldCaseInsensitive(Object, TEXT("a"), A);
            OutDefaultValue = FString::Printf(
                TEXT("(R=%s,G=%s,B=%s,A=%s)"),
                *FormatNiagaraPinNumber(R),
                *FormatNiagaraPinNumber(G),
                *FormatNiagaraPinNumber(B),
                *FormatNiagaraPinNumber(A));
            return true;
        }

        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        double W = 0.0;
        if (TryGetJsonNumberFieldCaseInsensitive(Object, TEXT("x"), X) &&
            TryGetJsonNumberFieldCaseInsensitive(Object, TEXT("y"), Y))
        {
            if (TryGetJsonNumberFieldCaseInsensitive(Object, TEXT("z"), Z))
            {
                if (TryGetJsonNumberFieldCaseInsensitive(Object, TEXT("w"), W))
                {
                    OutDefaultValue = FString::Printf(
                        TEXT("(X=%s,Y=%s,Z=%s,W=%s)"),
                        *FormatNiagaraPinNumber(X),
                        *FormatNiagaraPinNumber(Y),
                        *FormatNiagaraPinNumber(Z),
                        *FormatNiagaraPinNumber(W));
                }
                else
                {
                    OutDefaultValue = FString::Printf(
                        TEXT("(X=%s,Y=%s,Z=%s)"),
                        *FormatNiagaraPinNumber(X),
                        *FormatNiagaraPinNumber(Y),
                        *FormatNiagaraPinNumber(Z));
                }
            }
            else
            {
                OutDefaultValue = FString::Printf(
                    TEXT("X=%s Y=%s"),
                    *FormatNiagaraPinNumber(X),
                    *FormatNiagaraPinNumber(Y));
            }
            return true;
        }

        OutError = TEXT("Expected object Niagara pin default value with r/g/b[/a] or x/y[/z[/w]] fields");
        return false;
    }

    OutError = TEXT("Unsupported JSON type for Niagara pin default value");
    return false;
}

TSharedPtr<FJsonObject> LinkedPinToJsonObject(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (!Pin)
    {
        return PinObject;
    }

    const UEdGraphNode* OwningNode = Pin->GetOwningNode();
    PinObject->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
    PinObject->SetStringField(TEXT("node_name"), OwningNode ? OwningNode->GetName() : FString());
    PinObject->SetStringField(TEXT("node_class"), OwningNode ? OwningNode->GetClass()->GetName() : FString());
    PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);
    PinObject->SetStringField(TEXT("default_object"), NiagaraObjectPathOrEmpty(Pin->DefaultObject));
    return PinObject;
}

TArray<TSharedPtr<FJsonValue>> LinkedPinsToJson(const UEdGraphPin* Pin, bool bIncludeLinkedSources)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!Pin || !bIncludeLinkedSources)
    {
        return Values;
    }

    for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
    {
        Values.Add(MakeShared<FJsonValueObject>(LinkedPinToJsonObject(LinkedPin)));
    }
    return Values;
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


TSharedPtr<FJsonObject> HandleScratchDefault(FWork& W,const TSharedPtr<FJsonObject>& Params)
{

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
            FString::Printf(TEXT("Refusing to edit Niagara Scratch Pad graph outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    FString FunctionName;
    FString FunctionNodeGuid;
    FString InputPinName;
    int32 FunctionNodeIndex = INDEX_NONE;
    int32 FunctionCallIndexSelector = INDEX_NONE;
    Params->TryGetStringField(TEXT("function_name"), FunctionName);
    Params->TryGetStringField(TEXT("function_node_guid"), FunctionNodeGuid);
    FunctionNodeIndex = IndexParam(Params,TEXT("function_node_index"),FunctionNodeIndex);
    FunctionNodeIndex = IndexParam(Params,TEXT("node_index"),FunctionNodeIndex);
    FunctionCallIndexSelector = IndexParam(Params,TEXT("function_call_index"),FunctionCallIndexSelector);
    if (FunctionName.IsEmpty() && FunctionNodeGuid.IsEmpty() && FunctionNodeIndex == INDEX_NONE && FunctionCallIndexSelector == INDEX_NONE)
    {
        return W.Error(TEXT("Missing function selector. Provide 'function_name', 'function_node_guid', 'function_node_index', or 'function_call_index'."));
    }
    if (!Params->TryGetStringField(TEXT("input_pin_name"), InputPinName))
    {
        return W.Error(TEXT("Missing 'input_pin_name' parameter"));
    }

    FString NewDefaultValue;
    if (!Params->TryGetStringField(TEXT("default_value"), NewDefaultValue))
    {
        FString DefaultValueError;
        const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
        if (!BuildNiagaraPinDefaultValueFromJson(Value, NewDefaultValue, DefaultValueError))
        {
            return W.Error(DefaultValueError);
        }
    }

    FString ScratchPadScriptPath;
    FString ScratchPadOwnerKind = TEXT("system");
    FString ScratchPadName;
    int32 ScratchPadScriptIndex = INDEX_NONE;
    int32 ScratchPadEmitterIndex = INDEX_NONE;
    FString ScratchPadEmitterName;
    Params->TryGetStringField(TEXT("scratch_pad_script_path"), ScratchPadScriptPath);
    Params->TryGetStringField(TEXT("scratch_pad_owner_kind"), ScratchPadOwnerKind);
    Params->TryGetStringField(TEXT("scratch_pad_name"), ScratchPadName);
    Params->TryGetStringField(TEXT("scratch_pad_script_name"), ScratchPadName);
    ScratchPadScriptIndex = IndexParam(Params,TEXT("scratch_pad_script_index"),ScratchPadScriptIndex);
    ScratchPadEmitterIndex = IndexParam(Params,TEXT("scratch_pad_emitter_index"),ScratchPadEmitterIndex);
    Params->TryGetStringField(TEXT("scratch_pad_emitter_name"), ScratchPadEmitterName);
    if (ScratchPadScriptPath.IsEmpty() && ScratchPadName.IsEmpty() && ScratchPadScriptIndex == INDEX_NONE)
    {
        return W.Error(TEXT("Missing Scratch Pad selector. Provide 'scratch_pad_script_path', 'scratch_pad_name', or 'scratch_pad_script_index'."));
    }

    bool bBreakLinks = true;
    Params->TryGetBoolField(TEXT("break_links"), bBreakLinks);

    bool bAllowMultiLinkBreak = false;
    Params->TryGetBoolField(TEXT("allow_multi_link_break"), bAllowMultiLinkBreak);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    if(!ContinueRead())return W.Error(W.R.Error);
    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraScript* RequestedScript = nullptr;
    if (!ScratchPadScriptPath.IsEmpty())
    {
        RequestedScript = LoadResource<UNiagaraScript>(W,ScratchPadScriptPath);
        if (!RequestedScript)
        {
            return W.Error(FString::Printf(TEXT("Failed to load Scratch Pad script: %s"), *ScratchPadScriptPath));
        }
    }

    UNiagaraScript* ScratchPadScript = nullptr;
    FString ResolvedScratchPadOwnerKind;
    FString ResolvedScratchPadOwnerName;
    int32 ResolvedScratchPadEmitterIndex = INDEX_NONE;
    int32 ResolvedScratchPadScriptIndex = INDEX_NONE;

    auto TryMatchScratchPadScript = [&RequestedScript, &ScratchPadName, ScratchPadScriptIndex](
        const TArray<TObjectPtr<UNiagaraScript>>& Scripts,
        int32& OutScriptIndex) -> UNiagaraScript*
    {
        for (int32 ScriptIndex = 0; ScriptIndex < Scripts.Num(); ++ScriptIndex)
        {
            UNiagaraScript* Candidate = Scripts[ScriptIndex].Get();
            if (!Candidate)
            {
                continue;
            }

            const bool bPathMatches = !RequestedScript || Candidate == RequestedScript || Candidate->GetPathName().Equals(RequestedScript->GetPathName(), ESearchCase::IgnoreCase);
            const bool bIndexMatches = RequestedScript || ScratchPadScriptIndex == INDEX_NONE || ScratchPadScriptIndex == ScriptIndex;
            const bool bNameMatches = RequestedScript || ScratchPadName.IsEmpty() || Candidate->GetName().Equals(ScratchPadName, ESearchCase::IgnoreCase);
            if (bPathMatches && bIndexMatches && bNameMatches)
            {
                OutScriptIndex = ScriptIndex;
                return Candidate;
            }
        }
        return nullptr;
    };

    auto TryResolveSystemScratchPad = [&]()
    {
        int32 MatchedScriptIndex = INDEX_NONE;
        if (UNiagaraScript* Candidate = TryMatchScratchPadScript(System->ScratchPadScripts, MatchedScriptIndex))
        {
            ScratchPadScript = Candidate;
            ResolvedScratchPadOwnerKind = TEXT("system");
            ResolvedScratchPadOwnerName = System->GetName();
            ResolvedScratchPadEmitterIndex = INDEX_NONE;
            ResolvedScratchPadScriptIndex = MatchedScriptIndex;
            return true;
        }
        return false;
    };

    auto TryResolveEmitterScratchPad = [&]()
    {
        int32 CurrentEmitterIndex = 0;
        for (FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
        {
            const FString EmitterName = EmitterHandle.GetName().ToString();
            const bool bEmitterMatches =
                RequestedScript ||
                ((ScratchPadEmitterIndex == INDEX_NONE || ScratchPadEmitterIndex == CurrentEmitterIndex) &&
                (ScratchPadEmitterName.IsEmpty() || ScratchPadEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase)));
            if (bEmitterMatches)
            {
                if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
                {
                    if (EmitterData->ScratchPads)
                    {
                        int32 MatchedScriptIndex = INDEX_NONE;
                        if (UNiagaraScript* Candidate = TryMatchScratchPadScript(EmitterData->ScratchPads->Scripts, MatchedScriptIndex))
                        {
                            ScratchPadScript = Candidate;
                            ResolvedScratchPadOwnerKind = TEXT("emitter");
                            ResolvedScratchPadOwnerName = EmitterName;
                            ResolvedScratchPadEmitterIndex = CurrentEmitterIndex;
                            ResolvedScratchPadScriptIndex = MatchedScriptIndex;
                            return true;
                        }
                    }
                }
            }
            ++CurrentEmitterIndex;
        }
        return false;
    };

    if (RequestedScript)
    {
        TryResolveSystemScratchPad();
        if (!ScratchPadScript)
        {
            TryResolveEmitterScratchPad();
        }
    }
    else if (ScratchPadOwnerKind.Equals(TEXT("system"), ESearchCase::IgnoreCase))
    {
        TryResolveSystemScratchPad();
    }
    else if (ScratchPadOwnerKind.Equals(TEXT("emitter"), ESearchCase::IgnoreCase))
    {
        TryResolveEmitterScratchPad();
    }
    else
    {
        return W.Error(TEXT("Unsupported scratch_pad_owner_kind. Use 'system' or 'emitter'."));
    }

    if (!ScratchPadScript)
    {
        return W.Error(TEXT("No matching target-local Scratch Pad script was found"));
    }

    UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(ScratchPadScript->GetLatestSource());
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if (!Graph)
    {
        return W.Error(FString::Printf(TEXT("Scratch Pad script has no editable graph: %s"), *ScratchPadScript->GetPathName()));
    }

    if(!W.CaptureGraph(ScriptSource)||!W.Own(ScratchPadScript)||!W.GraphForWrite(Graph))return W.Error(W.R.Error);
    UNiagaraNodeFunctionCall* MatchedFunctionCall = nullptr;
    int32 MatchedFunctionNodeIndex = INDEX_NONE;
    int32 MatchedFunctionCallIndex = INDEX_NONE;
    int32 GraphNodeIndex = 0;
    int32 FunctionCallIndex = 0;
    int32 MatchCount = 0;
    for (UEdGraphNode* Node : TArray<TObjectPtr<UEdGraphNode>>(Graph->Nodes))
    {
        const int32 CurrentGraphNodeIndex = GraphNodeIndex++;
        UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node);
        if (!FunctionCall)
        {
            continue;
        }
        const int32 CurrentFunctionCallIndex = FunctionCallIndex++;

        FString CandidateFunctionName = FunctionNameForPath(FunctionCall);
        if(!ContinueRead())return W.Error(W.R.Error);
        if (CandidateFunctionName.IsEmpty())
        {
            CandidateFunctionName = FunctionCall->Signature.GetNameString();
        }
        const FString CandidateGuid = FunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
        const bool bIndexMatches = FunctionNodeIndex == INDEX_NONE || FunctionNodeIndex == CurrentGraphNodeIndex;
        const bool bFunctionCallIndexMatches = FunctionCallIndexSelector == INDEX_NONE || FunctionCallIndexSelector == CurrentFunctionCallIndex;
        const bool bNameMatches = FunctionName.IsEmpty() || CandidateFunctionName.Equals(FunctionName, ESearchCase::IgnoreCase);
        const bool bGuidMatches = FunctionNodeGuid.IsEmpty() || CandidateGuid.Equals(FunctionNodeGuid, ESearchCase::IgnoreCase);

        if (bIndexMatches && bFunctionCallIndexMatches && bNameMatches && bGuidMatches)
        {
            ++MatchCount;
            if (!MatchedFunctionCall)
            {
                MatchedFunctionCall = FunctionCall;
                MatchedFunctionNodeIndex = CurrentGraphNodeIndex;
                MatchedFunctionCallIndex = CurrentFunctionCallIndex;
            }
        }
    }

    if (!MatchedFunctionCall)
    {
        return W.Error(TEXT("No matching Scratch Pad function call was found"));
    }
    if (MatchCount > 1 && FunctionNodeIndex == INDEX_NONE && FunctionNodeGuid.IsEmpty() && FunctionCallIndexSelector == INDEX_NONE)
    {
        return W.Error(
            FString::Printf(TEXT("Function selector matched %d nodes. Provide 'function_node_index' or 'function_node_guid'."), MatchCount));
    }

    UEdGraphPin* TargetPin = nullptr;
    for (UEdGraphPin* Pin : MatchedFunctionCall->Pins)
    {
        if (!Pin || Pin->Direction != EGPD_Input)
        {
            continue;
        }

        const FString CandidatePinName = Pin->PinName.ToString();
        const bool bPinMatches = CandidatePinName.Equals(InputPinName, ESearchCase::IgnoreCase) ||
            InputPinName.EndsWith(FString::Printf(TEXT(".%s"), *CandidatePinName), ESearchCase::IgnoreCase);
        if (bPinMatches)
        {
            TargetPin = Pin;
            break;
        }
    }

    if (!TargetPin)
    {
        return W.Error(FString::Printf(TEXT("No matching input pin was found: %s"), *InputPinName));
    }
    if (bBreakLinks && TargetPin->LinkedTo.Num() > 1 && !bAllowMultiLinkBreak)
    {
        return W.Error(
            FString::Printf(
                TEXT("Input pin '%s' has %d links. Pass allow_multi_link_break=true to break all of them."),
                *TargetPin->PinName.ToString(),
                TargetPin->LinkedTo.Num()));
    }

    const UEdGraphSchema* RequestedSchema=TargetPin->GetSchema();
    if(RequestedSchema&&RequestedSchema->GetClass()!=UEdGraphSchema_Niagara::StaticClass())return W.Error(TEXT("Unsupported custom schema default setter."));
    if(TargetPin->bOrphanedPin&&NewDefaultValue.Equals(TargetPin->AutogeneratedDefaultValue,ESearchCase::CaseSensitive))return W.Error(TEXT("Orphaned pin auto-retirement on this exact default is unsupported before writer."));
    const FString PreviousDefaultValue = TargetPin->DefaultValue;
    const FString PreviousDefaultObject = NiagaraObjectPathOrEmpty(TargetPin->DefaultObject);
    const int32 PreviousLinkCount = TargetPin->LinkedTo.Num();
    const TArray<TSharedPtr<FJsonValue>> PreviousLinkedSources = LinkedPinsToJson(TargetPin, true);

    W.R.bSaveRequested=bSave;W.R.bCompileRequested=bRequestCompile;
    W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);
    if(!W.Modify(System)||!W.Modify(ScratchPadScript)||!W.Modify(ScriptSource)||!W.Modify(Graph)||!W.Modify(MatchedFunctionCall)||!SetPinDefault(W,TargetPin,NewDefaultValue))return W.Error(W.R.Error);

    int32 BrokenLinkCount = 0;
    if (bBreakLinks && PreviousLinkCount > 0)
    {
        BrokenLinkCount = PreviousLinkCount;
        if(!W.BreakLinks(TargetPin,true))return W.Error(W.R.Error);
    }

    if(!W.Step(TEXT("NotifyGraphChanged"),true,[&]{Graph->NotifyGraphChanged();})||!W.Dirty())return W.Error(W.R.Error);
    if (bRequestCompile)
    {
        bool Accepted=false;if(!W.Request(Accepted))return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(false);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after Scratch Pad function input default edit"));
        }
    }

    if(!W.Admit(TEXT("scratch_default_final_readback"))||!FindExpectedPin(W,TargetPin)||!TargetPin->DefaultValue.Equals(NewDefaultValue,ESearchCase::CaseSensitive)||TargetPin->DefaultObject||!TargetPin->DefaultTextValue.IsEmpty()||(bBreakLinks&&TargetPin->LinkedTo.Num()!=0))return W.Error(TEXT("Actual exact pin default/link readback failed after compile/save."));
    FString ResolvedFunctionName = FunctionNameForPath(MatchedFunctionCall);
    if(!ContinueRead())return W.Error(W.R.Error);
    if (ResolvedFunctionName.IsEmpty())
    {
        ResolvedFunctionName = MatchedFunctionCall->Signature.GetNameString();
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_script_path"), ScratchPadScript->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_script_name"), ScratchPadScript->GetName());
    Result->SetStringField(TEXT("scratch_pad_owner_kind"), ResolvedScratchPadOwnerKind);
    Result->SetStringField(TEXT("scratch_pad_owner_name"), ResolvedScratchPadOwnerName);
    Result->SetNumberField(TEXT("scratch_pad_emitter_index"), ResolvedScratchPadEmitterIndex);
    Result->SetNumberField(TEXT("scratch_pad_script_index"), ResolvedScratchPadScriptIndex);
    Result->SetStringField(TEXT("function_name"), ResolvedFunctionName);
    Result->SetNumberField(TEXT("function_node_index"), MatchedFunctionNodeIndex);
    Result->SetNumberField(TEXT("function_call_index"), MatchedFunctionCallIndex);
    Result->SetStringField(TEXT("function_node_guid"), MatchedFunctionCall->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("input_pin_name"), TargetPin->PinName.ToString());
    Result->SetStringField(TEXT("previous_default_value"), PreviousDefaultValue);
    Result->SetStringField(TEXT("previous_default_object"), PreviousDefaultObject);
    Result->SetStringField(TEXT("new_default_value"), TargetPin->DefaultValue);
    Result->SetNumberField(TEXT("previous_link_count"), PreviousLinkCount);
    Result->SetNumberField(TEXT("broken_link_count"), BrokenLinkCount);
    Result->SetNumberField(TEXT("new_link_count"), TargetPin->LinkedTo.Num());
    Result->SetArrayField(TEXT("previous_linked_sources"), PreviousLinkedSources);
    Result->SetObjectField(TEXT("pin"), GraphPinToJsonObject(TargetPin));
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("scratch_pad_internal_function_input_default"));
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);
    return Result;

}

TSharedPtr<FJsonObject> HandleScratchUser(FWork& W,const TSharedPtr<FJsonObject>& Params)
{

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
            FString::Printf(TEXT("Refusing to edit Niagara Scratch Pad graph outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    FString TargetPinName;
    if (!Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName))
    {
        Params->TryGetStringField(TEXT("input_pin_name"), TargetPinName);
    }
    TargetPinName.TrimStartAndEndInline();
    if (TargetPinName.IsEmpty())
    {
        return W.Error(TEXT("Missing 'target_pin_name' parameter"));
    }

    FString UserParameterName;
    if (!Params->TryGetStringField(TEXT("user_parameter_name"), UserParameterName))
    {
        return W.Error(TEXT("Missing 'user_parameter_name' parameter"));
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

    bool bOverwriteExisting = false;
    Params->TryGetBoolField(TEXT("overwrite_existing"), bOverwriteExisting);

    bool bAllowMultiLinkBreak = false;
    Params->TryGetBoolField(TEXT("allow_multi_link_break"), bAllowMultiLinkBreak);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    FString ScratchPadScriptPath;
    FString ScratchPadOwnerKind = TEXT("system");
    FString ScratchPadName;
    int32 ScratchPadScriptIndex = INDEX_NONE;
    int32 ScratchPadEmitterIndex = INDEX_NONE;
    FString ScratchPadEmitterName;
    Params->TryGetStringField(TEXT("scratch_pad_script_path"), ScratchPadScriptPath);
    Params->TryGetStringField(TEXT("scratch_pad_owner_kind"), ScratchPadOwnerKind);
    Params->TryGetStringField(TEXT("scratch_pad_name"), ScratchPadName);
    Params->TryGetStringField(TEXT("scratch_pad_script_name"), ScratchPadName);
    ScratchPadScriptIndex = IndexParam(Params,TEXT("scratch_pad_script_index"),ScratchPadScriptIndex);
    ScratchPadEmitterIndex = IndexParam(Params,TEXT("scratch_pad_emitter_index"),ScratchPadEmitterIndex);
    Params->TryGetStringField(TEXT("scratch_pad_emitter_name"), ScratchPadEmitterName);
    if (ScratchPadScriptPath.IsEmpty() && ScratchPadName.IsEmpty() && ScratchPadScriptIndex == INDEX_NONE)
    {
        return W.Error(TEXT("Missing Scratch Pad selector. Provide 'scratch_pad_script_path', 'scratch_pad_name', or 'scratch_pad_script_index'."));
    }

    FString TargetNodeGuid;
    Params->TryGetStringField(TEXT("target_node_guid"), TargetNodeGuid);
    Params->TryGetStringField(TEXT("parameter_map_set_node_guid"), TargetNodeGuid);

    int32 TargetNodeIndex = INDEX_NONE;
    TargetNodeIndex = IndexParam(Params,TEXT("target_node_index"),TargetNodeIndex);
    TargetNodeIndex = IndexParam(Params,TEXT("parameter_map_set_node_index"),TargetNodeIndex);

    int32 ParameterMapSetIndexSelector = INDEX_NONE;
    ParameterMapSetIndexSelector = IndexParam(Params,TEXT("parameter_map_set_index"),ParameterMapSetIndexSelector);

    const TSharedPtr<FJsonValue> DefaultValue = Params->TryGetField(TEXT("default_value"));
    const bool bHasDefaultValue = DefaultValue.IsValid();

    if(!ContinueRead())return W.Error(W.R.Error);
    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraScript* RequestedScript = nullptr;
    if (!ScratchPadScriptPath.IsEmpty())
    {
        RequestedScript = LoadResource<UNiagaraScript>(W,ScratchPadScriptPath);
        if (!RequestedScript)
        {
            return W.Error(FString::Printf(TEXT("Failed to load Scratch Pad script: %s"), *ScratchPadScriptPath));
        }
    }

    UNiagaraScript* ScratchPadScript = nullptr;
    FString ResolvedScratchPadOwnerKind;
    FString ResolvedScratchPadOwnerName;
    int32 ResolvedScratchPadEmitterIndex = INDEX_NONE;
    int32 ResolvedScratchPadScriptIndex = INDEX_NONE;

    auto TryMatchScratchPadScript = [&RequestedScript, &ScratchPadName, ScratchPadScriptIndex](
        const TArray<TObjectPtr<UNiagaraScript>>& Scripts,
        int32& OutScriptIndex) -> UNiagaraScript*
    {
        for (int32 ScriptIndex = 0; ScriptIndex < Scripts.Num(); ++ScriptIndex)
        {
            UNiagaraScript* Candidate = Scripts[ScriptIndex].Get();
            if (!Candidate)
            {
                continue;
            }

            const bool bPathMatches = !RequestedScript || Candidate == RequestedScript || Candidate->GetPathName().Equals(RequestedScript->GetPathName(), ESearchCase::IgnoreCase);
            const bool bIndexMatches = RequestedScript || ScratchPadScriptIndex == INDEX_NONE || ScratchPadScriptIndex == ScriptIndex;
            const bool bNameMatches = RequestedScript || ScratchPadName.IsEmpty() || Candidate->GetName().Equals(ScratchPadName, ESearchCase::IgnoreCase);
            if (bPathMatches && bIndexMatches && bNameMatches)
            {
                OutScriptIndex = ScriptIndex;
                return Candidate;
            }
        }
        return nullptr;
    };

    auto TryResolveSystemScratchPad = [&]()
    {
        int32 MatchedScriptIndex = INDEX_NONE;
        if (UNiagaraScript* Candidate = TryMatchScratchPadScript(System->ScratchPadScripts, MatchedScriptIndex))
        {
            ScratchPadScript = Candidate;
            ResolvedScratchPadOwnerKind = TEXT("system");
            ResolvedScratchPadOwnerName = System->GetName();
            ResolvedScratchPadEmitterIndex = INDEX_NONE;
            ResolvedScratchPadScriptIndex = MatchedScriptIndex;
            return true;
        }
        return false;
    };

    auto TryResolveEmitterScratchPad = [&]()
    {
        int32 CurrentEmitterIndex = 0;
        for (FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
        {
            const FString EmitterName = EmitterHandle.GetName().ToString();
            const bool bEmitterMatches =
                RequestedScript ||
                ((ScratchPadEmitterIndex == INDEX_NONE || ScratchPadEmitterIndex == CurrentEmitterIndex) &&
                (ScratchPadEmitterName.IsEmpty() || ScratchPadEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase)));
            if (bEmitterMatches)
            {
                if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
                {
                    if (EmitterData->ScratchPads)
                    {
                        int32 MatchedScriptIndex = INDEX_NONE;
                        if (UNiagaraScript* Candidate = TryMatchScratchPadScript(EmitterData->ScratchPads->Scripts, MatchedScriptIndex))
                        {
                            ScratchPadScript = Candidate;
                            ResolvedScratchPadOwnerKind = TEXT("emitter");
                            ResolvedScratchPadOwnerName = EmitterName;
                            ResolvedScratchPadEmitterIndex = CurrentEmitterIndex;
                            ResolvedScratchPadScriptIndex = MatchedScriptIndex;
                            return true;
                        }
                    }
                }
            }
            ++CurrentEmitterIndex;
        }
        return false;
    };

    if (RequestedScript)
    {
        TryResolveSystemScratchPad();
        if (!ScratchPadScript)
        {
            TryResolveEmitterScratchPad();
        }
    }
    else if (ScratchPadOwnerKind.Equals(TEXT("system"), ESearchCase::IgnoreCase))
    {
        TryResolveSystemScratchPad();
    }
    else if (ScratchPadOwnerKind.Equals(TEXT("emitter"), ESearchCase::IgnoreCase))
    {
        TryResolveEmitterScratchPad();
    }
    else
    {
        return W.Error(TEXT("Unsupported scratch_pad_owner_kind. Use 'system' or 'emitter'."));
    }

    if (!ScratchPadScript)
    {
        return W.Error(TEXT("No matching target-local Scratch Pad script was found"));
    }

    UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(ScratchPadScript->GetLatestSource());
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if (!Graph)
    {
        return W.Error(FString::Printf(TEXT("Scratch Pad script has no editable graph: %s"), *ScratchPadScript->GetPathName()));
    }

    if(!W.CaptureGraph(ScriptSource)||!W.Own(ScratchPadScript)||!W.GraphForWrite(Graph))return W.Error(W.R.Error);
    UEdGraphNode* MatchedSetNode = nullptr;
    UEdGraphPin* TargetPin = nullptr;
    int32 MatchedGraphNodeIndex = INDEX_NONE;
    int32 MatchedParameterMapSetIndex = INDEX_NONE;
    int32 MatchCount = 0;
    int32 GraphNodeIndex = 0;
    int32 ParameterMapSetIndex = 0;
    for (UEdGraphNode* Node : TArray<TObjectPtr<UEdGraphNode>>(Graph->Nodes))
    {
        const int32 CurrentGraphNodeIndex = GraphNodeIndex++;
        if (!Node || !Node->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive))
        {
            continue;
        }

        const int32 CurrentParameterMapSetIndex = ParameterMapSetIndex++;
        const FString CandidateGuid = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
        const bool bNodeIndexMatches = TargetNodeIndex == INDEX_NONE || TargetNodeIndex == CurrentGraphNodeIndex;
        const bool bSetIndexMatches = ParameterMapSetIndexSelector == INDEX_NONE || ParameterMapSetIndexSelector == CurrentParameterMapSetIndex;
        const bool bGuidMatches = TargetNodeGuid.IsEmpty() || TargetNodeGuid.Equals(CandidateGuid, ESearchCase::IgnoreCase);
        if (!bNodeIndexMatches || !bSetIndexMatches || !bGuidMatches)
        {
            continue;
        }

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Input)
            {
                continue;
            }

            const FString CandidatePinName = Pin->PinName.ToString();
            const bool bPinMatches = CandidatePinName.Equals(TargetPinName, ESearchCase::IgnoreCase) ||
                TargetPinName.EndsWith(FString::Printf(TEXT(".%s"), *CandidatePinName), ESearchCase::IgnoreCase) ||
                CandidatePinName.EndsWith(FString::Printf(TEXT(".%s"), *TargetPinName), ESearchCase::IgnoreCase);
            if (bPinMatches)
            {
                ++MatchCount;
                if (!MatchedSetNode)
                {
                    MatchedSetNode = Node;
                    TargetPin = Pin;
                    MatchedGraphNodeIndex = CurrentGraphNodeIndex;
                    MatchedParameterMapSetIndex = CurrentParameterMapSetIndex;
                }
            }
        }
    }

    if (!MatchedSetNode || !TargetPin)
    {
        return W.Error(FString::Printf(TEXT("No matching Scratch Pad ParameterMapSet input pin was found: %s"), *TargetPinName));
    }
    if (MatchCount > 1 && TargetNodeIndex == INDEX_NONE && TargetNodeGuid.IsEmpty() && ParameterMapSetIndexSelector == INDEX_NONE)
    {
        return W.Error(
            FString::Printf(TEXT("Target pin selector matched %d pins. Provide 'target_node_index', 'parameter_map_set_index', or 'target_node_guid'."), MatchCount));
    }
    if (TargetPin->PinName == TEXT("Source"))
    {
        return W.Error(TEXT("Refusing to link the ParameterMapSet Source pin; choose a value input pin instead."));
    }

    UEdGraphPin* SourcePin = nullptr;
    for (UEdGraphPin* Pin : MatchedSetNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == TEXT("Source"))
        {
            SourcePin = Pin;
            break;
        }
    }
    if (!SourcePin || SourcePin->LinkedTo.Num() != 1)
    {
        return W.Error(
            FString::Printf(
                TEXT("Target ParameterMapSet node '%s' must have exactly one linked Source pin before linking a User parameter. Found: %d"),
                *MatchedSetNode->GetName(),
                SourcePin ? SourcePin->LinkedTo.Num() : 0));
    }

    const FNiagaraTypeDefinition InputType = PinTypeForPath(TargetPin);
    if(!ContinueRead())return W.Error(W.R.Error);
    if (!InputType.IsValid() || InputType == FNiagaraTypeDefinition::GetParameterMapDef() || InputType.IsDataInterface() || InputType.IsUObject())
    {
        return W.Error(
            FString::Printf(TEXT("Unsupported Scratch Pad input pin type for User parameter link: %s (%s)"),
                *TargetPin->PinName.ToString(),
                *NiagaraTypeName(InputType)));
    }
    FNiagaraVariable UserParameter(InputType, FName(*UserParameterName));

    if(!PreflightMapValue(W,TargetPin))return W.Error(W.R.Error);
    if (bHasDefaultValue)
    {
        if(!SafeDefaultNumbers(InputType,DefaultValue))return W.Error(W.R.Error);
        FString DefaultValidationError;
        if (!SetNiagaraVariableDataFromJson(UserParameter, DefaultValue, DefaultValidationError))
        {
            return W.Error(DefaultValidationError);
        }
    }

    const FNiagaraVariable ParsedUser=UserParameter;
    FNiagaraUserRedirectionParameterStore& UserParameterStore=System->GetExposedParameters();bool bUserParameterAlreadyExisted=false;
    if(!ResolveUser(W,UserParameterName,InputType,UserParameter,bUserParameterAlreadyExisted))return W.Error(W.R.Error);
    if(!bUserParameterAlreadyExisted&&ParsedUser.IsDataAllocated())UserParameter.SetData(ParsedUser.GetData());
    const auto BeforeUsers=CaptureUserValues(W);if(!ContinueRead())return W.Error(W.R.Error);
    TSharedPtr<FJsonValue> ExpectedDefault;if(bHasDefaultValue){ExpectedDefault=ExpectedScalarForPath(UserParameter,DefaultValue);if(!ExpectedDefault)return W.Error(TEXT("Requested User default has no supported actual typed setter; no writer dispatched."));}

    const int32 PreviousLinkCount = TargetPin->LinkedTo.Num();
    if (PreviousLinkCount > 0 && !bOverwriteExisting)
    {
        return W.Error(
            FString::Printf(TEXT("Scratch Pad input pin '%s' already has %d link(s). Use overwrite_existing=true to replace it."),
                *TargetPin->PinName.ToString(),
                PreviousLinkCount));
    }
    if (PreviousLinkCount > 1 && !bAllowMultiLinkBreak)
    {
        return W.Error(
            FString::Printf(TEXT("Scratch Pad input pin '%s' has %d links. Pass allow_multi_link_break=true to break all of them."),
                *TargetPin->PinName.ToString(),
                PreviousLinkCount));
    }

    const FString PreviousDefaultValue = TargetPin->DefaultValue;
    const FString PreviousDefaultObject = NiagaraObjectPathOrEmpty(TargetPin->DefaultObject);
    const TArray<TSharedPtr<FJsonValue>> PreviousLinkedSources = LinkedPinsToJson(TargetPin, true);

    W.R.bSaveRequested=bSave;W.R.bCompileRequested=bRequestCompile;
    W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);
    if(!W.Modify(System)||!W.Modify(ScratchPadScript)||!W.Modify(ScriptSource)||!W.Modify(Graph)||!W.Modify(MatchedSetNode))return W.Error(W.R.Error);
    if(!bUserParameterAlreadyExisted&&!AddUserForPath(W,UserParameter))return W.Error(W.R.Error.IsEmpty()?TEXT("Failed to add Niagara user parameter."):W.R.Error);
    if(!VerifyUnrelatedUserValues(W,BeforeUsers,UserParameter,!bUserParameterAlreadyExisted))return W.Error(W.R.Error);
    if(bHasDefaultValue){FString E;if(!SetDefaultForPath(W,UserParameterStore,UserParameter,DefaultValue,E))return W.Error(E.IsEmpty()?W.R.Error:E);if(!VerifyUnrelatedUserValues(W,BeforeUsers,UserParameter,!bUserParameterAlreadyExisted)||!SameProjectedValue(ExpectedDefault,NiagaraParameterValueToJson(UserParameterStore,UserParameter)))return W.Error(TEXT("Actual selected default did not match requested typed User value."));}
    int32 BrokenLinkCount=0;if(PreviousLinkCount>0){BrokenLinkCount=PreviousLinkCount;if(!W.BreakLinks(TargetPin,true))return W.Error(W.R.Error);}
    const FNiagaraVariable LinkUserParameter=UserParameter;
    if(!LinkMapUser(W,TargetPin,LinkUserParameter))return W.Error(W.R.Error);

    if(!W.Step(TEXT("NotifyGraphChanged"),true,[&]{Graph->NotifyGraphChanged();})||!W.Dirty())return W.Error(W.R.Error);
    if (bRequestCompile)
    {
        bool Accepted=false;if(!W.Request(Accepted))return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(false);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after Scratch Pad user parameter link"));
        }
    }

    if(!W.Admit(TEXT("scratch_user_final_typed_readback"))||!VerifyUnrelatedUserValues(W,BeforeUsers,UserParameter,!bUserParameterAlreadyExisted)||!FindExpectedPin(W,TargetPin)||TargetPin->LinkedTo.Num()!=1||!FindExpectedPin(W,TargetPin->LinkedTo[0]))return W.Error(TEXT("Final exact typed User target or getter edge is unavailable."));
    auto* GetterPin=TargetPin->LinkedTo[0];auto* Getter=GetterPin->GetOwningNodeUnchecked();if(!Getter->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive)||GetterPin->PinName!=LinkUserParameter.GetName()||PinTypeForPath(GetterPin)!=InputType)return W.Error(TEXT("Final getter does not reference requested full typed User parameter."));
    if(!ContinueRead())return W.Error(W.R.Error);if(ExpectedDefault&&!SameProjectedValue(ExpectedDefault,NiagaraParameterValueToJson(UserParameterStore,UserParameter)))return W.Error(TEXT("Selected User default changed after compile/save."));
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_script_path"), ScratchPadScript->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_script_name"), ScratchPadScript->GetName());
    Result->SetStringField(TEXT("scratch_pad_owner_kind"), ResolvedScratchPadOwnerKind);
    Result->SetStringField(TEXT("scratch_pad_owner_name"), ResolvedScratchPadOwnerName);
    Result->SetNumberField(TEXT("scratch_pad_emitter_index"), ResolvedScratchPadEmitterIndex);
    Result->SetNumberField(TEXT("scratch_pad_script_index"), ResolvedScratchPadScriptIndex);
    Result->SetStringField(TEXT("parameter_map_set_node_name"), MatchedSetNode->GetName());
    Result->SetStringField(TEXT("parameter_map_set_node_guid"), MatchedSetNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("target_node_index"), MatchedGraphNodeIndex);
    Result->SetNumberField(TEXT("parameter_map_set_index"), MatchedParameterMapSetIndex);
    Result->SetStringField(TEXT("target_pin_name"), TargetPin->PinName.ToString());
    Result->SetStringField(TEXT("input_type"), NiagaraTypeName(InputType));
    Result->SetStringField(TEXT("user_parameter_name"), UserParameter.GetName().ToString());
    Result->SetStringField(TEXT("linked_parameter_name"), LinkUserParameter.GetName().ToString());
    Result->SetBoolField(TEXT("user_parameter_created"), !bUserParameterAlreadyExisted);
    Result->SetBoolField(TEXT("default_value_set"), bHasDefaultValue);
    if (bHasDefaultValue)
    {
        Result->SetField(TEXT("default_value"), NiagaraParameterValueToJson(UserParameterStore, UserParameter));
    }
    Result->SetStringField(TEXT("previous_default_value"), PreviousDefaultValue);
    Result->SetStringField(TEXT("previous_default_object"), PreviousDefaultObject);
    Result->SetNumberField(TEXT("previous_link_count"), PreviousLinkCount);
    Result->SetNumberField(TEXT("broken_link_count"), BrokenLinkCount);
    Result->SetNumberField(TEXT("new_link_count"), TargetPin->LinkedTo.Num());
    Result->SetArrayField(TEXT("previous_linked_sources"), PreviousLinkedSources);
    Result->SetArrayField(TEXT("new_linked_sources"), LinkedPinsToJson(TargetPin, true));
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("scratch_pad_pin_user_parameter_link"));
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);
    return Result;

}

FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
    bool LeafSuccess=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),LeafSuccess);bool Complete=false,Typed=false;
    W.R.Data->TryGetBoolField(TEXT("typed_target_postcondition_passed"),Typed);
    if(W.Before)W.R.Data->SetObjectField(TEXT("before"),W.Before);
    if(LeafSuccess&&Typed&&W.R.Error.IsEmpty()&&W.Before)
    {
        auto After=W.Projection();W.R.Data->SetObjectField(TEXT("after"),After);
        bool BeforeComplete=false,AfterComplete=false;W.Before->TryGetBoolField(TEXT("source_projection_complete"),BeforeComplete);After->TryGetBoolField(TEXT("source_projection_complete"),AfterComplete);
        if(BeforeComplete&&AfterComplete){W.R.bChanged|=!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive);Complete=W.Admit(TEXT("final_target_admission"));}
    }
    if(!LeafSuccess&&W.R.Error.IsEmpty()){FString Message,Code;if(Legacy){Legacy->TryGetStringField(TEXT("error"),Message);Legacy->TryGetStringField(TEXT("error_code"),Code);}W.Fail(Code.IsEmpty()?TEXT("niagara_resource_bindings_request_failed"):*Code,Message.IsEmpty()?TEXT("Original resource binding request failed; see legacy_result."):Message);}
    if(LeafSuccess&&(!Typed||!Complete)&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_resource_bindings_postcondition_incomplete"),TEXT("Dedicated typed readback and complete before/after target projections are required; partial state retained."));
    if(!W.SaveHealth&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_resource_bindings_save_failed"),TEXT("Current SDK save failed or returned with dirty target; previous true save history retained."));
    W.R.bSuccess=LeafSuccess&&Typed&&Complete&&W.SaveHealth&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;
    const bool Scoped=W.R.bSuccess&&Complete;if(Scoped)W.R.bChangeStateKnown=true;
    W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());
    W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);
    W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.Saves);W.R.Data->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);W.R.Data->SetArrayField(TEXT("actual_item_prefix"),W.Partial);W.R.Data->SetArrayField(TEXT("actual_sdk_prefix"),W.MutationRows);
    W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),Complete);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),Scoped);
    W.R.Data->SetStringField(TEXT("scoped_known_scope"),TEXT("Dedicated typed binding/default/link and canonical User declaration/value readback, complete bounded System/emitter/script/source/graph/pin projections and final admission. SDK loader/cache/global purity, every DI internal field, GPU resource contents and compilation completion are outside this scope."));
    W.R.Data->SetBoolField(TEXT("required_compile_wait_implemented"),W.DeferredMode);W.R.Data->SetBoolField(TEXT("automatic_failure_cleanup_implemented"),false);
    W.R.Data->SetBoolField(TEXT("partial_created_DI_residual_possible"),!W.R.bSuccess&&W.R.bMutationAttempted);
    W.R.Data->SetStringField(TEXT("failure_cleanup_scope"),TEXT("Existing dedicated override cleanup before replacement uses guarded public graph APIs. A newly created DI/override/User declaration may remain after a later failure. Original cast/AddParameter failure cleanup is not claimed equivalent; unsafe failure never triggers automatic delete, rollback or replay. Recorded creation paths and actual prefix are retained."));
    W.R.Data->SetStringField(TEXT("compile_scope"),W.DeferredMode?TEXT("DI/User binding and optional compile/save precede independent finite GT wait, including pre-existing work when request_compile is false. Busy Poll then current status then deadline; timeout remains a legacy diagnostic and compiled remains unproven/null after request. No global pump/Sleep/flush; SDK internal costs may block."):TEXT("Synchronous point observation only; genuine wait needing continuation is refused before writer. Scratch APIs retain their original no-wait contracts."));
    W.R.Data->SetStringField(TEXT("save_scope"),TEXT("SDK SaveLoadedAsset true is actual OR history preserved before post-SDK identity and dirty checks. Later failure does not erase it or clear dirty. RT/Texture retain optional save-before-wait order. Cold persistence, SDK callback atomicity and package-wide unrelated exports are not established by target readback."));
    W.R.Data->SetBoolField(TEXT("atomic_asset_or_undo_rollback_verified"),false);
    W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;
    return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,FHandler Handler)
{
    FWork W;
    if(!IsInGameThread()){W.Fail(TEXT("niagara_resource_bindings_game_thread_required"),TEXT("Editor game thread required; no SDK dispatch."));return Finish(W,W.Error(W.R.Error),Operation);}
    if(Running||DeferredLeaseActive){W.Fail(TEXT("niagara_resource_bindings_reentrant_request"),TEXT("Dedicated Niagara resource binding request already active; no nested dispatch or replay."));return Finish(W,W.Error(W.R.Error),Operation);}
    TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);TSharedPtr<FJsonObject> Params;
    if(!ParseParams(Text,Params)){W.Fail(TEXT("niagara_resource_bindings_invalid_params_json"),TEXT("Finite JSON object within 1Mi UTF16/depth64/262144 tokens and visits required; duplicate case-sensitive keys and embedded NUL are rejected."));return Finish(W,W.Error(W.R.Error),Operation);}
    W.R.Data->SetObjectField(TEXT("requested_params"),MakeShared<FJsonObject>(*Params));
    W.R.bSaveRequested=true;Params->TryGetBoolField(TEXT("save"),W.R.bSaveRequested);
    W.R.bCompileRequested=true;Params->TryGetBoolField(TEXT("request_compile"),W.R.bCompileRequested);
    if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);
    return Finish(W,Handler(W,Params),Operation);
}
}
FString SetRenderTarget2D(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_render_target2d_module_input"),&HandleRenderTarget2D);}
FString SetTexture2D(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_texture2d_module_input"),&HandleTexture2D);}
FString SetScratchDefault(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_scratch_pad_function_input_default"),&HandleScratchDefault);}
FString LinkScratchUser(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("link_niagara_scratch_pad_pin_to_user_parameter"),&HandleScratchUser);}
}

namespace Cubeless::UE58::NiagaraResourceBindingAsync
{
using namespace Cubeless::UE58::NiagaraResourceBindings;
namespace
{
std::atomic<bool>& Stopping=DeferredStopping;
TSharedPtr<FJobState> ActiveJob;
TMap<const UCubelessNiagaraResourceBindingJob*,FString> SnapshotCache;
TMap<const UCubelessNiagaraResourceBindingJob*,TWeakObjectPtr<UCubelessNiagaraResourceBindingJob>> CacheOwners;
// Unsafe records retain exact target/Job holds; Shutdown never drops them inside SDK calls or GC.
TArray<TSharedPtr<FJobState>>& UnsafeRecords(){static auto* Records=new TArray<TSharedPtr<FJobState>>();return *Records;}
enum class EPhase:uint8 { Prepare, Continue, Final, Terminal, Quarantined };
}
struct FJobState:public TSharedFromThis<FJobState>
{
 FWork Work;
 TStrongObjectPtr<UCubelessNiagaraResourceBindingJob> Job{nullptr};
 UCubelessNiagaraResourceBindingJob* RawJob=nullptr;
 TSharedPtr<FJsonObject> Params,Legacy;
 FString Operation,Id,LastSnapshot;
 EPhase Phase=EPhase::Prepare;
 FTSTicker::FDelegateHandle Ticker;
 FDelegateHandle PreExit;
 uint64 LastFrame=MAX_uint64;
 int64 Revision=0;
 bool Executing=false,Dispatched=false,FinalEncoded=false;
 explicit FJobState(const TCHAR* Op):Operation(Op),Id(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens))
 {Work.DeferredMode=true;Work.R.Data->SetBoolField(TEXT("async_resource_binding_continuation"),true);}
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
  D->SetStringField(TEXT("continuation_scope"),TEXT("Exact existing DI/User binding core on finite GT ticks. Optional compile/save precede independent wait, including pre-existing pending work without new request. Bounded timeout is diagnostic, not compiled success. No global pump/Sleep/replay; SDK calls may block internally."));
  D->SetObjectField(TEXT("native_call_counts"),Work.Counts);
  
  
  D->SetArrayField(TEXT("actual_save_prefix"),Work.Saves);D->SetArrayField(TEXT("actual_compile_prefix"),Work.Compiles);D->SetArrayField(TEXT("actual_sdk_prefix"),Work.MutationRows);D->SetArrayField(TEXT("actual_item_prefix"),Work.Partial);
  D->SetObjectField(TEXT("legacy_result"),Legacy.IsValid()?Legacy:MakeShared<FJsonObject>());
 }
 void Cache(const FString& Encoded,bool Terminal,bool ObjectSafe)
 {
  LastSnapshot=Encoded;
  if(RawJob)SnapshotCache.Add(RawJob,Encoded);
  if(ObjectSafe&&Job.IsValid())
  {
   Job->SnapshotJson=Encoded;
   // Snapshot precedes the reflected terminal property.
   Job->bIsTerminal=Terminal;
  }
 }
 void Quarantine(const FString& Reason)
 {
  if(Phase==EPhase::Quarantined)return;
  Unhook();
  Work.CancelRequested=true;Work.CancelReason=Reason;RefreshLegacy();
  if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_resource_bindings_async_context_quarantined"),Reason);
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
  if(!PublicationSafe()){Quarantine(TEXT("Editor/world/GC/exit unavailable while publishing pending resource binding snapshot."));return;}
  Cache(Operations::EncodeVariableResult(Work.R,*Operation),false,true);
 }
 void SetFailureLegacy()
 {
  if(!Legacy)Legacy=MakeShared<FJsonObject>();
  Legacy->SetBoolField(TEXT("success"),false);
  Legacy->SetStringField(TEXT("error"),Work.R.Error.IsEmpty()?TEXT("ResourceBinding job stopped; actual prefix retained."):Work.R.Error);
 }
 void Finalize()
 {
  if(FinalEncoded||Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("ResourceBinding terminal publication lost its exact Editor world; no recovery or replay."));return;}
  if(!Work.R.Error.IsEmpty()){RefreshLegacy();SetFailureLegacy();}
  Metadata(true);
  LastSnapshot=Finish(Work,Legacy,*Operation);
  FinalEncoded=true;
  if(!PublicationSafe()){FinalEncoded=false;Quarantine(TEXT("ResourceBinding final guard lost publication context."));return;}
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
  if(Work.CancelRequested||Stopping.load())
  {
   if(!Work.CancelRequested){Work.CancelRequested=true;Work.CancelReason=TEXT("Module shutdown stopped the owned resource binding job.");}
   Work.Fail(TEXT("niagara_resource_bindings_cancelled"),Work.CancelReason);
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
   if(Dispatched){Work.Fail(TEXT("niagara_resource_bindings_async_replay_forbidden"),TEXT("Original operation already dispatched."));Phase=EPhase::Final;Finalize();return;}
   Dispatched=true;Legacy=Operation==TEXT("set_niagara_render_target2d_module_input")?HandleRenderTarget2D(Work,Params):HandleTexture2D(Work,Params);if(Legacy.IsValid())Work.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
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
  if(!PublicationSafe()){Quarantine(TEXT("ResourceBinding ticker lost live Editor/game-thread/world context."));return false;}
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
  PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([Weak](){Stopping.store(true);if(!IsInGameThread())return;auto State=Weak.Pin();if(State.IsValid()){State->Work.CancelRequested=true;State->Work.CancelReason=TEXT("Engine pre-exit stopped resource binding job.");State->Unhook();if(!State->Executing)State->Quarantine(State->Work.CancelReason);}});
 }
};
namespace
{
void PruneSnapshots()
{
 for(auto It=CacheOwners.CreateIterator();It;++It)if(!It.Value().IsValid()){SnapshotCache.Remove(It.Key());It.RemoveCurrent();}
}
UCubelessNiagaraResourceBindingJob* Start(const FString& Text,const TCHAR* Operation)
{
 if(!IsInGameThread()||Stopping.load()||Running||DeferredLeaseActive||!Safe())return nullptr;
 auto State=MakeShared<FJobState>(Operation);
 State->Work.R.bSaveRequested=true;
 if(!State->Work.Admit(TEXT("async_resource_binding_start_admission")))return nullptr;
 PruneSnapshots();
 if(SnapshotCache.Num()>=256)return nullptr;
 ActiveJob=State;DeferredLeaseActive=true;
 UCubelessNiagaraResourceBindingJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(State->Executing,true);
  TGuardValue<bool> CoreLease(Running,true);
  TGuardValue<FWork*> Context(Current,&State->Work);
  if(State->Work.SDK(TEXT("NewObject_ResourceBindingJob"),true))
  {
   Created=NewObject<UCubelessNiagaraResourceBindingJob>(GetTransientPackage());
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_returned"),true);
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
   State->RawJob=Created;
   if(Stopping.load()||State->Work.CancelRequested||!State->PublicationSafe())
   {
    State->Quarantine(TEXT("ResourceBinding Job allocation interrupted; persistent resource binding dispatch not entered."));
    return nullptr;
   }
   if(!State->Work.After(TEXT("NewObject_ResourceBindingJob"))||!Created)
   {
    State->Quarantine(TEXT("ResourceBinding Job allocation could not be safely adopted."));
    return nullptr;
   }
   State->Job.Reset(Created);CacheOwners.Add(Created,Created);
   if(!ParseParams(Text,State->Params))
   {
    State->Work.Fail(TEXT("niagara_resource_bindings_invalid_params_json"),TEXT("Finite JSON object within1Mi/depth64/262144 slots required; raw presence/defaults are preserved."));
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
UCubelessNiagaraResourceBindingJob* StartRenderTargetInput(const FString& ParamsJson){return Start(ParamsJson,TEXT("set_niagara_render_target2d_module_input"));}
UCubelessNiagaraResourceBindingJob* StartTextureInput(const FString& ParamsJson){return Start(ParamsJson,TEXT("set_niagara_texture2d_module_input"));}
FString SnapshotJson(const UCubelessNiagaraResourceBindingJob* Job)
{
 if(!IsInGameThread()||!Job)return FString();
 if(const FString* Snapshot=SnapshotCache.Find(Job))return *Snapshot;
 return FString();
}
bool Cancel(UCubelessNiagaraResourceBindingJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!Job||!ActiveJob.IsValid()||ActiveJob->RawJob!=Job)return false;
 auto State=ActiveJob;
 if(State->Phase==EPhase::Terminal||State->Phase==EPhase::Quarantined)return false;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return true;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Cancellation lost resource binding publication context; actual prefix retained."));return true;}
 {
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> CoreLease(Running,true);TGuardValue<FWork*> Context(Current,&State->Work);
  State->Work.Fail(TEXT("niagara_resource_bindings_async_cancelled"),State->Work.CancelReason);
  State->Phase=EPhase::Final;State->Finalize();
 }
 State->ReleaseSafe();return true;
}
void Shutdown(const FString& Reason)
{
 Stopping.store(true);
 if(!IsInGameThread())return;
 auto State=ActiveJob;if(!State.IsValid())return;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe resource binding state without touching UObject publication."));return;}
 Cancel(State->RawJob,State->Work.CancelReason);
}
}
