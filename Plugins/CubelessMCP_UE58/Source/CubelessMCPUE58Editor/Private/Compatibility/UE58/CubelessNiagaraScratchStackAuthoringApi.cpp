
// GR dedicated Editor-only Niagara scratch/stack authoring. Source-only candidate.
#include "Compatibility/UE58/CubelessNiagaraScratchStackAuthoringApi.h"
#include "CubelessNiagaraScratchStackAuthoringJob.h"
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
namespace Cubeless::UE58::NiagaraScratchStackAuthoring
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
{UEdGraphPin* Pointer=nullptr;FGuid Id,PersistentId;bool NotConnectable=false,DefaultReadOnly=false;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;TArray<UEdGraphPin*> Links;FString DefaultValue,AutoDefault,DefaultText;TWeakObjectPtr<UObject> DefaultObject;};
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
    TSet<UNiagaraNodeCustomHlsl*> FreshUnlinkedHlslNodes;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Saves,Compiles,Partial,MutationRows;TSharedPtr<FJsonObject> Before;int64 Slots=0,TextUnits=0,Calls=0;
    FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty())Fail(TEXT("niagara_scratch_stack_request_failed"),Message.IsEmpty()?TEXT("Dedicated graph operation failed; actual prefix retained."):Message);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
    bool Budget(int64 N,int64 Text=0){Slots+=N;TextUnits+=Text;Prefix->SetNumberField(TEXT("captured_slots"),static_cast<double>(Slots));Prefix->SetNumberField(TEXT("observed_text_utf16_units"),static_cast<double>(TextUnits));return (Slots<=262144&&TextUnits<=32ll*1024*1024)||Fail(TEXT("niagara_scratch_stack_budget_exceeded"),TEXT("Bounded capture/output budget exceeded; partial prefix retained."));}
    bool Pin(UObject* O){if(!Live(O))return Fail(TEXT("niagara_scratch_stack_object_unavailable"),TEXT("Expected live loaded object."));for(const auto& I:Objects){if(I->Object.Get()==O)return I->Exact()||Fail(TEXT("niagara_scratch_stack_identity_changed"),TEXT("Captured object changed."));}if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool Own(UObject* O)
    {auto* Root=System.Get();if(!Admit(TEXT("writer_ownership"))||!Live(O)||!Live(Root)||(O!=Root&&!O->IsIn(Root))||O->GetOutermost()!=Root->GetOutermost()||O->GetPackage()!=Root->GetPackage())return Fail(TEXT("niagara_scratch_stack_external_writer_unsupported"),TEXT("Actual writer is outside the specified System outer chain/package. No external emitter/shared graph/source was changed."));if(!Pin(O))return false;for(auto& I:Objects){if(I->Object.Get()==O){I->Writer=true;I->Root=Root;}}return true;}
    bool Admit(const FString& Stage)
    {
        R.Stage=Stage;if(DeferredMode&&(DeferredStopping.load()||CancelRequested))return Fail(TEXT("niagara_scratch_stack_async_cancelled"),CancelReason.IsEmpty()?TEXT("Owned Scratch module-input continuation stopped; no further SDK."):CancelReason);if(!R.Error.IsEmpty())return false;
        if(!IsInGameThread()||!GEditor||IsEngineExitRequested()||GExitPurge||IsGarbageCollecting()||GIsSavingPackage)return Fail(TEXT("niagara_scratch_stack_context_unavailable"),TEXT("Live Editor game thread without GC/exit/save required; no replay."));
        if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_scratch_stack_editor_changed"),TEXT("Captured Editor changed before getter; no replay."));
        if(WriterIntent&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext()))return Fail(TEXT("niagara_scratch_stack_play_session_active"),TEXT("Niagara binding authoring requires stopped PIE/SIE."));
        UWorld* CurrentWorld=GEditor->GetEditorWorldContext().World();if(!Live(CurrentWorld)||CurrentWorld->bIsTearingDown)return Fail(TEXT("niagara_scratch_stack_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){World=CurrentWorld;if(DeferredMode)CapturedEditor=GEditor;WorldCaptured=true;}else if(World.Get()!=CurrentWorld)return Fail(TEXT("niagara_scratch_stack_world_changed"),TEXT("Editor world changed across SDK dispatch."));
        for(const auto& I:Objects){if(!I->Exact())return Fail(TEXT("niagara_scratch_stack_identity_changed"),TEXT("Captured class/outer/package/path/ownership changed; no replacement adopted."));}
        if(System.IsValid())
        {auto* O=System.Get();if(O->GetSystemSpawnScript()!=Spawn.Get()||O->GetSystemUpdateScript()!=Update.Get()||O->ScratchPadScripts.Num()!=Scratch.Num()||O->GetEmitterHandles().Num()!=Emitters.Num())return Fail(TEXT("niagara_scratch_stack_system_topology_changed"),TEXT("Captured System emitter/script topology changed."));
            for(int32 I=0;I<Scratch.Num();++I)if(O->ScratchPadScripts[I]!=Scratch[I].Get())return Fail(TEXT("niagara_scratch_stack_scratch_membership_changed"),TEXT("System scratch membership changed."));
            for(int32 I=0;I<Emitters.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=Emitters[I];if(H.GetId()!=E.Id||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.InstanceVersion||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion))return Fail(TEXT("niagara_scratch_stack_emitter_version_changed"),TEXT("Physical emitter/ref/version changed."));
                if(E.Data){if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.ParentScratch.Get())return Fail(TEXT("niagara_scratch_stack_emitter_routing_changed"),TEXT("Emitter source/scratch binding changed."));
                    auto Same=[](const UNiagaraScratchPadContainer* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& P){if(!C)return P.IsEmpty();if(C->Scripts.Num()!=P.Num())return false;for(int32 J=0;J<P.Num();++J)if(C->Scripts[J]!=P[J].Get())return false;return true;};
                    if(!Same(E.Scratch.Get(),E.ScratchScripts)||!Same(E.ParentScratch.Get(),E.ParentScripts))return Fail(TEXT("niagara_scratch_stack_scratch_membership_changed"),TEXT("Emitter scratch membership changed."));
                    const auto& A=E.Data->GetSimulationStages();if(A.Num()!=E.Stages.Num())return Fail(TEXT("niagara_scratch_stack_stage_membership_changed"),TEXT("Simulation stage count changed."));for(int32 J=0;J<A.Num();++J)if(A[J]!=E.Stages[J].Get())return Fail(TEXT("niagara_scratch_stack_stage_membership_changed"),TEXT("Simulation stage order changed."));
                }
            }
        }
        for(const auto& S:ScriptBindings){auto* O=S.Script.Get();if(!Live(O)||O->GetLatestSource()!=S.Source.Get()||O->GetExposedVersion().VersionGuid!=S.Version)return Fail(TEXT("niagara_scratch_stack_script_source_changed"),TEXT("Script source/exposed version changed."));}
        for(const auto& S:StageBindings){auto* O=S.Stage.Get();if(!Live(O)||O->Script!=S.Script.Get()||O->SimulationStageName!=S.Name)return Fail(TEXT("niagara_scratch_stack_stage_script_changed"),TEXT("Stage name/script binding changed."));}
        for(const auto& S:Sources){if(!Live(S.Source.Get())||S.Source->NodeGraph!=S.Graph.Get())return Fail(TEXT("niagara_scratch_stack_source_graph_changed"),TEXT("Captured source graph binding changed."));}
        for(const auto& G:Graphs){if(G.Graph.Get()==TransitionGraph)continue;auto* O=G.Graph.Get();if(!Live(O)||O->Schema!=G.Schema.Get()||O->Nodes.Num()!=G.Nodes.Num()||!GraphMetadataSignature(O).Equals(G.Meta,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_scratch_stack_graph_topology_changed"),TEXT("Graph node count changed outside the planned removal."));
            for(int32 I=0;I<G.Nodes.Num();++I){const auto& E=G.Nodes[I];auto* N=E.Node.Get();if(!Live(N)||O->Nodes[I]!=N||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num())return Fail(TEXT("niagara_scratch_stack_node_identity_changed"),TEXT("Captured node/pin membership changed."));
                if(E.Function){auto* F=Cast<UNiagaraNodeFunctionCall>(N);if(!F||F->FunctionScript!=E.FunctionScript.Get()||F->SelectedScriptVersion!=E.FunctionVersion||F->FunctionScriptAssetObjectPath!=E.FunctionAsset||F->FunctionSpecifiers.OrderIndependentCompareEqual(E.Specifiers)==false||!FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&F->Signature,&E.FunctionSignature,0))return Fail(TEXT("niagara_scratch_stack_function_binding_changed"),TEXT("Selected function script/version/signature/specifiers changed across SDK callback."));}
                for(int32 J=0;J<E.Pins.Num();++J){const auto& P=E.Pins[J];if(N->Pins[J]!=P.Pointer)return Fail(TEXT("niagara_scratch_stack_pin_identity_changed"),TEXT("Captured pin disappeared; stale pointer not dereferenced."));if(P.Pointer&&(P.Pointer->PinId!=P.Id||P.Pointer->PersistentGuid!=P.PersistentId||P.Pointer->bNotConnectable!=P.NotConnectable||P.Pointer->bDefaultValueIsReadOnly!=P.DefaultReadOnly||P.Pointer->GetOwningNodeUnchecked()!=N||!P.Pointer->PinName.ToString().Equals(P.Name.ToString(),ESearchCase::CaseSensitive)||P.Pointer->Direction!=P.Direction||P.Pointer->PinType!=P.Type||P.Pointer->LinkedTo!=P.Links||!P.Pointer->DefaultValue.Equals(P.DefaultValue,ESearchCase::CaseSensitive)||!P.Pointer->AutogeneratedDefaultValue.Equals(P.AutoDefault,ESearchCase::CaseSensitive)||!P.Pointer->DefaultTextValue.ToString().Equals(P.DefaultText,ESearchCase::CaseSensitive)||P.Pointer->DefaultObject!=P.DefaultObject.Get()))return Fail(TEXT("niagara_scratch_stack_pin_links_changed"),TEXT("Pin identity/type/links differ from expected admitted transition."));}
            }
        }for(const auto& E:ExpectedDI){auto* Node=E.Node.Get();auto* DI=E.DI.Get();if(!Live(Node)||!Live(DI)||ReflectedNodeDI(Node)!=DI||Node->Input!=E.Input||Node->CallSortPriority!=E.Sort||Node->Usage!=E.Usage||DI->GetOuter()!=Node||DI->GetPackage()!=System->GetPackage())return Fail(TEXT("niagara_scratch_stack_DI_replaced"),TEXT("Captured DI input-node binding/owner changed."));if(E.RT){auto* D=Cast<UNiagaraDataInterfaceRenderTarget2D>(DI);if(!D||D->RenderTargetUserParameter.Parameter!=E.User||D->bInheritUserParameterSettings!=E.Inherit)return Fail(TEXT("niagara_scratch_stack_DI_fields_changed"),TEXT("RT public binding fields changed outside requested assignment."));}else{auto* D=Cast<UNiagaraDataInterfaceTexture>(DI);if(!D||D->TextureUserParameter.Parameter!=E.User||D->Texture!=E.Texture.Get())return Fail(TEXT("niagara_scratch_stack_DI_fields_changed"),TEXT("Texture public binding fields changed outside requested assignment."));}}return true;
    }
    TSharedRef<FJsonObject> MutationRow(const TCHAR* Stage){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetBoolField(TEXT("SDK_returned"),false);MutationRows.Add(MakeShared<FJsonValueObject>(Row));return Row;}
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage))return false;if(!Budget(1,FCString::Strlen(Stage)))return false;if(++Calls>262144)return Fail(TEXT("niagara_scratch_stack_sdk_budget"),TEXT("SDK call budget exceeded."));R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;if(DeferredMode&&Mutation&&FCString::Strcmp(Stage,TEXT("NewObject_ScratchStackAuthoringJob"))!=0)PersistentMutationAttempted=true;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(Mutation)Prefix->SetBoolField(Stage,true);return true;}
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
#define GR_STEP(Stage,Mutation,Expr) do{if(!W.Step(TEXT(Stage),Mutation,[&]{Expr;}))return W.Error(W.R.Error);}while(false)

bool FWork::CaptureSystem(UNiagaraSystem* O)
{
    if(!Pin(O)||!Pin(O->GetOutermost()))return false;System=O;Spawn=O->GetSystemSpawnScript();Update=O->GetSystemUpdateScript();if(!CaptureScript(Spawn.Get())||!CaptureScript(Update.Get()))return false;
    for(UNiagaraScript* S:O->ScratchPadScripts){if(S&&!CaptureScript(S))return false;Scratch.Add(S);}
    if(!Budget(O->GetEmitterHandles().Num())||O->GetEmitterHandles().Num()>4096)return Fail(TEXT("niagara_scratch_stack_emitter_bound"),TEXT("Emitter inventory exceeds bound."));
    for(const auto& H:O->GetEmitterHandles()){FEmitterIdentity E;E.Id=H.GetId();E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Emitter.IsValid()&&!Pin(E.Emitter.Get()))return false;if(E.Data){E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.ParentScratch=E.Data->ParentScratchPads;if(E.Source.IsValid()&&!Pin(E.Source.Get()))return false;
        auto CaptureScripts=[&](UNiagaraScratchPadContainer* C,TArray<TWeakObjectPtr<UNiagaraScript>>& A){if(!C)return true;if(!Pin(C)||!Budget(C->Scripts.Num()))return false;for(UNiagaraScript* S:C->Scripts){if(S&&!CaptureScript(S))return false;A.Add(S);}return true;};
        if(!CaptureScripts(E.Scratch.Get(),E.ScratchScripts)||!CaptureScripts(E.ParentScratch.Get(),E.ParentScripts))return false;
        TArray<UNiagaraScript*> OwnedScripts;E.Data->GetScripts(OwnedScripts,false,false);if(!Budget(OwnedScripts.Num()))return false;for(UNiagaraScript* P:OwnedScripts)if(!CaptureScript(P))return false;
        if(!Budget(E.Data->GetSimulationStages().Num()))return false;for(UNiagaraSimulationStageBase* S:E.Data->GetSimulationStages()){if(S&&(!Pin(S)||!CaptureScript(S->Script)))return false;if(S)StageBindings.Add({S,S->Script,S->SimulationStageName});E.Stages.Add(S);}
    }Emitters.Add(MoveTemp(E));}return true;
}
UNiagaraSystem* LoadSystem(const FString& Path)
{
    if(!Current->SDK(TEXT("LoadObject_System")))return nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* O=LoadObject<UNiagaraSystem>(nullptr,*Expected);Current->Prefix->SetBoolField(TEXT("load_returned_nonnull"),O!=nullptr);if(!Current->After(TEXT("LoadObject_System")))return nullptr;if(O&&(!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Current->CaptureSystem(O))){Current->Fail(TEXT("niagara_scratch_stack_load_identity_unexpected"),TEXT("Loaded System differs from requested normalized exact path."));return nullptr;}return O;
}
bool FWork::CaptureGraph(UNiagaraScriptSource* S)
{
    if(!Admit(TEXT("capture_graph")))return false;if(!S||!S->NodeGraph)return true;if(!Pin(S)||!Pin(S->NodeGraph))return false;auto* G=S->NodeGraph.Get();
    bool Bound=false;for(const auto& E:Sources)if(E.Source.Get()==S){Bound=true;if(E.Graph.Get()!=G)return Fail(TEXT("niagara_scratch_stack_source_graph_changed"),TEXT("Source graph changed."));}
    if(!Bound)Sources.Add({S,G});for(const auto& E:Graphs)if(E.Graph.Get()==G)return true;
    if(!Budget(G->Nodes.Num())||G->Nodes.Num()>16384)return Fail(TEXT("niagara_scratch_stack_graph_bound"),TEXT("Graph node inventory exceeds bound."));
    if(G->GetAllMetaData().Num()>65536||!Budget(G->GetAllMetaData().Num()))return false;for(const auto& M:G->GetAllMetaData()){if(!Live(M.Value.Get())||!Pin(M.Value.Get()))return Fail(TEXT("niagara_scratch_stack_metadata_invalid"),TEXT("Live typed metadata required."));}
    FGraphIdentity E;E.Graph=G;E.Schema=G->Schema.Get();E.Meta=GraphMetadataSignature(G);
    for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N)||N->GetGraph()!=G)return Fail(TEXT("niagara_scratch_stack_node_owner_invalid"),TEXT("Null/foreign graph node cannot be safely inspected or mutated."));FNodeIdentity NE;NE.Node=N;NE.Id=N->NodeGuid;NE.X=N->NodePosX;NE.Y=N->NodePosY;if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()>65536||!Budget(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()))return Fail(TEXT("niagara_scratch_stack_signature_bound"),TEXT("Function signature inventory exceeds bound."));NE.Function=true;NE.FunctionScript=F->FunctionScript;NE.FunctionVersion=F->SelectedScriptVersion;NE.FunctionAsset=F->FunctionScriptAssetObjectPath;NE.FunctionSignature=F->Signature;NE.Specifiers=F->FunctionSpecifiers;if(F->FunctionScript&&!CaptureScript(F->FunctionScript))return false;}if(!Budget(N->Pins.Num()))return false;
        for(UEdGraphPin* P:N->Pins){if(!P)return Fail(TEXT("niagara_scratch_stack_null_pin"),TEXT("Null graph pin unsupported by Niagara public getters."));if(P->GetOwningNodeUnchecked()!=N||!Budget(P->LinkedTo.Num()))return Fail(TEXT("niagara_scratch_stack_pin_owner_invalid"),TEXT("Pin owner/link inventory is invalid."));FPinIdentity PE;PE.Pointer=P;PE.Id=P->PinId;PE.PersistentId=P->PersistentGuid;PE.NotConnectable=P->bNotConnectable;PE.DefaultReadOnly=P->bDefaultValueIsReadOnly;PE.Name=P->PinName;PE.Direction=P->Direction;PE.Type=P->PinType;PE.Links=P->LinkedTo;PE.DefaultValue=P->DefaultValue;PE.AutoDefault=P->AutogeneratedDefaultValue;PE.DefaultText=P->DefaultTextValue.ToString();PE.DefaultObject=P->DefaultObject;if(P->DefaultObject&&!Pin(P->DefaultObject))return false;if(P->PinType.PinSubCategoryObject.IsValid()&&!Pin(P->PinType.PinSubCategoryObject.Get()))return false;NE.Pins.Add(MoveTemp(PE));}
        E.Nodes.Add(MoveTemp(NE));}
    TSet<UEdGraphPin*> Known;for(const auto& N:E.Nodes)for(const auto& P:N.Pins)Known.Add(P.Pointer);for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links){if(!L||!Known.Contains(L))return Fail(TEXT("niagara_scratch_stack_unresolved_link_identity"),TEXT("Null/cross-graph link is outside captured exact pin membership; no stale link dereference."));}
    Graphs.Add(MoveTemp(E));return true;
}
bool FWork::GraphForWrite(UNiagaraGraph* G)
{
    if(!Admit(TEXT("graph_writer_admission"))||!Own(G))return false;bool Captured=false;
    for(const auto& S:Sources){if(S.Graph.Get()==G){if(!Own(S.Source.Get()))return false;Captured=true;}}
    TSet<UNiagaraScriptSource*> Sharing;int32 EmitterOwners=0;bool SystemOwner=false;auto HasGraph=[&](UNiagaraScriptSourceBase* Base){auto* Source=Cast<UNiagaraScriptSource>(Base);if(!Source)return false;if(!Live(Source)){Fail(TEXT("niagara_scratch_stack_shared_source_unresolved"),TEXT("Unloaded source prevents exact shared-graph admission."));return false;}if(Source->NodeGraph==G){Sharing.Add(Source);return true;}return false;};for(const auto& E:Emitters){if(HasGraph(E.Source.Get()))++EmitterOwners;}for(const auto& E:ScriptBindings){const bool Matches=HasGraph(E.Source.Get());if(Matches&&(E.Script.Get()==Spawn.Get()||E.Script.Get()==Update.Get()))SystemOwner=true;}if(!R.Error.IsEmpty())return false;if(Sharing.Num()>1||EmitterOwners>1||(EmitterOwners>0&&SystemOwner))return Fail(TEXT("niagara_scratch_stack_shared_graph_writer_unsupported"),TEXT("The selected graph is shared by multiple source/owner roles. Refused before first Modify; no duplicate or unselected owner mutation."));
    int32 ScratchRoles=0,RegularRoles=EmitterOwners+(SystemOwner?1:0);auto ScratchUses=[&](UNiagaraScript* P){auto* S=P?Cast<UNiagaraScriptSource>(P->GetLatestSource()):nullptr;return S&&S->NodeGraph==G;};for(const auto& P:Scratch)if(ScratchUses(P.Get()))++ScratchRoles;for(const auto& E:Emitters){for(const auto& P:E.ScratchScripts)if(ScratchUses(P.Get()))++ScratchRoles;for(const auto& P:E.ParentScripts)if(ScratchUses(P.Get()))++ScratchRoles;}if(ScratchRoles>1||(ScratchRoles>0&&RegularRoles>0))return Fail(TEXT("niagara_scratch_stack_shared_scratch_graph_unsupported"),TEXT("Mutable scratch graph is shared with another scratch or regular owner role."));
    if(!Captured)return Fail(TEXT("niagara_scratch_stack_uncaptured_graph"),TEXT("Mutable graph has no captured exact source binding."));
    for(const auto& E:Graphs){if(E.Graph.Get()==G){for(const auto& N:E.Nodes)if(!Own(N.Node.Get()))return false;}}
    return true;
}
FPinIdentity* FindExpectedPin(FWork& W,UEdGraphPin* P)
{for(auto& G:W.Graphs)for(auto& N:G.Nodes)for(auto& E:N.Pins)if(E.Pointer==P)return &E;return nullptr;}
bool FWork::PinForWrite(UEdGraphPin* P)
{
    if(!Admit(TEXT("pin_writer_admission"))||!P)return Fail(TEXT("niagara_scratch_stack_pin_unavailable"),TEXT("Pin unavailable."));
    auto* E=FindExpectedPin(*this,P);if(!E)return Fail(TEXT("niagara_scratch_stack_uncaptured_pin"),TEXT("Pin is not in captured exact node membership; stale pointer not dereferenced."));
    auto* N=P->GetOwningNodeUnchecked();auto* G=Cast<UNiagaraGraph>(N->GetGraph());if(!GraphForWrite(G)||!Own(N))return false;
    for(auto* L:E->Links){if(!L||!FindExpectedPin(*this,L))return Fail(TEXT("niagara_scratch_stack_external_pin_link_unsupported"),TEXT("Linked pin is outside captured mutable graph(s)."));auto* LN=L->GetOwningNodeUnchecked();if(LN->GetGraph()!=G||!Own(LN))return Fail(TEXT("niagara_scratch_stack_external_pin_link_unsupported"),TEXT("Cross-graph linked writer is unsupported before mutation."));}
    return true;
}
bool FWork::BreakLinks(UEdGraphPin* P,bool Notify)
{
    if(!PinForWrite(P)||!SDK(TEXT("BreakAllPinLinks"),true))return false;auto* E=FindExpectedPin(*this,P);const TArray<UEdGraphPin*> Links=E->Links;
    for(auto* L:Links){auto* Other=FindExpectedPin(*this,L);if(!Other)return Fail(TEXT("niagara_scratch_stack_link_plan_unavailable"),TEXT("Captured reciprocal pin unavailable."));Other->Links.Remove(P);}E->Links.Empty();
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
    FGraphIdentity* Expected=Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return Fail(TEXT("niagara_scratch_stack_missing_graph_plan"),TEXT("Captured graph plan unavailable."));
    const FNodeIdentity* NE=Expected->Nodes.FindByPredicate([&](const auto& E){return E.Node.Get()==N;});if(!NE)return Fail(TEXT("niagara_scratch_stack_node_already_removed"),TEXT("Exact captured node no longer belongs to graph."));TArray<UEdGraphPin*> Removing;for(const auto& P:NE->Pins)Removing.Add(P.Pointer);
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
    if(Direct&&(!FPackageName::IsValidLongPackageName(P->GetName(),false)||!FPackageName::TryConvertLongPackageNameToFilename(P->GetName(),Path,FPackageName::GetAssetPackageExtension())))return Fail(TEXT("niagara_scratch_stack_save_path_invalid"),TEXT("Exact package filename unavailable."));
    if(!SDK(Direct?TEXT("SavePackage"):TEXT("SaveLoadedAsset"),true))return false;R.bSaveAttempted=true;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_package"),P->GetName());Row->SetStringField(TEXT("requested_asset"),O->GetPathName());bool Saved=false;
    if(Direct){FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;Saved=UPackage::SavePackage(P,O,*Path,Args);}else Saved=UEditorAssetLibrary::SaveLoadedAsset(O,false);
    R.bSaved|=Saved;SaveHealth&=Saved;if(Saved&&Direct&&Filename)*Filename=Path;Row->SetStringField(TEXT("requested_filename"),Path);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Saved);Row->SetBoolField(TEXT("actual_saved_history"),R.bSaved);Saves.Add(MakeShared<FJsonValueObject>(Row));if(!After(Direct?TEXT("SavePackage"):TEXT("SaveLoadedAsset"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}
    const bool DirtyAfter=P->IsDirty();Row->SetStringField(TEXT("package_after"),P->GetName());Row->SetBoolField(TEXT("dirty_after"),DirtyAfter);if(Saved&&DirtyAfter){SaveHealth=false;Fail(TEXT("niagara_scratch_stack_saved_package_dirty"),TEXT("Save returned true but captured package is dirty after SDK callbacks. Save history retained; dirty flag was not cleared or retried."));return false;}return Saved;
}
bool FWork::WaitAdmission(bool Wait,bool WillRequest)
{
    if(!Wait)return true;if(!SDK(TEXT("compile_wait_admission")))return false;bool Pending=System->HasOutstandingCompilationRequests(true)||System->HasActiveCompilations();if(!After(TEXT("compile_wait_admission")))return false;
    if(DeferredMode)return true;
    if(WillRequest||Pending)return Fail(TEXT("niagara_scratch_stack_async_wait_unsupported"),TEXT("Actual requested/pending compile wait needs a deferred final result, exact lifetime and original save/absence continuation. This synchronous implementation rejects before writer; no GT sleep/flush, fake timeout or wait=False substitution."));return true;
}
TSharedPtr<FJsonObject> FWork::WaitSnapshot(bool Wait,double Timeout,double Poll)
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0.0);if(!System.IsValid()){O->SetBoolField(TEXT("outstanding_compilation_requests_after"),false);O->SetBoolField(TEXT("active_compilations_after"),false);return O;}
    if(!SDK(TEXT("compile_status_snapshot")))return O;const bool Pending=System->HasOutstandingCompilationRequests(true),Active=System->HasActiveCompilations();O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Pending);O->SetBoolField(TEXT("active_compilations_after"),Active);if(!After(TEXT("compile_status_snapshot")))return O;if(Wait&&(Pending||Active)){Fail(TEXT("niagara_scratch_stack_async_wait_unsupported"),TEXT("Compile became pending after actual mutation/save; partial history retained. No blocking or replay."));O->SetBoolField(TEXT("actual_wait_implemented"),false);}return O;
}

