// GP dedicated Editor-only factory bootstrap; source candidate, not build/runtime proof.
#include "Compatibility/UE58/CubelessNiagaraFactoryBootstrapApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Factories/Factory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEffectType.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRenderTarget2D.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraDecalRendererProperties.h"
#include "NiagaraVolumeRendererProperties.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraEmitterFactoryNew.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "CubelessNiagaraFactoryBootstrapJob.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#include "Templates/Function.h"
#include <atomic>
namespace Cubeless::UE58::NiagaraFactoryBootstrap
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false;
bool DeferredLeaseActive=false;
std::atomic<bool> DeferredShutdownRequested{false};
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage;}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
bool Forbidden(const FString& S){const FString L=S.ToLower();return L.Contains(TEXT("unrealmcp"))||L.Contains(TEXT("cubelessmcp"))||L.Contains(TEXT("modelcontextprotocol"))||L.Contains(TEXT("toolsetregistry"));}
struct FIdentity{TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path,PackageName;bool DirtyBefore=false;explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()),PackageName(O->GetOutermost()->GetName()),DirtyBefore(O->GetOutermost()->IsDirty()){}bool Exact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&Live(Package.Get())&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}};
struct FPin{UEdGraphPin* P=nullptr;FGuid Id;TArray<UEdGraphPin*> Links;FString Default;};
struct FNode{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;TArray<FPin> Pins;};
struct FGraph{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNode> Nodes;};
struct FScript{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Exposed;};
FString EmitterPropertyState(const FVersionedNiagaraEmitterData* D){auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("local_space"),D->bLocalSpace);O->SetNumberField(TEXT("sim_target"),static_cast<int32>(D->SimTarget));O->SetBoolField(TEXT("persistent_ids"),D->bRequiresPersistentIDs!=0);O->SetNumberField(TEXT("bounds_mode"),static_cast<int32>(D->CalculateBoundsMode));O->SetBoolField(TEXT("determinism"),D->bDeterminism);O->SetNumberField(TEXT("random_seed"),D->RandomSeed);O->SetNumberField(TEXT("scalability_quality_mask"),D->Platforms.QualityLevelMask);return Json(O);}
struct FEmitter{TWeakObjectPtr<UNiagaraEmitter> Emitter;FGuid Version;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TArray<TWeakObjectPtr<UNiagaraRendererProperties>> Renderers;TArray<TWeakObjectPtr<UNiagaraScript>> Scripts;TWeakObjectPtr<UNiagaraEmitter> Parent,MergeParent;bool Latest=false;FString UniqueName,PropertyState;};
struct FHandle{FGuid Id,Version;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;};
struct FSystem{TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<FHandle> Handles;};
struct FWork
{
 FResult R;bool Persistent=false,WorldCaptured=false,TypedPassed=false,KnownExistingTargetRefusal=false;
 TWeakObjectPtr<UEditorEngine> CapturedEditor;bool DeferredMode=false,CancelRequested=false;FString CancelReason,DeferredSavedFilename;
 TWeakObjectPtr<UNiagaraSystem> DeferredSystem;bool DeferredWait=false,DeferredSaveRequested=false;double DeferredTimeout=10.0,DeferredPoll=0.1;
 TSharedPtr<FJsonObject> DeferredWaitResult;TFunction<bool()> DeferredSaveStep;TFunction<TSharedPtr<FJsonObject>()> DeferredCompleteStep;FString FailedStage;TWeakObjectPtr<UWorld> World;TArray<TUniquePtr<FIdentity>> Objects;TArray<FGraph> Graphs;TArray<FScript> Scripts;TArray<FEmitter> Emitters;TArray<FSystem> Systems;TWeakObjectPtr<UFactory> Factory;TWeakObjectPtr<UClass> FactoryClass;bool EmitterFactory=false,FactoryDefaults=false;TArray<FVersionedNiagaraEmitter> ExpectedFactoryEmitters;TMap<UNiagaraSpriteRendererProperties*,UMaterialInterface*> ExpectedMaterials;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls,SaveRows,CreationRows;FString RequestedObject,RequestedPackage,RequestedFilename;int64 Slots=0,Chars=0;
 explicit FWork(bool P):Persistent(P){R.Compiled=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);R.Data->SetBoolField(TEXT("transient_creation_only"),!P);}
 bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
 TSharedPtr<FJsonObject> Error(const FString& M){if(R.Error.IsEmpty())Fail(TEXT("niagara_factory_bootstrap_failed"),M);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),M);return O;}
 bool Budget(int64 N,int64 Text=0){Slots+=N;Chars+=Text;return (Slots<=262144&&Chars<=32ll*1024*1024)||Fail(TEXT("niagara_factory_output_budget_exceeded"),TEXT("Bounded snapshot/output exceeded; partial prefix retained."));}
 bool Admit(const FString& Stage){R.Stage=Stage;if(DeferredMode&&DeferredShutdownRequested.load()){CancelRequested=true;if(CancelReason.IsEmpty())CancelReason=TEXT("Factory module shutdown stopped the owned job.");}if(CancelRequested)return Fail(TEXT("niagara_factory_async_cancelled"),CancelReason.IsEmpty()?TEXT("Owned factory job cancelled; no further target work."):CancelReason);if(!R.Error.IsEmpty())return false;if(!Safe())return Fail(TEXT("niagara_factory_context_unavailable"),TEXT("Live Editor game thread outside GC/exit/save required."));if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_factory_editor_changed"),TEXT("Captured Editor changed during async factory job."));if(Persistent&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld))return Fail(TEXT("niagara_factory_play_session_active"),TEXT("Persistent factory creation requires stopped PIE/SIE."));auto* W=GEditor->GetEditorWorldContext().World();if(!Live(W)||W->bIsTearingDown)return Fail(TEXT("niagara_factory_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){World=W;WorldCaptured=true;if(DeferredMode)CapturedEditor=GEditor;}else if(World.Get()!=W)return Fail(TEXT("niagara_factory_world_changed"),TEXT("Editor world changed during SDK dispatch."));for(const auto& I:Objects)if(!I->Exact())return Fail(TEXT("niagara_factory_identity_changed"),TEXT("Captured object class/outer/package/path changed; replacement not adopted."));
 for(const auto& S:Scripts)if(S.Script->GetLatestSource()!=S.Source.Get()||S.Script->GetExposedVersion().VersionGuid!=S.Exposed)return Fail(TEXT("niagara_factory_script_changed"),TEXT("Captured script source/version changed."));
 for(const auto& E:Emitters){auto* O=E.Emitter.Get();if(O->GetEmitterData(E.Version)!=E.Data||(E.Latest&&O->GetLatestEmitterData()!=E.Data)||!O->GetUniqueEmitterName().Equals(E.UniqueName,ESearchCase::CaseSensitive)||!EmitterPropertyState(E.Data).Equals(E.PropertyState,ESearchCase::CaseSensitive)||E.Data->GraphSource!=E.Source.Get()||E.Data->VersionedParent.Emitter!=E.Parent.Get()||E.Data->VersionedParentAtLastMerge.Emitter!=E.MergeParent.Get()||E.Data->GetRenderers().Num()!=E.Renderers.Num())return Fail(TEXT("niagara_factory_emitter_changed"),TEXT("Emitter version/source/parent/renderer topology changed."));for(int32 I=0;I<E.Renderers.Num();++I)if(E.Data->GetRenderers()[I]!=E.Renderers[I].Get())return Fail(TEXT("niagara_factory_renderer_changed"),TEXT("Physical renderer slot changed."));TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);if(A.Num()!=E.Scripts.Num())return Fail(TEXT("niagara_factory_emitter_scripts_changed"),TEXT("Emitter scripts changed."));for(int32 I=0;I<A.Num();++I)if(A[I]!=E.Scripts[I].Get())return Fail(TEXT("niagara_factory_emitter_scripts_changed"),TEXT("Emitter script order changed."));}
 for(const auto& S:Systems){auto* O=S.System.Get();if(O->GetSystemSpawnScript()!=S.Spawn.Get()||O->GetSystemUpdateScript()!=S.Update.Get()||O->GetEmitterHandles().Num()!=S.Handles.Num())return Fail(TEXT("niagara_factory_system_changed"),TEXT("System script/emitter topology changed."));for(int32 I=0;I<S.Handles.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=S.Handles[I];if(H.GetId()!=E.Id||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.Version||H.GetEmitterData()!=E.Data)return Fail(TEXT("niagara_factory_system_changed"),TEXT("System physical emitter/version changed."));}}
 for(const auto& G:Graphs){if(G.Source->NodeGraph!=G.Graph.Get()||G.Graph->Nodes.Num()!=G.Nodes.Num())return Fail(TEXT("niagara_factory_graph_changed"),TEXT("Captured source graph membership changed."));for(int32 I=0;I<G.Nodes.Num();++I){auto* N=G.Nodes[I].Node.Get();const auto& E=G.Nodes[I];if(G.Graph->Nodes[I]!=N||N->NodeGuid!=E.Id||N->Pins.Num()!=E.Pins.Num())return Fail(TEXT("niagara_factory_node_changed"),TEXT("Captured node/pin topology changed."));for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& V=E.Pins[J];if(P!=V.P||P->PinId!=V.Id||P->GetOwningNodeUnchecked()!=N||P->LinkedTo!=V.Links||!P->DefaultValue.Equals(V.Default,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_factory_pin_changed"),TEXT("Exact pin/links/default changed across SDK call."));}}}
 if(Factory.IsValid()){if(Factory->GetClass()!=FactoryClass.Get())return Fail(TEXT("niagara_factory_config_changed"),TEXT("Exact stock factory class changed."));if(EmitterFactory){auto* F=static_cast<UNiagaraEmitterFactoryNew*>(Factory.Get());if(F->EmitterToCopy.IsValid()||F->bUseInheritance||F->bAddDefaultModulesAndRenderersToEmptyEmitter!=FactoryDefaults)return Fail(TEXT("niagara_factory_config_changed"),TEXT("Emitter factory no-copy/no-inheritance configuration changed."));}else{auto* F=static_cast<UNiagaraSystemFactoryNew*>(Factory.Get());if(F->SystemToCopy.IsValid()||F->EmittersToAddToNewSystem.Num()!=ExpectedFactoryEmitters.Num())return Fail(TEXT("niagara_factory_config_changed"),TEXT("System factory no-copy/input-list configuration changed."));for(int32 I=0;I<ExpectedFactoryEmitters.Num();++I)if(F->EmittersToAddToNewSystem[I].Emitter!=ExpectedFactoryEmitters[I].Emitter||F->EmittersToAddToNewSystem[I].Version!=ExpectedFactoryEmitters[I].Version)return Fail(TEXT("niagara_factory_config_changed"),TEXT("System factory exact source emitter/version changed."));}}
 for(const auto& Pair:ExpectedMaterials)if(Pair.Key->Material!=Pair.Value)return Fail(TEXT("niagara_factory_material_readback_failed"),TEXT("Owned sprite material differs from requested exact material."));return true;}
 bool Pin(UObject* O){if(!Admit(TEXT("pin_object"))||!Live(O))return Fail(TEXT("niagara_factory_object_unavailable"),TEXT("Expected live stock object."));for(const auto& I:Objects)if(I->Object.Get()==O)return I->Exact();if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
 bool SDK(const TCHAR* Stage,bool Mutation=false,bool PersistentWrite=false){if(!Admit(Stage)||!Budget(1,FCString::Strlen(Stage)))return false;R.bMutationAttempted|=Mutation;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);if(PersistentWrite)R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("stage"),Stage);O->SetBoolField(TEXT("entered"),true);O->SetBoolField(TEXT("returned"),false);O->SetBoolField(TEXT("mutation_attempted"),Mutation);Calls.Add(MakeShared<FJsonValueObject>(O));return true;}
 bool After(const TCHAR* Stage){if(!Calls.IsEmpty())Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
 template<typename F>bool Step(const TCHAR* Stage,F&& Fn,bool Mutation=false,bool PersistentWrite=false){if(!SDK(Stage,Mutation,PersistentWrite))return false;Fn();return After(Stage);}
 bool Graph(UNiagaraScriptSourceBase* Base){if(!Admit(TEXT("capture_graph")))return false;if(!Base)return true;if(!Pin(Base))return false;auto* S=Cast<UNiagaraScriptSource>(Base);if(!S||!S->NodeGraph)return true;auto* G=S->NodeGraph.Get();for(const auto& E:Graphs)if(E.Source.Get()==S&&E.Graph.Get()==G)return true;if(!Pin(G)||G->Nodes.Num()>16384)return Fail(TEXT("niagara_factory_graph_bound"),TEXT("Graph node bound exceeded."));FGraph E;E.Source=S;E.Graph=G;TSet<UEdGraphPin*> Known;for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N))return Fail(TEXT("niagara_factory_graph_malformed"),TEXT("Null/unavailable graph node."));FNode NE;NE.Node=N;NE.Id=N->NodeGuid;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N||!Budget(1,P->DefaultValue.Len()))return Fail(TEXT("niagara_factory_pin_malformed"),TEXT("Null/foreign pin or capture bound exceeded."));FPin PE;PE.P=P;PE.Id=P->PinId;PE.Links=P->LinkedTo;PE.Default=P->DefaultValue;Known.Add(P);if(P->DefaultObject&&!Pin(P->DefaultObject))return false;if(P->PinType.PinSubCategoryObject.IsValid()&&!Pin(P->PinType.PinSubCategoryObject.Get()))return false;NE.Pins.Add(MoveTemp(PE));}if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()>65536)return Fail(TEXT("niagara_factory_signature_bound"),TEXT("Function signature bound exceeded."));if(F->FunctionScript&&!Pin(F->FunctionScript))return false;}E.Nodes.Add(MoveTemp(NE));}for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links)if(!L||!Known.Contains(L))return Fail(TEXT("niagara_factory_link_malformed"),TEXT("Link target is outside captured graph."));Graphs.Add(MoveTemp(E));return Admit(TEXT("graph_captured"));}
 bool Script(UNiagaraScript* S){if(!S)return true;for(const auto& E:Scripts)if(E.Script.Get()==S)return true;if(!Pin(S))return false;FScript E;E.Script=S;E.Source=S->GetLatestSource();E.Exposed=S->GetExposedVersion().VersionGuid;Scripts.Add(E);return Graph(E.Source.Get());}
 bool Emitter(UNiagaraEmitter* O,FVersionedNiagaraEmitterData* D,bool Latest){if(!O||!D||!Pin(O))return Fail(TEXT("niagara_factory_emitter_data_missing"),TEXT("Expected current versioned emitter data."));for(const auto& E:Emitters)if(E.Emitter.Get()==O&&E.Data==D)return true;FEmitter E;E.Emitter=O;E.Data=D;E.Version=D->Version.VersionGuid;E.Latest=Latest;E.UniqueName=O->GetUniqueEmitterName();E.PropertyState=EmitterPropertyState(D);E.Source=D->GraphSource;E.Parent=D->VersionedParent.Emitter;E.MergeParent=D->VersionedParentAtLastMerge.Emitter;if(!Graph(E.Source.Get()))return false;if(D->GetRenderers().Num()>4096)return Fail(TEXT("niagara_factory_renderer_bound"),TEXT("Renderer bound exceeded."));for(UNiagaraRendererProperties* P:D->GetRenderers()){E.Renderers.Add(P);if(P&&!Pin(P))return false;if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(P)){ExpectedMaterials.Add(Sprite,Sprite->Material);if(Sprite->Material&&!Pin(Sprite->Material))return false;}}TArray<UNiagaraScript*> A;D->GetScripts(A,false,false);if(A.Num()>4096)return Fail(TEXT("niagara_factory_script_bound"),TEXT("Script bound exceeded."));for(auto* P:A){E.Scripts.Add(P);if(!Script(P))return false;}Emitters.Add(MoveTemp(E));return Admit(TEXT("emitter_captured"));}
 bool System(UNiagaraSystem* O,int32 ExpectedCount){if(!Pin(O)||O->GetEmitterHandles().Num()!=ExpectedCount)return Fail(TEXT("niagara_factory_system_shape_invalid"),TEXT("Factory output emitter count differs from requested construction."));FSystem E;E.System=O;E.Spawn=O->GetSystemSpawnScript();E.Update=O->GetSystemUpdateScript();if(!Script(E.Spawn.Get())||!Script(E.Update.Get()))return false;for(const auto& H:O->GetEmitterHandles()){if(!H.GetInstance().Emitter||!H.GetEmitterData()||!H.GetInstance().Emitter->IsIn(O)||H.GetInstance().Emitter->GetPackage()!=O->GetPackage())return Fail(TEXT("niagara_factory_output_owner_invalid"),TEXT("Factory output emitter is external or invalid."));FHandle F;F.Id=H.GetId();F.Version=H.GetInstance().Version;F.Emitter=H.GetInstance().Emitter;F.Data=H.GetEmitterData();E.Handles.Add(F);if(!Emitter(F.Emitter.Get(),F.Data,false))return false;}Systems.Add(MoveTemp(E));return Admit(TEXT("system_captured"));}
 TSharedPtr<FJsonObject> DirtyProjection(){auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;const bool CanRead=(!DeferredMode||(R.Error.IsEmpty()&&CapturedEditor.Get()==GEditor))&&Safe()&&WorldCaptured&&Live(World.Get())&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get();for(const auto& I:Objects){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),I->Path);J->SetStringField(TEXT("package"),I->PackageName);J->SetBoolField(TEXT("dirty_before"),I->DirtyBefore);const bool Exact=CanRead&&I->Exact();J->SetBoolField(TEXT("current_observation_available"),Exact);if(Exact)J->SetBoolField(TEXT("dirty_after"),I->Package->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("objects"),Rows);O->SetBoolField(TEXT("safe_context"),CanRead);return O;}
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("read_continuation"));}

