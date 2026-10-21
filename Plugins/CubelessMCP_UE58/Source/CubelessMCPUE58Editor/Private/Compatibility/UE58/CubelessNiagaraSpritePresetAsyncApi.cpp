// GU dedicated Editor-only sprite preset composite. Source candidate, not build/runtime proof.
#include "Compatibility/UE58/CubelessNiagaraSpritePresetAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraSpritePresetJob.h"
#include "Containers/Ticker.h"
#include <atomic>
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterBase.h"
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
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/Archive.h"
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
#include "Compatibility/UE58/CubelessNiagaraFactoryBootstrapApi.h"
#include "Compatibility/UE58/CubelessNiagaraRendererApi.h"
#include "Compatibility/UE58/CubelessNiagaraEmitterConfigurationApi.h"
#include "Compatibility/UE58/CubelessNiagaraModuleApi.h"
namespace Cubeless::UE58::NiagaraSpritePresetAsync
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false;
std::atomic<bool> ShuttingDown{false};
constexpr const TCHAR* ProjectWeatherRainParticleMaterialPath = TEXT("/Game/MCPWeather/FX/Rain/Materials/M_MCPWeather_Rain_Particle.M_MCPWeather_Rain_Particle");
constexpr const TCHAR* ProjectWeatherSnowParticleMaterialPath = TEXT("/Game/MCPWeather/FX/Snow/Materials/M_MCPWeather_Snow_Particle.M_MCPWeather_Snow_Particle");
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage;}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
bool Forbidden(const FString& S){const FString L=S.ToLower();return L.Contains(TEXT("unrealmcp"))||L.Contains(TEXT("cubelessmcp"))||L.Contains(TEXT("modelcontextprotocol"))||L.Contains(TEXT("toolsetregistry"));}
TArray<TStrongObjectPtr<UObject>>* ExecutionPins=nullptr;
struct FIdentity{TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path,PackageName;bool DirtyBefore=false;explicit FIdentity(UObject* O):Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()),PackageName(O->GetOutermost()->GetName()),DirtyBefore(O->GetOutermost()->IsDirty()){}bool Exact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&Live(Package.Get())&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}};
struct FPin{UEdGraphPin* P=nullptr;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;bool NotConnectable=false,DefaultReadOnly=false;TWeakObjectPtr<UObject> DefaultObject;TArray<UEdGraphPin*> Links;FString Default,AutoDefault,DefaultText;};
struct FNode{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;TArray<FPin> Pins;};
struct FGraph{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNode> Nodes;};
struct FScript{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Exposed;};
FString EmitterPropertyState(const FVersionedNiagaraEmitterData* D){auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("local_space"),D->bLocalSpace);O->SetNumberField(TEXT("sim_target"),static_cast<int32>(D->SimTarget));O->SetBoolField(TEXT("persistent_ids"),D->bRequiresPersistentIDs!=0);O->SetNumberField(TEXT("bounds_mode"),static_cast<int32>(D->CalculateBoundsMode));O->SetBoolField(TEXT("determinism"),D->bDeterminism);O->SetNumberField(TEXT("random_seed"),D->RandomSeed);O->SetNumberField(TEXT("scalability_quality_mask"),D->Platforms.QualityLevelMask);return Json(O);}
struct FEmitter{TWeakObjectPtr<UNiagaraEmitter> Emitter;FGuid Version;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TArray<TWeakObjectPtr<UNiagaraRendererProperties>> Renderers;TArray<TWeakObjectPtr<UNiagaraScript>> Scripts;TWeakObjectPtr<UNiagaraEmitter> Parent,MergeParent;bool Latest=false;FString UniqueName,PropertyState;};
struct FHandle{FGuid Id,Version;TWeakObjectPtr<UNiagaraEmitter> Emitter;TWeakObjectPtr<UNiagaraEmitterBase> Base;FVersionedNiagaraEmitterData* Data=nullptr;FName Name;bool Enabled=false;ENiagaraEmitterMode Mode=ENiagaraEmitterMode::Standard;};
struct FSystem{TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<FHandle> Handles;};
struct FWork
{
 FResult R;bool Cancelled=false;FString CancelText;bool Persistent=false,WorldCaptured=false,TypedPassed=false,AnchorCaptured=false;FGuid AnchorId,AnchorVersion;TWeakObjectPtr<UNiagaraEmitter> AnchorEmitter;FVersionedNiagaraEmitterData* AnchorData=nullptr;TWeakObjectPtr<UNiagaraScript> AnchorSpawn,AnchorUpdate;TWeakObjectPtr<UNiagaraScriptSourceBase> AnchorEmitterSource;TWeakObjectPtr<UNiagaraGraph> AnchorEmitterGraph;TArray<TWeakObjectPtr<UNiagaraScript>> AnchorEmitterScripts;TArray<FScript> AnchorScripts;TArray<TWeakObjectPtr<UNiagaraGraph>> AnchorScriptGraphs;FString FailedStage;TWeakObjectPtr<UWorld> World;TArray<TUniquePtr<FIdentity>> Objects;TArray<FGraph> Graphs;TArray<FScript> Scripts;TArray<FEmitter> Emitters;TArray<FSystem> Systems;TMap<UNiagaraSpriteRendererProperties*,UMaterialInterface*> ExpectedMaterials;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls,SaveRows,CreationRows;TWeakObjectPtr<UNiagaraSystem> Source,Target;bool SourceDirty=false,SourceCaptured=false;FString SourceFilename,SourceFileHash;int64 SourceFileSize=-1;FString RequestedObject,RequestedPackage,RequestedFilename;int64 Slots=0,Chars=0;
 explicit FWork(bool P):Persistent(P){R.Compiled=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);}
 bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
 TSharedPtr<FJsonObject> Error(const FString& M){if(R.Error.IsEmpty())Fail(TEXT("niagara_sprite_preset_bootstrap_failed"),M);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),M);return O;}
 bool Budget(int64 N,int64 Text=0){Slots+=N;Chars+=Text;return (Slots<=262144&&Chars<=32ll*1024*1024)||Fail(TEXT("niagara_sprite_preset_output_budget_exceeded"),TEXT("Bounded snapshot/output exceeded; partial prefix retained."));}
 bool Admit(const FString& Stage){R.Stage=Stage;if(!R.Error.IsEmpty())return false;if(Cancelled||ShuttingDown.load())return Fail(TEXT("niagara_sprite_preset_async_cancelled"),CancelText.IsEmpty()?TEXT("Controller shutdown/cancelled; no rollback or compiler cancellation"):CancelText);if(!Safe())return Fail(TEXT("niagara_sprite_preset_context_unavailable"),TEXT("Live Editor game thread outside GC/exit/save required."));if(Persistent&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld))return Fail(TEXT("niagara_sprite_preset_play_session_active"),TEXT("Persistent factory creation requires stopped PIE/SIE."));auto* W=GEditor->GetEditorWorldContext().World();if(!Live(W)||W->bIsTearingDown)return Fail(TEXT("niagara_sprite_preset_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){World=W;WorldCaptured=true;}else if(World.Get()!=W)return Fail(TEXT("niagara_sprite_preset_world_changed"),TEXT("Editor world changed during SDK dispatch."));for(const auto& I:Objects)if(!I->Exact())return Fail(TEXT("niagara_sprite_preset_identity_changed"),TEXT("Captured object class/outer/package/path changed; replacement not adopted."));
 if(SourceCaptured&&(!Live(Source.Get())||Source->GetOutermost()->IsDirty()!=SourceDirty))return Fail(TEXT("niagara_sprite_preset_source_dirty_changed"),TEXT("Captured source dirty state changed across SDK callbacks; no replay or automatic source save."));
 if(AnchorCaptured){auto* O=Target.Get();if(!Live(O)||O->GetEmitterHandles().Num()!=1||O->GetSystemSpawnScript()!=AnchorSpawn.Get()||O->GetSystemUpdateScript()!=AnchorUpdate.Get())return Fail(TEXT("niagara_sprite_preset_target_changed"),TEXT("Created root or system scripts changed."));const auto& H=O->GetEmitterHandles()[0];if(H.GetId()!=AnchorId||H.GetInstance().Emitter!=AnchorEmitter.Get()||H.GetInstance().Version!=AnchorVersion||H.GetEmitterData()!=AnchorData||!Live(AnchorEmitter.Get())||!AnchorEmitter->IsIn(O)||AnchorEmitter->GetPackage()!=O->GetPackage())return Fail(TEXT("niagara_sprite_preset_emitter_changed"),TEXT("Created emitter exact version/owner changed; no replacement adopted."));if(AnchorData->GraphSource!=AnchorEmitterSource.Get())return Fail(TEXT("niagara_sprite_preset_anchor_source_changed"),TEXT("Created emitter source binding changed between continuation frames"));auto* ES=Cast<UNiagaraScriptSource>(AnchorEmitterSource.Get());if((ES?ES->NodeGraph.Get():nullptr)!=AnchorEmitterGraph.Get())return Fail(TEXT("niagara_sprite_preset_anchor_graph_changed"),TEXT("Created emitter NodeGraph binding changed"));TArray<UNiagaraScript*> AS;AnchorData->GetScripts(AS,false,false);if(AS.Num()!=AnchorEmitterScripts.Num())return Fail(TEXT("niagara_sprite_preset_anchor_scripts_changed"),TEXT("Created emitter script membership changed"));for(int32 I=0;I<AS.Num();++I)if(AS[I]!=AnchorEmitterScripts[I].Get())return Fail(TEXT("niagara_sprite_preset_anchor_scripts_changed"),TEXT("Created emitter script order changed"));for(int32 I=0;I<AnchorScripts.Num();++I){const auto& A=AnchorScripts[I];auto* Script=A.Script.Get();if(!Live(Script)||Script->GetLatestSource()!=A.Source.Get()||Script->GetExposedVersion().VersionGuid!=A.Exposed)return Fail(TEXT("niagara_sprite_preset_anchor_script_source_changed"),TEXT("Created script source/version binding changed"));auto* SS=Cast<UNiagaraScriptSource>(A.Source.Get());if((SS?SS->NodeGraph.Get():nullptr)!=AnchorScriptGraphs[I].Get())return Fail(TEXT("niagara_sprite_preset_anchor_script_graph_changed"),TEXT("Created script NodeGraph binding changed"));}}
 for(const auto& S:Scripts)if(S.Script->GetLatestSource()!=S.Source.Get()||S.Script->GetExposedVersion().VersionGuid!=S.Exposed)return Fail(TEXT("niagara_sprite_preset_script_changed"),TEXT("Captured script source/version changed."));
 for(const auto& E:Emitters){auto* O=E.Emitter.Get();if(O->GetEmitterData(E.Version)!=E.Data||(E.Latest&&O->GetLatestEmitterData()!=E.Data)||!O->GetUniqueEmitterName().Equals(E.UniqueName,ESearchCase::CaseSensitive)||!EmitterPropertyState(E.Data).Equals(E.PropertyState,ESearchCase::CaseSensitive)||E.Data->GraphSource!=E.Source.Get()||E.Data->VersionedParent.Emitter!=E.Parent.Get()||E.Data->VersionedParentAtLastMerge.Emitter!=E.MergeParent.Get()||E.Data->GetRenderers().Num()!=E.Renderers.Num())return Fail(TEXT("niagara_sprite_preset_emitter_changed"),TEXT("Emitter version/source/parent/renderer topology changed."));for(int32 I=0;I<E.Renderers.Num();++I)if(E.Data->GetRenderers()[I]!=E.Renderers[I].Get())return Fail(TEXT("niagara_sprite_preset_renderer_changed"),TEXT("Physical renderer slot changed."));TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);if(A.Num()!=E.Scripts.Num())return Fail(TEXT("niagara_sprite_preset_emitter_scripts_changed"),TEXT("Emitter scripts changed."));for(int32 I=0;I<A.Num();++I)if(A[I]!=E.Scripts[I].Get())return Fail(TEXT("niagara_sprite_preset_emitter_scripts_changed"),TEXT("Emitter script order changed."));}
 for(const auto& S:Systems){auto* O=S.System.Get();if(O->GetSystemSpawnScript()!=S.Spawn.Get()||O->GetSystemUpdateScript()!=S.Update.Get()||O->GetEmitterHandles().Num()!=S.Handles.Num())return Fail(TEXT("niagara_sprite_preset_system_changed"),TEXT("System script/emitter topology changed."));for(int32 I=0;I<S.Handles.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=S.Handles[I];if(H.GetId()!=E.Id||H.GetEmitterBase()!=E.Base.Get()||H.GetEmitterMode()!=E.Mode||H.GetName()!=E.Name||H.GetIsEnabled()!=E.Enabled||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.Version||H.GetEmitterData()!=E.Data)return Fail(TEXT("niagara_sprite_preset_system_changed"),TEXT("System physical emitter/version changed."));}}
 for(const auto& G:Graphs){if(G.Source->NodeGraph!=G.Graph.Get()||G.Graph->Nodes.Num()!=G.Nodes.Num())return Fail(TEXT("niagara_sprite_preset_graph_changed"),TEXT("Captured source graph membership changed."));for(int32 I=0;I<G.Nodes.Num();++I){auto* N=G.Nodes[I].Node.Get();const auto& E=G.Nodes[I];if(G.Graph->Nodes[I]!=N||N->NodeGuid!=E.Id||N->Pins.Num()!=E.Pins.Num())return Fail(TEXT("niagara_sprite_preset_node_changed"),TEXT("Captured node/pin topology changed."));for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& V=E.Pins[J];if(P!=V.P||P->PinId!=V.Id||P->PersistentGuid!=V.Persistent||P->PinName!=V.Name||P->Direction!=V.Direction||P->PinType!=V.Type||P->bNotConnectable!=V.NotConnectable||P->bDefaultValueIsReadOnly!=V.DefaultReadOnly||P->DefaultObject!=V.DefaultObject.Get()||!P->AutogeneratedDefaultValue.Equals(V.AutoDefault,ESearchCase::CaseSensitive)||!P->DefaultTextValue.ToString().Equals(V.DefaultText,ESearchCase::CaseSensitive)||P->GetOwningNodeUnchecked()!=N||P->LinkedTo!=V.Links||!P->DefaultValue.Equals(V.Default,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_sprite_preset_pin_changed"),TEXT("Exact pin/links/default changed across SDK call."));}}}
 for(const auto& Pair:ExpectedMaterials)if(Pair.Key->Material!=Pair.Value)return Fail(TEXT("niagara_sprite_preset_material_readback_failed"),TEXT("Owned sprite material differs from requested exact material."));return true;}
 bool Pin(UObject* O){if(!Admit(TEXT("pin_object"))||!Live(O))return Fail(TEXT("niagara_sprite_preset_object_unavailable"),TEXT("Expected live stock object."));for(const auto& I:Objects)if(I->Object.Get()==O)return I->Exact();if(!Budget(1,O->GetPathName().Len()))return false;if(ExecutionPins)ExecutionPins->Emplace(O);Objects.Add(MakeUnique<FIdentity>(O));return true;}
 bool SDK(const TCHAR* Stage,bool Mutation=false,bool PersistentWrite=false){if(!Admit(Stage)||!Budget(1,FCString::Strlen(Stage)))return false;R.bMutationAttempted|=Mutation;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);if(PersistentWrite)R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("stage"),Stage);O->SetBoolField(TEXT("entered"),true);O->SetBoolField(TEXT("returned"),false);O->SetBoolField(TEXT("mutation_attempted"),Mutation);Calls.Add(MakeShared<FJsonValueObject>(O));return true;}
 bool After(const TCHAR* Stage){if(!Calls.IsEmpty())Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
 template<typename F>bool Step(const TCHAR* Stage,F&& Fn,bool Mutation=false,bool PersistentWrite=false){if(!SDK(Stage,Mutation,PersistentWrite))return false;Fn();return After(Stage);}
 bool Graph(UNiagaraScriptSourceBase* Base){if(!Admit(TEXT("capture_graph")))return false;if(!Base)return true;if(!Pin(Base))return false;auto* S=Cast<UNiagaraScriptSource>(Base);if(!S||!S->NodeGraph)return true;auto* G=S->NodeGraph.Get();for(const auto& E:Graphs)if(E.Source.Get()==S&&E.Graph.Get()==G)return true;if(!Pin(G)||G->Nodes.Num()>16384)return Fail(TEXT("niagara_sprite_preset_graph_bound"),TEXT("Graph node bound exceeded."));FGraph E;E.Source=S;E.Graph=G;TSet<UEdGraphPin*> Known;for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N))return Fail(TEXT("niagara_sprite_preset_graph_malformed"),TEXT("Null/unavailable graph node."));FNode NE;NE.Node=N;NE.Id=N->NodeGuid;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N||!Budget(1+P->LinkedTo.Num(),P->DefaultValue.Len()+P->AutogeneratedDefaultValue.Len()+P->DefaultTextValue.ToString().Len()))return Fail(TEXT("niagara_sprite_preset_pin_malformed"),TEXT("Null/foreign pin or capture bound exceeded."));FPin PE;PE.P=P;PE.Id=P->PinId;PE.Persistent=P->PersistentGuid;PE.Name=P->PinName;PE.Direction=P->Direction;PE.Type=P->PinType;PE.NotConnectable=P->bNotConnectable;PE.DefaultReadOnly=P->bDefaultValueIsReadOnly;PE.DefaultObject=P->DefaultObject;PE.AutoDefault=P->AutogeneratedDefaultValue;PE.DefaultText=P->DefaultTextValue.ToString();PE.Links=P->LinkedTo;PE.Default=P->DefaultValue;Known.Add(P);if(P->DefaultObject&&!Pin(P->DefaultObject))return false;if(P->PinType.PinSubCategoryObject.IsValid()&&!Pin(P->PinType.PinSubCategoryObject.Get()))return false;NE.Pins.Add(MoveTemp(PE));}if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()>65536)return Fail(TEXT("niagara_sprite_preset_signature_bound"),TEXT("Function signature bound exceeded."));if(F->FunctionScript&&!Pin(F->FunctionScript))return false;}E.Nodes.Add(MoveTemp(NE));}for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links)if(!L||!Known.Contains(L))return Fail(TEXT("niagara_sprite_preset_link_malformed"),TEXT("Link target is outside captured graph."));Graphs.Add(MoveTemp(E));return Admit(TEXT("graph_captured"));}
 bool Script(UNiagaraScript* S){if(!S)return true;for(const auto& E:Scripts)if(E.Script.Get()==S)return true;if(!Pin(S))return false;FScript E;E.Script=S;E.Source=S->GetLatestSource();E.Exposed=S->GetExposedVersion().VersionGuid;Scripts.Add(E);return Graph(E.Source.Get());}
 bool Emitter(UNiagaraEmitter* O,FVersionedNiagaraEmitterData* D,bool Latest){if(!O||!D||!Pin(O))return Fail(TEXT("niagara_sprite_preset_emitter_data_missing"),TEXT("Expected current versioned emitter data."));for(const auto& E:Emitters)if(E.Emitter.Get()==O&&E.Data==D)return true;FEmitter E;E.Emitter=O;E.Data=D;E.Version=D->Version.VersionGuid;E.Latest=Latest;E.UniqueName=O->GetUniqueEmitterName();E.PropertyState=EmitterPropertyState(D);E.Source=D->GraphSource;E.Parent=D->VersionedParent.Emitter;E.MergeParent=D->VersionedParentAtLastMerge.Emitter;if(!Graph(E.Source.Get()))return false;if(D->GetRenderers().Num()>4096)return Fail(TEXT("niagara_sprite_preset_renderer_bound"),TEXT("Renderer bound exceeded."));for(UNiagaraRendererProperties* P:D->GetRenderers()){E.Renderers.Add(P);if(P&&!Pin(P))return false;if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(P)){ExpectedMaterials.Add(Sprite,Sprite->Material);if(Sprite->Material&&!Pin(Sprite->Material))return false;}}TArray<UNiagaraScript*> A;D->GetScripts(A,false,false);if(A.Num()>4096)return Fail(TEXT("niagara_sprite_preset_script_bound"),TEXT("Script bound exceeded."));for(auto* P:A){E.Scripts.Add(P);if(!Script(P))return false;}Emitters.Add(MoveTemp(E));return Admit(TEXT("emitter_captured"));}
 bool System(UNiagaraSystem* O,int32 ExpectedCount){if(!Pin(O)||O->GetEmitterHandles().Num()!=ExpectedCount)return Fail(TEXT("niagara_sprite_preset_system_shape_invalid"),TEXT("Factory output emitter count differs from requested construction."));FSystem E;E.System=O;E.Spawn=O->GetSystemSpawnScript();E.Update=O->GetSystemUpdateScript();if(!Script(E.Spawn.Get())||!Script(E.Update.Get()))return false;for(const auto& H:O->GetEmitterHandles()){if(!H.GetEmitterBase()||!Pin(H.GetEmitterBase()))return Fail(TEXT("niagara_sprite_preset_output_owner_invalid"),TEXT("Exact public emitter base unavailable."));if((!H.GetInstance().Emitter||!H.GetEmitterData())&&H.GetEmitterMode()!=ENiagaraEmitterMode::Stateless)return Fail(TEXT("niagara_sprite_preset_output_owner_invalid"),TEXT("Standard emitter has no current versioned instance."));FHandle F;F.Id=H.GetId();F.Base=H.GetEmitterBase();F.Name=H.GetName();F.Enabled=H.GetIsEnabled();F.Mode=H.GetEmitterMode();F.Version=H.GetInstance().Version;F.Emitter=H.GetInstance().Emitter;F.Data=H.GetEmitterData();E.Handles.Add(F);if(F.Emitter.IsValid()&&F.Data&&!Emitter(F.Emitter.Get(),F.Data,false))return false;}Systems.Add(MoveTemp(E));return Admit(TEXT("system_captured"));}
 bool BindCreated(const FString& Path){if(!Admit(TEXT("bind_created_system")))return false;if(!NormalizeExpected(Path))return false;auto* O=FindObject<UNiagaraSystem>(nullptr,*RequestedObject);if(!Live(O)||O->GetClass()!=UNiagaraSystem::StaticClass()||!O->GetPathName().Equals(RequestedObject,ESearchCase::CaseSensitive)||!O->GetOutermost()->GetName().Equals(RequestedPackage,ESearchCase::CaseSensitive)||O->GetOuter()!=O->GetOutermost()||O->GetPackage()!=O->GetOutermost()||O->GetEmitterHandles().Num()!=1)return Fail(TEXT("niagara_sprite_preset_created_identity_invalid"),TEXT("Returned created path is not the requested exact stock System root."));Target=O;if(!Pin(O)||!Pin(O->GetOutermost()))return false;const auto H=O->GetEmitterHandles()[0];UNiagaraEmitter* E=H.GetInstance().Emitter;if(!Live(E)||!H.GetEmitterData()||!E->IsIn(O)||E->GetPackage()!=O->GetPackage()||!Pin(E))return Fail(TEXT("niagara_sprite_preset_created_emitter_invalid"),TEXT("Expected one owned standard emitter."));AnchorId=H.GetId();AnchorVersion=H.GetInstance().Version;AnchorEmitter=E;AnchorData=H.GetEmitterData();AnchorSpawn=O->GetSystemSpawnScript();AnchorUpdate=O->GetSystemUpdateScript();if(!Live(AnchorSpawn.Get())||!Live(AnchorUpdate.Get())||!Pin(AnchorSpawn.Get())||!Pin(AnchorUpdate.Get()))return false;AnchorEmitterSource=AnchorData->GraphSource;auto CaptureSource=[&](UNiagaraScriptSourceBase* SourceBase, TWeakObjectPtr<UNiagaraGraph>& OutGraph){if(SourceBase&&!Pin(SourceBase))return false;if(auto* SS=Cast<UNiagaraScriptSource>(SourceBase)){OutGraph=SS->NodeGraph;if(OutGraph.IsValid()&&!Pin(OutGraph.Get()))return false;}return true;};if(!CaptureSource(AnchorEmitterSource.Get(),AnchorEmitterGraph))return false;TArray<UNiagaraScript*> A;AnchorData->GetScripts(A,false,false);for(auto* S:A)AnchorEmitterScripts.Add(S);A.AddUnique(AnchorSpawn.Get());A.AddUnique(AnchorUpdate.Get());for(auto* S:A){if(!S)continue;if(!Pin(S))return false;FScript B;B.Script=S;B.Source=S->GetLatestSource();B.Exposed=S->GetExposedVersion().VersionGuid;TWeakObjectPtr<UNiagaraGraph> G;if(!CaptureSource(B.Source.Get(),G))return false;AnchorScripts.Add(B);AnchorScriptGraphs.Add(G);}AnchorCaptured=true;return Admit(TEXT("created_system_bound"));}
 bool NormalizeExpected(const FString& Path){return Path.Equals(RequestedObject,ESearchCase::CaseSensitive)||Fail(TEXT("niagara_sprite_preset_returned_path_mismatch"),TEXT("Child created path differs from normalized requested root."));}
 TSharedPtr<FJsonObject> DirtyProjection(){auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;const bool CanRead=Safe()&&WorldCaptured&&Live(World.Get())&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get();for(const auto& I:Objects){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),I->Path);J->SetStringField(TEXT("package"),I->PackageName);J->SetBoolField(TEXT("dirty_before"),I->DirtyBefore);const bool Exact=CanRead&&I->Exact();J->SetBoolField(TEXT("current_observation_available"),Exact);if(Exact)J->SetBoolField(TEXT("dirty_after"),I->Package->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("objects"),Rows);O->SetBoolField(TEXT("safe_context"),CanRead);return O;}
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("read_continuation"));}

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
FString NormalizeRequiredRoot(FString RequiredRoot)
{
    RequiredRoot.TrimStartAndEndInline();
    RequiredRoot.TrimQuotesInline();
    RequiredRoot.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (RequiredRoot.Len() > 1 && RequiredRoot.EndsWith(TEXT("/")))
    {
        RequiredRoot.LeftChopInline(1);
    }
    return RequiredRoot;
}
bool IsUnderRequiredRoot(const FString& PackageName, const FString& RequiredRoot)
{
    const FString NormalizedRoot = NormalizeRequiredRoot(RequiredRoot);
    return NormalizedRoot.IsEmpty() ||
        PackageName.Equals(NormalizedRoot, ESearchCase::IgnoreCase) ||
        PackageName.StartsWith(NormalizedRoot + TEXT("/"), ESearchCase::IgnoreCase);
}
TArray<FString> GetStringArrayParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName)
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
            Value.ReplaceInline(TEXT("\\"), TEXT("/"));
            if (!Value.IsEmpty())
            {
                Values.AddUnique(NormalizeRequiredRoot(PackagePathFromObjectPath(Value)));
            }
        }
    }
    return Values;
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