struct FModuleInputCompileWait
{
 TSharedPtr<FJsonObject> Result=MakeShared<FJsonObject>();
 double Started=0,Next=0,Timeout=0,Poll=.1;int32 Iterations=0;bool Done=false;
 FModuleInputCompileWait(double Seconds,double Interval):Started(FPlatformTime::Seconds()),Timeout(FMath::Clamp(Seconds,0.0,60.0)),Poll(FMath::Clamp(Interval,.02,1.0))
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
void BeginModuleInputWait(FWork& W,UNiagaraSystem* System,double Timeout,double Poll,TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Completion)
{
 auto Wait=MakeShared<FModuleInputCompileWait>(Timeout,Poll);
 W.R.Data->SetObjectField(TEXT("compile_wait_partial"),Wait->Result);
 W.DeferredStep=[&W,System,Wait,Completion=MoveTemp(Completion)]()mutable
 {
  if(!Wait->Tick(W,System))return false;
  if(!W.Admit(TEXT("module_input_wait_completion_admission")))return true;
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
    if(!ContinueRead()||!FindExpectedPin(*Current,P)){Current->Fail(TEXT("niagara_scratch_stack_pin_unresolved"),TEXT("Pin is outside captured exact graph membership."));return Out;}
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
    if(!W.Budget(Variables.Num())||Variables.Num()>16384){W.Fail(TEXT("niagara_scratch_stack_store_bound"),TEXT("Exposed parameter inventory exceeds bound."));return Out;}
    for(const auto& V:Variables)
    {
        FUserValue E;E.Variable=V;if(!Store.RedirectUserVariable(E.Variable)){W.Fail(TEXT("niagara_scratch_stack_user_alias_unresolved"),TEXT("Captured User alias could not be resolved to the exact full-name variable."));return Out;}if(V.GetType().IsDataInterface()){auto* DI=Store.GetDataInterface(V);E.DI=DI;if(DI&&!W.Pin(DI))return Out;}
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
    if(FullParameterCount<0||FullParameterCount>65536||!W.Budget(FullParameterCount))return W.Fail(TEXT("niagara_scratch_stack_full_user_store_bound"),TEXT("Full exposed declaration inventory exceeds the bounded canonical User check."));
    TArray<FNiagaraVariable> FullParametersForUser;CanonicalStore.GetParameters(FullParametersForUser);
    int32 ExactFullMatches=0;for(const FNiagaraVariable& FullParameter:FullParametersForUser)if(FullParameter.GetName()==User.GetName()&&FullParameter.GetType()==User.GetType())++ExactFullMatches;
    FNiagaraVariable CanonicalProbe=User;const bool CanonicalRedirected=CanonicalStore.RedirectUserVariable(CanonicalProbe);
    W.Prefix->SetStringField(TEXT("last_user_full_name_checked"),User.GetName().ToString());
    W.Prefix->SetStringField(TEXT("last_user_full_name_redirect"),CanonicalProbe.GetName().ToString());
    if(ExactFullMatches!=1||!CanonicalRedirected||CanonicalProbe.GetName()!=User.GetName()||CanonicalProbe.GetType()!=User.GetType())return W.Fail(TEXT("niagara_scratch_stack_user_canonical_redirect_unsupported"),TEXT("Selected full typed User declaration is absent, ambiguous, or redirects to another declaration; no next store or graph writer dispatched."));
    return true;
}
bool VerifyUnrelatedUserValues(FWork& W,const TArray<FUserValue>& Before,const FNiagaraVariable& Selected,bool AllowNew)
{
    if(!VerifyCanonicalUserForPath(W,Selected))return false;
    auto After=CaptureUserValues(W);if(!ContinueRead())return false;
    int32 OtherBefore=0,OtherAfter=0;
    auto SelectedMatch=[&](const FUserValue& E){return E.Variable.GetName()==Selected.GetName()&&E.Variable.GetType()==Selected.GetType();};
    for(const auto& E:Before){if(SelectedMatch(E))continue;++OtherBefore;const auto* A=After.FindByPredicate([&](const auto& V){return V.Variable.GetName()==E.Variable.GetName()&&V.Variable.GetType()==E.Variable.GetType();});if(!A||!SameUserValue(E,*A))return W.Fail(TEXT("niagara_scratch_stack_unselected_store_changed"),TEXT("Unselected exposed parameter changed across SDK call."));}
    for(const auto& E:After)if(!SelectedMatch(E))++OtherAfter;
    const int32 SelectedCount=After.FilterByPredicate(SelectedMatch).Num();
    return (OtherBefore==OtherAfter&&SelectedCount==1&&(AllowNew||Before.ContainsByPredicate(SelectedMatch)))||W.Fail(TEXT("niagara_scratch_stack_store_shape_changed"),TEXT("Exposed parameter set differs from the admitted selected transition."));
}
bool PreflightStackMap(FWork& W,UNiagaraNodeFunctionCall* Function)
{
    if(!ContinueRead())return false;auto* G=Cast<UNiagaraGraph>(Function->GetGraph());if(!G||!W.GraphForWrite(G)||!W.Own(Function))return false;
    TArray<UEdGraphPin*> Inputs;for(auto* P:Function->Pins)if(P->Direction==EGPD_Input&&PinTypeForPath(P)==FNiagaraTypeDefinition::GetParameterMapDef())Inputs.Add(P);
    if(!ContinueRead())return false;
    if(Inputs.Num()!=1||Inputs[0]->LinkedTo.Num()!=1||!FindExpectedPin(W,Inputs[0]->LinkedTo[0]))return W.Fail(TEXT("niagara_scratch_stack_stack_map_shape_unsupported"),TEXT("The module must have one exact captured ParameterMap input and one upstream link before public stack SDK."));
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
    if(!GraphMetadataWithout(G,LinkedUser).Equals(OtherMetadataBefore,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_scratch_stack_unselected_metadata_changed"),TEXT("Unselected graph metadata changed during the requested stack transition."));
    if(G->Schema!=Before.Schema.Get()||G->Nodes.Num()<Before.Nodes.Num()||G->Nodes.Num()>Before.Nodes.Num()+1)return W.Fail(TEXT("niagara_scratch_stack_unexpected_node_delta"),TEXT("Public stack operation changed an unexpected number of nodes."));
    for(int32 I=0;I<Before.Nodes.Num();++I)if(G->Nodes[I]!=Before.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_scratch_stack_retained_node_changed"),TEXT("Retained node order/identity changed during stack SDK."));
    UEdGraphNode* NewNode=G->Nodes.Num()>Before.Nodes.Num()?G->Nodes.Last().Get():nullptr;
    if(NewNode&&(!Live(NewNode)||!NewNode->IsIn(W.System.Get())||NewNode->GetOutermost()!=W.System->GetOutermost()||NewNode->GetGraph()!=G))return W.Fail(TEXT("niagara_scratch_stack_generated_node_owner_invalid"),TEXT("New stack node is outside the requested graph/System."));
    const TCHAR* NewClass=WasOverride?TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"):TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet");
    if(NewNode&&!NewNode->GetClass()->GetPathName().Equals(NewClass,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_scratch_stack_generated_node_class_invalid"),TEXT("Public stack SDK created an unexpected node class."));
    if(!WasOverride&&!NewNode)return W.Fail(TEXT("niagara_scratch_stack_link_node_missing"),TEXT("User binding did not append its exact getter node."));
    TSet<UEdGraphPin*> LivePins;
    for(UEdGraphNode* N:G->Nodes){if(!Live(N)||!W.Pin(N))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_scratch_stack_generated_pin_invalid"),TEXT("Post-SDK pin owner is invalid."));LivePins.Add(P);}}
    if(Returned&&!LivePins.Contains(Returned))return W.Fail(TEXT("niagara_scratch_stack_returned_pin_expired"),TEXT("Returned override pin was removed during SDK callback; stale pointer not read."));
    if(ExistingOverride&&!LivePins.Contains(ExistingOverride))return W.Fail(TEXT("niagara_scratch_stack_existing_override_expired"),TEXT("Captured existing override pin disappeared during stack SDK; stale pointer not dereferenced."));
    UEdGraphPin* FunctionInput=Function?MapPin(Function,EGPD_Input):nullptr;if(Function&&(!FunctionInput||FunctionInput->LinkedTo.Num()!=1))return W.Fail(TEXT("niagara_scratch_stack_stack_map_changed"),TEXT("Module map input no longer has exact upstream."));
    UEdGraphNode* OverrideNode=WasOverride?(Returned?Returned->GetOwningNodeUnchecked():nullptr):ExistingOverride->GetOwningNodeUnchecked();
    if(!OverrideNode||!OverrideNode->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_scratch_stack_override_owner_invalid"),TEXT("Exact override owner is not the public reflected ParameterMapSet class."));
    UEdGraphPin* OverrideInput=MapPin(OverrideNode,EGPD_Input);UEdGraphPin* OverrideOutput=MapPin(OverrideNode,EGPD_Output);
    if(!OverrideInput||!OverrideOutput||OverrideInput->LinkedTo.Num()!=1)return W.Fail(TEXT("niagara_scratch_stack_override_stack_invalid"),TEXT("Override ParameterMap input must have one captured upstream link."));
    UEdGraphPin* Prior=OverrideInput->LinkedTo[0];if(!LivePins.Contains(Prior))return W.Fail(TEXT("niagara_scratch_stack_upstream_unresolved"),TEXT("Override upstream is outside current exact graph pins."));
    UEdGraphPin* NewGetInput=nullptr;UEdGraphPin* NewGetOutput=nullptr;
    if(!WasOverride)
    {
        NewGetInput=MapPin(NewNode,EGPD_Input);int32 Semantic=0;
        for(auto* P:NewNode->Pins)if(P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P)){++Semantic;NewGetOutput=P;}
        if(Semantic!=1||!NewGetInput||!NewGetOutput||!LinkedUser||NewGetOutput->PinName!=LinkedUser->GetName()||UEdGraphSchema_Niagara::PinToTypeDefinition(NewGetOutput)!=LinkedUser->GetType()||NewGetInput->LinkedTo!=TArray<UEdGraphPin*>{Prior}||NewGetOutput->LinkedTo!=TArray<UEdGraphPin*>{ExistingOverride})return W.Fail(TEXT("niagara_scratch_stack_user_getter_shape_invalid"),TEXT("Generated getter does not match the exact requested typed User link."));
    }
    for(const auto& NE:Before.Nodes)
    {
        auto* N=NE.Node.Get();if(N->NodeGuid!=NE.Id||N->NodePosX!=NE.X||N->NodePosY!=NE.Y||N->GetGraph()!=G||!FunctionFieldsExact(NE,N))return W.Fail(TEXT("niagara_scratch_stack_retained_node_fields_changed"),TEXT("Retained node GUID/position/graph changed during stack SDK."));int32 AddedPins=N->Pins.Num()-NE.Pins.Num();const bool IsOldOverride=WasOverride&&N==OverrideNode;
        if(AddedPins!=(IsOldOverride&&!ExistingOverride?1:0))return W.Fail(TEXT("niagara_scratch_stack_unexpected_pin_delta"),TEXT("Retained node pin count differs from requested override insertion."));
        TArray<UEdGraphPin*> Remaining;for(auto* P:N->Pins)if(!(IsOldOverride&&!ExistingOverride&&P==Returned))Remaining.Add(P);
        if(Remaining.Num()!=NE.Pins.Num())return W.Fail(TEXT("niagara_scratch_stack_pin_plan_invalid"),TEXT("Override insertion did not yield exact retained pin set."));
        for(int32 J=0;J<NE.Pins.Num();++J)
        {
            const auto& PE=NE.Pins[J];auto* P=Remaining[J];if(P!=PE.Pointer||!PinHeaderSame(PE,P))return W.Fail(TEXT("niagara_scratch_stack_retained_pin_changed"),TEXT("Retained exact pin/type/name changed during SDK."));
            TArray<UEdGraphPin*> Expected=PE.Links;
            if(WasOverride&&NewNode)
            {
                UEdGraphPin* OldFunctionInput=FunctionInput;
                const FPinIdentity* OldInput=FindExpectedPin(W,OldFunctionInput);
                if(!OldInput||OldInput->Links.Num()!=1)return W.Fail(TEXT("niagara_scratch_stack_original_map_unresolved"),TEXT("Original module upstream no longer known."));
                UEdGraphPin* OldPrior=OldInput->Links[0];const auto* OldPriorExpected=FindExpectedPin(W,OldPrior);
                if(!OldPriorExpected)return false;
                if(P==OldPrior)Expected={OverrideInput};
                else if(P==OldFunctionInput){Expected.Remove(OldPrior);Expected.AddUnique(OverrideOutput);}
                else if(OldPriorExpected->Links.Contains(P)){Expected.Remove(OldPrior);Expected.AddUnique(OverrideOutput);}
            }
            if(!WasOverride){if(P==ExistingOverride)Expected={NewGetOutput};else if(P==Prior)Expected.AddUnique(NewGetInput);}
            if(P->LinkedTo!=Expected)return W.Fail(TEXT("niagara_scratch_stack_unselected_links_changed"),TEXT("Retained links differ from the single requested public stack transition."));
        }
    }
    if(WasOverride&&Returned&&(Returned->PinName!=Alias.GetParameterHandleString()||Returned->Direction!=EGPD_Input||UEdGraphSchema_Niagara::PinToTypeDefinition(Returned)!=Type||(!ExistingOverride&&Returned->LinkedTo.Num()!=0)||(ExistingOverride&&Returned!=ExistingOverride)))return W.Fail(TEXT("niagara_scratch_stack_override_pin_shape_invalid"),TEXT("Override pin is not exact typed empty requested alias."));
    if(WasOverride&&NewNode){for(auto* P:NewNode->Pins)if(P!=OverrideInput&&P!=OverrideOutput&&P!=Returned&&!IsNiagaraDynamicAddPin(P))return W.Fail(TEXT("niagara_scratch_stack_override_extra_pin"),TEXT("Generated override contains an unexpected semantic pin."));}
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
    if(!W.PinForWrite(Override)||Override->LinkedTo.Num()!=0||!PreflightStackMap(W,Function))return W.Fail(TEXT("niagara_scratch_stack_override_not_empty"),TEXT("Exact override must be empty before User link SDK."));
    auto* G=Cast<UNiagaraGraph>(Function->GetGraph());auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return false;const FGraphIdentity Before=*Expected;const FString MetadataBefore=GraphMetadataWithout(G,&User);
    UNiagaraScriptVariable* ExistingMetadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&!W.Own(ExistingMetadata))return false;
    if(!W.SDK(TEXT("SetLinkedParameterValueForFunctionInput"),true))return false;auto Row=W.MutationRow(TEXT("SetLinkedParameterValueForFunctionInput"));Row->SetStringField(TEXT("module_guid"),Function->NodeGuid.ToString());Row->SetStringField(TEXT("user_parameter"),User.GetName().ToString());
    {TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);TSet<FNiagaraVariableBase> Known;Known.Add(User);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Override,User,Known);Row->SetBoolField(TEXT("SDK_returned"),true);if(!CaptureTransitionResult(W,G,Before,Function,FNiagaraParameterHandle(),User.GetType(),nullptr,Override,&User,false,MetadataBefore))return false;}
    UNiagaraScriptVariable* Metadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&Metadata!=ExistingMetadata)return W.Fail(TEXT("niagara_scratch_stack_metadata_identity_changed"),TEXT("Selected existing metadata was replaced during binding SDK."));if(!Metadata||!W.Own(Metadata)||Metadata->DefaultMode!=ENiagaraDefaultMode::FailIfPreviouslyNotSet)return W.Fail(TEXT("niagara_scratch_stack_binding_metadata_readback_failed"),TEXT("Exact selected User metadata was not observed after public binding."));
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
 if(!Current||!Current->Admit(TEXT("HLSL_property_read_preflight"))||!Live(Node)||Node->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass())return FString();FProperty* P=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));if(!CastField<FStrProperty>(P)||P->ArrayDim!=1||EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(P),EPropertyAccessResultFlags::PermissionDenied)){Current->Fail(TEXT("niagara_scratch_stack_HLSL_property_denied"),TEXT("Public CustomHlsl property is not readable."));return FString();}FString Encoded;if(!Current->Step(TEXT("GetObjectProperties_CustomHlsl"),false,[&]{Encoded=UToolsetLibrary::GetObjectProperties(Node,{FName(TEXT("CustomHlsl"))});}))return FString();TSharedPtr<FJsonObject> O;FString Body;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Encoded),O)||!O||!O->TryGetStringField(TEXT("CustomHlsl"),Body)){Current->Fail(TEXT("niagara_scratch_stack_HLSL_read_failed"),TEXT("Public HLSL property readback unavailable."));return FString();}if(!Current->Budget(0,Body.Len()))return FString();return Body;
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
    if(Type==FNiagaraTypeDefinition::GetIntDef()){double X=0;if(Value->TryGetNumber(X)&&(!FMath::IsFinite(X)||X<MIN_int32||X>MAX_int32))return Current->Fail(TEXT("niagara_scratch_stack_numeric_range_unsupported"),TEXT("Numeric default is outside the SDK int32 conversion range."));}
    else if(Type==FNiagaraTypeDefinition::GetFloatDef()){double X=0;if(Value->TryGetNumber(X)&&!FloatSafe(X))return Current->Fail(TEXT("niagara_scratch_stack_numeric_range_unsupported"),TEXT("Numeric default is outside finite float range."));}
    else if(Value->Type==EJson::Array&&Type!=FNiagaraTypeDefinition::GetPositionDef()){for(const auto& V:Value->AsArray()){double X=0;if(V&&V->TryGetNumber(X)&&!FloatSafe(X))return Current->Fail(TEXT("niagara_scratch_stack_numeric_range_unsupported"),TEXT("Vector/color default is outside finite float range."));}}
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

