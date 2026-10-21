// GG Editor-only Niagara collection authoring. Source candidate, not Engine/build evidence.
#include "Compatibility/UE58/CubelessNiagaraParameterCollectionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Compatibility/UE58/CubelessNiagaraScratchUserRecipeAsyncApi.h"
#include "CubelessNiagaraScratchUserRecipeJob.h"
#include "AssetCompilingManager.h"
#include "Containers/Ticker.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SavePackage.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraParameterStore.h"
#include "NiagaraDataInterface.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptVariable.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::NiagaraScratchUserRecipeAsync { bool CanPublishJobObject(); }
namespace Cubeless::UE58::NiagaraParameterCollections
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Running=false;
bool HasScriptRemapLease();
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O){FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));}return S;}
FString ValueText(const TSharedPtr<FJsonValue>& V){auto O=MakeShared<FJsonObject>();O->SetField(TEXT("value"),V.IsValid()?V:MakeShared<FJsonValueNull>());return Json(O);}
FString NormalizeNiagaraObjectPathForLoad(const FString&);
bool ForbiddenReference(const FString&);
TSharedPtr<FJsonValue> NiagaraParameterValueToJson(const FNiagaraParameterStore&,const FNiagaraVariable&);
FString VariableKey(const FNiagaraVariable& P){const auto& T=P.GetType();auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),P.GetName().ToString());O->SetStringField(TEXT("struct"),T.GetStruct()?T.GetStruct()->GetPathName():FString());O->SetStringField(TEXT("class"),T.GetClass()?T.GetClass()->GetPathName():FString());O->SetStringField(TEXT("enum"),T.GetEnum()?T.GetEnum()->GetPathName():FString());O->SetNumberField(TEXT("flags"),T.GetFlags());return Json(O);}
FString ParameterShape(const TArray<FNiagaraVariable>& Parameters)
{auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& P:Parameters){Rows.Add(MakeShared<FJsonValueString>(VariableKey(P)));}O->SetArrayField(TEXT("parameters"),Rows);return Json(O);}
TArray<FNiagaraVariable> StoreParameters(const FNiagaraParameterStore& S){TArray<FNiagaraVariable> P;S.GetParameters(P);return P;}
FString StoreShapeOf(const TArray<FNiagaraVariable>& P){TArray<FString> Keys;for(const auto& V:P){Keys.Add(VariableKey(V));}Keys.Sort([](const auto& A,const auto& B){return A.Compare(B,ESearchCase::CaseSensitive)<0;});auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& K:Keys){Rows.Add(MakeShared<FJsonValueString>(K));}O->SetArrayField(TEXT("parameters"),Rows);return Json(O);}
struct FIdentity
{
    TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer,OwnerRoot;TWeakObjectPtr<UPackage> Package;TWeakObjectPtr<UClass> Class;FString Path;bool Writer=false;
    explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Package(O->GetOutermost()),Class(O->GetClass()),Path(O->GetPathName()){}
    bool Exact()const{auto* O=Object.Get();if(!Live(O)||!Live(Package.Get())||!Live(Class.Get())||O->GetOuter()!=Outer.Get()||O->GetOutermost()!=Package.Get()||O->GetClass()!=Class.Get()||!O->GetPathName().Equals(Path,ESearchCase::CaseSensitive)){return false;}auto* Root=OwnerRoot.Get();return !Writer||(Live(Root)&&(O==Root||O->IsIn(Root))&&O->GetPackage()==Root->GetPackage()&&O->GetOutermost()==Root->GetOutermost());}
};
struct FPinIdentity{UEdGraphPin* Pointer=nullptr;FGuid Id;};
struct FNodeIdentity{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;TArray<FPinIdentity> Pins;};
struct FGraphIdentity{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNodeIdentity> Nodes;};
struct FCollectionIdentity{TWeakObjectPtr<UNiagaraParameterCollection> Collection;TWeakObjectPtr<UNiagaraParameterCollectionInstance> Instance;FString Declarations,StoreShape;TMap<FString,TSharedPtr<FJsonValue>> ExpectedValues;};
struct FScriptIdentity{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Exposed;};
struct FSystemEmitter{FGuid Id,InstanceVersion,DataVersion;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,ParentScratch;TArray<TWeakObjectPtr<UNiagaraScript>> ScratchScripts,ParentScripts,OwnedScripts;};
struct FSystemIdentity{TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<TWeakObjectPtr<UNiagaraScript>> Scratch;TArray<FSystemEmitter> Emitters;};
struct FWork
{
    FResult R;bool Writer=false,WorldCaptured=false,SaveHealthPassed=true,DeferredMode=false,WaitDeferred=false,GlobalWaitObserved=false,CancelRequested=false;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UEditorEngine> CapturedEditor;FString FailedStage,CancelReason;
    TFunction<TSharedPtr<FJsonObject>()> RemapTail;TArray<TUniquePtr<FIdentity>> GlobalSystems;int32 GlobalPumps=0,GlobalRemaining=-1;
    TArray<TUniquePtr<FIdentity>> Objects;TArray<FGraphIdentity> Graphs;TArray<FCollectionIdentity> Collections;TArray<FScriptIdentity> Scripts;TArray<FSystemIdentity> Systems;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Saves,Compiles,PartialRows;TSharedPtr<FJsonObject> Before;
    explicit FWork(bool W):Writer(W){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty()){Fail(TEXT("niagara_collection_request_failed"),Message);}auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
    bool Pin(UObject* O){if(!Live(O)){return Fail(TEXT("niagara_collection_object_unavailable"),TEXT("Expected a live loaded Engine object."));}for(const auto& I:Objects){if(I->Object.Get()==O){return I->Exact();}}Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool Own(UObject* O,UObject* Root)
    {if(!Admit(TEXT("writer_ownership"))||!Live(O)||!Live(Root)||(O!=Root&&!O->IsIn(Root))||O->GetPackage()!=Root->GetPackage()||O->GetOutermost()!=Root->GetOutermost()){return Fail(TEXT("niagara_collection_external_writer_unsupported"),TEXT("Actual writer is outside the specified asset's outer chain/package. No external graph, parent scratch/source or implicit duplicate was modified."));}if(!Pin(Root)||!Pin(O)){return false;}for(auto& I:Objects){if(I->Object.Get()==O){I->Writer=true;I->OwnerRoot=Root;}}return true;}
    bool Admit(const FString& Stage,bool ObserveFailure=false)
    {
        R.Stage=Stage;if(!R.Error.IsEmpty()&&!ObserveFailure){return false;}if(DeferredMode&&CancelRequested)return Fail(TEXT("niagara_collection_remap_cancelled"),CancelReason);
        if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||!GEditor||IsGarbageCollecting()||UE::IsSavingPackage()){return Fail(TEXT("niagara_collection_context_unavailable"),TEXT("Live Editor game thread without GC/exit/save is required; no replay."));}
        if(WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_collection_editor_changed"),TEXT("Captured Editor identity changed before further context access."));
        if(Writer&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld)){return Fail(TEXT("niagara_collection_play_session_active"),TEXT("Collection authoring requires stopped PIE/SIE."));}
        UWorld* Current=GEditor->GetEditorWorldContext().World();if(!Live(Current)||Current->bIsTearingDown){return Fail(TEXT("niagara_collection_world_unavailable"),TEXT("Editor world unavailable or tearing down."));}if(!WorldCaptured){CapturedEditor=GEditor;World=Current;WorldCaptured=true;}else if(CapturedEditor.Get()!=GEditor||World.Get()!=Current){return Fail(TEXT("niagara_collection_world_changed"),TEXT("Editor world changed across SDK dispatch."));}
        for(const auto& I:GlobalSystems){if(!I->Exact())return Fail(TEXT("niagara_collection_global_system_identity_changed"),TEXT("A captured global System expired or changed across the global wait; no local-idle substitution."));}
        for(const auto& I:Objects){if(!I->Exact()){return Fail(TEXT("niagara_collection_identity_changed"),TEXT("Captured class/outer/package/path/ownership changed; no replacement was adopted."));}}
        for(const auto& C:Collections)
        {auto* O=C.Collection.Get();auto* I=C.Instance.Get();if(!Live(O)||!Live(I)||O->GetDefaultInstance()!=I||I->GetParent()!=O||!ParameterShape(O->GetParameters()).Equals(C.Declarations,ESearchCase::CaseSensitive)||!StoreShapeOf(StoreParameters(I->GetParameterStore())).Equals(C.StoreShape,ESearchCase::CaseSensitive)){return Fail(TEXT("niagara_collection_parameter_shape_changed"),TEXT("Declaration/default-instance/store membership changed outside the admitted AddParameter."));}for(const auto& P:StoreParameters(I->GetParameterStore())){if(const auto* E=C.ExpectedValues.Find(VariableKey(P))){if(!ValueText(NiagaraParameterValueToJson(I->GetParameterStore(),P)).Equals(ValueText(*E),ESearchCase::CaseSensitive)){return Fail(TEXT("niagara_collection_typed_readback_failed"),TEXT("A requested typed store value no longer matches its actual readback."));}}}}
        for(const auto& S:Scripts){auto* O=S.Script.Get();if(!Live(O)||O->GetLatestSource()!=S.Source.Get()||O->GetExposedVersion().VersionGuid!=S.Exposed){return Fail(TEXT("niagara_collection_script_source_changed"),TEXT("Captured script exposed version/source changed."));}}
        for(const auto& G:Graphs)
        {if(!Live(G.Source.Get())||!Live(G.Graph.Get())||G.Source->NodeGraph!=G.Graph.Get()||G.Graph->Nodes.Num()!=G.Nodes.Num()){return Fail(TEXT("niagara_collection_graph_topology_changed"),TEXT("Captured source/graph/node membership changed."));}for(int32 N=0;N<G.Nodes.Num();++N){const auto& E=G.Nodes[N];auto* Node=E.Node.Get();if(!Live(Node)||G.Graph->Nodes[N]!=Node||Node->NodeGuid!=E.Id||Node->Pins.Num()!=E.Pins.Num()){return Fail(TEXT("niagara_collection_graph_topology_changed"),TEXT("Captured physical node/pin order changed."));}for(int32 P=0;P<E.Pins.Num();++P){auto* Pin=Node->Pins[P];if(Pin!=E.Pins[P].Pointer||(Pin&&(Pin->PinId!=E.Pins[P].Id||Pin->GetOwningNodeUnchecked()!=Node))){return Fail(TEXT("niagara_collection_pin_identity_changed"),TEXT("Exact captured pin disappeared or changed owner/id; stale pin was not dereferenced."));}}}}
        for(const auto& S:Systems)
        {auto* O=S.System.Get();if(!Live(O)||O->GetSystemSpawnScript()!=S.Spawn.Get()||O->GetSystemUpdateScript()!=S.Update.Get()||O->ScratchPadScripts.Num()!=S.Scratch.Num()||O->GetEmitterHandles().Num()!=S.Emitters.Num()){return Fail(TEXT("niagara_collection_system_topology_changed"),TEXT("Captured System script/emitter topology changed."));}for(int32 I=0;I<S.Scratch.Num();++I){if(O->ScratchPadScripts[I]!=S.Scratch[I].Get()){return Fail(TEXT("niagara_collection_system_topology_changed"),TEXT("System scratch script identity/order changed."));}}for(int32 I=0;I<S.Emitters.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=S.Emitters[I];if(H.GetId()!=E.Id||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.InstanceVersion||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion)){return Fail(TEXT("niagara_collection_emitter_version_changed"),TEXT("Captured physical emitter version/ref changed."));}if(E.Data){if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.ParentScratch.Get()){return Fail(TEXT("niagara_collection_emitter_source_changed"),TEXT("Emitter graph/scratch container binding changed."));}auto SameScripts=[](const auto* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& Expected){if(!C){return Expected.IsEmpty();}if(C->Scripts.Num()!=Expected.Num()){return false;}for(int32 J=0;J<Expected.Num();++J){if(C->Scripts[J]!=Expected[J].Get()){return false;}}return true;};if(!SameScripts(E.Scratch.Get(),E.ScratchScripts)||!SameScripts(E.ParentScratch.Get(),E.ParentScripts)){return Fail(TEXT("niagara_collection_emitter_source_changed"),TEXT("Emitter scratch script membership/order changed."));}TArray<UNiagaraScript*> Actual;E.Data->GetScripts(Actual,false,false);if(Actual.Num()!=E.OwnedScripts.Num()){return Fail(TEXT("niagara_collection_emitter_source_changed"),TEXT("Emitter script membership changed."));}for(int32 J=0;J<Actual.Num();++J){if(Actual[J]!=E.OwnedScripts[J].Get()){return Fail(TEXT("niagara_collection_emitter_source_changed"),TEXT("Emitter physical script order changed."));}}}}}
        return true;
    }
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage)){return false;}R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(Mutation){Prefix->SetBoolField(Stage,true);if(DeferredMode){R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),true);R.Data->SetBoolField(TEXT("transient_creation_only"),false);}}return true;}
    bool After(const TCHAR* Stage){return Admit(FString(TEXT("after_"))+Stage);}
    template<typename F>bool Step(const TCHAR* Stage,bool Mutation,F&& Body){if(!SDK(Stage,Mutation)){return false;}Body();Prefix->SetBoolField(FString(Stage)+TEXT("_SDK_returned"),true);return After(Stage);}
    template<typename T>T* Load(const FString& Path)
    {const FString Expanded=NormalizeNiagaraObjectPathForLoad(Path);if(Path.Len()>=NAME_SIZE||Expanded.Len()>=NAME_SIZE){Fail(TEXT("niagara_collection_path_bound"),TEXT("Raw and expanded object path must fit NAME_SIZE before load."));return nullptr;}if(ForbiddenReference(Path)){Fail(TEXT("niagara_collection_runtime_dependency_rejected"),TEXT("MCP/Cubeless/Toolset content references are forbidden."));return nullptr;}if(!SDK(TEXT("load_asset"))){return nullptr;}const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* O=LoadObject<T>(nullptr,*Expected);Prefix->SetBoolField(TEXT("load_asset_returned_nonnull"),O!=nullptr);if(!After(TEXT("load_asset"))){return nullptr;}if(O&&(!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Pin(O)||!Pin(O->GetOutermost()))){Fail(TEXT("niagara_collection_load_identity_unexpected"),TEXT("Loaded object does not match the normalized exact path."));return nullptr;}return O;}
    bool CaptureCollection(UNiagaraParameterCollection* O)
    {for(const auto& C:Collections){if(C.Collection.Get()==O){return true;}}auto* I=O?O->GetDefaultInstance():nullptr;if(!Live(O)||!Live(I)||I->GetParent()!=O||O->GetParameters().Num()>16384||StoreParameters(I->GetParameterStore()).Num()>16384){return Fail(TEXT("niagara_collection_default_store_unavailable"),TEXT("Default instance/store unavailable or exceeds bounded inventory."));}if(!Pin(O)||!Pin(I)){return false;}FCollectionIdentity C;C.Collection=O;C.Instance=I;C.Declarations=ParameterShape(O->GetParameters());C.StoreShape=StoreShapeOf(StoreParameters(I->GetParameterStore()));Collections.Add(MoveTemp(C));return true;}
    bool CaptureScript(UNiagaraScript* O)
    {if(!Pin(O)){return false;}for(const auto& S:Scripts){if(S.Script.Get()==O){return true;}}FScriptIdentity S;S.Script=O;S.Source=O->GetLatestSource();S.Exposed=O->GetExposedVersion().VersionGuid;Scripts.Add(S);return true;}
    bool CaptureGraph(UNiagaraScriptSourceBase* Base,UObject* Root,bool ForWrite)
    {auto* S=Cast<UNiagaraScriptSource>(Base);if(!S||!S->NodeGraph){return true;}auto* G=S->NodeGraph.Get();if(!Pin(S)||!Pin(G)||(ForWrite&&(!Own(S,Root)||!Own(G,Root)))){return false;}for(const auto& E:Graphs){if(E.Graph.Get()==G){if(E.Source.Get()!=S){return Fail(TEXT("niagara_collection_shared_graph_source_unsupported"),TEXT("Different source objects share one mutable graph. Rejected before graph mutation; every source-to-graph binding must have exact owned authority."));}if(ForWrite){for(const auto& N:E.Nodes){if(!Own(N.Node.Get(),Root)){return false;}}}return true;}}if(G->Nodes.Num()>16384){return Fail(TEXT("niagara_collection_graph_bound"),TEXT("Graph node bound exceeded."));}FGraphIdentity E;E.Source=S;E.Graph=G;int32 PinCount=0;for(UEdGraphNode* N:G->Nodes){if(!N){return Fail(TEXT("niagara_collection_null_graph_node"),TEXT("Null graph node unsupported for mutation."));}if(!Pin(N)||(ForWrite&&!Own(N,Root))){return false;}FNodeIdentity NE;NE.Node=N;NE.Id=N->NodeGuid;for(auto* P:N->Pins){if(++PinCount>262144){return Fail(TEXT("niagara_collection_graph_bound"),TEXT("Graph pin bound exceeded."));}NE.Pins.Add({P,P?P->PinId:FGuid()});}E.Nodes.Add(MoveTemp(NE));}Graphs.Add(MoveTemp(E));return true;}
    bool CaptureSystem(UNiagaraSystem* O)
    {if(!Pin(O)){return false;}FSystemIdentity S;S.System=O;S.Spawn=O->GetSystemSpawnScript();S.Update=O->GetSystemUpdateScript();for(UNiagaraScript* P:O->ScratchPadScripts){S.Scratch.Add(P);}if(O->GetEmitterHandles().Num()>4096){return Fail(TEXT("niagara_collection_system_bound"),TEXT("System emitter bound exceeded."));}for(const auto& H:O->GetEmitterHandles()){FSystemEmitter E;E.Id=H.GetId();E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Data){E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.ParentScratch=E.Data->ParentScratchPads;if(E.Scratch.IsValid()){for(UNiagaraScript* P:E.Scratch->Scripts){E.ScratchScripts.Add(P);}}if(E.ParentScratch.IsValid()){for(UNiagaraScript* P:E.ParentScratch->Scripts){E.ParentScripts.Add(P);}}TArray<UNiagaraScript*> V;E.Data->GetScripts(V,false,false);for(auto* P:V){E.OwnedScripts.Add(P);}if(!Pin(E.Emitter.Get())){return false;}}S.Emitters.Add(E);}Systems.Add(MoveTemp(S));return true;}
    bool Modify(UObject* O){return Step(TEXT("Modify"),true,[&]{O->Modify();});}
    bool Dirty(UObject* O){return Step(TEXT("MarkPackageDirty"),true,[&]{O->MarkPackageDirty();});}
    bool DeferredPackageOwned(UObject* Root)
    {if(!Admit(TEXT("IE_save_package_ownership"))||!Live(Root))return false;auto* P=Root->GetOutermost();if(!Live(P))return Fail(TEXT("niagara_collection_save_package_unavailable"),TEXT("Current captured package is unavailable."));TArray<UObject*> Exports;GetObjectsWithPackage(P,Exports,EGetObjectsFlags::IncludeNestedObjects);if(Exports.Num()>262144)return Fail(TEXT("niagara_collection_package_export_bound"),TEXT("Save export bound exceeded."));for(auto* O:Exports){if(!Live(O)||O->GetPackage()!=P||O->GetOutermost()!=P||(O!=Root&&!O->IsIn(Root)))return Fail(TEXT("niagara_collection_foreign_package_export"),TEXT("Save package contains an unavailable or foreign export outside the exact script subtree."));}return Admit(TEXT("IE_save_package_ownership_complete"));}
    bool Save(UObject* O)
    {R.bSaveRequested=true;if(DeferredMode&&!DeferredPackageOwned(O))return false;if(!SDK(TEXT("SaveLoadedAsset"),true)){return false;}R.bSaveAttempted=true;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_object"),O->GetPathName());Row->SetStringField(TEXT("requested_package"),O->GetOutermost()->GetName());const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(O,false);R.bSaved|=Saved;SaveHealthPassed&=Saved;Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Saved);Row->SetBoolField(TEXT("actual_saved_history"),R.bSaved);Saves.Add(MakeShared<FJsonValueObject>(Row));if(!After(TEXT("SaveLoadedAsset"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}if(DeferredMode&&!DeferredPackageOwned(O))return false;Row->SetStringField(TEXT("actual_package_after"),O->GetOutermost()->GetName());Row->SetBoolField(TEXT("dirty_after"),O->GetOutermost()->IsDirty());if(Saved&&O->GetOutermost()->IsDirty()){Fail(TEXT("niagara_collection_saved_dirty"),TEXT("Save returned true but captured package is dirty; true history retained."));return false;}return Saved;}
    bool Request(UNiagaraScript* S,const FGuid& V,bool Force)
    {R.bCompileRequested=true;if(!SDK(TEXT("RequestCompile_script"),true)){return false;}R.bCompileAttempted=true;R.Compiled.Reset();auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("script_path"),S->GetPathName());Row->SetStringField(TEXT("version"),V.ToString());S->RequestCompile(V,Force);Row->SetBoolField(TEXT("dispatch_returned"),true);Row->SetBoolField(TEXT("completion_verified"),false);Compiles.Add(MakeShared<FJsonValueObject>(Row));return After(TEXT("RequestCompile_script"));}
    bool Request(UNiagaraSystem* S)
    {R.bCompileRequested=true;if(!SDK(TEXT("RequestCompile_system"),true)){return false;}R.bCompileAttempted=true;R.Compiled.Reset();const bool Accepted=S->RequestCompile(false);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("system_path"),S->GetPathName());Row->SetBoolField(TEXT("accepted"),Accepted);Row->SetBoolField(TEXT("dispatch_returned"),true);Row->SetBoolField(TEXT("completion_verified"),false);Compiles.Add(MakeShared<FJsonValueObject>(Row));return After(TEXT("RequestCompile_system"));}
    bool AddParameter(UNiagaraParameterCollection* O,UNiagaraParameterCollectionInstance* I,const FNiagaraVariable& P,bool& Added)
    {auto* C=Collections.FindByPredicate([&](const auto& E){return E.Collection.Get()==O;});if(!C||!Admit(TEXT("AddParameter_preflight"))){return false;}TArray<FNiagaraVariable> Expected=StoreParameters(I->GetParameterStore());Expected.Add(P);if(!SDK(TEXT("AddParameter"),true)){return false;}Added=I->AddParameter(P);Prefix->SetBoolField(TEXT("AddParameter_SDK_returned"),true);Prefix->SetBoolField(TEXT("AddParameter_returned_success"),Added);if(Added){C->StoreShape=StoreShapeOf(Expected);R.bChanged=true;}return After(TEXT("AddParameter"));}
    TSharedPtr<FJsonObject> Projection();
};
bool CaptureGlobalSystems(FWork& W,bool& Added)
{
    Added=false;if(!W.Admit(TEXT("IE_global_system_inventory")))return false;
    for(TObjectIterator<UNiagaraSystem> It;It;++It)
    {
        auto* S=*It;if(!Live(S)||S->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))continue;
        bool Found=false;for(const auto& I:W.GlobalSystems)if(I->Object.Get()==S){Found=true;break;}
        if(!Found){if(W.GlobalSystems.Num()>=16384)return W.Fail(TEXT("niagara_collection_global_system_bound"),TEXT("Global System inventory exceeds 16384; no incomplete global success."));W.GlobalSystems.Add(MakeUnique<FIdentity>(S));Added=true;}
    }
    return W.Admit(TEXT("IE_after_global_system_inventory"));
}
bool ObserveGlobalCompilation(FWork& W,bool Pump,bool& Idle)
{
    Idle=false;if(!W.Admit(TEXT("IE_global_compilation_observation")))return false;
    if(Pump){if(!W.SDK(TEXT("IE_global_ProcessAsyncTasks"),true))return false;FAssetCompilingManager::Get().ProcessAsyncTasks(true);++W.GlobalPumps;if(!W.After(TEXT("IE_global_ProcessAsyncTasks")))return false;}
    bool Added=false;if(!CaptureGlobalSystems(W,Added))return false;
    bool Busy=false;int32 ActiveCount=0,OutstandingCount=0;TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& Identity:W.GlobalSystems)
    {
        if(!W.Admit(TEXT("IE_before_global_System_observation")))return false;
        auto* S=Cast<UNiagaraSystem>(Identity->Object.Get());if(!S)return W.Fail(TEXT("niagara_collection_global_system_unavailable"),TEXT("Captured global System unavailable; no load/replacement."));
        bool Needs=false,Active=false,Outstanding=false;
        if(!W.SDK(TEXT("IE_global_NeedsRequestCompile")))return false;Needs=S->NeedsRequestCompile();if(!W.After(TEXT("IE_global_NeedsRequestCompile")))return false;
        if(Needs)return W.Fail(TEXT("niagara_collection_global_pending_request_kind_unavailable"),TEXT("Global System has a pending request. Public SDK cannot distinguish original SkipPendingOnDemand policy; no forced compile, false global completion or later save."));
        if(!W.SDK(TEXT("IE_global_HasActiveCompilations")))return false;Active=S->HasActiveCompilations();if(!W.After(TEXT("IE_global_HasActiveCompilations")))return false;
        if(Pump&&Active){if(!W.SDK(TEXT("IE_global_PollForCompilationComplete"),true))return false;S->PollForCompilationComplete(false);if(!W.After(TEXT("IE_global_PollForCompilationComplete")))return false;}
        if(!W.SDK(TEXT("IE_global_HasOutstandingCompilationRequests")))return false;Outstanding=S->HasOutstandingCompilationRequests(true);if(!W.After(TEXT("IE_global_HasOutstandingCompilationRequests")))return false;
        if(!W.SDK(TEXT("IE_global_HasActiveCompilations_after")))return false;Active=S->HasActiveCompilations();if(!W.After(TEXT("IE_global_HasActiveCompilations_after")))return false;
        if(!W.SDK(TEXT("IE_global_NeedsRequestCompile_after")))return false;Needs=S->NeedsRequestCompile();if(!W.After(TEXT("IE_global_NeedsRequestCompile_after")))return false;
        if(Needs)return W.Fail(TEXT("niagara_collection_global_pending_request_kind_unavailable"),TEXT("Global pending request appeared during observation; original skip-on-demand distinction unavailable. Actual prefix retained, no later save."));
        Busy|=Active||Outstanding;ActiveCount+=Active?1:0;OutstandingCount+=Outstanding?1:0;
        if(Rows.Num()<32){auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("system_path"),Identity->Path);Row->SetBoolField(TEXT("active"),Active);Row->SetBoolField(TEXT("outstanding_including_GPU"),Outstanding);Rows.Add(MakeShared<FJsonValueObject>(Row));}
    }
    if(!W.SDK(TEXT("IE_global_GetNumRemainingAssets")))return false;W.GlobalRemaining=FAssetCompilingManager::Get().GetNumRemainingAssets();if(!W.After(TEXT("IE_global_GetNumRemainingAssets")))return false;
    if(W.GlobalRemaining<0)return W.Fail(TEXT("niagara_collection_global_remaining_invalid"),TEXT("Global compilation manager returned an invalid remaining count."));
    bool NewSystems=false;if(!CaptureGlobalSystems(W,NewSystems))return false;
    Idle=W.GlobalRemaining==0&&!Busy&&!NewSystems;auto Status=MakeShared<FJsonObject>();Status->SetStringField(TEXT("scope"),TEXT("all registered AssetCompilingManagers plus current loaded NiagaraSystems including GPU outstanding/results retrieval; no local-idle substitute"));Status->SetNumberField(TEXT("remaining_assets"),W.GlobalRemaining);Status->SetNumberField(TEXT("observed_system_count"),W.GlobalSystems.Num());Status->SetNumberField(TEXT("active_system_count"),ActiveCount);Status->SetNumberField(TEXT("outstanding_system_count"),OutstandingCount);Status->SetNumberField(TEXT("ProcessAsyncTasks_calls"),W.GlobalPumps);Status->SetBoolField(TEXT("new_systems_require_next_observation"),NewSystems);Status->SetBoolField(TEXT("global_idle_observed"),Idle);Status->SetBoolField(TEXT("timeout_available"),false);Status->SetArrayField(TEXT("bounded_system_diagnostics"),Rows);Status->SetNumberField(TEXT("omitted_system_diagnostic_count"),FMath::Max(0,W.GlobalSystems.Num()-Rows.Num()));W.R.Data->SetObjectField(TEXT("global_compilation_observation"),Status);
    return W.Admit(TEXT("IE_global_compilation_observation_complete"));
}