FString GetStringParam(const TSharedPtr<FJsonObject>& O,const TCHAR* K,const FString& D){FString V=D;if(O)O->TryGetStringField(K,V);return V;}
bool GetBoolParam(const TSharedPtr<FJsonObject>& O,const TCHAR* K,bool D){bool V=D;if(O)O->TryGetBoolField(K,V);return V;}
double GetNumberParam(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double D){double V=D;if(O)O->TryGetNumberField(K,V);return V;}
int32 BoundInt(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double D,int32 Min,int32 Max,bool Floor=false){const double V=GetNumberParam(O,K,D);if(!FMath::IsFinite(V)||V<MIN_int32||V>MAX_int32){Current->Fail(TEXT("niagara_factory_numeric_range_invalid"),FString(K)+TEXT(" requires a finite safe int32 conversion."));return Min;}return FMath::Clamp(Floor?FMath::FloorToInt(V):static_cast<int32>(V),Min,Max);}
bool NoEmbeddedNul(const FString& S)
{for(int32 I=0;I<S.Len();++I){if(S[I]==TCHAR(0)){return false;}}return true;}
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

template<typename T>FString NiagaraEnumValueName(T Value){const UEnum* E=StaticEnum<T>();return E?E->GetNameStringByValue(static_cast<int64>(Value)):FString::FromInt(static_cast<int32>(Value));}
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

FString NiagaraScriptUsageName(ENiagaraScriptUsage Usage)
{
    if (const UEnum* Enum = StaticEnum<ENiagaraScriptUsage>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Usage));
    }
    return FString::FromInt(static_cast<int32>(Usage));
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

    if(!ContinueRead()||!Current->Budget(1,Pin->PinName.ToString().Len()+Pin->DefaultValue.Len()))return PinObject;
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
    if(!ContinueRead()||!Current->Budget(1,Variable.GetName().ToString().Len()+NiagaraTypeName(Variable.GetType()).Len()))return VariableObject;
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

TSharedPtr<FJsonObject> BuildFunctionCallJson(const UNiagaraNodeFunctionCall* FunctionCall, int32 NodeIndex, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!FunctionCall)
    {
        return NodeObject;
    }

    if(!ContinueRead()||!Current->Budget(FunctionCall->Signature.Inputs.Num()+FunctionCall->Signature.Outputs.Num(),FunctionCall->GetName().Len()))return NodeObject;
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
    if(!ContinueRead())return ObjectJson;
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
    if(!ContinueRead())return nullptr;
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
    if(!ContinueRead())return GraphObject;
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
        if(!ContinueRead())return GraphObject;
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
        if(!ContinueRead())return GraphObject;
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