bool IsMCPDependencyReference(const FString& Value)
{
    FString Candidate = FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();
    Candidate.TrimQuotesInline();
    if (Candidate.IsEmpty())
    {
        return false;
    }

    return Candidate.Contains(TEXT("CubelessMCP"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("ToolsetRegistry"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("ModelContextProtocol"),ESearchCase::IgnoreCase)||Candidate.Contains(TEXT("/Script/UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("MCPUnreal"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("mcp_unreal"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/Script/CubelessControlRigTools"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessControlRigTools"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessControlRigToolsEditor"), ESearchCase::IgnoreCase);
}
FString GetMCPDependencyReferenceError(const FString& FieldName, const FString& Value)
{
    const FString FieldLabel = FieldName.IsEmpty() ? TEXT("value") : FieldName;
    return FString::Printf(
        TEXT("Refusing to persist MCP-only reference in '%s': %s. UnrealMCP is an editor authoring bridge; finished assets must not depend on it."),
        *FieldLabel,
        *Value);
}FString NormalizeNiagaraEnumToken(FString Value)
{
    Value.TrimStartAndEndInline();
    Value.ToLowerInline();
    Value.ReplaceInline(TEXT("_"), TEXT(""));
    Value.ReplaceInline(TEXT("-"), TEXT(""));
    Value.ReplaceInline(TEXT(" "), TEXT(""));
    return Value;
}
bool TryParseVector3FromJsonArray(const TArray<TSharedPtr<FJsonValue>>* Arr, FVector& OutVector)
{
    if (!Arr || Arr->Num() != 3)
    {
        return false;
    }
    double Components[3] = {0.0, 0.0, 0.0};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        double NumberValue = 0.0;
        if (!(*Arr)[Index].IsValid() || !(*Arr)[Index]->TryGetNumber(NumberValue) || !FMath::IsFinite(NumberValue))
        {
            return false;
        }
        Components[Index] = NumberValue;
    }
    OutVector = FVector(Components[0], Components[1], Components[2]);
    return true;
}
bool TryParseVector2FromJsonArray(const TArray<TSharedPtr<FJsonValue>>* Arr, FVector2D& OutVector)
{
    if (!Arr || Arr->Num() != 2)
    {
        return false;
    }
    double Components[2] = {0.0, 0.0};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        double NumberValue = 0.0;
        if (!(*Arr)[Index].IsValid() || !(*Arr)[Index]->TryGetNumber(NumberValue) || !FMath::IsFinite(NumberValue))
        {
            return false;
        }
        Components[Index] = NumberValue;
    }
    OutVector = FVector2D(Components[0], Components[1]);
    return true;
}
bool ParseNiagaraSpriteAlignmentToken(const FString& Value, ENiagaraSpriteAlignment& OutValue)
{
    const FString Token = NormalizeNiagaraEnumToken(Value);
    if (Token == TEXT("unaligned"))                                          { OutValue = ENiagaraSpriteAlignment::Unaligned; return true; }
    if (Token == TEXT("velocityaligned") || Token == TEXT("velocity"))       { OutValue = ENiagaraSpriteAlignment::VelocityAligned; return true; }
    if (Token == TEXT("customalignment") || Token == TEXT("custom"))         { OutValue = ENiagaraSpriteAlignment::CustomAlignment; return true; }
    if (Token == TEXT("automatic") || Token == TEXT("auto"))                 { OutValue = ENiagaraSpriteAlignment::Automatic; return true; }
    return false;
}
bool ParseNiagaraSpriteFacingModeToken(const FString& Value, ENiagaraSpriteFacingMode& OutValue)
{
    const FString Token = NormalizeNiagaraEnumToken(Value);
    if (Token == TEXT("facecamera") || Token == TEXT("camera"))              { OutValue = ENiagaraSpriteFacingMode::FaceCamera; return true; }
    if (Token == TEXT("facecameraplane") || Token == TEXT("cameraplane"))    { OutValue = ENiagaraSpriteFacingMode::FaceCameraPlane; return true; }
    if (Token == TEXT("customfacingvector") || Token == TEXT("custom"))      { OutValue = ENiagaraSpriteFacingMode::CustomFacingVector; return true; }
    if (Token == TEXT("facecameraposition"))                                 { OutValue = ENiagaraSpriteFacingMode::FaceCameraPosition; return true; }
    if (Token == TEXT("facecameradistanceblend"))                            { OutValue = ENiagaraSpriteFacingMode::FaceCameraDistanceBlend; return true; }
    if (Token == TEXT("automatic") || Token == TEXT("auto"))                 { OutValue = ENiagaraSpriteFacingMode::Automatic; return true; }
    return false;
}
FString NiagaraTypeName(const FNiagaraTypeDefinition& TypeDef)
{
    return TypeDef.GetNameText().ToString();
}
FString NiagaraScriptUsageName(ENiagaraScriptUsage Usage)
{
    if (const UEnum* Enum = StaticEnum<ENiagaraScriptUsage>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Usage));
    }
    return FString::FromInt(static_cast<int32>(Usage));
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
TArray<TSharedPtr<FJsonValue>> NumberArrayToJsonValues(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (double Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueNumber>(Value));
    }
    return JsonValues;
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
bool VerifyRI();
void RememberModuleEdits(const TSharedPtr<FJsonObject>& Receipt);

TSharedPtr<FJsonObject> ErrorResponse(const FString& Message){return Current->Error(Message);}
// Dedicated GU3 receipt boundary only. Actual typed SDK prefix is adopted before JSON evidence validation.
constexpr int64 ChildReceiptCharLimit=32ll*1024*1024;
constexpr int64 ChildReceiptSlotLimit=262144;
constexpr int32 ChildReceiptDepthLimit=64;
struct FChildReceiptBudget
{
 int64 Slots=0,TextChars=0;int32 MaximumDepth=0;FString Reason,At;
 TSet<const FJsonObject*> ActiveObjects;TSet<const FJsonValue*> ActiveValues;
 bool Reject(const TCHAR* Why,const FString& Where){if(Reason.IsEmpty()){Reason=Why;At=Where.Left(1024);}return false;}
 bool Text(const FString& S,const FString& Where){if(S.Len()>ChildReceiptCharLimit-TextChars)return Reject(TEXT("text_utf16_budget"),Where);TextChars+=S.Len();return true;}
 bool Value(const TSharedPtr<FJsonValue>& V,int32 Depth,const FString& Where)
 {
  MaximumDepth=FMath::Max(MaximumDepth,Depth);if(Depth>ChildReceiptDepthLimit)return Reject(TEXT("depth_budget"),Where);if(++Slots>ChildReceiptSlotLimit)return Reject(TEXT("slot_budget"),Where);if(!V.IsValid())return Reject(TEXT("null_shared_value"),Where);if(ActiveValues.Contains(V.Get()))return Reject(TEXT("value_cycle"),Where);ActiveValues.Add(V.Get());bool Good=false;
  switch(V->Type){case EJson::Null:case EJson::Boolean:Good=true;break;case EJson::Number:{double N=0;Good=V->TryGetNumber(N)&&FMath::IsFinite(N);if(!Good)Reject(TEXT("nonfinite_number"),Where);break;}case EJson::String:Good=Text(V->AsString(),Where);break;case EJson::Array:{Good=true;const auto& A=V->AsArray();if(A.Num()>ChildReceiptSlotLimit-Slots){Good=Reject(TEXT("slot_budget"),Where);break;}for(int32 I=0;I<A.Num();++I)if(!Value(A[I],Depth+1,Where+FString::Printf(TEXT("[%d]"),I))){Good=false;break;}break;}case EJson::Object:Good=Object(V->AsObject(),Depth,Where);break;default:Good=Reject(TEXT("unsupported_json_type"),Where);break;}
  ActiveValues.Remove(V.Get());return Good;
 }
 bool Object(const TSharedPtr<FJsonObject>& O,int32 Depth,const FString& Where)
 {
  if(!O.IsValid())return Reject(TEXT("null_shared_object"),Where);if(ActiveObjects.Contains(O.Get()))return Reject(TEXT("object_cycle"),Where);ActiveObjects.Add(O.Get());bool Good=true;
  if(O->Values.Num()>ChildReceiptSlotLimit-Slots)Good=Reject(TEXT("slot_budget"),Where);
  if(Good)for(const auto& Pair:O->Values){const FString Key(Pair.Key);if(!Text(Key,Where)||!Value(Pair.Value,Depth+1,(Where+TEXT(".")+Key.Left(128)).Left(1024))){Good=false;break;}}
  ActiveObjects.Remove(O.Get());return Good;
 }
};
// Public JSON writer against a counting archive: no FString output allocation and no parse round-trip.
// Structural validation above prevents cycles/nonfinite data before Serialize touches the shared JSON graph.
struct FChildReceiptCountArchive: FArchive
{
 int64 Bytes=0;bool Overflow=false;
 FChildReceiptCountArchive(){SetIsSaving(true);}
 virtual void Serialize(void*,int64 ByteCount) override{if(ByteCount<0||Bytes>MAX_int64-ByteCount){Overflow=true;SetError();return;}Bytes+=ByteCount;if(Bytes>ChildReceiptCharLimit*static_cast<int64>(sizeof(TCHAR)))SetError();}
};