#define GG_STEP(Stage,Mutation,Expr) do{if(!W.Step(TEXT(Stage),Mutation,[&]{Expr;})){return W.Error(W.R.Error);}}while(false)

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

FString NiagaraTypeName(const FNiagaraTypeDefinition& TypeDef)
{
    return TypeDef.GetNameText().ToString();
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
        if (!Item.IsValid() || !Item->TryGetNumber(NumberValue) || !FMath::IsFinite(NumberValue))
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

TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    return JsonValues;
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

FString NormalizeNiagaraParameterCollectionNamespace(FString Namespace)
{
    Namespace.TrimStartAndEndInline();
    if (Namespace.IsEmpty())
    {
        return Namespace;
    }
    if (!Namespace.StartsWith(TEXT("NPC.")))
    {
        Namespace = FString::Printf(TEXT("NPC.%s"), *Namespace);
    }
    if (!Namespace.EndsWith(TEXT(".")))
    {
        Namespace += TEXT(".");
    }
    return Namespace;
}

FString ReplaceNiagaraNamespacePrefix(const FString& Value, const FString& OldNamespace, const FString& NewNamespace)
{
    FString Updated = Value;
    Updated.ReplaceInline(*OldNamespace, *NewNamespace, ESearchCase::CaseSensitive);
    return Updated;
}

FString NiagaraParameterCollectionNamespaceShortName(const FString& Namespace)
{
    FString ShortName = NormalizeNiagaraParameterCollectionNamespace(Namespace);
    ShortName.RemoveFromStart(TEXT("NPC."));
    ShortName.RemoveFromEnd(TEXT("."));
    return ShortName;
}

int32 ReplaceNiagaraStringToken(FString& Value, const FString& OldToken, const FString& NewToken)
{
    if (OldToken.IsEmpty() || OldToken == NewToken || !Value.Contains(OldToken, ESearchCase::CaseSensitive))
    {
        return 0;
    }

    const int32 BeforeLength = Value.Len();
    Value.ReplaceInline(*OldToken, *NewToken, ESearchCase::CaseSensitive);
    return Value.Len() != BeforeLength || !Value.Contains(OldToken, ESearchCase::CaseSensitive) ? 1 : 0;
}

bool NiagaraObjectPathsReferToSamePackage(const FString& LeftPath, const FString& RightPath)
{
    return PackagePathFromObjectPath(LeftPath).Equals(PackagePathFromObjectPath(RightPath), ESearchCase::IgnoreCase);
}

bool MatchesNiagaraCollectionParameterName(
    const FNiagaraVariable& Candidate,
    const FString& RequestedName,
    const FString& PreferredNamespace)
{
    const FString CandidateName = Candidate.GetName().ToString();
    if (CandidateName.Equals(RequestedName, ESearchCase::IgnoreCase))
    {
        return true;
    }
    if (!PreferredNamespace.IsEmpty())
    {
        const FString NamespacedName = FString::Printf(TEXT("NPC.%s.%s"), *PreferredNamespace, *RequestedName);
        if (CandidateName.Equals(NamespacedName, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    return CandidateName.EndsWith(FString::Printf(TEXT(".%s"), *RequestedName), ESearchCase::IgnoreCase);
}
bool SetNiagaraParameterStoreValue(
    FNiagaraParameterStore& Store,
    const FNiagaraVariable& Parameter,
    const TSharedPtr<FJsonValue>& JsonValue,
    FString& OutError)
{
    const FNiagaraTypeDefinition& TypeDef = Parameter.GetType();
    if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
    {
        double NumberValue = 0.0;
        if (!JsonValue.IsValid() || !JsonValue->TryGetNumber(NumberValue) || !FMath::IsFinite(NumberValue))
        {
            OutError = TEXT("Expected numeric value for float Niagara collection parameter");
            return false;
        }
        if(FMath::Abs(NumberValue)>TNumericLimits<float>::Max()){OutError=TEXT("Float value exceeds finite float32 storage");return false;}
        return Store.SetParameterValue<float>(static_cast<float>(NumberValue), Parameter, false);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
    {
        double NumberValue = 0.0;
        if (!JsonValue.IsValid() || !JsonValue->TryGetNumber(NumberValue) || !FMath::IsFinite(NumberValue))
        {
            OutError = TEXT("Expected numeric value for int Niagara collection parameter");
            return false;
        }
        if(NumberValue<MIN_int32||NumberValue>MAX_int32){OutError=TEXT("Integer value exceeds int32 storage");return false;}
        return Store.SetParameterValue<int32>(static_cast<int32>(NumberValue), Parameter, false);
    }
    if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
    {
        bool BoolValue = false;
        if (!JsonValue.IsValid() || !JsonValue->TryGetBool(BoolValue))
        {
            OutError = TEXT("Expected boolean value for bool Niagara collection parameter");
            return false;
        }
        return Store.SetParameterValue<FNiagaraBool>(FNiagaraBool(BoolValue), Parameter, false);
    }

    TArray<double> Values;
    if (!JsonArrayToDoubles(JsonValue, Values))
    {
        OutError = TEXT("Expected numeric array value for vector/color Niagara collection parameter");
        return false;
    }

    if(TypeDef!=FNiagaraTypeDefinition::GetPositionDef()){const int32 Used=TypeDef==FNiagaraTypeDefinition::GetColorDef()?FMath::Min(4,Values.Num()):TypeDef==FNiagaraTypeDefinition::GetVec2Def()?FMath::Min(2,Values.Num()):TypeDef==FNiagaraTypeDefinition::GetVec3Def()?FMath::Min(3,Values.Num()):TypeDef==FNiagaraTypeDefinition::GetVec4Def()?FMath::Min(4,Values.Num()):0;for(int32 I=0;I<Used;++I){if(!FMath::IsFinite(Values[I])||FMath::Abs(Values[I])>TNumericLimits<float>::Max()){OutError=TEXT("Vector/color component exceeds finite float32 storage");return false;}}}
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

    OutError = FString::Printf(TEXT("Unsupported Niagara collection parameter type: %s"), *NiagaraTypeName(TypeDef));
    return false;
}
TSharedPtr<FJsonObject> NiagaraMeshPropertiesBaseToJsonObject(const FNiagaraMeshRendererMeshPropertiesBase& MeshProperties)
{
    TSharedPtr<FJsonObject> MeshObject = MakeShared<FJsonObject>();
    MeshObject->SetStringField(TEXT("mesh"), MeshProperties.Mesh?MeshProperties.Mesh->GetPathName():FString());
    MeshObject->SetField(TEXT("scale"), NumberArrayToJson({ MeshProperties.Scale.X, MeshProperties.Scale.Y, MeshProperties.Scale.Z }));
    MeshObject->SetField(TEXT("rotation"), NumberArrayToJson({ MeshProperties.Rotation.Pitch, MeshProperties.Rotation.Yaw, MeshProperties.Rotation.Roll }));
    MeshObject->SetField(TEXT("pivot_offset"), NumberArrayToJson({ MeshProperties.PivotOffset.X, MeshProperties.PivotOffset.Y, MeshProperties.PivotOffset.Z }));
    if (const UEnum* PivotOffsetSpaceEnum = StaticEnum<ENiagaraMeshPivotOffsetSpace>())
    {
        MeshObject->SetStringField(TEXT("pivot_offset_space"), PivotOffsetSpaceEnum->GetNameStringByValue(static_cast<int64>(MeshProperties.PivotOffsetSpace)));
    }
    else
    {
        MeshObject->SetStringField(TEXT("pivot_offset_space"), FString::FromInt(static_cast<int32>(MeshProperties.PivotOffsetSpace)));
    }
    return MeshObject;
}

// ArrayMesh's derived header is Internal. Only the exact loaded public UPROPERTY/base struct is read.
TSharedPtr<FJsonValue> NiagaraParameterValueToJson(const FNiagaraParameterStore& Store,const FNiagaraVariable& Parameter)
{
    const auto& T=Parameter.GetType();const int32 Offset=Store.IndexOf(Parameter);if(Offset==INDEX_NONE){return MakeShared<FJsonValueNull>();}if(!T.IsDataInterface()&&!T.IsUObject()&&T!=FNiagaraTypeDefinition::GetPositionDef()){const int32 Size=Parameter.GetSizeInBytes();if(Offset<0||Size<0||static_cast<int64>(Offset)+Size>Store.GetParameterDataArray().Num()){return MakeShared<FJsonValueNull>();}}
    if(T==FNiagaraTypeDefinition::GetFloatDef()){return MakeShared<FJsonValueNumber>(Store.GetParameterValue<float>(Parameter));}
    if(T==FNiagaraTypeDefinition::GetIntDef()){return MakeShared<FJsonValueNumber>(Store.GetParameterValue<int32>(Parameter));}
    if(T==FNiagaraTypeDefinition::GetBoolDef()){return MakeShared<FJsonValueBoolean>(Store.GetParameterValue<FNiagaraBool>(Parameter).GetValue());}
    if(T==FNiagaraTypeDefinition::GetColorDef()){const auto V=Store.GetParameterValue<FLinearColor>(Parameter);return NumberArrayToJson({V.R,V.G,V.B,V.A});}
    if(T==FNiagaraTypeDefinition::GetVec2Def()){const auto V=Store.GetParameterValue<FVector2f>(Parameter);return NumberArrayToJson({V.X,V.Y});}
    if(T==FNiagaraTypeDefinition::GetVec3Def()){const auto V=Store.GetParameterValue<FVector3f>(Parameter);return NumberArrayToJson({V.X,V.Y,V.Z});}
    if(T==FNiagaraTypeDefinition::GetVec4Def()){const auto V=Store.GetParameterValue<FVector4f>(Parameter);return NumberArrayToJson({V.X,V.Y,V.Z,V.W});}
    if(T==FNiagaraTypeDefinition::GetPositionDef()){if(const FVector* V=Store.GetPositionParameterValue(Parameter.GetName())){return NumberArrayToJson({V->X,V->Y,V->Z});}}
    if(T.IsDataInterface())
    {auto* DI=Store.GetDataInterfaces().IsValidIndex(Offset)?Store.GetDataInterface(Parameter):nullptr;if(Live(DI)&&DI->GetClass()->GetPathName().Equals(TEXT("/Script/Niagara.NiagaraDataInterfaceArrayMesh"),ESearchCase::CaseSensitive))
        {auto* A=FindFProperty<FArrayProperty>(DI->GetClass(),TEXT("MeshData"));auto* Inner=A?CastField<FStructProperty>(A->Inner):nullptr;UScriptStruct* S=Inner?Inner->Struct.Get():nullptr;
         if(A&&A->GetOwnerStruct()==DI->GetClass()&&A->ArrayDim==1&&A->HasAllPropertyFlags(CPF_NativeAccessSpecifierPublic)&&Live(S)&&S->GetPathName().Equals(TEXT("/Script/Niagara.NiagaraMeshRendererMeshPropertiesBase"),ESearchCase::CaseSensitive)&&S->GetStructureSize()==sizeof(FNiagaraMeshRendererMeshPropertiesBase)&&S->GetMinAlignment()==alignof(FNiagaraMeshRendererMeshPropertiesBase))
         {const auto* Rows=A->ContainerPtrToValuePtr<TArray<FNiagaraMeshRendererMeshPropertiesBase>>(DI);if(Rows&&Rows->Num()<=16384){TArray<TSharedPtr<FJsonValue>> Out;for(const auto& M:*Rows){Out.Add(MakeShared<FJsonValueObject>(NiagaraMeshPropertiesBaseToJsonObject(M)));}return MakeShared<FJsonValueArray>(Out);}}}}
    if(T.IsUObject()&&!T.IsDataInterface()){UObject* O=Store.GetUObjects().IsValidIndex(Offset)?Store.GetUObject(Parameter).Get():nullptr;if(Live(O)){return MakeShared<FJsonValueString>(O->GetPathName());}}
    return MakeShared<FJsonValueNull>();
}
FString NiagaraObjectPathOrEmpty(const UObject* O){return O?O->GetPathName():FString();}
bool ForbiddenReference(const FString& Path){const FString P=Path.ToLower();return P.Contains(TEXT("unrealmcp"))||P.Contains(TEXT("cubelessmcp"))||P.Contains(TEXT("modelcontextprotocol"))||P.Contains(TEXT("toolsetregistry"));}
FString DependencyError(const FString& Field,const FString& Value){return FString::Printf(TEXT("Refusing MCP-only content reference in %s: %s"),*Field,*Value);}
TSharedPtr<FJsonObject> FWork::Projection()
{
    auto O=MakeShared<FJsonObject>();if(!Admit(TEXT("target_projection"),true)){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);return O;}TArray<TSharedPtr<FJsonValue>> ObjectRows;
    for(const auto& E:Objects)
    {auto* A=E->Object.Get();auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),E->Path);J->SetStringField(TEXT("class"),A->GetClass()->GetPathName());J->SetStringField(TEXT("package"),A->GetOutermost()->GetName());J->SetBoolField(TEXT("dirty"),A->GetOutermost()->IsDirty());
     if(auto* C=Cast<UNiagaraParameterCollection>(A)){J->SetStringField(TEXT("declarations"),ParameterShape(C->GetParameters()));J->SetStringField(TEXT("namespace"),C->GetNamespace().ToString());J->SetStringField(TEXT("full_namespace"),C->GetFullNamespaceName().ToString());if(auto* I=C->GetDefaultInstance()){J->SetStringField(TEXT("default_instance"),I->GetPathName());TArray<TSharedPtr<FJsonValue>> Values;for(const auto& P:StoreParameters(I->GetParameterStore())){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("parameter"),VariableKey(P));V->SetField(TEXT("value"),NiagaraParameterValueToJson(I->GetParameterStore(),P));Values.Add(MakeShared<FJsonValueObject>(V));}J->SetArrayField(TEXT("store_values"),Values);}}
     if(auto* S=Cast<UNiagaraScript>(A)){const auto& VM=S->GetVMExecutableData();J->SetArrayField(TEXT("VM_paths"),StringArrayToJsonValues(VM.ParameterCollectionPaths));J->SetStringField(TEXT("HLSL"),VM.LastHlslTranslation);J->SetStringField(TEXT("HLSL_GPU"),VM.LastHlslTranslationGPU);J->SetStringField(TEXT("VM_error"),VM.ErrorMsg);TArray<TSharedPtr<FJsonValue>> Refs;for(UNiagaraParameterCollection* CollectionRef:S->CachedParameterCollectionReferences){Refs.Add(CollectionRef?MakeShared<FJsonValueString>(CollectionRef->GetPathName()):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));}J->SetArrayField(TEXT("cached_refs"),Refs);}
     if(auto* G=Cast<UNiagaraGraph>(A)){TArray<TSharedPtr<FJsonValue>> Ns;for(UEdGraphNode* N:G->Nodes){auto NJ=MakeShared<FJsonObject>();NJ->SetStringField(TEXT("node_guid"),N->NodeGuid.ToString());NJ->SetStringField(TEXT("comment"),N->NodeComment);TArray<TSharedPtr<FJsonValue>> Ps;for(auto* P:N->Pins){auto PJ=MakeShared<FJsonObject>();if(P){PJ->SetStringField(TEXT("pin_id"),P->PinId.ToString());PJ->SetStringField(TEXT("name"),P->PinName.ToString());PJ->SetStringField(TEXT("friendly"),P->PinFriendlyName.ToString());PJ->SetStringField(TEXT("tooltip"),P->PinToolTip);PJ->SetStringField(TEXT("default"),P->DefaultValue);}Ps.Add(MakeShared<FJsonValueObject>(PJ));}NJ->SetArrayField(TEXT("pins"),Ps);Ns.Add(MakeShared<FJsonValueObject>(NJ));}J->SetArrayField(TEXT("nodes"),Ns);TArray<FNiagaraVariable> Keys;G->GetAllMetaData().GetKeys(Keys);Keys.Sort([](const auto& L,const auto& Right){return VariableKey(L).Compare(VariableKey(Right),ESearchCase::CaseSensitive)<0;});TArray<TSharedPtr<FJsonValue>> Meta;for(const auto& K:Keys){auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("key"),VariableKey(K));const auto* V=G->GetAllMetaData().Find(K);M->SetStringField(TEXT("object"),V&&*V?(*V)->GetPathName():FString());M->SetStringField(TEXT("variable"),V&&*V?VariableKey((*V)->Variable):FString());Meta.Add(MakeShared<FJsonValueObject>(M));}J->SetArrayField(TEXT("metadata"),Meta);}
     ObjectRows.Add(MakeShared<FJsonValueObject>(J));}
    O->SetArrayField(TEXT("objects"),ObjectRows);O->SetBoolField(TEXT("target_projection_complete"),true);return O;
}
void CaptureBefore(FWork& W)
{auto Now=W.Projection();if(!W.Before){W.Before=Now;return;}const TArray<TSharedPtr<FJsonValue>>* OldRows=nullptr;const TArray<TSharedPtr<FJsonValue>>* NewRows=nullptr;if(!W.Before->TryGetArrayField(TEXT("objects"),OldRows)||!Now->TryGetArrayField(TEXT("objects"),NewRows)){return;}TArray<TSharedPtr<FJsonValue>> Merged=*OldRows;for(const auto& Row:*NewRows){FString Path;Row->AsObject()->TryGetStringField(TEXT("path"),Path);bool Present=false;for(const auto& Existing:Merged){FString OldPath;Existing->AsObject()->TryGetStringField(TEXT("path"),OldPath);if(OldPath.Equals(Path,ESearchCase::CaseSensitive)){Present=true;break;}}if(!Present){Merged.Add(Row);}}W.Before->SetArrayField(TEXT("objects"),Merged);}
TSharedPtr<FJsonObject> BuildNiagaraScriptCompileStatusJson(FWork& W,const UNiagaraScript* S,const FString& Kind,const FString& Name,int32 Index)
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("script_index"),Index);J->SetStringField(TEXT("owner_kind"),Kind);J->SetStringField(TEXT("owner_name"),Name);
    if(!S){J->SetStringField(TEXT("script_name"),FString());J->SetStringField(TEXT("script_path"),FString());J->SetStringField(TEXT("usage"),FString());J->SetStringField(TEXT("compile_status"),TEXT("missing"));J->SetBoolField(TEXT("has_error"),true);J->SetBoolField(TEXT("has_warning"),false);return J;}
    if(!W.SDK(TEXT("script_compile_status_snapshot"))){J->SetBoolField(TEXT("projection_skipped_unsafe"),true);return J;}const auto Status=S->GetLastCompileStatus();const bool CPU=S->IsReadyToRun(ENiagaraSimTarget::CPUSim),GPU=S->IsReadyToRun(ENiagaraSimTarget::GPUComputeSim);if(!W.After(TEXT("script_compile_status_snapshot"))){J->SetBoolField(TEXT("projection_skipped_unsafe"),true);return J;}J->SetStringField(TEXT("script_name"),S->GetName());J->SetStringField(TEXT("script_path"),S->GetPathName());J->SetStringField(TEXT("usage"),NiagaraScriptUsageName(S->GetUsage()));J->SetStringField(TEXT("usage_id"),S->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));J->SetStringField(TEXT("compile_status"),NiagaraCompileStatusName(Status));J->SetBoolField(TEXT("has_error"),IsNiagaraCompileErrorStatus(Status));J->SetBoolField(TEXT("has_warning"),IsNiagaraCompileWarningStatus(Status));J->SetBoolField(TEXT("is_ready_cpu"),CPU);J->SetBoolField(TEXT("is_ready_gpu"),GPU);return J;
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