int32 IndexParam(const TSharedPtr<FJsonObject>& P,const TCHAR* K,int32 Default=INDEX_NONE){double N=Default;if(!P->TryGetNumberField(K,N))return Default;if(!FMath::IsFinite(N)||N<MIN_int32||N>MAX_int32){Current->Fail(TEXT("niagara_scratch_stack_index_invalid"),FString(K)+TEXT(" requires a safe finite int32 conversion."));return Default;}int32 Out=Default;P->TryGetNumberField(K,Out);return Out;}
int32 IndexParam(FWork&,const TSharedPtr<FJsonObject>& P,const TCHAR* K,int32 Default=INDEX_NONE){return IndexParam(P,K,Default);}



bool FunctionFieldsExact(const FNodeIdentity& E,UEdGraphNode* N)
{if(!E.Function)return true;auto* F=Cast<UNiagaraNodeFunctionCall>(N);return F&&F->FunctionScript==E.FunctionScript.Get()&&F->SelectedScriptVersion==E.FunctionVersion&&F->FunctionScriptAssetObjectPath==E.FunctionAsset&&F->FunctionSpecifiers.OrderIndependentCompareEqual(E.Specifiers)&&FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&F->Signature,&E.FunctionSignature,0);}
bool ResolveUser(FWork& W,const FString& Requested,const FNiagaraTypeDefinition& Type,FNiagaraVariable& User,bool& Existed)
{
 if(!W.Admit(TEXT("resolve_user_parameter")))return false;User=FNiagaraVariable(Type,FName(*Requested));Existed=false;const auto& Store=W.System->GetExposedParameters();TArray<FNiagaraVariable> Full;Store.GetParameters(Full);if(Full.Num()>65536||!W.Budget(Full.Num()))return W.Fail(TEXT("niagara_scratch_stack_user_store_bound"),TEXT("Full typed declaration inventory exceeds bound."));int32 Count=0;for(const auto& V:Full){if(V.GetName()!=User.GetName())continue;if(V.GetType()!=Type)return W.Fail(TEXT("niagara_scratch_stack_user_type_conflict"),TEXT("Requested full User name has an existing incompatible type."));++Count;}
 FNiagaraVariable Probe=User;const bool Redirected=Store.RedirectUserVariable(Probe);if(Redirected&&(Probe.GetName()!=User.GetName()||Probe.GetType()!=User.GetType()))return W.Fail(TEXT("niagara_scratch_stack_user_alias_collision"),TEXT("Requested normalized full typed User name redirects to another declaration; no store/graph writer."));if(Count>1)return W.Fail(TEXT("niagara_scratch_stack_user_declaration_ambiguous"),TEXT("Requested full typed declaration appears more than once."));Existed=Count==1;if(Existed)return VerifyCanonicalUserForPath(W,User);return true;
}
bool PreflightMapValue(FWork& W,UEdGraphPin* P)
{
 if(!W.PinForWrite(P))return false;auto* N=P->GetOwningNodeUnchecked();if(!N->GetClass()->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive)||P->Direction!=EGPD_Input||IsNiagaraDynamicAddPin(P))return W.Fail(TEXT("niagara_scratch_stack_map_owner_unsupported"),TEXT("Value target must be a captured exact ParameterMapSet input."));auto* Source=MapPin(N,EGPD_Input);if(!Source||Source==P||Source->LinkedTo.Num()!=1||!FindExpectedPin(W,Source->LinkedTo[0])||Source->LinkedTo[0]->Direction!=EGPD_Output||Source->LinkedTo[0]->GetOwningNodeUnchecked()->GetGraph()!=N->GetGraph())return W.Fail(TEXT("niagara_scratch_stack_map_upstream_invalid"),TEXT("ParameterMapSet requires one captured exact upstream before public linked/DI SDK."));return true;
}
bool AdoptDITransition(FWork& W,UNiagaraGraph* G,const FGraphIdentity& Before,UEdGraphPin* Override,UClass* Class,UNiagaraDataInterface* DI,UNiagaraNodeInput*& CreatedNode)
{
 if(!W.Admit(TEXT("DI_transition_context")))return false;if(G->Schema!=Before.Schema.Get()||G->Nodes.Num()!=Before.Nodes.Num()+1||!GraphMetadataSignature(G).Equals(Before.Meta,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_scratch_stack_DI_graph_delta_invalid"),TEXT("DI creation changed more than one node or unrelated metadata."));for(int32 I=0;I<Before.Nodes.Num();++I)if(G->Nodes[I]!=Before.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_scratch_stack_retained_node_changed"),TEXT("DI creation replaced/reordered a retained node."));auto* New=G->Nodes.Last().Get();if(!Live(New)||New->GetClass()!=UNiagaraNodeInput::StaticClass()||New->GetGraph()!=G||!New->IsIn(W.System.Get())||New->GetPackage()!=W.System->GetPackage())return W.Fail(TEXT("niagara_scratch_stack_DI_node_owner_invalid"),TEXT("New DI node must be exact stock NodeInput in the requested System/graph/package."));TSet<UEdGraphPin*> Pins;for(UEdGraphNode* N:G->Nodes){if(!Live(N)||!W.Pin(N))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_scratch_stack_DI_pin_invalid"),TEXT("New graph pin membership is malformed."));Pins.Add(P);}}if(!Pins.Contains(Override))return W.Fail(TEXT("niagara_scratch_stack_override_expired"),TEXT("Returned exact override disappeared; stale pointer not dereferenced."));UEdGraphPin* Output=nullptr;int32 OutputCount=0;for(auto* P:New->Pins){if(P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P)){++OutputCount;Output=P;}else if(!P->LinkedTo.IsEmpty())return W.Fail(TEXT("niagara_scratch_stack_DI_extra_link"),TEXT("DI node contains an unexpected linked pin."));}if(OutputCount!=1||!Output)return W.Fail(TEXT("niagara_scratch_stack_DI_output_invalid"),TEXT("Expected one actual typed DI output."));
 for(const auto& E:Before.Nodes){auto* N=E.Node.Get();if(N->GetGraph()!=G||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||N->Pins.Num()!=E.Pins.Num()||!FunctionFieldsExact(E,N))return W.Fail(TEXT("niagara_scratch_stack_retained_node_changed"),TEXT("Retained node/function binding changed during DI SDK."));for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& PE=E.Pins[J];if(P!=PE.Pointer||!PinHeaderSame(PE,P))return W.Fail(TEXT("niagara_scratch_stack_retained_pin_changed"),TEXT("Retained pin/default/type changed during DI SDK."));const TArray<UEdGraphPin*> Expected=P==Override?TArray<UEdGraphPin*>{Output}:PE.Links;if(P->LinkedTo!=Expected)return W.Fail(TEXT("niagara_scratch_stack_DI_unselected_links_changed"),TEXT("Only exact override-to-new-DI edge was admitted."));}}
 auto* Input=static_cast<UNiagaraNodeInput*>(New);if(!Live(DI)||DI->GetClass()!=Class||DI->GetOuter()!=Input||DI->GetPackage()!=W.System->GetPackage()||ReflectedNodeDI(Input)!=DI||Input->Input.GetType()!=FNiagaraTypeDefinition(Class)||UEdGraphSchema_Niagara::PinToTypeDefinition(Output)!=FNiagaraTypeDefinition(Class)||Output->LinkedTo!=TArray<UEdGraphPin*>{Override}||Override->LinkedTo!=TArray<UEdGraphPin*>{Output})return W.Fail(TEXT("niagara_scratch_stack_DI_typed_readback_failed"),TEXT("Created exact DI/owner/public output does not match requested type and reciprocal edge."));if(!W.Own(Input)||!W.Own(DI))return false;CreatedNode=Input;auto* Source=W.Sources.FindByPredicate([&](const auto& S){return S.Graph.Get()==G;});if(!Source)return false;W.Graphs.RemoveAll([&](const auto& E){return E.Graph.Get()==G;});return W.CaptureGraph(Source->Source.Get());
}
UNiagaraDataInterface* CreateDI(FWork& W,UEdGraphPin* Override,UClass* Class,const FString& Name,bool RT)
{
 if(!PreflightMapValue(W,Override)||!Override->LinkedTo.IsEmpty()||!Class||!Class->IsChildOf(UNiagaraDataInterface::StaticClass())||!W.Pin(Class))return W.Fail(TEXT("niagara_scratch_stack_DI_precondition_failed"),TEXT("DI SDK needs an empty exact value override and public DI class.")),nullptr;auto* G=Cast<UNiagaraGraph>(Override->GetOwningNodeUnchecked()->GetGraph());const auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return nullptr;const FGraphIdentity Before=*Expected;UNiagaraDataInterface* DI=nullptr;UNiagaraNodeInput* Node=nullptr;if(!W.SDK(TEXT("SetDataInterfaceValueForFunctionInput"),true))return nullptr;auto Row=W.MutationRow(TEXT("SetDataInterfaceValueForFunctionInput"));Row->SetStringField(TEXT("input_alias"),Name);Row->SetStringField(TEXT("class"),Class->GetPathName());{TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);FNiagaraStackGraphUtilities::SetDataInterfaceValueForFunctionInput(*Override,Class,Name,DI);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),DI!=nullptr);W.R.bChanged|=DI!=nullptr;if(!AdoptDITransition(W,G,Before,Override,Class,DI,Node))return nullptr;}if(!W.After(TEXT("SetDataInterfaceValueForFunctionInput")))return nullptr;Row->SetStringField(TEXT("actual_DI_path"),DI->GetPathName());Row->SetStringField(TEXT("actual_input_node_path"),Node->GetPathName());FExpectedDI E;E.Node=Node;E.DI=DI;E.RT=RT;E.Input=Node->Input;E.Sort=Node->CallSortPriority;E.Usage=Node->Usage;if(RT){auto* D=Cast<UNiagaraDataInterfaceRenderTarget2D>(DI);E.User=D->RenderTargetUserParameter.Parameter;E.Inherit=D->bInheritUserParameterSettings;}else{auto* D=Cast<UNiagaraDataInterfaceTexture>(DI);E.User=D->TextureUserParameter.Parameter;E.Texture=D->Texture;}W.ExpectedDI.Add(E);return DI;
}
bool LinkMapUser(FWork& W,UEdGraphPin* Target,const FNiagaraVariable& User)
{
 if(!PreflightMapValue(W,Target)||!Target->LinkedTo.IsEmpty()||!VerifyCanonicalUserForPath(W,User))return false;auto* G=Cast<UNiagaraGraph>(Target->GetOwningNodeUnchecked()->GetGraph());auto* Expected=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!Expected)return false;const FGraphIdentity Before=*Expected;const FString Meta=GraphMetadataWithout(G,&User);UNiagaraScriptVariable* ExistingMetadata=G->GetAllMetaData().FindRef(User);if(ExistingMetadata&&!W.Own(ExistingMetadata))return false;if(!W.SDK(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"),true))return false;auto Row=W.MutationRow(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"));Row->SetStringField(TEXT("target_node"),Target->GetOwningNodeUnchecked()->NodeGuid.ToString());Row->SetStringField(TEXT("target_pin"),Target->PinId.ToString());Row->SetStringField(TEXT("full_user_parameter"),User.GetName().ToString());{TGuardValue<UNiagaraGraph*> Transition(W.TransitionGraph,G);TSet<FNiagaraVariableBase> Known;Known.Add(User);FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Target,User,Known);Row->SetBoolField(TEXT("SDK_returned"),true);W.R.bChanged=true;if(!CaptureTransitionResult(W,G,Before,nullptr,FNiagaraParameterHandle(),User.GetType(),nullptr,Target,&User,false,Meta))return false;}UNiagaraScriptVariable* Metadata=G->GetAllMetaData().FindRef(User);if((ExistingMetadata&&Metadata!=ExistingMetadata)||!Metadata||!W.Own(Metadata)||Metadata->DefaultMode!=ENiagaraDefaultMode::FailIfPreviouslyNotSet)return W.Fail(TEXT("niagara_scratch_stack_user_metadata_readback_failed"),TEXT("Selected metadata identity/default mode changed unexpectedly."));return W.After(TEXT("SetLinkedParameterValueForFunctionInput_Scratch"));
}
bool SetPinDefault(FWork& W,UEdGraphPin* P,const FString& Value)
{
 if(!W.PinForWrite(P))return false;const UEdGraphSchema* Schema=P->GetSchema();if(Schema&&Schema->GetClass()!=UEdGraphSchema_Niagara::StaticClass())return W.Fail(TEXT("niagara_scratch_stack_schema_unsupported"),TEXT("Only exact public Niagara default setter is supported."));if(Schema&&!W.Pin(Schema->GetClass()))return false;auto* E=FindExpectedPin(W,P);if(!W.Step(TEXT("ClearPinDefaultObjectAndText"),true,[&]{P->DefaultObject=nullptr;P->DefaultTextValue=FText::GetEmpty();E->DefaultObject=nullptr;E->DefaultText=FString();}))return false;if(!W.SDK(TEXT("Schema_TrySetDefaultValue"),true))return false;E->DefaultValue=Value;if(Schema)Schema->TrySetDefaultValue(*P,Value);else P->DefaultValue=Value;W.Prefix->SetStringField(TEXT("last_default_value_assigned"),Value);W.R.bChanged=true;return W.After(TEXT("Schema_TrySetDefaultValue"));
}


template<typename T>T* LoadResource(FWork& W,const FString& Path)
{
 if(Path.IsEmpty())return nullptr;if(Path.ToLower().Contains(TEXT("unrealmcp"))||Path.ToLower().Contains(TEXT("cubelessmcp"))||Path.ToLower().Contains(TEXT("toolsetregistry")))return W.Fail(TEXT("niagara_scratch_stack_dependency_rejected"),TEXT("MCP/Cubeless/Toolset resource dependency rejected.")),nullptr;const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);T* O=nullptr;if(!W.Step(TEXT("LoadObject_Resource"),false,[&]{O=LoadObject<T>(nullptr,*Expected);W.Prefix->SetBoolField(TEXT("resource_load_returned_nonnull"),O!=nullptr);}))return nullptr;if(!O||!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!W.Pin(O))return W.Fail(TEXT("niagara_scratch_stack_resource_load_failed"),TEXT("Exact requested resource could not be loaded.")),nullptr;return O;
}
bool VerifyResourceBinding(FWork& W,UNiagaraNodeFunctionCall* Function,const FNiagaraParameterHandle& Alias,const FNiagaraVariable& User,const TArray<FUserValue>& Before,bool New,UNiagaraDataInterface* DI,UObject* ExpectedObject)
{
 if(!W.Admit(TEXT("resource_binding_typed_readback"))||!VerifyUnrelatedUserValues(W,Before,User,New))return false;auto* Override=FindNiagaraStackFunctionInputOverridePin(Function,Alias);if(!ContinueRead())return false;if(!Override||!FindExpectedPin(W,Override)||Override->LinkedTo.Num()!=1||!FindExpectedPin(W,Override->LinkedTo[0]))return W.Fail(TEXT("niagara_scratch_stack_override_readback_failed"),TEXT("Exact module override-to-DI edge not observed after compile/save."));auto* Node=Cast<UNiagaraNodeInput>(Override->LinkedTo[0]->GetOwningNodeUnchecked());if(!Node||ReflectedNodeDI(Node)!=DI||DI->GetOuter()!=Node||W.System->GetExposedParameters().GetUObject(User).Get()!=ExpectedObject)return W.Fail(TEXT("niagara_scratch_stack_resource_readback_failed"),TEXT("Actual selected User object/DI node binding differs from requested typed target."));return W.Admit(TEXT("resource_binding_verified"));
}
bool TryParseNiagaraTypeDefinition(const FString& Value, FNiagaraTypeDefinition& OutType)
{
    FString Normalized = Value.TrimStartAndEnd().ToLower();
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));

    if (Normalized == TEXT("float") || Normalized == TEXT("scalar"))
    {
        OutType = FNiagaraTypeDefinition::GetFloatDef();
        return true;
    }
    if (Normalized == TEXT("int") || Normalized == TEXT("integer"))
    {
        OutType = FNiagaraTypeDefinition::GetIntDef();
        return true;
    }
    if (Normalized == TEXT("bool") || Normalized == TEXT("boolean"))
    {
        OutType = FNiagaraTypeDefinition::GetBoolDef();
        return true;
    }
    if (Normalized == TEXT("color") || Normalized == TEXT("linearcolor"))
    {
        OutType = FNiagaraTypeDefinition::GetColorDef();
        return true;
    }
    if (Normalized == TEXT("vec2") || Normalized == TEXT("vector2") || Normalized == TEXT("vector2d"))
    {
        OutType = FNiagaraTypeDefinition::GetVec2Def();
        return true;
    }
    if (Normalized == TEXT("vec3") || Normalized == TEXT("vector3") || Normalized == TEXT("vector"))
    {
        OutType = FNiagaraTypeDefinition::GetVec3Def();
        return true;
    }
    if (Normalized == TEXT("vec4") || Normalized == TEXT("vector4"))
    {
        OutType = FNiagaraTypeDefinition::GetVec4Def();
        return true;
    }
    if (Normalized == TEXT("position"))
    {
        OutType = FNiagaraTypeDefinition::GetPositionDef();
        return true;
    }

    if(Normalized==TEXT("texture")||Normalized==TEXT("texture2d")||Normalized==TEXT("texturedi")||Normalized==TEXT("niagaradatainterfacetexture")){OutType=FNiagaraTypeDefinition(UNiagaraDataInterfaceTexture::StaticClass());return true;}
    if(Value.Contains(TEXT("/Script/"))){UClass* C=nullptr;if(!Current||!Current->Step(TEXT("LoadClass_InputType"),false,[&]{C=LoadObject<UClass>(nullptr,*Value.TrimStartAndEnd());}))return false;if(C&&C->IsChildOf(UNiagaraDataInterface::StaticClass())&&Current->Pin(C)){OutType=FNiagaraTypeDefinition(C);return true;}}
    return false;
}

bool TryParseNiagaraScriptUsage(const FString& Value, ENiagaraScriptUsage& OutUsage)
{
    FString Normalized = Value.TrimStartAndEnd().ToLower();
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));

    if (Normalized == TEXT("systemspawn") || Normalized == TEXT("systemspawnscript"))
    {
        OutUsage = ENiagaraScriptUsage::SystemSpawnScript;
        return true;
    }
    if (Normalized == TEXT("systemupdate") || Normalized == TEXT("systemupdatescript"))
    {
        OutUsage = ENiagaraScriptUsage::SystemUpdateScript;
        return true;
    }
    if (Normalized == TEXT("emitterspawn") || Normalized == TEXT("emitterspawnscript"))
    {
        OutUsage = ENiagaraScriptUsage::EmitterSpawnScript;
        return true;
    }
    if (Normalized == TEXT("emitterupdate") || Normalized == TEXT("emitterupdatescript"))
    {
        OutUsage = ENiagaraScriptUsage::EmitterUpdateScript;
        return true;
    }
    if (Normalized == TEXT("particlespawn") || Normalized == TEXT("particlespawnscript"))
    {
        OutUsage = ENiagaraScriptUsage::ParticleSpawnScript;
        return true;
    }
    if (Normalized == TEXT("particlespawninterpolated") || Normalized == TEXT("particlespawnscriptinterpolated"))
    {
        OutUsage = ENiagaraScriptUsage::ParticleSpawnScriptInterpolated;
        return true;
    }
    if (Normalized == TEXT("particleupdate") || Normalized == TEXT("particleupdatescript"))
    {
        OutUsage = ENiagaraScriptUsage::ParticleUpdateScript;
        return true;
    }
    if (Normalized == TEXT("particleevent") || Normalized == TEXT("particleeventscript"))
    {
        OutUsage = ENiagaraScriptUsage::ParticleEventScript;
        return true;
    }
    if (Normalized == TEXT("particlesimulationstage") || Normalized == TEXT("particlesimulationstagescript") || Normalized == TEXT("simulationstage"))
    {
        OutUsage = ENiagaraScriptUsage::ParticleSimulationStageScript;
        return true;
    }
    return false;
}
FString NiagaraScriptUsageName(ENiagaraScriptUsage Usage)
{
    if (const UEnum* Enum = StaticEnum<ENiagaraScriptUsage>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Usage));
    }
    return FString::FromInt(static_cast<int32>(Usage));
}
bool IsEmitterStackUsage(ENiagaraScriptUsage Usage)
{
    return Usage == ENiagaraScriptUsage::EmitterSpawnScript ||
        Usage == ENiagaraScriptUsage::EmitterUpdateScript ||
        Usage == ENiagaraScriptUsage::ParticleSpawnScript ||
        Usage == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated ||
        Usage == ENiagaraScriptUsage::ParticleUpdateScript ||
        Usage == ENiagaraScriptUsage::ParticleEventScript ||
        Usage == ENiagaraScriptUsage::ParticleSimulationStageScript;
}
TSharedPtr<FJsonValue> NiagaraVariableDataToJsonValue(const FNiagaraVariable& Variable)
{
    if (!Variable.IsDataAllocated())
    {
        return MakeShared<FJsonValueNull>();
    }

    const FNiagaraTypeDefinition& TypeDef = Variable.GetType();
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
FString FormatNiagaraPinNumber(double Value)
{
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

struct FScratchSelection{UNiagaraSystem* System=nullptr;UNiagaraScript* Script=nullptr;UNiagaraScriptSource* Source=nullptr;UNiagaraGraph* Graph=nullptr;FString OwnerKind,OwnerName;int32 EmitterIndex=INDEX_NONE,ScriptIndex=INDEX_NONE;};
bool ResolveScratch(FWork& W,const TSharedPtr<FJsonObject>& Params,const FString& SystemPath,FScratchSelection& Selection,const FString& DefaultName=FString(),bool RequireModule=true)
{
    FString ScratchPadScriptPath;
    FString ScratchPadOwnerKind = TEXT("system");
    FString ScratchPadName=DefaultName;
    int32 ScratchPadScriptIndex = INDEX_NONE;
    int32 ScratchPadEmitterIndex = INDEX_NONE;
    FString ScratchPadEmitterName;
    Params->TryGetStringField(TEXT("scratch_pad_script_path"), ScratchPadScriptPath);
    Params->TryGetStringField(TEXT("scratch_pad_owner_kind"), ScratchPadOwnerKind);
    Params->TryGetStringField(TEXT("scratch_pad_name"), ScratchPadName);
    Params->TryGetStringField(TEXT("scratch_pad_script_name"), ScratchPadName);
    ScratchPadScriptIndex=IndexParam(W,Params,TEXT("scratch_pad_script_index"));
    ScratchPadEmitterIndex=IndexParam(W,Params,TEXT("scratch_pad_emitter_index"));
    Params->TryGetStringField(TEXT("scratch_pad_emitter_name"), ScratchPadEmitterName);
    if (ScratchPadScriptPath.IsEmpty() && ScratchPadName.IsEmpty() && ScratchPadScriptIndex == INDEX_NONE)
    {
        {W.Error(TEXT("Missing Scratch Pad selector. Provide 'scratch_pad_script_path', 'scratch_pad_name', or 'scratch_pad_script_index'."));return false;}
    }

    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        {W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));return false;}
    }

    UNiagaraScript* RequestedScript = nullptr;
    if (!ScratchPadScriptPath.IsEmpty())
    {
        RequestedScript = LoadResource<UNiagaraScript>(W,ScratchPadScriptPath);
        if (!RequestedScript)
        {
            {W.Error(FString::Printf(TEXT("Failed to load Scratch Pad script: %s"), *ScratchPadScriptPath));return false;}
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
        {W.Error(TEXT("Unsupported scratch_pad_owner_kind. Use 'system' or 'emitter'."));return false;}
    }

    if (!ScratchPadScript)
    {
        {W.Error(TEXT("No matching target-local Scratch Pad script was found"));return false;}
    }
    if(!W.Admit(TEXT("selected_scratch_context"))){W.Error(W.R.Error);return false;}
    if (RequireModule&&ScratchPadScript->GetUsage() != ENiagaraScriptUsage::Module)
    {
        {W.Error(
            FString::Printf(TEXT("Only Module Scratch Pad scripts can receive module inputs. Found: %s"), *NiagaraScriptUsageName(ScratchPadScript->GetUsage())));return false;}
    }

    UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(ScratchPadScript->GetLatestSource());
    UNiagaraGraph* Graph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
    if (!Graph)
    {
        {W.Error(FString::Printf(TEXT("Scratch Pad script has no editable graph: %s"), *ScratchPadScript->GetPathName()));return false;}
    }


    if(!W.Own(ScratchPadScript)||!W.Own(ScriptSource)||!W.CaptureGraph(ScriptSource)||!W.GraphForWrite(Graph))return false;
    Selection={System,ScratchPadScript,ScriptSource,Graph,ResolvedScratchPadOwnerKind,ResolvedScratchPadOwnerName,ResolvedScratchPadEmitterIndex,ResolvedScratchPadScriptIndex};return true;
}