TSharedPtr<FJsonValue> NumberArrayToJson(std::initializer_list<double> Values){TArray<TSharedPtr<FJsonValue>> A;for(double V:Values){if(!FMath::IsFinite(V)){Current->Fail(TEXT("niagara_factory_nonfinite_observation"),TEXT("Nonfinite observed value."));return MakeShared<FJsonValueNull>();}A.Add(MakeShared<FJsonValueNumber>(V));}return MakeShared<FJsonValueArray>(A);}
TArray<TSharedPtr<FJsonValue>> NumberArrayToJsonValues(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (double Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return JsonValues;
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

TSharedPtr<FJsonObject> NiagaraVariableWithDataToJsonObject(const FNiagaraVariable& Variable)
{
    TSharedPtr<FJsonObject> VariableObject = NiagaraVariableToJsonObject(Variable);
    VariableObject->SetBoolField(TEXT("has_data"), Variable.IsDataAllocated());
    VariableObject->SetNumberField(TEXT("data_size_bytes"), Variable.GetAllocatedSizeInBytes());
    VariableObject->SetField(TEXT("value"), NiagaraVariableDataToJsonValue(Variable));
    return VariableObject;
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
    BindingObject->SetStringField(TEXT("display_name"), Binding.GetName().ToString());
    BindingObject->SetStringField(TEXT("default_value_string"), Binding.GetDefaultValueString());
    return BindingObject;
}

TSharedPtr<FJsonObject> NiagaraParameterBindingToJsonObject(FNiagaraParameterBinding& Binding)
{
    TSharedPtr<FJsonObject> BindingObject = MakeShared<FJsonObject>();
    BindingObject->SetObjectField(TEXT("resolved_parameter"), NiagaraVariableToJsonObject(Binding.ResolvedParameter));
    BindingObject->SetObjectField(TEXT("aliased_parameter"), NiagaraVariableToJsonObject(Binding.AliasedParameter));
    BindingObject->SetObjectField(TEXT("default_aliased_parameter"), NiagaraVariableWithDataToJsonObject(Binding.GetDefaultAliasedParameter()));
    BindingObject->SetObjectField(TEXT("default_resolved_parameter"), NiagaraVariableWithDataToJsonObject(Binding.GetDefaultResolvedParameter()));
    BindingObject->SetBoolField(TEXT("is_set_to_default"), false);
    BindingObject->SetStringField(TEXT("is_set_to_default_note"), TEXT("unavailable_in_ue_5_6"));
    BindingObject->SetStringField(TEXT("display_string"), Binding.ToString());
    BindingObject->SetBoolField(TEXT("allow_user_parameters"), Binding.AllowUserParameters());
    BindingObject->SetBoolField(TEXT("allow_system_parameters"), Binding.AllowSystemParameters());
    BindingObject->SetBoolField(TEXT("allow_emitter_parameters"), Binding.AllowEmitterParameters());
    BindingObject->SetBoolField(TEXT("allow_particle_parameters"), Binding.AllowParticleParameters());
    BindingObject->SetBoolField(TEXT("allow_static_variables"), Binding.AllowStaticVariables());
    return BindingObject;
}

TSharedPtr<FJsonObject> NiagaraMeshPropertiesBaseToJsonObject(const FNiagaraMeshRendererMeshPropertiesBase& MeshProperties)
{
    TSharedPtr<FJsonObject> MeshObject = MakeShared<FJsonObject>();
    MeshObject->SetStringField(TEXT("mesh"), NiagaraObjectPathOrEmpty(MeshProperties.Mesh.Get()));
    MeshObject->SetArrayField(TEXT("scale"), NumberArrayToJsonValues({ MeshProperties.Scale.X, MeshProperties.Scale.Y, MeshProperties.Scale.Z }));
    MeshObject->SetArrayField(TEXT("rotation"), NumberArrayToJsonValues({ MeshProperties.Rotation.Pitch, MeshProperties.Rotation.Yaw, MeshProperties.Rotation.Roll }));
    MeshObject->SetArrayField(TEXT("pivot_offset"), NumberArrayToJsonValues({ MeshProperties.PivotOffset.X, MeshProperties.PivotOffset.Y, MeshProperties.PivotOffset.Z }));
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

FString RendererMaterialField(const UNiagaraRendererProperties* Renderer)
{
    if (Renderer->IsA<UNiagaraSpriteRendererProperties>())
    {
        return TEXT("Material");
    }
    if (Renderer->IsA<UNiagaraRibbonRendererProperties>())
    {
        return TEXT("Material");
    }
    if (Renderer->IsA<UNiagaraMeshRendererProperties>())
    {
        return TEXT("OverrideMaterials[material_slot].ExplicitMat");
    }
    if (Renderer->IsA<UNiagaraDecalRendererProperties>() || Renderer->IsA<UNiagaraVolumeRendererProperties>())
    {
        return TEXT("Material");
    }
    return FString();
}

TArray<TSharedPtr<FJsonValue>> MaterialArrayToJson(const TArray<UMaterialInterface*>& Materials)
{
    TArray<TSharedPtr<FJsonValue>> MaterialValues;
    for (const UMaterialInterface* Material : Materials)
    {
        MaterialValues.Add(MakeShared<FJsonValueString>(NiagaraObjectPathOrEmpty(Material)));
    }
    return MaterialValues;
}

TSharedPtr<FJsonObject> BuildRendererJson(FWork& W,
    const UNiagaraSystem* System,
    const FString& EmitterName,
    int32 EmitterIndex,
    const UNiagaraRendererProperties* Renderer,
    int32 RendererIndex)
{
    TSharedPtr<FJsonObject> RendererObject = MakeShared<FJsonObject>();
    if(!W.Admit(TEXT("renderer_projection"))){RendererObject->SetBoolField(TEXT("projection_skipped"),true);return RendererObject;}
    RendererObject->SetStringField(TEXT("system_path"), System ? System->GetPathName() : FString());
    RendererObject->SetStringField(TEXT("emitter_name"), EmitterName);
    RendererObject->SetNumberField(TEXT("emitter_index"), EmitterIndex);
    RendererObject->SetNumberField(TEXT("renderer_index"), RendererIndex);
    RendererObject->SetStringField(TEXT("renderer_class"), Renderer ? Renderer->GetClass()->GetName() : FString());
    RendererObject->SetStringField(TEXT("renderer_name"), Renderer ? Renderer->GetName() : FString());
    RendererObject->SetBoolField(TEXT("enabled"), Renderer ? Renderer->GetIsEnabled() : false);
    RendererObject->SetStringField(TEXT("material_field"), Renderer ? RendererMaterialField(Renderer) : FString());

    TArray<UMaterialInterface*> UsedMaterials;
    if (Renderer)
    {
        if(!W.SDK(TEXT("get_used_materials"))){return RendererObject;}Renderer->GetUsedMaterials(nullptr, UsedMaterials);if(!W.After(TEXT("get_used_materials"))){return RendererObject;}
    }
    for(auto* M:UsedMaterials){if(M&&!W.Pin(M))return RendererObject;}
    RendererObject->SetArrayField(TEXT("used_materials"), MaterialArrayToJson(UsedMaterials));

    if (const UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Renderer))
    {
        RendererObject->SetStringField(TEXT("primary_material"), NiagaraObjectPathOrEmpty(SpriteRenderer->Material));
        RendererObject->SetStringField(TEXT("source_mode"), NiagaraEnumValueName(SpriteRenderer->SourceMode));
        RendererObject->SetStringField(TEXT("alignment"), NiagaraEnumValueName(SpriteRenderer->Alignment));
        RendererObject->SetStringField(TEXT("facing_mode"), NiagaraEnumValueName(SpriteRenderer->FacingMode));
        RendererObject->SetStringField(TEXT("sort_mode"), NiagaraEnumValueName(SpriteRenderer->SortMode));
        RendererObject->SetNumberField(TEXT("sub_image_size_x"), SpriteRenderer->SubImageSize.X);
        RendererObject->SetNumberField(TEXT("sub_image_size_y"), SpriteRenderer->SubImageSize.Y);
        RendererObject->SetBoolField(TEXT("sub_image_blend"), SpriteRenderer->bSubImageBlend != 0);
        RendererObject->SetBoolField(TEXT("enable_camera_distance_culling"), SpriteRenderer->bEnableCameraDistanceCulling != 0);
        RendererObject->SetNumberField(TEXT("min_camera_distance"), SpriteRenderer->MinCameraDistance);
        RendererObject->SetNumberField(TEXT("max_camera_distance"), SpriteRenderer->MaxCameraDistance);

        TSharedPtr<FJsonObject> BindingsObject = MakeShared<FJsonObject>();
        BindingsObject->SetObjectField(TEXT("position"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->PositionBinding));
        BindingsObject->SetObjectField(TEXT("color"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->ColorBinding));
        BindingsObject->SetObjectField(TEXT("velocity"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->VelocityBinding));
        BindingsObject->SetObjectField(TEXT("sprite_rotation"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->SpriteRotationBinding));
        BindingsObject->SetObjectField(TEXT("sprite_size"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->SpriteSizeBinding));
        BindingsObject->SetObjectField(TEXT("sprite_facing"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->SpriteFacingBinding));
        BindingsObject->SetObjectField(TEXT("sprite_alignment"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->SpriteAlignmentBinding));
        BindingsObject->SetObjectField(TEXT("sub_image_index"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->SubImageIndexBinding));
        BindingsObject->SetObjectField(TEXT("camera_offset"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->CameraOffsetBinding));
        BindingsObject->SetObjectField(TEXT("uv_scale"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->UVScaleBinding));
        BindingsObject->SetObjectField(TEXT("pivot_offset"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->PivotOffsetBinding));
        BindingsObject->SetObjectField(TEXT("normalized_age"), NiagaraAttributeBindingToJsonObject(SpriteRenderer->NormalizedAgeBinding));
        RendererObject->SetObjectField(TEXT("sprite_bindings"), BindingsObject);
    }
    else if (const UNiagaraRibbonRendererProperties* RibbonRenderer = Cast<UNiagaraRibbonRendererProperties>(Renderer))
    {
        RendererObject->SetStringField(TEXT("primary_material"), NiagaraObjectPathOrEmpty(RibbonRenderer->Material));
    }
    else if (const UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
    {
        TArray<TSharedPtr<FJsonValue>> MeshValues;
        for (const FNiagaraMeshRendererMeshProperties& MeshProperties : MeshRenderer->Meshes)
        {
            MeshValues.Add(MakeShared<FJsonValueObject>(NiagaraMeshPropertiesBaseToJsonObject(MeshProperties)));
        }

        TArray<TSharedPtr<FJsonValue>> OverrideValues;
        for (int32 OverrideIndex = 0; OverrideIndex < MeshRenderer->OverrideMaterials.Num(); ++OverrideIndex)
        {
            TSharedPtr<FJsonObject> OverrideObject = MakeShared<FJsonObject>();
            OverrideObject->SetNumberField(TEXT("slot_index"), OverrideIndex);
            OverrideObject->SetStringField(TEXT("explicit_material"), NiagaraObjectPathOrEmpty(MeshRenderer->OverrideMaterials[OverrideIndex].ExplicitMat));
            OverrideValues.Add(MakeShared<FJsonValueObject>(OverrideObject));
        }
        FNiagaraParameterBinding MeshesBinding = MeshRenderer->MeshesBinding;
        RendererObject->SetArrayField(TEXT("meshes"), MeshValues);
        RendererObject->SetObjectField(TEXT("meshes_binding"), NiagaraParameterBindingToJsonObject(MeshesBinding));
        RendererObject->SetObjectField(TEXT("mesh_index_binding"), NiagaraAttributeBindingToJsonObject(MeshRenderer->MeshIndexBinding));
        RendererObject->SetBoolField(TEXT("override_materials_enabled"), MeshRenderer->bOverrideMaterials != 0);
        RendererObject->SetArrayField(TEXT("override_materials"), OverrideValues);
    }
    else if (const UNiagaraDecalRendererProperties* DecalRenderer = Cast<UNiagaraDecalRendererProperties>(Renderer))
    {
        RendererObject->SetStringField(TEXT("primary_material"), NiagaraObjectPathOrEmpty(DecalRenderer->Material));
    }
    else if (const UNiagaraVolumeRendererProperties* VolumeRenderer = Cast<UNiagaraVolumeRendererProperties>(Renderer))
    {
        RendererObject->SetStringField(TEXT("primary_material"), NiagaraObjectPathOrEmpty(VolumeRenderer->Material));
    }

    return RendererObject;
}

TSharedPtr<FJsonObject> NiagaraEmitterPropertiesToJsonObject(const FVersionedNiagaraEmitterData* Data)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (Data)
    {
        Obj->SetBoolField(TEXT("local_space"), Data->bLocalSpace);
        Obj->SetStringField(TEXT("sim_target"), NiagaraEnumValueName(Data->SimTarget));
        Obj->SetBoolField(TEXT("requires_persistent_ids"), Data->bRequiresPersistentIDs != 0);
        Obj->SetStringField(TEXT("calculate_bounds_mode"), NiagaraEnumValueName(Data->CalculateBoundsMode));
        Obj->SetBoolField(TEXT("determinism"), Data->bDeterminism);
        Obj->SetNumberField(TEXT("random_seed"), Data->RandomSeed);

        // Execution-gate diagnostics: a silently Disabled emitter instance is usually one of
        // scalability (Platforms.QualityLevelMask), the transient bIsAllowedToExecute cache,
        // or missing system EmitterCompiledData (reported at the system level by callers).
        Obj->SetNumberField(TEXT("scalability_quality_level_mask"), Data->Platforms.QualityLevelMask);
        Obj->SetBoolField(TEXT("scalability_platform_set_active"), Data->Platforms.IsActive());
        Obj->SetBoolField(TEXT("is_allowed_by_scalability"), Data->IsAllowedByScalability());
        Obj->SetBoolField(TEXT("cached_is_allowed_to_execute"), Data->IsAllowedToExecute());

        TSharedPtr<FJsonObject> Bounds = MakeShared<FJsonObject>();
        Bounds->SetBoolField(TEXT("is_valid"), Data->FixedBounds.IsValid != 0);
        TSharedPtr<FJsonObject> MinObj = MakeShared<FJsonObject>();
        MinObj->SetNumberField(TEXT("x"), Data->FixedBounds.Min.X);
        MinObj->SetNumberField(TEXT("y"), Data->FixedBounds.Min.Y);
        MinObj->SetNumberField(TEXT("z"), Data->FixedBounds.Min.Z);
        TSharedPtr<FJsonObject> MaxObj = MakeShared<FJsonObject>();
        MaxObj->SetNumberField(TEXT("x"), Data->FixedBounds.Max.X);
        MaxObj->SetNumberField(TEXT("y"), Data->FixedBounds.Max.Y);
        MaxObj->SetNumberField(TEXT("z"), Data->FixedBounds.Max.Z);
        Bounds->SetObjectField(TEXT("min"), MinObj);
        Bounds->SetObjectField(TEXT("max"), MaxObj);
        Obj->SetObjectField(TEXT("fixed_bounds"), Bounds);
    }
    return Obj;
}

int32 CountEmitterParentReferences(const UNiagaraSystem* System)
{
    if (!System)
    {
        return 0;
    }

    int32 ParentReferenceCount = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        if (const FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
        {
            if (EmitterData->VersionedParent.Emitter != nullptr)
            {
                ++ParentReferenceCount;
            }
            if (EmitterData->VersionedParentAtLastMerge.Emitter != nullptr)
            {
                ++ParentReferenceCount;
            }
        }
    }
    return ParentReferenceCount;
}


struct FTarget{FString Requested,Package,Folder,Name,Object,Filename;};
bool ResolveTarget(FWork& W,const FString& Path,bool Allow,FTarget& T)
{
 T.Requested=Path;T.Package=PackagePathFromObjectPath(Path);T.Package.ReplaceInline(TEXT("\\"),TEXT("/"));if(Forbidden(Path)||Forbidden(T.Package))return W.Fail(TEXT("niagara_factory_dependency_rejected"),TEXT("MCP/Cubeless/Toolset dependency reference is forbidden."));FText Why;if(!FPackageName::IsValidLongPackageName(T.Package,false,&Why))return W.Fail(TEXT("niagara_factory_target_invalid"),Why.ToString());T.Name=FPackageName::GetLongPackageAssetName(T.Package);if(T.Package.Len()>=NAME_SIZE||T.Name.Len()>=NAME_SIZE||T.Package.Len()+1+T.Name.Len()>=NAME_SIZE)return W.Fail(TEXT("niagara_factory_target_name_size"),TEXT("Expanded package/object path must fit installed FName NAME_SIZE before Engine lookup."));if(T.Name.IsEmpty()||!FName(*T.Name).IsValidObjectName(Why))return W.Fail(TEXT("niagara_factory_target_invalid"),TEXT("Invalid target asset object name."));if(!Allow&&!IsTempGeneratedNiagaraPath(T.Package))return W.Fail(TEXT("niagara_factory_target_root_rejected"),TEXT("Target requires /Game/_MCP_Temp/NiagaraGenerated/ unless allow_non_temp_path=True."));T.Folder=FPackageName::GetLongPackagePath(T.Package);T.Object=T.Package+TEXT(".")+T.Name;if(!FPackageName::TryConvertLongPackageNameToFilename(T.Package,T.Filename,FPackageName::GetAssetPackageExtension()))return W.Fail(TEXT("niagara_factory_target_invalid"),TEXT("Cannot resolve exact target filename."));W.RequestedObject=T.Object;W.RequestedPackage=T.Package;W.RequestedFilename=T.Filename;W.R.Data->SetStringField(TEXT("requested_object"),T.Object);W.R.Data->SetStringField(TEXT("requested_package"),T.Package);W.R.Data->SetStringField(TEXT("requested_filename"),T.Filename);return W.Admit(TEXT("target_resolved"));
}
bool FreshTarget(FWork& W,const FTarget& T)
{
 if(!W.Admit(TEXT("fresh_target")))return false;
 auto Clear=[&](){return !StaticFindObject(UObject::StaticClass(),nullptr,*T.Object)&&!FindPackage(nullptr,*T.Package)&&!FPackageName::DoesPackageExist(T.Package)&&!IFileManager::Get().FileExists(*T.Filename);};
 if(!Clear())return W.Fail(TEXT("niagara_factory_target_exists"),TEXT("Target object/package/file already exists; no overwrite or missing LoadObject probe."));
 FAssetRegistryModule* M=nullptr;if(!W.Step(TEXT("Load_AssetRegistry"),[&]{M=&FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));}))return false;
 bool Registered=false;if(!W.Step(TEXT("Registry_TargetExists"),[&]{Registered=M->Get().GetAssetByObjectPath(FSoftObjectPath(T.Object)).IsValid();}))return false;
 if(Registered||!Clear())return W.Fail(TEXT("niagara_factory_target_exists"),TEXT("Target collision appeared during registry/module callback."));return true;
}
UFactory* MakeFactory(FWork& W,bool ForEmitter,bool Defaults,UNiagaraEmitter* Source=nullptr,FGuid SourceVersion=FGuid())
{
 const FString ClassPath=ForEmitter?TEXT("/Script/NiagaraEditor.NiagaraEmitterFactoryNew"):TEXT("/Script/NiagaraEditor.NiagaraSystemFactoryNew");UClass* C=nullptr;if(!W.Step(TEXT("Load_FactoryClass"),[&]{C=LoadClass<UFactory>(nullptr,*ClassPath);}))return nullptr;if(!Live(C)||!C->IsChildOf(UFactory::StaticClass())||!C->GetPathName().Equals(ClassPath,ESearchCase::CaseSensitive)||C->HasAnyClassFlags(CLASS_Abstract|CLASS_NewerVersionExists)||!W.Pin(C))return W.Fail(TEXT("niagara_factory_class_unavailable"),TEXT("Expected exact public stock UFactory class.")),nullptr;
 UFactory* F=nullptr;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("kind"),ForEmitter?TEXT("transient_emitter_factory"):TEXT("transient_system_factory"));Row->SetStringField(TEXT("expected_class"),ClassPath);W.CreationRows.Add(MakeShared<FJsonValueObject>(Row));if(!W.SDK(TEXT("NewObject_Factory"),true))return nullptr;F=NewObject<UFactory>(GetTransientPackage(),C,NAME_None,RF_Transient);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),F!=nullptr);W.R.bChanged|=F!=nullptr;if(!W.After(TEXT("NewObject_Factory")))return nullptr;if(!F||F->GetClass()!=C||F->GetOuter()!=GetTransientPackage()||!F->HasAnyFlags(RF_Transient)||!W.Pin(F))return W.Fail(TEXT("niagara_factory_instance_invalid"),TEXT("New factory must be the exact transient stock class, never its CDO.")),nullptr;
 // Exact loaded UClass was verified before this public C++ field access. Do not reference unexported concrete StaticClass/ctor.
 if(ForEmitter){auto* E=static_cast<UNiagaraEmitterFactoryNew*>(F);E->EmitterToCopy=nullptr;E->bUseInheritance=false;E->bAddDefaultModulesAndRenderersToEmptyEmitter=Defaults;}else{auto* S=static_cast<UNiagaraSystemFactoryNew*>(F);S->SystemToCopy=nullptr;S->EmittersToAddToNewSystem.Reset();if(Source){S->EmittersToAddToNewSystem.Add(FVersionedNiagaraEmitter(Source,SourceVersion));W.ExpectedFactoryEmitters=S->EmittersToAddToNewSystem;}}
 W.Factory=F;W.FactoryClass=C;W.EmitterFactory=ForEmitter;W.FactoryDefaults=Defaults;return W.Admit(TEXT("factory_configured"))?F:nullptr;
}
UObject* CreateAsset(FWork& W,const FTarget& T,UClass* Class,UFactory* F,bool SystemFactory)
{
 FAssetToolsModule* Tools=nullptr;if(!W.Step(TEXT("Load_AssetTools"),[&]{Tools=&FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));}))return nullptr;if(!FreshTarget(W,T))return nullptr;
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("kind"),TEXT("persistent_factory_asset"));Row->SetStringField(TEXT("requested_object"),T.Object);Row->SetStringField(TEXT("requested_package"),T.Package);Row->SetStringField(TEXT("requested_filename"),T.Filename);Row->SetStringField(TEXT("expected_class"),Class->GetPathName());Row->SetBoolField(TEXT("actual_identity_verified"),false);Row->SetBoolField(TEXT("package_side_effect_possible_even_if_return_null"),true);W.CreationRows.Add(MakeShared<FJsonValueObject>(Row));
 if(SystemFactory){W.R.bCompileRequested=true;W.R.Data->SetBoolField(TEXT("factory_internal_compile_request_expected"),true);}
 UObject* O=nullptr;if(!W.SDK(TEXT("AssetTools_CreateAsset"),true,true))return nullptr;{TGuardValue<bool> Unattended(GIsRunningUnattendedScript,true);O=Tools->Get().CreateAsset(T.Name,T.Folder,Class,F,FName(TEXT("CubelessUE58NiagaraFactoryBootstrap")),false);}Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),O!=nullptr);W.R.bChanged|=O!=nullptr;if(SystemFactory&&O){W.R.bCompileAttempted=true;W.R.Compiled.Reset();Row->SetBoolField(TEXT("factory_internal_RequestCompile_return_path_reached"),true);}if(!W.After(TEXT("AssetTools_CreateAsset")))return nullptr;
 if(!O)return W.Fail(TEXT("niagara_factory_create_failed"),TEXT("CreateAsset returned null; package/registry/cache side effects may remain. No cleanup/retry.")),nullptr;
 if(O->GetClass()!=Class||!O->GetPathName().Equals(T.Object,ESearchCase::CaseSensitive)||O->GetOuter()!=O->GetOutermost()||!O->GetOutermost()->GetName().Equals(T.Package,ESearchCase::CaseSensitive)||O->GetPackage()!=O->GetOutermost()||!O->HasAllFlags(RF_Public|RF_Standalone)||O->HasAnyFlags(RF_Transient)||!W.Pin(O)||!W.Pin(O->GetOutermost()))return W.Fail(TEXT("niagara_factory_output_identity_invalid"),TEXT("Returned asset does not match exact requested stock class/outer/package/path/flags.")),nullptr;
 Row->SetBoolField(TEXT("actual_identity_verified"),true);Row->SetStringField(TEXT("actual_path"),O->GetPathName());W.R.Data->SetStringField(TEXT("created_asset_path"),O->GetPathName());W.R.Data->SetBoolField(TEXT("created_asset_retained"),true);return O;
}
bool PackageOwnership(FWork& W,UObject* Root,const TCHAR* Stage);
bool SaveAsset(FWork& W,UObject* Asset,const FTarget& T,FString& SavedFilename)
{
 if(!W.Admit(TEXT("save_admission")))return false;UPackage* P=Asset->GetOutermost();if(!Asset->GetPathName().Equals(T.Object,ESearchCase::CaseSensitive)||P==GetTransientPackage()||!P->GetName().Equals(T.Package,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_factory_save_target_changed"),TEXT("Exact captured output package is required for save."));
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("object_before"),Asset->GetPathName());Row->SetStringField(TEXT("package_before"),P->GetName());Row->SetStringField(TEXT("filename"),T.Filename);Row->SetBoolField(TEXT("dirty_before"),P->IsDirty());Row->SetBoolField(TEXT("attempted"),false);Row->SetBoolField(TEXT("SDK_returned"),false);W.SaveRows.Add(MakeShared<FJsonValueObject>(Row));
 if(!W.SDK(TEXT("UPackage_SavePackage"),true,true))return false;W.R.bSaveAttempted=true;Row->SetBoolField(TEXT("attempted"),true);FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;const bool Returned=UPackage::SavePackage(P,Asset,*T.Filename,Args);W.R.bSaved|=Returned;Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_success"),Returned);if(Returned){SavedFilename=T.Filename;W.R.Data->SetStringField(TEXT("actual_saved_filename"),SavedFilename);}if(!W.After(TEXT("UPackage_SavePackage")))return false;
 const bool Identity=Asset->GetOutermost()==P&&Asset->GetPackage()==P&&P->GetName().Equals(T.Package,ESearchCase::CaseSensitive)&&Asset->GetPathName().Equals(T.Object,ESearchCase::CaseSensitive);const bool Clean=!P->IsDirty();Row->SetStringField(TEXT("package_after"),P->GetName());Row->SetBoolField(TEXT("identity_verified"),Identity);Row->SetBoolField(TEXT("dirty_after"),!Clean);Row->SetBoolField(TEXT("save_completed"),Returned&&Identity&&Clean);if(!Returned||!Identity||!Clean)return W.Fail(TEXT("niagara_factory_save_postcondition_failed"),TEXT("Save return, exact package identity and actual clean readback did not all pass; SDK true history retained."));return true;
}
TSharedPtr<FJsonObject> CompileSnapshot(FWork& W,UNiagaraSystem* S,double Timeout,double Poll)
{
 auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),false);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0);bool Outstanding=false,Active=false;if(!W.Step(TEXT("System_CompileStatus"),[&]{Outstanding=S->HasOutstandingCompilationRequests(true);Active=S->HasActiveCompilations();}))return O;O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);O->SetBoolField(TEXT("active_compilations_after"),Active);O->SetBoolField(TEXT("completion_verified"),false);O->SetStringField(TEXT("scope"),TEXT("Single public status snapshot; no wrapper polling/wait/flush. Factory/internal SDK costs are not guaranteed nonblocking."));return O;
}
TSharedPtr<FJsonObject> DependencyAudit(FWork& W,UObject* Asset,const FString& File,bool BlockTemp)
{
 auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetBoolField(TEXT("audit_complete"),false);O->SetBoolField(TEXT("asset_resolved"),Asset!=nullptr);O->SetBoolField(TEXT("package_name_present"),false);O->SetBoolField(TEXT("dependency_query_attempted"),false);O->SetBoolField(TEXT("dependency_query_succeeded"),false);O->SetBoolField(TEXT("saved_file_scanned"),false);O->SetNumberField(TEXT("dependency_count"),0);O->SetNumberField(TEXT("forbidden_dependency_hit_count"),0);O->SetNumberField(TEXT("temp_dependency_hit_count"),0);O->SetBoolField(TEXT("has_forbidden_dependency"),false);O->SetBoolField(TEXT("has_temp_dependency"),false);if(!W.Admit(TEXT("dependency_audit"))||!Asset)return O;const FString Package=Asset->GetOutermost()->GetName(),Path=Asset->GetPathName();O->SetStringField(TEXT("package_name"),Package);O->SetStringField(TEXT("asset_path"),Path);O->SetStringField(TEXT("saved_filename"),File);O->SetBoolField(TEXT("package_name_present"),!Package.IsEmpty());O->SetArrayField(TEXT("additional_forbidden_dependency_prefixes"),TArray<TSharedPtr<FJsonValue>>{});FAssetRegistryModule* M=nullptr;if(!W.Step(TEXT("Load_AssetRegistry_Audit"),[&]{M=&FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));}))return O;if(!File.IsEmpty()){if(!W.Step(TEXT("Registry_ScanModifiedAssetFiles"),[&]{M->Get().ScanModifiedAssetFiles({File});}))return O;O->SetBoolField(TEXT("saved_file_scanned"),true);}TArray<FName> Deps;bool Query=false;O->SetBoolField(TEXT("dependency_query_attempted"),true);if(!W.Step(TEXT("Registry_GetDependencies"),[&]{Query=M->Get().GetDependencies(FName(*Package),Deps);}))return O;O->SetBoolField(TEXT("dependency_query_succeeded"),Query);O->SetBoolField(TEXT("audit_complete"),Query);O->SetBoolField(TEXT("success"),Query);if(Deps.Num()>65536){W.Fail(TEXT("niagara_factory_dependency_bound"),TEXT("Dependency list exceeds output bound."));return O;}TArray<TSharedPtr<FJsonValue>> Values,ForbiddenHits,TempHits;for(const FName& N:Deps){const FString D=N.ToString();if(!W.Budget(1,D.Len()))return O;Values.Add(MakeShared<FJsonValueString>(D));if(Forbidden(D))ForbiddenHits.Add(MakeShared<FJsonValueString>(D));if(BlockTemp&&(D.Equals(TEXT("/Game/_MCP_Temp"),ESearchCase::IgnoreCase)||D.StartsWith(TEXT("/Game/_MCP_Temp/"),ESearchCase::IgnoreCase)))TempHits.Add(MakeShared<FJsonValueString>(D));}if(Forbidden(Path))ForbiddenHits.Add(MakeShared<FJsonValueString>(Path));O->SetNumberField(TEXT("dependency_count"),Values.Num());O->SetNumberField(TEXT("forbidden_dependency_hit_count"),ForbiddenHits.Num());O->SetNumberField(TEXT("temp_dependency_hit_count"),TempHits.Num());O->SetBoolField(TEXT("has_forbidden_dependency"),!ForbiddenHits.IsEmpty());O->SetBoolField(TEXT("has_temp_dependency"),!TempHits.IsEmpty());O->SetArrayField(TEXT("dependencies"),Values);O->SetArrayField(TEXT("forbidden_dependency_hits"),ForbiddenHits);O->SetArrayField(TEXT("temp_dependency_hits"),TempHits);O->SetStringField(TEXT("audit_scope"),TEXT("Direct AssetRegistry package dependencies; not a transitive closure/cold reload/runtime execution proof."));W.R.Data->SetObjectField(TEXT("dependency_audit"),O);const bool PersistenceDeferred=!W.R.bSaveRequested&&File.IsEmpty();O->SetBoolField(TEXT("persistence_deferred"),PersistenceDeferred);O->SetBoolField(TEXT("direct_saved_package_dependency_query_complete"),Query&&!File.IsEmpty());W.R.Data->SetBoolField(TEXT("unsaved_construction_dependency_audit_deferred"),PersistenceDeferred);if(PersistenceDeferred)W.R.Data->SetStringField(TEXT("unsaved_construction_scope"),TEXT("Exact typed creation/ownership/settings only. Registry audit remains incomplete for an unsaved package; no persisted/transitive/RAM/runtime dependency proof is claimed. Final composite save/audit remains separate."));if((W.R.bSaveRequested&&!Query)||!ForbiddenHits.IsEmpty()||!TempHits.IsEmpty())W.Fail(TEXT("niagara_factory_dependency_audit_incomplete"),TEXT("Requested persisted dependency audit missing or contains forbidden/temp references; legacy result and actual asset/save prefix retained."));return O;
}