bool SetStoreValue(FWork& W,UNiagaraParameterCollection* C,UNiagaraParameterCollectionInstance* I,const FNiagaraVariable& P,const TSharedPtr<FJsonValue>& Value,FString& Error)
{
    // Parse after the actual AddParameter. Preview disables interface construction/layout notification; unsupported DI values still fail after the original actual add.
    FNiagaraParameterStore Preview;Preview.AddParameter(P,false,false);if(!SetNiagaraParameterStoreValue(Preview,P,Value,Error)){return false;}const auto Expected=NiagaraParameterValueToJson(Preview,P);
    auto* Identity=W.Collections.FindByPredicate([&](const auto& E){return E.Collection.Get()==C;});if(!Identity||!W.SDK(TEXT("SetParameterValue"),true)){Error=W.R.Error;return false;}
    Identity->ExpectedValues.Add(VariableKey(P),Expected);const bool Applied=SetNiagaraParameterStoreValue(I->GetParameterStore(),P,Value,Error);W.Prefix->SetBoolField(TEXT("SetParameterValue_SDK_returned"),true);W.Prefix->SetBoolField(TEXT("SetParameterValue_returned_success"),Applied);if(!Applied){Identity->ExpectedValues.Remove(VariableKey(P));}
    if(!W.After(TEXT("SetParameterValue"))){Error=W.R.Error;return false;}return Applied;
}
bool GraphOwnership(FWork& W,UNiagaraScriptSourceBase* Source,UObject* Root)
{
    if(!W.CaptureGraph(Source,Root,true)){return false;}auto* S=Cast<UNiagaraScriptSource>(Source);auto* G=S?S->NodeGraph.Get():nullptr;if(!G){return true;}
    if(G->GetAllMetaData().Num()>16384){return W.Fail(TEXT("niagara_collection_metadata_bound"),TEXT("Metadata bound exceeded."));}
    for(const auto& KV:G->GetAllMetaData()){auto* V=KV.Value.Get();if(V&&!W.Own(V,Root)){return false;}}return true;
}
int32 RemapNiagaraGraphParameterCollectionNamespace(FWork& W,UNiagaraScriptSourceBase* Base,const FString& Old,const FString& New)
{
    if(!W.Admit(TEXT("graph_remap_preflight"))){return 0;}auto* S=Cast<UNiagaraScriptSource>(Base);auto* G=S?S->NodeGraph.Get():nullptr;if(!G){return 0;}int32 Count=0;bool Changed=false;
    TArray<UEdGraphNode*> Nodes;for(UEdGraphNode* N:G->Nodes){Nodes.Add(N);}for(auto* N:Nodes)
    {
        bool NodeChanged=false;const TArray<UEdGraphPin*> Pins=N->Pins;
        for(auto* P:Pins)
        {if(!W.Admit(TEXT("pin_remap_admission"))){return Count;}if(!P){continue;}const FString Name=P->PinName.ToString(),Friendly=P->PinFriendlyName.ToString();
         const bool A=Name.Contains(Old,ESearchCase::CaseSensitive),B=Friendly.Contains(Old,ESearchCase::CaseSensitive),C=P->PinToolTip.Contains(Old,ESearchCase::CaseSensitive),D=P->DefaultValue.Contains(Old,ESearchCase::CaseSensitive);if(!(A||B||C||D)){continue;}
         const FString RemappedPinName=A?ReplaceNiagaraNamespacePrefix(Name,Old,New):Name;if(RemappedPinName.Len()>=NAME_SIZE){W.Fail(TEXT("niagara_collection_remapped_name_bound"),TEXT("Remapped physical pin name exceeds NAME_SIZE before explicit field mutation."));return Count;}if(!W.Step(TEXT("remap_pin_fields"),true,[&]{if(A){P->PinName=FName(*RemappedPinName);++Count;}if(B){P->PinFriendlyName=FText::FromString(ReplaceNiagaraNamespacePrefix(Friendly,Old,New));++Count;}if(C){P->PinToolTip=ReplaceNiagaraNamespacePrefix(P->PinToolTip,Old,New);++Count;}if(D){P->DefaultValue=ReplaceNiagaraNamespacePrefix(P->DefaultValue,Old,New);++Count;}})){return Count;}NodeChanged=true;}
        if(!W.Admit(TEXT("node_comment_remap"))){return Count;}if(N->NodeComment.Contains(Old,ESearchCase::CaseSensitive)){if(!W.Step(TEXT("remap_node_comment"),true,[&]{N->NodeComment=ReplaceNiagaraNamespacePrefix(N->NodeComment,Old,New);++Count;})){return Count;}NodeChanged=true;}
        if(NodeChanged){if(!W.Modify(N)){return Count;}Changed=true;}
    }
    TArray<FNiagaraVariable> Keys;G->GetAllMetaData().GetKeys(Keys);
    for(const auto& K:Keys)
    {
        if(!W.Admit(TEXT("metadata_remap_admission"))){return Count;}const FString Name=K.GetName().ToString();if(!Name.Contains(Old,ESearchCase::CaseSensitive)){continue;}const FString RemappedName=ReplaceNiagaraNamespacePrefix(Name,Old,New);if(RemappedName.Len()>=NAME_SIZE){W.Fail(TEXT("niagara_collection_remapped_name_bound"),TEXT("Remapped metadata name exceeds NAME_SIZE before writer."));return Count;}const FName NewName(*RemappedName);FNiagaraVariable NK(K);NK.SetName(NewName);
        TMap<FNiagaraVariable,TWeakObjectPtr<UNiagaraScriptVariable>> BeforeMap;for(const auto& KV:G->GetAllMetaData()){BeforeMap.Add(KV.Key,KV.Value.Get());}
        bool Merged=false,Renamed=false;if(!W.Step(TEXT("RenameParameter"),true,[&]{Renamed=G->RenameParameter(K,NewName,false,&Merged,true);})){return Count;}
        // Reacquire metadata after public RenameParameter broadcasts; no iterator/pointer from BeforeMap is reused.
        TArray<FNiagaraVariable> ExpectedKeys;BeforeMap.GetKeys(ExpectedKeys);if(Renamed&&K.GetName()!=NewName){ExpectedKeys.Remove(K);ExpectedKeys.AddUnique(NK);}
        TArray<FNiagaraVariable> ActualKeys;G->GetAllMetaData().GetKeys(ActualKeys);if(ExpectedKeys.Num()!=ActualKeys.Num()){W.Fail(TEXT("niagara_collection_metadata_transition_changed"),TEXT("Unexpected metadata key membership after RenameParameter."));return Count;}
        for(const auto& Expected:ExpectedKeys)
        {const auto* A=G->GetAllMetaData().Find(Expected);if(!A){W.Fail(TEXT("niagara_collection_metadata_transition_changed"),TEXT("Expected metadata key disappeared."));return Count;}if(Expected!=NK){if(const auto* B=BeforeMap.Find(Expected)){if(A->Get()!=B->Get()){W.Fail(TEXT("niagara_collection_metadata_transition_changed"),TEXT("Unrelated metadata object changed during rename."));return Count;}}}if(*A&&!W.Own(A->Get(),S->GetOutermost()==G->GetOutermost()?G:nullptr)){return Count;}}
        if(Renamed){++Count;Changed=true;continue;}
        const auto* Entry=G->GetAllMetaData().Find(K);UNiagaraScriptVariable* V=Entry?Entry->Get():nullptr;if(!Entry){continue;}if(V&&!W.Modify(V)){return Count;}
        if(!W.Step(TEXT("metadata_fallback_map"),true,[&]{auto& Map=G->GetAllMetaData();Map.Remove(K);Map.Add(NK,V);})){return Count;}
        if(V){if(!W.Step(TEXT("metadata_fallback_variable"),true,[&]{V->Variable.SetName(NewName);V->UpdateChangeId();})){return Count;}}
        ++Count;Changed=true;
    }
    if(Changed){if(!W.Modify(S)||!W.Modify(G)||!W.Step(TEXT("NotifyGraphChanged"),true,[&]{G->NotifyGraphChanged();})){return Count;}}
    return Count;
}
int32 RemapNiagaraScriptParameterCollectionReferences(FWork& W,UNiagaraScript* S,UNiagaraParameterCollection* Old,UNiagaraParameterCollection* New)
{
    if(!S||!New||!W.SDK(TEXT("Resolve_cached_collection_references"))){return 0;}
    auto& References=S->GetCachedParameterCollectionReferences();W.Prefix->SetBoolField(TEXT("Resolve_cached_collection_references_SDK_returned"),true);
    if(!W.After(TEXT("Resolve_cached_collection_references"))){return 0;}W.R.Data->SetBoolField(TEXT("cached_reference_getter_can_load_and_remove_VM_paths"),true);
    if(References.Num()>16384||S->GetVMExecutableData().ParameterCollectionPaths.Num()>16384){W.Fail(TEXT("niagara_collection_cached_refs_bound"),TEXT("Cached collection reference/path bound exceeded."));return 0;}
    int32 Count=0;bool Changed=false,HasNew=false;TArray<TObjectPtr<UNiagaraParameterCollection>> Next=References;
    for(auto& R:Next){if(R==New){HasNew=true;}if(Old&&R==Old){R=New;HasNew=true;Changed=true;++Count;}}
    if(!Old&&!HasNew){Next.Add(New);Changed=true;++Count;}for(int32 I=Next.Num()-1;I>=0;--I){if(!Next[I]){Next.RemoveAt(I);Changed=true;}}
    if(Changed){if(!W.Step(TEXT("remap_cached_collection_refs"),true,[&]{References=Next;})||!W.Modify(S)){return Count;}}
    return Count;
}
int32 RemapNiagaraScriptExecutableParameterCollectionData(FWork& W,UNiagaraScript* S,const FString& OldPath,const FString& NewPath,const FString& Old,const FString& New)
{
    if(!S||!W.Admit(TEXT("VM_remap_admission"))){return 0;}int32 Count=0;
    if(!W.Step(TEXT("remap_VM_executable_fields"),true,[&]{auto& D=S->GetVMExecutableData();for(auto& P:D.ParameterCollectionPaths){if(NiagaraObjectPathsReferToSamePackage(P,OldPath)){P=NewPath;++Count;}else{Count+=ReplaceNiagaraStringToken(P,OldPath,NewPath);Count+=ReplaceNiagaraStringToken(P,Old,New);}}const FString OS=NiagaraParameterCollectionNamespaceShortName(Old),NS=NiagaraParameterCollectionNamespaceShortName(New);Count+=ReplaceNiagaraStringToken(D.LastHlslTranslation,Old,New);Count+=ReplaceNiagaraStringToken(D.LastHlslTranslationGPU,Old,New);Count+=ReplaceNiagaraStringToken(D.LastHlslTranslation,OS,NS);Count+=ReplaceNiagaraStringToken(D.LastHlslTranslationGPU,OS,NS);Count+=ReplaceNiagaraStringToken(D.ErrorMsg,Old,New);Count+=ReplaceNiagaraStringToken(D.ErrorMsg,OS,NS);})){return Count;}
    return Count;
}
bool PotentialRemap(UNiagaraScript* S,UNiagaraParameterCollection* OldCollection,UNiagaraParameterCollection* NewCollection,const FString& OldPath,const FString& NewPath,const FString& Old,const FString& New)
{
    auto* Source=Cast<UNiagaraScriptSource>(S->GetLatestSource());auto* G=Source?Source->NodeGraph.Get():nullptr;if(G){for(UEdGraphNode* N:G->Nodes){if(N->NodeComment.Contains(Old,ESearchCase::CaseSensitive)){return true;}for(auto* P:N->Pins){if(P&&(P->PinName.ToString().Contains(Old,ESearchCase::CaseSensitive)||P->PinFriendlyName.ToString().Contains(Old,ESearchCase::CaseSensitive)||P->PinToolTip.Contains(Old,ESearchCase::CaseSensitive)||P->DefaultValue.Contains(Old,ESearchCase::CaseSensitive))){return true;}}}for(const auto& KV:G->GetAllMetaData()){if(KV.Key.GetName().ToString().Contains(Old,ESearchCase::CaseSensitive)){return true;}}}
    bool HasNew=false;for(UNiagaraParameterCollection* CollectionRef:S->CachedParameterCollectionReferences){if(CollectionRef==NewCollection){HasNew=true;}if(OldCollection&&CollectionRef==OldCollection){return true;}}
    const auto& D=S->GetVMExecutableData();for(const auto& P:D.ParameterCollectionPaths){if(NiagaraObjectPathsReferToSamePackage(P,NewPath)){HasNew=true;}if(OldCollection&&NiagaraObjectPathsReferToSamePackage(P,OldPath)){return true;}FString Temp=P;if(ReplaceNiagaraStringToken(Temp,OldPath,NewPath)+ReplaceNiagaraStringToken(Temp,Old,New)>0){return true;}}
    if(!OldCollection&&!HasNew){return true;}const FString OS=NiagaraParameterCollectionNamespaceShortName(Old),NS=NiagaraParameterCollectionNamespaceShortName(New);for(const FString* P:{&D.LastHlslTranslation,&D.LastHlslTranslationGPU,&D.ErrorMsg}){FString Temp=*P;if(ReplaceNiagaraStringToken(Temp,Old,New)+ReplaceNiagaraStringToken(Temp,OS,NS)>0){return true;}}return false;
}