// Domain-local expected graph transitions; unchanged captured nodes/pins/metadata remain exact.
struct FMetaObservation{FNiagaraVariableBase Variable;TWeakObjectPtr<UNiagaraScriptVariable> Object;FString State;};
bool MetadataSnapshot(FWork& W,UNiagaraGraph* G,TArray<FMetaObservation>& Rows)
{
 if(G->GetAllMetaData().Num()>65536||!W.Budget(G->GetAllMetaData().Num()))return W.Fail(TEXT("niagara_scratch_stack_metadata_bound"),TEXT("Metadata inventory exceeds bound."));
 for(const auto& Pair:G->GetAllMetaData()){UNiagaraScriptVariable* V=Pair.Value.Get();if(!Live(V)||V->Variable!=Pair.Key||!W.Pin(V))return W.Fail(TEXT("niagara_scratch_stack_metadata_invalid"),TEXT("Captured typed metadata must be live and exact."));const auto& Bytes=V->GetDefaultValueVariant();const int32 N=Bytes.GetNumBytes();if(N<0||N>65536||!W.Budget(N))return false;FString ValueHash;if(V->GetDefaultValueData()&&N)ValueHash=FMD5::HashBytes(V->GetDefaultValueData(),N);FString State=V->GetPathName()+TEXT("|")+V->Metadata.GetVariableGuid().ToString()+TEXT("|")+V->GetChangeId().ToString()+TEXT("|")+FString::FromInt(static_cast<int32>(V->DefaultMode))+TEXT("|")+V->DefaultBinding.GetName().ToString()+TEXT("|")+ValueHash;Rows.Add({Pair.Key,V,State});}return true;
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
 if(!W.Admit(TEXT("graph_edit_context")))return false;if(G->Schema!=Old.Schema.Get()||G->Nodes.Num()!=Old.Nodes.Num()+(Plan.AddedNode?1:0)||G->Nodes.Num()>16384)return W.Fail(TEXT("niagara_scratch_stack_unexpected_graph_delta"),TEXT("Unexpected schema/node count during dedicated edit."));
 TSet<UEdGraphPin*> LivePins;for(int32 I=0;I<G->Nodes.Num();++I){UEdGraphNode* N=G->Nodes[I];if(!Live(N)||N->GetGraph()!=G||!N->IsIn(W.System.Get())||N->GetPackage()!=W.System->GetPackage()||N->Pins.Num()>65536)return W.Fail(TEXT("niagara_scratch_stack_generated_node_invalid"),TEXT("Current node ownership/pin bound invalid."));if(I<Old.Nodes.Num()&&N!=Old.Nodes[I].Node.Get())return W.Fail(TEXT("niagara_scratch_stack_retained_node_replaced"),TEXT("Retained physical node order changed."));if(I==Old.Nodes.Num()&&N!=Plan.AddedNode)return false;if(!W.Pin(N)||!W.Budget(N->Pins.Num()))return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return W.Fail(TEXT("niagara_scratch_stack_pin_owner_invalid"),TEXT("Malformed current pin owner."));LivePins.Add(P);}}
 for(const auto& E:Old.Nodes){auto* N=E.Node.Get();if(!Plan.NodeMutable(N)&&(N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||!FunctionFieldsExact(E,N)||N->Pins.Num()!=E.Pins.Num()))return W.Fail(TEXT("niagara_scratch_stack_retained_node_fields_changed"),TEXT("Unselected node fields or pin count changed."));if(Plan.NodeMutable(N)){for(const auto& P:E.Pins)if(!LivePins.Contains(P.Pointer))return W.Fail(TEXT("niagara_scratch_stack_mutable_pin_retired"),TEXT("Allowed node edit unexpectedly removed an existing pin."));}
  for(int32 J=0;J<E.Pins.Num();++J){const auto& P=E.Pins[J];if(!LivePins.Contains(P.Pointer)||(!Plan.NodeMutable(N)&&N->Pins[J]!=P.Pointer))return W.Fail(TEXT("niagara_scratch_stack_retained_pin_replaced"),TEXT("Old exact pin retired/reordered."));if((P.PersistentId.IsValid()||!Plan.InitialDynamicPins.Contains(P.Pointer))&&P.Pointer->PersistentGuid!=P.PersistentId)return W.Fail(TEXT("niagara_scratch_stack_pair_guid_changed"),TEXT("Existing public pair PersistentGuid changed during callback."));if(P.Pointer->bNotConnectable!=P.NotConnectable||P.Pointer->bDefaultValueIsReadOnly!=P.DefaultReadOnly)return W.Fail(TEXT("niagara_scratch_stack_pair_flags_changed"),TEXT("Existing public default-pair flags changed."));if(!Plan.MutablePins.Contains(P.Pointer)&&!PinHeaderSame(P,P.Pointer))return W.Fail(TEXT("niagara_scratch_stack_unselected_pin_changed"),TEXT("Unselected pin header/default changed."));const auto* Expected=Plan.Links.Find(P.Pointer);if(P.Pointer->LinkedTo!=(Expected?*Expected:P.Links))return W.Fail(TEXT("niagara_scratch_stack_unselected_link_changed"),TEXT("Links differ from the admitted exact edge plan."));}}
 for(auto* P:LivePins)for(auto* L:P->LinkedTo)if(!L||!LivePins.Contains(L)||!L->LinkedTo.Contains(P))return W.Fail(TEXT("niagara_scratch_stack_foreign_link"),TEXT("Every resulting link must be same-graph reciprocal and live."));
 TArray<FMetaObservation> Now;if(!MetadataSnapshot(W,G,Now))return false;int32 Added=0;for(const auto& N:Now){const auto* Prior=OldMeta.FindByPredicate([&](const auto& M){return M.Variable==N.Variable;});if(!Prior){++Added;if(!Plan.MetaMutable(N.Variable)&&!(Plan.AllowOneAddedMetadata&&N.Variable.GetType()==Plan.AddedMetadataType))return W.Fail(TEXT("niagara_scratch_stack_extra_metadata"),TEXT("Unexpected typed metadata created."));}else if(!Plan.MetaMutable(N.Variable)&&(Prior->Object!=N.Object||!Prior->State.Equals(N.State,ESearchCase::CaseSensitive)))return W.Fail(TEXT("niagara_scratch_stack_unselected_metadata_changed"),TEXT("Unselected metadata/default changed."));}
 for(const auto& M:OldMeta)if(!Plan.MetaMutable(M.Variable)&&!Now.ContainsByPredicate([&](const auto& N){return N.Variable==M.Variable;}))return W.Fail(TEXT("niagara_scratch_stack_metadata_retired"),TEXT("Unselected typed metadata removed."));if(Added>(Plan.AllowOneAddedMetadata?1:Plan.MutableMetadata.Num()))return W.Fail(TEXT("niagara_scratch_stack_metadata_delta"),TEXT("Unexpected metadata insertion count."));
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
UEdGraphNode* CreateNode(FWork& W,UNiagaraGraph* G,UClass* Class,int32 X,int32 Y,const FNiagaraFunctionSignature* Signature=nullptr)
{
 if(!Class||!Class->IsChildOf(UEdGraphNode::StaticClass())||!W.Pin(Class)||!W.GraphForWrite(G))return nullptr;UEdGraphNode* N=nullptr;if(!W.Step(TEXT("NewObject_GraphNode"),true,[&]{N=NewObject<UEdGraphNode>(G,Class,NAME_None,RF_Transactional);W.Prefix->SetBoolField(TEXT("new_node_returned_nonnull"),N!=nullptr);W.R.bChanged|=N!=nullptr;}))return nullptr;if(!Live(N)||N->GetClass()!=Class||N->GetOuter()!=G||!W.Own(N))return nullptr;auto Row=W.MutationRow(TEXT("created_node_identity"));Row->SetStringField(TEXT("path"),N->GetPathName());Row->SetStringField(TEXT("class"),Class->GetPathName());
 N->NodePosX=X;N->NodePosY=Y;N->CreateNewGuid();if(Signature){auto* Custom=Cast<UNiagaraNodeCustomHlsl>(N);if(!Custom)return nullptr;Custom->ScriptUsage=ENiagaraScriptUsage::DynamicInput;Custom->Signature=*Signature;}FGraphEditPlan Add;Add.AddedNode=N;if(!GraphEdit(W,G,TEXT("Graph_AddNode"),Add,[&]{G->AddNode(N,false,false);}))return nullptr;FGraphEditPlan Init;Init.MutableNode=N;if(!GraphEdit(W,G,TEXT("Node_PostPlacedNewNode"),Init,[&]{N->PostPlacedNewNode();}))return nullptr;if(!GraphEdit(W,G,TEXT("Node_AllocateDefaultPins"),Init,[&]{N->AllocateDefaultPins();}))return nullptr;if(Signature&&N->GetClass()==UNiagaraNodeCustomHlsl::StaticClass())W.FreshUnlinkedHlslNodes.Add(static_cast<UNiagaraNodeCustomHlsl*>(N));return N;
}
bool SchemaLink(FWork& W,UNiagaraGraph* G,UEdGraphPin* Out,UEdGraphPin* In,FGraphEditPlan& Plan,bool Replace=false)
{
 if(!W.PinForWrite(Out)||!W.PinForWrite(In)||Out->Direction!=EGPD_Output||In->Direction!=EGPD_Input||Out->GetOwningNodeUnchecked()->GetGraph()!=G||In->GetOwningNodeUnchecked()->GetGraph()!=G)return false;const UEdGraphSchema_Niagara* Schema=Cast<UEdGraphSchema_Niagara>(G->GetSchema());if(!Schema||Schema->GetClass()!=UEdGraphSchema_Niagara::StaticClass()||!W.Pin(Schema->GetClass()))return W.Fail(TEXT("niagara_scratch_stack_schema_unsupported"),TEXT("Exact stock Niagara schema required."));FPinConnectionResponse Response(CONNECT_RESPONSE_DISALLOW,TEXT(""));if(!W.Step(TEXT("CanCreateConnection"),false,[&]{Response=Schema->CanCreateConnection(Out,In);}))return false;if(Response.Response!=CONNECT_RESPONSE_MAKE&&!(Replace&&Response.Response==CONNECT_RESPONSE_BREAK_OTHERS_B))return W.Fail(TEXT("niagara_scratch_stack_connection_unsupported"),TEXT("Only direct MAKE or selected input replacement is supported."));
 Plan.Links.Add(Out,Out->LinkedTo);Plan.Links[Out].AddUnique(In);Plan.Links.Add(In,TArray<UEdGraphPin*>{Out});if(Replace)for(auto* Previous:In->LinkedTo)if(Previous!=Out){Plan.Links.Add(Previous,Previous->LinkedTo);Plan.Links[Previous].Remove(In);}bool Linked=false;if(!GraphEdit(W,G,TEXT("TryCreateConnection"),Plan,[&]{Linked=Schema->TryCreateConnection(Out,In);W.Prefix->SetBoolField(TEXT("schema_connection_returned"),Linked);}))return false;return Linked&&Out->LinkedTo.Contains(In)&&In->LinkedTo.Contains(Out)||W.Fail(TEXT("niagara_scratch_stack_connection_failed"),TEXT("Schema did not retain exact reciprocal edge."));
}
bool SetMetadataDefault(FWork& W,UNiagaraGraph* G,const FNiagaraVariable& Variable,UNiagaraScriptVariable*& Metadata)
{
 if(!W.Admit(TEXT("metadata_default_preflight")))return false;Metadata=G->GetAllMetaData().FindRef(Variable);if(!Live(Metadata)||Metadata->Variable!=Variable||!Metadata->Metadata.GetVariableGuid().IsValid()||!W.Own(Metadata))return W.Fail(TEXT("niagara_scratch_stack_metadata_missing"),TEXT("Expected engine-created typed metadata missing."));if(!W.Modify(Metadata))return false;FGraphEditPlan Plan;Plan.MutableMetadata.Add(Variable);if(!GraphEdit(W,G,TEXT("SetMetadataDefault"),Plan,[&]{Metadata->DefaultMode=ENiagaraDefaultMode::Value;Metadata->DefaultBinding=FNiagaraScriptVariableBinding();if(Variable.IsDataAllocated())Metadata->SetDefaultValueData(Variable.GetData());Metadata->UpdateChangeId();W.R.bChanged=true;}))return false;const int32 N=Variable.GetSizeInBytes();return Metadata->DefaultMode==ENiagaraDefaultMode::Value&&!Metadata->DefaultBinding.IsValid()&&(!Variable.IsDataAllocated()||(Metadata->GetDefaultValueData()&&Metadata->GetDefaultValueVariant().GetNumBytes()==N&&FMemory::Memcmp(Metadata->GetDefaultValueData(),Variable.GetData(),N)==0))||W.Fail(TEXT("niagara_scratch_stack_metadata_readback_failed"),TEXT("Typed metadata default readback failed."));
}
struct FModuleRead{UEdGraphNode* Node=nullptr;UEdGraphPin* Output=nullptr;UEdGraphPin* Default=nullptr;UNiagaraScriptVariable* Metadata=nullptr;};
bool SpecializeModuleRead(FWork& W,UNiagaraGraph* G,UEdGraphNode* Get,UEdGraphPin* Consumer,const FNiagaraVariable& Variable,const FString& DefaultText,FModuleRead& Read)
{
 auto* Add=DynamicOutput(W,Get);if(!Add||!FindExpectedPin(W,Consumer)||PinTypeForPath(Consumer)!=Variable.GetType())return W.Fail(TEXT("niagara_scratch_stack_dynamic_pair_preflight"),TEXT("One unused DynamicAdd output and exact typed consumer required."));const FGuid Id=Add->PinId;TSet<UEdGraphPin*> Old;for(auto* P:Get->Pins)Old.Add(P);FGraphEditPlan Connect;Connect.MutableNode=Get;Connect.MutablePins.Add(Add);Connect.InitialDynamicPins.Add(Add);Connect.AllowOneAddedMetadata=true;Connect.AddedMetadataType=Variable.GetType();if(!SchemaLink(W,G,Add,Consumer,Connect,true))return false;
 if(!FindExpectedPin(W,Add)||Add->PinId!=Id||PinTypeForPath(Add)!=Variable.GetType()||!Add->PersistentGuid.IsValid()||IsNiagaraDynamicAddPin(Add))return W.Fail(TEXT("niagara_scratch_stack_pair_specialization_failed"),TEXT("Engine callback did not preserve and type-specialize the exact Add output."));UEdGraphPin* Default=nullptr;for(auto* P:Get->Pins)if(!Old.Contains(P)&&P->Direction==EGPD_Input&&PinTypeForPath(P)==Variable.GetType()){if(Default)return W.Fail(TEXT("niagara_scratch_stack_pair_ambiguous"),TEXT("Multiple engine-generated default inputs."));Default=P;}if(!Default||!Default->PersistentGuid.IsValid()||!Default->bNotConnectable||!Default->bDefaultValueIsReadOnly||!Default->LinkedTo.IsEmpty())return W.Fail(TEXT("niagara_scratch_stack_pair_unobservable"),TEXT("Engine-generated public default input shape invalid."));
 if(Add->PinName!=Variable.GetName()){const FNiagaraVariableBase OldVariable(Variable.GetType(),Add->PinName);FGraphEditPlan Rename;Rename.MutableNode=Get;Rename.MutablePins.Add(Add);Rename.MutablePins.Add(Default);Rename.MutableMetadata={OldVariable,Variable};bool Renamed=false;auto* Dynamic=Cast<UNiagaraNode>(Get);if(!GraphEdit(W,G,TEXT("CommitEditablePinName"),Rename,[&]{Renamed=Dynamic->CommitEditablePinName(FText::AsCultureInvariant(Variable.GetName().ToString()),Add,false);}))return false;if(!Renamed||!FindExpectedPin(W,Add)||Add->PinName!=Variable.GetName())return W.Fail(TEXT("niagara_scratch_stack_pin_name_commit_failed"),TEXT("Public rename did not retain requested Module name/identity."));}
 if(!SetMetadataDefault(W,G,Variable,Read.Metadata))return false;FGraphEditPlan SetDefault;SetDefault.MutablePins.Add(Default);if(!GraphEdit(W,G,TEXT("SetGeneratedDefaultPin"),SetDefault,[&]{Default->DefaultValue=DefaultText;Default->AutogeneratedDefaultValue=DefaultText;Default->DefaultObject=nullptr;Default->DefaultTextValue=FText();W.R.bChanged=true;}))return false;Read.Node=Get;Read.Output=Add;Read.Default=Default;return true;
}

bool RefreshReference(FWork& W,UNiagaraNodeFunctionCall* Call,const FNiagaraVariableBase& Input,bool& Refreshed)
{
 auto* G=Cast<UNiagaraGraph>(Call->GetGraph());if(!W.GraphForWrite(G)||!W.Modify(Call))return false;const FGraphIdentity Old=*W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});const auto* OldCall=Old.Nodes.FindByPredicate([&](const auto& E){return E.Node.Get()==Call;});if(!OldCall||!W.SDK(TEXT("RefreshFromExternalChanges"),true))return false;auto Row=W.MutationRow(TEXT("RefreshFromExternalChanges"));Row->SetStringField(TEXT("node_path"),Call->GetPathName());Row->SetStringField(TEXT("node_guid"),Call->NodeGuid.ToString());
 {TGuardValue<UNiagaraGraph*> Mutable(W.TransitionGraph,G);Refreshed=Call->RefreshFromExternalChanges();Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("pins_refreshed"),Refreshed);if(!W.Admit(TEXT("after_refresh_context")))return false;if(G->Nodes.Num()!=Old.Nodes.Num()||G->Schema!=Old.Schema.Get())return W.Fail(TEXT("niagara_scratch_stack_reference_node_delta"),TEXT("Signature refresh changed graph nodes/schema."));TSet<UEdGraphPin*> LivePins;for(int32 I=0;I<Old.Nodes.Num();++I){auto* N=G->Nodes[I].Get();if(N!=Old.Nodes[I].Node.Get()||!Live(N)||N->Pins.Num()>65536)return false;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N)return false;LivePins.Add(P);}}
 if(Call->NodeGuid!=OldCall->Id||Call->NodePosX!=OldCall->X||Call->NodePosY!=OldCall->Y||!FunctionFieldsExact(*OldCall,Call))return W.Fail(TEXT("niagara_scratch_stack_reference_identity_changed"),TEXT("Refresh changed selected reference script/version/signature identity."));TMap<UEdGraphPin*,UEdGraphPin*> Replaced;TSet<UEdGraphPin*> Used;for(const auto& E:OldCall->Pins){UEdGraphPin* Match=nullptr;for(auto* P:Call->Pins)if(!Used.Contains(P)&&P->PinName==E.Name&&P->Direction==E.Direction&&P->PinType==E.Type){if(Match)return W.Fail(TEXT("niagara_scratch_stack_reference_pin_ambiguous"),TEXT("Reconstructed old reference pin is ambiguous."));Match=P;}if(!Match||!PinHeaderSame(E,Match))return W.Fail(TEXT("niagara_scratch_stack_reference_pin_changed"),TEXT("Retained reference pin/default/GUID shape changed beyond requested input addition."));Used.Add(Match);Replaced.Add(E.Pointer,Match);}
 int32 Added=0;for(auto* P:Call->Pins)if(!Used.Contains(P)){++Added;FNiagaraVariable Candidate(PinTypeForPath(P),P->PinName);if(P->Direction!=EGPD_Input||Candidate.GetType()!=Input.GetType()||!NiagaraModuleInputNameMatches(Candidate,Input.GetName().ToString())||!P->LinkedTo.IsEmpty())return W.Fail(TEXT("niagara_scratch_stack_reference_extra_pin"),TEXT("Refresh added an unrelated reference pin."));}if(Added>1)return false;
 for(const auto& NE:Old.Nodes){auto* N=NE.Node.Get();if(N!=Call&&(N->Pins.Num()!=NE.Pins.Num()||N->NodeGuid!=NE.Id||N->NodePosX!=NE.X||N->NodePosY!=NE.Y||!FunctionFieldsExact(NE,N)))return false;for(int32 J=0;J<NE.Pins.Num();++J){const auto& P=NE.Pins[J];UEdGraphPin* Now=N==Call?Replaced.FindChecked(P.Pointer):N->Pins[J];if(!LivePins.Contains(Now)||(N!=Call&&(Now!=P.Pointer||!PinHeaderSame(P,Now))))return false;TArray<UEdGraphPin*> Expected;for(auto* L:P.Links){auto** New=Replaced.Find(L);Expected.Add(New?*New:L);}if(Now->LinkedTo!=Expected)return W.Fail(TEXT("niagara_scratch_stack_reference_links_changed"),TEXT("Refresh did not preserve reciprocal external links."));}}if(!GraphMetadataSignature(G).Equals(Old.Meta,ESearchCase::CaseSensitive))return false;auto* S=W.Sources.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});if(!S)return false;auto Source=S->Source;W.Graphs.RemoveAll([&](const auto& E){return E.Graph.Get()==G;});if(!W.CaptureGraph(Source.Get()))return false;}
 if(!W.After(TEXT("RefreshFromExternalChanges")))return false;return W.Step(TEXT("Reference_MarkSynchronization"),true,[&]{Call->MarkNodeRequiresSynchronization(TEXT("Cubeless: Scratch Pad module input signature refreshed"),true);});
}
bool RefreshedCalls(FWork& W,UNiagaraScript* Script,const FNiagaraVariableBase& Input,int32& Referencing,int32& Refreshed)
{
 TArray<UNiagaraNodeFunctionCall*> Calls;const auto Handles=W.System->GetEmitterHandles();for(const auto& H:Handles){auto* Data=H.GetEmitterData();if(!Data)continue;for(auto* C:CollectSortedNiagaraFunctionCalls(Data->GraphSource))if(C&&C->FunctionScript==Script)Calls.Add(C);if(!ContinueRead())return false;}
 if(!W.Budget(Calls.Num()))return false;for(auto* C:Calls){++Referencing;bool Changed=false;if(!RefreshReference(W,C,Input,Changed))return false;Refreshed+=Changed?1:0;}return true;
}
bool GRDefaultString(FWork& W,const FNiagaraVariable& Variable,FString& Text)
{const auto* Schema=GetDefault<UEdGraphSchema_Niagara>();bool Ok=false;if(!Schema||!W.Pin(Schema->GetClass()))return false;return W.Step(TEXT("TryGetPinDefaultValueFromNiagaraVariable"),false,[&]{Ok=Schema->TryGetPinDefaultValueFromNiagaraVariable(Variable,Text);})&&Ok;}