// GU-only lossless representation for CH-added graph observations. Original CH API/result rows are unchanged.
// Full graph payloads live once at the Native job root; child/legacy/stage/session copies contain exact references only.
constexpr const TCHAR* GraphEvidenceFormat=TEXT("cubeless.gu3.graph_observation.intern.v1");
constexpr const TCHAR* GraphEvidenceOwner=TEXT("native_job.data.graph_observation_evidence.entries");
bool EvidenceEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B,int32 Depth=0)
{
 if(!A.IsValid()||!B.IsValid()||Depth>ChildReceiptDepthLimit||A->Type!=B->Type)return false;
 switch(A->Type)
 {
 case EJson::Null:return true;
 case EJson::Boolean:return A->AsBool()==B->AsBool();
 case EJson::Number:return FMath::IsFinite(A->AsNumber())&&FMath::IsFinite(B->AsNumber())&&A->AsNumber()==B->AsNumber();
 case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
 case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num())return false;for(int32 I=0;I<X.Num();++I)if(!EvidenceEqual(X[I],Y[I],Depth+1))return false;return true;}
 case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num())return false;for(const auto& P:X->Values)if(!EvidenceEqual(P.Value,Y->TryGetField(P.Key.ToView()),Depth+1))return false;return true;}
 default:return false;
 }
}
struct FGraphEvidenceView
{
 struct FReference{TSharedPtr<FJsonObject> Ref;TSharedPtr<FJsonValue> Original;};
 TSharedRef<FJsonObject> Entries=MakeShared<FJsonObject>();
 TSharedPtr<FJsonObject> View,Envelope;
 TArray<FReference> References;
 FString Reason,At;
 bool Applied=false,ReferencesComplete=false;
 bool Reject(const TCHAR* Why,const FString& Where){if(Reason.IsEmpty()){Reason=Why;At=Where.Left(1024);}return false;}
 bool Intern(const TSharedPtr<FJsonValue>& Original,const FString& Path,const FString& Where,TSharedPtr<FJsonValue>& Out)
 {
  FChildReceiptBudget One;if(!Original.IsValid()||Original->Type!=EJson::Object)return Reject(TEXT("graph_evidence_shape"),Where);
  if(!One.Value(Original,0,Where))return Reject(*One.Reason,One.At);
  auto Object=Original->AsObject();const auto PathValue=Object->TryGetField(TEXT("graph_path"));
  if(!PathValue.IsValid()||PathValue->Type!=EJson::String||!PathValue->AsString().Equals(Path,ESearchCase::CaseSensitive))return Reject(TEXT("graph_evidence_path_mismatch"),Where);
  FString Id;
  // Never infer equality from graph path, ChangeID or a digest. Every reused entry is structurally compared.
  for(const auto& Existing:Entries->Values)
  {
   if(!Existing.Value.IsValid()||Existing.Value->Type!=EJson::Object)continue;
   const auto ExistingPath=Existing.Value->AsObject()->TryGetField(TEXT("graph_path"));
   if(ExistingPath.IsValid()&&ExistingPath->Type==EJson::String&&ExistingPath->AsString().Equals(Path,ESearchCase::CaseSensitive)&&EvidenceEqual(Original,Existing.Value)){Id=FString(Existing.Key.ToView());break;}
  }
  if(Id.IsEmpty())
  {
   if(Entries->Values.Num()>=ChildReceiptSlotLimit)return Reject(TEXT("graph_evidence_entry_budget"),Where);
   int32 Index=Entries->Values.Num();do{Id=FString::Printf(TEXT("g%d"),Index++);}while(Entries->HasField(Id));
   Entries->SetField(Id,Original);
  }
  auto Ref=MakeShared<FJsonObject>();Ref->SetNumberField(TEXT("representation_version"),1);
  Ref->SetStringField(TEXT("graph_evidence_ref"),Id);Ref->SetStringField(TEXT("graph_evidence_owner"),GraphEvidenceOwner);Ref->SetStringField(TEXT("graph_path"),Path);
  FReference R;R.Ref=Ref;R.Original=Original;References.Add(MoveTemp(R));Out=MakeShared<FJsonValueObject>(Ref);return true;
 }
 bool Phase(const TCHAR* Name)
 {
  const auto Original=View->TryGetField(Name);if(!Original.IsValid())return true;
  if(Original->Type!=EJson::Object)return Reject(TEXT("observation_phase_shape"),Name);
  auto PhaseView=MakeShared<FJsonObject>(*Original->AsObject());
  for(const TCHAR* Kind:{TEXT("owned_graphs"),TEXT("module_sources")})
  {
   const auto MapValue=PhaseView->TryGetField(Kind);if(!MapValue.IsValid())continue;
   if(MapValue->Type!=EJson::Object)return Reject(TEXT("observation_graph_map_shape"),FString(Name)+TEXT(".")+Kind);
   auto MapView=MakeShared<FJsonObject>();
   for(const auto& Pair:MapValue->AsObject()->Values)
   {
    const FString Path(Pair.Key.ToView()),Where=(FString(Name)+TEXT(".")+Kind+TEXT(".")+Path.Left(256)).Left(1024);
    TSharedPtr<FJsonValue> Ref;
    if(FCString::Strcmp(Kind,TEXT("owned_graphs"))==0)
    {if(!Intern(Pair.Value,Path,Where,Ref))return false;MapView->SetField(Path,Ref);}
    else
    {
     if(!Pair.Value.IsValid()||Pair.Value->Type!=EJson::Object)return Reject(TEXT("module_source_row_shape"),Where);
     auto Row=MakeShared<FJsonObject>(*Pair.Value->AsObject());
     if(!Intern(Row->TryGetField(TEXT("graph")),Path,Where+TEXT(".graph"),Ref))return false;
     Row->SetField(TEXT("graph"),Ref);MapView->SetObjectField(Path,Row);
    }
   }
   PhaseView->SetObjectField(Kind,MapView);
  }
  View->SetObjectField(Name,PhaseView);return true;
 }
 bool Prepare(const FResult& Child,const TCHAR* Operation)
 {
  View=MakeShared<FJsonObject>(*Child.Data);
  if(FCString::Strcmp(Operation,TEXT("set_niagara_module_inputs_batch"))!=0)return true;
  Applied=true;const auto Existing=Current->R.Data->TryGetField(TEXT("graph_observation_evidence"));
  if(Existing.IsValid())
  {
   if(Existing->Type!=EJson::Object)return Reject(TEXT("graph_evidence_owner_shape"),TEXT("job.data.graph_observation_evidence"));
   auto E=Existing->AsObject();FString Format;if(!E->TryGetStringField(TEXT("format"),Format)||!Format.Equals(GraphEvidenceFormat,ESearchCase::CaseSensitive))return Reject(TEXT("graph_evidence_owner_format"),TEXT("job.data.graph_observation_evidence"));
   const auto OldEntries=E->TryGetField(TEXT("entries"));if(!OldEntries.IsValid()||OldEntries->Type!=EJson::Object)return Reject(TEXT("graph_evidence_owner_entries"),TEXT("job.data.graph_observation_evidence.entries"));
   // Existing committed payloads are immutable; stage a new map so a rejected child cannot leave dangling refs.
   for(const auto& P:OldEntries->AsObject()->Values)Entries->SetField(FString(P.Key.ToView()),P.Value);
  }
  for(const TCHAR* Name:{TEXT("before_observation"),TEXT("after_edits"),TEXT("after_compile_request"),TEXT("after_save")})if(!Phase(Name))return false;
  for(int32 I=0;I<128;++I)if(!Phase(*FString::Printf(TEXT("after_edit_%d"),I)))return false;
  for(const auto& R:References)
  {
   FString Id,Owner;double Version=0;
   if(!R.Ref.IsValid()||R.Ref->Values.Num()!=4||!R.Ref->TryGetStringField(TEXT("graph_evidence_ref"),Id)||!R.Ref->TryGetStringField(TEXT("graph_evidence_owner"),Owner)||!Owner.Equals(GraphEvidenceOwner,ESearchCase::CaseSensitive)||!R.Ref->TryGetNumberField(TEXT("representation_version"),Version)||Version!=1||!EvidenceEqual(R.Original,Entries->TryGetField(Id)))return Reject(TEXT("graph_evidence_reference_incomplete"),Id);
  }
  ReferencesComplete=true;View->SetStringField(TEXT("graph_observation_representation"),GraphEvidenceFormat);View->SetStringField(TEXT("graph_evidence_owner"),GraphEvidenceOwner);
  View->SetNumberField(TEXT("graph_evidence_reference_count"),References.Num());View->SetBoolField(TEXT("graph_evidence_references_complete"),true);
  Envelope=MakeShared<FJsonObject>();Envelope->SetStringField(TEXT("format"),GraphEvidenceFormat);Envelope->SetObjectField(TEXT("entries"),Entries);Envelope->SetNumberField(TEXT("unique_graph_count"),Entries->Values.Num());
  Envelope->SetStringField(TEXT("scope"),TEXT("Lossless CH-added graph payloads only; exact refs are relative to this Native job root. Original aggregate/results, per-phase non-graph state and all real preservation observations are retained. No path/hash/ChangeID equality inference."));
  return true;
 }
};

TSharedRef<FJsonObject> TypedChildJson(const FResult& Child,const TCHAR* Operation)
{
 auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),Child.bSuccess);J->SetStringField(TEXT("operation"),Operation);J->SetStringField(TEXT("stage"),Child.Stage);J->SetStringField(TEXT("error_code"),Child.ErrorCode);J->SetStringField(TEXT("error"),Child.Error);J->SetBoolField(TEXT("mutation_attempted"),Child.bMutationAttempted);J->SetBoolField(TEXT("change_state_known"),Child.bChangeStateKnown);if(Child.bChangeStateKnown)J->SetBoolField(TEXT("changed"),Child.bChanged);else J->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());J->SetBoolField(TEXT("postcondition_passed"),Child.bPostconditionPassed);J->SetBoolField(TEXT("compile_requested"),Child.bCompileRequested);J->SetBoolField(TEXT("compile_attempted"),Child.bCompileAttempted);if(Child.Compiled.IsSet())J->SetBoolField(TEXT("compiled"),*Child.Compiled);else J->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());J->SetBoolField(TEXT("save_requested"),Child.bSaveRequested);J->SetBoolField(TEXT("save_attempted"),Child.bSaveAttempted);J->SetBoolField(TEXT("saved"),Child.bSaved);J->SetBoolField(TEXT("atomic_rollback_available"),false);J->SetBoolField(TEXT("auto_retry_safe"),false);J->SetObjectField(TEXT("data"),Child.Data);return J;
}
void AdoptActualTypedPrefix(const FResult& Child,const TCHAR* Stage)
{
 auto& W=*Current;W.R.bMutationAttempted|=Child.bMutationAttempted;W.R.bCompileAttempted|=Child.bCompileAttempted;W.R.bSaveAttempted|=Child.bSaveAttempted;W.R.bSaved|=Child.bSaved;W.R.bChanged|=Child.bChanged;if(Child.bCompileAttempted)W.R.Compiled.Reset();auto Prefix=MakeShared<FJsonObject>();Prefix->SetStringField(TEXT("stage"),Stage);Prefix->SetStringField(TEXT("child_stage"),Child.Stage.Left(4096));Prefix->SetStringField(TEXT("child_error_code"),Child.ErrorCode.Left(4096));Prefix->SetStringField(TEXT("child_error"),Child.Error.Left(4096));Prefix->SetBoolField(TEXT("child_header_text_truncated"),Child.Stage.Len()>4096||Child.ErrorCode.Len()>4096||Child.Error.Len()>4096);Prefix->SetBoolField(TEXT("returned_success"),Child.bSuccess);Prefix->SetBoolField(TEXT("mutation_attempted"),Child.bMutationAttempted);Prefix->SetBoolField(TEXT("change_state_known"),Child.bChangeStateKnown);if(Child.bChangeStateKnown)Prefix->SetBoolField(TEXT("changed"),Child.bChanged);else Prefix->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());Prefix->SetBoolField(TEXT("postcondition_passed"),Child.bPostconditionPassed);Prefix->SetBoolField(TEXT("compile_requested"),Child.bCompileRequested);Prefix->SetBoolField(TEXT("compile_attempted"),Child.bCompileAttempted);if(Child.Compiled.IsSet())Prefix->SetBoolField(TEXT("compiled"),*Child.Compiled);else Prefix->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());Prefix->SetBoolField(TEXT("save_requested"),Child.bSaveRequested);Prefix->SetBoolField(TEXT("save_attempted"),Child.bSaveAttempted);Prefix->SetBoolField(TEXT("saved"),Child.bSaved);W.R.Data->SetObjectField(TEXT("last_child_actual_prefix"),Prefix);W.R.Data->SetBoolField(TEXT("last_child_outcome_unknown"),!Child.bChangeStateKnown);W.R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);
}
TSharedPtr<FJsonObject> AdoptNativeObject(const TSharedPtr<FJsonObject>& Native,const TCHAR* Stage,bool PrefixAlreadyAdopted=false,bool ReceiptComplete=true)
{
 auto& W=*Current;auto GetBool=[&](const TCHAR* Key){bool V=false;Native->TryGetBoolField(Key,V);return V;};bool Known=false;Native->TryGetBoolField(TEXT("change_state_known"),Known);
 if(!PrefixAlreadyAdopted){W.R.bMutationAttempted|=GetBool(TEXT("mutation_attempted"));W.R.bCompileAttempted|=GetBool(TEXT("compile_attempted"));W.R.bSaveAttempted|=GetBool(TEXT("save_attempted"));W.R.bSaved|=GetBool(TEXT("saved"));bool Changed=false;Native->TryGetBoolField(TEXT("changed"),Changed);W.R.bChanged|=Changed;W.R.Data->SetBoolField(TEXT("last_child_outcome_unknown"),!Known);W.R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);if(GetBool(TEXT("compile_attempted")))W.R.Compiled.Reset();}
 const TSharedPtr<FJsonObject>* Data=nullptr;const TSharedPtr<FJsonObject>* Leaf=nullptr;TSharedPtr<FJsonObject> Legacy=MakeShared<FJsonObject>();if(Native->TryGetObjectField(TEXT("data"),Data)&&Data&&Data->IsValid()){if((*Data)->TryGetObjectField(TEXT("legacy_result"),Leaf)&&Leaf&&Leaf->IsValid())Legacy=MakeShared<FJsonObject>(**Leaf);else Legacy=MakeShared<FJsonObject>(**Data);}Legacy->SetObjectField(TEXT("native_result"),Native);Legacy->SetBoolField(TEXT("success"),ReceiptComplete&&GetBool(TEXT("success"))&&GetBool(TEXT("postcondition_passed"))&&Known);W.R.Data->SetObjectField(TEXT("last_child_result"),Native);
 if(!W.After(Stage))return Legacy;if(!GetBool(TEXT("success"))||!GetBool(TEXT("postcondition_passed"))||!Known){FString Code,Message;Native->TryGetStringField(TEXT("error_code"),Code);Native->TryGetStringField(TEXT("error"),Message);if(Message.IsEmpty())Message=TEXT("Dedicated child target health was incomplete; no replay.");W.Fail(Code.IsEmpty()?TEXT("niagara_sprite_preset_child_failed"):*Code,Message);Legacy->SetStringField(TEXT("error"),Message);}return Legacy;
}
TSharedPtr<FJsonObject> AdoptNative(const FResult& Child,const TCHAR* Operation,const TCHAR* Stage)
{
 auto& W=*Current;AdoptActualTypedPrefix(Child,Stage);auto Native=TypedChildJson(Child,Operation);
 FGraphEvidenceView Evidence;const bool Prepared=Evidence.Prepare(Child,Operation);
 if(Prepared&&Evidence.Applied)Native->SetObjectField(TEXT("data"),Evidence.View);
 // Include the entire staged canonical table in the unchanged child budget. Refs are not a budget bypass.
 TSharedRef<FJsonObject> BudgetRoot=Native;
 if(Prepared&&Evidence.Applied){BudgetRoot=MakeShared<FJsonObject>();BudgetRoot->SetObjectField(TEXT("native_receipt"),Native);BudgetRoot->SetObjectField(TEXT("canonical_graph_evidence"),Evidence.Envelope);}
 FChildReceiptBudget Budget;const bool Shape=Prepared&&Budget.Value(MakeShared<FJsonValueObject>(BudgetRoot),0,TEXT("child_bundle"));
 FChildReceiptCountArchive Count;bool Serialized=false;if(Shape)Serialized=FJsonSerializer::Serialize(BudgetRoot,TJsonWriterFactory<>::Create(&Count));
 const bool WireFits=Shape&&Serialized&&!Count.Overflow&&Count.Bytes<=ChildReceiptCharLimit*static_cast<int64>(sizeof(TCHAR));
 auto Diagnostic=MakeShared<FJsonObject>();Diagnostic->SetStringField(TEXT("stage"),Stage);Diagnostic->SetStringField(TEXT("route"),TEXT("typed_FResult_no_parse"));
 Diagnostic->SetNumberField(TEXT("UTF16_character_limit"),ChildReceiptCharLimit);Diagnostic->SetNumberField(TEXT("slot_limit"),ChildReceiptSlotLimit);Diagnostic->SetNumberField(TEXT("depth_limit"),ChildReceiptDepthLimit);
 Diagnostic->SetNumberField(TEXT("observed_slots"),Budget.Slots);Diagnostic->SetNumberField(TEXT("observed_text_UTF16_characters"),Budget.TextChars);Diagnostic->SetNumberField(TEXT("observed_maximum_depth"),Budget.MaximumDepth);
 Diagnostic->SetBoolField(TEXT("structural_budget_passed"),Shape);Diagnostic->SetBoolField(TEXT("serialized_measure_complete"),Serialized&&!Count.Overflow);Diagnostic->SetNumberField(TEXT("counted_serialized_TCHAR_bytes"),Count.Bytes);Diagnostic->SetNumberField(TEXT("TCHAR_bytes"),sizeof(TCHAR));
 Diagnostic->SetNumberField(TEXT("counted_serialized_UTF16_characters"),Count.Bytes/static_cast<int64>(sizeof(TCHAR)));Diagnostic->SetBoolField(TEXT("serialized_UTF16_limit_exceeded"),Count.Bytes>ChildReceiptCharLimit*static_cast<int64>(sizeof(TCHAR)));
 Diagnostic->SetBoolField(TEXT("actual_child_prefix_observed"),true);Diagnostic->SetBoolField(TEXT("payload_omitted"),!WireFits);Diagnostic->SetBoolField(TEXT("data_complete"),WireFits);
 Diagnostic->SetBoolField(TEXT("graph_evidence_intern_applied"),Evidence.Applied);Diagnostic->SetBoolField(TEXT("graph_evidence_references_complete"),Prepared&&(!Evidence.Applied||Evidence.ReferencesComplete));
 Diagnostic->SetBoolField(TEXT("graph_evidence_pool_committed"),WireFits&&Evidence.Applied);Diagnostic->SetNumberField(TEXT("graph_evidence_reference_count"),Evidence.References.Num());Diagnostic->SetNumberField(TEXT("graph_evidence_unique_count"),Evidence.Entries->Values.Num());
 Diagnostic->SetStringField(TEXT("graph_evidence_representation"),GraphEvidenceFormat);Diagnostic->SetStringField(TEXT("budget_scope"),TEXT("Current typed receipt plus entire staged job-root unique graph table; unchanged limits. Parent Python raw/native copies and total RPC size are separate."));
 if(!WireFits)
 {
  const FString Reason=!Prepared?Evidence.Reason:!Shape?Budget.Reason:Count.Overflow?TEXT("serialized_size_overflow"):Count.Bytes>ChildReceiptCharLimit*static_cast<int64>(sizeof(TCHAR))?TEXT("serialized_utf16_budget"):TEXT("JSON_writer_failure");
  Diagnostic->SetStringField(TEXT("failure_reason"),Reason);Diagnostic->SetStringField(TEXT("failure_path"),!Prepared?Evidence.At:Budget.At);
  Diagnostic->SetStringField(TEXT("omitted_scope"),TEXT("Current child Data and uncommitted graph refs/table omitted. Actual scalar prefix and previously committed evidence remain; omitted evidence is not success or safe replay."));
  Native->SetStringField(TEXT("stage"),Child.Stage.Left(4096));Native->SetStringField(TEXT("error_code"),Child.ErrorCode.Left(4096));Native->SetStringField(TEXT("error"),Child.Error.Left(4096));Native->SetObjectField(TEXT("data"),Diagnostic);
  W.R.bChangeStateKnown=false;W.Fail(TEXT("niagara_sprite_preset_child_receipt_budget"),FString(TEXT("Typed child receipt rejected: "))+Reason+TEXT("; actual prefix retained, no replay."));
 }
 else if(Evidence.Applied)
 {
  // Single canonical owner: never place the large table in child Native/Legacy/stage/session subtrees.
  W.R.Data->SetObjectField(TEXT("graph_observation_evidence"),Evidence.Envelope);
 }
 W.R.Data->SetObjectField(TEXT("last_child_receipt_diagnostic"),Diagnostic);return AdoptNativeObject(Native,Stage,true,WireFits);
}
TSharedPtr<FJsonObject> AdoptNative(const FString& Encoded,const TCHAR* Stage)
{
 auto& W=*Current;auto Diagnostic=MakeShared<FJsonObject>();Diagnostic->SetStringField(TEXT("stage"),Stage);Diagnostic->SetStringField(TEXT("route"),TEXT("encoded_FString"));Diagnostic->SetNumberField(TEXT("encoded_UTF16_characters"),Encoded.Len());Diagnostic->SetNumberField(TEXT("encoded_UTF16_payload_bytes"),static_cast<double>(Encoded.Len())*sizeof(TCHAR));Diagnostic->SetNumberField(TEXT("UTF16_character_limit"),ChildReceiptCharLimit);const bool Limit=Encoded.Len()>ChildReceiptCharLimit;Diagnostic->SetBoolField(TEXT("UTF16_limit_exceeded"),Limit);FString ParserError;TSharedPtr<FJsonObject> Native;bool Parsed=false;if(!Limit){auto Reader=TJsonReaderFactory<>::Create(Encoded);Parsed=FJsonSerializer::Deserialize(Reader,Native)&&Native.IsValid();ParserError=Reader->GetErrorMessage();}Diagnostic->SetBoolField(TEXT("parse_attempted"),!Limit);Diagnostic->SetBoolField(TEXT("parse_succeeded"),Parsed);Diagnostic->SetStringField(TEXT("parser_error"),ParserError);Diagnostic->SetBoolField(TEXT("actual_child_prefix_observed"),Parsed);Diagnostic->SetBoolField(TEXT("payload_omitted"),!Parsed);W.R.Data->SetObjectField(TEXT("last_child_receipt_diagnostic"),Diagnostic);
 if(!Parsed){Diagnostic->SetStringField(TEXT("failure_reason"),Limit?TEXT("encoded_utf16_budget"):TEXT("JSON_parse_failure"));Diagnostic->SetStringField(TEXT("omitted_scope"),TEXT("Unparsed child text not adopted as outcome; previous child is not evidence for this dispatch. Original request must not be replayed."));auto Failed=MakeShared<FJsonObject>();Failed->SetBoolField(TEXT("success"),false);Failed->SetObjectField(TEXT("data"),Diagnostic);W.R.Data->SetObjectField(TEXT("last_child_result"),Failed);W.R.Data->SetObjectField(TEXT("last_child_actual_prefix"),Diagnostic);W.R.Data->SetBoolField(TEXT("last_child_outcome_unknown"),true);W.R.bChangeStateKnown=false;W.Fail(TEXT("niagara_sprite_preset_child_receipt_invalid"),Limit?TEXT("Dedicated child encoded UTF16 length exceeds unchanged32Mi-character cap; no replay."):TEXT("Dedicated child JSON parse failed; parser diagnostic retained, no replay."));W.After(Stage);return ErrorResponse(W.R.Error);}return AdoptNativeObject(Native,Stage);
}
template<typename F>TSharedPtr<FJsonObject> RunChild(const TCHAR* Stage,const TSharedPtr<FJsonObject>& Params,F&& Call){auto& W=*Current;if(!W.SDK(Stage))return ErrorResponse(W.R.Error);auto Leaf=AdoptNative(Call(Json(Params)),Stage);if(W.R.Error.IsEmpty()&&!VerifyRI())Leaf->SetBoolField(TEXT("success"),false);return Leaf;}
TSharedPtr<FJsonObject> HandleCreateBasicSpriteNiagaraSystem(const TSharedPtr<FJsonObject>& P){return RunChild(TEXT("GP_CreateBasicSpriteSystem"),P,[](const FString& S){return NiagaraFactoryBootstrap::CreateBasicSpriteSystem(S);});}
TSharedPtr<FJsonObject> HandleSetNiagaraSpriteRendererSettings(const TSharedPtr<FJsonObject>& P){return RunChild(TEXT("GE_SetSpriteSettings"),P,[](const FString& S){return NiagaraRenderers::SetSpriteSettings(S);});}
TSharedPtr<FJsonObject> HandleSetNiagaraEmitterFixedBounds(const TSharedPtr<FJsonObject>& P){return RunChild(TEXT("GF_SetFixedBounds"),P,[](const FString& S){return NiagaraEmitterConfiguration::SetFixedBounds(S);});}
TSharedPtr<FJsonObject> HandleSetNiagaraEmitterLocalSpace(const TSharedPtr<FJsonObject>& P){return RunChild(TEXT("GF_SetLocalSpace"),P,[](const FString& S){return NiagaraEmitterConfiguration::SetLocalSpace(S);});}
TSharedPtr<FJsonObject> HandleAddNiagaraStockModuleToStack(const TSharedPtr<FJsonObject>& P)
{
 auto& W=*Current;if(!W.SDK(TEXT("NiagaraModule_Add")))return ErrorResponse(W.R.Error);FString Path,Module,Usage;P->TryGetStringField(TEXT("target_system_path"),Path);P->TryGetStringField(TEXT("module_script_path"),Module);P->TryGetStringField(TEXT("target_usage"),Usage);
 auto Child=NiagaraModule::Add(Path,Module,Usage,FString(),TEXT("0"),FString(),TEXT("null"),FString(),true,true,false,false);
 FResult V;V.bSuccess=Child.bSuccess;V.bMutationAttempted=Child.bMutationAttempted;V.bChanged=Child.bChanged;V.bChangeStateKnown=Child.bChangeStateKnown;V.bPostconditionPassed=Child.bPostconditionPassed;V.Compiled=false;V.Stage=Child.Stage;V.ErrorCode=Child.ErrorCode;V.Error=Child.Error;V.Data=Child.Data;
 auto Leaf=AdoptNative(V,TEXT("add_niagara_stock_module_to_stack"),TEXT("NiagaraModule_Add"));if(W.R.Error.IsEmpty()&&!VerifyRI())Leaf->SetBoolField(TEXT("success"),false);return Leaf;
}
TSharedPtr<FJsonObject> HandleSetNiagaraModuleInputsBatch(const TSharedPtr<FJsonObject>& P,const TFunction<bool(UNiagaraSystem*,const TCHAR*)>* OwnedAuthoringGuard=nullptr)
{
 auto& W=*Current;FString Path,Operation;P->TryGetStringField(TEXT("system_path"),Path);P->TryGetStringField(TEXT("operation"),Operation);bool Overwrite=false;P->TryGetBoolField(TEXT("overwrite_existing"),Overwrite);const TArray<TSharedPtr<FJsonValue>>* Edits=nullptr;if(!P->TryGetArrayField(TEXT("edits"),Edits)||!Edits)return ErrorResponse(TEXT("Missing preset edits"));
 TArray<TSharedPtr<FJsonValue>> PhysicalEdits;for(const auto& V:*Edits){if(!V.IsValid()||V->Type!=EJson::Object)return ErrorResponse(TEXT("Malformed preset edit"));auto O=MakeShared<FJsonObject>(*V->AsObject());O->SetNumberField(TEXT("emitter_index"),0);PhysicalEdits.Add(MakeShared<FJsonValueObject>(O));}
 FString EditsJson;FJsonSerializer::Serialize(PhysicalEdits,TJsonWriterFactory<>::Create(&EditsJson));if(!W.SDK(TEXT("NiagaraModule_SetInputsBatch")))return ErrorResponse(W.R.Error);
 const int32 ParentCallIndex=W.Calls.Num()-1;auto Child=NiagaraModule::SetInputsBatch(Path,EditsJson,Operation,Overwrite,false,true,false,false,false,10.0,0.1,OwnedAuthoringGuard);
 if(W.Calls.IsValidIndex(ParentCallIndex))W.Calls[ParentCallIndex]->AsObject()->SetBoolField(TEXT("returned"),true);
 auto Leaf=AdoptNative(Child,TEXT("set_niagara_module_inputs_batch"),TEXT("NiagaraModule_SetInputsBatch"));if(W.R.Error.IsEmpty()){RememberModuleEdits(Leaf);if(!VerifyRI())Leaf->SetBoolField(TEXT("success"),false);}return Leaf;
}
bool FloatStorage(double N){return FMath::IsFinite(N)&&N>=-static_cast<double>(MAX_flt)&&N<=static_cast<double>(MAX_flt);}
bool SafeTypedNumbers(const FNiagaraVariable& V,const TSharedPtr<FJsonValue>& J)
{
 const auto& T=V.GetType();if(T==FNiagaraTypeDefinition::GetFloatDef()||T==FNiagaraTypeDefinition::GetIntDef()){double N=0;if(!J.IsValid()||!J->TryGetNumber(N)||!FMath::IsFinite(N))return false;return T==FNiagaraTypeDefinition::GetIntDef()?(N>=MIN_int32&&N<=MAX_int32):FloatStorage(N);}if(J.IsValid()&&J->Type==EJson::Array)for(const auto& X:J->AsArray()){double N=0;if(!X.IsValid()||!X->TryGetNumber(N)||!FloatStorage(N))return false;}return true;
}