TSharedPtr<FJsonObject> HandleInspectNiagaraParameterCollectionValues(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("collection_path"), CollectionPath))
    {
        return W.Error(TEXT("Missing 'collection_path' parameter"));
    }

    if (ForbiddenReference(CollectionPath))
    {
        return W.Error(
            DependencyError(TEXT("collection_path"), CollectionPath));
    }

    FString PreferredNamespace;
    Params->TryGetStringField(TEXT("namespace"), PreferredNamespace);

    int32 MaxParameters = 256;
    Params->TryGetNumberField(TEXT("max_parameters"), MaxParameters);
    MaxParameters = FMath::Clamp(MaxParameters, 1, 4096);

    UNiagaraParameterCollection* Collection = W.Load<UNiagaraParameterCollection>(CollectionPath);
    if (!Collection)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara parameter collection: %s"), *CollectionPath));
    }

    const UNiagaraParameterCollectionInstance* DefaultInstance = Collection->GetDefaultInstance();
    if (!DefaultInstance)
    {
        return W.Error(FString::Printf(TEXT("Niagara parameter collection has no default instance: %s"), *Collection->GetPathName()));
    }

    if(!W.CaptureCollection(Collection)) return W.Error(W.R.Error);
    CaptureBefore(W);
    const FNiagaraParameterStore& Store = DefaultInstance->GetParameterStore();

    TSet<FString> RequestedNames;
    const TArray<TSharedPtr<FJsonValue>>* ParameterNames = nullptr;
    if (Params->TryGetArrayField(TEXT("parameter_names"), ParameterNames) && ParameterNames)
    {
        for (const TSharedPtr<FJsonValue>& Value : *ParameterNames)
        {
            FString Name;
            if (Value.IsValid() && Value->TryGetString(Name))
            {
                RequestedNames.Add(Name);
            }
        }
    }

    TArray<TSharedPtr<FJsonValue>> ParameterValues;
    int32 MatchedCount = 0;
    int32 MissingStoreCount = 0;
    for (const FNiagaraVariable& Parameter : Collection->GetParameters())
    {
        bool bRequested = RequestedNames.IsEmpty();
        if (!bRequested)
        {
            for (const FString& RequestedName : RequestedNames)
            {
                if (MatchesNiagaraCollectionParameterName(Parameter, RequestedName, PreferredNamespace))
                {
                    bRequested = true;
                    break;
                }
            }
        }
        if (!bRequested)
        {
            continue;
        }

        ++MatchedCount;
        const bool bHasStoreValue = Store.IndexOf(Parameter) != INDEX_NONE;
        if (!bHasStoreValue)
        {
            ++MissingStoreCount;
        }

        TSharedPtr<FJsonObject> ParameterObj = MakeShared<FJsonObject>();
        ParameterObj->SetStringField(TEXT("name"), Parameter.GetName().ToString());
        ParameterObj->SetStringField(TEXT("type"), NiagaraTypeName(Parameter.GetType()));
        ParameterObj->SetBoolField(TEXT("has_default_store_value"), bHasStoreValue);
        ParameterObj->SetField(TEXT("value"), bHasStoreValue ? NiagaraParameterValueToJson(Store, Parameter) : MakeShared<FJsonValueNull>());
        ParameterValues.Add(MakeShared<FJsonValueObject>(ParameterObj));

        if (ParameterValues.Num() >= MaxParameters)
        {
            break;
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("operation"), TEXT("inspect_niagara_parameter_collection_values"));
    Result->SetStringField(TEXT("collection_path"), Collection->GetPathName());
    Result->SetStringField(TEXT("namespace"), Collection->GetNamespace().ToString());
    Result->SetStringField(TEXT("full_namespace"), Collection->GetFullNamespaceName().ToString());
    Result->SetStringField(TEXT("preferred_namespace"), PreferredNamespace);
    Result->SetNumberField(TEXT("collection_parameter_count"), Collection->GetParameters().Num());
    Result->SetNumberField(TEXT("matched_count"), MatchedCount);
    Result->SetNumberField(TEXT("returned_count"), ParameterValues.Num());
    Result->SetNumberField(TEXT("missing_store_count"), MissingStoreCount);
    Result->SetArrayField(TEXT("parameters"), ParameterValues);
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraParameterCollectionValues(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("collection_path"), CollectionPath))
    {
        return W.Error(TEXT("Missing 'collection_path' parameter"));
    }

    if (ForbiddenReference(CollectionPath))
    {
        return W.Error(
            DependencyError(TEXT("collection_path"), CollectionPath));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(CollectionPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara parameter collection outside %s: %s"), NiagaraTempGenerationRoot, *CollectionPath));
    }

    const TSharedPtr<FJsonObject>* ValuesObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("values"), ValuesObj) || !ValuesObj || !ValuesObj->IsValid())
    {
        return W.Error(TEXT("Missing 'values' object"));
    }

    FString PreferredNamespace;
    Params->TryGetStringField(TEXT("namespace"), PreferredNamespace);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;

    UNiagaraParameterCollection* Collection = W.Load<UNiagaraParameterCollection>(CollectionPath);
    if (!Collection)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara parameter collection: %s"), *CollectionPath));
    }

    UNiagaraParameterCollectionInstance* DefaultInstance = Collection->GetDefaultInstance();
    if (!DefaultInstance)
    {
        return W.Error(FString::Printf(TEXT("Niagara parameter collection has no default instance: %s"), *Collection->GetPathName()));
    }

    if(!W.CaptureCollection(Collection)||!W.Own(Collection,Collection)||!W.Own(DefaultInstance,Collection)) return W.Error(W.R.Error);
    CaptureBefore(W);
    FNiagaraParameterStore& Store = DefaultInstance->GetParameterStore();
    GG_STEP("Collection_Modify",true,Collection->Modify());
    GG_STEP("DefaultInstance_Modify",true,DefaultInstance->Modify());

    TArray<TSharedPtr<FJsonValue>> ResultValues;
    TArray<TSharedPtr<FJsonValue>> FailureValues;
    int32 AppliedCount = 0;
    for (const auto& Pair : (*ValuesObj)->Values)
    {
        const FString RequestedName(Pair.Key);
        const FNiagaraVariable* MatchedParameter = Collection->GetParameters().FindByPredicate(
            [&RequestedName, &PreferredNamespace](const FNiagaraVariable& Candidate)
            {
                return MatchesNiagaraCollectionParameterName(Candidate, RequestedName, PreferredNamespace);
            });

        TOptional<FNiagaraVariable> MatchedValue;if(MatchedParameter){MatchedValue=*MatchedParameter;MatchedParameter=&MatchedValue.GetValue();}
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        W.PartialRows.Add(MakeShared<FJsonValueObject>(Item));
        Item->SetStringField(TEXT("requested_name"), RequestedName);
        if (!MatchedParameter)
        {
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(TEXT("error"), TEXT("No matching Niagara collection parameter was found"));
            FailureValues.Add(MakeShared<FJsonValueObject>(Item));
            ResultValues.Add(MakeShared<FJsonValueObject>(Item));
            continue;
        }

        bool Added=true;if(Store.IndexOf(*MatchedParameter)==INDEX_NONE&&!W.AddParameter(Collection,DefaultInstance,*MatchedParameter,Added)) return W.Error(W.R.Error);
        if (!Added)
        {
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(TEXT("parameter_name"), MatchedParameter->GetName().ToString());
            Item->SetStringField(TEXT("error"), TEXT("Failed to add missing parameter to default store"));
            FailureValues.Add(MakeShared<FJsonValueObject>(Item));
            ResultValues.Add(MakeShared<FJsonValueObject>(Item));
            continue;
        }

        FString ErrorMessage;
        if (!SetStoreValue(W,Collection,DefaultInstance,*MatchedParameter,Pair.Value,ErrorMessage))
        {
            if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
            Item->SetBoolField(TEXT("success"), false);
            Item->SetStringField(TEXT("parameter_name"), MatchedParameter->GetName().ToString());
            Item->SetStringField(TEXT("type"), NiagaraTypeName(MatchedParameter->GetType()));
            Item->SetStringField(TEXT("error"), ErrorMessage);
            FailureValues.Add(MakeShared<FJsonValueObject>(Item));
            ResultValues.Add(MakeShared<FJsonValueObject>(Item));
            continue;
        }

        ++AppliedCount;
        Item->SetBoolField(TEXT("success"), true);
        Item->SetStringField(TEXT("parameter_name"), MatchedParameter->GetName().ToString());
        Item->SetStringField(TEXT("type"), NiagaraTypeName(MatchedParameter->GetType()));
        Item->SetField(TEXT("value"), NiagaraParameterValueToJson(Store, *MatchedParameter));
        ResultValues.Add(MakeShared<FJsonValueObject>(Item));
    }

    if (AppliedCount > 0)
    {
        GG_STEP("RefreshCompileId",true,Collection->RefreshCompileId());
        GG_STEP("Collection_MarkPackageDirty",true,Collection->MarkPackageDirty());
        GG_STEP("DefaultInstance_MarkPackageDirty",true,DefaultInstance->MarkPackageDirty());
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(Collection);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara parameter collection after value update"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), FailureValues.IsEmpty());
    Result->SetStringField(TEXT("operation"), TEXT("set_niagara_parameter_collection_values"));
    Result->SetStringField(TEXT("collection_path"), Collection->GetPathName());
    Result->SetStringField(TEXT("namespace"), Collection->GetNamespace().ToString());
    Result->SetStringField(TEXT("full_namespace"), Collection->GetFullNamespaceName().ToString());
    Result->SetStringField(TEXT("preferred_namespace"), PreferredNamespace);
    Result->SetNumberField(TEXT("requested_count"), (*ValuesObj)->Values.Num());
    Result->SetNumberField(TEXT("applied_count"), AppliedCount);
    Result->SetNumberField(TEXT("failed_count"), FailureValues.Num());
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetArrayField(TEXT("results"), ResultValues);
    Result->SetArrayField(TEXT("failures"), FailureValues);
    return Result;
}