UNiagaraSystem* TransientSystem(FWork& W,bool Defaults)
{
 UNiagaraSystem* S=nullptr;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("kind"),TEXT("transient_system"));W.CreationRows.Add(MakeShared<FJsonValueObject>(Row));if(!W.SDK(TEXT("NewObject_TransientSystem"),true))return nullptr;S=NewObject<UNiagaraSystem>(GetTransientPackage(),NAME_None,RF_Transient|RF_Transactional);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),S!=nullptr);W.R.bChanged|=S!=nullptr;if(!W.After(TEXT("NewObject_TransientSystem")))return nullptr;if(!S||S->GetClass()!=UNiagaraSystem::StaticClass()||S->GetOuter()!=GetTransientPackage()||!S->HasAnyFlags(RF_Transient)||!W.Pin(S))return W.Fail(TEXT("niagara_factory_transient_system_invalid"),TEXT("Transient System creation identity failed.")),nullptr;
 if(!W.Step(TEXT("InitializeSystem"),[&]{UNiagaraSystemFactoryNew::InitializeSystem(S,Defaults);},true))return nullptr;return W.System(S,0)?S:nullptr;
}
UNiagaraEmitter* TransientEmitter(FWork& W,const FString& Name,bool Defaults,bool InspectorTransient)
{
 UNiagaraEmitter* E=nullptr;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("kind"),TEXT("transient_emitter"));Row->SetBoolField(TEXT("RF_Transient_requested"),InspectorTransient);Row->SetStringField(TEXT("requested_unique_emitter_name"),Name);W.CreationRows.Add(MakeShared<FJsonValueObject>(Row));if(!W.SDK(TEXT("NewObject_TransientEmitter"),true))return nullptr;const FName Unique=MakeUniqueObjectName(GetTransientPackage(),UNiagaraEmitter::StaticClass(),FName(*Name));E=NewObject<UNiagaraEmitter>(GetTransientPackage(),Unique,InspectorTransient?(RF_Transient|RF_Transactional):RF_Transactional);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("returned_nonnull"),E!=nullptr);W.R.bChanged|=E!=nullptr;if(!W.After(TEXT("NewObject_TransientEmitter")))return nullptr;if(!E||E->GetClass()!=UNiagaraEmitter::StaticClass()||E->GetOuter()!=GetTransientPackage()||E->HasAnyFlags(RF_Transient)!=InspectorTransient||!W.Pin(E))return W.Fail(TEXT("niagara_factory_transient_emitter_invalid"),TEXT("Working emitter must be exact stock class/transient outer/expected persistence flags.")),nullptr;
 FIdentity* RenameIdentity=nullptr;
 for(const auto& Captured:W.Objects)if(Captured->Object.Get()==E){RenameIdentity=Captured.Get();break;}
 if(!RenameIdentity||!RenameIdentity->Exact())return W.Fail(TEXT("niagara_factory_rename_identity_missing"),TEXT("Exact newly created working emitter identity required before logical-name SDK.")),nullptr;
 const EObjectFlags RenameFlags=E->GetFlags();const FString RenameOldPath=RenameIdentity->Path;Row->SetStringField(TEXT("object_path_before_logical_name"),RenameOldPath);bool RenameAdmitted=false;
 if(!W.Step(TEXT("SetUniqueEmitterName"),[&]{
     E->SetUniqueEmitterName(Name);
     Row->SetBoolField(TEXT("logical_name_SDK_returned"),true);
     // This SDK intentionally renames only this fresh transient emitter. Admit its
     // exact immutable identity before adopting its new path; never refresh peers.
     if(!Safe()||!W.WorldCaptured||!Live(W.World.Get())||W.World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=W.World.Get()
         ||!Live(E)||RenameIdentity->Object.Get()!=E||!Live(RenameIdentity->Class.Get())||RenameIdentity->Class->HasAnyClassFlags(CLASS_NewerVersionExists)
         ||E->GetClass()!=RenameIdentity->Class.Get()||E->GetOuter()!=RenameIdentity->Outer.Get()||E->GetOutermost()!=RenameIdentity->Package.Get()
         ||E->GetPackage()!=RenameIdentity->Effective.Get()||!Live(RenameIdentity->Package.Get())||!E->GetOutermost()->GetName().Equals(RenameIdentity->PackageName,ESearchCase::CaseSensitive)||E->GetFlags()!=RenameFlags||!E->GetUniqueEmitterName().Equals(Name,ESearchCase::CaseSensitive)
         ||!E->GetFName().GetPlainNameString().Equals(FName(*Name).GetPlainNameString(),ESearchCase::CaseSensitive))
     {W.Fail(TEXT("niagara_factory_rename_identity_changed"),TEXT("Logical-name SDK did not retain exact fresh emitter owner/class/package/flags/name stem; replacement not adopted."));return;}
     const FString RenameNewPath=E->GetPathName();Row->SetStringField(TEXT("object_path_after_logical_name"),RenameNewPath);
     if(!W.Budget(1,RenameNewPath.Len()))return;
     RenameIdentity->Path=RenameNewPath;RenameAdmitted=true;Row->SetBoolField(TEXT("expected_fresh_emitter_rename_admitted"),true);
 },true)||!RenameAdmitted)return nullptr;if(!InspectorTransient){if(!W.Step(TEXT("SetWorkingEmitterInheritance"),[&]{E->bIsInheritable=false;},true))return nullptr;}if(!W.Step(TEXT("InitializeEmitter"),[&]{UNiagaraEmitterFactoryNew::InitializeEmitter(E,Defaults);},true))return nullptr;return W.Emitter(E,E->GetLatestEmitterData(),true)?E:nullptr;
}