bool SetNiagaraVariableDataFromJson(FNiagaraVariable& Variable, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
{
    if(!SafeTypedNumbers(Variable,JsonValue)){OutError=TEXT("Preset default exceeds finite native storage range");return false;}
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

struct FRIExpected{TWeakObjectPtr<UNiagaraScript> Script;FNiagaraVariable Variable;TArray<uint8> Bytes;};
TArray<FRIExpected>* RIExpected=nullptr;
bool SameStoreShape(UNiagaraScript* Script,const TArray<FNiagaraVariable>& Before){TArray<FNiagaraVariable> Now;Script->RapidIterationParameters.GetParameters(Now);if(Now.Num()!=Before.Num())return false;for(int32 I=0;I<Now.Num();++I)if(Now[I].GetName()!=Before[I].GetName()||Now[I].GetType()!=Before[I].GetType())return false;return true;}
bool VerifyRI(){auto& W=*Current;if(!W.Admit(TEXT("verify_initialize_RI")))return false;for(const auto& E:*RIExpected){auto* S=E.Script.Get();if(!Live(S))return W.Fail(TEXT("niagara_sprite_preset_RI_script_lost"),TEXT("Initialize RI script expired."));const uint8* Actual=S->RapidIterationParameters.GetParameterData(E.Variable);if(!Actual||E.Bytes.Num()!=E.Variable.GetSizeInBytes()||FMemory::Memcmp(Actual,E.Bytes.GetData(),E.Bytes.Num())!=0)return W.Fail(TEXT("niagara_sprite_preset_RI_readback_failed"),TEXT("Actual typed Initialize RI bytes differ from requested stored bytes."));}return true;}
TSharedPtr<FJsonObject> SetGeneratedPresetInitializeParticleDefaults(UNiagaraSystem* System,int32 EmitterIndex,const FVector2D& SpriteSize,double Lifetime)
{
 auto& W=*Current;auto Result=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;TArray<FString> Missing;int32 SizeUpdates=0,LifeUpdates=0;
 auto Publish=[&](){Result->SetArrayField(TEXT("updated_parameters"),Rows);Result->SetNumberField(TEXT("updated_parameter_count"),Rows.Num());Result->SetArrayField(TEXT("missing_fields"),StringArrayToJsonValues(Missing));W.R.Data->SetObjectField(TEXT("initialize_particle_partial"),Result);};
 if(!W.Admit(TEXT("initialize_RI_entry"))||System!=W.Target.Get()||EmitterIndex!=0)return ErrorResponse(TEXT("Exact created Initialize emitter unavailable"));
 TArray<UNiagaraScript*> Scripts;System->GetEmitterHandles()[0].GetEmitterData()->GetScripts(Scripts,false,false);if(Scripts.Num()>4096)return ErrorResponse(TEXT("Initialize script bound exceeded"));for(auto* S:Scripts)if(S&&!W.Pin(S))return ErrorResponse(W.R.Error);
 if(!W.Step(TEXT("Initialize_System_Modify"),[&]{System->Modify();},true,true))return ErrorResponse(W.R.Error);
 for(auto* S:Scripts){if(!W.Admit(TEXT("initialize_script")))break;if(!S||(S->GetUsage()!=ENiagaraScriptUsage::ParticleSpawnScript&&S->GetUsage()!=ENiagaraScriptUsage::ParticleSpawnScriptInterpolated))continue;
 TArray<FNiagaraVariable> Params;S->RapidIterationParameters.GetParameters(Params);if(Params.Num()>65536){W.Fail(TEXT("niagara_sprite_preset_RI_bound"),TEXT("Initialize parameter bound exceeded"));break;}
 for(const auto& V:Params){if(!W.Admit(TEXT("initialize_parameter")))break;const FString Name=V.GetName().ToString();const bool Size=Name.Contains(TEXT(".SetVariables_"),ESearchCase::IgnoreCase)&&Name.EndsWith(TEXT(".Particles.SpriteSize"),ESearchCase::IgnoreCase);const bool Life=Name.Contains(TEXT(".SetVariables_"),ESearchCase::IgnoreCase)&&Name.EndsWith(TEXT(".Particles.Lifetime"),ESearchCase::IgnoreCase);if(!Size&&!Life)continue;
 const uint8* Old=S->RapidIterationParameters.GetParameterData(V);if(!Old){W.Fail(TEXT("niagara_sprite_preset_RI_data_missing"),TEXT("Matched Initialize typed declaration has no value data"));break;}if(V.GetType().GetScriptStruct()&&!W.Pin(V.GetType().GetScriptStruct()))break;if(V.GetType().GetEnum()&&!W.Pin(V.GetType().GetEnum()))break;FNiagaraVariable Updated(V);Updated.SetData(Old);TSharedPtr<FJsonValue> Previous,Desired;if(Size)Desired=NumberArrayToJson({SpriteSize.X,SpriteSize.Y});else Desired=MakeShared<FJsonValueNumber>(Lifetime);FString ConversionError;
 if(!W.SDK(TEXT("Initialize_ConvertTypedValue")))break;Previous=NiagaraVariableDataToJsonValue(Updated);const bool Converted=SetNiagaraVariableDataFromJson(Updated,Desired,ConversionError);if(!W.After(TEXT("Initialize_ConvertTypedValue")))break;if(!Converted){W.Fail(TEXT("niagara_sprite_preset_RI_conversion_failed"),ConversionError);break;}
 FRIExpected E;E.Script=S;E.Variable=V;E.Bytes.Append(Updated.GetData(),Updated.GetSizeInBytes());
 if(!W.Step(TEXT("Initialize_Script_Modify"),[&]{S->Modify();},true,true))break;if(!SameStoreShape(S,Params)){W.Fail(TEXT("niagara_sprite_preset_RI_shape_changed"),TEXT("RI store declarations changed in Modify callback"));break;}
 const FString TypeLabel=NiagaraTypeName(V.GetType()),ScriptName=S->GetName(),UsageName=NiagaraScriptUsageName(S->GetUsage());
 bool SetReturned=false;if(!W.SDK(TEXT("Initialize_SetParameterData"),true,true))break;SetReturned=S->RapidIterationParameters.SetParameterData(E.Bytes.GetData(),V,false);
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("field"),Size?TEXT("sprite_size"):TEXT("lifetime"));Row->SetStringField(TEXT("parameter_name"),Name);Row->SetStringField(TEXT("parameter_type"),TypeLabel);Row->SetStringField(TEXT("script_name"),ScriptName);Row->SetStringField(TEXT("script_usage"),UsageName);Row->SetField(TEXT("previous_value"),Previous);Row->SetBoolField(TEXT("setter_returned"),SetReturned);Rows.Add(MakeShared<FJsonValueObject>(Row));Publish();
 if(!W.After(TEXT("Initialize_SetParameterData")))break;if(!SameStoreShape(S,Params)){W.Fail(TEXT("niagara_sprite_preset_RI_shape_changed"),TEXT("RI store declarations changed during setter callback"));break;}
 const uint8* Actual=S->RapidIterationParameters.GetParameterData(V);if(!SetReturned||!Actual||FMemory::Memcmp(Actual,E.Bytes.GetData(),E.Bytes.Num())!=0){W.Fail(TEXT("niagara_sprite_preset_RI_readback_failed"),TEXT("Initialize setter returned false or actual bytes differ"));break;}FNiagaraVariable Readback(V);Readback.SetData(Actual);Row->SetField(TEXT("new_value"),NiagaraVariableDataToJsonValue(Readback));RIExpected->Add(MoveTemp(E));if(Size)++SizeUpdates;else ++LifeUpdates;W.R.bChanged=true;
 if(!W.Step(TEXT("Initialize_MarkPackageDirty"),[&]{System->MarkPackageDirty();},true,true))break;
 }if(!W.R.Error.IsEmpty())break;}
 if(SizeUpdates==0)Missing.Add(TEXT("sprite_size"));if(LifeUpdates==0)Missing.Add(TEXT("lifetime"));Publish();const bool Passed=W.R.Error.IsEmpty()&&Missing.IsEmpty()&&VerifyRI();Result->SetBoolField(TEXT("success"),Passed);if(!Passed){if(W.R.Error.IsEmpty())W.Fail(TEXT("niagara_sprite_preset_initialize_fields_missing"),TEXT("Missing InitializeParticle RI fields; actual partial updates retained"));Result->SetStringField(TEXT("error"),W.R.Error);return Result;}
 Result->SetStringField(TEXT("system_path"),System->GetPathName());Result->SetNumberField(TEXT("emitter_index"),EmitterIndex);Result->SetArrayField(TEXT("sprite_size"),NumberArrayToJsonValues({SpriteSize.X,SpriteSize.Y}));Result->SetNumberField(TEXT("lifetime"),Lifetime);Result->SetBoolField(TEXT("compile_requested"),false);Result->SetBoolField(TEXT("saved"),false);Result->SetStringField(TEXT("write_scope"),TEXT("generated_temp_initialize_particle_rapid_iteration_defaults"));return Result;
}


FString BytesHex(const uint8* P,int32 N){return BytesToHex(P,N);}
void RememberModuleEdits(const TSharedPtr<FJsonObject>& Leaf)
{
 auto& W=*Current;if(!W.Admit(TEXT("remember_module_values")))return;const TSharedPtr<FJsonObject>* Native=nullptr;const TSharedPtr<FJsonObject>* Data=nullptr;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!Leaf->TryGetObjectField(TEXT("native_result"),Native)||!Native||!(*Native)->TryGetObjectField(TEXT("data"),Data)||!Data||!(*Data)->TryGetArrayField(TEXT("results"),Rows)||!Rows){W.Fail(TEXT("niagara_sprite_preset_module_receipt_incomplete"),TEXT("Actual module edit rows required for final independent RI readback"));return;}
 for(const auto& V:*Rows){auto Row=V->AsObject();bool Success=false;Row->TryGetBoolField(TEXT("success"),Success);if(!Success)continue;FString Path,Name,Hex;Row->TryGetStringField(TEXT("script_path"),Path);Row->TryGetStringField(TEXT("rapid_iteration_name"),Name);Row->TryGetStringField(TEXT("new_bytes_hex"),Hex);auto* S=FindObject<UNiagaraScript>(nullptr,*Path);if(!Live(S)||!S->IsIn(W.Target.Get())||S->GetPackage()!=W.Target->GetPackage()||!W.Pin(S)){W.Fail(TEXT("niagara_sprite_preset_module_RI_owner_invalid"),TEXT("Child edit's exact script is not owned by the created root"));return;}
 TArray<FNiagaraVariable> Vars;S->RapidIterationParameters.GetParameters(Vars);int32 Matches=0;for(const auto& P:Vars){if(!P.GetName().ToString().Equals(Name,ESearchCase::CaseSensitive))continue;++Matches;const uint8* Actual=S->RapidIterationParameters.GetParameterData(P);if(!Actual||P.GetSizeInBytes()<=0||P.GetSizeInBytes()>4096||!BytesHex(Actual,P.GetSizeInBytes()).Equals(Hex,ESearchCase::IgnoreCase)){W.Fail(TEXT("niagara_sprite_preset_module_RI_readback_failed"),TEXT("Child expected bytes do not match independent actual RI bytes"));return;}FRIExpected E;E.Script=S;E.Variable=P;E.Bytes.Append(Actual,P.GetSizeInBytes());RIExpected->Add(MoveTemp(E));}if(Matches!=1){W.Fail(TEXT("niagara_sprite_preset_module_RI_ambiguous"),TEXT("Child typed RI name is missing or ambiguous"));return;}}
}


bool PackageOwnership(FWork& W,UNiagaraSystem* Root,const TCHAR* Stage)
{
 if(!W.Admit(Stage)||Root!=W.Target.Get()||!Live(Root))return false;UPackage* P=Root->GetOutermost();if(Root->GetOuter()!=P||Root->GetPackage()!=P||!Root->GetPathName().Equals(W.RequestedObject,ESearchCase::CaseSensitive)||!P->GetName().Equals(W.RequestedPackage,ESearchCase::CaseSensitive))return W.Fail(TEXT("niagara_sprite_preset_root_package_changed"),TEXT("Exact created root/package changed"));TArray<UObject*> Exports;if(!W.Step(TEXT("GetObjectsWithPackage_Preset"),[&]{GetObjectsWithPackage(P,Exports,true);}))return false;if(Exports.Num()>65536||!W.Budget(Exports.Num()))return W.Fail(TEXT("niagara_sprite_preset_export_bound"),TEXT("Package export capture exceeds bound"));for(UObject* O:Exports){if(!Live(O)||O->GetPackage()!=P||O->GetOutermost()!=P||(O!=Root&&!O->IsIn(Root)))return W.Fail(TEXT("niagara_sprite_preset_foreign_package_export"),TEXT("Requested package contains object outside exact created root subtree; save refused, residual retained"));if(!W.Pin(O))return false;}return W.Admit(Stage);
}