FString NiagaraObjectPathOrEmpty(const UObject* O){return O?O->GetPathName():FString();}
void AddUniqueString(TArray<FString>& Values, const FString& Value)
{
    if (!Value.IsEmpty() && !Values.Contains(Value))
    {
        Values.Add(Value);
    }
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

bool CapturedPinForJson(const UEdGraphPin* Pin)
{
 if(!Pin)return true;if(!ContinueRead())return false;for(const auto& G:Current->Graphs)for(const auto& N:G.Nodes)for(const auto& P:N.Pins)if(P.Pointer==Pin)return true;return Current->Fail(TEXT("niagara_scratch_stack_untracked_pin_projection"),TEXT("Pin projection requires captured current membership before dereference."));
}
TSharedPtr<FJsonObject> DetailedPinRefToJsonObject(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (!Pin||!CapturedPinForJson(Pin))
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
TSharedPtr<FJsonObject> LinkedPinToJsonObject(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (!Pin||!CapturedPinForJson(Pin))
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
    if (!Pin || !bIncludeLinkedSources || !CapturedPinForJson(Pin))
    {
        return Values;
    }

    const TArray<UEdGraphPin*> LinkedSnapshot=Pin->LinkedTo;
    for (const UEdGraphPin* LinkedPin : LinkedSnapshot)
    {
        if(!CapturedPinForJson(LinkedPin))return Values;
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
TSharedPtr<FJsonObject> BuildFunctionCallJson(const UNiagaraNodeFunctionCall* FunctionCall, int32 NodeIndex, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!FunctionCall)
    {
        return NodeObject;
    }

    if(!ContinueRead())return NodeObject;
    FString FunctionName = FunctionNameForPath(FunctionCall);if(!ContinueRead())return NodeObject;
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
    for (const auto& Variable : FunctionCall->Signature.Outputs)
    {
        SearchText += TEXT(" ");
        SearchText += Variable.GetName().ToString();
    }
    NodeObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(SearchText)));

    if (bIncludePins)
    {
        TArray<UEdGraphPin*> InputPins;
        TArray<UEdGraphPin*> OutputPins;
        for(auto* P:FunctionCall->Pins){if(P->Direction==EGPD_Input)InputPins.Add(P);else OutputPins.Add(P);}
        NodeObject->SetArrayField(TEXT("input_pins"), DetailedPinsToJson(InputPins,false));
        NodeObject->SetArrayField(TEXT("output_pins"), DetailedPinsToJson(OutputPins,false));
    }

    return NodeObject;
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

    if(!ContinueRead())return NodeObject;FText Title,Tooltip;if(!Current->Step(TEXT("GetNodeTitle"),false,[&]{Title=Node->GetNodeTitle(ENodeTitleType::ListView);}))return NodeObject;if(!Current->Step(TEXT("GetNodeTooltip"),false,[&]{Tooltip=Node->GetTooltipText();}))return NodeObject;
    NodeObject->SetNumberField(TEXT("node_index"), NodeIndex);
    NodeObject->SetStringField(TEXT("node_name"), Node->GetName());
    NodeObject->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    NodeObject->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
    NodeObject->SetStringField(TEXT("node_title"), Title.ToString());
    NodeObject->SetStringField(TEXT("tooltip"), Tooltip.ToString());
    NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
    NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);
    NodeObject->SetNumberField(TEXT("width"), Node->NodeWidth);
    NodeObject->SetNumberField(TEXT("height"), Node->NodeHeight);
    NodeObject->SetNumberField(TEXT("pin_count"), Node->Pins.Num());

    if (const UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
    {
        FString FunctionName = FunctionNameForPath(FunctionCall);if(!ContinueRead())return NodeObject;
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

bool GRPathAdmission(FWork& W,const TSharedPtr<FJsonObject>& P,const FString& Path)
{bool Allow=false;P->TryGetBoolField(TEXT("allow_source_edit"),Allow);if(!Allow&&!IsTempGeneratedNiagaraPath(Path))return W.Fail(TEXT("niagara_scratch_stack_path_rejected"),TEXT("Target is outside original generated temp root without allow_source_edit."));const FString Lower=Path.ToLower();return !(Lower.Contains(TEXT("unrealmcp"))||Lower.Contains(TEXT("cubelessmcp"))||Lower.Contains(TEXT("toolsetregistry")))||W.Fail(TEXT("niagara_scratch_stack_runtime_dependency_rejected"),TEXT("MCP/Cubeless runtime target dependency is not permitted."));}
bool ModuleReadVerified(FWork& W,UNiagaraGraph* G,const FModuleRead& Read,const FNiagaraVariable& V,const FString& Default,UEdGraphPin* Consumer)
{
 if(!W.Admit(TEXT("module_read_final_readback"))||!Read.Node||!FindExpectedPin(W,Read.Output)||!FindExpectedPin(W,Read.Default)||!Live(Read.Metadata)||G->GetAllMetaData().FindRef(V)!=Read.Metadata)return false;
 if(Read.Output->PinName!=V.GetName()||PinTypeForPath(Read.Output)!=V.GetType()||!Read.Output->PersistentGuid.IsValid()||!Read.Default->PersistentGuid.IsValid()||!Read.Default->bNotConnectable||!Read.Default->bDefaultValueIsReadOnly||!Read.Default->LinkedTo.IsEmpty()||!Read.Default->DefaultValue.Equals(Default,ESearchCase::CaseSensitive)||!Read.Default->AutogeneratedDefaultValue.Equals(Default,ESearchCase::CaseSensitive)||Read.Metadata->Variable!=V||Read.Metadata->DefaultMode!=ENiagaraDefaultMode::Value||!Read.Metadata->Metadata.GetVariableGuid().IsValid())return W.Fail(TEXT("niagara_scratch_stack_pair_readback_failed"),TEXT("Actual typed public output/default pair and metadata differ from request."));
 if(V.IsDataAllocated()&&(!Read.Metadata->GetDefaultValueData()||Read.Metadata->GetDefaultValueVariant().GetNumBytes()!=V.GetSizeInBytes()||FMemory::Memcmp(Read.Metadata->GetDefaultValueData(),V.GetData(),V.GetSizeInBytes())!=0))return false;if(Consumer&&(!FindExpectedPin(W,Consumer)||Consumer->LinkedTo!=TArray<UEdGraphPin*>{Read.Output}||!Read.Output->LinkedTo.Contains(Consumer)))return false;return W.Admit(TEXT("module_read_verified"));
}
TSharedPtr<FJsonObject> HandleModuleInput(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
 FString SystemPath;if(!Params->TryGetStringField(TEXT("system_path"),SystemPath))return W.Error(TEXT("Missing 'system_path' parameter"));if(!GRPathAdmission(W,Params,SystemPath))return W.Error(W.R.Error);FString InputName;if(!Params->TryGetStringField(TEXT("input_name"),InputName))Params->TryGetStringField(TEXT("module_input_name"),InputName);InputName.TrimStartAndEndInline();const FString RequestedInputName=InputName;if(InputName.IsEmpty())return W.Error(TEXT("Missing 'input_name' parameter"));if(!InputName.Contains(TEXT(".")))InputName=TEXT("Module.")+InputName;else if(!InputName.StartsWith(TEXT("Module."),ESearchCase::IgnoreCase))return W.Error(TEXT("'input_name' must be unqualified or Module.*"));FString TypeText=TEXT("bool");Params->TryGetStringField(TEXT("input_type"),TypeText);Params->TryGetStringField(TEXT("parameter_type"),TypeText);FNiagaraTypeDefinition Type;if(!TryParseNiagaraTypeDefinition(TypeText,Type))return W.Error(TEXT("Unsupported input_type: ")+TypeText);
 const bool DI=Type.IsDataInterface()&&Type.GetClass()&&(Type.GetClass()->IsChildOf(UNiagaraDataInterfaceRenderTarget2D::StaticClass())||Type.GetClass()->IsChildOf(UNiagaraDataInterfaceTexture::StaticClass()));const bool Scalar=Type==FNiagaraTypeDefinition::GetFloatDef()||Type==FNiagaraTypeDefinition::GetIntDef()||Type==FNiagaraTypeDefinition::GetBoolDef()||Type==FNiagaraTypeDefinition::GetColorDef()||Type==FNiagaraTypeDefinition::GetVec2Def()||Type==FNiagaraTypeDefinition::GetVec3Def()||Type==FNiagaraTypeDefinition::GetVec4Def()||Type==FNiagaraTypeDefinition::GetPositionDef();if(!DI&&!Scalar)return W.Error(TEXT("Scratch module input type is not supported by original first-pass contract."));const auto DefaultJson=Params->TryGetField(TEXT("default_value")).IsValid()?Params->TryGetField(TEXT("default_value")):Params->TryGetField(TEXT("value"));FNiagaraVariable Variable(Type,FName(*InputName));FString DefaultText,Error;if(!DefaultJson&&!DI)return W.Error(TEXT("Missing 'default_value' or 'value' parameter"));if(DefaultJson&&(!SafeDefaultNumbers(Type==FNiagaraTypeDefinition::GetPositionDef()?FNiagaraTypeDefinition::GetVec3Def():Type,DefaultJson)||!SetNiagaraVariableDataFromJson(Variable,DefaultJson,Error)||!GRDefaultString(W,Variable,DefaultText)))return W.Error(Error.IsEmpty()?W.R.Error:Error);
 FString ConsumerName;Params->TryGetStringField(TEXT("consumer_pin_name"),ConsumerName);Params->TryGetStringField(TEXT("connect_to_parameter_name"),ConsumerName);Params->TryGetStringField(TEXT("target_pin_name"),ConsumerName);ConsumerName.TrimStartAndEndInline();bool Require=true,Overwrite=false,Save=true,Compile=true,Wait=false;Params->TryGetBoolField(TEXT("require_consumer_link"),Require);Params->TryGetBoolField(TEXT("overwrite_existing_link"),Overwrite);Params->TryGetBoolField(TEXT("save"),Save);Params->TryGetBoolField(TEXT("request_compile"),Compile);double Timeout=10,Poll=.1;ReadNiagaraCompileWaitParams(Params,Wait,Timeout,Poll);if(Require&&ConsumerName.IsEmpty())return W.Error(TEXT("Missing 'consumer_pin_name'; original first-pass requires a reachable typed Scratch consumer."));FScratchSelection S;if(!ResolveScratch(W,Params,SystemPath,S))return W.Error(W.R.Error);auto* Graph=S.Graph;auto* System=S.System;
 UEdGraphPin* InputMap=nullptr;UNiagaraNodeOutput* ModuleOutput=nullptr;const auto Nodes=Graph->Nodes;for(UEdGraphNode* N:Nodes){if(auto* I=Cast<UNiagaraNodeInput>(N)){if(I->Input.GetType()==FNiagaraTypeDefinition::GetParameterMapDef())for(auto* P:I->Pins)if(P->Direction==EGPD_Output&&PinTypeForPath(P)==FNiagaraTypeDefinition::GetParameterMapDef()){InputMap=P;break;}}else if(auto* O=Cast<UNiagaraNodeOutput>(N))if(!ModuleOutput&&O->GetUsage()==ENiagaraScriptUsage::Module)ModuleOutput=O;}if(!ContinueRead()||!InputMap||!ModuleOutput)return W.Error(TEXT("Scratch Module InputMap/output is missing."));
 UEdGraphPin* Consumer=nullptr;UEdGraphNode* ConsumerNode=nullptr;int32 ConsumerIndex=INDEX_NONE,Matches=0;FModuleRead Read;int32 MapGetIndex=INDEX_NONE;for(int32 I=0;I<Nodes.Num();++I){UEdGraphNode* N=Nodes[I];const FString Class=N->GetClass()->GetPathName();if(Class.Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive))for(auto* P:N->Pins)if(!Read.Output&&P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P)&&P->PinName.ToString().Equals(InputName,ESearchCase::IgnoreCase)){if(PinTypeForPath(P)!=Type)return W.Error(TEXT("Existing Scratch module input has a different type."));Read.Node=N;Read.Output=P;MapGetIndex=I;}
  if(!ConsumerName.IsEmpty()&&(Class.Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapSet"),ESearchCase::CaseSensitive)||N->IsA<UNiagaraNodeCustomHlsl>()))for(auto* P:N->Pins){if(P->Direction!=EGPD_Input||IsNiagaraDynamicAddPin(P)||P->PinName==TEXT("Source"))continue;const FString Name=P->PinName.ToString();if(!(Name.Equals(ConsumerName,ESearchCase::IgnoreCase)||Name.EndsWith(TEXT(".")+ConsumerName,ESearchCase::IgnoreCase)||ConsumerName.EndsWith(TEXT(".")+Name,ESearchCase::IgnoreCase)))continue;if(PinTypeForPath(P)!=Type)return W.Error(TEXT("Consumer pin type differs from requested input."));++Matches;if(!Consumer){Consumer=P;ConsumerNode=N;ConsumerIndex=I;}}}
 if(!ConsumerName.IsEmpty()&&!Consumer)return W.Error(TEXT("No matching Scratch consumer pin found (validated before writer)."));if(Matches>1)return W.Error(TEXT("Consumer selector is ambiguous (validated before writer)."));const bool Created=Read.Node==nullptr;if(Created&&!Consumer)return W.Fail(TEXT("niagara_scratch_stack_unconnected_pair_unsupported"),TEXT("New unconnected default-pair creation has no confirmed public lifecycle path; no writer.")),W.Error(W.R.Error);if(Read.Node){int32 Semantic=0;for(auto* P:Read.Node->Pins)if(P->Direction==EGPD_Output&&!IsNiagaraDynamicAddPin(P))++Semantic;for(auto* P:Read.Node->Pins)if(P->Direction==EGPD_Input&&PinTypeForPath(P)==Type&&P->bNotConnectable&&P->bDefaultValueIsReadOnly){if(Read.Default)return W.Error(TEXT("Existing public default pair is ambiguous."));Read.Default=P;}if(Semantic!=1||!Read.Default||!Read.Output->PersistentGuid.IsValid()||!Read.Default->PersistentGuid.IsValid())return W.Fail(TEXT("niagara_scratch_stack_existing_complex_pair_unsupported"),TEXT("Only one existing semantic output/default public pair is supported; no private pair-map inspection.")),W.Error(W.R.Error);}
 const int32 PreviousLinks=Consumer?Consumer->LinkedTo.Num():0;const bool Already=Consumer&&Read.Output&&Consumer->LinkedTo.Contains(Read.Output);const auto Prior=Consumer?LinkedPinsToJson(Consumer,true):TArray<TSharedPtr<FJsonValue>>();if(Consumer&&!Already&&PreviousLinks>0&&!Overwrite)return W.Error(TEXT("Consumer already has links; overwrite_existing_link=True is required before writer."));if(!W.WaitAdmission(Wait,Compile))return W.Error(W.R.Error);UClass* GetClass=nullptr;if(Created&&!W.Step(TEXT("LoadClass_ParameterMapGet"),false,[&]{GetClass=StaticLoadClass(UEdGraphNode::StaticClass(),nullptr,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"));}))return W.Error(W.R.Error);if(Created&&(!GetClass||!GetClass->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive)))return W.Error(TEXT("Exact stock MapGet class unavailable."));
 W.Before=W.Projection();if(!ContinueRead()||!W.Modify(System)||!W.Modify(S.Script)||!W.Modify(S.Source)||!W.Modify(Graph))return W.Error(W.R.Error);if(Created){Read.Node=CreateNode(W,Graph,GetClass,ModuleOutput->NodePosX-640,ModuleOutput->NodePosY+260);if(!Read.Node||!SpecializeModuleRead(W,Graph,Read.Node,Consumer,Variable,DefaultText,Read))return W.Error(W.R.Error);MapGetIndex=Graph->Nodes.Find(Read.Node);}else{if(!SetMetadataDefault(W,Graph,Variable,Read.Metadata))return W.Error(W.R.Error);FGraphEditPlan D;D.MutablePins.Add(Read.Default);if(!GraphEdit(W,Graph,TEXT("UpdateExistingDefaultPin"),D,[&]{Read.Default->DefaultValue=DefaultText;Read.Default->AutogeneratedDefaultValue=DefaultText;W.R.bChanged=true;}))return W.Error(W.R.Error);if(Consumer&&!Already){FGraphEditPlan L;if(!SchemaLink(W,Graph,Read.Output,Consumer,L,Overwrite))return W.Error(W.R.Error);}}
 auto* SourcePin=MapPin(Read.Node,EGPD_Input);if(!SourcePin)return W.Error(TEXT("MapGet Source ParameterMap pin missing."));const bool SourceAlready=SourcePin->LinkedTo.Contains(InputMap);if(!SourceAlready&&SourcePin->LinkedTo.IsEmpty()){FGraphEditPlan SourceLink;if(!SchemaLink(W,Graph,InputMap,SourcePin,SourceLink))return W.Error(W.R.Error);}auto* NiagaraGet=Cast<UNiagaraNode>(Read.Node);if(!W.Step(TEXT("MapGet_MarkSynchronization"),true,[&]{NiagaraGet->MarkNodeRequiresSynchronization(TEXT("Cubeless: Scratch module input authored"),true);}))return W.Error(W.R.Error);int32 Referencing=0,Refreshed=0;if(!RefreshedCalls(W,S.Script,Variable,Referencing,Refreshed))return W.Error(W.R.Error);if(!W.Step(TEXT("NotifyGraphChanged"),true,[&]{static_cast<UEdGraph*>(Graph)->NotifyGraphChanged();})||!W.Dirty())return W.Error(W.R.Error);bool Accepted=false;if(Compile&&!W.Request(Accepted))return W.Error(W.R.Error);auto CompleteModuleInput=[=,&W](const TSharedPtr<FJsonObject>& WaitResult)->TSharedPtr<FJsonObject>
 {
  if(!W.Admit(TEXT("module_input_post_wait_admission")))return W.Error(W.R.Error);
  if(Save&&!W.Save(false))return W.Error(W.R.Error);if(!ModuleReadVerified(W,Graph,Read,Variable,DefaultText,Consumer))return W.Error(W.R.Error);
 auto Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("success"),true);Result->SetBoolField(TEXT("created_map_get_node"),Created);Result->SetStringField(TEXT("system_path"),System->GetPathName());Result->SetStringField(TEXT("scratch_pad_script_path"),S.Script->GetPathName());Result->SetStringField(TEXT("scratch_pad_script_name"),S.Script->GetName());Result->SetStringField(TEXT("scratch_pad_owner_kind"),S.OwnerKind);Result->SetStringField(TEXT("scratch_pad_owner_name"),S.OwnerName);Result->SetNumberField(TEXT("scratch_pad_emitter_index"),S.EmitterIndex);Result->SetNumberField(TEXT("scratch_pad_script_index"),S.ScriptIndex);Result->SetStringField(TEXT("input_name"),Read.Output->PinName.ToString());Result->SetStringField(TEXT("requested_input_name"),InputName);Result->SetStringField(TEXT("input_type"),NiagaraTypeName(Type));Result->SetField(TEXT("default_value"),NiagaraVariableDataToJsonValue(Variable));Result->SetStringField(TEXT("default_pin_value"),DefaultText);Result->SetStringField(TEXT("script_variable_guid"),Read.Metadata->Metadata.GetVariableGuid().ToString(EGuidFormats::DigitsWithHyphens));Result->SetStringField(TEXT("parameter_map_get_node_name"),Read.Node->GetName());Result->SetStringField(TEXT("parameter_map_get_node_guid"),Read.Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));Result->SetNumberField(TEXT("parameter_map_get_node_index"),MapGetIndex);Result->SetStringField(TEXT("map_get_source_pin"),SourcePin->PinName.ToString());Result->SetNumberField(TEXT("map_get_source_link_count"),SourcePin->LinkedTo.Num());Result->SetBoolField(TEXT("map_get_source_was_already_linked"),SourceAlready);Result->SetStringField(TEXT("consumer_pin_name"),Consumer?Consumer->PinName.ToString():FString());Result->SetStringField(TEXT("consumer_node_name"),ConsumerNode?ConsumerNode->GetName():FString());Result->SetStringField(TEXT("consumer_node_class"),ConsumerNode?ConsumerNode->GetClass()->GetName():FString());Result->SetStringField(TEXT("consumer_node_guid"),ConsumerNode?ConsumerNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens):FString());Result->SetNumberField(TEXT("consumer_node_index"),ConsumerIndex);Result->SetNumberField(TEXT("previous_consumer_link_count"),PreviousLinks);Result->SetNumberField(TEXT("new_consumer_link_count"),Consumer?Consumer->LinkedTo.Num():0);Result->SetBoolField(TEXT("consumer_already_linked"),Already);Result->SetArrayField(TEXT("previous_consumer_links"),Prior);Result->SetObjectField(TEXT("module_input_pin"),GraphPinToJsonObject(Read.Output));Result->SetObjectField(TEXT("module_input_default_pin"),GraphPinToJsonObject(Read.Default));Result->SetObjectField(TEXT("parameter_map_get_node"),DetailedGraphNodeToJsonObject(Read.Node,MapGetIndex,true,true));Result->SetNumberField(TEXT("referencing_function_call_count"),Referencing);Result->SetNumberField(TEXT("refreshed_function_call_count"),Refreshed);Result->SetBoolField(TEXT("compile_requested"),Accepted);Result->SetObjectField(TEXT("compile_wait"),WaitResult);Result->SetBoolField(TEXT("saved"),W.R.bSaved);Result->SetStringField(TEXT("write_scope"),TEXT("scratch_pad_module_input_read"));W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);W.R.Data->SetBoolField(TEXT("protected_pair_storage_independently_read"),false);return Result;
 };
 if(W.DeferredMode&&Wait)
 {
  BeginModuleInputWait(W,System,Timeout,Poll,MoveTemp(CompleteModuleInput));
  auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);return Pending;
 }
 auto WaitResult=W.WaitSnapshot(Wait,Timeout,Poll);if(!ContinueRead())return W.Error(W.R.Error);return CompleteModuleInput(WaitResult);
}

bool CaptureSelectedGraph(FWork& W,UNiagaraGraph* G)
{
 if(!W.Admit(TEXT("selected_stack_graph")))return false;UNiagaraScriptSource* Source=nullptr;for(const auto& B:W.ScriptBindings){auto* S=Cast<UNiagaraScriptSource>(B.Source.Get());if(S&&S->NodeGraph==G){Source=S;break;}}if(!Source)for(const auto& E:W.Emitters){auto* S=Cast<UNiagaraScriptSource>(E.Source.Get());if(S&&S->NodeGraph==G){Source=S;break;}}return Source&&W.Own(Source)&&W.CaptureGraph(Source)&&W.GraphForWrite(G);
}
UEdGraphPin* UniqueMapPin(FWork& W,UEdGraphNode* N,EEdGraphPinDirection Direction)
{if(!W.Admit(TEXT("unique_map_pin"))||!Live(N))return nullptr;UEdGraphPin* Found=nullptr;for(auto* P:N->Pins)if(P->Direction==Direction&&PinTypeForPath(P)==FNiagaraTypeDefinition::GetParameterMapDef()){if(Found)return nullptr;Found=P;}return Found;}
bool StackChain(FWork& W,UNiagaraNodeOutput* Output,TArray<UEdGraphNode*>& Forward)
{
 if(!W.Admit(TEXT("stack_chain_preflight")))return false;TSet<UEdGraphNode*> Seen;UEdGraphNode* N=Output;while(N){if(Seen.Contains(N)||Seen.Num()>16384)return W.Fail(TEXT("niagara_scratch_stack_stack_cycle"),TEXT("Bounded acyclic exact ParameterMap stack required."));Seen.Add(N);Forward.Insert(N,0);if(auto* Input=Cast<UNiagaraNodeInput>(N)){if(Input->Input.GetType()!=FNiagaraTypeDefinition::GetParameterMapDef()||!UniqueMapPin(W,N,EGPD_Output))return false;return Forward.Num()>=2;}auto* In=UniqueMapPin(W,N,EGPD_Input);if(!In||In->LinkedTo.Num()!=1||!FindExpectedPin(W,In->LinkedTo[0]))return W.Fail(TEXT("niagara_scratch_stack_stack_input_invalid"),TEXT("Every selected stack group needs one captured upstream ParameterMap source."));auto* Prior=In->LinkedTo[0];if(Prior->Direction!=EGPD_Output||!Prior->LinkedTo.Contains(In)||Prior->GetOwningNodeUnchecked()->GetGraph()!=Output->GetGraph())return false;N=Prior->GetOwningNodeUnchecked();}return false;
}
bool ReadNewAssignmentMapPins(FWork& W,UNiagaraGraph* G,UNiagaraNodeAssignment* Node,UEdGraphPin*& Input,UEdGraphPin*& Output)
{
 Input=nullptr;Output=nullptr;
 if(!W.Admit(TEXT("new_assignment_pin_admission"))||W.TransitionGraph!=G||!Live(G)||!Live(Node)||Node->GetClass()!=UNiagaraNodeAssignment::StaticClass()||!W.Own(Node))return false;
 const TWeakObjectPtr<UNiagaraNodeAssignment> WeakNode(Node);const FString NodePath=Node->GetPathName();const EObjectFlags Flags=Node->GetFlags();const TArray<UEdGraphPin*> Pins=Node->Pins;
 if(Pins.IsEmpty()||Pins.Num()>4096||!W.Budget(Pins.Num()))return W.Fail(TEXT("niagara_scratch_stack_new_assignment_pin_bound"),TEXT("Fresh Assignment pin inventory exceeds its bounded local transition."));
 auto Fresh=[&]()->bool
 {
  if(!W.Admit(TEXT("new_assignment_pin_membership"))||W.TransitionGraph!=G||!Live(G)||WeakNode.Get()!=Node||!Live(Node)||Node->GetClass()!=UNiagaraNodeAssignment::StaticClass()||!Node->GetPathName().Equals(NodePath,ESearchCase::CaseSensitive)||Node->GetFlags()!=Flags||Node->GetOuter()!=G||Node->GetGraph()!=G||!Node->IsIn(W.System.Get())||Node->GetPackage()!=G->GetPackage()||Node->GetOutermost()!=G->GetOutermost()||G->Nodes.Num()>65536||G->Nodes.FilterByPredicate([&](const auto& Entry){return Entry.Get()==Node;}).Num()!=1||Node->Pins!=Pins)return W.Fail(TEXT("niagara_scratch_stack_new_assignment_pin_identity_changed"),TEXT("Fresh owned Assignment identity or current pin membership changed before exact splice capture."));
  return true;
 };
 if(!Fresh())return false;TSet<FGuid> PinIds;
 for(UEdGraphPin* P:Pins)
 {
  if(!Fresh()||!P||!Node->Pins.Contains(P)||P->GetOwningNodeUnchecked()!=Node||!P->PinId.IsValid()||PinIds.Contains(P->PinId))return W.Fail(TEXT("niagara_scratch_stack_new_assignment_pin_invalid"),TEXT("Fresh Assignment pin is not a unique exact current member."));
  PinIds.Add(P->PinId);const FGuid Id=P->PinId,Persistent=P->PersistentGuid;const FEdGraphPinType BeforeType=P->PinType;const EEdGraphPinDirection Direction=P->Direction;const TArray<UEdGraphPin*> Links=P->LinkedTo;FNiagaraTypeDefinition Type;
  if(!W.Step(TEXT("PinToTypeDefinition_NewAssignment"),false,[&]{Type=UEdGraphSchema_Niagara::PinToTypeDefinition(P);})||!Fresh())return false;
  if(!P||!Node->Pins.Contains(P)||P->GetOwningNodeUnchecked()!=Node||P->PinId!=Id||P->PersistentGuid!=Persistent||P->PinType!=BeforeType||P->Direction!=Direction||P->LinkedTo!=Links)return W.Fail(TEXT("niagara_scratch_stack_new_assignment_pin_changed"),TEXT("Fresh Assignment pin identity/type/links changed during public type observation."));
  if(Direction!=EGPD_Input&&Direction!=EGPD_Output)return W.Fail(TEXT("niagara_scratch_stack_new_assignment_pin_direction_invalid"),TEXT("Fresh Assignment pin has an invalid direction."));
  if(Type==FNiagaraTypeDefinition::GetParameterMapDef()){auto*& Match=Direction==EGPD_Input?Input:Output;if(Match)return W.Fail(TEXT("niagara_scratch_stack_new_assignment_map_pin_ambiguous"),TEXT("Fresh Assignment needs exactly one ParameterMap input and output."));Match=P;}
 }
 return (Input&&Output)||W.Fail(TEXT("niagara_scratch_stack_new_assignment_map_pin_missing"),TEXT("Fresh Assignment ParameterMap input/output not observed; actual creation remains recorded."));
}
UNiagaraNodeAssignment* AddAssignment(FWork& W,UNiagaraGraph* G,UNiagaraNodeOutput* Output,int32 Index,const FNiagaraVariable& Target,const FString& Default)
{
 TArray<UEdGraphNode*> Chain;if(!StackChain(W,Output,Chain))return nullptr;int32 Modules=0;for(auto* N:Chain)if(N->IsA<UNiagaraNodeFunctionCall>())++Modules;const int32 SafeIndex=Index==INDEX_NONE?INDEX_NONE:FMath::Clamp(Index,0,Modules);W.Prefix->SetNumberField(TEXT("assignment_SDK_target_index"),SafeIndex);const FGraphIdentity Old=*W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});TArray<FMetaObservation> OldMeta;if(!MetadataSnapshot(W,G,OldMeta)||!W.SDK(TEXT("AddParameterModuleToStack"),true))return nullptr;UNiagaraNodeAssignment* Node=nullptr;auto Row=W.MutationRow(TEXT("AddParameterModuleToStack"));Row->SetStringField(TEXT("output_guid"),Output->NodeGuid.ToString());Row->SetStringField(TEXT("target_name"),Target.GetName().ToString());
 {TGuardValue<UNiagaraGraph*> Mutable(W.TransitionGraph,G);const TArray<FNiagaraVariable> Targets{Target};const TArray<FString> Defaults{Default};Node=FNiagaraStackGraphUtilities::AddParameterModuleToStack(Targets,*Output,SafeIndex,Defaults);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),Node!=nullptr);W.R.bChanged|=Node!=nullptr;if(!W.Admit(TEXT("after_assignment_context")))return nullptr;if(!Live(Node)||Node->GetClass()!=UNiagaraNodeAssignment::StaticClass()||Node->GetGraph()!=G||!W.Own(Node)||Node->GetAssignmentTargets()!=Targets||Node->GetAssignmentDefaults()!=Defaults)return W.Fail(TEXT("niagara_scratch_stack_assignment_readback_failed"),TEXT("Actual assignment target/default/owner differs from requested exact one.")),nullptr;Row->SetStringField(TEXT("actual_node_path"),Node->GetPathName());Row->SetStringField(TEXT("actual_node_guid"),Node->NodeGuid.ToString());UEdGraphPin* In=nullptr;UEdGraphPin* Out=nullptr;if(!ReadNewAssignmentMapPins(W,G,Node,In,Out)||In->LinkedTo.Num()!=1||Out->LinkedTo.IsEmpty())return nullptr;auto* Prior=In->LinkedTo[0];if(!FindExpectedPin(W,Prior))return nullptr;const auto* PriorBefore=FindExpectedPin(W,Prior);FGraphEditPlan Plan;Plan.AddedNode=Node;Plan.Links.Add(Prior,PriorBefore->Links);for(auto* Next:Out->LinkedTo){const auto* OldInput=FindExpectedPin(W,Next);if(!OldInput||OldInput->Links!=TArray<UEdGraphPin*>{Prior})return W.Fail(TEXT("niagara_scratch_stack_assignment_splice_invalid"),TEXT("Assignment rewired an unowned or unexpected old stack input.")),nullptr;Plan.Links[Prior].Remove(Next);Plan.Links.Add(Next,TArray<UEdGraphPin*>{Out});}Plan.Links[Prior].AddUnique(In);if(!ObserveGraphTransition(W,G,Old,OldMeta,Plan))return nullptr;}
 if(!W.After(TEXT("AddParameterModuleToStack")))return nullptr;TArray<UEdGraphNode*> After;if(!StackChain(W,Output,After)||!After.Contains(Node))return nullptr;TArray<UEdGraphNode*> Calls;for(auto* N:After)if(N->IsA<UNiagaraNodeFunctionCall>())Calls.Add(N);const int32 Expected=SafeIndex==INDEX_NONE?Calls.Num()-1:SafeIndex;if(!Calls.IsValidIndex(Expected)||Calls[Expected]!=Node)return W.Fail(TEXT("niagara_scratch_stack_assignment_index_readback_failed"),TEXT("New assignment is not at the requested clamped module index.")),nullptr;return Node;
}
TSharedPtr<FJsonObject> HandleAssignment(FWork& W,const TSharedPtr<FJsonObject>& Params)
{

    FString ProofToken;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("allow_assignment_module_route"), ProofToken) ||
        ProofToken != TEXT("ALLOW_SCRATCH_PAD_ASSIGNMENT_MODULE_ROUTE"))
    {
        return W.Error(TEXT("add_niagara_scratch_pad_assignment_module_to_stack requires allow_assignment_module_route=ALLOW_SCRATCH_PAD_ASSIGNMENT_MODULE_ROUTE"));
    }

    FString TargetSystemPath;
    if (!Params->TryGetStringField(TEXT("target_system_path"), TargetSystemPath) && !Params->TryGetStringField(TEXT("system_path"), TargetSystemPath))
    {
        return W.Error(TEXT("Missing 'target_system_path' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(TargetSystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara system outside %s: %s"), NiagaraTempGenerationRoot, *TargetSystemPath));
    }

    if(!GRPathAdmission(W,Params,TargetSystemPath))return W.Error(W.R.Error);
    FString ParameterName = TEXT("Particles.SpriteSize");
    Params->TryGetStringField(TEXT("parameter_name"), ParameterName);
    ParameterName.TrimStartAndEndInline();

    FString ParameterTypeString = TEXT("vec2");
    Params->TryGetStringField(TEXT("parameter_type"), ParameterTypeString);
    FNiagaraTypeDefinition ParameterType;
    if (!TryParseNiagaraTypeDefinition(ParameterTypeString, ParameterType))
    {
        return W.Error(FString::Printf(TEXT("Unsupported assignment-module parameter_type: %s"), *ParameterTypeString));
    }

    FString DefaultValueString = TEXT("50.000000,50.000000");
    Params->TryGetStringField(TEXT("default_value"), DefaultValueString);
    if (!Params->HasField(TEXT("default_value")) && Params->HasField(TEXT("value")))
    {
        FString ParseError;
        if (!BuildNiagaraPinDefaultValueFromJson(Params->TryGetField(TEXT("value")), DefaultValueString, ParseError))
        {
            return W.Error(ParseError);
        }
    }
    DefaultValueString.TrimStartAndEndInline();

    const bool bSpriteSizeAssignmentShape =
        ParameterName.Equals(TEXT("Particles.SpriteSize"), ESearchCase::IgnoreCase) &&
        ParameterType == FNiagaraTypeDefinition::GetVec2Def() &&
        DefaultValueString.Equals(TEXT("50.000000,50.000000"), ESearchCase::CaseSensitive);
    const bool bColorAssignmentShape =
        ParameterName.Equals(TEXT("Particles.Color"), ESearchCase::IgnoreCase) &&
        ParameterType == FNiagaraTypeDefinition::GetColorDef() &&
        DefaultValueString.Equals(TEXT("1.000000,1.000000,1.000000,1.000000"), ESearchCase::CaseSensitive);
    const bool bLifetimeAssignmentShape =
        ParameterName.Equals(TEXT("Particles.Lifetime"), ESearchCase::IgnoreCase) &&
        ParameterType == FNiagaraTypeDefinition::GetFloatDef() &&
        DefaultValueString.Equals(TEXT("5.000000"), ESearchCase::CaseSensitive);
    const bool bVelocityAssignmentShape =
        ParameterName.Equals(TEXT("Particles.Velocity"), ESearchCase::IgnoreCase) &&
        ParameterType == FNiagaraTypeDefinition::GetVec3Def() &&
        DefaultValueString.Equals(TEXT("0.000000,0.000000,100.000000"), ESearchCase::CaseSensitive);
    if (!bSpriteSizeAssignmentShape && !bColorAssignmentShape && !bLifetimeAssignmentShape && !bVelocityAssignmentShape)
    {
        return W.Error(
            TEXT("The assignment-module route admits only reviewed temp shapes: Particles.SpriteSize vec2 default 50.000000,50.000000, Particles.Color Linear Color default 1.000000,1.000000,1.000000,1.000000, Particles.Lifetime float default 5.000000, or Particles.Velocity vec3 default 0.000000,0.000000,100.000000"));
    }

    FString TargetUsageString = TEXT("ParticleUpdateScript");
    Params->TryGetStringField(TEXT("target_usage"), TargetUsageString);
    ENiagaraScriptUsage TargetUsage = ENiagaraScriptUsage::ParticleUpdateScript;
    if (!TryParseNiagaraScriptUsage(TargetUsageString, TargetUsage))
    {
        return W.Error(FString::Printf(TEXT("Unsupported target_usage: %s"), *TargetUsageString));
    }

    FGuid TargetUsageId;
    FString TargetUsageIdString;
    Params->TryGetStringField(TEXT("target_usage_id"), TargetUsageIdString);
    if (!TargetUsageIdString.IsEmpty() && !FGuid::Parse(TargetUsageIdString, TargetUsageId))
    {
        return W.Error(FString::Printf(TEXT("Invalid target_usage_id GUID: %s"), *TargetUsageIdString));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    TargetEmitterIndex=IndexParam(W,Params,TEXT("target_emitter_index"));
    Params->TryGetStringField(TEXT("target_emitter_name"), TargetEmitterName);

    int32 TargetIndex = INDEX_NONE;
    TargetIndex=IndexParam(W,Params,TEXT("target_index"));

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    bool bSkipIfDuplicate = true;
    Params->TryGetBoolField(TEXT("skip_if_duplicate"), bSkipIfDuplicate);

    UNiagaraSystem* TargetSystem = LoadSystem(TargetSystemPath);
    if (!TargetSystem)
    {
        return W.Error(FString::Printf(TEXT("Failed to load target Niagara system: %s"), *TargetSystemPath));
    }

    UNiagaraGraph* TargetGraph = nullptr;
    UNiagaraNodeOutput* TargetOutputNode = nullptr;
    FString ResolvedStackOwnerKind;
    FString ResolvedStackOwnerName;
    int32 ResolvedTargetEmitterIndex = INDEX_NONE;

    if (TargetUsage == ENiagaraScriptUsage::SystemSpawnScript || TargetUsage == ENiagaraScriptUsage::SystemUpdateScript)
    {
        UNiagaraScript* TargetScript = TargetUsage == ENiagaraScriptUsage::SystemSpawnScript
            ? TargetSystem->GetSystemSpawnScript()
            : TargetSystem->GetSystemUpdateScript();
        UNiagaraScriptSource* ScriptSource = TargetScript ? Cast<UNiagaraScriptSource>(TargetScript->GetLatestSource()) : nullptr;
        TargetGraph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
        ResolvedStackOwnerKind = TEXT("system");
        ResolvedStackOwnerName = TargetSystem->GetName();
    }
    else if (IsEmitterStackUsage(TargetUsage))
    {
        int32 CurrentEmitterIndex = 0;
        for (FNiagaraEmitterHandle& EmitterHandle : TargetSystem->GetEmitterHandles())
        {
            const FString EmitterName = EmitterHandle.GetName().ToString();
            const bool bEmitterMatches =
                (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
                (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));
            if (bEmitterMatches)
            {
                if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
                {
                    UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(EmitterData->GraphSource);
                    TargetGraph = ScriptSource ? ScriptSource->NodeGraph : nullptr;
                    ResolvedStackOwnerKind = TEXT("emitter");
                    ResolvedStackOwnerName = EmitterName;
                    ResolvedTargetEmitterIndex = CurrentEmitterIndex;
                }
                break;
            }
            ++CurrentEmitterIndex;
        }
    }

    if (!TargetGraph)
    {
        return W.Error(TEXT("No matching Niagara stack graph was found for the requested target_usage/emitter selector"));
    }

    if(!CaptureSelectedGraph(W,TargetGraph))return W.Error(W.R.Error);
    if(!W.Step(TEXT("FindEquivalentOutputNode"),false,[&]{TargetOutputNode=TargetGraph->FindEquivalentOutputNode(TargetUsage,TargetUsageId);}))return W.Error(W.R.Error);
    if (!TargetOutputNode)
    {
        return W.Error(
            FString::Printf(TEXT("No matching output node was found for target_usage '%s'"), *NiagaraScriptUsageName(TargetUsage)));
    }

    TArray<UEdGraphNode*> OriginalStack;if(!StackChain(W,TargetOutputNode,OriginalStack))return W.Error(W.R.Error);
    const int32 NodeCountBefore = TargetGraph->Nodes.Num();
    W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);
    if (bSkipIfDuplicate)
    {
        for (UEdGraphNode* Node : OriginalStack)
        {
            UNiagaraNodeAssignment* ExistingAssignment = Cast<UNiagaraNodeAssignment>(Node);
            if (!ExistingAssignment)
            {
                continue;
            }
            if (!OriginalStack.Contains(ExistingAssignment))
            {
                continue;
            }
            bool bMatchesParameter = false;
            for (const FNiagaraVariable& Target : ExistingAssignment->GetAssignmentTargets())
            {
                if (Target.GetName().ToString().Equals(ParameterName, ESearchCase::IgnoreCase))
                {
                    bMatchesParameter = true;
                    break;
                }
            }
            if (!bMatchesParameter)
            {
                continue;
            }

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("success"), true);
            Result->SetBoolField(TEXT("skipped_duplicate"), true);
            Result->SetStringField(TEXT("skip_reason"), TEXT("assignment_module_parameter_already_exists_in_target_usage"));
            Result->SetStringField(TEXT("target_system_path"), TargetSystem->GetPathName());
            Result->SetStringField(TEXT("system_path"), TargetSystem->GetPathName());
            Result->SetStringField(TEXT("parameter_name"), ParameterName);
            Result->SetStringField(TEXT("parameter_type"), NiagaraTypeName(ParameterType));
            Result->SetStringField(TEXT("default_value"), DefaultValueString);
            Result->SetStringField(TEXT("target_usage"), NiagaraScriptUsageName(TargetUsage));
            Result->SetStringField(TEXT("target_usage_id"), TargetUsageId.ToString(EGuidFormats::DigitsWithHyphens));
            Result->SetStringField(TEXT("stack_owner_kind"), ResolvedStackOwnerKind);
            Result->SetStringField(TEXT("stack_owner_name"), ResolvedStackOwnerName);
            Result->SetNumberField(TEXT("target_emitter_index"), ResolvedTargetEmitterIndex);
            Result->SetNumberField(TEXT("target_index"), TargetIndex);
            Result->SetStringField(TEXT("existing_assignment_node_guid"), ExistingAssignment->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
            Result->SetStringField(TEXT("existing_assignment_node_name"), ExistingAssignment->GetName());
            Result->SetStringField(TEXT("output_node_guid"), TargetOutputNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
            Result->SetStringField(TEXT("graph_path"), TargetGraph->GetPathName());
            Result->SetNumberField(TEXT("graph_node_count_before"), NodeCountBefore);
            Result->SetNumberField(TEXT("graph_node_count_after"), TargetGraph->Nodes.Num());
            Result->SetBoolField(TEXT("compile_requested"), false);
            Result->SetBoolField(TEXT("saved"), false);
            Result->SetStringField(TEXT("write_scope"), TEXT("target_temp_assignment_module_duplicate_skip"));
            Result->SetObjectField(TEXT("module"), BuildFunctionCallJson(ExistingAssignment, INDEX_NONE, false));
            W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;
        }
    }

    if(!W.Modify(TargetSystem)||!W.Modify(TargetGraph)||!W.Modify(TargetOutputNode))return W.Error(W.R.Error);

    TArray<FNiagaraVariable> AssignmentTargets;
    AssignmentTargets.Add(FNiagaraVariable(ParameterType, FName(*ParameterName)));
    TArray<FString> DefaultValues;
    DefaultValues.Add(DefaultValueString);

    UNiagaraNodeAssignment* NewAssignmentNode=AddAssignment(W,TargetGraph,TargetOutputNode,TargetIndex,AssignmentTargets[0],DefaultValues[0]);
    if (!NewAssignmentNode)
    {
        return W.Error(TEXT("Niagara stack utility failed to add the assignment module"));
    }

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
            return W.Error(TEXT("Failed to save target Niagara system after assignment-module stack insertion"));
        }
    }

    if(!W.Admit(TEXT("assignment_final_readback"))||NewAssignmentNode->GetAssignmentTargets()!=AssignmentTargets||NewAssignmentNode->GetAssignmentDefaults()!=DefaultValues)return W.Error(TEXT("Final assignment typed readback changed after compile/save."));
    TArray<TSharedPtr<FJsonValue>> AssignmentTargetValues;
    for (const FNiagaraVariable& Target : NewAssignmentNode->GetAssignmentTargets())
    {
        AssignmentTargetValues.Add(MakeShared<FJsonValueObject>(NiagaraVariableToJsonObject(Target)));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("skipped_duplicate"), false);
    Result->SetStringField(TEXT("target_system_path"), TargetSystem->GetPathName());
    Result->SetStringField(TEXT("system_path"), TargetSystem->GetPathName());
    Result->SetStringField(TEXT("parameter_name"), ParameterName);
    Result->SetStringField(TEXT("parameter_type"), NiagaraTypeName(ParameterType));
    Result->SetStringField(TEXT("default_value"), DefaultValueString);
    Result->SetStringField(TEXT("target_usage"), NiagaraScriptUsageName(TargetUsage));
    Result->SetStringField(TEXT("target_usage_id"), TargetUsageId.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("stack_owner_kind"), ResolvedStackOwnerKind);
    Result->SetStringField(TEXT("stack_owner_name"), ResolvedStackOwnerName);
    Result->SetNumberField(TEXT("target_emitter_index"), ResolvedTargetEmitterIndex);
    Result->SetNumberField(TEXT("target_index"), TargetIndex);
    Result->SetStringField(TEXT("new_assignment_node_guid"), NewAssignmentNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("new_assignment_node_name"), NewAssignmentNode->GetName());
    Result->SetStringField(TEXT("new_assignment_function_name"), FunctionNameForPath(NewAssignmentNode));
    Result->SetNumberField(TEXT("assignment_target_count"), NewAssignmentNode->GetAssignmentTargets().Num());
    Result->SetArrayField(TEXT("assignment_targets"), AssignmentTargetValues);
    Result->SetStringField(TEXT("output_node_guid"), TargetOutputNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("graph_path"), TargetGraph->GetPathName());
    Result->SetNumberField(TEXT("graph_node_count_before"), NodeCountBefore);
    Result->SetNumberField(TEXT("graph_node_count_after"), TargetGraph->Nodes.Num());
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("target_temp_assignment_module_stack_insert"));
    Result->SetObjectField(TEXT("module"), BuildFunctionCallJson(NewAssignmentNode, INDEX_NONE, false));
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;

}
bool SetHlslPublic(FWork& W,UNiagaraGraph* G,UNiagaraNodeCustomHlsl* Node,const FString& Body)
{
 FProperty* Field=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));
 if(!CastField<FStrProperty>(Field)||Field->ArrayDim!=1||EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(Field),EPropertyAccessResultFlags::PermissionDenied)||EnumHasAnyFlags(PropertyAccessUtil::CanSetPropertyValue(Field,PropertyAccessUtil::EditorReadOnlyFlags,false),EPropertyAccessResultFlags::PermissionDenied))return W.Fail(TEXT("niagara_scratch_stack_HLSL_property_denied"),TEXT("Exact public editor-accessible CustomHlsl property required before writer."));
 // The public property callback rebuilds pins. Only this request's newly created,
 // completely unlinked HLSL node receives a one-use expected reconstruction.
 if(!W.Admit(TEXT("fresh_HLSL_reconstruction_preflight"))||!W.FreshUnlinkedHlslNodes.Contains(Node)||!Live(Node)||Node->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass()||Node->GetOuter()!=G||Node->GetGraph()!=G||!W.Own(Node)||!W.GraphForWrite(G))return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_unsupported"),TEXT("Pin reconstruction is limited to a fresh owned and unlinked HLSL node from this request."));
 const auto* GraphBefore=W.Graphs.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});
 const auto* SourceBefore=W.Sources.FindByPredicate([&](const auto& E){return E.Graph.Get()==G;});
 if(!GraphBefore||!SourceBefore)return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_uncaptured"),TEXT("Exact captured graph/source required."));
 const FGraphIdentity Old=*GraphBefore;const TWeakObjectPtr<UNiagaraScriptSource> Source=SourceBefore->Source;
 const auto* SelectedBefore=Old.Nodes.FindByPredicate([&](const auto& E){return E.Node.Get()==Node;});
 if(!SelectedBefore||!SelectedBefore->Function||SelectedBefore->Pins.IsEmpty()||SelectedBefore->Pins.Num()>4096||!W.Budget(SelectedBefore->Pins.Num()))return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_bound"),TEXT("Fresh HLSL requires a bounded captured signature and pin inventory."));
 const FNodeIdentity Selected=*SelectedBefore;const EObjectFlags NodeFlags=Node->GetFlags();const ENiagaraScriptUsage Usage=Node->ScriptUsage;
 TSet<FGuid> OldIds;
 for(const auto& E:Selected.Pins)
 {
  if(!E.Pointer||!E.Id.IsValid()||OldIds.Contains(E.Id)||!E.Links.IsEmpty()||!Node->Pins.Contains(E.Pointer)||E.Pointer->GetOwningNodeUnchecked()!=Node||E.Pointer->bOrphanedPin||E.Pointer->IsPendingKill()||!PinHeaderSame(E,E.Pointer))return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_precondition"),TEXT("Fresh HLSL pins must be unique, current, non-orphaned and completely unlinked before property edit."));
  OldIds.Add(E.Id);
 }
 TArray<FMetaObservation> MetadataBefore;if(!MetadataSnapshot(W,G,MetadataBefore))return false;
 auto Values=MakeShared<FJsonObject>();Values->SetStringField(TEXT("CustomHlsl"),Body);TArray<FName> Written;bool Ok=false;
 if(!W.SDK(TEXT("SetObjectProperties_CustomHlsl"),true))return false;
 W.FreshUnlinkedHlslNodes.Remove(Node);auto Row=W.MutationRow(TEXT("SetObjectProperties_CustomHlsl"));Row->SetStringField(TEXT("node_path"),Node->GetPathName());Row->SetStringField(TEXT("node_guid"),Node->NodeGuid.ToString());Row->SetNumberField(TEXT("expected_pin_count"),Selected.Pins.Num());
 {
  TGuardValue<UNiagaraGraph*> Mutable(W.TransitionGraph,G);
  Ok=UToolsetLibrary::SetObjectProperties(Node,Json(Values),Written,EBypassContainerCheck::No);
  Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("property_set_returned"),Ok);W.Prefix->SetBoolField(TEXT("HLSL_property_set_returned"),Ok);W.R.bChanged|=Ok;
  if(!W.Admit(TEXT("after_HLSL_property_context")))return false;
  if(!Live(G)||G->Schema!=Old.Schema.Get()||G->Nodes.Num()!=Old.Nodes.Num()||G->Nodes.Num()>16384||!Live(Node)||Node->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass()||Node->GetOuter()!=G||Node->GetGraph()!=G||Node->GetFlags()!=NodeFlags||Node->ScriptUsage!=Usage||!Node->IsIn(W.System.Get())||Node->GetPackage()!=W.System->GetPackage()||Node->GetOutermost()!=W.System->GetOutermost())return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_identity_changed"),TEXT("Fresh HLSL or graph identity changed during the property callback."));
  for(int32 I=0;I<Old.Nodes.Num();++I)
  {
   const auto& E=Old.Nodes[I];auto* CurrentNode=G->Nodes[I].Get();
   if(!Live(CurrentNode)||CurrentNode!=E.Node.Get()||CurrentNode->GetGraph()!=G||CurrentNode->NodeGuid!=E.Id||CurrentNode->NodePosX!=E.X||CurrentNode->NodePosY!=E.Y||CurrentNode->Pins.Num()!=E.Pins.Num()||!FunctionFieldsExact(E,CurrentNode))return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_node_delta"),TEXT("Property edit changed node order/identity/signature or pin count beyond exact reconstruction."));
   if(CurrentNode!=Node)
   {
    for(int32 J=0;J<E.Pins.Num();++J){auto* P=CurrentNode->Pins[J];const auto& Prior=E.Pins[J];if(P!=Prior.Pointer||!P||P->GetOwningNodeUnchecked()!=CurrentNode||!PinHeaderSame(Prior,P)||P->LinkedTo!=Prior.Links)return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_unrelated_delta"),TEXT("An unrelated pin or link changed during HLSL reconstruction."));}
    continue;
   }
   // Old pin pointers may already be garbage. Match captured values to current
   // members by exact PinId; never dereference an old pointer after the SDK.
   TSet<FGuid> NewIds;
   for(auto* P:CurrentNode->Pins)
   {
    if(!P||P->GetOwningNodeUnchecked()!=Node||!P->PinId.IsValid()||NewIds.Contains(P->PinId)||P->bOrphanedPin||P->IsPendingKill()||!P->LinkedTo.IsEmpty())return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_pin_invalid"),TEXT("Rebuilt HLSL pins must be unique, live, non-orphaned and unlinked current members."));
    const auto* Prior=Selected.Pins.FindByPredicate([&](const auto& OldPin){return OldPin.Id==P->PinId;});
    if(!Prior||!PinHeaderSame(*Prior,P))return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_pin_value_changed"),TEXT("Rebuilt pin differs from captured exact GUID/name/direction/type/default/flags."));
    NewIds.Add(P->PinId);
   }
   if(NewIds.Num()!=OldIds.Num())return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_not_bijective"),TEXT("Rebuilt HLSL pins do not form the exact captured one-to-one inventory."));
  }
  TArray<FMetaObservation> MetadataAfter;if(!MetadataSnapshot(W,G,MetadataAfter))return false;
  if(MetadataAfter.Num()!=MetadataBefore.Num())return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_metadata_changed"),TEXT("Metadata inventory changed during HLSL body edit."));
  for(const auto& E:MetadataBefore){const auto* Now=MetadataAfter.FindByPredicate([&](const auto& V){return V.Variable==E.Variable;});if(!Now||Now->Object!=E.Object||!Now->State.Equals(E.State,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_scratch_stack_HLSL_reconstruction_metadata_changed"),TEXT("Unrelated typed metadata/default changed during HLSL body edit."));}
  if(!W.Admit(TEXT("HLSL_reconstruction_before_capture")))return false;
  W.Graphs.RemoveAll([&](const auto& E){return E.Graph.Get()==G;});if(!W.CaptureGraph(Source.Get()))return false;
  Row->SetBoolField(TEXT("fresh_unlinked_pin_reconstruction_verified"),true);W.Prefix->SetBoolField(TEXT("fresh_HLSL_pin_reconstruction_verified"),true);
 }
 if(!W.After(TEXT("SetObjectProperties_CustomHlsl")))return false;
 const FString Actual=GetNiagaraCustomHlslString(Node);
 return ContinueRead()&&Ok&&Written.Contains(FName(TEXT("CustomHlsl")))&&Actual.Equals(Body,ESearchCase::CaseSensitive)||W.Fail(TEXT("niagara_scratch_stack_HLSL_body_readback_failed"),TEXT("Exact public HLSL body readback differs from requested expression."));
}
bool ExclusiveReplacement(FWork& W,UEdGraphNode* Node,UEdGraphPin* ExpectedConsumer)
{
 if(!W.Own(Node)||!W.Admit(TEXT("exclusive_old_node_preflight")))return false;for(auto* P:Node->Pins)if(P->Direction==EGPD_Output)for(auto* L:P->LinkedTo)if(L!=ExpectedConsumer)return W.Fail(TEXT("niagara_scratch_stack_shared_old_node_unsupported"),TEXT("Old node has another consumer; no destructive replacement."));return true;
}

TSharedPtr<FJsonObject> HandleWrap(FWork& W,const TSharedPtr<FJsonObject>& Params)
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

    if(!GRPathAdmission(W,Params,SystemPath))return W.Error(W.R.Error);
    FString TargetPinName = TEXT("StackContext.RGBA");
    Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName);
    Params->TryGetStringField(TEXT("input_pin_name"), TargetPinName);
    TargetPinName.TrimStartAndEndInline();
    if (TargetPinName.IsEmpty())
    {
        return W.Error(TEXT("'target_pin_name' cannot be empty"));
    }

    FString LocalValuePinName = TEXT("LocalStampValue");
    Params->TryGetStringField(TEXT("local_value_pin_name"), LocalValuePinName);
    LocalValuePinName.TrimStartAndEndInline();
    if (LocalValuePinName.IsEmpty())
    {
        return W.Error(TEXT("'local_value_pin_name' cannot be empty"));
    }

    FString PreviousStackValuePinName = TEXT("PreviousStackValue");
    Params->TryGetStringField(TEXT("previous_stack_value_pin_name"), PreviousStackValuePinName);
    PreviousStackValuePinName.TrimStartAndEndInline();
    if (PreviousStackValuePinName.IsEmpty())
    {
        return W.Error(TEXT("'previous_stack_value_pin_name' cannot be empty"));
    }

    FString Expression = TEXT("max(PreviousStackValue, LocalStampValue)");
    Params->TryGetStringField(TEXT("expression"), Expression);
    Expression.TrimStartAndEndInline();
    if (Expression.IsEmpty())
    {
        return W.Error(TEXT("'expression' cannot be empty"));
    }

    bool bSkipIfAlreadyWrapped = true;
    Params->TryGetBoolField(TEXT("skip_if_already_wrapped"), bSkipIfAlreadyWrapped);

    bool bReplaceExistingCustom = true;
    Params->TryGetBoolField(TEXT("replace_existing_custom"), bReplaceExistingCustom);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    FString TargetNodeGuid;
    Params->TryGetStringField(TEXT("target_node_guid"), TargetNodeGuid);
    Params->TryGetStringField(TEXT("parameter_map_set_node_guid"), TargetNodeGuid);

    int32 TargetNodeIndex = INDEX_NONE;
    TargetNodeIndex=IndexParam(W,Params,TEXT("target_node_index"));
    TargetNodeIndex=IndexParam(W,Params,TEXT("parameter_map_set_node_index"),TargetNodeIndex);

    int32 ParameterMapSetIndexSelector = INDEX_NONE;
    ParameterMapSetIndexSelector=IndexParam(W,Params,TEXT("parameter_map_set_index"));


 FScratchSelection S;if(!ResolveScratch(W,Params,SystemPath,S,TEXT("RenderCircleToGrid"),false))return W.Error(W.R.Error);auto* System=S.System;auto* ScratchPadScript=S.Script;auto* ScriptSource=S.Source;auto* Graph=S.Graph;const FString ResolvedScratchPadOwnerKind=S.OwnerKind,ResolvedScratchPadOwnerName=S.OwnerName;const int32 ResolvedScratchPadEmitterIndex=S.EmitterIndex,ResolvedScratchPadScriptIndex=S.ScriptIndex;
    UEdGraphNode* MatchedSetNode = nullptr;
    UEdGraphPin* TargetPin = nullptr;
    int32 MatchedGraphNodeIndex = INDEX_NONE;
    int32 MatchedParameterMapSetIndex = INDEX_NONE;
    int32 MatchCount = 0;
    int32 GraphNodeIndex = 0;
    int32 ParameterMapSetIndex = 0;
    const auto SelectedNodes=Graph->Nodes;
    for (UEdGraphNode* Node : SelectedNodes)
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
        return W.Error(TEXT("Refusing to wrap the ParameterMapSet Source pin; choose a value input pin instead."));
    }

    UEdGraphPin* SourcePin = NamedGraphPin(W,MatchedSetNode, EGPD_Input, FName(TEXT("Source")));
    if (!SourcePin || SourcePin->LinkedTo.Num() != 1)
    {
        return W.Error(
            FString::Printf(
                TEXT("Target ParameterMapSet node '%s' must have exactly one linked Source pin before wrapping. Found: %d"),
                *MatchedSetNode->GetName(),
                SourcePin ? SourcePin->LinkedTo.Num() : 0));
    }

    W.Before=W.Projection();if(!ContinueRead())return W.Error(W.R.Error);
    UEdGraphPin* PreviousLocalValuePin = nullptr;
    UNiagaraNodeCustomHlsl* ExistingCustomNodeToReplace = nullptr;
    UEdGraphNode* ExistingParameterMapGetNodeToReplace = nullptr;
    const int32 PreviousLinkCount = TargetPin->LinkedTo.Num();
    if (PreviousLinkCount == 1)
    {
        if (UNiagaraNodeCustomHlsl* ExistingCustomNode = Cast<UNiagaraNodeCustomHlsl>(TargetPin->LinkedTo[0]->GetOwningNode()))
        {
            const FString ExistingExpression = GetNiagaraCustomHlslString(ExistingCustomNode).TrimStartAndEnd();if(!ContinueRead())return W.Error(W.R.Error);
            UEdGraphPin* ExistingPreviousStackValuePin = NamedGraphPin(W,ExistingCustomNode, EGPD_Input, FName(*PreviousStackValuePinName));
            UEdGraphPin* ExistingLocalValuePin = NamedGraphPin(W,ExistingCustomNode, EGPD_Input, FName(*LocalValuePinName));
            if (bSkipIfAlreadyWrapped && ExistingExpression.Equals(Expression, ESearchCase::CaseSensitive) && ExistingPreviousStackValuePin && ExistingLocalValuePin)
            {
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("success"), true);
                Result->SetBoolField(TEXT("already_wrapped"), true);
                Result->SetStringField(TEXT("system_path"), System->GetPathName());
                Result->SetStringField(TEXT("scratch_pad_script_path"), ScratchPadScript->GetPathName());
                Result->SetStringField(TEXT("scratch_pad_script_name"), ScratchPadScript->GetName());
                Result->SetStringField(TEXT("parameter_map_set_node_name"), MatchedSetNode->GetName());
                Result->SetStringField(TEXT("parameter_map_set_node_guid"), MatchedSetNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
                Result->SetStringField(TEXT("target_pin_name"), TargetPin->PinName.ToString());
                Result->SetStringField(TEXT("custom_hlsl_node_guid"), ExistingCustomNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
                Result->SetStringField(TEXT("expression"), ExistingExpression);
                Result->SetNumberField(TEXT("new_link_count"), TargetPin->LinkedTo.Num());
                Result->SetArrayField(TEXT("new_linked_sources"), LinkedPinsToJson(TargetPin, true));
                Result->SetBoolField(TEXT("compile_requested"), false);
                Result->SetBoolField(TEXT("saved"), false);
                Result->SetStringField(TEXT("write_scope"), TEXT("scratch_pad_output_stack_context_accumulator"));
                W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;
            }

            if (!bReplaceExistingCustom)
            {
                return W.Error(
                    FString::Printf(TEXT("Scratch Pad pin '%s' is already linked to a Custom HLSL node with a different expression."), *TargetPin->PinName.ToString()));
            }
            if (!ExistingLocalValuePin || ExistingLocalValuePin->LinkedTo.Num() != 1)
            {
                return W.Error(
                    FString::Printf(TEXT("Existing Custom HLSL node for '%s' does not expose exactly one '%s' input link to preserve."),
                        *TargetPin->PinName.ToString(),
                        *LocalValuePinName));
            }

            ExistingCustomNodeToReplace = ExistingCustomNode;
            PreviousLocalValuePin = ExistingLocalValuePin->LinkedTo[0];
            if (ExistingPreviousStackValuePin && ExistingPreviousStackValuePin->LinkedTo.Num() == 1)
            {
                UEdGraphNode* ExistingPreviousStackNode = ExistingPreviousStackValuePin->LinkedTo[0]->GetOwningNode();
                if (ExistingPreviousStackNode && ExistingPreviousStackNode->GetClass()->GetName().Equals(TEXT("NiagaraNodeParameterMapGet")))
                {
                    ExistingParameterMapGetNodeToReplace = ExistingPreviousStackNode;
                }
            }
        }
        else
        {
            PreviousLocalValuePin = TargetPin->LinkedTo[0];
        }
    }
    if (PreviousLinkCount != 1)
    {
        return W.Error(
            FString::Printf(TEXT("Scratch Pad pin '%s' must have exactly one existing value link to wrap. Found: %d"),
                *TargetPin->PinName.ToString(),
                PreviousLinkCount));
    }

    const FNiagaraTypeDefinition TargetType = UEdGraphSchema_Niagara::PinToTypeDefinition(TargetPin);
    if (!TargetType.IsValid())
    {
        return W.Error(FString::Printf(TEXT("Unable to resolve Niagara type for Scratch Pad pin: %s"), *TargetPin->PinName.ToString()));
    }

    UEdGraphPin* PreviousStackNodeOutputPin = SourcePin->LinkedTo[0];
    const FString PreviousDefaultValue = TargetPin->DefaultValue;
    const FString PreviousDefaultObject = NiagaraObjectPathOrEmpty(TargetPin->DefaultObject);
    const TArray<TSharedPtr<FJsonValue>> PreviousLinkedSources = LinkedPinsToJson(TargetPin, true);
    if (!PreviousLocalValuePin)
    {
        return W.Error(TEXT("Unable to resolve the previous local value pin to wrap"));
    }


 if(!ContinueRead()||!FindExpectedPin(W,PreviousLocalValuePin)||!FindExpectedPin(W,PreviousStackNodeOutputPin))return W.Error(W.R.Error);if(TargetType.IsDataInterface()||TargetType.IsUObject()||TargetType==FNiagaraTypeDefinition::GetParameterMapDef()||TargetType==FNiagaraTypeDefinition::GetGenericNumericDef())return W.Fail(TEXT("niagara_scratch_stack_HLSL_type_unsupported"),TEXT("Concrete value type required for accumulator; opaque/generic target is unsupported before writer.")),W.Error(W.R.Error);
 if(LocalValuePinName==PreviousStackValuePinName||LocalValuePinName.Equals(TEXT("Map"),ESearchCase::IgnoreCase)||PreviousStackValuePinName.Equals(TEXT("Map"),ESearchCase::IgnoreCase))return W.Error(TEXT("Accumulator input names must be distinct from each other and Map."));if(ExistingCustomNodeToReplace){if(!ExclusiveReplacement(W,ExistingCustomNodeToReplace,TargetPin)||PreviousLocalValuePin->GetOwningNodeUnchecked()==ExistingCustomNodeToReplace)return W.Error(W.R.Error);if(ExistingParameterMapGetNodeToReplace){auto* OldPrevious=NamedGraphPin(W,ExistingCustomNodeToReplace,EGPD_Input,FName(*PreviousStackValuePinName));if(!OldPrevious||!ExclusiveReplacement(W,ExistingParameterMapGetNodeToReplace,OldPrevious)||PreviousLocalValuePin->GetOwningNodeUnchecked()==ExistingParameterMapGetNodeToReplace)return W.Error(TEXT("Old getter/local source is not independently preservable."));}}
 FProperty* BodyField=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));if(!CastField<FStrProperty>(BodyField)||BodyField->ArrayDim!=1||EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(BodyField),EPropertyAccessResultFlags::PermissionDenied)||EnumHasAnyFlags(PropertyAccessUtil::CanSetPropertyValue(BodyField,PropertyAccessUtil::EditorReadOnlyFlags,false),EPropertyAccessResultFlags::PermissionDenied))return W.Error(TEXT("Public HLSL property permission is unavailable before writer."));UClass* GetClass=nullptr;if(!W.Step(TEXT("LoadClass_WrapMapGet"),false,[&]{GetClass=StaticLoadClass(UEdGraphNode::StaticClass(),nullptr,TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"));}))return W.Error(W.R.Error);if(!GetClass||!GetClass->GetPathName().Equals(TEXT("/Script/NiagaraEditor.NiagaraNodeParameterMapGet"),ESearchCase::CaseSensitive))return W.Error(TEXT("Exact stock MapGet unavailable."));
 if(!W.Modify(System)||!W.Modify(ScratchPadScript)||!W.Modify(ScriptSource)||!W.Modify(Graph)||!W.Modify(MatchedSetNode))return W.Error(W.R.Error);FNiagaraFunctionSignature Signature;Signature.Name=TEXT("IF_StackContextAccumulator");Signature.bRequiresContext=true;Signature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(),TEXT("Map")));Signature.Inputs.Add(FNiagaraVariable(TargetType,FName(*PreviousStackValuePinName)));Signature.Inputs.Add(FNiagaraVariable(TargetType,FName(*LocalValuePinName)));Signature.Outputs.Add(FNiagaraVariable(TargetType,TEXT("CustomHLSLOutput")));
 UEdGraphNode* MapGetNode=CreateNode(W,Graph,GetClass,MatchedSetNode->NodePosX-640,MatchedSetNode->NodePosY+40);if(!MapGetNode)return W.Error(W.R.Error);auto* CustomNode=Cast<UNiagaraNodeCustomHlsl>(CreateNode(W,Graph,UNiagaraNodeCustomHlsl::StaticClass(),MatchedSetNode->NodePosX-320,MatchedSetNode->NodePosY+160,&Signature));if(!CustomNode||!SetHlslPublic(W,Graph,CustomNode,Expression))return W.Error(W.R.Error);
 auto* CustomMapPin=NamedGraphPin(W,CustomNode,EGPD_Input,TEXT("Map"));auto* CustomPreviousStackValuePin=NamedGraphPin(W,CustomNode,EGPD_Input,FName(*PreviousStackValuePinName));auto* CustomLocalValuePin=NamedGraphPin(W,CustomNode,EGPD_Input,FName(*LocalValuePinName));auto* CustomOutputPin=NamedGraphPin(W,CustomNode,EGPD_Output,TEXT("CustomHLSLOutput"));if(!CustomMapPin||!CustomPreviousStackValuePin||!CustomLocalValuePin||!CustomOutputPin)return W.Error(TEXT("Actual accumulator signature pins missing."));FNiagaraVariable PreviousVariable(TargetType,FName(*TargetPinName));if(!TargetType.IsDataInterface()&&!TargetType.IsUObject()){PreviousVariable.AllocateData();if(PreviousVariable.IsDataAllocated())FMemory::Memzero(PreviousVariable.GetData(),PreviousVariable.GetSizeInBytes());}FString DefaultText;if(PreviousVariable.IsDataAllocated()&&!GRDefaultString(W,PreviousVariable,DefaultText))return W.Error(W.R.Error);FModuleRead PreviousRead;if(!SpecializeModuleRead(W,Graph,MapGetNode,CustomPreviousStackValuePin,PreviousVariable,DefaultText,PreviousRead))return W.Error(W.R.Error);auto* MapGetSourcePin=UniqueMapPin(W,MapGetNode,EGPD_Input);auto* MapGetOutputPin=PreviousRead.Output;if(!MapGetSourcePin||!W.BreakLinks(TargetPin,true))return W.Error(W.R.Error);
 if(ExistingCustomNodeToReplace&&!W.RemoveNode(Graph,ExistingCustomNodeToReplace,true,false))return W.Error(W.R.Error);if(ExistingParameterMapGetNodeToReplace&&!W.RemoveNode(Graph,ExistingParameterMapGetNodeToReplace,true,false))return W.Error(W.R.Error);FGraphEditPlan L1,L2,L3,L4;if(!SchemaLink(W,Graph,PreviousStackNodeOutputPin,MapGetSourcePin,L1)||!SchemaLink(W,Graph,PreviousStackNodeOutputPin,CustomMapPin,L2)||!SchemaLink(W,Graph,PreviousLocalValuePin,CustomLocalValuePin,L3)||!SchemaLink(W,Graph,CustomOutputPin,TargetPin,L4))return W.Error(W.R.Error);
 if(!W.Step(TEXT("Wrap_NotifyGraphChanged"),true,[&]{static_cast<UEdGraph*>(Graph)->NotifyGraphChanged();})||!W.Dirty())return W.Error(W.R.Error);if(bRequestCompile){bool Accepted=false;if(!W.Request(Accepted))return W.Error(W.R.Error);}bool bSaved=false;if(bSave){bSaved=W.Save(false);if(!bSaved)return W.Error(W.R.Error);}const FString ActualBody=GetNiagaraCustomHlslString(CustomNode);if(!ContinueRead()||!ActualBody.Equals(Expression,ESearchCase::CaseSensitive)||!FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&CustomNode->Signature,&Signature,0)||!ModuleReadVerified(W,Graph,PreviousRead,PreviousVariable,DefaultText,CustomPreviousStackValuePin)||CustomMapPin->LinkedTo!=TArray<UEdGraphPin*>{PreviousStackNodeOutputPin}||CustomLocalValuePin->LinkedTo!=TArray<UEdGraphPin*>{PreviousLocalValuePin}||TargetPin->LinkedTo!=TArray<UEdGraphPin*>{CustomOutputPin}||MapGetSourcePin->LinkedTo!=TArray<UEdGraphPin*>{PreviousStackNodeOutputPin})return W.Error(TEXT("Final accumulator signature/body/typed pair/reciprocal links failed after compile/save."));
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("already_wrapped"), false);
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
    Result->SetStringField(TEXT("input_type"), NiagaraTypeName(TargetType));
    Result->SetStringField(TEXT("previous_stack_value_pin_name"), PreviousStackValuePinName);
    Result->SetStringField(TEXT("local_value_pin_name"), LocalValuePinName);
    Result->SetStringField(TEXT("expression"), Expression);
    Result->SetStringField(TEXT("parameter_map_get_node_guid"), MapGetNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("custom_hlsl_node_guid"), CustomNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("previous_default_value"), PreviousDefaultValue);
    Result->SetStringField(TEXT("previous_default_object"), PreviousDefaultObject);
    Result->SetNumberField(TEXT("previous_link_count"), PreviousLinkCount);
    Result->SetNumberField(TEXT("broken_link_count"), PreviousLinkCount);
    Result->SetNumberField(TEXT("new_link_count"), TargetPin->LinkedTo.Num());
    Result->SetArrayField(TEXT("previous_linked_sources"), PreviousLinkedSources);
    Result->SetArrayField(TEXT("new_linked_sources"), LinkedPinsToJson(TargetPin, true));
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("scratch_pad_output_stack_context_accumulator"));
    W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return Result;

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
    if(!LeafSuccess&&W.R.Error.IsEmpty()){FString Message,Code;if(Legacy){Legacy->TryGetStringField(TEXT("error"),Message);Legacy->TryGetStringField(TEXT("error_code"),Code);}W.Fail(Code.IsEmpty()?TEXT("niagara_scratch_stack_request_failed"):*Code,Message.IsEmpty()?TEXT("Original resource binding request failed; see legacy_result."):Message);}
    if(LeafSuccess&&(!Typed||!Complete)&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_scratch_stack_postcondition_incomplete"),TEXT("Dedicated typed readback and complete before/after target projections are required; partial state retained."));
    if(!W.SaveHealth&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_scratch_stack_save_failed"),TEXT("Current SDK save failed or returned with dirty target; previous true save history retained."));
    W.R.bSuccess=LeafSuccess&&Typed&&Complete&&W.SaveHealth&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;
    const bool Scoped=W.R.bSuccess&&Complete;if(Scoped)W.R.bChangeStateKnown=true;
    W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());
    W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);
    W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.Saves);W.R.Data->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);W.R.Data->SetArrayField(TEXT("actual_item_prefix"),W.Partial);W.R.Data->SetArrayField(TEXT("actual_sdk_prefix"),W.MutationRows);
    W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),Complete);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),Scoped);
    W.R.Data->SetStringField(TEXT("scoped_known_scope"),TEXT("Dedicated typed Module output/default pair, metadata default, assignment target/default/stack splice or HLSL body/signature/reciprocal edge readback plus complete bounded before/after and final admission. Public PersistentGuid and default-pair flags are fixed after engine creation. Private pair storage is not independently inspected. SDK global purity, shader execution and compilation completion remain outside this scope."));
    W.R.Data->SetBoolField(TEXT("protected_pair_storage_independently_read"),false);W.R.Data->SetBoolField(TEXT("required_compile_wait_implemented"),W.DeferredMode);W.R.Data->SetBoolField(TEXT("automatic_failure_cleanup_implemented"),false);
    W.R.Data->SetBoolField(TEXT("partial_graph_residual_possible"),!W.R.bSuccess&&W.R.bMutationAttempted);
    W.R.Data->SetStringField(TEXT("failure_cleanup_scope"),TEXT("New nodes/metadata or completed edge edits may remain after later failure; exact creation/mutation prefix is retained. Exclusive old-node replacement uses public guarded removal. Original eager cleanup/rollback equivalence is not claimed; unsafe failure never triggers cleanup, delete or replay."));
    W.R.Data->SetStringField(TEXT("compile_scope"),W.DeferredMode?TEXT("Original ModuleInput core edits/default-pair/consumer/refreshed calls -> RequestCompile -> finite owned GT wait -> optional save -> original typed final verification. Busy Poll then guarded current status then deadline; timeout remains legacy diagnostic. RequestCompile acceptance and bounded wait completion do not prove compile success; compiled remains null after request. No global pump/Sleep/flush/replay; individual SDK costs may block internally."):TEXT("Synchronous ModuleInput genuine wait remains prewriter unsupported when requested or pending. Assignment raw has no wait3: its existing compile/save precede Python-owned follow-up status; Wrap has no added wait. No wait=False substitution."));
    W.R.Data->SetStringField(TEXT("save_scope"),TEXT("Input save/request_compile intent is recorded immediately after valid JSON parse and differs from actual attempted/accepted/saved history, including duplicate/no-op or prewriter failures. SaveLoadedAsset true is OR history preserved before post-Guard and dirty checks. No dirty flag is cleared after save. Cold persistence and SDK callback atomicity remain unverified."));
    W.R.Data->SetBoolField(TEXT("atomic_asset_or_undo_rollback_verified"),false);
    W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;
    return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,FHandler Handler)
{
    FWork W;
    if(!IsInGameThread()){W.Fail(TEXT("niagara_scratch_stack_game_thread_required"),TEXT("Editor game thread required; no SDK dispatch."));return Finish(W,W.Error(W.R.Error),Operation);}
    if(Running||DeferredLeaseActive){W.Fail(TEXT("niagara_scratch_stack_reentrant_request"),TEXT("Dedicated Niagara resource binding request already active; no nested dispatch or replay."));return Finish(W,W.Error(W.R.Error),Operation);}
    TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);TSharedPtr<FJsonObject> Params;
    if(!ParseParams(Text,Params)){W.Fail(TEXT("niagara_scratch_stack_invalid_params_json"),TEXT("Finite JSON object within 1Mi UTF16/depth64/262144 tokens and visits required; duplicate case-sensitive keys and embedded NUL are rejected."));return Finish(W,W.Error(W.R.Error),Operation);}
    bool SaveRequested=true,CompileRequested=true;Params->TryGetBoolField(TEXT("save"),SaveRequested);Params->TryGetBoolField(TEXT("request_compile"),CompileRequested);W.R.bSaveRequested=SaveRequested;W.R.bCompileRequested=CompileRequested;
    W.R.Data->SetObjectField(TEXT("requested_params"),MakeShared<FJsonObject>(*Params));
    if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);
    return Finish(W,Handler(W,Params),Operation);
}
}