TSharedPtr<FJsonObject> HandleInspectDefaults(FWork& W,const TSharedPtr<FJsonObject>& Params)
{

    FString Kind = TEXT("all");
    if (Params.IsValid())
    {
        Params->TryGetStringField(TEXT("kind"), Kind);
    }
    Kind.TrimStartAndEndInline();
    const FString KindLower = Kind.IsEmpty() ? TEXT("all") : Kind.ToLower();

    const bool bInspectSystem = KindLower == TEXT("all") || KindLower == TEXT("system");
    const bool bInspectParamEmitter = KindLower == TEXT("emitter");
    const bool bInspectEmptyEmitter = KindLower == TEXT("all") || KindLower == TEXT("empty_emitter");
    const bool bInspectBasicSpriteEmitter = KindLower == TEXT("all") || KindLower == TEXT("basic_sprite_emitter") || KindLower == TEXT("sprite_emitter");

    if (!bInspectSystem && !bInspectParamEmitter && !bInspectEmptyEmitter && !bInspectBasicSpriteEmitter)
    {
        return W.Error(
            TEXT("'kind' must be one of: all, system, emitter, empty_emitter, basic_sprite_emitter"));
    }

    bool bCreateDefaultSystemNodes = true;
    bool bParamEmitterAddsDefaultModulesAndRenderers = false;
    bool bIncludePins = false;
    int32 MaxFunctionCalls = 80;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("create_default_system_nodes"), bCreateDefaultSystemNodes);
        Params->TryGetBoolField(TEXT("add_default_modules_and_renderers"), bParamEmitterAddsDefaultModulesAndRenderers);
        Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);
        MaxFunctionCalls=BoundInt(Params,TEXT("max_function_calls"),80,0,500);
    }
    MaxFunctionCalls = FMath::Clamp(MaxFunctionCalls, 0, 500);

    auto BuildScriptSummary =
        [&W,bIncludePins, MaxFunctionCalls](const UNiagaraScript* Script, const FString& Context) -> TSharedPtr<FJsonObject>
        {
            TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
            if(!W.Admit(TEXT("script_summary")))return ScriptObject;
            ScriptObject->SetStringField(TEXT("context"), Context);
            ScriptObject->SetBoolField(TEXT("is_valid"), Script != nullptr);
            if (!Script)
            {
                ScriptObject->SetObjectField(TEXT("graph"), BuildGraphAnalysisJson(nullptr, Context, bIncludePins, MaxFunctionCalls));
                return ScriptObject;
            }

            ScriptObject->SetStringField(TEXT("name"), Script->GetName());
            ScriptObject->SetStringField(TEXT("path"), Script->GetPathName());
            ScriptObject->SetStringField(TEXT("usage"), NiagaraScriptUsageName(Script->GetUsage()));
            ScriptObject->SetStringField(TEXT("usage_id"), Script->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
            ScriptObject->SetObjectField(TEXT("graph"), BuildGraphAnalysisJson(Script->GetLatestSource(), Context, bIncludePins, MaxFunctionCalls));
            return ScriptObject;
        };

    auto BuildSystemInspection = [&W,&BuildScriptSummary, bCreateDefaultSystemNodes]() -> TSharedPtr<FJsonObject>
    {
        UNiagaraSystem* System = TransientSystem(W,bCreateDefaultSystemNodes);
        TSharedPtr<FJsonObject> SystemObject = MakeShared<FJsonObject>();
        SystemObject->SetStringField(TEXT("variant"), TEXT("system"));
        SystemObject->SetBoolField(TEXT("created"), System != nullptr);
        SystemObject->SetBoolField(TEXT("create_default_system_nodes"), bCreateDefaultSystemNodes);
        if (!System)
        {
            return SystemObject;
        }



        TArray<TSharedPtr<FJsonValue>> ScriptValues;
        TSharedPtr<FJsonObject> SpawnScriptObject = BuildScriptSummary(System->GetSystemSpawnScript(), TEXT("system_spawn"));
        if(!W.Admit(TEXT("after_spawn_summary")))return SystemObject;
        TSharedPtr<FJsonObject> UpdateScriptObject = BuildScriptSummary(System->GetSystemUpdateScript(), TEXT("system_update"));
        if(!W.Admit(TEXT("after_system_script_summaries")))return SystemObject;
        ScriptValues.Add(MakeShared<FJsonValueObject>(SpawnScriptObject));
        ScriptValues.Add(MakeShared<FJsonValueObject>(UpdateScriptObject));

        auto AccumulateScriptGraphCounts =
            [](const TSharedPtr<FJsonObject>& ScriptObject, int32& TotalNodeCount, int32& TotalFunctionCallCount, int32& TotalOutputNodeCount)
            {
                const TSharedPtr<FJsonObject>* GraphJson = nullptr;
                if (ScriptObject.IsValid() && ScriptObject->TryGetObjectField(TEXT("graph"), GraphJson) && GraphJson && GraphJson->IsValid())
                {
                    TotalNodeCount += static_cast<int32>((*GraphJson)->GetNumberField(TEXT("node_count")));
                    TotalFunctionCallCount += static_cast<int32>((*GraphJson)->GetNumberField(TEXT("function_call_count")));
                    TotalOutputNodeCount += static_cast<int32>((*GraphJson)->GetNumberField(TEXT("output_node_count")));
                }
            };

        int32 TotalNodeCount = 0;
        int32 TotalFunctionCallCount = 0;
        int32 TotalOutputNodeCount = 0;
        AccumulateScriptGraphCounts(SpawnScriptObject, TotalNodeCount, TotalFunctionCallCount, TotalOutputNodeCount);
        AccumulateScriptGraphCounts(UpdateScriptObject, TotalNodeCount, TotalFunctionCallCount, TotalOutputNodeCount);

        SystemObject->SetStringField(TEXT("name"), System->GetName());
        SystemObject->SetStringField(TEXT("path"), System->GetPathName());
        SystemObject->SetStringField(TEXT("class"), System->GetClass() ? System->GetClass()->GetName() : FString());
        SystemObject->SetStringField(TEXT("outer_path"), NiagaraObjectPathOrEmpty(System->GetOuter()));
        SystemObject->SetBoolField(TEXT("outer_is_transient_package"), System->GetOutermost() == GetTransientPackage());
        SystemObject->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
        SystemObject->SetNumberField(TEXT("system_script_count"), ScriptValues.Num());
        SystemObject->SetNumberField(TEXT("total_node_count"), TotalNodeCount);
        SystemObject->SetNumberField(TEXT("total_function_call_count"), TotalFunctionCallCount);
        SystemObject->SetNumberField(TEXT("total_output_node_count"), TotalOutputNodeCount);
        SystemObject->SetArrayField(TEXT("system_scripts"), ScriptValues);
        return SystemObject;
    };

    auto BuildEmitterInspection =
        [&W, &BuildScriptSummary, bIncludePins, MaxFunctionCalls](const FString& Variant, bool bAddDefaultModulesAndRenderers) -> TSharedPtr<FJsonObject>
        {
            UNiagaraEmitter* Emitter = TransientEmitter(W,Variant,bAddDefaultModulesAndRenderers,true);
            TSharedPtr<FJsonObject> EmitterObject = MakeShared<FJsonObject>();
            EmitterObject->SetStringField(TEXT("variant"), Variant);
            EmitterObject->SetBoolField(TEXT("created"), Emitter != nullptr);
            EmitterObject->SetBoolField(TEXT("add_default_modules_and_renderers"), bAddDefaultModulesAndRenderers);
            if (!Emitter)
            {
                return EmitterObject;
            }



            FVersionedNiagaraEmitterData* EmitterData = Emitter->GetLatestEmitterData();
            EmitterObject->SetStringField(TEXT("name"), Emitter->GetName());
            EmitterObject->SetStringField(TEXT("path"), Emitter->GetPathName());
            EmitterObject->SetStringField(TEXT("class"), Emitter->GetClass() ? Emitter->GetClass()->GetName() : FString());
            EmitterObject->SetStringField(TEXT("unique_emitter_name"), Emitter->GetUniqueEmitterName());
            EmitterObject->SetStringField(TEXT("outer_path"), NiagaraObjectPathOrEmpty(Emitter->GetOuter()));
            EmitterObject->SetBoolField(TEXT("outer_is_transient_package"), Emitter->GetOutermost() == GetTransientPackage());
            EmitterObject->SetBoolField(TEXT("has_emitter_data"), EmitterData != nullptr);
            TSharedPtr<FJsonObject> Props;
            if(!W.Step(TEXT("Emitter_Properties"),[&]{Props=NiagaraEmitterPropertiesToJsonObject(EmitterData);}))return EmitterObject;
            EmitterObject->SetObjectField(TEXT("properties"),Props);

            TArray<TSharedPtr<FJsonValue>> RendererValues;
            TArray<TSharedPtr<FJsonValue>> ScriptValues;
            TSharedPtr<FJsonObject> GraphObject = BuildGraphAnalysisJson(nullptr, FString::Printf(TEXT("emitter:%s"), *Variant), bIncludePins, MaxFunctionCalls);
            int32 TotalFunctionCallCount = 0;
            int32 TotalOutputNodeCount = 0;
            if (EmitterData)
            {
                GraphObject = BuildGraphAnalysisJson(
                    EmitterData->GraphSource,
                    FString::Printf(TEXT("emitter:%s"), *Variant),
                    bIncludePins,
                    MaxFunctionCalls);
                if(!W.Admit(TEXT("after_emitter_graph_summary")))return EmitterObject;
                TotalFunctionCallCount = static_cast<int32>(GraphObject->GetNumberField(TEXT("function_call_count")));
                TotalOutputNodeCount = static_cast<int32>(GraphObject->GetNumberField(TEXT("output_node_count")));

                int32 RendererIndex = 0;
                const auto RendererSnapshot=EmitterData->GetRenderers();
                for (const UNiagaraRendererProperties* Renderer : RendererSnapshot)
                {
                    RendererValues.Add(MakeShared<FJsonValueObject>(BuildRendererJson(W,nullptr, Variant, INDEX_NONE, Renderer, RendererIndex)));
                    if(!W.Admit(TEXT("after_renderer_summary")))return EmitterObject;
                    ++RendererIndex;
                }

                TArray<UNiagaraScript*> EmitterScripts;
                EmitterData->GetScripts(EmitterScripts, false, false);
                if(!W.Admit(TEXT("emitter_script_inventory")))return EmitterObject;
                for (const UNiagaraScript* Script : EmitterScripts)
                {
                    if(!W.Admit(TEXT("emitter_script_summary")))return EmitterObject;
                    if (!Script)
                    {
                        continue;
                    }
                    ScriptValues.Add(MakeShared<FJsonValueObject>(
                        BuildScriptSummary(
                            Script,
                            FString::Printf(TEXT("%s:%s"), *Variant, *NiagaraScriptUsageName(Script->GetUsage())))));
                }
            }

            EmitterObject->SetNumberField(TEXT("renderer_count"), RendererValues.Num());
            EmitterObject->SetNumberField(TEXT("script_count"), ScriptValues.Num());
            EmitterObject->SetNumberField(TEXT("graph_node_count"), GraphObject->GetNumberField(TEXT("node_count")));
            EmitterObject->SetNumberField(TEXT("function_call_count"), TotalFunctionCallCount);
            EmitterObject->SetNumberField(TEXT("output_node_count"), TotalOutputNodeCount);
            EmitterObject->SetObjectField(TEXT("graph"), GraphObject);
            EmitterObject->SetArrayField(TEXT("renderers"), RendererValues);
            EmitterObject->SetArrayField(TEXT("scripts"), ScriptValues);
            return EmitterObject;
        };

    if(!W.Admit(TEXT("inspect_parameters")))return W.Error(W.R.Error);
    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    TSharedPtr<FJsonObject> SystemObject;
    if (bInspectSystem)
    {
        SystemObject = BuildSystemInspection();
        W.R.Data->SetObjectField(TEXT("partial_system_inspection"),SystemObject);
    }
    if (bInspectParamEmitter)
    {
        EmitterValues.Add(MakeShared<FJsonValueObject>(BuildEmitterInspection(TEXT("emitter"), bParamEmitterAddsDefaultModulesAndRenderers)));
    }
    if (bInspectEmptyEmitter)
    {
        EmitterValues.Add(MakeShared<FJsonValueObject>(BuildEmitterInspection(TEXT("empty_emitter"), false)));
    }
    if (bInspectBasicSpriteEmitter)
    {
        EmitterValues.Add(MakeShared<FJsonValueObject>(BuildEmitterInspection(TEXT("basic_sprite_emitter"), true)));
    }

    W.R.Data->SetArrayField(TEXT("partial_emitter_inspections"),EmitterValues);
    if(!W.Admit(TEXT("inspect_final_readback")))return W.Error(W.R.Error);
    W.TypedPassed=true;
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("kind"), KindLower);
    Result->SetBoolField(TEXT("read_only"), true);
    Result->SetBoolField(TEXT("transient_only"), true);
    Result->SetBoolField(TEXT("saved"), false);
    Result->SetBoolField(TEXT("compile_requested"), false);
    Result->SetBoolField(TEXT("asset_registry_registered"), false);
    Result->SetBoolField(TEXT("include_pins"), bIncludePins);
    Result->SetNumberField(TEXT("max_function_calls"), MaxFunctionCalls);
    Result->SetStringField(TEXT("source_policy"), TEXT("transient_factory_inspection_no_package_no_save_no_compile_request"));
    Result->SetStringField(TEXT("authoring_status"), TEXT("inspection_only; use this before enabling saved temp Niagara asset creation"));
    if (SystemObject.IsValid())
    {
        Result->SetObjectField(TEXT("system"), SystemObject);
    }
    Result->SetNumberField(TEXT("emitter_variant_count"), EmitterValues.Num());
    Result->SetArrayField(TEXT("emitters"), EmitterValues);
    return Result;

}