bool SaveNiagaraAssetDirect(UNiagaraSystem* System,FString& Filename)
{
 auto& W=*Current;Filename.Reset();if(!PackageOwnership(W,System,TEXT("before_preset_save"))||!VerifyRI())return false;auto* P=System->GetOutermost();const FString Path=W.RequestedFilename;if(Path.IsEmpty())return W.Fail(TEXT("niagara_sprite_preset_filename_invalid"),TEXT("Exact package filename unavailable"));if(!W.SDK(TEXT("Preset_SavePackage"),true,true))return false;W.R.bSaveAttempted=true;FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;const bool Returned=UPackage::SavePackage(P,System,*Path,Args);W.R.bSaved|=Returned;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("asset"),W.RequestedObject);Row->SetStringField(TEXT("package"),W.RequestedPackage);Row->SetStringField(TEXT("filename"),Path);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Returned);W.SaveRows.Add(MakeShared<FJsonValueObject>(Row));if(Returned)Filename=Path;if(!W.After(TEXT("Preset_SavePackage"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}Row->SetBoolField(TEXT("dirty_after"),P->IsDirty());const bool Healthy=Returned&&!P->IsDirty()&&PackageOwnership(W,System,TEXT("after_preset_save"))&&VerifyRI();Row->SetBoolField(TEXT("save_postcondition_passed"),Healthy);if(!Healthy&&W.R.Error.IsEmpty())W.Fail(TEXT("niagara_sprite_preset_save_failed"),TEXT("SavePackage false, dirty after callback, or post-save identity/readback failed; true write history retained"));return Healthy;
}

TSharedPtr<FJsonObject> BuildNiagaraDependencyAuditJson(
    UObject* Asset,
    const FString& SavedFilename,
    bool bBlockTempDependenciesForNonTempAsset,
    const TArray<FString>& AdditionalForbiddenDependencyPrefixes = TArray<FString>())
{
    auto& W=*Current;TSharedPtr<FJsonObject> AuditObject = MakeShared<FJsonObject>();
    AuditObject->SetBoolField(TEXT("success"), false);
    AuditObject->SetBoolField(TEXT("audit_complete"), false);
    AuditObject->SetBoolField(TEXT("asset_resolved"), Asset != nullptr);
    AuditObject->SetBoolField(TEXT("package_name_present"), false);
    AuditObject->SetBoolField(TEXT("dependency_query_attempted"), false);
    AuditObject->SetBoolField(TEXT("dependency_query_succeeded"), false);
    AuditObject->SetBoolField(TEXT("saved_file_scanned"), false);
    AuditObject->SetNumberField(TEXT("dependency_count"), 0);
    AuditObject->SetNumberField(TEXT("forbidden_dependency_hit_count"), 0);
    AuditObject->SetNumberField(TEXT("temp_dependency_hit_count"), 0);
    AuditObject->SetBoolField(TEXT("has_forbidden_dependency"), false);
    AuditObject->SetBoolField(TEXT("has_temp_dependency"), false);

    if(!W.Admit(TEXT("dependency_audit_entry")))return AuditObject;
    if (!Asset)
    {
        return AuditObject;
    }

    const FString PackageName = Asset->GetOutermost() ? Asset->GetOutermost()->GetName() : FString();
    const bool bPackageNamePresent = !PackageName.IsEmpty();
    AuditObject->SetBoolField(TEXT("package_name_present"), bPackageNamePresent);
    AuditObject->SetStringField(TEXT("package_name"), PackageName);
    AuditObject->SetStringField(TEXT("asset_path"), Asset->GetPathName());
    AuditObject->SetStringField(TEXT("saved_filename"), SavedFilename);
    AuditObject->SetArrayField(TEXT("additional_forbidden_dependency_prefixes"), StringArrayToJsonValues(AdditionalForbiddenDependencyPrefixes));

    FAssetRegistryModule* Registry=nullptr;if(!W.Step(TEXT("LoadModule_AssetRegistry"),[&]{Registry=FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));}))return AuditObject;if(!Registry){W.Fail(TEXT("niagara_sprite_preset_registry_unavailable"),TEXT("Public AssetRegistry module unavailable."));return AuditObject;}
    if (!SavedFilename.IsEmpty())
    {
        TArray<FString> ModifiedFiles;
        ModifiedFiles.Add(SavedFilename);
        if(!W.Step(TEXT("ScanModifiedAssetFiles"),[&]{Registry->Get().ScanModifiedAssetFiles(ModifiedFiles);}))return AuditObject;
        AuditObject->SetBoolField(TEXT("saved_file_scanned"), true);
    }

    TArray<FName> Dependencies;
    bool bDependencyQuerySucceeded = false;
    if (bPackageNamePresent)
    {
        AuditObject->SetBoolField(TEXT("dependency_query_attempted"), true);
        if(!W.Step(TEXT("GetDependencies"),[&]{bDependencyQuerySucceeded=Registry->Get().GetDependencies(FName(*PackageName),Dependencies);}))return AuditObject;
        if(Dependencies.Num()>65536||!W.Budget(Dependencies.Num())){W.Fail(TEXT("niagara_sprite_preset_dependency_bound"),TEXT("Direct dependency output exceeds bounded collection; partial audit retained."));return AuditObject;}
    }
    AuditObject->SetBoolField(TEXT("dependency_query_succeeded"), bDependencyQuerySucceeded);
    AuditObject->SetBoolField(TEXT("audit_complete"), bDependencyQuerySucceeded);
    AuditObject->SetBoolField(TEXT("success"), bDependencyQuerySucceeded);

    TArray<TSharedPtr<FJsonValue>> DependencyValues;
    TArray<TSharedPtr<FJsonValue>> ForbiddenHits;
    TArray<TSharedPtr<FJsonValue>> TempHits;
    for (const FName& Dependency : Dependencies)
    {
        const FString DependencyString = Dependency.ToString();if(!W.Budget(1,DependencyString.Len()))return AuditObject;
        DependencyValues.Add(MakeShared<FJsonValueString>(DependencyString));
        if (IsMCPDependencyReference(DependencyString))
        {
            ForbiddenHits.Add(MakeShared<FJsonValueString>(DependencyString));
        }
        for (const FString& ForbiddenPrefix : AdditionalForbiddenDependencyPrefixes)
        {
            if (!ForbiddenPrefix.IsEmpty() && IsUnderRequiredRoot(DependencyString, ForbiddenPrefix))
            {
                ForbiddenHits.Add(MakeShared<FJsonValueString>(DependencyString));
                break;
            }
        }
        if (bBlockTempDependenciesForNonTempAsset && IsUnderRequiredRoot(DependencyString, TEXT("/Game/_MCP_Temp")))
        {
            TempHits.Add(MakeShared<FJsonValueString>(DependencyString));
        }
    }

    if (IsMCPDependencyReference(Asset->GetPathName()))
    {
        ForbiddenHits.Add(MakeShared<FJsonValueString>(Asset->GetPathName()));
    }

    AuditObject->SetNumberField(TEXT("dependency_count"), DependencyValues.Num());
    AuditObject->SetNumberField(TEXT("forbidden_dependency_hit_count"), ForbiddenHits.Num());
    AuditObject->SetNumberField(TEXT("temp_dependency_hit_count"), TempHits.Num());
    AuditObject->SetBoolField(TEXT("has_forbidden_dependency"), ForbiddenHits.Num() > 0);
    AuditObject->SetBoolField(TEXT("has_temp_dependency"), TempHits.Num() > 0);
    AuditObject->SetArrayField(TEXT("dependencies"), DependencyValues);
    AuditObject->SetArrayField(TEXT("forbidden_dependency_hits"), ForbiddenHits);
    AuditObject->SetArrayField(TEXT("temp_dependency_hits"), TempHits);
    AuditObject->SetStringField(TEXT("dependency_scope"),TEXT("AssetRegistry default Package-category direct dependencies from on-disk package; not transitive or RAM closure. saved_file_scanned means ScanModifiedAssetFiles returned, not independent binary/cold validation."));return AuditObject;
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


struct FSpriteNiagaraPresetSettings
    {
        FString CanonicalPreset = TEXT("basic");
        double SpawnRate = 10.0;
        FVector Velocity = FVector(0.0, 0.0, 100.0);
        FString Alignment = TEXT("Automatic");
        FString FacingMode = TEXT("FaceCamera");
        FVector2D SubImageSize = FVector2D(1.0, 1.0);
        bool bSetFixedBounds = false;
        FVector FixedBoundsMin = FVector(-1000.0, -1000.0, -1000.0);
        FVector FixedBoundsMax = FVector(1000.0, 1000.0, 1000.0);
        bool bSetLocalSpace = false;
        bool bLocalSpace = false;
        bool bAddSpawnBoxLocation = false;
        FString SpawnBoxModulePath = TEXT("/Niagara/Modules/Spawn/Location/BoxLocation.BoxLocation");
        FVector SpawnBoxSize = FVector(100.0, 100.0, 100.0);
        FVector SpawnBoxOffset = FVector::ZeroVector;
        bool bAddWeatherDetailModules = false;
        double Drag = 0.0;
        double RotationalDrag = 0.0;
        FVector Gravity = FVector(0.0, 0.0, -980.0);
        double CurlNoiseStrength = 0.0;
        double CurlNoiseFrequency = 0.05;
        FVector CurlNoisePan = FVector::ZeroVector;
        FVector WindSpeed = FVector::ZeroVector;
        double WindSpeedScale = 1.0;
        double WindFrequency = 1.0;
        double WindScale = 0.5;
        FVector ColorScale = FVector(1.0, 1.0, 1.0);
        double AlphaScale = 1.0;
        double SpriteUniformScale = 1.0;
        bool bSetInitializeParticleDefaults = false;
        FVector2D SpriteSize = FVector2D(50.0, 50.0);
        double Lifetime = 5.0;
        TArray<FString> Limitations;
    };
enum class EStageCall:uint8{Create,Batch,Module,Initialize,Renderer,Bounds,Local};
struct FStagePlan{EStageCall Kind;FString Name;TSharedPtr<FJsonObject> Params;};
struct FRecipe{FSpriteNiagaraPresetSettings Settings;FString TargetPath,EmitterName,MaterialPath,MaterialSource,RequestedPreset,CreatedPath;bool Allow=false,Save=true,Wait=true,WeatherMaterial=false;double Timeout=10.0,Poll=0.1;TArray<FStagePlan> Plan;};