FString AddModuleInput(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("add_niagara_scratch_pad_module_input"),&HandleModuleInput);}
FString AddAssignmentModule(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("add_niagara_scratch_pad_assignment_module_to_stack"),&HandleAssignment);}
FString WrapStackContext(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("wrap_niagara_scratch_pad_output_with_stack_context"),&HandleWrap);}
}

namespace Cubeless::UE58::NiagaraScratchStackAuthoringAsync
{
using namespace Cubeless::UE58::NiagaraScratchStackAuthoring;
namespace
{
std::atomic<bool>& Stopping=DeferredStopping;
TSharedPtr<FJobState> ActiveJob;
TMap<const UCubelessNiagaraScratchStackAuthoringJob*,FString> SnapshotCache;
TMap<const UCubelessNiagaraScratchStackAuthoringJob*,TWeakObjectPtr<UCubelessNiagaraScratchStackAuthoringJob>> CacheOwners;
// Unsafe records retain exact target/Job holds; Shutdown never drops them inside SDK calls or GC.
TArray<TSharedPtr<FJobState>>& UnsafeRecords(){static auto* Records=new TArray<TSharedPtr<FJobState>>();return *Records;}
enum class EPhase:uint8 { Prepare, Continue, Final, Terminal, Quarantined };
}
struct FJobState:public TSharedFromThis<FJobState>
{
 FWork Work;
 TStrongObjectPtr<UCubelessNiagaraScratchStackAuthoringJob> Job{nullptr};
 UCubelessNiagaraScratchStackAuthoringJob* RawJob=nullptr;
 TSharedPtr<FJsonObject> Params,Legacy;
 FString Operation,Id,LastSnapshot;
 EPhase Phase=EPhase::Prepare;
 FTSTicker::FDelegateHandle Ticker;
 FDelegateHandle PreExit;
 uint64 LastFrame=MAX_uint64;
 int64 Revision=0;
 bool Executing=false,Dispatched=false,FinalEncoded=false;
 explicit FJobState(const TCHAR* Op):Operation(Op),Id(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens))
 {Work.DeferredMode=true;Work.R.Data->SetBoolField(TEXT("async_scratch_stack_continuation"),true);}
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
  D->SetStringField(TEXT("continuation_scope"),TEXT("Exact existing ModuleInput core on finite GT ticks. Optional compile precedes actual bounded wait then optional save and original typed/default-pair/consumer final verification. Existing pending is observed when request_compile is false. Bounded timeout is diagnostic, not compiled success. No global pump/Sleep/replay; SDK calls may block internally."));
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
  if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_scratch_stack_async_context_quarantined"),Reason);
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
  if(!PublicationSafe()){Quarantine(TEXT("Editor/world/GC/exit unavailable while publishing pending Scratch module-input snapshot."));return;}
  Cache(Operations::EncodeVariableResult(Work.R,*Operation),false,true);
 }
 void SetFailureLegacy()
 {
  if(!Legacy)Legacy=MakeShared<FJsonObject>();
  Legacy->SetBoolField(TEXT("success"),false);
  Legacy->SetStringField(TEXT("error"),Work.R.Error.IsEmpty()?TEXT("ScratchStackAuthoring job stopped; actual prefix retained."):Work.R.Error);
 }
 void Finalize()
 {
  if(FinalEncoded||Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("ScratchStackAuthoring terminal publication lost its exact Editor world; no recovery or replay."));return;}
  if(!Work.R.Error.IsEmpty()){RefreshLegacy();SetFailureLegacy();}
  Metadata(true);
  LastSnapshot=Finish(Work,Legacy,*Operation);
  FinalEncoded=true;
  if(!PublicationSafe()){FinalEncoded=false;Quarantine(TEXT("ScratchStackAuthoring final guard lost publication context."));return;}
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
   if(!Work.CancelRequested){Work.CancelRequested=true;Work.CancelReason=TEXT("Module shutdown stopped the owned Scratch module-input job.");}
   Work.Fail(TEXT("niagara_scratch_stack_cancelled"),Work.CancelReason);
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
   if(Dispatched){Work.Fail(TEXT("niagara_scratch_stack_async_replay_forbidden"),TEXT("Original operation already dispatched."));Phase=EPhase::Final;Finalize();return;}
   Dispatched=true;Legacy=HandleModuleInput(Work,Params);if(Legacy.IsValid())Work.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
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
  if(!PublicationSafe()){Quarantine(TEXT("ScratchStackAuthoring ticker lost live Editor/game-thread/world context."));return false;}
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
  PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([Weak](){Stopping.store(true);if(!IsInGameThread())return;auto State=Weak.Pin();if(State.IsValid()){State->Work.CancelRequested=true;State->Work.CancelReason=TEXT("Engine pre-exit stopped Scratch module-input job.");State->Unhook();if(!State->Executing)State->Quarantine(State->Work.CancelReason);}});
 }
};
namespace
{
void PruneSnapshots()
{
 for(auto It=CacheOwners.CreateIterator();It;++It)if(!It.Value().IsValid()){SnapshotCache.Remove(It.Key());It.RemoveCurrent();}
}
UCubelessNiagaraScratchStackAuthoringJob* Start(const FString& Text,const TCHAR* Operation)
{
 if(!IsInGameThread()||Stopping.load()||Running||DeferredLeaseActive||!Safe())return nullptr;
 auto State=MakeShared<FJobState>(Operation);
 State->Work.R.bSaveRequested=true;
 if(!State->Work.Admit(TEXT("async_scratch_stack_start_admission")))return nullptr;
 PruneSnapshots();
 if(SnapshotCache.Num()>=256)return nullptr;
 ActiveJob=State;DeferredLeaseActive=true;
 UCubelessNiagaraScratchStackAuthoringJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(State->Executing,true);
  TGuardValue<bool> CoreLease(Running,true);
  TGuardValue<FWork*> Context(Current,&State->Work);
  if(State->Work.SDK(TEXT("NewObject_ScratchStackAuthoringJob"),true))
  {
   Created=NewObject<UCubelessNiagaraScratchStackAuthoringJob>(GetTransientPackage());
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_returned"),true);
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
   State->RawJob=Created;
   if(Stopping.load()||State->Work.CancelRequested||!State->PublicationSafe())
   {
    State->Quarantine(TEXT("ScratchStackAuthoring Job allocation interrupted; persistent Scratch module-input dispatch not entered."));
    return nullptr;
   }
   if(!State->Work.After(TEXT("NewObject_ScratchStackAuthoringJob"))||!Created)
   {
    State->Quarantine(TEXT("ScratchStackAuthoring Job allocation could not be safely adopted."));
    return nullptr;
   }
   State->Job.Reset(Created);CacheOwners.Add(Created,Created);
   if(!ParseParams(Text,State->Params))
   {
    State->Work.Fail(TEXT("niagara_scratch_stack_invalid_params_json"),TEXT("Finite JSON object within1Mi/depth64/262144 slots required; raw presence/defaults are preserved."));
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
UCubelessNiagaraScratchStackAuthoringJob* StartModuleInput(const FString& ParamsJson){return Start(ParamsJson,TEXT("add_niagara_scratch_pad_module_input"));}
FString SnapshotJson(const UCubelessNiagaraScratchStackAuthoringJob* Job)
{
 if(!IsInGameThread()||!Job)return FString();
 if(const FString* Snapshot=SnapshotCache.Find(Job))return *Snapshot;
 return FString();
}
bool Cancel(UCubelessNiagaraScratchStackAuthoringJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!Job||!ActiveJob.IsValid()||ActiveJob->RawJob!=Job)return false;
 auto State=ActiveJob;
 if(State->Phase==EPhase::Terminal||State->Phase==EPhase::Quarantined)return false;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return true;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Cancellation lost Scratch module-input publication context; actual prefix retained."));return true;}
 {
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> CoreLease(Running,true);TGuardValue<FWork*> Context(Current,&State->Work);
  State->Work.Fail(TEXT("niagara_scratch_stack_async_cancelled"),State->Work.CancelReason);
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
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe Scratch module-input state without touching UObject publication."));return;}
 Cancel(State->RawJob,State->Work.CancelReason);
}
}