bool PackageOwnership(FWork& W,UObject* Root,const TCHAR* Stage)
{
 if(!W.Admit(Stage))return false;UPackage* P=Root->GetOutermost();TArray<UObject*> A;if(!W.Step(TEXT("GetObjectsWithPackage_OutputOwnership"),[&]{GetObjectsWithPackage(P,A,EGetObjectsFlags::IncludeNestedObjects);}))return false;if(A.Num()>65536)return W.Fail(TEXT("niagara_factory_package_export_bound"),TEXT("New output package export bound exceeded."));TArray<TSharedPtr<FJsonValue>> Exports;for(UObject* O:A){if(!Live(O))return W.Fail(TEXT("niagara_factory_output_export_unavailable"),TEXT("Output package contains an unavailable export."));auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),O->GetPathName());J->SetStringField(TEXT("class"),O->GetClass()->GetPathName());const bool Owned=(O==Root||O->IsIn(Root))&&O->GetPackage()==P&&O->GetOutermost()==P&&!Forbidden(O->GetClass()->GetPathName());J->SetBoolField(TEXT("owned"),Owned);Exports.Add(MakeShared<FJsonValueObject>(J));if(!Owned){W.R.Data->SetArrayField(TEXT("output_package_exports"),Exports);return W.Fail(TEXT("niagara_factory_foreign_package_export"),TEXT("Exact output package contains an export outside the created root subtree; no save/cleanup/retry."));}if(!W.Pin(O))return false;}W.R.Data->SetArrayField(TEXT("output_package_exports"),Exports);return W.Admit(TEXT("output_package_ownership_verified"));
}
bool ParseCreation(FWork& W,const TSharedPtr<FJsonObject>& Params,bool ForEmitter,FTarget& Target,bool& Allow,bool& Save,bool& Wait,double& Timeout,double& Poll)
{
 const TCHAR* Key=ForEmitter?TEXT("target_emitter_path"):TEXT("target_system_path");FString Path;if(!Params->TryGetStringField(Key,Path))return W.Fail(TEXT("niagara_factory_target_required"),FString(TEXT("Missing '"))+Key+TEXT("' parameter"));Allow=GetBoolParam(Params,TEXT("allow_non_temp_path"),false);if(GetBoolParam(Params,TEXT("overwrite_existing"),false))return W.Fail(TEXT("niagara_factory_overwrite_unsupported"),TEXT("overwrite_existing is not supported for first-pass Niagara asset creation."));Save=GetBoolParam(Params,TEXT("save"),true);Wait=!ForEmitter&&GetBoolParam(Params,TEXT("wait_for_completion"),true);Timeout=GetNumberParam(Params,TEXT("timeout_seconds"),10.0);Poll=GetNumberParam(Params,TEXT("poll_interval_seconds"),0.1);W.R.bSaveRequested=Save;if(!ResolveTarget(W,Path,Allow,Target))return false;if(Wait&&!W.DeferredMode){W.R.Data->SetBoolField(TEXT("required_wait_implemented"),false);W.R.Data->SetBoolField(TEXT("request_wait_for_completion"),true);return W.Fail(TEXT("niagara_factory_async_wait_unsupported"),TEXT("Required compile wait needs actual deferred final delivery. No persistent factory creation/compile/save attempted; wait was not forced False."));}return FreshTarget(W,Target);
}
TSharedPtr<FJsonObject> CreationBase(const FTarget& T,const TCHAR* Command,const TCHAR* PathKey,UObject* Asset,bool Allow,bool Save,const FWork& W,const FString& SavedFilename)
{
 auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),true);O->SetStringField(TEXT("command"),Command);O->SetStringField(TEXT("requested_target_path"),T.Requested);O->SetStringField(PathKey,Asset->GetPathName());O->SetStringField(TEXT("target_package_name"),T.Package);O->SetStringField(TEXT("target_object_path"),T.Object);O->SetStringField(TEXT("created_asset_class"),Asset->GetClass()->GetName());O->SetBoolField(TEXT("save_requested"),Save);O->SetBoolField(TEXT("saved"),W.R.bSaved);O->SetStringField(TEXT("saved_filename"),SavedFilename);O->SetBoolField(TEXT("asset_registry_registered"),true);O->SetBoolField(TEXT("allow_non_temp_path"),Allow);O->SetStringField(TEXT("write_scope"),TEXT("generated_temp_niagara_asset_creation"));return O;
}
bool VerifyRegistered(FWork& W,const FTarget& T,UObject* O)
{
 FAssetRegistryModule* M=nullptr;if(!W.Step(TEXT("Load_AssetRegistry_Final"),[&]{M=&FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));}))return false;FAssetData A;if(!W.Step(TEXT("Registry_CreatedAssetReadback"),[&]{A=M->Get().GetAssetByObjectPath(FSoftObjectPath(T.Object));}))return false;const bool Good=A.IsValid()&&A.PackageName.ToString().Equals(T.Package,ESearchCase::CaseSensitive)&&A.AssetName.ToString().Equals(T.Name,ESearchCase::CaseSensitive)&&A.AssetClassPath.ToString().Equals(O->GetClass()->GetPathName(),ESearchCase::CaseSensitive);W.R.Data->SetBoolField(TEXT("asset_registry_identity_verified"),Good);return Good||W.Fail(TEXT("niagara_factory_registry_readback_failed"),TEXT("Registry output class/package/name does not match exact created asset."));
}
TSharedPtr<FJsonObject> HandleCreateEmptySystem(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
 FTarget T;bool Allow=false,Save=true,Wait=true;double Timeout=10,Poll=.1;
 if(!ParseCreation(W,Params,false,T,Allow,Save,Wait,Timeout,Poll))return W.Error(W.R.Error);
 UFactory* F=MakeFactory(W,false,false);if(!F)return W.Error(W.R.Error);
 UNiagaraSystem* S=static_cast<UNiagaraSystem*>(CreateAsset(W,T,UNiagaraSystem::StaticClass(),F,true));if(!S)return W.Error(W.R.Error);
 if(!W.System(S,0)||!PackageOwnership(W,S,TEXT("after_create_package_ownership")))return W.Error(W.R.Error);
 TSharedPtr<FJsonObject> Compile;
 if(W.DeferredMode)
 {
  Compile=MakeShared<FJsonObject>();Compile->SetBoolField(TEXT("wait_for_completion"),Wait);Compile->SetNumberField(TEXT("timeout_seconds"),Timeout);Compile->SetNumberField(TEXT("poll_interval_seconds"),Poll);Compile->SetBoolField(TEXT("wait_timed_out"),false);Compile->SetNumberField(TEXT("wait_iterations"),0);Compile->SetNumberField(TEXT("wait_elapsed_seconds"),0);Compile->SetBoolField(TEXT("idle_observation_known"),false);
 }
 else Compile=CompileSnapshot(W,S,Timeout,Poll);
 if(!W.Admit(TEXT("after_compile_status")))return W.Error(W.R.Error);
 auto SaveStep=[&W,S,T,Save]() -> bool
 {return !Save||(PackageOwnership(W,S,TEXT("before_save_package_ownership"))&&SaveAsset(W,S,T,W.DeferredSavedFilename));};
 auto CompleteStep=[&W,S,T,Allow,Save,Compile]() -> TSharedPtr<FJsonObject>
 {
  if(!W.Admit(TEXT("empty_system_resume"))||!VerifyRegistered(W,T,S))return W.Error(W.R.Error);
  auto Result=CreationBase(T,TEXT("create_empty_niagara_system"),TEXT("system_path"),S,Allow,Save,W,W.DeferredSavedFilename);Result->SetNumberField(TEXT("emitter_count"),S->GetEmitterHandles().Num());Result->SetStringField(TEXT("effect_type"),NiagaraObjectPathOrEmpty(S->GetEffectType()));Result->SetBoolField(TEXT("compile_requested"),true);Result->SetObjectField(TEXT("compile_wait"),Compile);
  auto Audit=DependencyAudit(W,S,W.DeferredSavedFilename,!IsTempGeneratedNiagaraPath(T.Object));Result->SetObjectField(TEXT("dependency_audit"),Audit);
  if(W.R.Error.IsEmpty()&&W.Admit(TEXT("empty_system_final_typed_readback"))&&PackageOwnership(W,S,TEXT("final_package_ownership")))W.TypedPassed=S->GetEmitterHandles().IsEmpty();return Result;
 };
 if(W.DeferredMode)
 {
  W.DeferredSystem=S;W.DeferredWait=Wait;W.DeferredTimeout=Timeout;W.DeferredPoll=Poll;W.DeferredSaveRequested=Save;W.DeferredWaitResult=Compile;W.DeferredSaveStep=MoveTemp(SaveStep);W.DeferredCompleteStep=MoveTemp(CompleteStep);
  auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);Pending->SetStringField(TEXT("system_path"),T.Object);Pending->SetObjectField(TEXT("compile_wait"),Compile);return Pending;
 }
 if(!SaveStep())return W.Error(W.R.Error);return CompleteStep();
}
TSharedPtr<FJsonObject> HandleCreateEmptyEmitter(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
 FTarget T;bool Allow=false,Save=true,Wait=false;double Timeout=10,Poll=.1;if(!ParseCreation(W,Params,true,T,Allow,Save,Wait,Timeout,Poll))return W.Error(W.R.Error);const bool Defaults=GetBoolParam(Params,TEXT("add_default_modules_and_renderers"),false);auto* F=MakeFactory(W,true,Defaults);if(!F)return W.Error(W.R.Error);auto* E=static_cast<UNiagaraEmitter*>(CreateAsset(W,T,UNiagaraEmitter::StaticClass(),F,false));if(!E)return W.Error(W.R.Error);if(!W.Emitter(E,E->GetLatestEmitterData(),true)||!PackageOwnership(W,E,TEXT("after_create_package_ownership")))return W.Error(W.R.Error);FString File;if(Save&&(!PackageOwnership(W,E,TEXT("before_save_package_ownership"))||!SaveAsset(W,E,T,File)))return W.Error(W.R.Error);if(!VerifyRegistered(W,T,E))return W.Error(W.R.Error);auto Result=CreationBase(T,TEXT("create_empty_niagara_emitter"),TEXT("emitter_path"),E,Allow,Save,W,File);auto* D=E->GetLatestEmitterData();Result->SetStringField(TEXT("unique_emitter_name"),E->GetUniqueEmitterName());Result->SetBoolField(TEXT("add_default_modules_and_renderers"),Defaults);Result->SetNumberField(TEXT("renderer_count"),D->GetRenderers().Num());TSharedPtr<FJsonObject> Props;if(!W.Step(TEXT("Emitter_Properties"),[&]{Props=NiagaraEmitterPropertiesToJsonObject(D);}))return W.Error(W.R.Error);Result->SetObjectField(TEXT("properties"),Props);Result->SetBoolField(TEXT("compile_requested"),false);auto Audit=DependencyAudit(W,E,File,!IsTempGeneratedNiagaraPath(T.Object));Result->SetObjectField(TEXT("dependency_audit"),Audit);if(W.R.Error.IsEmpty()&&W.Admit(TEXT("empty_emitter_final_typed_readback"))&&PackageOwnership(W,E,TEXT("final_package_ownership"))){W.TypedPassed=E->GetLatestEmitterData()==D;}return Result;
}
TSharedPtr<FJsonObject> HandleCreateBasicSpriteSystem(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
 FTarget T;bool Allow=false,Save=true,Wait=true;double Timeout=10,Poll=.1;if(!ParseCreation(W,Params,false,T,Allow,Save,Wait,Timeout,Poll))return W.Error(W.R.Error);FString Name=GetStringParam(Params,TEXT("emitter_name"),TEXT("Emitter")).TrimStartAndEnd();if(Name.IsEmpty())Name=TEXT("Emitter");FText Why;if(Name.Len()>=NAME_SIZE)return W.Error(TEXT("emitter_name exceeds installed FName NAME_SIZE."));if(!FName(*Name).IsValidObjectName(Why))return W.Error(TEXT("Invalid emitter_name: ")+Why.ToString());FString MaterialPath=GetStringParam(Params,TEXT("material_path"),FString()).TrimStartAndEnd();if(Forbidden(MaterialPath))return W.Error(TEXT("MCP/Cubeless/Toolset material reference is forbidden."));UMaterialInterface* Material=nullptr;if(!MaterialPath.IsEmpty()){const FString Exact=NormalizeNiagaraObjectPathForLoad(MaterialPath);if(Exact.Len()>=NAME_SIZE)return W.Error(TEXT("Material object path exceeds installed FName NAME_SIZE."));if(!W.Step(TEXT("Load_Material"),[&]{Material=LoadObject<UMaterialInterface>(nullptr,*Exact);}))return W.Error(W.R.Error);if(!Material||!Material->GetPathName().Equals(Exact,ESearchCase::CaseSensitive)||Forbidden(Material->GetClass()->GetPathName())||!W.Pin(Material))return W.Error(TEXT("Failed to load exact permitted material."));}
 // The source lives in the transient package but deliberately lacks RF_Transient. Preserve its duplicate subtree for serialization.
 auto* Source=TransientEmitter(W,Name,true,false);if(!Source)return W.Error(W.R.Error);auto* F=MakeFactory(W,false,false,Source,Source->GetExposedVersion().VersionGuid);if(!F)return W.Error(W.R.Error);auto* S=static_cast<UNiagaraSystem*>(CreateAsset(W,T,UNiagaraSystem::StaticClass(),F,true));if(!S)return W.Error(W.R.Error);if(!W.System(S,1)||!PackageOwnership(W,S,TEXT("after_create_package_ownership")))return W.Error(W.R.Error);
 int32 Cleared=0,TransientChildren=0,SpriteCount=0;bool Applied=false;TArray<UNiagaraEmitter*> OwnedEmitters;for(const auto& H:S->GetEmitterHandles())OwnedEmitters.Add(H.GetInstance().Emitter);for(auto* E:OwnedEmitters){if(!W.Admit(TEXT("owned_emitter_flags")))return W.Error(W.R.Error);if(E->HasAnyFlags(RF_Transient)){if(!W.Step(TEXT("ClearOwnedEmitterTransientFlag"),[&]{E->ClearFlags(RF_Transient);++Cleared;},true,true))return W.Error(W.R.Error);}TArray<UObject*> Children;if(!W.Step(TEXT("GetObjectsWithOuter_Emitter"),[&]{GetObjectsWithOuter(E,Children,true);}))return W.Error(W.R.Error);if(Children.Num()>65536)return W.Error(TEXT("Owned emitter subobject bound exceeded."));for(auto* C:Children){if(!Live(C)||!C->IsIn(E)||C->GetPackage()!=S->GetPackage()||!W.Pin(C))return W.Error(TEXT("Owned emitter child identity invalid."));if(C->HasAnyFlags(RF_Transient))++TransientChildren;}}
 TArray<UNiagaraSpriteRendererProperties*> Sprites;for(const auto& H:S->GetEmitterHandles())for(UNiagaraRendererProperties* P:H.GetEmitterData()->GetRenderers())if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(P))Sprites.Add(Sprite);SpriteCount=Sprites.Num();if(SpriteCount==0)return W.Error(TEXT("Basic sprite factory output has no sprite renderer."));if(Material){if(!W.Step(TEXT("Modify_SystemForMaterial"),[&]{S->Modify();},true,true))return W.Error(W.R.Error);for(auto* Sprite:Sprites){if(!Sprite->IsIn(S)||Sprite->GetPackage()!=S->GetPackage())return W.Error(TEXT("Renderer is outside exact output ownership."));if(!W.Step(TEXT("Modify_SpriteRenderer"),[&]{Sprite->Modify();},true,true))return W.Error(W.R.Error);if(!W.Step(TEXT("SetSpriteMaterial"),[&]{Sprite->Material=Material;W.ExpectedMaterials.Add(Sprite,Material);Applied=true;},true,true))return W.Error(W.R.Error);}if(Applied){if(!W.Step(TEXT("MarkPackageDirty_System"),[&]{S->MarkPackageDirty();},true,true))return W.Error(W.R.Error);if(!W.SDK(TEXT("RequestCompile_Material"),true,true))return W.Error(W.R.Error);W.R.bCompileRequested=true;W.R.bCompileAttempted=true;W.R.Compiled.Reset();const bool Accepted=S->RequestCompile(false);W.R.Data->SetBoolField(TEXT("material_compile_request_accepted"),Accepted);if(!W.After(TEXT("RequestCompile_Material")))return W.Error(W.R.Error);}}
 TSharedPtr<FJsonObject> Compile;
 if(W.DeferredMode)
 {
  Compile=MakeShared<FJsonObject>();Compile->SetBoolField(TEXT("wait_for_completion"),Wait);Compile->SetNumberField(TEXT("timeout_seconds"),Timeout);Compile->SetNumberField(TEXT("poll_interval_seconds"),Poll);Compile->SetBoolField(TEXT("wait_timed_out"),false);Compile->SetNumberField(TEXT("wait_iterations"),0);Compile->SetNumberField(TEXT("wait_elapsed_seconds"),0);Compile->SetBoolField(TEXT("idle_observation_known"),false);
 }
 else Compile=CompileSnapshot(W,S,Timeout,Poll);
 if(!W.Admit(TEXT("after_compile_status")))return W.Error(W.R.Error);
 auto SaveStep=[&W,S,T,Save]() -> bool
 {return !Save||(PackageOwnership(W,S,TEXT("before_save_package_ownership"))&&SaveAsset(W,S,T,W.DeferredSavedFilename));};
 auto CompleteStep=[&W,S,T,Allow,Save,Compile,Name,Material,Source,OwnedEmitters,Sprites,SpriteCount,Applied,Cleared,TransientChildren]() -> TSharedPtr<FJsonObject>
 {
  if(!W.Admit(TEXT("basic_sprite_resume"))||!VerifyRegistered(W,T,S))return W.Error(W.R.Error);
  auto Result=CreationBase(T,TEXT("create_basic_sprite_niagara_system"),TEXT("system_path"),S,Allow,Save,W,W.DeferredSavedFilename);Result->SetStringField(TEXT("requested_emitter_name"),Name);Result->SetNumberField(TEXT("emitter_count"),S->GetEmitterHandles().Num());Result->SetNumberField(TEXT("sprite_renderer_count"),SpriteCount);Result->SetStringField(TEXT("material_path"),Material?Material->GetPathName():FString());Result->SetBoolField(TEXT("material_applied"),Applied);
  const int32 ParentCount=CountEmitterParentReferences(S);Result->SetNumberField(TEXT("emitter_parent_reference_count"),ParentCount);Result->SetBoolField(TEXT("has_emitter_parent_references"),ParentCount>0);Result->SetNumberField(TEXT("cleared_transient_emitter_flag_count"),Cleared);Result->SetNumberField(TEXT("transient_owned_subobject_count"),TransientChildren);Result->SetStringField(TEXT("effect_type"),NiagaraObjectPathOrEmpty(S->GetEffectType()));Result->SetBoolField(TEXT("compile_requested"),true);Result->SetObjectField(TEXT("compile_wait"),Compile);
  auto Audit=DependencyAudit(W,S,W.DeferredSavedFilename,!IsTempGeneratedNiagaraPath(T.Object));Result->SetObjectField(TEXT("dependency_audit"),Audit);
  if(W.R.Error.IsEmpty()&&W.Admit(TEXT("basic_sprite_final_typed_readback"))&&PackageOwnership(W,S,TEXT("final_package_ownership")))
  {
   bool Good=ParentCount==0&&S->GetEmitterHandles().Num()==1&&Source->GetOutermost()==GetTransientPackage()&&!Source->HasAnyFlags(RF_Transient)&&!Source->bIsInheritable;
   for(UNiagaraEmitter* E:OwnedEmitters)Good&=!E->HasAnyFlags(RF_Transient);for(UNiagaraSpriteRendererProperties* P:Sprites)Good&=P->IsIn(S)&&P->GetPackage()==S->GetPackage()&&(!Material||P->Material==Material);
   W.TypedPassed=Good;if(!Good)W.Fail(TEXT("niagara_factory_basic_sprite_postcondition_failed"),TEXT("Owned emitter persistence/parent/source/material postcondition failed; actual output/save history retained."));
  }
  return Result;
 };
 if(W.DeferredMode)
 {
  W.DeferredSystem=S;W.DeferredWait=Wait;W.DeferredTimeout=Timeout;W.DeferredPoll=Poll;W.DeferredSaveRequested=Save;W.DeferredWaitResult=Compile;W.DeferredSaveStep=MoveTemp(SaveStep);W.DeferredCompleteStep=MoveTemp(CompleteStep);
  auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);Pending->SetStringField(TEXT("system_path"),T.Object);Pending->SetObjectField(TEXT("compile_wait"),Compile);return Pending;
 }
 if(!SaveStep())return W.Error(W.R.Error);return CompleteStep();
}