bool PrepareRecipe(FWork& W,const TSharedPtr<FJsonObject>& Params,FRecipe& Out)
{

    FString TargetSystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("target_system_path"), TargetSystemPath))
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("Missing 'target_system_path' parameter"));
    }

    bool bAllowNonTempPath = false;
    Params->TryGetBoolField(TEXT("allow_non_temp_path"), bAllowNonTempPath);

    bool bOverwriteExisting = false;
    Params->TryGetBoolField(TEXT("overwrite_existing"), bOverwriteExisting);
    if (bOverwriteExisting)
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("overwrite_existing is not supported for Niagara preset asset creation"));
    }

    FString EmitterName = TEXT("Emitter");
    Params->TryGetStringField(TEXT("emitter_name"), EmitterName);
    EmitterName.TrimStartAndEndInline();
    if (EmitterName.IsEmpty())
    {
        EmitterName = TEXT("Emitter");
    }
    FText InvalidEmitterNameReason;
    if (!FName(*EmitterName).IsValidObjectName(InvalidEmitterNameReason))
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),FString::Printf(
            TEXT("Invalid emitter_name '%s': %s"),
            *EmitterName,
            *InvalidEmitterNameReason.ToString()));
    }

    FString MaterialPath;
    Params->TryGetStringField(TEXT("material_path"), MaterialPath);
    MaterialPath.TrimStartAndEndInline();
    if (!MaterialPath.IsEmpty() && IsMCPDependencyReference(MaterialPath))
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),GetMCPDependencyReferenceError(TEXT("material_path"), MaterialPath));
    }
    bool bUseProjectWeatherMaterials = false;
    Params->TryGetBoolField(TEXT("use_project_weather_materials"), bUseProjectWeatherMaterials);
    FString MaterialSource = MaterialPath.IsEmpty() ? TEXT("none") : TEXT("caller");

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bWaitForCompletion = true;
    Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);

    double TimeoutSeconds = 10.0;
    Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);

    double PollIntervalSeconds = 0.1;
    Params->TryGetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);

    

    FString RequestedPreset = TEXT("basic");
    Params->TryGetStringField(TEXT("preset"), RequestedPreset);
    const FString PresetToken = NormalizeNiagaraEnumToken(RequestedPreset);

    FSpriteNiagaraPresetSettings Settings;
    if (PresetToken.IsEmpty() || PresetToken == TEXT("basic") || PresetToken == TEXT("default") || PresetToken == TEXT("basicsprite"))
    {
        Settings.CanonicalPreset = TEXT("basic");
        Settings.Limitations.Add(TEXT("Factory default SystemLocation spawns particles at the system origin; no volume distribution module is inserted yet."));
    }
    else if (PresetToken == TEXT("rain"))
    {
        Settings.CanonicalPreset = TEXT("rain");
        Settings.SpawnRate = 1200.0;
        Settings.Velocity = FVector(0.0, 0.0, -1800.0);
        Settings.Alignment = TEXT("VelocityAligned");
        Settings.FacingMode = TEXT("FaceCamera");
        Settings.bSetFixedBounds = true;
        Settings.FixedBoundsMin = FVector(-3000.0, -3000.0, -3000.0);
        Settings.FixedBoundsMax = FVector(3000.0, 3000.0, 1000.0);
        Settings.bAddSpawnBoxLocation = true;
        Settings.SpawnBoxSize = FVector(6000.0, 6000.0, 3000.0);
        Settings.SpawnBoxOffset = FVector(0.0, 0.0, 500.0);
        Settings.bAddWeatherDetailModules = true;
        Settings.Drag = 0.05;
        Settings.RotationalDrag = 0.0;
        Settings.Gravity = FVector(0.0, 0.0, -650.0);
        Settings.CurlNoiseStrength = 22.0;
        Settings.CurlNoiseFrequency = 0.01;
        Settings.CurlNoisePan = FVector(90.0, 20.0, -240.0);
        Settings.WindSpeed = FVector(180.0, 30.0, -40.0);
        Settings.WindSpeedScale = 0.35;
        Settings.WindFrequency = 0.15;
        Settings.WindScale = 0.20;
        Settings.ColorScale = FVector(0.78, 0.88, 1.0);
        Settings.AlphaScale = 0.58;
        Settings.SpriteUniformScale = 0.35;
        Settings.bSetInitializeParticleDefaults = true;
        Settings.SpriteSize = FVector2D(5.0, 120.0);
        Settings.Lifetime = 1.8;
        Settings.Limitations.Add(TEXT("Rain preset uses BoxLocation plus conservative force/noise/color/size/lifetime shaping; collision, camera-following controls, and material-specific streak textures remain separate authoring steps."));
    }
    else if (PresetToken == TEXT("snow"))
    {
        Settings.CanonicalPreset = TEXT("snow");
        Settings.SpawnRate = 450.0;
        Settings.Velocity = FVector(30.0, 0.0, -120.0);
        Settings.Alignment = TEXT("Automatic");
        Settings.FacingMode = TEXT("FaceCamera");
        Settings.bSetFixedBounds = true;
        Settings.FixedBoundsMin = FVector(-3000.0, -3000.0, -2000.0);
        Settings.FixedBoundsMax = FVector(3000.0, 3000.0, 2000.0);
        Settings.bAddSpawnBoxLocation = true;
        Settings.SpawnBoxSize = FVector(6000.0, 6000.0, 2500.0);
        Settings.SpawnBoxOffset = FVector(0.0, 0.0, 600.0);
        Settings.bAddWeatherDetailModules = true;
        Settings.Drag = 0.35;
        Settings.RotationalDrag = 0.20;
        Settings.Gravity = FVector(0.0, 0.0, -80.0);
        Settings.CurlNoiseStrength = 65.0;
        Settings.CurlNoiseFrequency = 0.008;
        Settings.CurlNoisePan = FVector(35.0, 20.0, -20.0);
        Settings.WindSpeed = FVector(80.0, 25.0, -20.0);
        Settings.WindSpeedScale = 0.45;
        Settings.WindFrequency = 0.25;
        Settings.WindScale = 0.55;
        Settings.ColorScale = FVector(1.0, 1.0, 1.0);
        Settings.AlphaScale = 0.85;
        Settings.SpriteUniformScale = 0.80;
        Settings.bSetInitializeParticleDefaults = true;
        Settings.SpriteSize = FVector2D(22.0, 22.0);
        Settings.Lifetime = 12.0;
        Settings.Limitations.Add(TEXT("Snow preset uses BoxLocation plus conservative drag/gravity/wind/noise/color/size/lifetime shaping; variation, camera-following controls, and material-specific flake textures remain separate authoring steps."));
    }
    else if (PresetToken == TEXT("dust"))
    {
        Settings.CanonicalPreset = TEXT("dust");
        Settings.SpawnRate = 80.0;
        Settings.Velocity = FVector(40.0, 0.0, 80.0);
        Settings.Alignment = TEXT("Automatic");
        Settings.FacingMode = TEXT("FaceCamera");
        Settings.bSetFixedBounds = true;
        Settings.FixedBoundsMin = FVector(-2000.0, -2000.0, -500.0);
        Settings.FixedBoundsMax = FVector(2000.0, 2000.0, 1500.0);
        Settings.Limitations.Add(TEXT("Dust preset is a factory sprite seed with gentle upward velocity; noise/color/lifetime shaping is not inserted yet."));
    }
    else if (PresetToken == TEXT("spark") || PresetToken == TEXT("sparks"))
    {
        Settings.CanonicalPreset = TEXT("sparks");
        Settings.SpawnRate = 140.0;
        Settings.Velocity = FVector(0.0, 0.0, 450.0);
        Settings.Alignment = TEXT("VelocityAligned");
        Settings.FacingMode = TEXT("FaceCamera");
        Settings.bSetFixedBounds = true;
        Settings.FixedBoundsMin = FVector(-800.0, -800.0, -200.0);
        Settings.FixedBoundsMax = FVector(800.0, 800.0, 1200.0);
        Settings.Limitations.Add(TEXT("Sparks preset tunes factory velocity and renderer only; burst timing, drag, gravity, and color-over-life are later authoring steps."));
    }
    else
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),FString::Printf(
            TEXT("Unsupported Niagara sprite preset '%s' (use basic, rain, snow, dust, or sparks)"),
            *RequestedPreset));
    }

    if (bUseProjectWeatherMaterials && MaterialPath.IsEmpty())
    {
        if (Settings.CanonicalPreset == TEXT("rain"))
        {
            MaterialPath = ProjectWeatherRainParticleMaterialPath;
            MaterialSource = TEXT("project_weather_default");
        }
        else if (Settings.CanonicalPreset == TEXT("snow"))
        {
            MaterialPath = ProjectWeatherSnowParticleMaterialPath;
            MaterialSource = TEXT("project_weather_default");
        }
        else
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("use_project_weather_materials is currently supported only for rain and snow presets"));
        }
    }

    Params->TryGetNumberField(TEXT("spawn_rate"), Settings.SpawnRate);
    if (Settings.SpawnRate < 0.0)
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("'spawn_rate' must be >= 0"));
    }

    const TArray<TSharedPtr<FJsonValue>>* VelocityArray = nullptr;
    if (Params->TryGetArrayField(TEXT("velocity"), VelocityArray) && !TryParseVector3FromJsonArray(VelocityArray, Settings.Velocity))
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("'velocity' must be [x, y, z] finite numbers"));
    }

    FString AlignmentOverride;
    if (Params->TryGetStringField(TEXT("alignment"), AlignmentOverride))
    {
        ENiagaraSpriteAlignment ParsedAlignment = ENiagaraSpriteAlignment::Automatic;
        if (!ParseNiagaraSpriteAlignmentToken(AlignmentOverride, ParsedAlignment))
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),FString::Printf(
                TEXT("Unsupported alignment: %s (Unaligned|VelocityAligned|CustomAlignment|Automatic)"),
                *AlignmentOverride));
        }
        Settings.Alignment = AlignmentOverride;
    }

    FString FacingModeOverride;
    if (Params->TryGetStringField(TEXT("facing_mode"), FacingModeOverride))
    {
        ENiagaraSpriteFacingMode ParsedFacingMode = ENiagaraSpriteFacingMode::Automatic;
        if (!ParseNiagaraSpriteFacingModeToken(FacingModeOverride, ParsedFacingMode))
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),FString::Printf(TEXT("Unsupported facing_mode: %s"), *FacingModeOverride));
        }
        Settings.FacingMode = FacingModeOverride;
    }

    const TArray<TSharedPtr<FJsonValue>>* SubImageArray = nullptr;
    if (Params->TryGetArrayField(TEXT("sub_image_size"), SubImageArray))
    {
        if (!TryParseVector2FromJsonArray(SubImageArray, Settings.SubImageSize))
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("'sub_image_size' must be [x, y] finite numbers"));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* SpriteSizeArray = nullptr;
    const bool bHasSpriteSizeOverride =
        Params->TryGetArrayField(TEXT("sprite_size"), SpriteSizeArray) ||
        Params->TryGetArrayField(TEXT("initialize_sprite_size"), SpriteSizeArray);
    if (bHasSpriteSizeOverride)
    {
        if (!TryParseVector2FromJsonArray(SpriteSizeArray, Settings.SpriteSize))
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("'sprite_size' must be [x, y] finite numbers"));
        }
        Settings.bSetInitializeParticleDefaults = true;
    }

    double LifetimeOverride = 0.0;
    if (Params->TryGetNumberField(TEXT("lifetime"), LifetimeOverride) ||
        Params->TryGetNumberField(TEXT("particle_lifetime"), LifetimeOverride))
    {
        Settings.Lifetime = LifetimeOverride;
        Settings.bSetInitializeParticleDefaults = true;
    }
    if (Settings.bSetInitializeParticleDefaults &&
        (Settings.SpriteSize.X <= 0.0 || Settings.SpriteSize.Y <= 0.0 || Settings.Lifetime <= 0.0))
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("sprite_size values and lifetime must be > 0 when InitializeParticle defaults are enabled"));
    }

    const TArray<TSharedPtr<FJsonValue>>* BoundsMinArray = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* BoundsMaxArray = nullptr;
    const bool bHasBoundsMin =
        Params->TryGetArrayField(TEXT("fixed_bounds_min"), BoundsMinArray) ||
        Params->TryGetArrayField(TEXT("bounds_min"), BoundsMinArray);
    const bool bHasBoundsMax =
        Params->TryGetArrayField(TEXT("fixed_bounds_max"), BoundsMaxArray) ||
        Params->TryGetArrayField(TEXT("bounds_max"), BoundsMaxArray);
    if (bHasBoundsMin || bHasBoundsMax)
    {
        if (!bHasBoundsMin || !bHasBoundsMax)
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("Both 'fixed_bounds_min' and 'fixed_bounds_max' are required when overriding fixed bounds"));
        }
        if (!TryParseVector3FromJsonArray(BoundsMinArray, Settings.FixedBoundsMin) ||
            !TryParseVector3FromJsonArray(BoundsMaxArray, Settings.FixedBoundsMax))
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("'fixed_bounds_min' and 'fixed_bounds_max' must be [x, y, z] finite numbers"));
        }
        Settings.bSetFixedBounds = true;
    }
    if (Settings.bSetFixedBounds &&
        (Settings.FixedBoundsMin.X > Settings.FixedBoundsMax.X ||
         Settings.FixedBoundsMin.Y > Settings.FixedBoundsMax.Y ||
         Settings.FixedBoundsMin.Z > Settings.FixedBoundsMax.Z))
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("fixed bounds min values must be <= max values"));
    }

    if (Params->TryGetBoolField(TEXT("local_space"), Settings.bLocalSpace))
    {
        Settings.bSetLocalSpace = true;
    }

    bool bSpawnBoxLocationOverride = false;
    const bool bHasSpawnBoxLocationOverride = Params->TryGetBoolField(TEXT("add_spawn_box_location"), bSpawnBoxLocationOverride);
    if (bHasSpawnBoxLocationOverride)
    {
        Settings.bAddSpawnBoxLocation = bSpawnBoxLocationOverride;
    }

    const TArray<TSharedPtr<FJsonValue>>* SpawnBoxSizeArray = nullptr;
    const bool bHasSpawnBoxSize =
        Params->TryGetArrayField(TEXT("spawn_box_size"), SpawnBoxSizeArray) ||
        Params->TryGetArrayField(TEXT("box_location_size"), SpawnBoxSizeArray);
    const TArray<TSharedPtr<FJsonValue>>* SpawnBoxOffsetArray = nullptr;
    const bool bHasSpawnBoxOffset =
        Params->TryGetArrayField(TEXT("spawn_box_offset"), SpawnBoxOffsetArray) ||
        Params->TryGetArrayField(TEXT("box_location_offset"), SpawnBoxOffsetArray);

    if (bHasSpawnBoxLocationOverride && !Settings.bAddSpawnBoxLocation && (bHasSpawnBoxSize || bHasSpawnBoxOffset))
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("spawn_box_size/spawn_box_offset cannot be supplied when add_spawn_box_location=false"));
    }
    if (bHasSpawnBoxSize)
    {
        if (!TryParseVector3FromJsonArray(SpawnBoxSizeArray, Settings.SpawnBoxSize))
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("'spawn_box_size' must be [x, y, z] finite numbers"));
        }
        if (!bHasSpawnBoxLocationOverride)
        {
            Settings.bAddSpawnBoxLocation = true;
        }
    }
    if (bHasSpawnBoxOffset)
    {
        if (!TryParseVector3FromJsonArray(SpawnBoxOffsetArray, Settings.SpawnBoxOffset))
        {
            return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("'spawn_box_offset' must be [x, y, z] finite numbers"));
        }
        if (!bHasSpawnBoxLocationOverride)
        {
            Settings.bAddSpawnBoxLocation = true;
        }
    }
    if (Settings.bAddSpawnBoxLocation &&
        (Settings.SpawnBoxSize.X <= 0.0 || Settings.SpawnBoxSize.Y <= 0.0 || Settings.SpawnBoxSize.Z <= 0.0))
    {
        return W.Fail(TEXT("niagara_sprite_preset_async_preflight_invalid"),TEXT("spawn_box_size values must be > 0 when add_spawn_box_location is enabled"));
    }

    bool bWeatherDetailModulesOverride = false;
    if (Params->TryGetBoolField(TEXT("add_weather_detail_modules"), bWeatherDetailModulesOverride) ||
        Params->TryGetBoolField(TEXT("add_detail_modules"), bWeatherDetailModulesOverride))
    {
        Settings.bAddWeatherDetailModules = bWeatherDetailModulesOverride;
    }



 FRecipe& Recipe=Out;Recipe.Settings=Settings;Recipe.TargetPath=TargetSystemPath;Recipe.EmitterName=EmitterName;Recipe.MaterialPath=MaterialPath;Recipe.MaterialSource=MaterialSource;Recipe.RequestedPreset=RequestedPreset;Recipe.Allow=bAllowNonTempPath;Recipe.Save=bSave;Recipe.Wait=bWaitForCompletion;Recipe.WeatherMaterial=bUseProjectWeatherMaterials;Recipe.Timeout=TimeoutSeconds;Recipe.Poll=PollIntervalSeconds;Recipe.CreatedPath=NormalizeNiagaraObjectPathForLoad(TargetSystemPath);
 for(double V:{Settings.SpawnRate,Settings.Lifetime,Settings.SpriteSize.X,Settings.SpriteSize.Y,Settings.Velocity.X,Settings.Velocity.Y,Settings.Velocity.Z,Settings.SubImageSize.X,Settings.SubImageSize.Y,Settings.FixedBoundsMin.X,Settings.FixedBoundsMin.Y,Settings.FixedBoundsMin.Z,Settings.FixedBoundsMax.X,Settings.FixedBoundsMax.Y,Settings.FixedBoundsMax.Z,Settings.SpawnBoxSize.X,Settings.SpawnBoxSize.Y,Settings.SpawnBoxSize.Z,Settings.SpawnBoxOffset.X,Settings.SpawnBoxOffset.Y,Settings.SpawnBoxOffset.Z})if(!FloatStorage(V))return W.Fail(TEXT("niagara_sprite_preset_async_storage_range"),TEXT("Preset values exceed finite float32 destination range"));
 auto Add=[&](EStageCall Kind,const FString& Name,TSharedPtr<FJsonObject> P){Recipe.Plan.Add({Kind,Name,P});};
 auto Common=[&](){auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("system_path"),Recipe.CreatedPath);P->SetNumberField(TEXT("emitter_index"),0);P->SetBoolField(TEXT("save"),false);P->SetBoolField(TEXT("request_compile"),false);P->SetBoolField(TEXT("allow_source_edit"),true);return P;};
 auto Create=MakeShared<FJsonObject>();Create->SetStringField(TEXT("target_system_path"),TargetSystemPath);Create->SetStringField(TEXT("emitter_name"),EmitterName);Create->SetStringField(TEXT("material_path"),MaterialPath);Create->SetBoolField(TEXT("allow_non_temp_path"),bAllowNonTempPath);Create->SetBoolField(TEXT("overwrite_existing"),false);Create->SetBoolField(TEXT("save"),false);Create->SetBoolField(TEXT("wait_for_completion"),false);Create->SetNumberField(TEXT("timeout_seconds"),TimeoutSeconds);Create->SetNumberField(TEXT("poll_interval_seconds"),PollIntervalSeconds);Add(EStageCall::Create,TEXT("create_basic_sprite_niagara_system"),Create);
 auto Edit=[](const FString& Module,const FString& Input,TSharedPtr<FJsonValue> Value){auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("module_name"),Module);P->SetStringField(TEXT("input_name"),Input);P->SetField(TEXT("value"),Value);P->SetNumberField(TEXT("emitter_index"),0);return MakeShared<FJsonValueObject>(P);};
 auto Batch=[&](const FString& Name,const FString& Operation,const TArray<TSharedPtr<FJsonValue>>& Edits){auto P=Common();P->SetStringField(TEXT("operation"),Operation);P->SetBoolField(TEXT("overwrite_existing"),Operation.Equals(TEXT("upsert")));P->SetBoolField(TEXT("continue_on_error"),false);P->SetArrayField(TEXT("edits"),Edits);Add(EStageCall::Batch,Name,P);};
 auto Module=[&](const FString& Name,const FString& Path,const FString& Usage){auto P=Common();P->SetStringField(TEXT("target_system_path"),Recipe.CreatedPath);P->SetStringField(TEXT("module_script_path"),Path);P->SetStringField(TEXT("target_usage"),Usage);P->SetNumberField(TEXT("target_emitter_index"),0);P->SetBoolField(TEXT("skip_if_duplicate"),true);Add(EStageCall::Module,Name,P);};
 Batch(TEXT("set_preset_module_inputs"),TEXT("set_existing"),{Edit(TEXT("SpawnRate"),TEXT("SpawnRate"),MakeShared<FJsonValueNumber>(Settings.SpawnRate)),Edit(TEXT("AddVelocity"),TEXT("Velocity"),NumberArrayToJson({Settings.Velocity.X,Settings.Velocity.Y,Settings.Velocity.Z}))});
 if(Settings.bAddSpawnBoxLocation){Module(TEXT("add_preset_spawn_box_location"),Settings.SpawnBoxModulePath,TEXT("ParticleSpawnScriptInterpolated"));Batch(TEXT("set_preset_spawn_box_location_inputs"),TEXT("upsert"),{Edit(TEXT("BoxLocation"),TEXT("Box Size"),NumberArrayToJson({Settings.SpawnBoxSize.X,Settings.SpawnBoxSize.Y,Settings.SpawnBoxSize.Z})),Edit(TEXT("BoxLocation"),TEXT("Box Offset"),NumberArrayToJson({Settings.SpawnBoxOffset.X,Settings.SpawnBoxOffset.Y,Settings.SpawnBoxOffset.Z}))});}
 if(Settings.bAddWeatherDetailModules){
 struct FWeather{FString Token,Name,Path;TArray<TSharedPtr<FJsonValue>> Values;};TArray<FWeather> Weather;
 Weather.Add({TEXT("drag"),TEXT("Drag"),TEXT("/Niagara/Modules/Update/Forces/Drag.Drag"),{Edit(TEXT("Drag"),TEXT("Drag"),MakeShared<FJsonValueNumber>(Settings.Drag)),Edit(TEXT("Drag"),TEXT("Rotational Drag"),MakeShared<FJsonValueNumber>(Settings.RotationalDrag))}});
 Weather.Add({TEXT("gravity_force"),TEXT("GravityForce"),TEXT("/Niagara/Modules/Update/Forces/GravityForce.GravityForce"),{Edit(TEXT("GravityForce"),TEXT("Gravity"),NumberArrayToJson({Settings.Gravity.X,Settings.Gravity.Y,Settings.Gravity.Z}))}});
 Weather.Add({TEXT("curl_noise_force"),TEXT("CurlNoiseForce"),TEXT("/Niagara/Modules/Update/Forces/CurlNoiseForce.CurlNoiseForce"),{Edit(TEXT("CurlNoiseForce"),TEXT("Noise Strength"),MakeShared<FJsonValueNumber>(Settings.CurlNoiseStrength)),Edit(TEXT("CurlNoiseForce"),TEXT("Noise Frequency"),MakeShared<FJsonValueNumber>(Settings.CurlNoiseFrequency)),Edit(TEXT("CurlNoiseForce"),TEXT("Pan Noise Field"),NumberArrayToJson({Settings.CurlNoisePan.X,Settings.CurlNoisePan.Y,Settings.CurlNoisePan.Z}))}});
 Weather.Add({TEXT("wind_force"),TEXT("WindForce"),TEXT("/Niagara/Modules/Update/Forces/WindForce.WindForce"),{Edit(TEXT("WindForce"),TEXT("Wind Speed"),NumberArrayToJson({Settings.WindSpeed.X,Settings.WindSpeed.Y,Settings.WindSpeed.Z})),Edit(TEXT("WindForce"),TEXT("Wind Speed Scale"),MakeShared<FJsonValueNumber>(Settings.WindSpeedScale)),Edit(TEXT("WindForce"),TEXT("Frequency"),MakeShared<FJsonValueNumber>(Settings.WindFrequency)),Edit(TEXT("WindForce"),TEXT("Scale"),MakeShared<FJsonValueNumber>(Settings.WindScale))}});
 Weather.Add({TEXT("scale_color"),TEXT("ScaleColor"),TEXT("/Niagara/Modules/Update/Color/ScaleColor.ScaleColor"),{Edit(TEXT("ScaleColor"),TEXT("Scale RGB"),NumberArrayToJson({Settings.ColorScale.X,Settings.ColorScale.Y,Settings.ColorScale.Z})),Edit(TEXT("ScaleColor"),TEXT("Scale Alpha"),MakeShared<FJsonValueNumber>(Settings.AlphaScale))}});
 Weather.Add({TEXT("scale_sprite_size"),TEXT("ScaleSpriteSize"),TEXT("/Niagara/Modules/Update/Size/ScaleSpriteSize.ScaleSpriteSize"),{Edit(TEXT("ScaleSpriteSize"),TEXT("Uniform Curve Scale"),MakeShared<FJsonValueNumber>(Settings.SpriteUniformScale))}});
 for(const auto& X:Weather){Module(TEXT("add_weather_detail_")+X.Token,X.Path,TEXT("ParticleUpdateScript"));Batch(TEXT("set_weather_detail_")+X.Token+TEXT("_inputs"),TEXT("upsert"),X.Values);}}
 if(Settings.bSetInitializeParticleDefaults)Add(EStageCall::Initialize,TEXT("set_preset_initialize_particle_defaults"),Common());
 auto Renderer=Common();Renderer->SetStringField(TEXT("alignment"),Settings.Alignment);Renderer->SetStringField(TEXT("facing_mode"),Settings.FacingMode);Renderer->SetArrayField(TEXT("sub_image_size"),NumberArrayToJsonValues({Settings.SubImageSize.X,Settings.SubImageSize.Y}));Add(EStageCall::Renderer,TEXT("set_preset_sprite_renderer_settings"),Renderer);
 if(Settings.bSetFixedBounds){auto P=Common();P->SetStringField(TEXT("mode"),TEXT("Fixed"));P->SetArrayField(TEXT("min"),NumberArrayToJsonValues({Settings.FixedBoundsMin.X,Settings.FixedBoundsMin.Y,Settings.FixedBoundsMin.Z}));P->SetArrayField(TEXT("max"),NumberArrayToJsonValues({Settings.FixedBoundsMax.X,Settings.FixedBoundsMax.Y,Settings.FixedBoundsMax.Z}));Add(EStageCall::Bounds,TEXT("set_preset_fixed_bounds"),P);}
 if(Settings.bSetLocalSpace){auto P=Common();P->SetBoolField(TEXT("value"),Settings.bLocalSpace);Add(EStageCall::Local,TEXT("set_preset_local_space"),P);}return true;

}

bool VerifySettings(FWork& W,const FRecipe& Recipe)
{
 auto* System=W.Target.Get();const auto& Settings=Recipe.Settings;const FString& MaterialPath=Recipe.MaterialPath;
        if(!W.Admit(TEXT("preset_typed_readback"))||!VerifyRI())return false;
        auto* D=System->GetEmitterHandles()[0].GetEmitterData();if(!D)return W.Fail(TEXT("niagara_sprite_preset_emitter_missing"),TEXT("Selected emitter data disappeared"));
        if(Settings.bSetLocalSpace&&D->bLocalSpace!=Settings.bLocalSpace)return W.Fail(TEXT("niagara_sprite_preset_local_space_readback_failed"),TEXT("Actual local-space differs"));
        if(Settings.bSetFixedBounds&&(D->CalculateBoundsMode!=ENiagaraEmitterCalculateBoundMode::Fixed||D->FixedBounds.Min!=Settings.FixedBoundsMin||D->FixedBounds.Max!=Settings.FixedBoundsMax))return W.Fail(TEXT("niagara_sprite_preset_bounds_readback_failed"),TEXT("Actual finite fixed bounds differ"));
        ENiagaraSpriteAlignment Alignment;ENiagaraSpriteFacingMode Facing;if(!ParseNiagaraSpriteAlignmentToken(Settings.Alignment,Alignment)||!ParseNiagaraSpriteFacingModeToken(Settings.FacingMode,Facing))return false;
        int32 Count=0;for(UNiagaraRendererProperties* P:D->GetRenderers())if(auto* S=Cast<UNiagaraSpriteRendererProperties>(P)){++Count;if(S->Alignment!=Alignment||S->FacingMode!=Facing||S->SubImageSize!=Settings.SubImageSize)return W.Fail(TEXT("niagara_sprite_preset_renderer_readback_failed"),TEXT("Actual typed sprite alignment/facing/subimage differs"));if(!MaterialPath.IsEmpty()&&(!S->Material||!S->Material->GetPathName().Equals(NormalizeNiagaraObjectPathForLoad(MaterialPath),ESearchCase::CaseSensitive)))return W.Fail(TEXT("niagara_sprite_preset_material_readback_failed"),TEXT("Actual material path differs"));}
        return Count>0||W.Fail(TEXT("niagara_sprite_preset_renderer_missing"),TEXT("No actual sprite renderer"));
}