TSharedPtr<FJsonObject> HandleRepairNiagaraParameterCollection(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString CollectionPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("collection_path"), CollectionPath))
    {
        return W.Error(TEXT("Missing 'collection_path' parameter"));
    }

    if (ForbiddenReference(CollectionPath))
    {
        return W.Error(
            DependencyError(TEXT("collection_path"), CollectionPath));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(CollectionPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara parameter collection outside %s: %s"), NiagaraTempGenerationRoot, *CollectionPath));
    }

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;

    UNiagaraParameterCollection* Collection = W.Load<UNiagaraParameterCollection>(CollectionPath);
    if (!Collection)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara parameter collection: %s"), *CollectionPath));
    }

    UNiagaraParameterCollectionInstance* DefaultInstance = Collection->GetDefaultInstance();
    if (!DefaultInstance)
    {
        return W.Error(FString::Printf(TEXT("Niagara parameter collection has no default instance: %s"), *Collection->GetPathName()));
    }

    if(!W.CaptureCollection(Collection)||!W.Own(Collection,Collection)||!W.Own(DefaultInstance,Collection)) return W.Error(W.R.Error);
    CaptureBefore(W);
    FNiagaraParameterStore& DefaultStore = DefaultInstance->GetParameterStore();
    const FString CurrentFullNamespace = Collection->GetFullNamespaceName().ToString();
    const FString CurrentNamespace = Collection->GetNamespace().ToString();

    TArray<FNiagaraVariable> StoreParametersBefore;
    DefaultInstance->GetParameters(StoreParametersBefore);

    int32 MissingDefaultStoreCount = 0;
    int32 CurrentNamespaceParameterCount = 0;
    int32 OtherNamespaceParameterCount = 0;
    TArray<FString> MissingDefaultStoreSamples;
    TArray<FString> OtherNamespaceSamples;

    GG_STEP("Collection_Modify",true,Collection->Modify());
    GG_STEP("DefaultInstance_Modify",true,DefaultInstance->Modify());
    const TArray<FNiagaraVariable> DeclaredParameters=Collection->GetParameters();
    for (const FNiagaraVariable& Parameter : DeclaredParameters)
    {
        const FString ParameterName = Parameter.GetName().ToString();
        if (ParameterName.StartsWith(CurrentFullNamespace))
        {
            ++CurrentNamespaceParameterCount;
        }
        else if (ParameterName.StartsWith(TEXT("NPC.")))
        {
            ++OtherNamespaceParameterCount;
            if (OtherNamespaceSamples.Num() < 50)
            {
                OtherNamespaceSamples.Add(ParameterName);
            }
        }

        if (DefaultStore.IndexOf(Parameter) == INDEX_NONE)
        {
            bool Added=false;if(!W.AddParameter(Collection,DefaultInstance,Parameter,Added)) return W.Error(W.R.Error);
            if (Added)
            {
                ++MissingDefaultStoreCount;
                if (MissingDefaultStoreSamples.Num() < 50)
                {
                    MissingDefaultStoreSamples.Add(ParameterName);
                }
            }
        }
    }

    TArray<FNiagaraVariable> StoreParametersAfter;
    DefaultInstance->GetParameters(StoreParametersAfter);

    if (MissingDefaultStoreCount > 0)
    {
        GG_STEP("RefreshCompileId",true,Collection->RefreshCompileId());
        GG_STEP("Collection_MarkPackageDirty",true,Collection->MarkPackageDirty());
        GG_STEP("DefaultInstance_MarkPackageDirty",true,DefaultInstance->MarkPackageDirty());
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(Collection);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara parameter collection after repair"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("operation"), TEXT("repair_niagara_parameter_collection"));
    Result->SetStringField(TEXT("collection_path"), Collection->GetPathName());
    Result->SetStringField(TEXT("namespace"), CurrentNamespace);
    Result->SetStringField(TEXT("full_namespace"), CurrentFullNamespace);
    Result->SetNumberField(TEXT("collection_parameter_count"), Collection->GetParameters().Num());
    Result->SetNumberField(TEXT("default_store_parameter_count_before"), StoreParametersBefore.Num());
    Result->SetNumberField(TEXT("default_store_parameter_count_after"), StoreParametersAfter.Num());
    Result->SetNumberField(TEXT("missing_default_store_count"), MissingDefaultStoreCount);
    Result->SetNumberField(TEXT("current_namespace_parameter_count"), CurrentNamespaceParameterCount);
    Result->SetNumberField(TEXT("other_namespace_parameter_count"), OtherNamespaceParameterCount);
    Result->SetArrayField(TEXT("missing_default_store_samples"), StringArrayToJsonValues(MissingDefaultStoreSamples));
    Result->SetArrayField(TEXT("other_namespace_samples"), StringArrayToJsonValues(OtherNamespaceSamples));
    Result->SetBoolField(TEXT("saved"), bSaved);
    return Result;
}
TSharedPtr<FJsonObject> HandleRemapNiagaraParameterCollectionReferences(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    FString NewCollectionPath;
    if (!Params->TryGetStringField(TEXT("new_collection_path"), NewCollectionPath))
    {
        return W.Error(TEXT("Missing 'new_collection_path' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to remap Niagara parameter collection references outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bReplaceObjectReferences = false;
    Params->TryGetBoolField(TEXT("replace_object_references"), bReplaceObjectReferences);

    UNiagaraSystem* System = W.Load<UNiagaraSystem>(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraParameterCollection* NewCollection = W.Load<UNiagaraParameterCollection>(NewCollectionPath);
    if (!NewCollection)
    {
        return W.Error(FString::Printf(TEXT("Failed to load new Niagara parameter collection: %s"), *NewCollectionPath));
    }

    FString OldCollectionPath;
    UNiagaraParameterCollection* OldCollection = nullptr;
    if (Params->TryGetStringField(TEXT("old_collection_path"), OldCollectionPath) && !OldCollectionPath.IsEmpty())
    {
        OldCollection = W.Load<UNiagaraParameterCollection>(OldCollectionPath);
        if (!OldCollection)
        {
            return W.Error(FString::Printf(TEXT("Failed to load old Niagara parameter collection: %s"), *OldCollectionPath));
        }
    }

    FString OldNamespace;
    if (OldCollection)
    {
        OldNamespace = OldCollection->GetFullNamespaceName().ToString();
    }
    else if (!Params->TryGetStringField(TEXT("old_namespace"), OldNamespace) || OldNamespace.IsEmpty())
    {
        return W.Error(TEXT("Missing 'old_collection_path' or 'old_namespace' parameter"));
    }
    OldNamespace = NormalizeNiagaraParameterCollectionNamespace(OldNamespace);

    const FString NewNamespace = NormalizeNiagaraParameterCollectionNamespace(NewCollection->GetFullNamespaceName().ToString());
    const FString NewCollectionObjectPath = NormalizeNiagaraObjectPathForLoad(NewCollectionPath);
    const FString OldCollectionObjectPath = OldCollection
        ? NormalizeNiagaraObjectPathForLoad(OldCollection->GetPathName())
        : FString();
    if (OldNamespace.IsEmpty() || NewNamespace.IsEmpty())
    {
        return W.Error(TEXT("Niagara parameter collection namespace cannot be empty"));
    }
    if (OldNamespace == NewNamespace)
    {
        return W.Error(TEXT("Old and new Niagara parameter collection namespaces are identical"));
    }

    TArray<TSharedPtr<FJsonValue>> GraphResults;
    TArray<TSharedPtr<FJsonValue>> ScriptResults;
    TSet<UNiagaraScriptSourceBase*> SeenSources;
    TSet<UNiagaraScript*> SeenScripts;
    TArray<TPair<UNiagaraScriptSourceBase*, FString>> GraphSources;
    TArray<TPair<UNiagaraScript*, FString>> Scripts;

    auto AddGraphSource = [&GraphSources, &SeenSources](UNiagaraScriptSourceBase* SourceBase, const FString& Context)
    {
        if (!SourceBase || SeenSources.Contains(SourceBase))
        {
            return;
        }
        SeenSources.Add(SourceBase);
        GraphSources.Emplace(SourceBase, Context);
    };

    auto AddScript = [&Scripts, &SeenScripts, &AddGraphSource](UNiagaraScript* Script, const FString& Context)
    {
        if (!Script || SeenScripts.Contains(Script))
        {
            return;
        }
        SeenScripts.Add(Script);
        Scripts.Emplace(Script, Context);
        AddGraphSource(Script->GetLatestSource(), Context);
    };

    AddScript(System->GetSystemSpawnScript(), TEXT("system_spawn"));
    AddScript(System->GetSystemUpdateScript(), TEXT("system_update"));

    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            const FString EmitterContext = FString::Printf(TEXT("emitter[%d].%s.graph"), EmitterIndex, *EmitterName);
            AddGraphSource(EmitterData->GraphSource, EmitterContext);

            TArray<UNiagaraScript*> EmitterScripts;
            EmitterData->GetScripts(EmitterScripts, false, false);
            for (UNiagaraScript* Script : EmitterScripts)
            {
                AddScript(Script, FString::Printf(TEXT("emitter[%d].%s.%s"), EmitterIndex, *EmitterName, *NiagaraScriptUsageName(Script ? Script->GetUsage() : ENiagaraScriptUsage::Function)));
            }

            if (EmitterData->ScratchPads)
            {
                for (int32 ScriptIndex = 0; ScriptIndex < EmitterData->ScratchPads->Scripts.Num(); ++ScriptIndex)
                {
                    AddScript(
                        EmitterData->ScratchPads->Scripts[ScriptIndex].Get(),
                        FString::Printf(TEXT("emitter[%d].%s.scratch_pad[%d]"), EmitterIndex, *EmitterName, ScriptIndex));
                }
            }

            if (EmitterData->ParentScratchPads)
            {
                for (int32 ScriptIndex = 0; ScriptIndex < EmitterData->ParentScratchPads->Scripts.Num(); ++ScriptIndex)
                {
                    AddScript(
                        EmitterData->ParentScratchPads->Scripts[ScriptIndex].Get(),
                        FString::Printf(TEXT("emitter[%d].%s.parent_scratch_pad[%d]"), EmitterIndex, *EmitterName, ScriptIndex));
                }
            }
        }
        ++EmitterIndex;
    }

    for (int32 ScriptIndex = 0; ScriptIndex < System->ScratchPadScripts.Num(); ++ScriptIndex)
    {
        AddScript(
            System->ScratchPadScripts[ScriptIndex].Get(),
            FString::Printf(TEXT("system_scratch_pad[%d]"), ScriptIndex));
    }

    if(!W.CaptureSystem(System)||!W.Own(System,System)) return W.Error(W.R.Error);
    for(const auto& Entry:Scripts){if(!W.CaptureScript(Entry.Key)||!W.Own(Entry.Key,System)) return W.Error(W.R.Error);}
    for(const auto& Entry:GraphSources){if(!GraphOwnership(W,Entry.Key,System)) return W.Error(W.R.Error);}
    CaptureBefore(W);
    int32 GraphRemapCount = 0;
    for (const TPair<UNiagaraScriptSourceBase*, FString>& GraphSource : GraphSources)
    {
        const int32 RemapCount = RemapNiagaraGraphParameterCollectionNamespace(W,GraphSource.Key, OldNamespace, NewNamespace);
        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        GraphRemapCount += RemapCount;
        if (RemapCount > 0)
        {
            TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
            GraphObject->SetStringField(TEXT("context"), GraphSource.Value);
            GraphObject->SetStringField(TEXT("source_path"), NiagaraObjectPathOrEmpty(GraphSource.Key));
            GraphObject->SetNumberField(TEXT("remap_count"), RemapCount);
            GraphResults.Add(MakeShared<FJsonValueObject>(GraphObject));
        }
    }

    int32 ScriptReferenceReplaceCount = 0;
    int32 ExecutableDataRemapCount = 0;
    for (const TPair<UNiagaraScript*, FString>& ScriptEntry : Scripts)
    {
        const int32 ReplaceCount = RemapNiagaraScriptParameterCollectionReferences(W,ScriptEntry.Key, OldCollection, NewCollection);
        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        const int32 ExecutableReplaceCount = RemapNiagaraScriptExecutableParameterCollectionData(
            W,ScriptEntry.Key,
            OldCollectionObjectPath,
            NewCollectionObjectPath,
            OldNamespace,
            NewNamespace);
        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        ScriptReferenceReplaceCount += ReplaceCount;
        ExecutableDataRemapCount += ExecutableReplaceCount;
        if (ReplaceCount > 0 || ExecutableReplaceCount > 0)
        {
            TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
            ScriptObject->SetStringField(TEXT("context"), ScriptEntry.Value);
            ScriptObject->SetStringField(TEXT("script_path"), NiagaraObjectPathOrEmpty(ScriptEntry.Key));
            ScriptObject->SetNumberField(TEXT("reference_replace_count"), ReplaceCount);
            ScriptObject->SetNumberField(TEXT("executable_data_remap_count"), ExecutableReplaceCount);
            ScriptResults.Add(MakeShared<FJsonValueObject>(ScriptObject));
        }
    }

    int64 ObjectReferenceReplaceCount = 0;
    if (OldCollection && bReplaceObjectReferences)
    {
        TMap<UNiagaraParameterCollection*, UNiagaraParameterCollection*> ReplacementMap;
        ReplacementMap.Add(OldCollection, NewCollection);
        if(!W.SDK(TEXT("archive_reference_replace"),true)) return W.Error(W.R.Error);
        W.R.Data->SetBoolField(TEXT("archive_serialization_projection_uncovered"),true);
        {FArchiveReplaceObjectRef<UNiagaraParameterCollection> ReplaceArchive(System,ReplacementMap,EArchiveReplaceObjectFlags::TrackReplacedReferences|EArchiveReplaceObjectFlags::IgnoreArchetypeRef);ObjectReferenceReplaceCount=ReplaceArchive.GetCount();W.Prefix->SetNumberField(TEXT("archive_replaced_reference_count"),static_cast<double>(ObjectReferenceReplaceCount));}
        if(!W.After(TEXT("archive_reference_replace"))) return W.Error(W.R.Error);
    }

    const bool bMutated = GraphRemapCount > 0 || ScriptReferenceReplaceCount > 0 || ExecutableDataRemapCount > 0 || ObjectReferenceReplaceCount > 0;
    bool bCompileRequested = false;
    if (bMutated)
    {
        GG_STEP("System_Modify",true,System->Modify());
        GG_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
        if (bRequestCompile)
        {
            if(!W.Request(System)) return W.Error(W.R.Error);
            bCompileRequested = true;
        }
    }

    bool bSaved = false;
    if (bSave && bMutated)
    {
        bSaved = W.Save(System);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after parameter collection remap"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("old_collection_path"), OldCollection ? OldCollection->GetPathName() : FString());
    Result->SetStringField(TEXT("new_collection_path"), NewCollection->GetPathName());
    Result->SetStringField(TEXT("old_namespace"), OldNamespace);
    Result->SetStringField(TEXT("new_namespace"), NewNamespace);
    Result->SetNumberField(TEXT("graph_source_count"), GraphSources.Num());
    Result->SetNumberField(TEXT("script_count"), Scripts.Num());
    Result->SetNumberField(TEXT("graph_remap_count"), GraphRemapCount);
    Result->SetNumberField(TEXT("script_reference_replace_count"), ScriptReferenceReplaceCount);
    Result->SetNumberField(TEXT("executable_data_remap_count"), ExecutableDataRemapCount);
    Result->SetNumberField(TEXT("object_reference_replace_count"), static_cast<double>(ObjectReferenceReplaceCount));
    Result->SetBoolField(TEXT("replace_object_references"), bReplaceObjectReferences);
    Result->SetBoolField(TEXT("mutated"), bMutated);
    Result->SetBoolField(TEXT("compile_requested"), bCompileRequested);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetArrayField(TEXT("graph_results"), GraphResults);
    Result->SetArrayField(TEXT("script_results"), ScriptResults);
    Result->SetStringField(TEXT("write_scope"), TEXT("parameter_collection_reference_remap"));
    return Result;
}

TSharedPtr<FJsonObject> HandleRemapNiagaraScriptParameterCollectionReferences(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return W.Error(TEXT("Missing parameters"));
    }

    FString NewCollectionPath;
    if (!Params->TryGetStringField(TEXT("new_collection_path"), NewCollectionPath))
    {
        return W.Error(TEXT("Missing 'new_collection_path' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bWaitForCompletion = true;
    Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);

    UNiagaraParameterCollection* NewCollection = W.Load<UNiagaraParameterCollection>(NewCollectionPath);
    if (!NewCollection)
    {
        return W.Error(FString::Printf(TEXT("Failed to load new Niagara parameter collection: %s"), *NewCollectionPath));
    }

    FString OldCollectionPath;
    UNiagaraParameterCollection* OldCollection = nullptr;
    if (Params->TryGetStringField(TEXT("old_collection_path"), OldCollectionPath) && !OldCollectionPath.IsEmpty())
    {
        OldCollection = W.Load<UNiagaraParameterCollection>(OldCollectionPath);
        if (!OldCollection)
        {
            return W.Error(FString::Printf(TEXT("Failed to load old Niagara parameter collection: %s"), *OldCollectionPath));
        }
    }

    FString OldNamespace;
    if (OldCollection)
    {
        OldNamespace = OldCollection->GetFullNamespaceName().ToString();
    }
    else if (!Params->TryGetStringField(TEXT("old_namespace"), OldNamespace) || OldNamespace.IsEmpty())
    {
        return W.Error(TEXT("Missing 'old_collection_path' or 'old_namespace' parameter"));
    }
    OldNamespace = NormalizeNiagaraParameterCollectionNamespace(OldNamespace);

    const FString NewNamespace = NormalizeNiagaraParameterCollectionNamespace(NewCollection->GetFullNamespaceName().ToString());
    const FString NewCollectionObjectPath = NormalizeNiagaraObjectPathForLoad(NewCollectionPath);
    const FString OldCollectionObjectPath = OldCollection
        ? NormalizeNiagaraObjectPathForLoad(OldCollection->GetPathName())
        : FString();

    TArray<FString> ScriptPaths;
    FString ScriptPath;
    if (Params->TryGetStringField(TEXT("script_path"), ScriptPath) && !ScriptPath.IsEmpty())
    {
        ScriptPaths.Add(PackagePathFromObjectPath(ScriptPath));
    }

    const TArray<TSharedPtr<FJsonValue>>* ScriptPathValues = nullptr;
    if (Params->TryGetArrayField(TEXT("script_paths"), ScriptPathValues) && ScriptPathValues)
    {
        for (const TSharedPtr<FJsonValue>& ScriptPathValue : *ScriptPathValues)
        {
            FString ArrayScriptPath;
            if (ScriptPathValue.IsValid() && ScriptPathValue->TryGetString(ArrayScriptPath) && !ArrayScriptPath.IsEmpty())
            {
                ScriptPaths.Add(PackagePathFromObjectPath(ArrayScriptPath));
            }
        }
    }

    FString RootPath;
    if (Params->TryGetStringField(TEXT("root_path"), RootPath) && !RootPath.IsEmpty())
    {
        if (!RootPath.StartsWith(TEXT("/")))
        {
            return W.Error(TEXT("'root_path' must be a package path such as /Game/MCPWeather/FX/Snow/Scripts"));
        }

        if(!W.SDK(TEXT("load_asset_registry_module"))) return W.Error(W.R.Error);
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        if(!W.After(TEXT("load_asset_registry_module"))) return W.Error(W.R.Error);
        FARFilter Filter;
        if(RootPath.Len()>=NAME_SIZE)return W.Error(TEXT("Root package path exceeds NAME_SIZE before registry lookup."));
        Filter.PackagePaths.Add(FName(*RootPath));
        Filter.bRecursivePaths = true;
        Filter.ClassPaths.Add(UNiagaraScript::StaticClass()->GetClassPathName());

        TArray<FAssetData> ScriptAssets;
        GG_STEP("registry_GetAssets",false,AssetRegistryModule.Get().GetAssets(Filter,ScriptAssets));
        if(ScriptAssets.Num()>16384) return W.Error(TEXT("NiagaraScript inventory exceeds bounded 16384 assets"));
        for (const FAssetData& AssetData : ScriptAssets)
        {
            ScriptPaths.Add(AssetData.PackageName.ToString());
        }
    }

    ScriptPaths.Sort();
    for (int32 Index = ScriptPaths.Num() - 1; Index > 0; --Index)
    {
        if (ScriptPaths[Index].Equals(ScriptPaths[Index - 1], ESearchCase::IgnoreCase))
        {
            ScriptPaths.RemoveAt(Index);
        }
    }

    if(ScriptPaths.Num()>16384) return W.Error(TEXT("Script inventory exceeds bounded 16384 paths"));
    if (ScriptPaths.IsEmpty())
    {
        return W.Error(TEXT("No Niagara script paths were provided or found"));
    }

    TMap<FString,UNiagaraScript*> PreflightLoaded;
    if(bRequestCompile&&bWaitForCompletion)
    {
        for(const auto& P:ScriptPaths)
        {if(!bAllowSourceEdit&&!IsTempGeneratedNiagaraPath(P)){continue;}auto* S=W.Load<UNiagaraScript>(P);if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);if(!S){continue;}if(!W.CaptureScript(S)||!W.Own(S,S)||!GraphOwnership(W,S->GetLatestSource(),S)) return W.Error(W.R.Error);PreflightLoaded.Add(P,S);if(PotentialRemap(S,OldCollection,NewCollection,OldCollectionObjectPath,NewCollectionObjectPath,OldNamespace,NewNamespace)&&!W.DeferredMode){W.Fail(TEXT("niagara_collection_async_wait_unsupported"),TEXT("Changing script remap with request_compile and wait_for_completion requires global compilation/save continuation not implemented by this synchronous API. Rejected before explicit writers; loaded/cache global effects remain unknown. No fake timeout/completion or automatic replay."));return W.Error(W.R.Error);}}
    }
    if(W.DeferredMode&&bRequestCompile&&bWaitForCompletion){bool Potential=false;for(const auto& Pair:PreflightLoaded){if(PotentialRemap(Pair.Value,OldCollection,NewCollection,OldCollectionObjectPath,NewCollectionObjectPath,OldNamespace,NewNamespace)){Potential=true;break;}}if(Potential){bool Idle=false;if(!ObserveGlobalCompilation(W,false,Idle))return W.Error(W.R.Error);}}
    TArray<TSharedPtr<FJsonValue>> ResultValues;
    TArray<TWeakObjectPtr<UNiagaraScript>> ModifiedScripts;
    int32 LoadedCount = 0;
    int32 MutatedCount = 0;
    int32 FailedCount = 0;
    int32 TotalGraphRemapCount = 0;
    int32 TotalScriptReferenceReplaceCount = 0;
    int32 TotalExecutableDataRemapCount = 0;
    bool bAnyCompileRequested = false;

    for (const FString& PackagePath : ScriptPaths)
    {
        TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
        W.PartialRows.Add(MakeShared<FJsonValueObject>(ScriptObject));
        ScriptObject->SetStringField(TEXT("script_path"), PackagePath);

        if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(PackagePath))
        {
            ScriptObject->SetBoolField(TEXT("success"), false);
            ScriptObject->SetStringField(TEXT("error"), FString::Printf(TEXT("Refusing to edit Niagara script outside %s: %s"), NiagaraTempGenerationRoot, *PackagePath));
            ResultValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
            ++FailedCount;
            continue;
        }

        UNiagaraScript* Script = bRequestCompile&&bWaitForCompletion?PreflightLoaded.FindRef(PackagePath):W.Load<UNiagaraScript>(PackagePath);
        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        if (!Script)
        {
            ScriptObject->SetBoolField(TEXT("success"), false);
            ScriptObject->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Niagara script: %s"), *PackagePath));
            ResultValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
            ++FailedCount;
            continue;
        }

        ++LoadedCount;
        if(!W.CaptureScript(Script)||!W.Own(Script,Script)||!GraphOwnership(W,Script->GetLatestSource(),Script)) return W.Error(W.R.Error);
        CaptureBefore(W);
        const int32 GraphRemapCount = RemapNiagaraGraphParameterCollectionNamespace(W,Script->GetLatestSource(), OldNamespace, NewNamespace);
        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        const int32 ScriptReferenceReplaceCount = RemapNiagaraScriptParameterCollectionReferences(W,Script, OldCollection, NewCollection);
        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        const int32 ExecutableDataRemapCount = RemapNiagaraScriptExecutableParameterCollectionData(
            W,Script,
            OldCollectionObjectPath,
            NewCollectionObjectPath,
            OldNamespace,
            NewNamespace);

        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        TotalGraphRemapCount += GraphRemapCount;
        TotalScriptReferenceReplaceCount += ScriptReferenceReplaceCount;
        TotalExecutableDataRemapCount += ExecutableDataRemapCount;

        const bool bMutated = GraphRemapCount > 0 || ScriptReferenceReplaceCount > 0 || ExecutableDataRemapCount > 0;
        if (bMutated)
        {
            ++MutatedCount;
            GG_STEP("Script_Modify",true,Script->Modify());
            GG_STEP("InvalidateCompileResults",true,Script->InvalidateCompileResults(TEXT("Niagara parameter collection references remapped")));
            GG_STEP("Script_MarkPackageDirty",true,Script->MarkPackageDirty());
            ModifiedScripts.Add(Script);
            if (bRequestCompile)
            {
                if(!W.Request(Script,FGuid(),true)) return W.Error(W.R.Error);
                bAnyCompileRequested = true;
            }
        }

        ScriptObject->SetBoolField(TEXT("success"), true);
        ScriptObject->SetStringField(TEXT("loaded_path"), Script->GetPathName());
        ScriptObject->SetNumberField(TEXT("graph_remap_count"), GraphRemapCount);
        ScriptObject->SetNumberField(TEXT("script_reference_replace_count"), ScriptReferenceReplaceCount);
        ScriptObject->SetNumberField(TEXT("executable_data_remap_count"), ExecutableDataRemapCount);
        ScriptObject->SetBoolField(TEXT("mutated"), bMutated);
        ResultValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
    }

    const FString OldLoadedPath=OldCollection?OldCollection->GetPathName():FString(),NewLoadedPath=NewCollection->GetPathName();const int32 RequestedCount=ScriptPaths.Num();
    auto CompleteRemap=[&W,ModifiedScripts,ResultValues,OldLoadedPath,NewLoadedPath,OldNamespace,NewNamespace,RequestedCount,LoadedCount,MutatedCount,FailedCount,TotalGraphRemapCount,TotalScriptReferenceReplaceCount,TotalExecutableDataRemapCount,bAnyCompileRequested,bWaitForCompletion,bSave]()->TSharedPtr<FJsonObject>
    {
    int32 SavedCount = 0;
    if (bSave)
    {
        for (const auto& WeakScript : ModifiedScripts)
        {
            if(!W.Admit(TEXT("IE_before_modified_script_save")))return W.Error(W.R.Error);
            auto* Script=WeakScript.Get();if(!Live(Script))return W.Error(TEXT("Modified script expired before ordered save; no reload/retry."));
            if (W.Save(Script))
            {
                ++SavedCount;
            }
            if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), FailedCount == 0);
    Result->SetStringField(TEXT("old_collection_path"), OldLoadedPath);
    Result->SetStringField(TEXT("new_collection_path"), NewLoadedPath);
    Result->SetStringField(TEXT("old_namespace"), OldNamespace);
    Result->SetStringField(TEXT("new_namespace"), NewNamespace);
    Result->SetNumberField(TEXT("requested_script_count"), RequestedCount);
    Result->SetNumberField(TEXT("loaded_count"), LoadedCount);
    Result->SetNumberField(TEXT("mutated_count"), MutatedCount);
    Result->SetNumberField(TEXT("failed_count"), FailedCount);
    Result->SetNumberField(TEXT("saved_count"), SavedCount);
    Result->SetNumberField(TEXT("graph_remap_count"), TotalGraphRemapCount);
    Result->SetNumberField(TEXT("script_reference_replace_count"), TotalScriptReferenceReplaceCount);
    Result->SetNumberField(TEXT("executable_data_remap_count"), TotalExecutableDataRemapCount);
    Result->SetBoolField(TEXT("compile_requested"), bAnyCompileRequested);
    Result->SetBoolField(TEXT("waited_for_completion"), bAnyCompileRequested && bWaitForCompletion);
    Result->SetBoolField(TEXT("saved"), SavedCount > 0);
    Result->SetArrayField(TEXT("scripts"), ResultValues);
    Result->SetStringField(TEXT("write_scope"), TEXT("script_parameter_collection_reference_remap"));
    return Result;
    };
    if(bAnyCompileRequested&&bWaitForCompletion)
    {
        if(!W.DeferredMode){W.Fail(TEXT("niagara_collection_async_wait_unsupported"),TEXT("Compile wait unexpectedly became necessary after guarded preflight; no global FinishAllCompilation or fake completion was performed. Actual prefix retained."));return W.Error(W.R.Error);}
        W.WaitDeferred=true;W.RemapTail=MoveTemp(CompleteRemap);auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);Pending->SetStringField(TEXT("phase"),TEXT("global_compile_wait_before_script_save"));Pending->SetArrayField(TEXT("scripts"),ResultValues);return Pending;
    }
    return CompleteRemap();
}

TSharedPtr<FJsonObject> HandleRecompileNiagaraScriptsInRoot(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    if (!Params.IsValid())
    {
        return W.Error(TEXT("Missing parameters"));
    }

    FString RootPath;
    Params->TryGetStringField(TEXT("root_path"), RootPath);
    RootPath = PackagePathFromObjectPath(RootPath);

    bool bAllowSourceCompile = false;
    Params->TryGetBoolField(TEXT("allow_source_compile"), bAllowSourceCompile);

    bool bForce = true;
    Params->TryGetBoolField(TEXT("force"), bForce);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;

    TArray<FString> ScriptPaths;
    const TArray<TSharedPtr<FJsonValue>>* ScriptPathValues = nullptr;
    if (Params->TryGetArrayField(TEXT("script_paths"), ScriptPathValues) && ScriptPathValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *ScriptPathValues)
        {
            FString ScriptPath;
            if (Value.IsValid() && Value->TryGetString(ScriptPath))
            {
                ScriptPaths.AddUnique(PackagePathFromObjectPath(ScriptPath));
            }
        }
    }

    if (!RootPath.IsEmpty())
    {
        if (!FPackageName::IsValidLongPackageName(RootPath) || !RootPath.StartsWith(TEXT("/Game/")))
        {
            return W.Error(FString::Printf(TEXT("Invalid root_path: %s"), *RootPath));
        }
        if (!bAllowSourceCompile && !IsTempGeneratedNiagaraPath(RootPath))
        {
            return W.Error(
                FString::Printf(TEXT("Refusing to recompile Niagara scripts outside %s: %s"), NiagaraTempGenerationRoot, *RootPath));
        }

        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
        GG_STEP("registry_ScanPathsSynchronous",false,AssetRegistry.ScanPathsSynchronous({RootPath},true));

        TArray<FAssetData> AssetDataValues;
        GG_STEP("registry_GetAssetsByPath",false,AssetRegistry.GetAssetsByPath(FName(*RootPath),AssetDataValues,true));
        if(AssetDataValues.Num()>16384) return W.Error(TEXT("Asset inventory exceeds bounded 16384 entries"));
        for (const FAssetData& AssetData : AssetDataValues)
        {
            if (AssetData.AssetClassPath.GetAssetName().ToString() == TEXT("NiagaraScript"))
            {
                ScriptPaths.AddUnique(AssetData.PackageName.ToString());
            }
        }
    }

    if (ScriptPaths.IsEmpty())
    {
        return W.Error(TEXT("Provide root_path or script_paths with at least one NiagaraScript asset"));
    }

    TArray<TSharedPtr<FJsonValue>> ScriptResultValues;
    TArray<TSharedPtr<FJsonValue>> FailedValues;
    int32 CompiledCount = 0;
    int32 SavedCount = 0;

    for (const FString& ScriptPath : ScriptPaths)
    {
        TSharedPtr<FJsonObject> ScriptResult = MakeShared<FJsonObject>();
        W.PartialRows.Add(MakeShared<FJsonValueObject>(ScriptResult));
        ScriptResult->SetStringField(TEXT("script_path"), ScriptPath);

        if (!FPackageName::IsValidLongPackageName(ScriptPath))
        {
            ScriptResult->SetBoolField(TEXT("success"), false);
            ScriptResult->SetStringField(TEXT("error"), TEXT("Invalid NiagaraScript package path"));
            FailedValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
            ScriptResultValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
            continue;
        }
        if (ForbiddenReference(ScriptPath))
        {
            ScriptResult->SetBoolField(TEXT("success"), false);
            ScriptResult->SetStringField(TEXT("error"), DependencyError(TEXT("script_path"), ScriptPath));
            FailedValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
            ScriptResultValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
            continue;
        }
        if (!bAllowSourceCompile && !IsTempGeneratedNiagaraPath(ScriptPath))
        {
            ScriptResult->SetBoolField(TEXT("success"), false);
            ScriptResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Refusing to recompile NiagaraScript outside %s"), NiagaraTempGenerationRoot));
            FailedValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
            ScriptResultValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
            continue;
        }

        UNiagaraScript* Script = W.Load<UNiagaraScript>(ScriptPath);
        if (!Script)
        {
            ScriptResult->SetBoolField(TEXT("success"), false);
            ScriptResult->SetStringField(TEXT("error"), TEXT("Failed to load NiagaraScript"));
            FailedValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
            ScriptResultValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
            continue;
        }

        if(!W.CaptureScript(Script)||!W.Own(Script,Script)||!GraphOwnership(W,Script->GetLatestSource(),Script)) return W.Error(W.R.Error);
        CaptureBefore(W);
        const FGuid VersionGuid = Script->GetExposedVersion().VersionGuid;
        GG_STEP("Script_Modify",true,Script->Modify());
        GG_STEP("MarkScriptAndSourceDesynchronized",true,Script->MarkScriptAndSourceDesynchronized(TEXT("MCP authoring helper requested NiagaraScript recompile"), VersionGuid));
        if(!W.Request(Script,VersionGuid,bForce)) return W.Error(W.R.Error);
        GG_STEP("Script_MarkPackageDirty",true,Script->MarkPackageDirty());
        ++CompiledCount;

        bool bSaved = false;
        if (bSave)
        {
            bSaved = W.Save(Script);
            if (bSaved)
            {
                ++SavedCount;
            }
        }

        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        ScriptResult->SetBoolField(TEXT("success"), true);
        ScriptResult->SetStringField(TEXT("object_path"), Script->GetPathName());
        ScriptResult->SetStringField(TEXT("version_guid"), VersionGuid.ToString(EGuidFormats::DigitsWithHyphens));
        ScriptResult->SetBoolField(TEXT("force"), bForce);
        ScriptResult->SetBoolField(TEXT("saved"), bSaved);
        ScriptResult->SetObjectField(TEXT("status"), BuildNiagaraScriptCompileStatusJson(W,Script, TEXT("asset"), Script->GetName(), CompiledCount - 1));
        if(!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        ScriptResultValues.Add(MakeShared<FJsonValueObject>(ScriptResult));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), FailedValues.IsEmpty());
    Result->SetStringField(TEXT("operation"), TEXT("recompile_niagara_scripts_in_root"));
    Result->SetStringField(TEXT("root_path"), RootPath);
    Result->SetNumberField(TEXT("requested_count"), ScriptPaths.Num());
    Result->SetNumberField(TEXT("compiled_count"), CompiledCount);
    Result->SetNumberField(TEXT("saved_count"), SavedCount);
    Result->SetNumberField(TEXT("failed_count"), FailedValues.Num());
    Result->SetBoolField(TEXT("allow_source_compile"), bAllowSourceCompile);
    Result->SetBoolField(TEXT("force"), bForce);
    Result->SetBoolField(TEXT("save"), bSave);
    Result->SetArrayField(TEXT("scripts"), ScriptResultValues);
    Result->SetArrayField(TEXT("failures"), FailedValues);
    return Result;
}
FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
    bool Success=false;if(Legacy){Legacy->TryGetBoolField(TEXT("success"),Success);}bool ProjectionComplete=false;
    if(W.Before)
    {auto After=W.Projection();W.R.Data->SetObjectField(TEXT("before"),W.Before);W.R.Data->SetObjectField(TEXT("after"),After);bool BeforeOK=false,AfterOK=false;W.Before->TryGetBoolField(TEXT("target_projection_complete"),BeforeOK);After->TryGetBoolField(TEXT("target_projection_complete"),AfterOK);if(BeforeOK&&AfterOK){W.R.bChanged|=!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive);ProjectionComplete=W.R.Error.IsEmpty()&&W.Admit(TEXT("final_scoped_observation"));}}
    if(!Success&&W.R.Error.IsEmpty()){FString Message;if(!Legacy||!Legacy->TryGetStringField(TEXT("error"),Message)){Message=TEXT("Original collection operation returned failure; see legacy_result and actual prefix.");}W.Fail(TEXT("niagara_collection_request_failed"),Message);}
    if(!W.SaveHealthPassed&&W.R.Error.IsEmpty()){W.R.Stage=TEXT("save_result_health");W.Fail(TEXT("niagara_collection_save_failed"),TEXT("At least one requested SaveLoadedAsset returned false. Original legacy success/counts and earlier actual saved true history are preserved separately."));}
    W.R.Data->SetBoolField(TEXT("requested_save_health_pass"),W.SaveHealthPassed);
    W.R.bSuccess=Success&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;bool ArchiveUncovered=false;W.R.Data->TryGetBoolField(TEXT("archive_serialization_projection_uncovered"),ArchiveUncovered);const bool ScopedKnown=W.R.bSuccess&&W.R.bPostconditionPassed&&ProjectionComplete&&!ArchiveUncovered;
    if(ScopedKnown){W.R.bChangeStateKnown=true;}W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),ProjectionComplete);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),ScopedKnown);W.R.Data->SetBoolField(TEXT("typed_readback_pass"),W.R.bSuccess&&ProjectionComplete);
    W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("postcondition_passed"),W.R.bPostconditionPassed);W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.Saves);W.R.Data->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);W.R.Data->SetArrayField(TEXT("actual_partial_rows"),W.PartialRows);
    W.R.Data->SetStringField(TEXT("change_state_known_scope"),TEXT("Successful dedicated before/after projection and final admission only: exact object/class/outer/package, declarations/default-store typed values, script version/source, physical graph node/pin membership, selected metadata fields, raw VM/cache fields and package dirty flags. This does not establish global SDK purity, entire arbitrary UObject serialization, external dependency graphs or async compile/shader/render completion."));
    W.R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("Current-request SaveLoadedAsset true return history is retained before post-Guard failure; clean readback is separate. Earlier saves remain true after later failure. No reload, rollback or retry is performed."));W.R.Data->SetStringField(TEXT("compile_scope"),W.DeferredMode?TEXT("RequestCompile retains its original synchronous internal work. Required wait observes all AssetCompilingManagers and loaded NiagaraSystems including GPU/results retrieval before original ordered save. Global idle is not successful VM/shader compilation; compiled remains null after dispatch. Unobservable pending-on-demand distinction fails without forcing global requests or saving. No public timeout/deadline was invented."):TEXT("RequestCompile dispatch/acceptance and original compiled_count are not completion. Changing script-remap with actual request_compile+wait remains unsupported before explicit writers. Ineffective wait flags/no-change paths are preserved; no global FinishAllCompilation, sleep, poll or forced flush."));W.R.Data->SetBoolField(TEXT("genuine_compile_wait_implemented"),W.DeferredMode);W.R.Data->SetBoolField(TEXT("Cubeless_runtime_asset_dependency_allowed"),false);W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,bool Writer,FHandler Handler)
{
    FWork W(Writer);TSharedPtr<FJsonObject> Params;if(!ParseParams(Text,Params)){W.Fail(TEXT("niagara_collection_invalid_params_json"),TEXT("Expected one finite JSON object within 1Mi UTF16/depth64/262144 notation tokens and value visits, without duplicate case-sensitive keys or NUL."));return Finish(W,W.Error(W.R.Error),Operation);}if(Running||HasScriptRemapLease()){W.Fail(TEXT("niagara_collection_reentrant_request"),TEXT("Another dedicated collection request is active; no replay."));return Finish(W,W.Error(W.R.Error),Operation);}TGuardValue<bool> Lease(Running,true);if(!W.Admit(TEXT("preflight"))){return Finish(W,W.Error(W.R.Error),Operation);}return Finish(W,Handler(W,Params),Operation);
}
namespace HN=NiagaraScratchUserRecipeAsync;
using FRemapJob=UCubelessNiagaraScratchUserRecipeJob;
struct FScriptRemapContinuation;
TSharedPtr<FScriptRemapContinuation> RemapPending;
TArray<TSharedPtr<FScriptRemapContinuation>> RemapUnsafe;
bool RemapStarting=false;
bool HasScriptRemapLease(){return RemapStarting||RemapPending.IsValid()||!RemapUnsafe.IsEmpty();}
struct FScriptRemapContinuation:TSharedFromThis<FScriptRemapContinuation>
{
    FWork W{true};TWeakObjectPtr<FRemapJob> Job;FRemapJob* Key=nullptr;FGuid Id=FGuid::NewGuid();FString JobPath,LastJson;TSharedPtr<FJsonObject> Params,Legacy;
    FTSTicker::FDelegateHandle Ticker;int64 Revision=0;uint64 LastFrame=MAX_uint64;bool Executing=false,Dispatched=false,Terminal=false,Quarantined=false,Registered=false;
    const TCHAR* Operation=TEXT("remap_niagara_script_parameter_collection_references");
    FScriptRemapContinuation(){W.DeferredMode=true;W.R.Data->SetBoolField(TEXT("all_SDK_calls_nonblocking_proven"),false);W.R.Data->SetBoolField(TEXT("RequestCompile_contains_synchronous_internal_wait"),true);W.R.Data->SetBoolField(TEXT("external_transaction_across_yield_allowed"),false);}
    void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
    bool PublicationSafe()const
    {if(!HN::CanPublishJobObject())return false;if(!W.WorldCaptured)return true;if(W.CapturedEditor.Get()!=GEditor)return false;auto* CurrentWorld=GEditor->GetEditorWorldContext().World();return Live(CurrentWorld)&&!CurrentWorld->bIsTearingDown&&W.World.Get()==CurrentWorld&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext();}
    bool JobExact()const
    {return PublicationSafe()&&Job.Get()==Key&&Live(Key)&&Key->GetClass()==FRemapJob::StaticClass()&&Key->GetOuter()==GetTransientPackage()&&Key->GetOutermost()==GetTransientPackage()&&Key->GetPackage()==GetTransientPackage()&&Key->HasAnyFlags(RF_Transient)&&Key->GetPathName().Equals(JobPath,ESearchCase::CaseSensitive);}
    bool Acquire()
    {if(!W.Admit(TEXT("IE_before_execution_pins")))return false;for(auto* Group:{&W.Objects,&W.GlobalSystems})for(auto& I:*Group){auto* O=I->Object.Get();if(!Live(O))return W.Fail(TEXT("niagara_collection_wait_object_expired"),TEXT("Captured weak object expired across the wait; no reload/replacement/replay."));I->Hold=TStrongObjectPtr<UObject>(O);}return W.Admit(TEXT("IE_after_execution_pins"));}
    void Release()
    {if(PublicationSafe())for(auto* Group:{&W.Objects,&W.GlobalSystems})for(auto& I:*Group)I->Hold.Reset();}
    void Prefix(bool Final)
    {
        auto& D=W.R.Data;D->SetStringField(TEXT("operation"),Operation);D->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::Digits));D->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);D->SetBoolField(TEXT("native_job_terminal"),Final);D->SetBoolField(TEXT("operation_complete"),Final);D->SetBoolField(TEXT("genuine_compile_wait_implemented"),true);D->SetBoolField(TEXT("global_wait_before_save_completed"),W.GlobalWaitObserved);D->SetBoolField(TEXT("global_compiler_cancel_attempted"),false);D->SetBoolField(TEXT("compiler_may_continue_after_job"),W.R.bCompileAttempted&&!W.GlobalWaitObserved);D->SetBoolField(TEXT("successful_compilation_proven"),false);D->SetBoolField(TEXT("automatic_retry_permitted"),false);D->SetBoolField(TEXT("single_core_dispatch"),Dispatched);if(Legacy)D->SetObjectField(TEXT("legacy_result"),Legacy);D->SetArrayField(TEXT("actual_save_prefix"),W.Saves);D->SetArrayField(TEXT("actual_compile_prefix"),W.Compiles);D->SetArrayField(TEXT("actual_partial_rows"),W.PartialRows);if(W.Before)D->SetObjectField(TEXT("before"),W.Before);
    }
    void Pending()
    {W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;Prefix(false);LastJson=Operations::EncodeVariableResult(W.R,Operation);HN::Publish(Key,LastJson,false,JobExact());}
    void Unsafe(const FString& Reason)
    {if(Terminal||Quarantined)return;Unhook();Quarantined=true;W.Fail(TEXT("niagara_collection_wait_unsafe_publication"),Reason);W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;W.R.Data->SetBoolField(TEXT("native_writers_stopped"),true);W.R.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);Prefix(false);LastJson=Operations::EncodeVariableResult(W.R,Operation);HN::Publish(Key,LastJson,false,false);if(!RemapUnsafe.Contains(AsShared()))RemapUnsafe.Add(AsShared());}
    void Final()
    {
        if(Terminal||Quarantined)return;if(!JobExact()){Unsafe(TEXT("Exact Job/Editor/world cannot publish a final receipt; actual prefix retained."));return;}
        Prefix(true);
        if(!W.R.Error.IsEmpty())
        {bool SDKUnknown=false;W.R.Data->TryGetBoolField(TEXT("SDK_global_purity_unverified"),SDKUnknown);W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=!SDKUnknown&&!W.R.bCompileAttempted&&!W.R.bSaveAttempted;auto Skipped=MakeShared<FJsonObject>();Skipped->SetBoolField(TEXT("projection_skipped_after_error"),true);W.R.Data->SetObjectField(TEXT("after"),Skipped);LastJson=Operations::EncodeVariableResult(W.R,Operation);}
        else LastJson=Finish(W,Legacy,Operation);
        if(!JobExact()){Unsafe(TEXT("Context changed during final projection; final was not published."));return;}
        Terminal=true;Unhook();HN::Publish(Key,LastJson,true,true);Release();if(RemapPending.Get()==this)RemapPending.Reset();
    }
    void CancelPlain(const FString& Reason)
    {if(Terminal||Quarantined)return;W.CancelRequested=true;W.CancelReason=Reason.Left(8192);if(Executing)return;Unhook();W.Fail(TEXT("niagara_collection_remap_cancelled"),W.CancelReason);if(!PublicationSafe()){Unsafe(W.CancelReason);return;}Final();}
    bool Step()
    {
        if(Terminal||Quarantined)return false;if(!PublicationSafe()){Unsafe(TEXT("Unsafe GT/GC/exit/save/Editor/world; no more compiler processing or save."));return false;}
        if(Executing||LastFrame==GFrameCounter)return true;LastFrame=GFrameCounter;
        if(Running){W.Fail(TEXT("niagara_collection_remap_core_busy"),TEXT("Another request owns the exact Collection core; no nested SDK."));Final();return false;}
        if(HN::IsStopping()){CancelPlain(TEXT("HN shutdown; stop later phases without cancelling global compiler queues."));return false;}
        if(!JobExact()){Unsafe(TEXT("Exact Job identity expired/changed."));return false;}
        if(W.CancelRequested){CancelPlain(W.CancelReason);return false;}
        TGuardValue<bool> Execution(Executing,true);TGuardValue<bool> Lease(Running,true);
        if(!Acquire()){Final();return false;}
        if(!Dispatched){Dispatched=true;Legacy=HandleRemapNiagaraScriptParameterCollectionReferences(W,Params);}
        else if(W.WaitDeferred)
        {
            bool Idle=false;if(!ObserveGlobalCompilation(W,true,Idle)){Final();return false;}
            if(Idle){W.GlobalWaitObserved=true;W.WaitDeferred=false;auto Tail=MoveTemp(W.RemapTail);if(Tail)Legacy=Tail();if(W.R.Error.IsEmpty()){bool FinalIdle=false;if(!ObserveGlobalCompilation(W,false,FinalIdle)){Final();return false;}W.R.Data->SetBoolField(TEXT("global_idle_at_final"),FinalIdle);if(!FinalIdle)W.Fail(TEXT("niagara_collection_global_compile_restarted_after_save"),TEXT("Global work appeared during save/final observation. Actual SavedTrue remains; no repeated save or false completion."));}}
        }
        if(!PublicationSafe()){Unsafe(TEXT("SDK returned across an unsafe lifetime boundary; actual prefix cached without another UObject call."));return false;}
        if(!W.R.Error.IsEmpty()||W.CancelRequested){Final();return false;}
        if(!W.WaitDeferred){Final();return false;}Pending();Release();return !Quarantined;
    }
};