bool OutputFiniteBudget(const TSharedPtr<FJsonValue>& V,int32 Depth,int64& Values,int64& Text)
{
 if(!V||Depth>64||++Values>262144)return false;if(V->Type==EJson::Number){double N=0;return V->TryGetNumber(N)&&FMath::IsFinite(N);}if(V->Type==EJson::String){const FString S=V->AsString();Text+=S.Len();return Text<=32ll*1024*1024&&NoEmbeddedNul(S);}if(V->Type==EJson::Array){for(const auto& X:V->AsArray())if(!OutputFiniteBudget(X,Depth+1,Values,Text))return false;}else if(V->Type==EJson::Object){for(const auto& X:V->AsObject()->Values){Text+=FString(X.Key).Len();if(Text>32ll*1024*1024||!OutputFiniteBudget(X.Value,Depth+1,Values,Text))return false;}}return V->Type!=EJson::None;
}
FString Finish(FWork& W,TSharedPtr<FJsonObject> Legacy,const TCHAR* Operation)
{
 bool LeafSuccess=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),LeafSuccess);if(!LeafSuccess&&W.R.Error.IsEmpty()){FString M;if(Legacy)Legacy->TryGetStringField(TEXT("error"),M);W.Fail(TEXT("niagara_factory_bootstrap_failed"),M.IsEmpty()?TEXT("Dedicated operation failed; actual prefix preserved."):M);}int64 Values=0,Text=0;if(Legacy&&!OutputFiniteBudget(MakeShared<FJsonValueObject>(Legacy),0,Values,Text)){W.Fail(TEXT("niagara_factory_output_invalid_or_too_large"),TEXT("Legacy diagnostic output exceeds finite bounded JSON scope; creation/save prefix remains."));Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);Legacy->SetBoolField(TEXT("diagnostic_output_omitted_due_to_bound"),true);}
 const bool Complete=LeafSuccess&&W.TypedPassed&&W.R.Error.IsEmpty()&&W.Admit(TEXT("final_exact_factory_admission"));W.R.bSuccess=Complete;W.R.bPostconditionPassed=Complete;if(LeafSuccess&&!Complete&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_factory_postcondition_incomplete"),TEXT("Supported typed construction/inspection postcondition did not complete."));if(Complete||W.KnownExistingTargetRefusal)W.R.bChangeStateKnown=true;
 if(!W.RequestedPackage.IsEmpty()){auto Presence=MakeShared<FJsonObject>();const bool CanObserve=(!W.DeferredMode||(W.R.Error.IsEmpty()&&W.CapturedEditor.Get()==GEditor))&&Safe()&&W.WorldCaptured&&Live(W.World.Get())&&!W.World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==W.World.Get();Presence->SetBoolField(TEXT("observation_available"),CanObserve);Presence->SetStringField(TEXT("requested_package"),W.RequestedPackage);Presence->SetStringField(TEXT("requested_object"),W.RequestedObject);if(CanObserve){Presence->SetBoolField(TEXT("memory_package_exists"),FindPackage(nullptr,*W.RequestedPackage)!=nullptr);Presence->SetBoolField(TEXT("memory_object_exists"),StaticFindObject(UObject::StaticClass(),nullptr,*W.RequestedObject)!=nullptr);Presence->SetBoolField(TEXT("disk_file_exists"),IFileManager::Get().FileExists(*W.RequestedFilename));}W.R.Data->SetObjectField(TEXT("requested_target_presence"),Presence);}
 W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),Complete);W.R.Data->SetBoolField(TEXT("scoped_explicit_outcome_known"),W.R.bChangeStateKnown);W.R.Data->SetStringField(TEXT("change_state_known_scope"),W.KnownExistingTargetRefusal?TEXT("Known unchanged persistent target at exact existing-target preflight rejection. Actual transient Job allocation and SDK-global effects remain separately recorded/unverified; not a whole-editor purity proof."):TEXT("Successful exact created/transient targets and explicit requested modifications only. SDK global effects, pending compile, GPU/render and persistence cold reload remain unverified."));W.R.Data->SetObjectField(TEXT("native_call_counts"),W.Counts);W.R.Data->SetArrayField(TEXT("actual_SDK_prefix"),W.Calls);W.R.Data->SetArrayField(TEXT("actual_creation_prefix"),W.CreationRows);W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.SaveRows);W.R.Data->SetObjectField(TEXT("target_dirty_projection"),W.DirtyProjection());W.R.Data->SetBoolField(TEXT("source_policy_runtime_Cubeless_dependency_added"),false);W.R.Data->SetBoolField(TEXT("compile_completion_verified"),false);W.R.Data->SetBoolField(TEXT("GPU_render_completion_verified"),false);W.R.Data->SetBoolField(TEXT("cold_persistence_verified"),false);W.R.Data->SetNumberField(TEXT("extra_wrapper_wait_poll_sleep_flush_calls"),0);W.R.Data->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);W.R.Data->SetBoolField(TEXT("cleanup_attempted"),false);W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);W.R.Data->SetBoolField(TEXT("do_not_retry"),!Complete&&W.R.bMutationAttempted);W.R.Data->SetBoolField(TEXT("residual_output_possible"),W.Persistent&&W.R.bMutationAttempted&&!Complete&&!W.KnownExistingTargetRefusal);W.R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("saved is OR of actual singleton SavePackage SDK true returns, including later partial failure; row save_completed additionally requires captured package identity and actual clean readback. It is not cold reload/cook/whole persistence proof."));W.R.Data->SetStringField(TEXT("save_attempted_scope"),TEXT("Explicit wrapper singleton SavePackage calls only; factory/initializers and other SDK internals may have unmeasured effects."));W.R.Data->SetStringField(TEXT("failure_cleanup_scope"),TEXT("No automatic delete/overwrite/rename/Undo reset/rollback; exact output and possible empty package residue are retained and reported."));W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString Run(const FString& Text,const TCHAR* Operation,bool Persistent,FHandler Handler)
{
 FWork W(Persistent);TSharedPtr<FJsonObject> Params;if(!Parse(Text,Params)){W.Fail(TEXT("niagara_factory_invalid_params_json"),TEXT("Finite JSON object within1Mi UTF16/depth64/262144 slots with case-sensitive unique keys and no embedded NUL required."));return Finish(W,W.Error(W.R.Error),Operation);}if(!IsInGameThread()){W.Fail(TEXT("niagara_factory_game_thread_required"),TEXT("Editor game thread required; no shared Native context accessed."));return Finish(W,W.Error(W.R.Error),Operation);}if(Running||DeferredLeaseActive){W.Fail(TEXT("niagara_factory_reentrant_request"),TEXT("Dedicated factory operation is already active; nested Native direct entry rejected without replay."));return Finish(W,W.Error(W.R.Error),Operation);}TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);if(!W.Admit(TEXT("preflight")))return Finish(W,W.Error(W.R.Error),Operation);return Finish(W,Handler(W,Params),Operation);
}
}
FString InspectDefaults(const FString& ParamsJson){return Run(ParamsJson,TEXT("inspect_niagara_factory_bootstrap_defaults"),false,&HandleInspectDefaults);}
FString CreateEmptySystem(const FString& ParamsJson){return Run(ParamsJson,TEXT("create_empty_niagara_system"),true,&HandleCreateEmptySystem);}
FString CreateEmptyEmitter(const FString& ParamsJson){return Run(ParamsJson,TEXT("create_empty_niagara_emitter"),true,&HandleCreateEmptyEmitter);}
FString CreateBasicSpriteSystem(const FString& ParamsJson){return Run(ParamsJson,TEXT("create_basic_sprite_niagara_system"),true,&HandleCreateBasicSpriteSystem);}
}