TSharedPtr<FJsonObject> BuildSuccess(FWork& W,const FRecipe& Recipe,const TArray<TSharedPtr<FJsonValue>>& StageResults,const TSharedPtr<FJsonObject>& WaitObject,const FString& SavedFilename,const TSharedPtr<FJsonObject>& DependencyAudit)
{
 auto* System=W.Target.Get();const auto& Settings=Recipe.Settings;const FString& TargetSystemPath=Recipe.TargetPath;const FString& RequestedPreset=Recipe.RequestedPreset;const FString& EmitterName=Recipe.EmitterName;const FString& MaterialPath=Recipe.MaterialPath;const FString& MaterialSource=Recipe.MaterialSource;const bool bUseProjectWeatherMaterials=Recipe.WeatherMaterial,bAllowNonTempPath=Recipe.Allow,bSave=Recipe.Save,bSaved=W.R.bSaved;
 int32 SpriteRendererCount=0;for(const auto& H:System->GetEmitterHandles())if(auto* D=H.GetEmitterData())for(const auto* R:D->GetRenderers())if(Cast<UNiagaraSpriteRendererProperties>(R))++SpriteRendererCount;
    TSharedPtr<FJsonObject> AppliedSettings = MakeShared<FJsonObject>();
    AppliedSettings->SetNumberField(TEXT("spawn_rate"), Settings.SpawnRate);
    AppliedSettings->SetArrayField(TEXT("velocity"), NumberArrayToJsonValues({ Settings.Velocity.X, Settings.Velocity.Y, Settings.Velocity.Z }));
    AppliedSettings->SetStringField(TEXT("material_path"), MaterialPath);
    AppliedSettings->SetStringField(TEXT("material_source"), MaterialSource);
    AppliedSettings->SetBoolField(TEXT("use_project_weather_materials"), bUseProjectWeatherMaterials);
    AppliedSettings->SetStringField(TEXT("alignment"), Settings.Alignment);
    AppliedSettings->SetStringField(TEXT("facing_mode"), Settings.FacingMode);
    AppliedSettings->SetArrayField(TEXT("sub_image_size"), NumberArrayToJsonValues({ Settings.SubImageSize.X, Settings.SubImageSize.Y }));
    AppliedSettings->SetBoolField(TEXT("initialize_particle_defaults_enabled"), Settings.bSetInitializeParticleDefaults);
    if (Settings.bSetInitializeParticleDefaults)
    {
        AppliedSettings->SetArrayField(TEXT("sprite_size"), NumberArrayToJsonValues({ Settings.SpriteSize.X, Settings.SpriteSize.Y }));
        AppliedSettings->SetNumberField(TEXT("lifetime"), Settings.Lifetime);
    }
    AppliedSettings->SetBoolField(TEXT("fixed_bounds_enabled"), Settings.bSetFixedBounds);
    if (Settings.bSetFixedBounds)
    {
        AppliedSettings->SetArrayField(TEXT("fixed_bounds_min"), NumberArrayToJsonValues({
            Settings.FixedBoundsMin.X,
            Settings.FixedBoundsMin.Y,
            Settings.FixedBoundsMin.Z
        }));
        AppliedSettings->SetArrayField(TEXT("fixed_bounds_max"), NumberArrayToJsonValues({
            Settings.FixedBoundsMax.X,
            Settings.FixedBoundsMax.Y,
            Settings.FixedBoundsMax.Z
        }));
    }
    AppliedSettings->SetBoolField(TEXT("local_space_set"), Settings.bSetLocalSpace);
    AppliedSettings->SetBoolField(TEXT("local_space"), Settings.bLocalSpace);
    AppliedSettings->SetBoolField(TEXT("spawn_box_location_enabled"), Settings.bAddSpawnBoxLocation);
    if (Settings.bAddSpawnBoxLocation)
    {
        AppliedSettings->SetStringField(TEXT("spawn_box_module_path"), Settings.SpawnBoxModulePath);
        AppliedSettings->SetArrayField(TEXT("spawn_box_size"), NumberArrayToJsonValues({
            Settings.SpawnBoxSize.X,
            Settings.SpawnBoxSize.Y,
            Settings.SpawnBoxSize.Z
        }));
        AppliedSettings->SetArrayField(TEXT("spawn_box_offset"), NumberArrayToJsonValues({
            Settings.SpawnBoxOffset.X,
            Settings.SpawnBoxOffset.Y,
            Settings.SpawnBoxOffset.Z
        }));
    }
    AppliedSettings->SetBoolField(TEXT("weather_detail_modules_enabled"), Settings.bAddWeatherDetailModules);
    if (Settings.bAddWeatherDetailModules)
    {
        AppliedSettings->SetNumberField(TEXT("drag"), Settings.Drag);
        AppliedSettings->SetNumberField(TEXT("rotational_drag"), Settings.RotationalDrag);
        AppliedSettings->SetArrayField(TEXT("gravity"), NumberArrayToJsonValues({
            Settings.Gravity.X,
            Settings.Gravity.Y,
            Settings.Gravity.Z
        }));
        AppliedSettings->SetNumberField(TEXT("curl_noise_strength"), Settings.CurlNoiseStrength);
        AppliedSettings->SetNumberField(TEXT("curl_noise_frequency"), Settings.CurlNoiseFrequency);
        AppliedSettings->SetArrayField(TEXT("curl_noise_pan"), NumberArrayToJsonValues({
            Settings.CurlNoisePan.X,
            Settings.CurlNoisePan.Y,
            Settings.CurlNoisePan.Z
        }));
        AppliedSettings->SetArrayField(TEXT("wind_speed"), NumberArrayToJsonValues({
            Settings.WindSpeed.X,
            Settings.WindSpeed.Y,
            Settings.WindSpeed.Z
        }));
        AppliedSettings->SetNumberField(TEXT("wind_speed_scale"), Settings.WindSpeedScale);
        AppliedSettings->SetNumberField(TEXT("wind_frequency"), Settings.WindFrequency);
        AppliedSettings->SetNumberField(TEXT("wind_scale"), Settings.WindScale);
        AppliedSettings->SetArrayField(TEXT("color_scale"), NumberArrayToJsonValues({
            Settings.ColorScale.X,
            Settings.ColorScale.Y,
            Settings.ColorScale.Z
        }));
        AppliedSettings->SetNumberField(TEXT("alpha_scale"), Settings.AlphaScale);
        AppliedSettings->SetNumberField(TEXT("sprite_uniform_scale"), Settings.SpriteUniformScale);
    }

    const int32 ParentReferenceCount = CountEmitterParentReferences(System);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("command"), TEXT("create_sprite_niagara_preset_system"));
    Result->SetStringField(TEXT("requested_target_path"), TargetSystemPath);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("requested_preset"), RequestedPreset);
    Result->SetStringField(TEXT("preset"), Settings.CanonicalPreset);
    Result->SetStringField(TEXT("requested_emitter_name"), EmitterName);
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("sprite_renderer_count"), SpriteRendererCount);
    Result->SetStringField(TEXT("material_path"), MaterialPath);
    Result->SetStringField(TEXT("material_source"), MaterialSource);
    Result->SetBoolField(TEXT("use_project_weather_materials"), bUseProjectWeatherMaterials);
    Result->SetObjectField(TEXT("applied_settings"), AppliedSettings);
    Result->SetArrayField(TEXT("limitations"), StringArrayToJsonValues(Settings.Limitations));
    Result->SetNumberField(TEXT("emitter_parent_reference_count"), ParentReferenceCount);
    Result->SetBoolField(TEXT("has_emitter_parent_references"), ParentReferenceCount > 0);
    Result->SetBoolField(TEXT("compile_requested"), true);
    Result->SetObjectField(TEXT("compile_wait"), WaitObject);
    Result->SetBoolField(TEXT("save_requested"), bSave);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("saved_filename"), SavedFilename);
    Result->SetBoolField(TEXT("asset_registry_registered"), true);
    Result->SetBoolField(TEXT("allow_non_temp_path"), bAllowNonTempPath);
    Result->SetStringField(TEXT("write_scope"), TEXT("generated_temp_niagara_preset_creation"));
    Result->SetObjectField(TEXT("dependency_audit"), DependencyAudit);
    Result->SetArrayField(TEXT("stage_results"), StageResults);
    return Result;
}
struct FExecutionScope
{
 TArray<TStrongObjectPtr<UObject>> Pins;TArray<TStrongObjectPtr<UObject>>* Previous=nullptr;
 explicit FExecutionScope(FWork& W):Previous(ExecutionPins){ExecutionPins=&Pins;if(Live(W.World.Get()))Pins.Emplace(W.World.Get());for(const auto& I:W.Objects){if(auto* O=I->Object.Get())if(Live(O))Pins.Emplace(O);}}
 ~FExecutionScope(){ExecutionPins=Previous;}
};
} // private helpers
namespace {TSharedPtr<FJobState> Pending;FString LastTerminal;}
struct FJobState:public TSharedFromThis<FJobState>
{
 enum class EPhase:uint8{Prepare,Stages,WaitFactory,WaitMiddle,FinalRequest,WaitFinal,Verify,Save,Audit,Final,Terminal};
 FWork Work{true};FRecipe Recipe;TSharedPtr<FJsonObject> Requested;TStrongObjectPtr<UCubelessNiagaraSpritePresetJob> Job{nullptr};EPhase Phase=EPhase::Prepare;FTSTicker::FDelegateHandle Ticker;
 FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits),LastJson,SavedFilename,CreatedPath,WaitReason;int32 Revision=0,StageIndex=0;bool Executing=false,Prepared=false,FactoryCreated=false,FinalRequested=false,FinalIdle=false,SnapshotCaptured=false,Changed=false;
 double Started=FPlatformTime::Seconds(),WaitStarted=0,NextPoll=0,FinalWaitStarted=-1;int32 WaitBudgetStage=INDEX_NONE;double StageWaitStarted=0;TArray<FRIExpected> Expected;TArray<TSharedPtr<FJsonValue>> StageResults;TSharedPtr<FJsonObject> AuditResult;TSharedRef<FJsonObject> FactoryWait=MakeShared<FJsonObject>(),MiddleWait=MakeShared<FJsonObject>(),FinalWait=MakeShared<FJsonObject>();
 FJobState(){Job.Reset(NewObject<UCubelessNiagaraSpritePresetJob>(GetTransientPackage(),NAME_None,RF_Transient));Work.R.bCompileRequested=true;Work.R.bSaveRequested=true;Work.R.Data->SetBoolField(TEXT("compiler_cancel_attempted"),false);Work.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);}
 void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
 TSharedPtr<FJsonObject> FailureLeaf()
 {
  auto L=MakeShared<FJsonObject>();L->SetBoolField(TEXT("success"),false);L->SetStringField(TEXT("command"),TEXT("create_sprite_niagara_preset_system"));L->SetStringField(TEXT("requested_target_path"),Recipe.TargetPath);L->SetStringField(TEXT("requested_preset"),Recipe.RequestedPreset);L->SetStringField(TEXT("preset"),Recipe.Settings.CanonicalPreset);L->SetStringField(TEXT("failed_stage"),Work.FailedStage);L->SetStringField(TEXT("created_system_path"),CreatedPath);L->SetStringField(TEXT("error"),Work.R.Error);L->SetStringField(TEXT("error_code"),Work.R.ErrorCode);L->SetBoolField(TEXT("saved"),Work.R.bSaved);L->SetBoolField(TEXT("created_asset_cleanup_attempted"),false);L->SetBoolField(TEXT("created_asset_cleanup_deleted"),false);const bool Deferred=!CreatedPath.IsEmpty()&&IsTempGeneratedNiagaraPath(CreatedPath);L->SetBoolField(TEXT("created_asset_cleanup_deferred"),Deferred);if(Deferred){L->SetStringField(TEXT("created_asset_cleanup_path"),PackagePathFromObjectPath(CreatedPath));L->SetStringField(TEXT("created_asset_cleanup_deferred_reason"),TEXT("Live deletion of freshly loaded generated Niagara systems is deferred to cleanup_generated_niagara_assets_offline after editor close"));L->SetStringField(TEXT("created_asset_cleanup_tool"),TEXT("cleanup_generated_niagara_assets_offline"));}L->SetArrayField(TEXT("stage_results"),StageResults);L->SetStringField(TEXT("write_scope"),TEXT("generated_temp_niagara_preset_creation"));return L;
 }
 void Publish()
 {
  const bool IsTerminal=Phase==EPhase::Terminal;auto& R=Work.R;R.Data->SetStringField(TEXT("native_job_id"),Id);R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);R.Data->SetBoolField(TEXT("native_job_terminal"),IsTerminal);R.Data->SetBoolField(TEXT("operation_complete"),IsTerminal);R.Data->SetNumberField(TEXT("job_elapsed_seconds"),FPlatformTime::Seconds()-Started);R.Data->SetNumberField(TEXT("completed_stage_count"),StageIndex);R.Data->SetArrayField(TEXT("stage_results"),StageResults);R.Data->SetObjectField(TEXT("factory_compile_wait"),FactoryWait);R.Data->SetObjectField(TEXT("interstage_compile_wait"),MiddleWait);R.Data->SetObjectField(TEXT("final_compile_wait"),FinalWait);R.Data->SetArrayField(TEXT("actual_save_prefix"),Work.SaveRows);R.Data->SetArrayField(TEXT("sdk_calls"),Work.Calls);R.Data->SetObjectField(TEXT("sdk_call_counts"),Work.Counts);R.Data->SetBoolField(TEXT("compiler_may_continue_after_job"),(R.bCompileAttempted||FactoryCreated||FinalRequested)&&(!FinalIdle||!R.bSuccess));R.Data->SetBoolField(TEXT("compiler_cancel_attempted"),false);R.Data->SetBoolField(TEXT("explicit_poll_pump_flush_sleep_invoked"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("semantic_restore_available"),false);R.Data->SetBoolField(TEXT("full_migration_complete"),false);R.Data->SetBoolField(TEXT("requested_target_may_remain"),R.bMutationAttempted);R.Data->SetBoolField(TEXT("live_failure_observation_skipped"),!R.Error.IsEmpty());R.Data->SetStringField(TEXT("requested_object"),Work.RequestedObject);R.Data->SetStringField(TEXT("requested_package"),Work.RequestedPackage);R.Data->SetStringField(TEXT("created_path_scope"),TEXT("Captured child creation receipt; live exact binding is verified separately, never inferred on unsafe failure"));R.Data->SetBoolField(TEXT("created_target_identity_verified"),FactoryCreated);R.Data->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("OR of actual SDK true returns before health checks; not full-operation or cold persistence proof; dirty is not cleared"));R.Data->SetStringField(TEXT("compile_scope"),TEXT("Factory SDK may implicitly request compilation and factory wait remains strict. After a successful owned edit, interstage/pre-final authoring waits require no active VM/GPU compile but may retain a deferred request. GU final RequestCompile is separate; requested final completion/verification/save remain strict. wait=False skips only final idle wait. SDK PreSave can synchronously wait inside SavePackage."));
  auto Session=MakeShared<FJsonObject>();Session->SetBoolField(TEXT("mutation_attempted"),R.bMutationAttempted);Session->SetBoolField(TEXT("compile_attempted"),R.bCompileAttempted);Session->SetBoolField(TEXT("saved"),R.bSaved);Session->SetArrayField(TEXT("stage_results"),StageResults);R.Data->SetObjectField(TEXT("session_execution"),Session);if(!R.Error.IsEmpty())R.Data->SetObjectField(TEXT("legacy_result"),FailureLeaf());else if(!IsTerminal||!R.Data->HasField(TEXT("legacy_result"))){auto L=FailureLeaf();L->RemoveField(TEXT("error"));L->RemoveField(TEXT("error_code"));L->SetStringField(TEXT("failed_stage"),FString());L->SetBoolField(TEXT("pending"),!IsTerminal);R.Data->SetObjectField(TEXT("legacy_result"),L);}
  LastJson=Operations::EncodeVariableResult(R,TEXT("create_sprite_niagara_preset_system"));
  if(Safe()&&Live(Job.Get())){Job->SnapshotJson=LastJson;Job->bIsTerminal=IsTerminal;}
 }
 void Terminal(bool Success)
 {
  if(Phase==EPhase::Terminal)return;Unhook();auto& R=Work.R;if(Success&&R.Error.IsEmpty()){R.bSuccess=true;R.bPostconditionPassed=true;R.bChangeStateKnown=true;R.bChanged=Changed;R.Stage=TEXT("complete");}else{if(R.Error.IsEmpty())Work.Fail(TEXT("niagara_sprite_preset_async_failed"),TEXT("Preset continuation failed; actual partial prefix retained"));R.bSuccess=false;R.bPostconditionPassed=false;if(R.bMutationAttempted)R.bChangeStateKnown=false;if(!Work.FailedStage.IsEmpty())R.Stage=Work.FailedStage;}Phase=EPhase::Terminal;Publish();LastTerminal=LastJson;if(Pending.Get()==this)Pending.Reset();
 }
 void CancelPlain(const FString& Reason){if(Phase==EPhase::Terminal)return;Work.Cancelled=true;Work.CancelText=Reason;Unhook();if(!Executing){Work.Fail(TEXT("niagara_sprite_preset_async_cancelled"),Reason);Terminal(false);}}
 // Only a successfully returned non-factory owned stage can leave an edit request deferred.
 // Factory and final completion still require the strict SDK outstanding predicate to clear.
 bool MayAwaitAuthoringQuiescence()const
 {
  return FactoryCreated&&!FinalRequested&&StageIndex>1&&StageIndex<=Recipe.Plan.Num()&&StageResults.Num()==StageIndex;
 }
 bool ObserveIdle(bool& Idle,bool AuthoringQuiescence=false)
 {
  Idle=false;if(!Work.SDK(TEXT("ObserveOwnedCompileIdle")))return false;
  auto* S=Work.Target.Get();
  const bool InitialOutstanding=S->HasOutstandingCompilationRequests(true),InitialActive=S->HasActiveCompilations(),InitialNeedsRequest=S->NeedsRequestCompile();
  auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("outstanding_compilation_requests_after"),InitialOutstanding);O->SetBoolField(TEXT("active_compilations_after"),InitialActive);O->SetBoolField(TEXT("needs_request_compile_after"),InitialNeedsRequest);O->SetBoolField(TEXT("SDK_returned"),true);O->SetBoolField(TEXT("authoring_quiescence_requested"),AuthoringQuiescence);Work.R.Data->SetObjectField(TEXT("last_compile_point"),O);
  if(!Work.After(TEXT("ObserveOwnedCompileIdle")))return false;
  if(AuthoringQuiescence&&!MayAwaitAuthoringQuiescence())return Work.Fail(TEXT("niagara_sprite_preset_authoring_wait_unavailable"),TEXT("Deferred-request authoring wait requires an already successful owned edit stage before final RequestCompile."));
  // The existing anchor closes the exact emitter/version; never retain its data across a callback.
  if(!Work.AnchorCaptured||!Work.AnchorData)return Work.Fail(TEXT("niagara_sprite_preset_gpu_compile_role_unavailable"),TEXT("Created emitter anchor is required for independent GPU compilation observation."));
  if(!Work.SDK(TEXT("ObserveOwnedGPUCompileScript")))return false;
  // Public getter may resolve a TObjectPtr: preserve it as an SDK boundary, not a pure read.
  TStrongObjectPtr<UNiagaraScript> GPU(Work.AnchorData->GetGPUComputeScript());
  const bool HasGPU=GPU.Get()!=nullptr;O->SetBoolField(TEXT("gpu_compute_script_present"),HasGPU);O->SetBoolField(TEXT("gpu_compute_script_returned"),true);
  if(HasGPU&&!Live(GPU.Get()))return Work.Fail(TEXT("niagara_sprite_preset_gpu_compile_role_unavailable"),TEXT("Returned GPU script is unavailable; no later compilation observation."));
  if(!Work.After(TEXT("ObserveOwnedGPUCompileScript")))return false;
  auto GPUCurrent=[&]()
  {
   if(!Work.SDK(TEXT("ReobserveOwnedGPUCompileScript")))return false;
   TStrongObjectPtr<UNiagaraScript> FreshGPU(Work.AnchorData->GetGPUComputeScript());
   const bool SameBinding=FreshGPU.Get()==GPU.Get();
   if(!Work.After(TEXT("ReobserveOwnedGPUCompileScript")))return false;
   if(!SameBinding||(FreshGPU.Get()&&!Live(FreshGPU.Get())))return Work.Fail(TEXT("niagara_sprite_preset_gpu_compile_role_changed"),TEXT("Public GPU script getter returned a changed or unavailable nullable binding; no replacement adopted."));
   return true;
  };
  if(!GPUCurrent())return false;
  if(HasGPU&&!Work.Pin(GPU.Get()))return false;
  if(!Work.Admit(TEXT("before_owned_gpu_compilation_observation")))return false;
  if(!GPUCurrent())return false;
  bool GPUActive=false;const FNiagaraShaderScript* ObservedShader=nullptr;
  if(HasGPU)
  {
   if(!GPUCurrent())return false;
   if(!Work.SDK(TEXT("ObserveOwnedGPUShaderCompilation")))return false;
   // These public GT accessors are callback-free in the installed SDK. The non-UObject
   // shader pointer is used immediately and is never dereferenced beyond Work.After.
   const FNiagaraShaderScript* Shader=GPU->GetRenderThreadScript();ObservedShader=Shader;
   GPUActive=Shader&&!Shader->IsCompilationFinished();
   O->SetBoolField(TEXT("gpu_shader_resource_present"),Shader!=nullptr);O->SetBoolField(TEXT("gpu_compilation_in_progress_after"),GPUActive);O->SetBoolField(TEXT("gpu_compilation_observation_returned"),true);
   if(!Work.After(TEXT("ObserveOwnedGPUShaderCompilation")))return false;
   if(!GPUCurrent())return false;
   if(GPU->GetRenderThreadScript()!=Shader)return Work.Fail(TEXT("niagara_sprite_preset_gpu_compile_role_changed"),TEXT("GPU script or shader-resource binding changed across observation; no stale resource read or replacement adopted."));
  }
  else{O->SetBoolField(TEXT("gpu_shader_resource_present"),false);O->SetBoolField(TEXT("gpu_compilation_in_progress_after"),false);O->SetBoolField(TEXT("gpu_compilation_observation_returned"),true);}
  // HasOutstanding(true) also invokes the public GPU getter internally. Treat the
  // final requery as an SDK boundary too; do not assume implicit resolution is pure.
  if(!Work.SDK(TEXT("ReobserveOwnedCompileIdle")))return false;
  S=Work.Target.Get();bool NeedsRequest=S->NeedsRequestCompile(),Active=S->HasActiveCompilations();const bool Outstanding=S->HasOutstandingCompilationRequests(true);
  O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);O->SetBoolField(TEXT("active_compilations_after"),Active);O->SetBoolField(TEXT("needs_request_compile_after"),NeedsRequest);
  if(!Work.After(TEXT("ReobserveOwnedCompileIdle")))return false;
  if(!GPUCurrent())return false;
  if(HasGPU)
  {
   const FNiagaraShaderScript* FreshShader=GPU->GetRenderThreadScript();
   if(FreshShader!=ObservedShader)return Work.Fail(TEXT("niagara_sprite_preset_gpu_compile_role_changed"),TEXT("Shader-resource binding changed across the final status requery; no stale resource read."));
   GPUActive=FreshShader&&!FreshShader->IsCompilationFinished();
   O->SetBoolField(TEXT("gpu_compilation_in_progress_after"),GPUActive);
  }
  // These two public inline scalar getters have no handle resolution/callback.
  S=Work.Target.Get();NeedsRequest=S->NeedsRequestCompile();Active=S->HasActiveCompilations();
  const bool StrictIdle=!NeedsRequest&&!Outstanding&&!Active&&!GPUActive;
  const bool Quiescent=!Active&&!GPUActive&&(!Outstanding||NeedsRequest);
  O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);O->SetBoolField(TEXT("active_compilations_after"),Active);O->SetBoolField(TEXT("needs_request_compile_after"),NeedsRequest);O->SetBoolField(TEXT("strict_compile_idle_observed"),StrictIdle);O->SetBoolField(TEXT("authoring_quiescent_observed"),Quiescent);O->SetBoolField(TEXT("deferred_request_allowed_for_authoring"),AuthoringQuiescence&&Quiescent&&NeedsRequest);O->SetStringField(TEXT("observation_scope"),TEXT("SDK pending-request, active VM and independent GPU shader state; pending request is not proof of dirty cause or completed compilation."));
  Idle=AuthoringQuiescence?Quiescent:StrictIdle;O->SetBoolField(TEXT("idle_observed"),StrictIdle);O->SetBoolField(TEXT("continuation_ready_observed"),Idle);O->SetBoolField(TEXT("authoring_wait_is_not_compile_completion"),AuthoringQuiescence);return true;
 }
 void BeginWait(EPhase Waiting,const FString& Reason){Phase=Waiting;const double Now=FPlatformTime::Seconds();if(Waiting==EPhase::WaitFinal){if(FinalWaitStarted<0)FinalWaitStarted=Now;WaitStarted=FinalWaitStarted;}else{if(WaitBudgetStage!=StageIndex){WaitBudgetStage=StageIndex;StageWaitStarted=Now;MiddleWait=MakeShared<FJsonObject>();}WaitStarted=StageWaitStarted;}NextPoll=Now;WaitReason=Reason;Work.R.Stage=Reason;Publish();}
 void WaitTick()
 {
  const double Now=FPlatformTime::Seconds();if(Now<NextPoll)return;NextPoll=Now+FMath::Clamp(Recipe.Poll,0.02,1.0);const bool AuthoringWait=Phase==EPhase::WaitMiddle&&MayAwaitAuthoringQuiescence();bool Idle=false;if(!ObserveIdle(Idle,AuthoringWait)){Terminal(false);return;}auto Wait=Phase==EPhase::WaitFinal?FinalWait:(Phase==EPhase::WaitFactory?FactoryWait:MiddleWait);double Iterations=0;Wait->TryGetNumberField(TEXT("wait_iterations"),Iterations);Wait->SetNumberField(TEXT("wait_iterations"),Iterations+1);Wait->SetNumberField(TEXT("wait_elapsed_seconds"),Now-WaitStarted);Wait->SetBoolField(TEXT("idle_observed"),Idle);Wait->SetBoolField(TEXT("wait_timed_out"),false);Wait->SetNumberField(TEXT("timeout_seconds"),FMath::Clamp(Recipe.Timeout,0.0,60.0));Wait->SetNumberField(TEXT("poll_interval_seconds"),FMath::Clamp(Recipe.Poll,0.02,1.0));Wait->SetBoolField(TEXT("wait_for_completion"),true);
  const TSharedPtr<FJsonObject>* Point=nullptr;if(Work.R.Data->TryGetObjectField(TEXT("last_compile_point"),Point)&&Point)for(const auto& Pair:(*Point)->Values)Wait->SetField(FString(Pair.Key),Pair.Value);
  if(Idle){if(Phase==EPhase::WaitFinal){FinalIdle=true;Phase=EPhase::Verify;}else Phase=EPhase::Stages;Work.R.Stage=AuthoringWait?TEXT("authoring_quiescent_continue_next_editor_frame"):TEXT("compile_idle_continue_next_editor_frame");Publish();return;}if(Now-WaitStarted>=FMath::Clamp(Recipe.Timeout,0.0,60.0)){Wait->SetBoolField(TEXT("wait_timed_out"),true);Work.Fail(TEXT("niagara_sprite_preset_async_compile_timeout"),TEXT("Owned compile did not become idle before monotonic timeout; partial assets remain, compiler not cancelled, no replay/save"));Terminal(false);return;}Publish();
 }
 void Prepare()
 {
  if(!PrepareRecipe(Work,Requested,Recipe)){Terminal(false);return;}Work.RequestedObject=Recipe.CreatedPath;Work.RequestedPackage=PackagePathFromObjectPath(Recipe.CreatedPath);FPackageName::TryConvertLongPackageNameToFilename(Work.RequestedPackage,Work.RequestedFilename,FPackageName::GetAssetPackageExtension());Prepared=true;Phase=EPhase::Stages;Work.R.Stage=TEXT("prepared_create_once_next_editor_frame");Publish();
 }
 bool NoOuterTransaction(){return !GEditor->IsTransactionActive()||Work.Fail(TEXT("niagara_sprite_preset_async_outer_transaction"),TEXT("Continuation writer refuses an active external Editor transaction; no implicit Undo/transaction reset"));}
 void Stage()
 {
  if(!NoOuterTransaction()){Terminal(false);return;}
  if(StageIndex>=Recipe.Plan.Num()){Phase=EPhase::FinalRequest;Publish();return;}if(FactoryCreated){bool Idle=false;if(!ObserveIdle(Idle,MayAwaitAuthoringQuiescence())){Terminal(false);return;}if(!Idle){BeginWait(EPhase::WaitMiddle,TEXT("awaiting_owned_interstage_compile_idle"));return;}}
  if(FactoryCreated&&!VerifyRI()){Terminal(false);return;}const FStagePlan& Plan=Recipe.Plan[StageIndex];TSharedPtr<FJsonObject> Leaf;Work.R.Stage=Plan.Name;
  switch(Plan.Kind){case EStageCall::Create:Leaf=HandleCreateBasicSpriteNiagaraSystem(Plan.Params);break;case EStageCall::Batch:
   {
    TFunction<bool(UNiagaraSystem*,const TCHAR*)> OwnedGuard=[this](UNiagaraSystem* System,const TCHAR* Boundary)
    {
     if(!Executing||!Running||Pending.Get()!=this||Current!=&Work||Phase!=EPhase::Stages||!FactoryCreated||FinalRequested||!Recipe.Plan.IsValidIndex(StageIndex)||Recipe.Plan[StageIndex].Kind!=EStageCall::Batch)return Work.Fail(TEXT("niagara_sprite_preset_owned_batch_scope_changed"),TEXT("Only the executing GU job's current Batch stage may borrow authoring admission."));
     if(!Work.Admit(Boundary))return false;
     if(System!=Work.Target.Get())return Work.Fail(TEXT("niagara_sprite_preset_owned_batch_target_changed"),TEXT("Resolved child System is not the held GU-created target."));
     bool Ready=false;if(!ObserveIdle(Ready,MayAwaitAuthoringQuiescence()))return false;
     if(!Ready)return Work.Fail(TEXT("niagara_sprite_preset_owned_batch_compile_active"),TEXT("Owned child is no longer VM/GPU-quiescent under the current strict/authoring phase; no later child SDK."));
     return true;
    };
    Leaf=HandleSetNiagaraModuleInputsBatch(Plan.Params,&OwnedGuard);break;
   }case EStageCall::Module:Leaf=HandleAddNiagaraStockModuleToStack(Plan.Params);break;case EStageCall::Initialize:Leaf=SetGeneratedPresetInitializeParticleDefaults(Work.Target.Get(),0,Recipe.Settings.SpriteSize,Recipe.Settings.Lifetime);break;case EStageCall::Renderer:Leaf=HandleSetNiagaraSpriteRendererSettings(Plan.Params);break;case EStageCall::Bounds:Leaf=HandleSetNiagaraEmitterFixedBounds(Plan.Params);break;case EStageCall::Local:Leaf=HandleSetNiagaraEmitterLocalSpace(Plan.Params);break;}
  if(Leaf){Leaf->SetStringField(TEXT("stage"),Plan.Name);StageResults.Add(MakeShared<FJsonValueObject>(Leaf));if(Plan.Kind==EStageCall::Create){FString Reported;Leaf->TryGetStringField(TEXT("system_path"),Reported);const TSharedPtr<FJsonObject>* Native=nullptr;const TSharedPtr<FJsonObject>* Data=nullptr;if(Reported.IsEmpty()&&Leaf->TryGetObjectField(TEXT("native_result"),Native)&&Native&&(*Native)->TryGetObjectField(TEXT("data"),Data)&&Data)(*Data)->TryGetStringField(TEXT("created_asset_path"),Reported);if(Reported.Equals(Work.RequestedObject,ESearchCase::CaseSensitive))CreatedPath=Reported;}}Changed|=Work.R.bChanged;bool Success=false;if(Leaf)Leaf->TryGetBoolField(TEXT("success"),Success);Publish(); // actual child prefix is visible before error/terminal
  if(!Work.R.Error.IsEmpty()||!Leaf||!Success){if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_sprite_preset_async_stage_failed"),TEXT("Dedicated stage failed its actual target contract"));Terminal(false);return;}
  if(Plan.Kind==EStageCall::Create){FString ReturnedPath;Leaf->TryGetStringField(TEXT("system_path"),ReturnedPath);if(!Work.BindCreated(ReturnedPath)){Terminal(false);return;}CreatedPath=ReturnedPath;FactoryCreated=true;Changed=true;}
  ++StageIndex;if(Plan.Kind==EStageCall::Create)BeginWait(EPhase::WaitFactory,TEXT("awaiting_factory_compile_idle"));else Publish();
 }
 void RequestFinal()
 {
  if(!NoOuterTransaction()){Terminal(false);return;}
  bool Idle=false;if(!ObserveIdle(Idle,MayAwaitAuthoringQuiescence())){Terminal(false);return;}if(!Idle){BeginWait(EPhase::WaitMiddle,TEXT("awaiting_pre_final_compile_idle"));return;}if(!VerifySettings(Work,Recipe)){Terminal(false);return;}auto* S=Work.Target.Get();if(!SnapshotCaptured){if(!Work.System(S,1)||!PackageOwnership(Work,S,TEXT("final_precompile_package"))){Terminal(false);return;}SnapshotCaptured=true;}
  if(!Work.SDK(TEXT("Preset_Final_RequestCompile"),true,true)){Terminal(false);return;}Work.R.bCompileAttempted=true;Work.R.Compiled.Reset();FinalRequested=true;const bool Accepted=S->RequestCompile(false);Work.R.Data->SetBoolField(TEXT("final_compile_request_returned"),true);Work.R.Data->SetBoolField(TEXT("final_compile_accepted"),Accepted);if(!Work.After(TEXT("Preset_Final_RequestCompile"))){Terminal(false);return;}if(Recipe.Wait)BeginWait(EPhase::WaitFinal,TEXT("awaiting_final_compile_idle"));else{FinalWait->SetBoolField(TEXT("wait_for_completion"),false);FinalWait->SetNumberField(TEXT("timeout_seconds"),Recipe.Timeout);FinalWait->SetNumberField(TEXT("poll_interval_seconds"),Recipe.Poll);FinalWait->SetBoolField(TEXT("wait_timed_out"),false);FinalWait->SetNumberField(TEXT("wait_iterations"),0);FinalWait->SetNumberField(TEXT("wait_elapsed_seconds"),0);bool PointIdle=false;if(!ObserveIdle(PointIdle)){Terminal(false);return;}FinalIdle=PointIdle;const TSharedPtr<FJsonObject>* Point=nullptr;if(Work.R.Data->TryGetObjectField(TEXT("last_compile_point"),Point)&&Point)for(const auto& Pair:(*Point)->Values)FinalWait->SetField(FString(Pair.Key),Pair.Value);Phase=EPhase::Verify;Publish();}
 }
 void Verify()
 {
  if(Recipe.Wait){bool Idle=false;if(!ObserveIdle(Idle)){Terminal(false);return;}if(!Idle){BeginWait(EPhase::WaitFinal,TEXT("final_compile_resumed_before_verification"));return;}}
  if(!VerifySettings(Work,Recipe)){Terminal(false);return;}Phase=Recipe.Save?EPhase::Save:EPhase::Audit;Publish();
 }
 void Save()
 {
  if(!NoOuterTransaction()){Terminal(false);return;}
  if(Recipe.Wait){bool Idle=false;if(!ObserveIdle(Idle)){Terminal(false);return;}if(!Idle){BeginWait(EPhase::WaitFinal,TEXT("final_compile_resumed_before_save"));return;}}
  if(!VerifySettings(Work,Recipe)){Terminal(false);return;}const bool Saved=SaveNiagaraAssetDirect(Work.Target.Get(),SavedFilename);Publish();if(!Saved||!Work.R.Error.IsEmpty()){Terminal(false);return;}if(!VerifySettings(Work,Recipe)){Terminal(false);return;}Phase=EPhase::Audit;Publish();
 }
 void Audit()
 {
  if(!Work.Admit(TEXT("final_dependency_audit"))){Terminal(false);return;}auto* S=Work.Target.Get();const bool BlockTemp=!IsTempGeneratedNiagaraPath(Work.RequestedObject);AuditResult=BuildNiagaraDependencyAuditJson(S,SavedFilename,BlockTemp);if(!Work.R.Error.IsEmpty()||!VerifySettings(Work,Recipe)){Terminal(false);return;}Phase=EPhase::Final;Publish();
 }
 void Final()
 {
  if(!Work.Admit(TEXT("final_health"))||!VerifySettings(Work,Recipe)){Terminal(false);return;}if(Recipe.Wait){bool Idle=false;if(!ObserveIdle(Idle)){Terminal(false);return;}if(!Idle){Work.Fail(TEXT("niagara_sprite_preset_async_postsave_compile_resumed"),TEXT("Compilation resumed after final verification/save; actual save history retained, no re-save/replay"));Terminal(false);return;}FinalIdle=true;}
  Work.R.Data->SetObjectField(TEXT("legacy_result"),BuildSuccess(Work,Recipe,StageResults,FinalWait,SavedFilename,AuditResult));Work.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);Work.R.Data->SetBoolField(TEXT("requested_final_idle_verified"),!Recipe.Wait||FinalIdle);Work.R.Data->SetObjectField(TEXT("object_observation"),Work.DirtyProjection());Terminal(true);
 }
 bool Tick(float)
 {
  if(Phase==EPhase::Terminal)return false;if(!IsInGameThread())return true;if(Executing)return true;auto KeepAlive=AsShared();TGuardValue<bool> Busy(Executing,true);
  if(ShuttingDown.load()||Work.Cancelled){Work.Fail(TEXT("niagara_sprite_preset_async_cancelled"),Work.CancelText.IsEmpty()?TEXT("Controller shutdown"):Work.CancelText);Terminal(false);return false;}if(!Safe()){Work.Fail(TEXT("niagara_sprite_preset_async_context_unavailable"),TEXT("Unsafe GC/exit/save/Editor context; partial prefix retained without live observation"));Terminal(false);return false;}
  FExecutionScope Pins(Work);TGuardValue<FWork*> Scope(Current,&Work);TGuardValue<TArray<FRIExpected>*> RIScope(RIExpected,&Expected);TGuardValue<bool> RunningScope(Running,true);if(!Work.Admit(TEXT("job_tick"))){Terminal(false);return false;}
  switch(Phase){case EPhase::Prepare:Prepare();break;case EPhase::Stages:Stage();break;case EPhase::WaitFactory:case EPhase::WaitMiddle:case EPhase::WaitFinal:WaitTick();break;case EPhase::FinalRequest:RequestFinal();break;case EPhase::Verify:Verify();break;case EPhase::Save:Save();break;case EPhase::Audit:Audit();break;case EPhase::Final:Final();break;default:break;}
  if(Work.Cancelled&&Phase!=EPhase::Terminal){Work.Fail(TEXT("niagara_sprite_preset_async_cancelled"),Work.CancelText);Terminal(false);}return Phase!=EPhase::Terminal;
 }
};
UCubelessNiagaraSpritePresetJob* Start(const FString& ParamsJson)
{
 if(!Safe())return nullptr;auto State=MakeShared<FJobState>();if(ShuttingDown.load()||Pending.IsValid()||Running){State->Work.Fail(TEXT("niagara_sprite_preset_async_busy"),TEXT("One owned job or Native execution is active; no duplicate dispatch"));State->Terminal(false);return State->Job.Get();}
 if(!Parse(ParamsJson,State->Requested)){State->Work.Fail(TEXT("niagara_sprite_preset_async_params_invalid"),TEXT("Bounded finite object with exact unique keys and no embedded NUL required"));State->Terminal(false);return State->Job.Get();}State->Requested->TryGetBoolField(TEXT("save"),State->Work.R.bSaveRequested);State->Work.R.Data->SetObjectField(TEXT("requested_params"),State->Requested);if(!State->Work.Admit(TEXT("start_context"))){State->Terminal(false);return State->Job.Get();}Pending=State;TWeakPtr<FJobState> Weak=State;State->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){if(auto S=Weak.Pin())return S->Tick(Delta);return false;}),0.0f);if(!State->Ticker.IsValid()){State->Work.Fail(TEXT("niagara_sprite_preset_async_ticker_unavailable"),TEXT("Owned ticker registration failed before creation"));State->Terminal(false);}else{State->Work.R.Stage=TEXT("queued_prepare_next_editor_frame");State->Publish();}return State->Job.Get();
}
bool Cancel(UCubelessNiagaraSpritePresetJob* Job,const FString& Reason)
{if(!IsInGameThread()||!Pending.IsValid()||Pending->Job.Get()!=Job)return false;auto State=Pending;State->CancelPlain(Reason);return true;}
void Shutdown(const FString& Reason)
{ShuttingDown.store(true);if(!IsInGameThread())return;if(auto State=Pending){State->Unhook();State->CancelPlain(Reason);}}
FString RejectSynchronous(const FString& ParamsJson)
{
 FResult R;R.Compiled=false;R.bCompileRequested=true;R.bSaveRequested=true;TSharedPtr<FJsonObject> Params;if(Parse(ParamsJson,Params))Params->TryGetBoolField(TEXT("save"),R.bSaveRequested);R.Stage=TEXT("async_delivery_admission");R.ErrorCode=TEXT("niagara_sprite_preset_async_delivery_required");R.Error=TEXT("Sprite preset requires the same owned Native Job to resume after factory compile idle. Direct/RawBatch continuation is not connected; rejected before first writer, no creation/request replay.");R.Data->SetBoolField(TEXT("operation_complete"),true);R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);auto L=MakeShared<FJsonObject>();L->SetBoolField(TEXT("success"),false);L->SetStringField(TEXT("command"),TEXT("create_sprite_niagara_preset_system"));L->SetStringField(TEXT("error_code"),R.ErrorCode);L->SetStringField(TEXT("error"),R.Error);R.Data->SetObjectField(TEXT("legacy_result"),L);return Operations::EncodeVariableResult(R,TEXT("create_sprite_niagara_preset_system"));
}
} // NiagaraSpritePresetAsync