#undef GG_STEP
}
UCubelessNiagaraScratchUserRecipeJob* StartScriptRemapContinuation(const FString& Text)
{
    using namespace NiagaraScratchUserRecipeAsync;
    if(!CanPublishJobObject()||IsStopping()||HasScriptRemapLease())return nullptr;
    TGuardValue<bool> Starting(RemapStarting,true);auto State=MakeShared<FScriptRemapContinuation>();
    if(!ParseParams(Text,State->Params))State->W.Fail(TEXT("niagara_collection_invalid_params_json"),TEXT("Expected original bounded finite remap JSON params."));
    if(Running)State->W.Fail(TEXT("niagara_collection_remap_core_busy"),TEXT("Existing synchronous Collection core owns the lease; no nested target SDK."));
    if(State->W.R.Error.IsEmpty())State->W.Admit(TEXT("IE_before_job_creation"));
    if(!CanPublishJobObject())return nullptr;
    // Explicit controller allocation is separate from target mutation; subsequent SDK admission is rechecked.
    auto* Created=NewObject<UCubelessNiagaraScratchUserRecipeJob>(GetTransientPackage(),NAME_None,RF_Transient);State->Key=Created;State->W.R.bMutationAttempted=true;State->W.R.Data->SetBoolField(TEXT("transient_job_allocation_returned"),Created!=nullptr);State->W.R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);State->W.R.Data->SetBoolField(TEXT("transient_creation_only"),true);
    RemapPending=State;if(!State->PublicationSafe()||!Live(Created)){State->Unsafe(TEXT("Job allocation returned across unsafe context; no target SDK or replay."));return Created;}
    State->Job=Created;State->JobPath=Created->GetPathName();RegisterJob(Created,EJobKind::CollectionScriptRemap,State->Id);State->Registered=true;if(!State->JobExact()){State->Unsafe(TEXT("Job/context changed during registration; no target dispatch."));return Created;}
    if(!State->W.R.Error.IsEmpty()){State->Legacy=State->W.Error(State->W.R.Error);State->Final();return Created;}
    State->Pending();State->Step();if(!State->Terminal&&!State->Quarantined){TWeakPtr<FScriptRemapContinuation> Weak=State;State->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float){auto Strong=Weak.Pin();return Strong.IsValid()&&Strong->Step();}));}return Created;
}
bool CancelScriptRemapContinuation(UCubelessNiagaraScratchUserRecipeJob* Job,const FString& Reason)
{if(!IsInGameThread()||!RemapPending.IsValid()||RemapPending->Key!=Job)return false;auto State=RemapPending;State->CancelPlain(Reason);return true;}
void ShutdownScriptRemapContinuation(const FString& Reason)
{if(!IsInGameThread())return;if(auto State=RemapPending){State->Unhook();State->CancelPlain(Reason);}}
bool HasScriptRemapContinuationLease(){return HasScriptRemapLease();}

FString InspectValues(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_parameter_collection_values"),false,&HandleInspectNiagaraParameterCollectionValues);}
FString SetValues(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_parameter_collection_values"),true,&HandleSetNiagaraParameterCollectionValues);}
FString RepairDefaultStore(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("repair_niagara_parameter_collection"),true,&HandleRepairNiagaraParameterCollection);}
FString RemapSystem(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("remap_niagara_parameter_collection_references"),true,&HandleRemapNiagaraParameterCollectionReferences);}
FString RemapScripts(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("remap_niagara_script_parameter_collection_references"),true,&HandleRemapNiagaraScriptParameterCollectionReferences);}
FString RecompileScripts(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("recompile_niagara_scripts_in_root"),true,&HandleRecompileNiagaraScriptsInRoot);}
}