namespace Cubeless::UE58::NiagaraFactoryBootstrapAsync
{
using namespace Cubeless::UE58::NiagaraFactoryBootstrap;
namespace
{
std::atomic<bool>& Stopping=DeferredShutdownRequested;
TSharedPtr<FJobState> ActiveJob;
TMap<const UCubelessNiagaraFactoryBootstrapJob*,FString> SnapshotCache;
TMap<const UCubelessNiagaraFactoryBootstrapJob*,TWeakObjectPtr<UCubelessNiagaraFactoryBootstrapJob>> CacheOwners;
// Unsafe records retain exact target/Job holds; Shutdown never drops them inside SDK calls or GC.
TArray<TSharedPtr<FJobState>>& UnsafeRecords(){static auto* Records=new TArray<TSharedPtr<FJobState>>();return *Records;}
enum class EPhase:uint8 { Prepare, Wait, Save, Audit, Final, Terminal, Quarantined };
}
struct FJobState:public TSharedFromThis<FJobState>
{
 FWork Work{true};
 TStrongObjectPtr<UCubelessNiagaraFactoryBootstrapJob> Job{nullptr};
 UCubelessNiagaraFactoryBootstrapJob* RawJob=nullptr;
 TSharedPtr<FJsonObject> Params,Legacy;
 FString Operation,Id,LastSnapshot,AllocatedJobPath;
 EPhase Phase=EPhase::Prepare;
 FTSTicker::FDelegateHandle Ticker;
 FDelegateHandle PreExit;
 uint64 LastFrame=MAX_uint64;
 int64 Revision=0;
 bool Executing=false,Dispatched=false,SaveDispatched=false,AuditDispatched=false,FinalEncoded=false,InitialStatus=false;
 bool LastOutstanding=false,LastActive=false,ObservationKnown=false,ObservationComplete=false,Idle=false,TimedOut=false;
 int32 WaitIterations=0;
 double WaitStart=0,NextPoll=0;
 explicit FJobState(const TCHAR* Op):Operation(Op),Id(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens))
 {Work.DeferredMode=true;Work.R.Data->SetBoolField(TEXT("async_factory_continuation"),true);}
 bool PublicationSafe()const
 {
  if(!Safe()||Work.CapturedEditor.Get()!=GEditor)return false;
  if(GEditor->GetPIEWorldContext()||GEditor->PlayWorld)return false;
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
  auto& D=Work.R.Data;
  D->SetStringField(TEXT("native_job_id"),Id);
  D->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
  D->SetBoolField(TEXT("native_job_terminal"),Terminal);
  D->SetBoolField(TEXT("operation_complete"),Terminal);
  D->SetStringField(TEXT("operation"),Operation);
  D->SetBoolField(TEXT("automatic_retry_permitted"),false);
  D->SetBoolField(TEXT("async_core_dispatched"),Dispatched);
  D->SetBoolField(TEXT("async_save_phase_dispatched"),SaveDispatched);
  D->SetBoolField(TEXT("async_audit_phase_dispatched"),AuditDispatched);
  D->SetBoolField(TEXT("bounded_wait_observation_complete"),ObservationComplete);
  D->SetBoolField(TEXT("compile_idle_observation_known"),ObservationKnown);
  if(ObservationKnown)D->SetBoolField(TEXT("compile_idle_observed"),Idle);
  D->SetBoolField(TEXT("timeout_does_not_imply_compile_success"),true);
  D->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);
  D->SetStringField(TEXT("continuation_scope"),TEXT("Same owned factory System, finite GT ticker; no global task pump/Sleep/wait/flush, no replay/rollback. Public Poll may request compilation and SDK calls may block internally."));
  D->SetObjectField(TEXT("native_call_counts"),Work.Counts);
  D->SetArrayField(TEXT("actual_SDK_prefix"),Work.Calls);
  D->SetArrayField(TEXT("actual_creation_prefix"),Work.CreationRows);
  D->SetArrayField(TEXT("actual_save_prefix"),Work.SaveRows);
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
  Work.CancelRequested=true;Work.CancelReason=Reason;
  if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_factory_async_context_quarantined"),Reason);
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
  if(!PublicationSafe()){Quarantine(TEXT("Editor/world/GC/exit unavailable while publishing pending factory snapshot."));return;}
  Cache(Operations::EncodeVariableResult(Work.R,*Operation),false,true);
 }
 void SetFailureLegacy()
 {
  if(!Legacy)Legacy=MakeShared<FJsonObject>();
  Legacy->SetBoolField(TEXT("success"),false);
  Legacy->SetStringField(TEXT("error"),Work.R.Error.IsEmpty()?TEXT("Factory job stopped; actual prefix retained."):Work.R.Error);
 }
 bool ExistingTargetRejectedBeforeTargetSDK()const
 {
  // Exact observed failure only: NewObject of this controller is not a target write.
  // Keep mutation_attempted and SDK_global_purity_unverified from that real SDK call.
  if(!Work.DeferredMode||!PublicationSafe()||Stopping.load()||Work.CancelRequested||!Dispatched||SaveDispatched||AuditDispatched)return false;
  if(!Work.R.ErrorCode.Equals(TEXT("niagara_factory_target_exists"),ESearchCase::CaseSensitive)||!Work.FailedStage.Equals(TEXT("fresh_target"),ESearchCase::CaseSensitive))return false;
  if(Work.RequestedObject.IsEmpty()||Work.RequestedPackage.IsEmpty()||Work.RequestedFilename.IsEmpty()||Work.TypedPassed)return false;
  if(!Work.CreationRows.IsEmpty()||!Work.SaveRows.IsEmpty()||!Work.Objects.IsEmpty()||!Work.Graphs.IsEmpty()||!Work.Scripts.IsEmpty()||!Work.Emitters.IsEmpty()||!Work.Systems.IsEmpty()||Work.Factory.IsValid()||Work.FactoryClass.IsValid()||!Work.ExpectedFactoryEmitters.IsEmpty()||!Work.ExpectedMaterials.IsEmpty())return false;
  if(Work.DeferredSystem.IsValid()||Work.DeferredWaitResult.IsValid()||Work.DeferredSaveStep||Work.DeferredCompleteStep||InitialStatus||ObservationKnown||ObservationComplete||WaitIterations!=0)return false;
  if(!Work.R.bMutationAttempted||Work.R.bChanged||Work.R.bCompileRequested||Work.R.bCompileAttempted||!Work.R.Compiled.IsSet()||Work.R.Compiled.GetValue()||Work.R.bSaveAttempted||Work.R.bSaved)return false;
  bool PersistentAttempt=false;
  if(!Work.R.Data->TryGetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),PersistentAttempt)||PersistentAttempt)return false;
  double Count=0;
  if(Work.Counts->Values.Num()!=1||!Work.Counts->TryGetNumberField(TEXT("NewObject_FactoryJob"),Count)||Count!=1.0||Work.Calls.Num()!=1||!Work.Calls[0].IsValid()||Work.Calls[0]->Type!=EJson::Object)return false;
  const TSharedPtr<FJsonObject> Row=Work.Calls[0]->AsObject();FString Stage;bool Entered=false,Returned=false,Mutation=false;
  if(!Row.IsValid()||!Row->TryGetStringField(TEXT("stage"),Stage)||!Stage.Equals(TEXT("NewObject_FactoryJob"),ESearchCase::CaseSensitive)||!Row->TryGetBoolField(TEXT("entered"),Entered)||!Entered||!Row->TryGetBoolField(TEXT("returned"),Returned)||!Returned||!Row->TryGetBoolField(TEXT("mutation_attempted"),Mutation)||!Mutation)return false;
  UCubelessNiagaraFactoryBootstrapJob* Owned=Job.Get();UPackage* Transient=GetTransientPackage();
  return Live(Owned)&&Owned==RawJob&&Owned->GetClass()==UCubelessNiagaraFactoryBootstrapJob::StaticClass()&&!AllocatedJobPath.IsEmpty()&&Owned->GetPathName().Equals(AllocatedJobPath,ESearchCase::CaseSensitive)&&Owned->GetOuter()==Transient&&Owned->GetPackage()==Transient&&Owned->GetOutermost()==Transient&&!Owned->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject);
 }
 void Finalize()
 {
  if(FinalEncoded||Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("Factory terminal publication lost its exact Editor world; no recovery or replay."));return;}
  if(!Work.R.Error.IsEmpty())SetFailureLegacy();
  Work.KnownExistingTargetRefusal=ExistingTargetRejectedBeforeTargetSDK();
  if(Work.KnownExistingTargetRefusal)
  {
   Work.R.Data->SetBoolField(TEXT("known_existing_target_preflight_refusal"),true);
   Work.R.Data->SetBoolField(TEXT("persistent_target_change_state_known"),true);
   Work.R.Data->SetBoolField(TEXT("persistent_target_mutation_attempted"),false);
   Work.R.Data->SetBoolField(TEXT("persistent_target_changed"),false);
   Work.R.Data->SetBoolField(TEXT("transient_job_allocation_observed"),true);
   Work.R.Data->SetStringField(TEXT("known_refusal_scope"),TEXT("Only exact fresh_target/target_exists with one completed owned transient Job allocation and no target/factory/capture/compile/save/continuation SDK. Global SDK purity is still unverified; no retry is performed."));
  }
  Metadata(true);
  LastSnapshot=Finish(Work,Legacy,*Operation);
  FinalEncoded=true;
  if(!PublicationSafe()){FinalEncoded=false;Quarantine(TEXT("Factory final guard lost publication context."));return;}
  Cache(LastSnapshot,true,true);
  Phase=EPhase::Terminal;Unhook();
 }
 bool Status(UNiagaraSystem* System,const TCHAR* Stage)
 {
  if(!Work.SDK(Stage))return false;
  const bool Outstanding=System->HasOutstandingCompilationRequests(true);
  Work.DeferredWaitResult->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);
  if(!Work.After(Stage))return false;
  if(!Work.SDK(TEXT("System_HasActiveCompilations")))return false;
  const bool Active=System->HasActiveCompilations();
  Work.DeferredWaitResult->SetBoolField(TEXT("active_compilations_after"),Active);
  if(!Work.After(TEXT("System_HasActiveCompilations")))return false;
  LastOutstanding=Outstanding;LastActive=Active;ObservationKnown=true;Idle=!Outstanding&&!Active;
  Work.DeferredWaitResult->SetBoolField(TEXT("idle_observation_known"),true);
  Work.DeferredWaitResult->SetBoolField(TEXT("idle_observed"),Idle);
  return true;
 }
 void WaitPhase()
 {
  if(!Work.Admit(TEXT("async_compile_wait_admission")))return;
  UNiagaraSystem* System=Work.DeferredSystem.Get();
  if(!System||!Work.DeferredWaitResult){Work.Fail(TEXT("niagara_factory_async_state_missing"),TEXT("Same captured output System and wait result required."));return;}
  const double Now=FPlatformTime::Seconds();
  if(InitialStatus&&Work.DeferredWait&&Now<NextPoll)return;
  if(!Status(System,TEXT("System_HasOutstandingCompilationRequests")))return;
  InitialStatus=true;
  if(!Work.DeferredWait)
  {
   ObservationComplete=true;Phase=EPhase::Save;
   Work.DeferredWaitResult->SetBoolField(TEXT("completion_verified"),false);
   Work.DeferredWaitResult->SetStringField(TEXT("scope"),TEXT("Single public status snapshot; wait_for_completion=False. No compile success/GPU readiness claim."));
   return;
  }
  if(!Idle)
  {
   if(!Work.SDK(TEXT("System_PollForCompilationComplete")))return;
   System->PollForCompilationComplete();
   if(!Work.After(TEXT("System_PollForCompilationComplete")))return;
   if(!Status(System,TEXT("System_HasOutstandingCompilationRequests_AfterPoll")))return;
   if(!Idle&&FPlatformTime::Seconds()>=WaitStart+FMath::Clamp(Work.DeferredTimeout,0.0,60.0))TimedOut=true;
   if(!Idle&&!TimedOut)++WaitIterations;
  }
  const double Elapsed=FMath::Max(0.0,FPlatformTime::Seconds()-WaitStart);
  Work.DeferredWaitResult->SetBoolField(TEXT("wait_timed_out"),TimedOut);
  Work.DeferredWaitResult->SetNumberField(TEXT("wait_iterations"),WaitIterations);
  Work.DeferredWaitResult->SetNumberField(TEXT("wait_elapsed_seconds"),Elapsed);
  Work.DeferredWaitResult->SetBoolField(TEXT("completion_verified"),Idle);
  Work.DeferredWaitResult->SetStringField(TEXT("scope"),TEXT("Bounded status/Poll observation on finite GT ticks. Timeout is original diagnostic; optional save/audit still follow. No compile-error/GPU/cold-load proof."));
  if(Idle||TimedOut){ObservationComplete=true;Phase=EPhase::Save;}
  else NextPoll=FPlatformTime::Seconds()+FMath::Clamp(Work.DeferredPoll,0.02,1.0);
 }
 void ExecutePhase()
 {
  if(Work.CancelRequested||Stopping.load())
  {
   if(!Work.CancelRequested){Work.CancelRequested=true;Work.CancelReason=TEXT("Module shutdown stopped the owned factory job.");}
   Work.Fail(TEXT("niagara_factory_async_cancelled"),Work.CancelReason);
  }
  if(!Work.R.Error.IsEmpty()){Phase=EPhase::Final;Finalize();return;}
  if(!Work.Admit(TEXT("async_factory_phase_admission"))){Phase=EPhase::Final;Finalize();return;}
  switch(Phase)
  {
   case EPhase::Prepare:
   {
    if(Dispatched){Work.Fail(TEXT("niagara_factory_async_replay_forbidden"),TEXT("Factory core dispatch already entered."));break;}
    Dispatched=true;
    Legacy=Operation==TEXT("create_empty_niagara_system")?HandleCreateEmptySystem(Work,Params):HandleCreateBasicSpriteSystem(Work,Params);
    if(Work.R.Error.IsEmpty()&&(!Work.DeferredSaveStep||!Work.DeferredCompleteStep||!Work.DeferredSystem.IsValid()))Work.Fail(TEXT("niagara_factory_async_state_missing"),TEXT("Factory core did not provide exact owned continuation."));
    if(Work.R.Error.IsEmpty()){WaitStart=FPlatformTime::Seconds();Phase=EPhase::Wait;}
    break;
   }
   case EPhase::Wait:WaitPhase();break;
   case EPhase::Save:
    if(SaveDispatched){Work.Fail(TEXT("niagara_factory_async_save_replay_forbidden"),TEXT("Save continuation already entered."));break;}
    SaveDispatched=true;
    if(!Work.DeferredSaveStep()){if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_factory_async_save_failed"),TEXT("Save continuation did not complete."));}
    else Phase=EPhase::Audit;
    break;
   case EPhase::Audit:
    if(AuditDispatched){Work.Fail(TEXT("niagara_factory_async_audit_replay_forbidden"),TEXT("Audit continuation already entered."));break;}
    AuditDispatched=true;Legacy=Work.DeferredCompleteStep();Phase=EPhase::Final;break;
   case EPhase::Final:Finalize();return;
   default:return;
  }
  if(!Work.R.Error.IsEmpty()){Phase=EPhase::Final;Finalize();return;}
  PublishPending();
 }
 void ReleaseSafe()
 {
  if(Executing||Phase!=EPhase::Terminal||!PublicationSafe())return;
  Work.DeferredSaveStep=nullptr;Work.DeferredCompleteStep=nullptr;
  Work.Objects.Reset();Job.Reset();
  if(ActiveJob.Get()==this){ActiveJob.Reset();DeferredLeaseActive=false;}
 }
 bool Tick(float)
 {
  TSharedRef<FJobState> Hold=AsShared();
  if(Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return false;
  if(Executing||Running||LastFrame==GFrameCounter)return true;
  LastFrame=GFrameCounter;
  if(!PublicationSafe()){Quarantine(TEXT("Factory ticker lost live Editor/game-thread/world context."));return false;}
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
  PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([Weak](){Stopping.store(true);if(!IsInGameThread())return;auto State=Weak.Pin();if(State.IsValid()){State->Work.CancelRequested=true;State->Work.CancelReason=TEXT("Engine pre-exit stopped factory job.");State->Unhook();if(!State->Executing)State->Quarantine(State->Work.CancelReason);}});
 }
};
namespace
{
void PruneSnapshots()
{
 for(auto It=CacheOwners.CreateIterator();It;++It)if(!It.Value().IsValid()){SnapshotCache.Remove(It.Key());It.RemoveCurrent();}
}
UCubelessNiagaraFactoryBootstrapJob* Start(const FString& Text,const TCHAR* Operation)
{
 if(!IsInGameThread()||Stopping.load()||Running||DeferredLeaseActive||!Safe())return nullptr;
 auto State=MakeShared<FJobState>(Operation);
 State->Work.R.bSaveRequested=true;
 if(!State->Work.Admit(TEXT("async_factory_start_admission")))return nullptr;
 PruneSnapshots();
 if(SnapshotCache.Num()>=256)return nullptr;
 ActiveJob=State;DeferredLeaseActive=true;
 UCubelessNiagaraFactoryBootstrapJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(State->Executing,true);
  TGuardValue<bool> CoreLease(Running,true);
  TGuardValue<FWork*> Context(Current,&State->Work);
  if(State->Work.SDK(TEXT("NewObject_FactoryJob"),true,false))
  {
   Created=NewObject<UCubelessNiagaraFactoryBootstrapJob>(GetTransientPackage());
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_returned"),true);
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
   State->RawJob=Created;
   if(Stopping.load()||State->Work.CancelRequested||!State->PublicationSafe())
   {
    State->Quarantine(TEXT("Factory Job allocation interrupted; persistent factory dispatch not entered."));
    return nullptr;
   }
   if(!State->Work.After(TEXT("NewObject_FactoryJob"))||!Created)
   {
    State->Quarantine(TEXT("Factory Job allocation could not be safely adopted."));
    return nullptr;
   }
   State->Job.Reset(Created);State->AllocatedJobPath=Created->GetPathName();CacheOwners.Add(Created,Created);
   if(!Parse(Text,State->Params))
   {
    State->Work.Fail(TEXT("niagara_factory_invalid_params_json"),TEXT("Finite unique-key JSON object within1Mi/depth64/262144 slots required."));
    State->Legacy=State->Work.Error(State->Work.R.Error);State->Phase=EPhase::Final;State->Finalize();
   }
   else
   {
    State->Work.R.bSaveRequested=GetBoolParam(State->Params,TEXT("save"),true);
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
UCubelessNiagaraFactoryBootstrapJob* StartEmptySystem(const FString& ParamsJson){return Start(ParamsJson,TEXT("create_empty_niagara_system"));}
UCubelessNiagaraFactoryBootstrapJob* StartBasicSpriteSystem(const FString& ParamsJson){return Start(ParamsJson,TEXT("create_basic_sprite_niagara_system"));}
FString SnapshotJson(const UCubelessNiagaraFactoryBootstrapJob* Job)
{
 if(!IsInGameThread()||!Job)return FString();
 if(const FString* Snapshot=SnapshotCache.Find(Job))return *Snapshot;
 return FString();
}
bool Cancel(UCubelessNiagaraFactoryBootstrapJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!Job||!ActiveJob.IsValid()||ActiveJob->RawJob!=Job)return false;
 auto State=ActiveJob;
 if(State->Phase==EPhase::Terminal||State->Phase==EPhase::Quarantined)return false;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return true;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Cancellation lost factory publication context; actual prefix retained."));return true;}
 {
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> CoreLease(Running,true);TGuardValue<FWork*> Context(Current,&State->Work);
  State->Work.Fail(TEXT("niagara_factory_async_cancelled"),State->Work.CancelReason);
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
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe factory state without touching UObject publication."));return;}
 Cancel(State->RawJob,State->Work.CancelReason);
}
}
