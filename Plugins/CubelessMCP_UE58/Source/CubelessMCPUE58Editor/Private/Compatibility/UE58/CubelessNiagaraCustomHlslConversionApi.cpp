// GV dedicated Editor-only HLSL conversion. Source candidate, not build/runtime proof.
#include "Compatibility/UE58/CubelessNiagaraCustomHlslConversionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
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
#include "EditorAssetLibrary.h"
#include "NiagaraNodeCustomHlsl.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraActions.h"
#include "UObject/PropertyAccessUtil.h"
#include "ToolsetRegistry/ToolsetLibrary.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"
#include "Modules/ModuleManager.h"
#include "Compatibility/UE58/CubelessNiagaraAuthoringInspectionApi.h"
#include "CubelessNiagaraHlslConversionJob.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#include "Templates/Function.h"
#include <atomic>
namespace Cubeless::UE58::NiagaraCustomHlslConversion
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
struct FIdentity
{
 TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;
 FString Path,PackageName,PlainClass,PlainOuter,PlainPackage,PlainEffective;bool DirtyBefore=false;
  TWeakObjectPtr<UNiagaraDataInterface> CompiledCacheRoot;FString CompiledCacheRelative,RetiredCachePath;EObjectFlags CapturedFlags=RF_NoFlags,RetiredCacheFlags=RF_NoFlags;bool CompiledCacheRetired=false;
 static bool ReadableChain(const UObject* O)
 {
  if(!Safe())return false;int32 Depth=0;
  for(const UObject* A=O;A;A=A->GetOuter()){if(!Live(A)||++Depth>64)return false;}
  return true;
 }
 static FString DiagnosticPath(const UObject* O)
 {
  if(!O)return TEXT("<null>");
  return ReadableChain(O)?O->GetPathName().Left(1024):FString(TEXT("<unavailable>"));
 }
 explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()),PackageName(O->GetOutermost()->GetName()),PlainClass(DiagnosticPath(O->GetClass())),PlainOuter(DiagnosticPath(O->GetOuter())),PlainPackage(DiagnosticPath(O->GetOutermost())),PlainEffective(DiagnosticPath(O->GetPackage())),DirtyBefore(O->GetOutermost()->IsDirty()),CapturedFlags(O->GetFlags()){}
 bool BaseExact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&Live(Package.Get())&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get();}
  bool Exact()const{auto* O=Object.Get();return BaseExact()&&O->GetPathName().Equals(CompiledCacheRetired?RetiredCachePath:Path,ESearchCase::CaseSensitive)&&(!CompiledCacheRetired||O->GetFlags()==RetiredCacheFlags);}
 TSharedPtr<FJsonObject> FailureDiagnostic(const FString& Stage)const
 {
  auto Row=MakeShared<FJsonObject>(),Expected=MakeShared<FJsonObject>(),Observed=MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("stage"),Stage.Left(256));Row->SetNumberField(TEXT("path_character_limit"),1024);
  Expected->SetStringField(TEXT("path"),Path.Left(1024));Expected->SetBoolField(TEXT("path_truncated"),Path.Len()>1024);
  Expected->SetStringField(TEXT("class"),PlainClass);Expected->SetStringField(TEXT("outer"),PlainOuter);Expected->SetStringField(TEXT("outermost_package"),PlainPackage);Expected->SetStringField(TEXT("effective_package"),PlainEffective);
  Row->SetObjectField(TEXT("expected"),Expected);Row->SetObjectField(TEXT("current"),Observed);
  Observed->SetBoolField(TEXT("available"),false);
  if(!Safe()){Row->SetStringField(TEXT("reason"),TEXT("current_context_unavailable"));return Row;}
  UObject* O=Object.Get();if(!Live(O)){Row->SetStringField(TEXT("reason"),TEXT("captured_object_unavailable_or_not_live"));return Row;}
  if(!ReadableChain(O)){Row->SetStringField(TEXT("reason"),TEXT("current_owner_chain_unavailable_or_bounded"));return Row;}
  UClass* C=O->GetClass();UObject* CurrentOuter=O->GetOuter();UPackage* CurrentPackage=O->GetOutermost();UPackage* CurrentEffective=O->GetPackage();
  const FString CurrentPath=O->GetPathName();
  Observed->SetBoolField(TEXT("available"),true);Observed->SetStringField(TEXT("path"),CurrentPath.Left(1024));Observed->SetBoolField(TEXT("path_truncated"),CurrentPath.Len()>1024);
  Observed->SetStringField(TEXT("class"),DiagnosticPath(C));Observed->SetStringField(TEXT("outer"),DiagnosticPath(CurrentOuter));Observed->SetStringField(TEXT("outermost_package"),DiagnosticPath(CurrentPackage));Observed->SetStringField(TEXT("effective_package"),DiagnosticPath(CurrentEffective));
  Observed->SetBoolField(TEXT("same_class"),C==Class.Get());Observed->SetBoolField(TEXT("same_outer"),CurrentOuter==Outer.Get());Observed->SetBoolField(TEXT("same_outermost_package"),CurrentPackage==Package.Get());Observed->SetBoolField(TEXT("same_effective_package"),CurrentEffective==Effective.Get());
  const TCHAR* Reason=!Live(Class.Get())?TEXT("captured_class_unavailable"):Class->HasAnyClassFlags(CLASS_NewerVersionExists)?TEXT("captured_class_newer_version_exists"):!Live(Package.Get())?TEXT("captured_outermost_package_unavailable"):C!=Class.Get()?TEXT("class_identity_changed"):CurrentOuter!=Outer.Get()?TEXT("outer_identity_changed"):CurrentPackage!=Package.Get()?TEXT("outermost_package_identity_changed"):CurrentEffective!=Effective.Get()?TEXT("effective_package_identity_changed"):!CurrentPath.Equals(Path,ESearchCase::CaseSensitive)?TEXT("path_changed"):TEXT("exact_failure_not_reproduced_in_safe_diagnostic");
  Row->SetStringField(TEXT("reason"),Reason);return Row;
 }
};
struct FPin{UEdGraphPin* P=nullptr;FGuid Id,Persistent;FName Name;EEdGraphPinDirection Direction=EGPD_Input;FEdGraphPinType Type;bool NotConnectable=false,DefaultReadOnly=false,Orphaned=false;TWeakObjectPtr<UObject> DefaultObject;TArray<UEdGraphPin*> Links;FString Default,AutoDefault,DefaultText;};
struct FNode{TWeakObjectPtr<UEdGraphNode> Node;FGuid Id;int32 X=0,Y=0;TArray<FPin> Pins;};
struct FGraph{TWeakObjectPtr<UNiagaraScriptSource> Source;TWeakObjectPtr<UNiagaraGraph> Graph;TArray<FNode> Nodes;};
struct FScript{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Exposed;};
FString EmitterPropertyState(const FVersionedNiagaraEmitterData* D){auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("local_space"),D->bLocalSpace);O->SetNumberField(TEXT("sim_target"),static_cast<int32>(D->SimTarget));O->SetBoolField(TEXT("persistent_ids"),D->bRequiresPersistentIDs!=0);O->SetNumberField(TEXT("bounds_mode"),static_cast<int32>(D->CalculateBoundsMode));O->SetBoolField(TEXT("determinism"),D->bDeterminism);O->SetNumberField(TEXT("random_seed"),D->RandomSeed);O->SetNumberField(TEXT("scalability_quality_mask"),D->Platforms.QualityLevelMask);return Json(O);}
struct FCustom{FNiagaraFunctionSignature Signature;ENiagaraScriptUsage Usage=ENiagaraScriptUsage::Function;FString Body;};
struct FEmitter{TWeakObjectPtr<UNiagaraEmitter> Emitter;FGuid Version;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TArray<TWeakObjectPtr<UNiagaraRendererProperties>> Renderers;TArray<TWeakObjectPtr<UNiagaraScript>> Scripts;TWeakObjectPtr<UNiagaraEmitter> Parent,MergeParent;bool Latest=false;FString UniqueName,PropertyState;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,ParentScratch;TArray<TWeakObjectPtr<UNiagaraScript>> ScratchScripts,ParentScripts;};
struct FHandle{FGuid Id,Version;TWeakObjectPtr<UNiagaraEmitter> Emitter;TWeakObjectPtr<UNiagaraEmitterBase> Base;FVersionedNiagaraEmitterData* Data=nullptr;FName Name;bool Enabled=false;ENiagaraEmitterMode Mode=ENiagaraEmitterMode::Standard;};
struct FSystem{TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<FHandle> Handles;TArray<TWeakObjectPtr<UNiagaraScript>> Scratch;};
struct FCachedDIRole
{
 TWeakObjectPtr<UNiagaraDataInterface> DI;TWeakObjectPtr<UNiagaraScript> Owner;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;FNiagaraScriptDataInterfaceInfo Info;uint64 CapturedEpoch=0;FName OriginalName;FString OriginalPath;bool Retired=false;
 bool Matches(const FNiagaraScriptDataInterfaceInfo& Other)const{return Info.Name==Other.Name&&Info.CompileName==Other.CompileName&&Info.UserPtrIdx==Other.UserPtrIdx&&Info.Type==Other.Type&&Info.RegisteredParameterMapRead==Other.RegisteredParameterMapRead&&Info.RegisteredParameterMapWrite==Other.RegisteredParameterMapWrite&&Info.SourceEmitterName.Equals(Other.SourceEmitterName,ESearchCase::CaseSensitive);}
};
struct FWork
{
 uint64 OwnedCompileEpoch=0;TArray<FCachedDIRole> CompiledCacheRoles;TArray<TSharedPtr<FJsonValue>> CompiledCacheTransitions;
 bool CaptureCompiledCacheRoles();bool AdmitObjectIdentities(const FString& Stage);
 FResult R;bool DeferredMode=false,CancelRequested=false;FString CancelReason;TWeakObjectPtr<UEditorEngine> CapturedEditor;UNiagaraGraph* TransitionGraph=nullptr;TFunction<bool()> DeferredStep;TFunction<void()> DeferredInterrupted;bool Persistent=false,WorldCaptured=false,TypedPassed=false,TargetChanged=false;UNiagaraNodeCustomHlsl* TransitionNode=nullptr;TMap<UNiagaraNodeCustomHlsl*,FCustom> Custom;FString FailedStage;TWeakObjectPtr<UWorld> World;TArray<TUniquePtr<FIdentity>> Objects;TArray<TStrongObjectPtr<UObject>> TransitionHolds;TArray<FGraph> Graphs;TArray<FScript> Scripts;TArray<FEmitter> Emitters;TArray<FSystem> Systems;TMap<UNiagaraSpriteRendererProperties*,UMaterialInterface*> ExpectedMaterials;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls,SaveRows,CreationRows;TWeakObjectPtr<UNiagaraSystem> Source,Target;bool SourceDirty=false,SourceCaptured=false;FString SourceFilename,SourceFileHash;int64 SourceFileSize=-1;FString RequestedObject,RequestedPackage,RequestedFilename;int64 Slots=0,Chars=0;
 explicit FWork(bool P):Persistent(P){R.Compiled=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);}
 bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
 TSharedPtr<FJsonObject> Error(const FString& M){if(R.Error.IsEmpty())Fail(TEXT("niagara_hlsl_conversion_bootstrap_failed"),M);auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),M);return O;}
 bool Budget(int64 N,int64 Text=0){Slots+=N;Chars+=Text;return (Slots<=262144&&Chars<=32ll*1024*1024)||Fail(TEXT("niagara_hlsl_conversion_output_budget_exceeded"),TEXT("Bounded snapshot/output exceeded; partial prefix retained."));}
 bool Admit(const FString& Stage){R.Stage=Stage;if(DeferredMode&&DeferredShutdownRequested.load()){CancelRequested=true;if(CancelReason.IsEmpty())CancelReason=TEXT("Module shutdown stopped HLSL conversion.");}if(CancelRequested)return Fail(TEXT("niagara_hlsl_conversion_cancelled"),CancelReason);if(!R.Error.IsEmpty())return false;if(!Safe())return Fail(TEXT("niagara_hlsl_conversion_context_unavailable"),TEXT("Live Editor game thread outside GC/exit/save required."));if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_hlsl_conversion_editor_changed"),TEXT("Captured Editor changed during continuation."));if(Persistent&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld))return Fail(TEXT("niagara_hlsl_conversion_play_session_active"),TEXT("Persistent factory creation requires stopped PIE/SIE."));auto* W=GEditor->GetEditorWorldContext().World();if(!Live(W)||W->bIsTearingDown)return Fail(TEXT("niagara_hlsl_conversion_world_unavailable"),TEXT("Editor world unavailable or tearing down."));if(!WorldCaptured){World=W;WorldCaptured=true;if(DeferredMode)CapturedEditor=GEditor;}else if(World.Get()!=W)return Fail(TEXT("niagara_hlsl_conversion_world_changed"),TEXT("Editor world changed during SDK dispatch."));if(!AdmitObjectIdentities(Stage))return false;
 if(SourceCaptured&&(!Live(Source.Get())||Source->GetOutermost()->IsDirty()!=SourceDirty))return Fail(TEXT("niagara_hlsl_conversion_source_dirty_changed"),TEXT("Captured source dirty state changed across SDK callbacks; no replay or automatic source save."));
 for(const auto& S:Scripts)if(S.Script->GetLatestSource()!=S.Source.Get()||S.Script->GetExposedVersion().VersionGuid!=S.Exposed)return Fail(TEXT("niagara_hlsl_conversion_script_changed"),TEXT("Captured script source/version changed."));
 for(const auto& P:Custom){auto* N=P.Key;if(!Live(N))return Fail(TEXT("niagara_hlsl_conversion_custom_lost"),TEXT("Captured HLSL node expired"));if(!FNiagaraFunctionSignature::StaticStruct()->CompareScriptStruct(&N->Signature,&P.Value.Signature,0)||N->ScriptUsage!=P.Value.Usage)return Fail(TEXT("niagara_hlsl_conversion_signature_changed"),TEXT("Captured HLSL signature or usage changed"));if(N!=TransitionNode){auto* Field=FindFProperty<FStrProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));if(!Field||!Field->GetPropertyValue_InContainer(N).Equals(P.Value.Body,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_hlsl_conversion_body_changed"),TEXT("Captured HLSL body changed outside exact setter transition"));}}
 for(const auto& E:Emitters){auto* O=E.Emitter.Get();if(O->GetEmitterData(E.Version)!=E.Data||(E.Latest&&O->GetLatestEmitterData()!=E.Data))return Fail(TEXT("niagara_hlsl_conversion_emitter_data_replaced"),TEXT("Current emitter version data no longer matches captured pointer; retired raw data is not read"));if(E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.ParentScratch.Get())return Fail(TEXT("niagara_hlsl_conversion_scratch_container_changed"),TEXT("Captured emitter scratch container changed"));auto SameContainer=[](UNiagaraScratchPadContainer* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& A){if(!C)return A.IsEmpty();if(C->Scripts.Num()!=A.Num())return false;for(int32 I=0;I<A.Num();++I)if(C->Scripts[I]!=A[I].Get())return false;return true;};if(!SameContainer(E.Scratch.Get(),E.ScratchScripts)||!SameContainer(E.ParentScratch.Get(),E.ParentScripts))return Fail(TEXT("niagara_hlsl_conversion_scratch_membership_changed"),TEXT("Captured emitter scratch physical scripts changed"));if(O->GetEmitterData(E.Version)!=E.Data||(E.Latest&&O->GetLatestEmitterData()!=E.Data)||!O->GetUniqueEmitterName().Equals(E.UniqueName,ESearchCase::CaseSensitive)||!EmitterPropertyState(E.Data).Equals(E.PropertyState,ESearchCase::CaseSensitive)||E.Data->GraphSource!=E.Source.Get()||E.Data->VersionedParent.Emitter!=E.Parent.Get()||E.Data->VersionedParentAtLastMerge.Emitter!=E.MergeParent.Get()||E.Data->GetRenderers().Num()!=E.Renderers.Num())return Fail(TEXT("niagara_hlsl_conversion_emitter_changed"),TEXT("Emitter version/source/parent/renderer topology changed."));for(int32 I=0;I<E.Renderers.Num();++I)if(E.Data->GetRenderers()[I]!=E.Renderers[I].Get())return Fail(TEXT("niagara_hlsl_conversion_renderer_changed"),TEXT("Physical renderer slot changed."));TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);if(A.Num()!=E.Scripts.Num())return Fail(TEXT("niagara_hlsl_conversion_emitter_scripts_changed"),TEXT("Emitter scripts changed."));for(int32 I=0;I<A.Num();++I)if(A[I]!=E.Scripts[I].Get())return Fail(TEXT("niagara_hlsl_conversion_emitter_scripts_changed"),TEXT("Emitter script order changed."));}
 for(const auto& S:Systems){auto* O=S.System.Get();if(O->ScratchPadScripts.Num()!=S.Scratch.Num())return Fail(TEXT("niagara_hlsl_conversion_system_scratch_changed"),TEXT("System scratch scripts changed"));for(int32 I=0;I<S.Scratch.Num();++I)if(O->ScratchPadScripts[I]!=S.Scratch[I].Get())return Fail(TEXT("niagara_hlsl_conversion_system_scratch_changed"),TEXT("System scratch order changed"));if(O->GetSystemSpawnScript()!=S.Spawn.Get()||O->GetSystemUpdateScript()!=S.Update.Get()||O->GetEmitterHandles().Num()!=S.Handles.Num())return Fail(TEXT("niagara_hlsl_conversion_system_changed"),TEXT("System script/emitter topology changed."));for(int32 I=0;I<S.Handles.Num();++I){const auto& H=O->GetEmitterHandles()[I];const auto& E=S.Handles[I];if(H.GetId()!=E.Id||H.GetEmitterBase()!=E.Base.Get()||H.GetEmitterMode()!=E.Mode||H.GetName()!=E.Name||H.GetIsEnabled()!=E.Enabled||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.Version||H.GetEmitterData()!=E.Data)return Fail(TEXT("niagara_hlsl_conversion_system_changed"),TEXT("System physical emitter/version changed."));}}
 for(const auto& G:Graphs){if(G.Graph.Get()==TransitionGraph){if(G.Source->NodeGraph!=G.Graph.Get())return Fail(TEXT("niagara_hlsl_conversion_source_graph_changed"),TEXT("Transition source graph routing changed."));continue;}if(G.Source->NodeGraph!=G.Graph.Get()||G.Graph->Nodes.Num()!=G.Nodes.Num())return Fail(TEXT("niagara_hlsl_conversion_graph_changed"),TEXT("Captured source graph membership changed."));for(int32 I=0;I<G.Nodes.Num();++I){auto* N=G.Nodes[I].Node.Get();const auto& E=G.Nodes[I];if(G.Graph->Nodes[I]!=N||N->NodeGuid!=E.Id||(DeferredMode&&(N->NodePosX!=E.X||N->NodePosY!=E.Y))||(N!=TransitionNode&&N->Pins.Num()!=E.Pins.Num()))return Fail(TEXT("niagara_hlsl_conversion_node_changed"),TEXT("Captured node/pin topology changed."));if(N==TransitionNode)continue;for(int32 J=0;J<E.Pins.Num();++J){auto* P=N->Pins[J];const auto& V=E.Pins[J];if(P!=V.P||P->PinId!=V.Id||P->PersistentGuid!=V.Persistent||P->PinName!=V.Name||P->Direction!=V.Direction||P->PinType!=V.Type||P->bNotConnectable!=V.NotConnectable||P->bDefaultValueIsReadOnly!=V.DefaultReadOnly||P->bOrphanedPin!=V.Orphaned||P->DefaultObject!=V.DefaultObject.Get()||!P->AutogeneratedDefaultValue.Equals(V.AutoDefault,ESearchCase::CaseSensitive)||!P->DefaultTextValue.ToString().Equals(V.DefaultText,ESearchCase::CaseSensitive)||P->GetOwningNodeUnchecked()!=N||(!TransitionNode&&P->LinkedTo!=V.Links)||!P->DefaultValue.Equals(V.Default,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_hlsl_conversion_pin_changed"),TEXT("Exact pin/links/default changed across SDK call."));}}}
 for(const auto& Pair:ExpectedMaterials)if(Pair.Key->Material!=Pair.Value)return Fail(TEXT("niagara_hlsl_conversion_material_readback_failed"),TEXT("Owned sprite material differs from requested exact material."));return true;}
 bool Pin(UObject* O){if(!Admit(TEXT("pin_object"))||!Live(O))return Fail(TEXT("niagara_hlsl_conversion_object_unavailable"),TEXT("Expected live stock object."));for(const auto& I:Objects)if(I->Object.Get()==O)return I->Exact();if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
 bool SDK(const TCHAR* Stage,bool Mutation=false,bool PersistentWrite=false){if(!Admit(Stage)||!Budget(1,FCString::Strlen(Stage)))return false;R.bMutationAttempted|=Mutation;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);if(PersistentWrite)R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("stage"),Stage);O->SetBoolField(TEXT("entered"),true);O->SetBoolField(TEXT("returned"),false);O->SetBoolField(TEXT("mutation_attempted"),Mutation);Calls.Add(MakeShared<FJsonValueObject>(O));return true;}
 bool After(const TCHAR* Stage){if(!Calls.IsEmpty())Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
 template<typename F>bool Step(const TCHAR* Stage,F&& Fn,bool Mutation=false,bool PersistentWrite=false){if(!SDK(Stage,Mutation,PersistentWrite))return false;Fn();return After(Stage);}
 bool Graph(UNiagaraScriptSourceBase* Base){if(!Admit(TEXT("capture_graph")))return false;if(!Base)return true;if(!Pin(Base))return false;auto* S=Cast<UNiagaraScriptSource>(Base);if(!S||!S->NodeGraph)return true;auto* G=S->NodeGraph.Get();for(const auto& E:Graphs)if(E.Source.Get()==S&&E.Graph.Get()==G)return true;if(!Pin(G)||G->Nodes.Num()>16384)return Fail(TEXT("niagara_hlsl_conversion_graph_bound"),TEXT("Graph node bound exceeded."));FGraph E;E.Source=S;E.Graph=G;TSet<UEdGraphPin*> Known;for(UEdGraphNode* N:G->Nodes){if(!N||!Pin(N))return Fail(TEXT("niagara_hlsl_conversion_graph_malformed"),TEXT("Null/unavailable graph node."));if(N->GetGraph()!=G||N->GetOuter()!=G)return Fail(TEXT("niagara_hlsl_conversion_foreign_graph_node"),TEXT("Each captured physical node must have exact graph and outer ownership before writer"));FNode NE;NE.Node=N;NE.Id=N->NodeGuid;NE.X=N->NodePosX;NE.Y=N->NodePosY;for(auto* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N||!Budget(1+P->LinkedTo.Num(),P->DefaultValue.Len()+P->AutogeneratedDefaultValue.Len()+P->DefaultTextValue.ToString().Len()))return Fail(TEXT("niagara_hlsl_conversion_pin_malformed"),TEXT("Null/foreign pin or capture bound exceeded."));FPin PE;PE.P=P;PE.Id=P->PinId;PE.Persistent=P->PersistentGuid;PE.Name=P->PinName;PE.Direction=P->Direction;PE.Type=P->PinType;PE.NotConnectable=P->bNotConnectable;PE.DefaultReadOnly=P->bDefaultValueIsReadOnly;PE.Orphaned=P->bOrphanedPin;PE.DefaultObject=P->DefaultObject;PE.AutoDefault=P->AutogeneratedDefaultValue;PE.DefaultText=P->DefaultTextValue.ToString();PE.Links=P->LinkedTo;PE.Default=P->DefaultValue;Known.Add(P);if(P->DefaultObject&&!Pin(P->DefaultObject))return false;if(P->PinType.PinSubCategoryObject.IsValid()&&!Pin(P->PinType.PinSubCategoryObject.Get()))return false;NE.Pins.Add(MoveTemp(PE));}if(auto* C=Cast<UNiagaraNodeCustomHlsl>(N)){FCustom State;State.Signature=C->Signature;State.Usage=C->ScriptUsage;auto* Field=FindFProperty<FStrProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));if(!Field||EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(Field),EPropertyAccessResultFlags::PermissionDenied))return Fail(TEXT("niagara_hlsl_conversion_property_denied"),TEXT("Exact readable public HLSL property unavailable"));State.Body=Field->GetPropertyValue_InContainer(C);if(!Budget(State.Signature.Inputs.Num()+State.Signature.Outputs.Num(),State.Body.Len()))return false;Custom.Add(C,MoveTemp(State));}if(auto* F=Cast<UNiagaraNodeFunctionCall>(N)){if(F->Signature.Inputs.Num()+F->Signature.Outputs.Num()>65536)return Fail(TEXT("niagara_hlsl_conversion_signature_bound"),TEXT("Function signature bound exceeded."));if(F->FunctionScript&&!Pin(F->FunctionScript))return false;}E.Nodes.Add(MoveTemp(NE));}for(const auto& N:E.Nodes)for(const auto& P:N.Pins)for(auto* L:P.Links)if(!L||!Known.Contains(L))return Fail(TEXT("niagara_hlsl_conversion_link_malformed"),TEXT("Link target is outside captured graph."));Graphs.Add(MoveTemp(E));return Admit(TEXT("graph_captured"));}
 bool Script(UNiagaraScript* S){if(!S)return true;for(const auto& E:Scripts)if(E.Script.Get()==S)return true;if(!Pin(S))return false;FScript E;E.Script=S;E.Source=S->GetLatestSource();E.Exposed=S->GetExposedVersion().VersionGuid;Scripts.Add(E);return Graph(E.Source.Get());}
 bool Emitter(UNiagaraEmitter* O,FVersionedNiagaraEmitterData* D,bool Latest){if(!O||!D||!Pin(O))return Fail(TEXT("niagara_hlsl_conversion_emitter_data_missing"),TEXT("Expected current versioned emitter data."));for(const auto& E:Emitters)if(E.Emitter.Get()==O&&E.Data==D)return true;FEmitter E;E.Emitter=O;E.Data=D;E.Version=D->Version.VersionGuid;E.Latest=Latest;E.UniqueName=O->GetUniqueEmitterName();E.PropertyState=EmitterPropertyState(D);E.Source=D->GraphSource;E.Scratch=D->ScratchPads;E.ParentScratch=D->ParentScratchPads;auto CaptureContainer=[&](UNiagaraScratchPadContainer* C,TArray<TWeakObjectPtr<UNiagaraScript>>& A){if(!C)return true;if(!Pin(C)||C->Scripts.Num()>4096)return false;for(UNiagaraScript* S:C->Scripts){A.Add(S);if(S&&!Script(S))return false;}return true;};if(!CaptureContainer(E.Scratch.Get(),E.ScratchScripts)||!CaptureContainer(E.ParentScratch.Get(),E.ParentScripts))return false;E.Parent=D->VersionedParent.Emitter;E.MergeParent=D->VersionedParentAtLastMerge.Emitter;if(!Graph(E.Source.Get()))return false;if(D->GetRenderers().Num()>4096)return Fail(TEXT("niagara_hlsl_conversion_renderer_bound"),TEXT("Renderer bound exceeded."));for(UNiagaraRendererProperties* P:D->GetRenderers()){E.Renderers.Add(P);if(P&&!Pin(P))return false;if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(P)){ExpectedMaterials.Add(Sprite,Sprite->Material);if(Sprite->Material&&!Pin(Sprite->Material))return false;}}TArray<UNiagaraScript*> A;D->GetScripts(A,false,false);if(A.Num()>4096)return Fail(TEXT("niagara_hlsl_conversion_script_bound"),TEXT("Script bound exceeded."));for(auto* P:A){E.Scripts.Add(P);if(!Script(P))return false;}Emitters.Add(MoveTemp(E));return Admit(TEXT("emitter_captured"));}
 bool System(UNiagaraSystem* O,int32 ExpectedCount){if(!Pin(O)||O->GetEmitterHandles().Num()!=ExpectedCount)return Fail(TEXT("niagara_hlsl_conversion_system_shape_invalid"),TEXT("Factory output emitter count differs from requested construction."));FSystem E;E.System=O;E.Spawn=O->GetSystemSpawnScript();E.Update=O->GetSystemUpdateScript();if(!Script(E.Spawn.Get())||!Script(E.Update.Get()))return false;if(O->ScratchPadScripts.Num()>4096)return Fail(TEXT("niagara_hlsl_conversion_scratch_bound"),TEXT("Scratch script cap exceeded"));for(UNiagaraScript* S:O->ScratchPadScripts){E.Scratch.Add(S);if(S&&!Script(S))return false;}for(const auto& H:O->GetEmitterHandles()){if(!H.GetEmitterBase()||!Pin(H.GetEmitterBase()))return Fail(TEXT("niagara_hlsl_conversion_output_owner_invalid"),TEXT("Exact public emitter base unavailable."));if((!H.GetInstance().Emitter||!H.GetEmitterData())&&H.GetEmitterMode()!=ENiagaraEmitterMode::Stateless)return Fail(TEXT("niagara_hlsl_conversion_output_owner_invalid"),TEXT("Standard emitter has no current versioned instance."));FHandle F;F.Id=H.GetId();F.Base=H.GetEmitterBase();F.Name=H.GetName();F.Enabled=H.GetIsEnabled();F.Mode=H.GetEmitterMode();F.Version=H.GetInstance().Version;F.Emitter=H.GetInstance().Emitter;F.Data=H.GetEmitterData();E.Handles.Add(F);if(F.Emitter.IsValid()&&F.Data&&!Emitter(F.Emitter.Get(),F.Data,false))return false;}Systems.Add(MoveTemp(E));return Admit(TEXT("system_captured"));}
 TSharedPtr<FJsonObject> DirtyProjection(){auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;const bool CanRead=(!DeferredMode||(R.Error.IsEmpty()&&CapturedEditor.Get()==GEditor))&&Safe()&&WorldCaptured&&Live(World.Get())&&!World->bIsTearingDown&&GEditor->GetEditorWorldContext().World()==World.Get();for(const auto& I:Objects){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),I->Path);J->SetStringField(TEXT("package"),I->PackageName);J->SetBoolField(TEXT("dirty_before"),I->DirtyBefore);const bool Exact=CanRead&&I->Exact();J->SetBoolField(TEXT("current_observation_available"),Exact);if(Exact)J->SetBoolField(TEXT("dirty_after"),I->Package->IsDirty());Rows.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("objects"),Rows);O->SetBoolField(TEXT("safe_context"),CanRead);return O;}
};

bool FWork::CaptureCompiledCacheRoles()
{
 if(!DeferredMode||!Admit(TEXT("capture_compiled_cache_roles")))return R.Error.IsEmpty();
 UNiagaraSystem* Root=Target.Get();if(!Live(Root))return false;
 for(const auto& Binding:Scripts)
 {
  UNiagaraScript* Owner=Binding.Script.Get();
  if(!Live(Owner)||!Owner->IsIn(Root)||Owner->GetPackage()!=Root->GetPackage())continue;
  const auto Cache=static_cast<const UNiagaraScript*>(Owner)->GetCachedDefaultDataInterfaces();
  if(Cache.Num()>65536||!Budget(Cache.Num()))return false;
  for(const FNiagaraScriptDataInterfaceInfo& Info:Cache)
  {
   UNiagaraDataInterface* DI=Info.DataInterface.Get();if(!DI)continue;if(!Live(DI))return Fail(TEXT("niagara_hlsl_conversion_cache_role_invalid"),TEXT("Current compiled DI is not live."));if(DI->GetOuter()!=Owner)continue;
   if(Info.Type.GetClass()!=DI->GetClass())return Fail(TEXT("niagara_hlsl_conversion_cache_role_invalid"),TEXT("Live exact typed compiled DI required."));
   const bool ExistingRole=CompiledCacheRoles.ContainsByPredicate([&](const FCachedDIRole& E){return E.DI.Get()==DI;});
   bool SourceDefault=false;for(const auto& G:Graphs)for(const auto& N:G.Nodes)for(const auto& P:N.Pins){UObject* Default=P.DefaultObject.Get();if(Default&&(!Live(Default)||!FIdentity::ReadableChain(Default)))return Fail(TEXT("niagara_hlsl_conversion_cache_source_default_unavailable"),TEXT("Captured source DefaultObject owner chain is not live."));if(Default&&(Default==DI||Default->IsIn(DI)))SourceDefault=true;}
   if(SourceDefault)continue;
   FIdentity* Identity=nullptr;for(const auto& I:Objects)if(I->Object.Get()==DI){Identity=I.Get();break;}
   // Only package-owned exports already strongly captured before compile can acquire this role.
   if(!Identity||!Identity->Exact())continue;
   int32 Membership=0;for(const auto& Row:Cache)Membership+=Row.DataInterface==DI?1:0;
   if(Membership!=1)continue;
   if(!ExistingRole){FCachedDIRole Role;Role.DI=DI;Role.Owner=Owner;Role.Source=Binding.Source;Role.Version=Binding.Exposed;Role.Info=Info;Role.CapturedEpoch=OwnedCompileEpoch;Role.OriginalName=DI->GetFName();Role.OriginalPath=Identity->Path;CompiledCacheRoles.Add(MoveTemp(Role));}
   for(const auto& I:Objects)if(I->Object.Get()==DI||I->Object->IsIn(DI))
   {if(I->CompiledCacheRoot.IsValid()&&I->CompiledCacheRoot.Get()!=DI)return Fail(TEXT("niagara_hlsl_conversion_cache_role_ambiguous"),TEXT("Overlapping compiled-cache subtrees are not admitted."));I->CompiledCacheRoot=DI;I->CompiledCacheRelative=I->Object.Get()==DI?FString():I->Object->GetPathName(DI);}
  }
 }
 return true;
}
bool FWork::AdmitObjectIdentities(const FString& Stage)
{
 auto IdentityFailure=[&](const FIdentity& I)
 {
  if(!R.Data->HasField(TEXT("first_identity_failure")))R.Data->SetObjectField(TEXT("first_identity_failure"),I.FailureDiagnostic(Stage));
  return Fail(TEXT("niagara_hlsl_conversion_identity_changed"),TEXT("Captured object class/outer/package/path changed; no unproven replacement adopted."));
 };
 TSet<UNiagaraDataInterface*> Pending;
 // First validate every strict identity and owner. No cache or retired raw pin is read to excuse another failure.
 for(const auto& I:Objects)
 {
  if(I->Exact())continue;
  UNiagaraDataInterface* Root=I->CompiledCacheRoot.Get();
  const FCachedDIRole* Role=CompiledCacheRoles.FindByPredicate([&](const FCachedDIRole& E){return E.DI.Get()==Root;});
  if(!DeferredMode||OwnedCompileEpoch==0||!Role||Role->Retired||OwnedCompileEpoch<=Role->CapturedEpoch||!I->BaseExact())return IdentityFailure(*I);
  Pending.Add(Root);
 }
 if(CompiledCacheRoles.IsEmpty())return true;
 for(const auto& Role:CompiledCacheRoles)
 {
  UNiagaraScript* Owner=Role.Owner.Get();
  const FScript* Binding=Scripts.FindByPredicate([&](const FScript& S){return S.Script.Get()==Owner;});
  if(!Live(Owner)||!Binding||Binding->Source!=Role.Source||Binding->Exposed!=Role.Version||Owner->GetLatestSource()!=Role.Source.Get()||Owner->GetExposedVersion().VersionGuid!=Role.Version)
   return Fail(TEXT("niagara_hlsl_conversion_cache_owner_changed"),TEXT("Compiled DI owner Script source/version is not the captured role."));
 }
 auto Membership=[&](const UNiagaraDataInterface* DI,int32& Count)
 {
  Count=0;int32 Visits=0;
  for(const auto& Binding:Scripts)
  {
   const UNiagaraScript* Owner=Binding.Script.Get();if(!Live(Owner))return false;
   const auto Cache=Owner->GetCachedDefaultDataInterfaces();
   if(Cache.Num()>65536||(Visits+=Cache.Num())>262144)return false;
   for(const auto& Row:Cache)Count+=Row.DataInterface==DI?1:0;
  }
  return true;
 };
 TArray<FCachedDIRole> AddedRoles;
 for(auto& Role:CompiledCacheRoles)
 {
  UNiagaraDataInterface* Old=Role.DI.Get();UNiagaraScript* Owner=Role.Owner.Get();
  FIdentity* RootIdentity=nullptr;for(const auto& I:Objects)if(I->Object.Get()==Old){RootIdentity=I.Get();break;}
  if(!RootIdentity||!Live(Old)||!RootIdentity->BaseExact())return Fail(TEXT("niagara_hlsl_conversion_cache_identity_unavailable"),TEXT("Strong captured compiled DI root is no longer live/exact."));
  const auto Cache=static_cast<const UNiagaraScript*>(Owner)->GetCachedDefaultDataInterfaces();
  if(Cache.Num()>65536)return Fail(TEXT("niagara_hlsl_conversion_cache_bound"),TEXT("Compiled DI cache bound exceeded."));
  int32 OldCount=0;if(!Membership(Old,OldCount))return Fail(TEXT("niagara_hlsl_conversion_cache_bound"),TEXT("Current compiled DI membership cannot be bounded."));
  if(Role.Retired)
  {if(OldCount!=0)return Fail(TEXT("niagara_hlsl_conversion_retired_cache_reentered"),TEXT("A retired compiled DI unexpectedly reentered a current cache."));continue;}
  if(!Pending.Contains(Old))
  {
   int32 Matches=0;for(const auto& Row:Cache)if(Role.Matches(Row)&&Row.DataInterface==Old)++Matches;
   if(OldCount!=1||Matches!=1)return Fail(TEXT("niagara_hlsl_conversion_cache_membership_changed"),TEXT("Captured active compiled DI role changed without a proven invalidation transition."));
   continue;
  }
  FName InvalidatedName(*(FString(TEXT("Invalidated_"))+Role.OriginalName.ToString()));
  const FName ActualName=Old->GetFName();
  const EObjectFlags RetiredFlags=RootIdentity->CapturedFlags&~RF_Public;
  if(Old->GetOuter()!=Owner||Old->GetFlags()!=RetiredFlags||ActualName.GetComparisonIndex()!=InvalidatedName.GetComparisonIndex()||ActualName.GetNumber()<InvalidatedName.GetNumber()||!ActualName.GetPlainNameString().Equals(InvalidatedName.GetPlainNameString(),ESearchCase::CaseSensitive)||OldCount!=0)
   return IdentityFailure(*RootIdentity);
  const FNiagaraScriptDataInterfaceInfo* ReplacementRole=nullptr;int32 Matches=0;
  for(const auto& Row:Cache)if(Role.Matches(Row)){++Matches;ReplacementRole=&Row;}
  if(Matches!=1||!ReplacementRole||!ReplacementRole->DataInterface)return IdentityFailure(*RootIdentity);
  UNiagaraDataInterface* Replacement=ReplacementRole->DataInterface.Get();int32 ReplacementCount=0;
  if(Replacement==Old||!Live(Replacement)||!FIdentity::ReadableChain(Replacement)||Replacement->GetClass()!=RootIdentity->Class.Get()||Replacement->GetOuter()!=Owner||Replacement->GetOutermost()!=RootIdentity->Package.Get()||Replacement->GetPackage()!=RootIdentity->Effective.Get()||!Membership(Replacement,ReplacementCount)||ReplacementCount!=1)
   return IdentityFailure(*RootIdentity);
  for(const auto& I:Objects)if(I->CompiledCacheRoot.Get()==Old)
  {
   UObject* O=I->Object.Get();if(!I->BaseExact()||!FIdentity::ReadableChain(O)||(O!=Old&&(!O->IsIn(Old)||!O->GetPathName(Old).Equals(I->CompiledCacheRelative,ESearchCase::CaseSensitive)))||O->GetFlags()!=(O==Old?RetiredFlags:I->CapturedFlags))return IdentityFailure(*I);
  }
  if(CompiledCacheTransitions.Num()>=128||!Budget(1,Role.OriginalPath.Len()+Old->GetPathName().Len()+Replacement->GetPathName().Len()))return false;
  // Retain the original strong holds and exact relative subtree, then hold the new current cache object.
  for(const auto& I:Objects)if(I->CompiledCacheRoot.Get()==Old)
  {I->CompiledCacheRetired=true;I->RetiredCachePath=I->Object->GetPathName();I->RetiredCacheFlags=I->Object->GetFlags();}
  Role.Retired=true;
  FIdentity* NewIdentity=nullptr;for(const auto& I:Objects)if(I->Object.Get()==Replacement){NewIdentity=I.Get();break;}
  if(NewIdentity){if(!NewIdentity->Exact())return IdentityFailure(*NewIdentity);}
  else{Objects.Add(MakeUnique<FIdentity>(Replacement));NewIdentity=Objects.Last().Get();}
  NewIdentity->CompiledCacheRoot=Replacement;NewIdentity->CompiledCacheRelative.Reset();
  FCachedDIRole Next=Role;Next.DI=Replacement;Next.Info=*ReplacementRole;Next.CapturedEpoch=OwnedCompileEpoch;Next.OriginalName=Replacement->GetFName();Next.OriginalPath=Replacement->GetPathName();Next.Retired=false;AddedRoles.Add(MoveTemp(Next));
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("provenance"),TEXT("owned_compile_cached_default_DI_invalidation_and_role_replacement"));Row->SetStringField(TEXT("stage"),Stage.Left(256));Row->SetNumberField(TEXT("owned_compile_epoch"),static_cast<double>(OwnedCompileEpoch));Row->SetStringField(TEXT("owner_script"),Owner->GetPathName().Left(1024));Row->SetStringField(TEXT("role_name"),Role.Info.Name.ToString().Left(1024));Row->SetStringField(TEXT("compile_name"),Role.Info.CompileName.ToString().Left(1024));Row->SetStringField(TEXT("previous_path"),Role.OriginalPath.Left(1024));Row->SetStringField(TEXT("retired_path"),Old->GetPathName().Left(1024));Row->SetStringField(TEXT("current_cache_path"),Replacement->GetPathName().Left(1024));Row->SetBoolField(TEXT("old_current_cache_membership_absent"),true);Row->SetBoolField(TEXT("new_exact_role_membership_unique"),true);Row->SetBoolField(TEXT("original_strong_hold_retained"),true);
  CompiledCacheTransitions.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("compiled_cache_DI_transitions"),CompiledCacheTransitions);
 }
 CompiledCacheRoles.Append(MoveTemp(AddedRoles));
 // Only the recorded cache-role retirement may differ from the initial pathname.
 for(const auto& I:Objects)if(!I->Exact())return IdentityFailure(*I);
 return true;
}

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
bool IsNiagaraDynamicAddPin(const UEdGraphPin* Pin)
{
    return Pin &&
        Pin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc &&
        Pin->PinType.PinSubCategory == TEXT("DynamicAddPin");
}

TSharedPtr<FJsonObject> ErrorResponse(const FString& Message){return Current->Error(Message);}
TSharedPtr<FJsonObject> MakeNiagaraGraphRecipeError(const FString& Message,const FString& Code){Current->Fail(*Code,Message);auto O=ErrorResponse(Message);O->SetStringField(TEXT("error_code"),Code);O->SetBoolField(TEXT("saved"),Current->R.bSaved);O->SetBoolField(TEXT("mutation_performed"),Current->TargetChanged);O->SetBoolField(TEXT("mutation_applied_in_memory"),Current->TargetChanged);return O;}
FString NiagaraObjectPathOrEmpty(const UObject* O){return O?O->GetPathName():FString();}
UNiagaraSystem* LoadSystem(const FString& Path)
{
 auto& W=*Current;const FString Exact=NormalizeNiagaraObjectPathForLoad(Path);if(Exact.Len()>=NAME_SIZE){W.Fail(TEXT("niagara_hlsl_conversion_name_size"),TEXT("Expanded System object path exceeds installed FName NAME_SIZE."));return nullptr;}UNiagaraSystem* O=nullptr;if(!W.Step(TEXT("LoadObject_System"),[&]{O=LoadObject<UNiagaraSystem>(nullptr,*Exact);}))return nullptr;if(!Live(O)){W.Fail(TEXT("niagara_hlsl_conversion_system_missing"),TEXT("Expected live Niagara System"));return nullptr;}W.Target=O;W.Source=O;W.SourceDirty=O->GetOutermost()->IsDirty();W.SourceCaptured=true;if(!W.System(O,O->GetEmitterHandles().Num()))return nullptr;return O;
}
FString GetNiagaraCustomHlslString(const UNiagaraNodeCustomHlsl* Node)
{
 auto& W=*Current;if(!W.Admit(TEXT("HLSL_read"))||!Live(Node)||Node->GetClass()!=UNiagaraNodeCustomHlsl::StaticClass())return FString();FProperty* P=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));if(!CastField<FStrProperty>(P)||P->ArrayDim!=1||EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(P),EPropertyAccessResultFlags::PermissionDenied)){W.Fail(TEXT("niagara_hlsl_conversion_property_denied"),TEXT("Exact public readable CustomHlsl property required"));return FString();}FString Encoded;if(!W.Step(TEXT("GetObjectProperties_CustomHlsl"),[&]{Encoded=UToolsetLibrary::GetObjectProperties(Node,{FName(TEXT("CustomHlsl"))});}))return FString();TSharedPtr<FJsonObject> O;FString Body;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Encoded),O)||!O||!O->TryGetStringField(TEXT("CustomHlsl"),Body)){W.Fail(TEXT("niagara_hlsl_conversion_property_read_failed"),TEXT("Typed CustomHlsl readback unavailable"));return FString();}if(!W.Budget(0,Body.Len()))return FString();return Body;
}
TArray<TSharedPtr<FJsonValue>> GraphPinsToJson(const TArray<UEdGraphPin*>& Pins)
{
 TArray<TSharedPtr<FJsonValue>> Out;auto& W=*Current;if(!W.Admit(TEXT("pin_projection")))return Out;for(auto* P:Pins){if(!P)continue;auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),P->PinName.ToString());O->SetStringField(TEXT("pin_id"),P->PinId.ToString(EGuidFormats::DigitsWithHyphens));O->SetStringField(TEXT("type"),NiagaraTypeName(UEdGraphSchema_Niagara::PinToTypeDefinition(P)));O->SetStringField(TEXT("default_value"),P->DefaultValue);O->SetNumberField(TEXT("link_count"),P->LinkedTo.Num());Out.Add(MakeShared<FJsonValueObject>(O));}return Out;
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
        if(!Current->Graph(Source))return;
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
    if(!ContinueRead())return NodeObject;
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
    if(!ContinueRead())return false;
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
bool PackageOwnership(FWork& W,UNiagaraSystem* System);

bool PinHeaderEqual(const FPin& E,const UEdGraphPin* P)
{return P&&P->PinId==E.Id&&P->PersistentGuid==E.Persistent&&P->PinName==E.Name&&P->Direction==E.Direction&&P->PinType==E.Type&&P->bNotConnectable==E.NotConnectable&&P->bDefaultValueIsReadOnly==E.DefaultReadOnly&&P->bOrphanedPin==E.Orphaned&&P->DefaultObject==E.DefaultObject.Get()&&P->DefaultValue.Equals(E.Default,ESearchCase::CaseSensitive)&&P->AutogeneratedDefaultValue.Equals(E.AutoDefault,ESearchCase::CaseSensitive)&&P->DefaultTextValue.ToString().Equals(E.DefaultText,ESearchCase::CaseSensitive);}
bool ReconcileHlslPins(FWork& W,UNiagaraGraph* G,UNiagaraNodeCustomHlsl* Node,const FGraph& Before)
{
 if(!W.Admit(TEXT("HLSL_reconstruction_context")))return false;const FNode* Old=Before.Nodes.FindByPredicate([&](const FNode& N){return N.Node.Get()==Node;});if(!Old||Node->Pins.Num()!=Old->Pins.Num())return W.Fail(TEXT("niagara_hlsl_conversion_pin_shape_changed"),TEXT("HLSL body setter changed physical pin count"));
 TMap<UEdGraphPin*,UEdGraphPin*> Map;TSet<UEdGraphPin*> Chosen;for(const auto& E:Old->Pins){UEdGraphPin* Selected=nullptr;int32 Matches=0;for(auto* P:Node->Pins){if(!P||P->GetOwningNodeUnchecked()!=Node)continue;if(P->PinId==E.Id&&P->PersistentGuid==E.Persistent&&PinHeaderEqual(E,P)){++Matches;Selected=P;}}if(Matches!=1||Chosen.Contains(Selected))return W.Fail(TEXT("niagara_hlsl_conversion_pin_identity_unresolved"),TEXT("Each captured old HLSL value pin must map to exactly one current typed pin with preserved ID/default/header"));Chosen.Add(Selected);Map.Add(E.P,Selected);}
 FGraph Expected=Before;for(auto& N:Expected.Nodes){auto* Owner=N.Node.Get();if(!Live(Owner)||Owner->Pins.Num()!=N.Pins.Num())return W.Fail(TEXT("niagara_hlsl_conversion_external_pin_changed"),TEXT("Captured external node pin count changed"));for(int32 I=0;I<N.Pins.Num();++I){auto& E=N.Pins[I];UEdGraphPin* Actual=E.P;if(auto* Found=Map.Find(E.P))Actual=*Found; // Never dereference a retired old raw pin.
 if(Owner==Node){if(!Node->Pins.Contains(Actual))return false;E.P=Actual;}else if(Owner->Pins[I]!=Actual||!PinHeaderEqual(E,Actual))return W.Fail(TEXT("niagara_hlsl_conversion_external_pin_changed"),TEXT("Exact external endpoint header/member changed"));
 TArray<UEdGraphPin*> Links;for(auto* Link:E.Links){auto* Found=Map.Find(Link);Links.Add(Found?*Found:Link);}if(Actual->LinkedTo!=Links)return W.Fail(TEXT("niagara_hlsl_conversion_link_reconstruction_failed"),TEXT("Reconstructed HLSL or external reciprocal ordered links differ from captured exact semantic endpoints"));E.Links=MoveTemp(Links);}}
 auto* Stored=W.Graphs.FindByPredicate([&](const FGraph& E){return E.Graph.Get()==G;});if(!Stored)return false;*Stored=MoveTemp(Expected);W.R.Data->SetNumberField(TEXT("HLSL_reconstructed_pin_count"),Map.Num());return true;
}
bool WritableGraph(FWork& W,UNiagaraScriptSource* Selected,UNiagaraNodeCustomHlsl* Node)
{
 if(!W.Admit(TEXT("writer_owner")))return false;auto* S=W.Target.Get();auto* G=Selected?Selected->NodeGraph.Get():nullptr;if(!Live(S)||!Live(Selected)||!Live(G)||!Live(Node)||Node->GetOuter()!=G||G->GetOuter()!=Selected||Selected->NodeGraph!=G||!Selected->IsIn(S)||!G->IsIn(S)||Selected->GetPackage()!=S->GetPackage()||G->GetPackage()!=S->GetPackage()||Node->GetPackage()!=S->GetPackage())return W.Fail(TEXT("niagara_hlsl_conversion_external_source_unsupported"),TEXT("Selected source/graph/node must be inside exact System package before writer"));
 int32 ScratchRoles=0,RegularRoles=0;TSet<UNiagaraScriptSource*> Distinct;auto SameGraph=[&](UNiagaraScriptSourceBase* Base){auto* Source=Cast<UNiagaraScriptSource>(Base);if(Source&&Source->NodeGraph==G){Distinct.Add(Source);return true;}return false;};
 for(const auto& Root:W.Systems){const bool Spawn=SameGraph(Root.Spawn->GetLatestSource()),Update=SameGraph(Root.Update->GetLatestSource());if(Spawn||Update)++RegularRoles;for(const auto& P:Root.Scratch)if(P.IsValid()&&SameGraph(P->GetLatestSource()))++ScratchRoles;}
 for(const auto& E:W.Emitters){if(SameGraph(E.Source.Get()))++RegularRoles;for(const auto& P:E.ScratchScripts)if(P.IsValid()&&SameGraph(P->GetLatestSource()))++ScratchRoles;for(const auto& P:E.ParentScripts)if(P.IsValid()&&SameGraph(P->GetLatestSource()))++ScratchRoles;}
 if(Distinct.Num()!=1||ScratchRoles>1||(ScratchRoles&&RegularRoles)||RegularRoles>1)return W.Fail(TEXT("niagara_hlsl_conversion_shared_source_unsupported"),TEXT("Selected mutable graph is shared across scratch/emitter/system owner roles; allow_source_edit does not authorize foreign side effects"));return true;
}
bool SetNiagaraCustomHlslString(UNiagaraNodeCustomHlsl* Node,const FString& Body)
{
 auto& W=*Current;auto* G=Cast<UNiagaraGraph>(Node->GetOuter());auto* Source=G?Cast<UNiagaraScriptSource>(G->GetOuter()):nullptr;if(!WritableGraph(W,Source,Node))return false;
 FProperty* Field=FindFProperty<FProperty>(UNiagaraNodeCustomHlsl::StaticClass(),TEXT("CustomHlsl"));if(!CastField<FStrProperty>(Field)||Field->ArrayDim!=1||EnumHasAnyFlags(PropertyAccessUtil::CanGetPropertyValue(Field),EPropertyAccessResultFlags::PermissionDenied)||EnumHasAnyFlags(PropertyAccessUtil::CanSetPropertyValue(Field,PropertyAccessUtil::EditorReadOnlyFlags,false),EPropertyAccessResultFlags::PermissionDenied))return W.Fail(TEXT("niagara_hlsl_conversion_property_write_denied"),TEXT("Public editor property permission denied"));
 if(!PackageOwnership(W,W.Target.Get()))return false;
 const FGraph* Existing=W.Graphs.FindByPredicate([&](const FGraph& E){return E.Graph.Get()==G;});if(!Existing)return false;const FGraph Before=*Existing;auto Object=MakeShared<FJsonObject>();Object->SetStringField(TEXT("CustomHlsl"),Body);TArray<FName> Written;bool Returned=false;
 if(!W.Step(TEXT("HLSL_Node_Modify"),[&]{Node->Modify();},true,true))return false;if(!W.SDK(TEXT("SetObjectProperties_CustomHlsl"),true,true))return false;
 {TGuardValue<UNiagaraNodeCustomHlsl*> Transition(W.TransitionNode,Node);Returned=UToolsetLibrary::SetObjectProperties(Node,Json(Object),Written,EBypassContainerCheck::No);W.R.Data->SetBoolField(TEXT("HLSL_property_set_returned"),Returned);W.TargetChanged|=Returned;
 if(!W.After(TEXT("SetObjectProperties_CustomHlsl"))||!ReconcileHlslPins(W,G,Node,Before))return false;W.Custom.FindChecked(Node).Body=Body;}
 if(!W.Admit(TEXT("HLSL_property_transition_complete")))return false;if(!Returned||!Written.Contains(FName(TEXT("CustomHlsl"))))return W.Fail(TEXT("niagara_hlsl_conversion_property_write_failed"),TEXT("Public HLSL setter did not report target property written"));
 if(!W.Step(TEXT("MarkNodeRequiresSynchronization"),[&]{Node->MarkNodeRequiresSynchronization(TEXT("Cubeless: replace custom hlsl"),true);},true,true))return false;const FString Actual=GetNiagaraCustomHlslString(Node);return ContinueRead()&&Actual.Equals(Body,ESearchCase::CaseSensitive)||W.Fail(TEXT("niagara_hlsl_conversion_body_readback_failed"),TEXT("Exact CaseSensitive HLSL readback differs"));
}

struct FNiagaraScriptCompileSnapshot
{
    TObjectPtr<UNiagaraScript> Script = nullptr;
    FString OwnerKind;
    FString OwnerName;
    FString ScriptPath;
    FNiagaraVMExecutableDataId CachedId;
    FNiagaraVMExecutableDataId ComputedId;
    ENiagaraScriptCompileStatus CompileStatus = ENiagaraScriptCompileStatus::NCS_Unknown;
};


FString NiagaraCompileIdFingerprint(const FNiagaraVMExecutableDataId& CompileId)
{
    if (!CompileId.IsValid())
    {
        return FString();
    }

    FString KeyString;
    CompileId.AppendKeyString(KeyString);
    return FMD5::HashAnsiString(*KeyString);
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
void AddNiagaraScriptCompileSnapshot(
    UNiagaraScript* Script,
    const FString& OwnerKind,
    const FString& OwnerName,
    TSet<const UNiagaraScript*>& SeenScripts,
    TArray<FNiagaraScriptCompileSnapshot>& OutSnapshots)
{
    if(!ContinueRead())return;
    if (!Script || SeenScripts.Contains(Script))
    {
        return;
    }

    if(!Current->Pin(Script))return;
    SeenScripts.Add(Script);
    FNiagaraScriptCompileSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
    Snapshot.Script = Script;
    Snapshot.OwnerKind = OwnerKind;
    Snapshot.OwnerName = OwnerName;
    Snapshot.ScriptPath = Script->GetPathName();
    Snapshot.CachedId = Script->GetVMExecutableDataCompilationId();
    if(!Current->Step(TEXT("ComputeVMCompilationId"),[&]{Script->ComputeVMCompilationId(Snapshot.ComputedId,FGuid());}))return;
    Snapshot.CompileStatus = Script->GetLastCompileStatus();
}
TArray<FNiagaraScriptCompileSnapshot> CaptureNiagaraSystemCompileSnapshots(UNiagaraSystem* System)
{
 TArray<FNiagaraScriptCompileSnapshot> Out;auto& W=*Current;if(!W.Admit(TEXT("compile_snapshot_entry"))||System!=W.Target.Get())return Out;TSet<const UNiagaraScript*> Seen;
 AddNiagaraScriptCompileSnapshot(System->GetSystemSpawnScript(),TEXT("system"),TEXT("system_spawn"),Seen,Out);if(!ContinueRead())return Out;
 AddNiagaraScriptCompileSnapshot(System->GetSystemUpdateScript(),TEXT("system"),TEXT("system_update"),Seen,Out);if(!ContinueRead())return Out;
 int32 I=0;for(const auto& E:W.Emitters){if(!ContinueRead())return Out;TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);const FString Owner=FString::Printf(TEXT("emitter:%d"),I++);for(auto* S:A){AddNiagaraScriptCompileSnapshot(S,Owner,E.UniqueName,Seen,Out);if(!ContinueRead())return Out;}}
 return Out;
}
const FNiagaraScriptCompileSnapshot* FindNiagaraScriptCompileSnapshot(
    const TArray<FNiagaraScriptCompileSnapshot>& Snapshots,
    const FString& ScriptPath)
{
    return Snapshots.FindByPredicate(
        [&ScriptPath](const FNiagaraScriptCompileSnapshot& Snapshot)
        {
            return Snapshot.ScriptPath == ScriptPath;
        });
}
TSharedPtr<FJsonObject> BuildNiagaraScriptCompileSnapshotJson(
    const FNiagaraScriptCompileSnapshot& Snapshot,
    int32 ScriptIndex)
{
    TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
    ScriptObject->SetNumberField(TEXT("script_index"), ScriptIndex);
    ScriptObject->SetStringField(TEXT("owner_kind"), Snapshot.OwnerKind);
    ScriptObject->SetStringField(TEXT("owner_name"), Snapshot.OwnerName);
    ScriptObject->SetStringField(TEXT("script_path"), Snapshot.ScriptPath);
    ScriptObject->SetStringField(TEXT("usage"), Snapshot.Script ? NiagaraScriptUsageName(Snapshot.Script->GetUsage()) : FString());
    ScriptObject->SetStringField(TEXT("compile_status"), NiagaraCompileStatusName(Snapshot.CompileStatus));
    ScriptObject->SetBoolField(TEXT("has_error"), IsNiagaraCompileErrorStatus(Snapshot.CompileStatus));
    ScriptObject->SetBoolField(TEXT("has_warning"), IsNiagaraCompileWarningStatus(Snapshot.CompileStatus));
    ScriptObject->SetBoolField(TEXT("cached_id_valid"), Snapshot.CachedId.IsValid());
    ScriptObject->SetBoolField(TEXT("computed_id_valid"), Snapshot.ComputedId.IsValid());
    ScriptObject->SetStringField(TEXT("cached_id_fingerprint"), NiagaraCompileIdFingerprint(Snapshot.CachedId));
    ScriptObject->SetStringField(TEXT("computed_id_fingerprint"), NiagaraCompileIdFingerprint(Snapshot.ComputedId));
    ScriptObject->SetBoolField(
        TEXT("source_synchronized"),
        Snapshot.CachedId.IsValid() && Snapshot.ComputedId.IsValid() && Snapshot.CachedId == Snapshot.ComputedId);
    return ScriptObject;
}
constexpr double NiagaraGraphRecipeMaxAbsNodeCoordinate=1000000.0;
constexpr const TCHAR* NiagaraRecipeScratchPadReferenceScanMode=TEXT("direct_enabled_link_traversal_no_compile_time_selectors");
struct FResolvedSystemScratchPadGraph{UNiagaraSystem* System=nullptr;UNiagaraScript* Script=nullptr;UNiagaraScriptSource* Source=nullptr;UNiagaraGraph* Graph=nullptr;int32 ScriptIndex=INDEX_NONE;};
struct FNiagaraRecipeScratchPadReferenceScan
{
    int32 ReferenceCount = 0;
    bool bComplete = true;
    TSet<UNiagaraScript*> CallerScripts;
    TArray<FString> UnresolvedOutputContexts;
};

using FNiagaraRecipeCallerResolver =
    TFunction<UNiagaraScript*(ENiagaraScriptUsage, const FGuid&)>;


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

    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        OutError = FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath);
        return false;
    }

    if(!ContinueRead())return false;
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

    if(!ContinueRead()||!Current->Script(MatchedScript)||!Current->Graph(Source))return false;
    OutResolved.System = System;
    OutResolved.Script = MatchedScript;
    OutResolved.Source = Source;
    OutResolved.Graph = Graph;
    OutResolved.ScriptIndex = MatchedIndex;
    return true;
}
bool IsNiagaraRecipeCompileTimeSelectorBarrier(const UClass* NodeClass)
{
    static const TSet<FName> BarrierClassNames = {
        FName(TEXT("NiagaraNodeStaticSwitch")),
        FName(TEXT("NiagaraNodeUsageSelector")),
        FName(TEXT("NiagaraNodeSimTargetSelector")),
        FName(TEXT("NiagaraNodeSelect")),
    };
    for (const UClass* Class = NodeClass; Class; Class = Class->GetSuperClass())
    {
        if (BarrierClassNames.Contains(Class->GetFName()))
        {
            return true;
        }
    }
    return false;
}
void ScanNiagaraRecipeScratchPadReferencesInSource(
    const UNiagaraScriptSourceBase* SourceBase,
    const UNiagaraScript* TargetScript,
    TSet<FString>& SeenGraphOwnerContexts,
    const FString& OwnerContext,
    const FNiagaraRecipeCallerResolver& ResolveCaller,
    FNiagaraRecipeScratchPadReferenceScan& OutScan)
{
    if(!ContinueRead()){OutScan.bComplete=false;return;}
    if (!TargetScript)
    {
        OutScan.bComplete = false;
        OutScan.UnresolvedOutputContexts.AddUnique(
            FString::Printf(TEXT("%s|missing_target_script"), *OwnerContext));
        return;
    }
    const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SourceBase);
    const UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    if (!Graph)
    {
        OutScan.bComplete = false;
        OutScan.UnresolvedOutputContexts.AddUnique(
            FString::Printf(TEXT("%s|missing_or_unsupported_graph_source"), *OwnerContext));
        return;
    }
    const FString GraphOwnerContext = Graph->GetPathName() + TEXT("|") + OwnerContext;
    if (SeenGraphOwnerContexts.Contains(GraphOwnerContext))
    {
        return;
    }
    SeenGraphOwnerContexts.Add(GraphOwnerContext);

    const FString TargetPath = TargetScript->GetPathName();
    TArray<UNiagaraNodeOutput*> OutputNodes;
    for (UEdGraphNode* GraphNode : Graph->Nodes)
    {
        if (UNiagaraNodeOutput* OutputNode = Cast<UNiagaraNodeOutput>(GraphNode))
        {
            OutputNodes.Add(OutputNode);
        }
    }
    for (UNiagaraNodeOutput* OutputNode : OutputNodes)
    {
        if (!OutputNode || !OutputNode->IsNodeEnabled())
        {
            continue;
        }

        TSet<const UEdGraphNode*> SeenReachableNodes;
        TArray<const UEdGraphNode*> PendingNodes;
        PendingNodes.Add(OutputNode);
        while (!PendingNodes.IsEmpty())
        {
            const UEdGraphNode* TraversalNode = PendingNodes.Pop(EAllowShrinking::No);
            if (!TraversalNode || SeenReachableNodes.Contains(TraversalNode))
            {
                continue;
            }
            if(!Current->Budget(1)){OutScan.bComplete=false;return;}
            SeenReachableNodes.Add(TraversalNode);

            const UNiagaraNode* NiagaraNode = Cast<UNiagaraNode>(TraversalNode);
            if (!NiagaraNode || !NiagaraNode->IsNodeEnabled())
            {
                continue;
            }
            if (IsNiagaraRecipeCompileTimeSelectorBarrier(NiagaraNode->GetClass()))
            {
                OutScan.bComplete = false;
                OutScan.UnresolvedOutputContexts.AddUnique(FString::Printf(
                    TEXT("%s|usage=%s|usage_id=%s|compile_time_selector=%s"),
                    *OwnerContext,
                    *NiagaraScriptUsageName(OutputNode->GetUsage()),
                    *OutputNode->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens),
                    *NiagaraNode->GetClass()->GetName()));
                continue;
            }

            const UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(TraversalNode);
            if (FunctionCall)
            {
                const FString ReferencedPath = NiagaraObjectPathOrEmpty(FunctionCall->FunctionScript);
                if (FunctionCall->FunctionScript == TargetScript ||
                    ReferencedPath.Equals(TargetPath, ESearchCase::IgnoreCase))
                {
                    UNiagaraScript* CallerScript = ResolveCaller
                        ? ResolveCaller(OutputNode->GetUsage(), OutputNode->GetUsageId())
                        : nullptr;
                    if (CallerScript)
                    {
                        ++OutScan.ReferenceCount;
                        OutScan.CallerScripts.Add(CallerScript);
                    }
                    else
                    {
                        OutScan.bComplete = false;
                        OutScan.UnresolvedOutputContexts.AddUnique(FString::Printf(
                            TEXT("%s|usage=%s|usage_id=%s"),
                            *OwnerContext,
                            *NiagaraScriptUsageName(OutputNode->GetUsage()),
                            *OutputNode->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens)));
                    }
                }
            }

            for (const UEdGraphPin* InputPin : TraversalNode->Pins)
            {
                if (!InputPin || InputPin->Direction != EGPD_Input)
                {
                    continue;
                }
                for (const UEdGraphPin* LinkedPin : InputPin->LinkedTo)
                {
                    if (LinkedPin && LinkedPin->GetOwningNode())
                    {
                        PendingNodes.Add(LinkedPin->GetOwningNode());
                    }
                }
            }
        }
    }
}
FNiagaraRecipeScratchPadReferenceScan ScanNiagaraRecipeScratchPadSystemReferences(
    UNiagaraSystem* System,
    const UNiagaraScript* TargetScript)
{
    FNiagaraRecipeScratchPadReferenceScan Scan;
    if(!ContinueRead()){Scan.bComplete=false;return Scan;}
    if (!System || !TargetScript)
    {
        Scan.bComplete = false;
        return Scan;
    }

    TSet<FString> SeenGraphOwnerContexts;
    const FNiagaraRecipeCallerResolver ResolveSystemCaller = [System](
        ENiagaraScriptUsage Usage,
        const FGuid& UsageId) -> UNiagaraScript*
    {
        UNiagaraScript* SystemSpawn = System->GetSystemSpawnScript();
        if (SystemSpawn && SystemSpawn->GetUsage() == Usage && SystemSpawn->GetUsageId() == UsageId)
        {
            return SystemSpawn;
        }
        UNiagaraScript* SystemUpdate = System->GetSystemUpdateScript();
        if (SystemUpdate && SystemUpdate->GetUsage() == Usage && SystemUpdate->GetUsageId() == UsageId)
        {
            return SystemUpdate;
        }
        return nullptr;
    };

    UNiagaraScript* SystemSpawn = System->GetSystemSpawnScript();
    if (SystemSpawn)
    {
        ScanNiagaraRecipeScratchPadReferencesInSource(
            SystemSpawn->GetLatestSource(),
            TargetScript,
            SeenGraphOwnerContexts,
            TEXT("system"),
            ResolveSystemCaller,
            Scan);
    }
    if (const UNiagaraScript* SystemUpdate = System->GetSystemUpdateScript())
    {
        ScanNiagaraRecipeScratchPadReferencesInSource(
            SystemUpdate->GetLatestSource(),
            TargetScript,
            SeenGraphOwnerContexts,
            TEXT("system"),
            ResolveSystemCaller,
            Scan);
    }
    int32 EmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        if (!EmitterHandle.GetIsEnabled())
        {
            ++EmitterIndex;
            continue;
        }
        if (EmitterHandle.GetEmitterMode() == ENiagaraEmitterMode::Stateless)
        {
            ++EmitterIndex;
            continue;
        }
        const FString EmitterOwnerContext = FString::Printf(
            TEXT("emitter:%d:%s"),
            EmitterIndex,
            *EmitterHandle.GetName().ToString());
        const FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
        if (!EmitterData)
        {
            Scan.bComplete = false;
            Scan.UnresolvedOutputContexts.AddUnique(
                EmitterOwnerContext + TEXT("|missing_standard_emitter_data"));
            ++EmitterIndex;
            continue;
        }
        {
            const FNiagaraRecipeCallerResolver ResolveEmitterCaller = [EmitterData](
                ENiagaraScriptUsage Usage,
                const FGuid& UsageId) -> UNiagaraScript*
            {
                return EmitterData->GetScript(Usage, UsageId);
            };
            ScanNiagaraRecipeScratchPadReferencesInSource(
                EmitterData->GraphSource,
                TargetScript,
                SeenGraphOwnerContexts,
                EmitterOwnerContext,
                ResolveEmitterCaller,
                Scan);
        }
        ++EmitterIndex;
    }
    Scan.UnresolvedOutputContexts.Sort();
    return Scan;
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
            !IsMCPDependencyReference(FunctionScriptPath);
    }
    return false;
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
UEdGraphPin* FindNiagaraRecipePin(
    UEdGraphNode* Node,
    EEdGraphPinDirection Direction,
    const FString& PinName,
    int32 PinOccurrence = 0)
{
    if (!Node || PinOccurrence < 0)
    {
        return nullptr;
    }

    int32 MatchIndex = 0;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->Direction != Direction || IsNiagaraDynamicAddPin(Pin))
        {
            continue;
        }
        const bool bMatches = PinName.IsEmpty() || Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase);
        if (bMatches && MatchIndex++ == PinOccurrence)
        {
            return Pin;
        }
    }
    return nullptr;
}
FString NiagaraRecipePinIdentity(const UEdGraphPin* Pin)
{
    if (!Pin)
    {
        return TEXT("null_pin");
    }

    const UEdGraphNode* Node = Pin->GetOwningNodeUnchecked();
    const FGuid StablePinGuid = Pin->PersistentGuid.IsValid() ? Pin->PersistentGuid : Pin->PinId;
    return FString::Printf(
        TEXT("%s|%s|%s|%d"),
        Node ? *Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : TEXT("null_node"),
        *StablePinGuid.ToString(EGuidFormats::DigitsWithHyphens),
        *Pin->PinName.ToString(),
        static_cast<int32>(Pin->Direction));
}

TArray<TSharedPtr<FNiagaraAction_NewNode>> CollectNiagaraGraphActions(UNiagaraGraph* Graph,UNiagaraGraph*& OutTemporaryOwner)
{
 auto& W=*Current;OutTemporaryOwner=nullptr;TArray<TSharedPtr<FNiagaraAction_NewNode>> A;if(!W.Step(TEXT("NewObject_ActionOwner"),[&]{OutTemporaryOwner=NewObject<UNiagaraGraph>(GetTransientPackage(),NAME_None,RF_Transient);W.R.Data->SetBoolField(TEXT("transient_creation_attempted"),true);W.R.Data->SetBoolField(TEXT("transient_creation_returned"),OutTemporaryOwner!=nullptr);},true,false))return A;if(!OutTemporaryOwner||!W.Pin(OutTemporaryOwner))return A;const auto* Schema=GetDefault<UEdGraphSchema_Niagara>();if(!Schema){W.Fail(TEXT("niagara_hlsl_conversion_schema_unavailable"),TEXT("Public Niagara schema unavailable"));return A;}if(!W.Step(TEXT("GetGraphActions"),[&]{A=Schema->GetGraphActions(Graph,nullptr,OutTemporaryOwner);}))return A;if(A.Num()>16384||!W.Budget(A.Num())){W.Fail(TEXT("niagara_hlsl_conversion_action_bound"),TEXT("Action inventory exceeds bound"));return {};}for(const auto& Item:A){if(!Item.IsValid())continue;auto* N=Item->WeakNodeTemplate.Get();if(N&&!W.Pin(N))return {};if(!W.Budget(0,Item->DisplayName.ToString().Len()+Item->ToolTip.ToString().Len()))return {};}return A;
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

    FAssetRegistryModule* Registry=nullptr;if(!W.Step(TEXT("LoadModule_AssetRegistry"),[&]{Registry=FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));}))return AuditObject;if(!Registry){W.Fail(TEXT("niagara_hlsl_conversion_registry_unavailable"),TEXT("Public AssetRegistry module unavailable."));return AuditObject;}
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
        if(Dependencies.Num()>65536||!W.Budget(Dependencies.Num())){W.Fail(TEXT("niagara_hlsl_conversion_dependency_bound"),TEXT("Direct dependency output exceeds bounded collection; partial audit retained."));return AuditObject;}
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
struct FNiagaraRecipeSerializedReferenceScan
{
    bool bComplete = true;
    int32 GraphNodeSlotCount = 0;
    int32 SerializedNodeCount = 0;
    int32 PinSlotCount = 0;
    int32 AuditedPinCount = 0;
    int32 SerializedObjectCount = 0;
    int32 ReferenceFinderNodeCount = 0;
    int32 NiagaraTypeDefinitionPinScanCount = 0;
    int32 DynamicAddPinSkipCount = 0;
    TSet<FString> SerializedHardObjectPaths;
    TSet<FString> SerializedSoftObjectPaths;
    TSet<FString> SerializedWeakObjectPaths;
    TSet<FString> SerializedExportClassObjectPaths;
    TSet<FString> ReferenceFinderHardObjectPaths;
    TSet<FString> PinSubCategoryObjectPaths;
    TSet<FString> PinTerminalSubCategoryObjectPaths;
    TSet<FString> NiagaraPinTypeDefinitionObjectPaths;
    TArray<FString> Failures;
};

class FNiagaraRecipeSerializedReferenceArchive final : public FArchiveUObject
{
public:
    FNiagaraRecipeSerializedReferenceArchive(
        const FString& InContext,
        TSet<FString>& InHardObjectPaths,
        TSet<FString>& InSoftObjectPaths,
        TSet<FString>& InWeakObjectPaths,
        TArray<FString>& InFailures)
        : Context(InContext)
        , HardObjectPaths(InHardObjectPaths)
        , SoftObjectPaths(InSoftObjectPaths)
        , WeakObjectPaths(InWeakObjectPaths)
        , Failures(InFailures)
    {
        // Match the save-time reference harvesting contract used by UE 5.6's
        // package collector so transient / skipped properties are not treated
        // as persisted dependencies.  This archive is strictly read-only:
        // ArIsModifyingWeakAndStrongReferences would write resolved/null weak
        // references back into the inspected object during a dry-run audit.
        ArIsObjectReferenceCollector = true;
        ArIsModifyingWeakAndStrongReferences = false;
        ArShouldSkipBulkData = true;
        SetIsSaving(true);
        SetIsPersistent(true);
    }

    using FArchiveUObject::operator<<;

    virtual FArchive& operator<<(UObject*& Object) override
    {
        AddHardObject(Object);
        return *this;
    }

    virtual FArchive& operator<<(FObjectPtr& Object) override
    {
        AddHardObject(Object.Get());
        return *this;
    }

    virtual FArchive& operator<<(FSoftObjectPtr& Object) override
    {
        AddSoftObjectPath(Object.GetUniqueID());
        return *this;
    }

    virtual FArchive& operator<<(FSoftObjectPath& ObjectPath) override
    {
        AddSoftObjectPath(ObjectPath);
        return *this;
    }

    virtual FArchive& operator<<(FWeakObjectPtr& Object) override
    {
        // Resolve for inspection only.  Never assign back into Object.
        AddWeakObject(Object.Get(true));
        return *this;
    }

private:
    void AddHardObject(UObject* Object)
    {
        if(!ContinueRead()){Failures.AddUnique(Context+TEXT("|unsafe_context"));return;}
        if (!Object)
        {
            return;
        }
        if(!Live(Object)){Failures.AddUnique(Context+TEXT("|object_unavailable"));return;}
        const FString ObjectPath = Object->GetPathName();
        if (ObjectPath.IsEmpty())
        {
            Failures.AddUnique(Context + TEXT("|hard_reference_without_path"));
            return;
        }
        if(!Current->Budget(1,ObjectPath.Len()))return;
        HardObjectPaths.Add(ObjectPath);
    }

    void AddSoftObjectPath(const FSoftObjectPath& ObjectPath)
    {
        if (ObjectPath.IsNull())
        {
            return;
        }
        const FString PathString = ObjectPath.ToString();
        if (PathString.IsEmpty())
        {
            Failures.AddUnique(Context + TEXT("|soft_reference_without_path"));
            return;
        }
        if(!ContinueRead()||!Current->Budget(1,PathString.Len()))return;
        SoftObjectPaths.Add(PathString);
    }

    void AddWeakObject(UObject* Object)
    {
        if(!ContinueRead()){Failures.AddUnique(Context+TEXT("|unsafe_context"));return;}
        if (!Object)
        {
            return;
        }
        if(!Live(Object)){Failures.AddUnique(Context+TEXT("|object_unavailable"));return;}
        const FString ObjectPath = Object->GetPathName();
        if (ObjectPath.IsEmpty())
        {
            Failures.AddUnique(Context + TEXT("|weak_reference_without_path"));
            return;
        }
        if(!Current->Budget(1,ObjectPath.Len()))return;
        WeakObjectPaths.Add(ObjectPath);
    }

    FString Context;
    TSet<FString>& HardObjectPaths;
    TSet<FString>& SoftObjectPaths;
    TSet<FString>& WeakObjectPaths;
    TArray<FString>& Failures;
};

void ScanNiagaraRecipeSerializedObjectReferences(
    UObject* Object,
    const FString& Context,
    FNiagaraRecipeSerializedReferenceScan& OutScan)
{
    if(!ContinueRead()){OutScan.bComplete=false;return;}
    if (!Object)
    {
        OutScan.bComplete = false;
        OutScan.Failures.AddUnique(Context + TEXT("|missing_object"));
        return;
    }

    const int32 FailureCountBefore = OutScan.Failures.Num();
    FNiagaraRecipeSerializedReferenceArchive Archive(
        Context,
        OutScan.SerializedHardObjectPaths,
        OutScan.SerializedSoftObjectPaths,
        OutScan.SerializedWeakObjectPaths,
        OutScan.Failures);
    if(!Current->Step(TEXT("Recipe_Object_Serialize"),[&]{Object->Serialize(Archive);})) {OutScan.bComplete=false;return;}
    ++OutScan.SerializedObjectCount;
    if (Archive.IsError() || Archive.IsCriticalError())
    {
        OutScan.Failures.AddUnique(Context + TEXT("|serialization_archive_error"));
    }
    if (OutScan.Failures.Num() != FailureCountBefore)
    {
        OutScan.bComplete = false;
    }
}


bool IsNiagaraRecipeTraversableDependencyPackage(const FString& PackagePath)
{
    return !PackagePath.IsEmpty() &&
        FPackageName::IsValidLongPackageName(PackagePath) &&
        !PackagePath.StartsWith(TEXT("/Script/"), ESearchCase::IgnoreCase);
}
void AddNiagaraRecipeDependencyRoot(const FString& ObjectPath, TSet<FName>& OutRoots)
{
    const FString PackagePath = PackagePathFromObjectPath(ObjectPath);
    if(PackagePath.Len()>=NAME_SIZE){Current->Fail(TEXT("niagara_graph_recipe_dependency_name_size"),TEXT("Dependency package exceeds installed FName NAME_SIZE."));return;}
    if (IsNiagaraRecipeTraversableDependencyPackage(PackagePath))
    {
        OutRoots.Add(FName(*PackagePath));
    }
}
TSharedPtr<FJsonObject> IncompleteRecipeAudit(){auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetBoolField(TEXT("audit_complete"),false);O->SetBoolField(TEXT("serialized_reference_scan_complete"),false);O->SetStringField(TEXT("error"),Current->R.Error);return O;}

TSharedPtr<FJsonObject> BuildNiagaraRecipePreSaveDependencyAudit(
    UNiagaraSystem* System,
    UNiagaraGraph* Graph,
    const TArray<FString>& SelectedFunctionPaths,
    const TArray<TSharedPtr<FJsonValue>>& NodeSpecs)
{
    auto& W=*Current;
    TSharedPtr<FJsonObject> DirectAudit = BuildNiagaraDependencyAuditJson(System, FString(), false);
    if(!ContinueRead())return IncompleteRecipeAudit();
    FAssetRegistryModule* Registry=nullptr;if(!W.Step(TEXT("Recipe_Load_AssetRegistry"),[&]{Registry=FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));})||!Registry)return IncompleteRecipeAudit();
    TSet<FName> RootPackages;
    TSet<FString> ForbiddenHits;
    TSet<FString> LiteralValues;
    TSet<FString> FailedDependencyQueries;
    const auto AuditLiteralValue = [&](const FString& Literal)
    {
        if (Literal.IsEmpty())
        {
            return;
        }
        LiteralValues.Add(Literal);
        if (IsMCPDependencyReference(Literal))
        {
            ForbiddenHits.Add(Literal);
        }
        const FString PackagePath = PackagePathFromObjectPath(Literal);
        if (IsNiagaraRecipeTraversableDependencyPackage(PackagePath))
        {
            RootPackages.Add(FName(*PackagePath));
        }
    };

    const bool bDirectAuditComplete = DirectAudit.IsValid() &&
        DirectAudit->HasTypedField<EJson::Boolean>(TEXT("success")) &&
        DirectAudit->GetBoolField(TEXT("success")) &&
        DirectAudit->HasTypedField<EJson::Boolean>(TEXT("audit_complete")) &&
        DirectAudit->GetBoolField(TEXT("audit_complete")) &&
        DirectAudit->HasTypedField<EJson::Boolean>(TEXT("dependency_query_succeeded")) &&
        DirectAudit->GetBoolField(TEXT("dependency_query_succeeded")) &&
        DirectAudit->HasTypedField<EJson::Number>(TEXT("forbidden_dependency_hit_count")) &&
        DirectAudit->HasTypedField<EJson::Array>(TEXT("dependencies")) &&
        DirectAudit->HasTypedField<EJson::Array>(TEXT("forbidden_dependency_hits"));

    bool bDependencyQueriesSucceeded = false;
    if (System && System->GetOutermost())
    {
        TArray<FName> DirectSystemDependencies;
        const FName SystemPackageName(*System->GetOutermost()->GetName());
        if(!W.Step(TEXT("Recipe_DirectDependencies"),[&]{bDependencyQueriesSucceeded=Registry->Get().GetDependencies(SystemPackageName,DirectSystemDependencies);}))return IncompleteRecipeAudit();if(DirectSystemDependencies.Num()>65536||!W.Budget(DirectSystemDependencies.Num()))return IncompleteRecipeAudit();
        if (!bDependencyQueriesSucceeded)
        {
            FailedDependencyQueries.Add(SystemPackageName.ToString());
        }
        for (const FName& Dependency : DirectSystemDependencies)
        {
            const FString DependencyString = Dependency.ToString();
            if (IsMCPDependencyReference(DependencyString))
            {
                ForbiddenHits.Add(DependencyString);
            }
            if (IsNiagaraRecipeTraversableDependencyPackage(DependencyString))
            {
                RootPackages.Add(Dependency);
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* DirectForbiddenValues = nullptr;
    if (DirectAudit.IsValid() && DirectAudit->TryGetArrayField(TEXT("forbidden_dependency_hits"), DirectForbiddenValues) && DirectForbiddenValues)
    {
        for (const TSharedPtr<FJsonValue>& Value : *DirectForbiddenValues)
        {
            FString Dependency;
            if (Value.IsValid() && Value->TryGetString(Dependency))
            {
                ForbiddenHits.Add(Dependency);
            }
        }
    }

    for (const FString& FunctionPath : SelectedFunctionPaths)
    {
        AddNiagaraRecipeDependencyRoot(FunctionPath, RootPackages);
        if (IsMCPDependencyReference(FunctionPath))
        {
            ForbiddenHits.Add(FunctionPath);
        }
    }

    FNiagaraRecipeSerializedReferenceScan SerializedReferenceScan;
    const auto AuditSerializedReferencePath = [&](const FString& ReferencePath)
    {
        if (ReferencePath.IsEmpty())
        {
            return;
        }
        if (IsMCPDependencyReference(ReferencePath))
        {
            ForbiddenHits.Add(ReferencePath);
        }
        const FString PackagePath = PackagePathFromObjectPath(ReferencePath);
        if (PackagePath.StartsWith(TEXT("/Script/"), ESearchCase::IgnoreCase))
        {
            return;
        }
        if (!IsNiagaraRecipeTraversableDependencyPackage(PackagePath))
        {
            SerializedReferenceScan.bComplete = false;
            SerializedReferenceScan.Failures.AddUnique(
                TEXT("unresolvable_serialized_reference_package|") + ReferencePath);
            return;
        }
        RootPackages.Add(FName(*PackagePath));
    };
    const auto AddExplicitObjectReference = [&SerializedReferenceScan, &AuditSerializedReferencePath, &W](
        UObject* Object,
        const FString& Context,
        TSet<FString>& OutPaths)
    {
        if (!Object)
        {
            return;
        }
        if(!Live(Object)||!W.Pin(Object)){SerializedReferenceScan.bComplete=false;SerializedReferenceScan.Failures.AddUnique(Context+TEXT("|object_unavailable"));return;}
        const FString ObjectPath = Object->GetPathName();
        if (ObjectPath.IsEmpty())
        {
            SerializedReferenceScan.bComplete = false;
            SerializedReferenceScan.Failures.AddUnique(Context + TEXT("|object_without_path"));
            return;
        }
        OutPaths.Add(ObjectPath);
        AuditSerializedReferencePath(ObjectPath);
    };

    if (!Graph)
    {
        SerializedReferenceScan.bComplete = false;
        SerializedReferenceScan.Failures.AddUnique(TEXT("graph|missing_graph"));
    }
    else
    {
        SerializedReferenceScan.GraphNodeSlotCount = Graph->Nodes.Num();
        ScanNiagaraRecipeSerializedObjectReferences(Graph, TEXT("graph"), SerializedReferenceScan);if(!ContinueRead())return IncompleteRecipeAudit();
        AddExplicitObjectReference(
            Graph->GetClass(),
            TEXT("graph|export_class"),
            SerializedReferenceScan.SerializedExportClassObjectPaths);
        for (int32 NodeIndex = 0; NodeIndex < Graph->Nodes.Num(); ++NodeIndex)
        {
            UEdGraphNode* Node = Graph->Nodes[NodeIndex];
            const FString NodeContext = FString::Printf(TEXT("node[%d]"), NodeIndex);
            if (!Node)
            {
                SerializedReferenceScan.bComplete = false;
                SerializedReferenceScan.Failures.AddUnique(NodeContext + TEXT("|missing_node"));
                continue;
            }
            ++SerializedReferenceScan.SerializedNodeCount;
            ScanNiagaraRecipeSerializedObjectReferences(Node, NodeContext, SerializedReferenceScan);if(!ContinueRead())return IncompleteRecipeAudit();
            AddExplicitObjectReference(
                Node->GetClass(),
                NodeContext + TEXT("|export_class"),
                SerializedReferenceScan.SerializedExportClassObjectPaths);

            // FReferenceFinder is a deliberately broader, independent hard
            // reference pass over UPROPERTY data and AddReferencedObjects.
            // It closes gaps in custom node implementations; only the exact
            // save archive above is used to seed transitive package traversal.
            TArray<UObject*> FinderReferences;
            FReferenceFinder ReferenceFinder(
                FinderReferences,
                nullptr,
                false,
                true,
                false,
                true);
            if(!W.Step(TEXT("Recipe_ReferenceFinder"),[&]{ReferenceFinder.FindReferences(Node);}))return IncompleteRecipeAudit();if(FinderReferences.Num()>65536||!W.Budget(FinderReferences.Num()))return IncompleteRecipeAudit();
            ++SerializedReferenceScan.ReferenceFinderNodeCount;
            for (UObject* Reference : FinderReferences)
            {
                if (!Reference)
                {
                    SerializedReferenceScan.bComplete = false;
                    SerializedReferenceScan.Failures.AddUnique(NodeContext + TEXT("|reference_finder_returned_null"));
                    continue;
                }
                if(!Live(Reference)||!W.Pin(Reference))return IncompleteRecipeAudit();
                const FString ReferencePath = Reference->GetPathName();
                if (ReferencePath.IsEmpty())
                {
                    SerializedReferenceScan.bComplete = false;
                    SerializedReferenceScan.Failures.AddUnique(NodeContext + TEXT("|reference_finder_path_missing"));
                    continue;
                }
                SerializedReferenceScan.ReferenceFinderHardObjectPaths.Add(ReferencePath);
                if (IsMCPDependencyReference(ReferencePath))
                {
                    ForbiddenHits.Add(ReferencePath);
                }
            }

            if (const UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
            {
                const FString FunctionPath = !FunctionCall->FunctionScriptAssetObjectPath.IsNone()
                    ? FunctionCall->FunctionScriptAssetObjectPath.ToString()
                    : NiagaraObjectPathOrEmpty(FunctionCall->FunctionScript);
                AddNiagaraRecipeDependencyRoot(FunctionPath, RootPackages);
                if (IsMCPDependencyReference(FunctionPath))
                {
                    ForbiddenHits.Add(FunctionPath);
                }
            }

            SerializedReferenceScan.PinSlotCount += Node->Pins.Num();
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin)
                {
                    SerializedReferenceScan.bComplete = false;
                    SerializedReferenceScan.Failures.AddUnique(NodeContext + TEXT("|missing_pin"));
                    continue;
                }
                ++SerializedReferenceScan.AuditedPinCount;
                const FString PinContext = NodeContext + TEXT("|pin=") + NiagaraRecipePinIdentity(Pin);

                if (Pin->PinType.PinSubCategoryObject.IsStale())
                {
                    SerializedReferenceScan.bComplete = false;
                    SerializedReferenceScan.Failures.AddUnique(PinContext + TEXT("|stale_pin_subcategory_object"));
                }
                else
                {
                    AddExplicitObjectReference(
                        Pin->PinType.PinSubCategoryObject.Get(),
                        PinContext + TEXT("|pin_subcategory_object"),
                        SerializedReferenceScan.PinSubCategoryObjectPaths);
                }

                if (Pin->PinType.PinValueType.TerminalSubCategoryObject.IsStale())
                {
                    SerializedReferenceScan.bComplete = false;
                    SerializedReferenceScan.Failures.AddUnique(PinContext + TEXT("|stale_terminal_subcategory_object"));
                }
                else
                {
                    AddExplicitObjectReference(
                        Pin->PinType.PinValueType.TerminalSubCategoryObject.Get(),
                        PinContext + TEXT("|terminal_subcategory_object"),
                        SerializedReferenceScan.PinTerminalSubCategoryObjectPaths);
                }

                if (!IsNiagaraDynamicAddPin(Pin))
                {
                    ++SerializedReferenceScan.NiagaraTypeDefinitionPinScanCount;
                    const FNiagaraTypeDefinition TypeDefinition =
                        UEdGraphSchema_Niagara::PinToTypeDefinition(Pin);
                    if (TypeDefinition.IsValid())
                    {
                        AddExplicitObjectReference(
                            TypeDefinition.IsEnum()?static_cast<UObject*>(TypeDefinition.GetEnum()):(TypeDefinition.IsUObject()||TypeDefinition.IsDataInterface()?static_cast<UObject*>(TypeDefinition.GetClass()):static_cast<UObject*>(TypeDefinition.GetScriptStruct())),
                            PinContext + TEXT("|niagara_type_definition"),
                            SerializedReferenceScan.NiagaraPinTypeDefinitionObjectPaths);
                    }
                }
                else
                {
                    ++SerializedReferenceScan.DynamicAddPinSkipCount;
                }

                if (Pin->DefaultObject)
                {
                    const FString DefaultObjectPath = Pin->DefaultObject->GetPathName();
                    AuditLiteralValue(DefaultObjectPath);
                    AddNiagaraRecipeDependencyRoot(DefaultObjectPath, RootPackages);
                }
                AuditLiteralValue(Pin->DefaultValue);
                AuditLiteralValue(Pin->DefaultTextValue.ToString());
            }
        }
    }

    for (const FString& ReferencePath : SerializedReferenceScan.SerializedHardObjectPaths)
    {
        AuditSerializedReferencePath(ReferencePath);
    }
    for (const FString& ReferencePath : SerializedReferenceScan.SerializedSoftObjectPaths)
    {
        AuditSerializedReferencePath(ReferencePath);
    }
    for (const FString& ReferencePath : SerializedReferenceScan.SerializedWeakObjectPaths)
    {
        AuditSerializedReferencePath(ReferencePath);
    }

    for (const TSharedPtr<FJsonValue>& NodeValue : NodeSpecs)
    {
        const TSharedPtr<FJsonObject> NodeSpec = NodeValue.IsValid()&&NodeValue->Type==EJson::Object ? NodeValue->AsObject() : nullptr;
        const TSharedPtr<FJsonObject>* Defaults = nullptr;
        if (!NodeSpec.IsValid() || !NodeSpec->TryGetObjectField(TEXT("defaults"), Defaults) || !Defaults || !Defaults->IsValid())
        {
            continue;
        }
        for (const auto& Pair : (*Defaults)->Values)
        {
            FString Literal;
            if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(Literal))
            {
                continue;
            }
            AuditLiteralValue(Literal);
        }
    }

    TArray<FName> Pending = RootPackages.Array();
    TSet<FName> Queued = RootPackages;
    TSet<FName> Visited;
    constexpr int32 MaxTransitiveDependencyPackages = 4096;
    int32 PendingIndex = 0;
    while (PendingIndex < Pending.Num() && Visited.Num() < MaxTransitiveDependencyPackages)
    {
        const FName DependencyPackage = Pending[PendingIndex++];
        if (Visited.Contains(DependencyPackage))
        {
            continue;
        }
        Visited.Add(DependencyPackage);
        const FString CurrentString = DependencyPackage.ToString();
        if (IsMCPDependencyReference(CurrentString))
        {
            ForbiddenHits.Add(CurrentString);
        }

        TArray<FName> Dependencies;
        bool GotDependencies=false;if(!W.Step(TEXT("Recipe_TransitiveDependencies"),[&]{GotDependencies=Registry->Get().GetDependencies(DependencyPackage,Dependencies);}))return IncompleteRecipeAudit();if(Dependencies.Num()>65536||!W.Budget(Dependencies.Num()))return IncompleteRecipeAudit();
        if (!GotDependencies)
        {
            bDependencyQueriesSucceeded = false;
            FailedDependencyQueries.Add(CurrentString);
            continue;
        }
        for (const FName& Dependency : Dependencies)
        {
            const FString DependencyString = Dependency.ToString();
            if (IsMCPDependencyReference(DependencyString))
            {
                ForbiddenHits.Add(DependencyString);
            }
            if (IsNiagaraRecipeTraversableDependencyPackage(DependencyString) &&
                !Visited.Contains(Dependency) && !Queued.Contains(Dependency))
            {
                if(!W.Budget(1,DependencyString.Len()))return IncompleteRecipeAudit();
                Queued.Add(Dependency);
                Pending.Add(Dependency);
            }
        }
    }
    const bool bSerializedReferenceScanComplete = SerializedReferenceScan.bComplete &&
        Graph != nullptr &&
        SerializedReferenceScan.SerializedNodeCount == SerializedReferenceScan.GraphNodeSlotCount &&
        SerializedReferenceScan.AuditedPinCount == SerializedReferenceScan.PinSlotCount &&
        SerializedReferenceScan.SerializedObjectCount == SerializedReferenceScan.SerializedNodeCount + 1 &&
        SerializedReferenceScan.ReferenceFinderNodeCount == SerializedReferenceScan.SerializedNodeCount &&
        SerializedReferenceScan.NiagaraTypeDefinitionPinScanCount + SerializedReferenceScan.DynamicAddPinSkipCount ==
            SerializedReferenceScan.AuditedPinCount;
    const bool bTraversalComplete = PendingIndex >= Pending.Num() &&
        bDirectAuditComplete && bDependencyQueriesSucceeded && FailedDependencyQueries.IsEmpty() &&
        bSerializedReferenceScanComplete;

    TArray<FString> RootStrings;
    for (const FName& Root : RootPackages)
    {
        RootStrings.Add(Root.ToString());
    }
    RootStrings.Sort();
    TArray<FString> DependencyStrings;
    for (const FName& Dependency : Visited)
    {
        DependencyStrings.Add(Dependency.ToString());
    }
    DependencyStrings.Sort();
    TArray<FString> ForbiddenStrings = ForbiddenHits.Array();
    ForbiddenStrings.Sort();
    TArray<FString> LiteralStrings = LiteralValues.Array();
    LiteralStrings.Sort();
    TArray<FString> FailedDependencyQueryStrings = FailedDependencyQueries.Array();
    FailedDependencyQueryStrings.Sort();
    TArray<FString> SerializedHardReferenceStrings = SerializedReferenceScan.SerializedHardObjectPaths.Array();
    SerializedHardReferenceStrings.Sort();
    TArray<FString> SerializedSoftReferenceStrings = SerializedReferenceScan.SerializedSoftObjectPaths.Array();
    SerializedSoftReferenceStrings.Sort();
    TArray<FString> SerializedWeakReferenceStrings = SerializedReferenceScan.SerializedWeakObjectPaths.Array();
    SerializedWeakReferenceStrings.Sort();
    TArray<FString> SerializedExportClassReferenceStrings =
        SerializedReferenceScan.SerializedExportClassObjectPaths.Array();
    SerializedExportClassReferenceStrings.Sort();
    TArray<FString> ReferenceFinderHardReferenceStrings = SerializedReferenceScan.ReferenceFinderHardObjectPaths.Array();
    ReferenceFinderHardReferenceStrings.Sort();
    TArray<FString> PinSubCategoryReferenceStrings = SerializedReferenceScan.PinSubCategoryObjectPaths.Array();
    PinSubCategoryReferenceStrings.Sort();
    TArray<FString> PinTerminalSubCategoryReferenceStrings =
        SerializedReferenceScan.PinTerminalSubCategoryObjectPaths.Array();
    PinTerminalSubCategoryReferenceStrings.Sort();
    TArray<FString> NiagaraTypeDefinitionReferenceStrings =
        SerializedReferenceScan.NiagaraPinTypeDefinitionObjectPaths.Array();
    NiagaraTypeDefinitionReferenceStrings.Sort();
    TArray<FString> SerializedReferenceFailureStrings = SerializedReferenceScan.Failures;
    SerializedReferenceFailureStrings.Sort();

    TSharedPtr<FJsonObject> Audit = MakeShared<FJsonObject>();
    Audit->SetBoolField(TEXT("success"), bTraversalComplete && ForbiddenStrings.IsEmpty());
    Audit->SetBoolField(TEXT("audit_complete"), bTraversalComplete);
    Audit->SetBoolField(TEXT("direct_system_audit_complete"), bDirectAuditComplete);
    Audit->SetBoolField(TEXT("dependency_queries_succeeded"), bDependencyQueriesSucceeded);
    Audit->SetBoolField(TEXT("serialized_reference_scan_complete"), bSerializedReferenceScanComplete);
    Audit->SetStringField(
        TEXT("serialized_reference_scan_mode"),
        TEXT("ue56_non_mutating_save_archive_graph_and_nodes_hard_soft_weak_plus_export_classes_reference_finder_explicit_pin_and_niagara_type_definition"));
    Audit->SetStringField(
        TEXT("serialized_reference_archive_contract"),
        TEXT("object_reference_collector+explicit_non_mutating_weak_reference_walk+saving+persistent+skip_bulk_data"));
    Audit->SetBoolField(TEXT("reference_finder_seeds_transitive_dependencies"), false);
    Audit->SetNumberField(TEXT("transitive_dependency_limit"), MaxTransitiveDependencyPackages);
    Audit->SetNumberField(TEXT("graph_node_slot_count"), SerializedReferenceScan.GraphNodeSlotCount);
    Audit->SetNumberField(TEXT("serialized_node_count"), SerializedReferenceScan.SerializedNodeCount);
    Audit->SetNumberField(TEXT("pin_slot_count"), SerializedReferenceScan.PinSlotCount);
    Audit->SetNumberField(TEXT("audited_pin_count"), SerializedReferenceScan.AuditedPinCount);
    Audit->SetNumberField(TEXT("serialized_object_count"), SerializedReferenceScan.SerializedObjectCount);
    Audit->SetNumberField(TEXT("reference_finder_node_count"), SerializedReferenceScan.ReferenceFinderNodeCount);
    Audit->SetNumberField(
        TEXT("niagara_type_definition_pin_scan_count"),
        SerializedReferenceScan.NiagaraTypeDefinitionPinScanCount);
    Audit->SetNumberField(TEXT("dynamic_add_pin_skip_count"), SerializedReferenceScan.DynamicAddPinSkipCount);
    Audit->SetNumberField(TEXT("serialized_hard_reference_count"), SerializedHardReferenceStrings.Num());
    Audit->SetNumberField(TEXT("serialized_soft_reference_count"), SerializedSoftReferenceStrings.Num());
    Audit->SetNumberField(TEXT("serialized_weak_reference_count"), SerializedWeakReferenceStrings.Num());
    Audit->SetNumberField(
        TEXT("serialized_export_class_reference_count"),
        SerializedExportClassReferenceStrings.Num());
    Audit->SetNumberField(TEXT("reference_finder_hard_reference_count"), ReferenceFinderHardReferenceStrings.Num());
    Audit->SetNumberField(TEXT("pin_subcategory_object_reference_count"), PinSubCategoryReferenceStrings.Num());
    Audit->SetNumberField(
        TEXT("pin_terminal_subcategory_object_reference_count"),
        PinTerminalSubCategoryReferenceStrings.Num());
    Audit->SetNumberField(
        TEXT("niagara_pin_type_definition_reference_count"),
        NiagaraTypeDefinitionReferenceStrings.Num());
    Audit->SetNumberField(TEXT("root_package_count"), RootStrings.Num());
    Audit->SetNumberField(TEXT("transitive_dependency_count"), DependencyStrings.Num());
    Audit->SetNumberField(TEXT("forbidden_dependency_hit_count"), ForbiddenStrings.Num());
    Audit->SetBoolField(TEXT("has_forbidden_dependency"), !ForbiddenStrings.IsEmpty());
    Audit->SetArrayField(TEXT("root_packages"), StringArrayToJsonValues(RootStrings));
    Audit->SetArrayField(TEXT("transitive_dependencies"), StringArrayToJsonValues(DependencyStrings));
    Audit->SetArrayField(TEXT("forbidden_dependency_hits"), StringArrayToJsonValues(ForbiddenStrings));
    Audit->SetArrayField(TEXT("recipe_default_literals"), StringArrayToJsonValues(LiteralStrings));
    Audit->SetArrayField(TEXT("failed_dependency_queries"), StringArrayToJsonValues(FailedDependencyQueryStrings));
    Audit->SetArrayField(
        TEXT("serialized_hard_references"),
        StringArrayToJsonValues(SerializedHardReferenceStrings));
    Audit->SetArrayField(
        TEXT("serialized_soft_references"),
        StringArrayToJsonValues(SerializedSoftReferenceStrings));
    Audit->SetArrayField(
        TEXT("serialized_weak_references"),
        StringArrayToJsonValues(SerializedWeakReferenceStrings));
    Audit->SetArrayField(
        TEXT("serialized_export_class_references"),
        StringArrayToJsonValues(SerializedExportClassReferenceStrings));
    Audit->SetArrayField(
        TEXT("reference_finder_hard_references"),
        StringArrayToJsonValues(ReferenceFinderHardReferenceStrings));
    Audit->SetArrayField(
        TEXT("pin_subcategory_object_references"),
        StringArrayToJsonValues(PinSubCategoryReferenceStrings));
    Audit->SetArrayField(
        TEXT("pin_terminal_subcategory_object_references"),
        StringArrayToJsonValues(PinTerminalSubCategoryReferenceStrings));
    Audit->SetArrayField(
        TEXT("niagara_pin_type_definition_references"),
        StringArrayToJsonValues(NiagaraTypeDefinitionReferenceStrings));
    Audit->SetArrayField(
        TEXT("serialized_reference_scan_failures"),
        StringArrayToJsonValues(SerializedReferenceFailureStrings));
    Audit->SetObjectField(TEXT("direct_system_audit"), DirectAudit);
    return Audit;
}

TSharedPtr<FJsonObject> PointCompileSnapshot(FWork& W,UNiagaraSystem* System,double Timeout,double Poll)
{
 auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),true);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0);bool Outstanding=true,Active=true;if(!W.Step(TEXT("PreexistingCompile_PointObservation"),[&]{Outstanding=System->HasOutstandingCompilationRequests(true);Active=System->HasActiveCompilations();}))return O;O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);O->SetBoolField(TEXT("active_compilations_after"),Active);O->SetBoolField(TEXT("zero_wait_idle_observed"),!Outstanding&&!Active);O->SetBoolField(TEXT("actual_wait_performed"),false);return O;
}
bool PackageOwnership(FWork& W,UNiagaraSystem* System)
{
 if(!W.Admit(TEXT("package_writer_ownership"))||System!=W.Target.Get())return false;auto* P=System->GetOutermost();if(System->GetOuter()!=P||System->GetPackage()!=P)return W.Fail(TEXT("niagara_hlsl_conversion_root_owner_invalid"),TEXT("Exact System root package required"));TArray<UObject*> Objects;if(!W.Step(TEXT("GetObjectsWithPackage_Save"),[&]{GetObjectsWithPackage(P,Objects,true);}))return false;if(Objects.Num()>65536||!W.Budget(Objects.Num()))return false;for(auto* O:Objects){if(!Live(O)||O->GetPackage()!=P||O->GetOutermost()!=P||(O!=System&&!O->IsIn(System)))return W.Fail(TEXT("niagara_hlsl_conversion_foreign_package_export"),TEXT("Save would include export outside exact selected System subtree"));if(!W.Pin(O))return false;}if(!W.CaptureCompiledCacheRoles())return false;return W.Admit(TEXT("package_ownership_complete"));
}
bool SaveHlslSystem(FWork& W,UNiagaraSystem* System)
{
 if(!PackageOwnership(W,System)||!W.SDK(TEXT("SaveLoadedAsset_HLSL"),true,true))return false;W.R.bSaveAttempted=true;const bool Returned=UEditorAssetLibrary::SaveLoadedAsset(System,false);W.R.bSaved|=Returned;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("asset"),W.Objects[0]->Path);Row->SetBoolField(TEXT("SDK_returned"),true);Row->SetBoolField(TEXT("SDK_save_success"),Returned);W.SaveRows.Add(MakeShared<FJsonValueObject>(Row));if(!W.After(TEXT("SaveLoadedAsset_HLSL"))){Row->SetBoolField(TEXT("post_save_observation_skipped"),true);return false;}const bool Dirty=System->GetOutermost()->IsDirty();Row->SetBoolField(TEXT("dirty_after"),Dirty);if(!Returned||Dirty)return W.Fail(TEXT("niagara_hlsl_conversion_save_failed"),TEXT("Save returned false or exact package dirty after callback; actual true history retained"));return PackageOwnership(W,System);
}


bool ExactCallerSets(const TArray<FNiagaraScriptCompileSnapshot>& A,const TArray<FNiagaraScriptCompileSnapshot>& B)
{
 if(A.IsEmpty()||A.Num()!=B.Num())return false;TSet<const UNiagaraScript*> Used;
 for(const auto& X:A){int32 Matches=0;for(const auto& Y:B)if(X.Script.Get()==Y.Script.Get()&&X.ScriptPath.Equals(Y.ScriptPath,ESearchCase::CaseSensitive)&&X.OwnerKind.Equals(Y.OwnerKind,ESearchCase::CaseSensitive)&&X.OwnerName.Equals(Y.OwnerName,ESearchCase::CaseSensitive))++Matches;if(!X.Script||Matches!=1||Used.Contains(X.Script.Get()))return false;Used.Add(X.Script.Get());}return true;
}

// HQ original recipe validation/data helpers.
TArray<FNiagaraScriptCompileSnapshot> CaptureNiagaraRecipeCallerSnapshots(
    const FNiagaraRecipeScratchPadReferenceScan& Scan)
{
    if(!ContinueRead())return {};
    TArray<UNiagaraScript*> CallerScripts = Scan.CallerScripts.Array();
    for(UNiagaraScript* S:CallerScripts)if(!S||!Current->Pin(S))return {};
    CallerScripts.Sort([](const UNiagaraScript& Left, const UNiagaraScript& Right)
    {
        return Left.GetPathName() < Right.GetPathName();
    });

    TArray<FNiagaraScriptCompileSnapshot> Snapshots;
    TSet<const UNiagaraScript*> SeenScripts;
    for (UNiagaraScript* CallerScript : CallerScripts)
    {
        AddNiagaraScriptCompileSnapshot(
            CallerScript,
            TEXT("scratch_pad_caller"),
            CallerScript ? CallerScript->GetOuter()->GetName() : FString(),
            SeenScripts,
            Snapshots);
        if(!ContinueRead())return Snapshots;
    }
    return Snapshots;
}

TSharedPtr<FJsonObject> BuildNiagaraRecipeScratchPadContextValidation(
    const UNiagaraScript* Script,
    const FString& Phase,
    const FNiagaraRecipeScratchPadReferenceScan& Scan,
    int32 ExpectedReferenceCount,
    bool bSystemCompileClean)
{
    TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
    const TArray<FNiagaraScriptCompileSnapshot> CallerSnapshots = CaptureNiagaraRecipeCallerSnapshots(Scan);
    if(!ContinueRead())return Validation;
    TArray<TSharedPtr<FJsonValue>> CallerValues;
    bool bCallerCompileContextClean = CallerSnapshots.Num() > 0;
    for (int32 CallerIndex = 0; CallerIndex < CallerSnapshots.Num(); ++CallerIndex)
    {
        const FNiagaraScriptCompileSnapshot& CallerSnapshot = CallerSnapshots[CallerIndex];
        const bool bSourceSynchronized = CallerSnapshot.CachedId.IsValid() &&
            CallerSnapshot.ComputedId.IsValid() &&
            CallerSnapshot.CachedId == CallerSnapshot.ComputedId;
        const bool bCompileStatusClean = CallerSnapshot.CompileStatus == ENiagaraScriptCompileStatus::NCS_UpToDate;
        bCallerCompileContextClean &= bSourceSynchronized && bCompileStatusClean;
        TSharedPtr<FJsonObject> CallerObject = BuildNiagaraScriptCompileSnapshotJson(CallerSnapshot, CallerIndex);
        CallerObject->SetBoolField(TEXT("compile_status_clean"), bCompileStatusClean);
        CallerValues.Add(MakeShared<FJsonValueObject>(CallerObject));
    }
    const bool bReferencesStable = Scan.ReferenceCount > 0 &&
        Scan.ReferenceCount == ExpectedReferenceCount;
    const bool bClean = Script && Scan.bComplete && bReferencesStable &&
        bCallerCompileContextClean && bSystemCompileClean;
    Validation->SetBoolField(TEXT("success"), bClean);
    Validation->SetBoolField(TEXT("clean"), bClean);
    Validation->SetStringField(TEXT("phase"), Phase);
    Validation->SetStringField(TEXT("validation_mode"), TEXT("referenced_system_compile"));
    Validation->SetNumberField(TEXT("scratch_pad_reference_count"), Scan.ReferenceCount);
    Validation->SetNumberField(TEXT("expected_scratch_pad_reference_count"), ExpectedReferenceCount);
    Validation->SetBoolField(TEXT("references_stable"), bReferencesStable);
    Validation->SetBoolField(TEXT("reference_scan_complete"), Scan.bComplete);
    Validation->SetNumberField(TEXT("caller_script_count"), CallerSnapshots.Num());
    Validation->SetBoolField(TEXT("caller_compile_context_clean"), bCallerCompileContextClean);
    Validation->SetArrayField(TEXT("caller_scripts"), CallerValues);
    Validation->SetArrayField(
        TEXT("unresolved_output_contexts"),
        StringArrayToJsonValues(Scan.UnresolvedOutputContexts));
    Validation->SetStringField(TEXT("reference_scan_mode"), NiagaraRecipeScratchPadReferenceScanMode);
    Validation->SetStringField(TEXT("indirect_reference_policy"), TEXT("reject_as_unproven"));
    Validation->SetBoolField(TEXT("direct_compile_requested"), false);
    Validation->SetBoolField(TEXT("system_compile_required"), true);
    Validation->SetBoolField(TEXT("system_compile_clean"), bSystemCompileClean);
    Validation->SetStringField(TEXT("script_name"), Script ? Script->GetName() : FString());
    Validation->SetStringField(TEXT("script_path"), Script ? Script->GetPathName() : FString());
    Validation->SetStringField(TEXT("unreferenced_policy"), TEXT("reject_before_mutation"));
    return Validation;
}

TSharedPtr<FJsonObject> BuildNiagaraRecipeCallerCompileIdValidation(
    const TArray<FNiagaraScriptCompileSnapshot>& BaselineSnapshots,
    const TArray<FNiagaraScriptCompileSnapshot>& CurrentSnapshots,
    const TArray<FNiagaraScriptCompileSnapshot>* ExpectedPreCompileSnapshots,
    const FString& Phase,
    bool bExpectChanged,
    bool& OutSuccess)
{
    int32 ChangedCount = 0;
    int32 RestoredCount = 0;
    int32 CleanSynchronizedCount = 0;
    int32 MatchedExpectedPreCompileCount = 0;
    TArray<TSharedPtr<FJsonValue>> ScriptValues;
    for (const FNiagaraScriptCompileSnapshot& BaselineSnapshot : BaselineSnapshots)
    {
        const FNiagaraScriptCompileSnapshot* CurrentSnapshot = FindNiagaraScriptCompileSnapshot(
            CurrentSnapshots,
            BaselineSnapshot.ScriptPath);
        TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
        ScriptObject->SetStringField(TEXT("script_path"), BaselineSnapshot.ScriptPath);
        const bool bCurrentFound = CurrentSnapshot != nullptr;
        ScriptObject->SetBoolField(TEXT("current_snapshot_found"), bCurrentFound);
        const FNiagaraScriptCompileSnapshot* ExpectedPreCompileSnapshot = ExpectedPreCompileSnapshots
            ? FindNiagaraScriptCompileSnapshot(*ExpectedPreCompileSnapshots, BaselineSnapshot.ScriptPath)
            : nullptr;
        const bool bExpectedPreCompileRequired = ExpectedPreCompileSnapshots != nullptr;
        const bool bExpectedPreCompileFound = !bExpectedPreCompileRequired || ExpectedPreCompileSnapshot != nullptr;
        ScriptObject->SetBoolField(TEXT("expected_precompile_snapshot_required"), bExpectedPreCompileRequired);
        ScriptObject->SetBoolField(TEXT("expected_precompile_snapshot_found"), bExpectedPreCompileFound);
        if (CurrentSnapshot)
        {
            const bool bSourceSynchronized = CurrentSnapshot->CachedId.IsValid() &&
                CurrentSnapshot->ComputedId.IsValid() &&
                CurrentSnapshot->CachedId == CurrentSnapshot->ComputedId;
            const bool bCompileStatusClean = CurrentSnapshot->CompileStatus == ENiagaraScriptCompileStatus::NCS_UpToDate;
            const bool bChanged = BaselineSnapshot.CachedId.IsValid() &&
                CurrentSnapshot->CachedId.IsValid() &&
                CurrentSnapshot->CachedId != BaselineSnapshot.CachedId;
            const bool bRestored = BaselineSnapshot.CachedId.IsValid() &&
                BaselineSnapshot.ComputedId.IsValid() &&
                CurrentSnapshot->CachedId == BaselineSnapshot.CachedId &&
                CurrentSnapshot->ComputedId == BaselineSnapshot.ComputedId;
            const bool bMatchesExpectedPreCompile = !bExpectedPreCompileRequired ||
                (ExpectedPreCompileSnapshot && ExpectedPreCompileSnapshot->ComputedId.IsValid() &&
                    CurrentSnapshot->CachedId == ExpectedPreCompileSnapshot->ComputedId &&
                    CurrentSnapshot->ComputedId == ExpectedPreCompileSnapshot->ComputedId);
            ChangedCount += bChanged ? 1 : 0;
            RestoredCount += bRestored ? 1 : 0;
            CleanSynchronizedCount += bSourceSynchronized && bCompileStatusClean ? 1 : 0;
            MatchedExpectedPreCompileCount += bMatchesExpectedPreCompile ? 1 : 0;
            ScriptObject->SetBoolField(TEXT("source_synchronized"), bSourceSynchronized);
            ScriptObject->SetBoolField(TEXT("compile_status_clean"), bCompileStatusClean);
            ScriptObject->SetStringField(
                TEXT("compile_status"),
                NiagaraCompileStatusName(CurrentSnapshot->CompileStatus));
            ScriptObject->SetBoolField(TEXT("compile_id_changed_from_baseline"), bChanged);
            ScriptObject->SetBoolField(TEXT("compile_id_restored_to_baseline"), bRestored);
            ScriptObject->SetBoolField(
                TEXT("current_matches_expected_precompile_computed_id"),
                bMatchesExpectedPreCompile);
            ScriptObject->SetStringField(
                TEXT("baseline_cached_id_fingerprint"),
                NiagaraCompileIdFingerprint(BaselineSnapshot.CachedId));
            ScriptObject->SetStringField(
                TEXT("current_cached_id_fingerprint"),
                NiagaraCompileIdFingerprint(CurrentSnapshot->CachedId));
            ScriptObject->SetStringField(
                TEXT("expected_precompile_computed_id_fingerprint"),
                ExpectedPreCompileSnapshot
                    ? NiagaraCompileIdFingerprint(ExpectedPreCompileSnapshot->ComputedId)
                    : FString());
        }
        ScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
    }

    const bool bSnapshotSetsMatch = ExactCallerSets(BaselineSnapshots,CurrentSnapshots) && (!ExpectedPreCompileSnapshots||ExactCallerSets(BaselineSnapshots,*ExpectedPreCompileSnapshots)) && BaselineSnapshots.Num() > 0 &&
        CurrentSnapshots.Num() == BaselineSnapshots.Num() &&
        ScriptValues.Num() == BaselineSnapshots.Num() &&
        (!ExpectedPreCompileSnapshots || ExpectedPreCompileSnapshots->Num() == BaselineSnapshots.Num());
    const bool bAllCleanSynchronized = CleanSynchronizedCount == BaselineSnapshots.Num();
    const bool bAllMatchExpectedPreCompile = !ExpectedPreCompileSnapshots ||
        MatchedExpectedPreCompileCount == BaselineSnapshots.Num();
    const bool bRelationshipSatisfied = bExpectChanged
        ? ChangedCount == BaselineSnapshots.Num()
        : RestoredCount == BaselineSnapshots.Num();
    OutSuccess = bSnapshotSetsMatch && bAllCleanSynchronized && bRelationshipSatisfied &&
        bAllMatchExpectedPreCompile;

    TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
    Validation->SetBoolField(TEXT("success"), OutSuccess);
    Validation->SetStringField(TEXT("phase"), Phase);
    Validation->SetStringField(
        TEXT("expected_compile_id_relationship"),
        bExpectChanged ? TEXT("changed_from_baseline") : TEXT("restored_to_baseline"));
    Validation->SetNumberField(TEXT("baseline_caller_script_count"), BaselineSnapshots.Num());
    Validation->SetNumberField(TEXT("current_caller_script_count"), CurrentSnapshots.Num());
    Validation->SetNumberField(TEXT("changed_script_count"), ChangedCount);
    Validation->SetNumberField(TEXT("restored_script_count"), RestoredCount);
    Validation->SetNumberField(TEXT("clean_synchronized_script_count"), CleanSynchronizedCount);
    Validation->SetNumberField(
        TEXT("expected_precompile_caller_script_count"),
        ExpectedPreCompileSnapshots ? ExpectedPreCompileSnapshots->Num() : 0);
    Validation->SetNumberField(
        TEXT("matched_expected_precompile_script_count"),
        ExpectedPreCompileSnapshots ? MatchedExpectedPreCompileCount : 0);
    Validation->SetBoolField(TEXT("snapshot_sets_match"), bSnapshotSetsMatch);
    Validation->SetBoolField(TEXT("all_callers_clean_and_synchronized"), bAllCleanSynchronized);
    Validation->SetBoolField(
        TEXT("all_callers_match_expected_precompile_computed_id"),
        bAllMatchExpectedPreCompile);
    Validation->SetArrayField(TEXT("scripts"), ScriptValues);
    return Validation;
}

TSharedPtr<FJsonObject> BuildNiagaraRecipePreCompileMutationValidation(
    const TArray<FNiagaraScriptCompileSnapshot>& BaselineSnapshots,
    const TArray<FNiagaraScriptCompileSnapshot>& PendingSnapshots,
    const FString& Phase,
    bool& OutSuccess)
{
    int32 ChangedComputedIdCount = 0;
    TArray<TSharedPtr<FJsonValue>> ScriptValues;
    for (const FNiagaraScriptCompileSnapshot& BaselineSnapshot : BaselineSnapshots)
    {
        const FNiagaraScriptCompileSnapshot* PendingSnapshot = FindNiagaraScriptCompileSnapshot(
            PendingSnapshots,
            BaselineSnapshot.ScriptPath);
        const bool bPendingFound = PendingSnapshot != nullptr;
        const bool bComputedIdChanged = PendingSnapshot && BaselineSnapshot.CachedId.IsValid() &&
            BaselineSnapshot.ComputedId.IsValid() && PendingSnapshot->ComputedId.IsValid() &&
            PendingSnapshot->ComputedId != BaselineSnapshot.CachedId &&
            PendingSnapshot->ComputedId != BaselineSnapshot.ComputedId;
        ChangedComputedIdCount += bComputedIdChanged ? 1 : 0;

        TSharedPtr<FJsonObject> ScriptObject = MakeShared<FJsonObject>();
        ScriptObject->SetStringField(TEXT("script_path"), BaselineSnapshot.ScriptPath);
        ScriptObject->SetBoolField(TEXT("pending_snapshot_found"), bPendingFound);
        ScriptObject->SetBoolField(TEXT("computed_id_changed_from_baseline"), bComputedIdChanged);
        ScriptObject->SetStringField(
            TEXT("baseline_cached_id_fingerprint"),
            NiagaraCompileIdFingerprint(BaselineSnapshot.CachedId));
        ScriptObject->SetStringField(
            TEXT("baseline_computed_id_fingerprint"),
            NiagaraCompileIdFingerprint(BaselineSnapshot.ComputedId));
        ScriptObject->SetStringField(
            TEXT("expected_precompile_computed_id_fingerprint"),
            PendingSnapshot ? NiagaraCompileIdFingerprint(PendingSnapshot->ComputedId) : FString());
        ScriptValues.Add(MakeShared<FJsonValueObject>(ScriptObject));
    }

    const bool bSnapshotSetsMatch = ExactCallerSets(BaselineSnapshots,PendingSnapshots) && BaselineSnapshots.Num() > 0 &&
        PendingSnapshots.Num() == BaselineSnapshots.Num() &&
        ScriptValues.Num() == BaselineSnapshots.Num();
    OutSuccess = bSnapshotSetsMatch && ChangedComputedIdCount == BaselineSnapshots.Num();

    TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
    Validation->SetBoolField(TEXT("success"), OutSuccess);
    Validation->SetStringField(TEXT("phase"), Phase);
    Validation->SetNumberField(TEXT("baseline_caller_script_count"), BaselineSnapshots.Num());
    Validation->SetNumberField(TEXT("pending_caller_script_count"), PendingSnapshots.Num());
    Validation->SetNumberField(TEXT("changed_computed_id_script_count"), ChangedComputedIdCount);
    Validation->SetBoolField(TEXT("snapshot_sets_match"), bSnapshotSetsMatch);
    Validation->SetBoolField(
        TEXT("all_pending_computed_ids_changed_from_baseline"),
        ChangedComputedIdCount == BaselineSnapshots.Num());
    Validation->SetArrayField(TEXT("scripts"), ScriptValues);
    return Validation;
}

bool JsonValueToNiagaraPinDefault(const TSharedPtr<FJsonValue>& Value, FString& OutDefault)
{
    if (!Value.IsValid())
    {
        return false;
    }
    if (Value->TryGetString(OutDefault))
    {
        return true;
    }
    double NumberValue = 0.0;
    if (Value->TryGetNumber(NumberValue))
    {
        OutDefault = FString::SanitizeFloat(NumberValue);
        return true;
    }
    bool BoolValue = false;
    if (Value->TryGetBool(BoolValue))
    {
        OutDefault = BoolValue ? TEXT("true") : TEXT("false");
        return true;
    }
    return false;
}

bool ReadNiagaraRecipeEndpoint(
    const TSharedPtr<FJsonObject>& Endpoint,
    const TMap<FString, UEdGraphNode*>& CreatedNodes,
    UNiagaraNodeCustomHlsl* CustomNode,
    EEdGraphPinDirection DesiredDirection,
    UEdGraphPin*& OutPin,
    FString& OutError,
    bool bResolveCustomInputUpstream)
{
    OutPin = nullptr;
    if(!ContinueRead())return false;
    if (!Endpoint.IsValid())
    {
        OutError = TEXT("Recipe endpoint is missing");
        return false;
    }

    FString Kind;
    FString PinName;
    FString NodeId;
    int32 PinOccurrence = 0;
    Endpoint->TryGetStringField(TEXT("kind"), Kind);
    Endpoint->TryGetStringField(TEXT("pin"), PinName);
    Endpoint->TryGetStringField(TEXT("node_id"), NodeId);
    Endpoint->TryGetNumberField(TEXT("pin_occurrence"), PinOccurrence);
    Kind.TrimStartAndEndInline();
    PinName.TrimStartAndEndInline();
    NodeId.TrimStartAndEndInline();

    if (Kind.Equals(TEXT("node"), ESearchCase::IgnoreCase))
    {
        UEdGraphNode* const* FoundNode = CreatedNodes.Find(NodeId);
        if (!FoundNode || !*FoundNode)
        {
            OutError = FString::Printf(TEXT("Recipe endpoint references unknown node_id: %s"), *NodeId);
            return false;
        }
        OutPin = FindNiagaraRecipePin(*FoundNode, DesiredDirection, PinName, PinOccurrence);
    }
    else if (Kind.Equals(TEXT("custom_hlsl_input"), ESearchCase::IgnoreCase))
    {
        if (DesiredDirection != EGPD_Output)
        {
            OutError = TEXT("custom_hlsl_input is valid only as a recipe source");
            return false;
        }
        UEdGraphPin* CustomInput = FindNiagaraRecipePin(CustomNode, EGPD_Input, PinName, PinOccurrence);
        if (!CustomInput)
        {
            OutError = FString::Printf(TEXT("Custom HLSL input pin was not found: %s"), *PinName);
            return false;
        }
        if (bResolveCustomInputUpstream)
        {
            if (CustomInput->LinkedTo.Num() != 1)
            {
                OutError = FString::Printf(
                    TEXT("Custom HLSL input '%s' must have exactly one upstream link for this endpoint; found %d"),
                    *PinName,
                    CustomInput->LinkedTo.Num());
                return false;
            }
            OutPin = CustomInput->LinkedTo[0];
        }
        else
        {
            OutPin = CustomInput;
        }
    }
    else
    {
        OutError = FString::Printf(TEXT("Unsupported recipe endpoint kind: %s"), *Kind);
        return false;
    }

    if (!OutPin)
    {
        OutError = FString::Printf(TEXT("Recipe pin was not found: kind=%s node_id=%s pin=%s"), *Kind, *NodeId, *PinName);
        return false;
    }
    return true;
}

FString BuildNiagaraRecipeGraphFingerprint(const UNiagaraGraph* Graph)
{
    if (!ContinueRead()||!Graph)
    {
        return FString();
    }

    TArray<FString> Entries;
    TSet<FString> UniqueLinks;
    for (int32 NodeIndex = 0; NodeIndex < Graph->Nodes.Num(); ++NodeIndex)
    {
        const UEdGraphNode* Node = Graph->Nodes[NodeIndex];
        if (!Node)
        {
            continue;
        }

        FString NodeEntry = FString::Printf(
            TEXT("node|index=%d|%s|%s|pos=%d,%d"),
            NodeIndex,
            *Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens),
            *Node->GetClass()->GetPathName(),
            Node->NodePosX,
            Node->NodePosY);
        if (const UNiagaraNodeCustomHlsl* CustomHlsl = Cast<UNiagaraNodeCustomHlsl>(Node))
        {
            const FString Body=GetNiagaraCustomHlslString(CustomHlsl);
            if(!ContinueRead())return FString();
            NodeEntry += TEXT("|hlsl=") + FMD5::HashAnsiString(*Body);
        }
        Entries.Add(MoveTemp(NodeEntry));

        for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
        {
            const UEdGraphPin* Pin = Node->Pins[PinIndex];
            if (!Pin || IsNiagaraDynamicAddPin(Pin))
            {
                continue;
            }

            const FString DefaultObjectPath = Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : FString();
            Entries.Add(FString::Printf(
                TEXT("pin|index=%d|%s|type=%s|default=%s|default_object=%s|default_text=%s"),
                PinIndex,
                *NiagaraRecipePinIdentity(Pin),
                *NiagaraTypeName(UEdGraphSchema_Niagara::PinToTypeDefinition(Pin)),
                *Pin->DefaultValue,
                *DefaultObjectPath,
                *Pin->DefaultTextValue.ToString()));

            for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (!LinkedPin)
                {
                    continue;
                }
                FString A = NiagaraRecipePinIdentity(Pin);
                FString B = NiagaraRecipePinIdentity(LinkedPin);
                if (B < A)
                {
                    Swap(A, B);
                }
                UniqueLinks.Add(A + TEXT("<->") + B);
            }
        }
    }

    for (const FString& Link : UniqueLinks)
    {
        Entries.Add(TEXT("link|") + Link);
    }
    Entries.Sort();
    return FMD5::HashAnsiString(*FString::Join(Entries, TEXT("\n")));
}

void SetNiagaraGraphRecipeReloadRequiredRecoveryContract(const TSharedPtr<FJsonObject>& Json)
{
    if (!Json.IsValid())
    {
        return;
    }

    // Structural repair cannot prove that Niagara's serialized IDs and caller
    // script state match the on-disk baseline.  Undo is therefore not offered
    // as a recovery guarantee; the package must be reloaded before retrying.
    Json->SetBoolField(TEXT("requires_package_reload"), true);
    Json->SetBoolField(TEXT("undo_recovery_unproven"), true);
    Json->SetBoolField(TEXT("caller_script_undo_recovery_supported"), false);
    Json->SetBoolField(TEXT("do_not_retry"), true);
    Json->SetStringField(TEXT("recovery_policy"), TEXT("reload_required"));
    Json->SetStringField(TEXT("rollback_policy"), TEXT("reload_required"));

    // Compatibility field for older clients.  recovery_policy is authoritative
    // and deliberately does not claim that Undo is a safe alternative.
    Json->SetBoolField(TEXT("requires_reload_or_undo"), true);
}

bool NiagaraCompileStatusIsClean(const TSharedPtr<FJsonObject>& Status, FString& OutError)
{
    if (!Status.IsValid() ||
        !Status->HasTypedField<EJson::Number>(TEXT("error_count")) ||
        !Status->HasTypedField<EJson::Number>(TEXT("warning_count")) ||
        !Status->HasTypedField<EJson::Number>(TEXT("dirty_count")) ||
        !Status->HasTypedField<EJson::Number>(TEXT("unknown_count")) ||
        !Status->HasTypedField<EJson::Number>(TEXT("missing_count")))
    {
        OutError = TEXT("Niagara compile status response is incomplete");
        return false;
    }

    const int32 ErrorCount = Status->GetIntegerField(TEXT("error_count"));
    const int32 WarningCount = Status->GetIntegerField(TEXT("warning_count"));
    const int32 DirtyCount = Status->GetIntegerField(TEXT("dirty_count"));
    const int32 UnknownCount = Status->GetIntegerField(TEXT("unknown_count"));
    const int32 MissingCount = Status->GetIntegerField(TEXT("missing_count"));
    const bool bWaitTimedOut = Status->HasTypedField<EJson::Boolean>(TEXT("wait_timed_out")) &&
        Status->GetBoolField(TEXT("wait_timed_out"));
    const bool bOutstanding = Status->HasTypedField<EJson::Boolean>(TEXT("outstanding_compilation_requests_after")) &&
        Status->GetBoolField(TEXT("outstanding_compilation_requests_after"));
    if (ErrorCount != 0 || WarningCount != 0 || DirtyCount != 0 || MissingCount != 0 || bWaitTimedOut || bOutstanding)
    {
        OutError = FString::Printf(
            TEXT("Niagara graph recipe requires a stable system state; errors=%d warnings=%d dirty=%d unknown=%d (stable baseline allowed) missing=%d timed_out=%s outstanding=%s"),
            ErrorCount,
            WarningCount,
            DirtyCount,
            UnknownCount,
            MissingCount,
            bWaitTimedOut ? TEXT("true") : TEXT("false"),
            bOutstanding ? TEXT("true") : TEXT("false"));
        return false;
    }
    return true;
}

bool NiagaraCompileStatusDidNotRegress(
    const TSharedPtr<FJsonObject>& Baseline,
    const TSharedPtr<FJsonObject>& Observed,
    FString& OutError)
{
    if (!Baseline.IsValid() || !Observed.IsValid() ||
        !Baseline->HasTypedField<EJson::Number>(TEXT("error_count")) ||
        !Baseline->HasTypedField<EJson::Number>(TEXT("warning_count")) ||
        !Baseline->HasTypedField<EJson::Number>(TEXT("unknown_count")) ||
        !Observed->HasTypedField<EJson::Number>(TEXT("error_count")) ||
        !Observed->HasTypedField<EJson::Number>(TEXT("warning_count")) ||
        !Observed->HasTypedField<EJson::Number>(TEXT("dirty_count")) ||
        !Observed->HasTypedField<EJson::Number>(TEXT("unknown_count")) ||
        !Observed->HasTypedField<EJson::Number>(TEXT("missing_count")))
    {
        OutError = TEXT("Niagara compile status response is incomplete");
        return false;
    }

    const int32 BaselineErrors = Baseline->GetIntegerField(TEXT("error_count"));
    const int32 BaselineWarnings = Baseline->GetIntegerField(TEXT("warning_count"));
    const int32 BaselineUnknown = Baseline->GetIntegerField(TEXT("unknown_count"));
    const int32 CurrentErrors = Observed->GetIntegerField(TEXT("error_count"));
    const int32 CurrentWarnings = Observed->GetIntegerField(TEXT("warning_count"));
    const int32 CurrentDirty = Observed->GetIntegerField(TEXT("dirty_count"));
    const int32 CurrentUnknown = Observed->GetIntegerField(TEXT("unknown_count"));
    const int32 CurrentMissing = Observed->GetIntegerField(TEXT("missing_count"));
    const bool bWaitTimedOut = Observed->HasTypedField<EJson::Boolean>(TEXT("wait_timed_out")) &&
        Observed->GetBoolField(TEXT("wait_timed_out"));
    const bool bOutstanding = Observed->HasTypedField<EJson::Boolean>(TEXT("outstanding_compilation_requests_after")) &&
        Observed->GetBoolField(TEXT("outstanding_compilation_requests_after"));
    if (CurrentErrors > BaselineErrors || CurrentWarnings > BaselineWarnings ||
        CurrentDirty != 0 || CurrentUnknown != BaselineUnknown || CurrentMissing != 0 || bWaitTimedOut || bOutstanding)
    {
        OutError = FString::Printf(
            TEXT("Niagara system compile gate failed: errors %d -> %d, warnings %d -> %d, dirty=%d unknown %d -> %d, missing=%d timed_out=%s outstanding=%s"),
            BaselineErrors,
            CurrentErrors,
            BaselineWarnings,
            CurrentWarnings,
            CurrentDirty,
            BaselineUnknown,
            CurrentUnknown,
            CurrentMissing,
            bWaitTimedOut ? TEXT("true") : TEXT("false"),
            bOutstanding ? TEXT("true") : TEXT("false"));
        return false;
    }
    return true;
}


struct FConversionCompileWait
{
 TSharedPtr<FJsonObject> Result;TWeakObjectPtr<UNiagaraSystem> System;double Start=0,Next=0,Timeout=30,Poll=.05;bool Done=false,Idle=false,TimedOut=false;int32 Iterations=0;
 void Begin(UNiagaraSystem* S,double T,double P)
 {
  System=S;Timeout=T;Poll=P;Start=FPlatformTime::Seconds();Next=0;Done=false;Idle=false;TimedOut=false;Iterations=0;
  Result=MakeShared<FJsonObject>();Result->SetBoolField(TEXT("wait_for_completion"),true);Result->SetNumberField(TEXT("timeout_seconds"),T);Result->SetNumberField(TEXT("poll_interval_seconds"),P);Result->SetBoolField(TEXT("wait_timed_out"),false);Result->SetNumberField(TEXT("wait_iterations"),0);Result->SetNumberField(TEXT("wait_elapsed_seconds"),0);
 }
 bool Tick(FWork& W)
 {
  if(Done)return true;if(FPlatformTime::Seconds()<Next)return false;if(!W.Admit(TEXT("conversion_wait_admission")))return true;
  UNiagaraSystem* S=System.Get();if(!S){W.Fail(TEXT("niagara_hlsl_conversion_wait_target_missing"),TEXT("Exact captured System expired."));return true;}
  auto Read=[&]()->bool
  {
   if(!W.SDK(TEXT("Conversion_HasOutstandingCompilationRequests")))return false;const bool O=S->HasOutstandingCompilationRequests(true);Result->SetBoolField(TEXT("outstanding_compilation_requests_after"),O);if(!W.After(TEXT("Conversion_HasOutstandingCompilationRequests")))return false;
   if(!W.SDK(TEXT("Conversion_HasActiveCompilations")))return false;const bool A=S->HasActiveCompilations();Result->SetBoolField(TEXT("active_compilations_after"),A);if(!W.After(TEXT("Conversion_HasActiveCompilations")))return false;Idle=!O&&!A;return true;
  };
  if(!Read())return true;
  if(!Idle)
  {
   if(!W.SDK(TEXT("Conversion_PollForCompilationComplete")))return true;S->PollForCompilationComplete();if(!W.After(TEXT("Conversion_PollForCompilationComplete")))return true;
   if(!Read())return true;
   if(!Idle&&FPlatformTime::Seconds()>=Start+Timeout)TimedOut=true;
   if(!Idle&&!TimedOut)++Iterations;
  }
  Result->SetBoolField(TEXT("wait_timed_out"),TimedOut);Result->SetNumberField(TEXT("wait_iterations"),Iterations);Result->SetNumberField(TEXT("wait_elapsed_seconds"),FMath::Max(0.0,FPlatformTime::Seconds()-Start));Result->SetBoolField(TEXT("actual_wait_performed"),true);
  Done=Idle||TimedOut;if(!Done)Next=FPlatformTime::Seconds()+Poll;return Done;
 }
 bool Clean()const{return Done&&Idle&&!TimedOut;}
};
bool RequestConversionCompile(FWork& W,UNiagaraSystem* S,const TCHAR* Stage,bool& Accepted)
{
 if(!W.SDK(Stage,true,true))return false;W.R.bCompileRequested=true;W.R.bCompileAttempted=true;W.R.Compiled.Reset();
 if(W.DeferredMode&&S==W.Target.Get())++W.OwnedCompileEpoch;
  Accepted=S->RequestCompile(false);W.R.Data->SetBoolField(FString(Stage)+TEXT("_accepted"),Accepted);return W.After(Stage);
}
TSharedPtr<FJsonObject> InspectRecipeCompileStatus(FWork& W,UNiagaraSystem* S)
{
 auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("system_path"),S->GetPathName());P->SetBoolField(TEXT("request_compile"),false);P->SetBoolField(TEXT("force"),false);P->SetBoolField(TEXT("allow_source_compile"),false);P->SetBoolField(TEXT("wait_for_completion"),false);
 FString Encoded;if(!W.Step(TEXT("Recipe_InspectCompileStatus"),[&]{Encoded=NiagaraAuthoringInspection::InspectCompileStatus(Json(P));}))return nullptr;
 TSharedPtr<FJsonObject> Typed;if(!Parse(Encoded,Typed)){W.Fail(TEXT("niagara_graph_recipe_compile_status_invalid"),TEXT("Bounded typed compile status response required."));return nullptr;}
 bool Health=false,Post=false;Typed->TryGetBoolField(TEXT("success"),Health);Typed->TryGetBoolField(TEXT("postcondition_passed"),Post);
 const TSharedPtr<FJsonObject>* Data=nullptr;const TSharedPtr<FJsonObject>* Legacy=nullptr;
 if(!Health||!Post||!Typed->TryGetObjectField(TEXT("data"),Data)||!Data||!(*Data)->TryGetObjectField(TEXT("legacy_result"),Legacy)||!Legacy){W.Fail(TEXT("niagara_graph_recipe_compile_status_failed"),TEXT("Existing read-only compile-status leaf did not complete; no replay."));return nullptr;}
 return *Legacy;
}


struct FReplacementContinuation
{
 FWork& W;UNiagaraSystem* System;UNiagaraScriptSource* MatchedSource;UNiagaraNodeCustomHlsl* MatchedNode;int32 MatchedNodeIndex,MaxHlslChars;FString ReplacementHlsl;bool bSave,bRequestCompile;double TimeoutSeconds,PollIntervalSeconds;TSharedPtr<FJsonObject> Result;FConversionCompileWait Wait;int32 Phase=0;
 TArray<FNiagaraScriptCompileSnapshot> BeforeCompileSnapshots,AfterMutationSnapshots;TArray<FString> AffectedScriptPaths;
 bool bReadbackVerified=false,bNodeChangeIdChanged=false,bSourceOnlyOwnersCovered=true,bSourceInvalidationVerified=false,bCompileLaunched=false,bCompileCompleted=false,bCompileEffectVerified=false;
 int32 CompiledIdChangedCount=0,CompiledDataAppliedCount=0,StableCompileInputCount=0,AffectedCompileErrorCount=0;TArray<TSharedPtr<FJsonValue>> CompileEffects;
 FReplacementContinuation(FWork& InW,UNiagaraSystem* InS,UNiagaraScriptSource* InSource,UNiagaraNodeCustomHlsl* InNode,int32 Index,int32 Max,const FString& Body,bool Save,bool Compile,double Timeout,double Poll,const TSharedPtr<FJsonObject>& Out):W(InW),System(InS),MatchedSource(InSource),MatchedNode(InNode),MatchedNodeIndex(Index),MaxHlslChars(Max),ReplacementHlsl(Body),bSave(Save),bRequestCompile(Compile),TimeoutSeconds(Timeout),PollIntervalSeconds(Poll),Result(Out){Wait.Begin(System,TimeoutSeconds,PollIntervalSeconds);}
 bool Mutate()
 {
    W.Persistent=true;W.SourceCaptured=false;if(!W.Admit(TEXT("replace_prewrite_idle")))return false;
    BeforeCompileSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
    if(!ContinueRead())return false;
    const FGuid NodeChangeIdBefore = MatchedNode->GetChangeId();
    const FGuid GraphChangeIdBefore = MatchedSource->NodeGraph ? MatchedSource->NodeGraph->GetChangeID() : FGuid();
    Result->SetStringField(TEXT("node_change_id_before"), NodeChangeIdBefore.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("graph_change_id_before"), GraphChangeIdBefore.ToString(EGuidFormats::DigitsWithHyphens));

    if(!W.Step(TEXT("HLSL_System_Modify"),[&]{System->Modify();},true,true)||!W.Step(TEXT("HLSL_Source_Modify"),[&]{MatchedSource->Modify();},true,true))return false;
    if (!SetNiagaraCustomHlslString(MatchedNode, ReplacementHlsl))
    {
        Result->SetStringField(TEXT("error"), TEXT("Failed to update Niagara Custom HLSL string"));
        Result->SetStringField(TEXT("write_scope"), TEXT("custom_hlsl_replace_failed_during_mutation"));
        return false;
    }

    const FString ReadbackHlsl = GetNiagaraCustomHlslString(MatchedNode);
    if(!ContinueRead())return false;
    bReadbackVerified = ReadbackHlsl.Equals(ReplacementHlsl,ESearchCase::CaseSensitive);
    const FGuid NodeChangeIdAfter = MatchedNode->GetChangeId();
    const FGuid GraphChangeIdAfter = MatchedSource->NodeGraph ? MatchedSource->NodeGraph->GetChangeID() : FGuid();
    bNodeChangeIdChanged = NodeChangeIdAfter.IsValid() && NodeChangeIdAfter != NodeChangeIdBefore;
    const bool bGraphChangeIdChanged = GraphChangeIdAfter.IsValid() && GraphChangeIdAfter != GraphChangeIdBefore;

    AfterMutationSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
    if(!ContinueRead())return false;
    AffectedScriptPaths.Reset();
    TArray<FString> RequiredSystemOwnerPaths;
    TArray<TSharedPtr<FJsonValue>> SourceOnlyCompileInputs;
    TArray<TSharedPtr<FJsonValue>> MutationCompileInputs;
    for (const FNiagaraScriptCompileSnapshot& AfterMutationSnapshot : AfterMutationSnapshots)
    {
        const FNiagaraScriptCompileSnapshot* BeforeSnapshot = FindNiagaraScriptCompileSnapshot(
            BeforeCompileSnapshots,
            AfterMutationSnapshot.ScriptPath);
        if (!BeforeSnapshot || BeforeSnapshot->ComputedId == AfterMutationSnapshot.ComputedId)
        {
            continue;
        }

        // UE 5.6 folds emitter spawn/update into the corresponding system script.
        // These source-only scripts never receive standalone VM executable IDs.
        // Keep their invalidation evidence and require the compiled owner below.
        if (AfterMutationSnapshot.Script && !AfterMutationSnapshot.Script->IsCompilable())
        {
            UNiagaraScript* CompiledOwner = AfterMutationSnapshot.Script->IsEmitterSpawnScript()
                ? System->GetSystemSpawnScript() : System->GetSystemUpdateScript();
            const FString OwnerPath = CompiledOwner ? CompiledOwner->GetPathName() : FString();
            RequiredSystemOwnerPaths.AddUnique(OwnerPath);
            TSharedPtr<FJsonObject> SourceObject = BuildNiagaraScriptCompileSnapshotJson(
                AfterMutationSnapshot, SourceOnlyCompileInputs.Num());
            SourceObject->SetStringField(TEXT("compiled_owner_path"), OwnerPath);
            SourceObject->SetStringField(TEXT("verification_route"), TEXT("source_only_emitter_via_system_owner"));
            SourceOnlyCompileInputs.Add(MakeShared<FJsonValueObject>(SourceObject));
            continue;
        }

        AffectedScriptPaths.Add(AfterMutationSnapshot.ScriptPath);
        TSharedPtr<FJsonObject> ScriptObject = BuildNiagaraScriptCompileSnapshotJson(
            AfterMutationSnapshot,
            MutationCompileInputs.Num());
        ScriptObject->SetStringField(TEXT("computed_id_before_fingerprint"), NiagaraCompileIdFingerprint(BeforeSnapshot->ComputedId));
        ScriptObject->SetBoolField(TEXT("computed_id_changed_by_mutation"), true);
        ScriptObject->SetBoolField(
            TEXT("source_out_of_sync_after_mutation"),
            AfterMutationSnapshot.ComputedId.IsValid() && AfterMutationSnapshot.ComputedId != AfterMutationSnapshot.CachedId);
        MutationCompileInputs.Add(MakeShared<FJsonValueObject>(ScriptObject));
    }

    bSourceOnlyOwnersCovered = true;
    for (const FString& OwnerPath : RequiredSystemOwnerPaths)
    {
        bSourceOnlyOwnersCovered &= !OwnerPath.IsEmpty() && AffectedScriptPaths.Contains(OwnerPath);
    }
    Result->SetArrayField(TEXT("source_only_compile_inputs"), SourceOnlyCompileInputs);
    Result->SetBoolField(TEXT("source_only_owners_covered"), bSourceOnlyOwnersCovered);

    bSourceInvalidationVerified =
        bReadbackVerified &&
        bNodeChangeIdChanged &&
        bSourceOnlyOwnersCovered &&
        AffectedScriptPaths.Num() > 0;

    if(!W.Step(TEXT("HLSL_MarkPackageDirty"),[&]{System->MarkPackageDirty();},true,true))return false;
    Result->SetBoolField(TEXT("mutation_performed"), true);
    Result->SetBoolField(TEXT("readback_verified"), bReadbackVerified);
    Result->SetBoolField(TEXT("node_change_id_changed"), bNodeChangeIdChanged);
    Result->SetBoolField(TEXT("graph_change_id_changed"), bGraphChangeIdChanged);
    Result->SetBoolField(TEXT("source_invalidation_verified"), bSourceInvalidationVerified);
    Result->SetStringField(TEXT("node_change_id_after"), NodeChangeIdAfter.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("graph_change_id_after"), GraphChangeIdAfter.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("affected_script_count"), AffectedScriptPaths.Num());
    Result->SetArrayField(TEXT("mutation_compile_inputs"), MutationCompileInputs);
    Result->SetObjectField(TEXT("after"), BuildCustomHlslNodeJson(MatchedSource, MatchedNode, MatchedNodeIndex, MaxHlslChars));

  return true;
 }
 void CompileEffect()
 {
        const TArray<FNiagaraScriptCompileSnapshot> FinalCompileSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
        if(!ContinueRead())return;
        for (const FString& AffectedScriptPath : AffectedScriptPaths)
        {
            const FNiagaraScriptCompileSnapshot* BeforeSnapshot = FindNiagaraScriptCompileSnapshot(
                BeforeCompileSnapshots,
                AffectedScriptPath);
            const FNiagaraScriptCompileSnapshot* ExpectedSnapshot = FindNiagaraScriptCompileSnapshot(
                AfterMutationSnapshots,
                AffectedScriptPath);
            const FNiagaraScriptCompileSnapshot* FinalSnapshot = FindNiagaraScriptCompileSnapshot(
                FinalCompileSnapshots,
                AffectedScriptPath);
            if (!BeforeSnapshot || !ExpectedSnapshot || !FinalSnapshot)
            {
                continue;
            }

            const bool bCompiledIdChanged = FinalSnapshot->CachedId.IsValid() && FinalSnapshot->CachedId != BeforeSnapshot->CachedId;
            const bool bCompileInputStable = FinalSnapshot->ComputedId.IsValid() && FinalSnapshot->ComputedId == ExpectedSnapshot->ComputedId;
            const bool bCompiledDataApplied =
                FinalSnapshot->CachedId.IsValid() &&
                ExpectedSnapshot->ComputedId.IsValid() &&
                FinalSnapshot->CachedId == ExpectedSnapshot->ComputedId;
            const bool bHasError = IsNiagaraCompileErrorStatus(FinalSnapshot->CompileStatus);

            CompiledIdChangedCount += bCompiledIdChanged ? 1 : 0;
            StableCompileInputCount += bCompileInputStable ? 1 : 0;
            CompiledDataAppliedCount += bCompiledDataApplied ? 1 : 0;
            AffectedCompileErrorCount += bHasError ? 1 : 0;

            TSharedPtr<FJsonObject> ScriptObject = BuildNiagaraScriptCompileSnapshotJson(
                *FinalSnapshot,
                CompileEffects.Num());
            ScriptObject->SetStringField(TEXT("cached_id_before_fingerprint"), NiagaraCompileIdFingerprint(BeforeSnapshot->CachedId));
            ScriptObject->SetStringField(TEXT("expected_post_edit_id_fingerprint"), NiagaraCompileIdFingerprint(ExpectedSnapshot->ComputedId));
            ScriptObject->SetBoolField(TEXT("compiled_id_changed"), bCompiledIdChanged);
            ScriptObject->SetBoolField(TEXT("compile_input_stable"), bCompileInputStable);
            ScriptObject->SetBoolField(TEXT("compiled_data_applied"), bCompiledDataApplied);
            CompileEffects.Add(MakeShared<FJsonValueObject>(ScriptObject));
        }

        bCompileEffectVerified =
            bCompileLaunched &&
            bCompileCompleted &&
            bSourceOnlyOwnersCovered &&
            AffectedScriptPaths.Num() > 0 &&
            CompiledIdChangedCount == AffectedScriptPaths.Num() &&
            StableCompileInputCount == AffectedScriptPaths.Num() &&
            CompiledDataAppliedCount == AffectedScriptPaths.Num() &&
            AffectedCompileErrorCount == 0;
  if(bCompileCompleted)W.R.Compiled=bCompileEffectVerified;
  W.R.Data->SetBoolField(TEXT("actual_compile_completion_verified"),bCompileEffectVerified);
 }
 bool Complete()
 {
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("compile_launched"), bCompileLaunched);
    Result->SetBoolField(TEXT("compile_completed"), bCompileCompleted);
    Result->SetBoolField(TEXT("compile_effect_verified"), bCompileEffectVerified);
    Result->SetNumberField(TEXT("compiled_id_changed_count"), CompiledIdChangedCount);
    Result->SetNumberField(TEXT("stable_compile_input_count"), StableCompileInputCount);
    Result->SetNumberField(TEXT("compiled_data_applied_count"), CompiledDataAppliedCount);
    Result->SetNumberField(TEXT("affected_compile_error_count"), AffectedCompileErrorCount);
    Result->SetArrayField(TEXT("compile_effects"), CompileEffects);

    const bool bPreSaveVerificationPass =
        bReadbackVerified &&
        bSourceInvalidationVerified &&
        (!bRequestCompile || bCompileEffectVerified);

    bool bSaved = false;
    if (bSave && bPreSaveVerificationPass)
    {
        bSaved = SaveHlslSystem(W,System);if(!ContinueRead())return true;
    }

    const bool bPostSaveReadbackVerified = GetNiagaraCustomHlslString(MatchedNode).Equals(ReplacementHlsl,ESearchCase::CaseSensitive);
    if(!ContinueRead())return true;
    const bool bSaveVerified = !bSave || bSaved;
    const bool bVerificationPass = bPreSaveVerificationPass && bSaveVerified && bPostSaveReadbackVerified;

    Result->SetBoolField(TEXT("pre_save_verification_pass"), bPreSaveVerificationPass);
    Result->SetBoolField(TEXT("save_blocked_by_verification"), bSave && !bPreSaveVerificationPass);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("save_verified"), bSaveVerified);
    Result->SetBoolField(TEXT("post_save_readback_verified"), bPostSaveReadbackVerified);
    Result->SetBoolField(TEXT("package_dirty_after"), System->GetOutermost() ? System->GetOutermost()->IsDirty() : false);
    Result->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    Result->SetBoolField(TEXT("success"), bVerificationPass);
    Result->SetStringField(TEXT("write_scope"), TEXT("custom_hlsl_replace"));

    if (!bVerificationPass)
    {
        FString ErrorMessage;
        if (!bReadbackVerified)
        {
            ErrorMessage = TEXT("Custom HLSL readback did not match the requested replacement");
        }
        else if (!bNodeChangeIdChanged)
        {
            ErrorMessage = TEXT("Custom HLSL changed but the Niagara node ChangeId did not change");
        }
        else if (AffectedScriptPaths.Num() == 0)
        {
            ErrorMessage = TEXT("Custom HLSL changed but no dependent Niagara script Compile ID was invalidated");
        }
        else if (bRequestCompile && !bCompileLaunched)
        {
            ErrorMessage = TEXT("Niagara compile was requested but no compilation was launched");
        }
        else if (bRequestCompile && !bCompileCompleted)
        {
            ErrorMessage = TEXT("Niagara compilation did not complete before the timeout");
        }
        else if (bRequestCompile && !bCompileEffectVerified)
        {
            ErrorMessage = TEXT("Niagara compilation completed without applying the expected post-edit Compile IDs to every affected script");
        }
        else if (bSave && !bSaved)
        {
            ErrorMessage = TEXT("Failed to save Niagara system after verified Custom HLSL replacement");
        }
        else
        {
            ErrorMessage = TEXT("Custom HLSL replacement failed postcondition verification");
        }
        Result->SetStringField(TEXT("error"), ErrorMessage);
    }
    return true;
 }
 bool Tick()
 {
  if(!W.Admit(TEXT("replace_continuation_phase")))return true;
  if(Phase==0)
  {
   if(!Wait.Tick(W))return false;if(!W.R.Error.IsEmpty())return true;
   Result->SetObjectField(TEXT("preexisting_compile_wait"),Wait.Result);Result->SetBoolField(TEXT("preexisting_compile_idle"),Wait.Clean());
   if(!Wait.Clean()){Result->SetStringField(TEXT("error"),TEXT("Timed out waiting for pre-existing Niagara compilation before Custom HLSL replacement"));Result->SetStringField(TEXT("write_scope"),TEXT("custom_hlsl_replace_blocked_before_mutation"));return true;}
   Phase=1;return false;
  }
  if(Phase==1)
  {
   if(!Mutate())return true;
   if(bRequestCompile)
   {
    if(!RequestConversionCompile(W,System,TEXT("HLSL_RequestCompile"),bCompileLaunched))return true;
    Result->SetBoolField(TEXT("compile_requested"),true);Result->SetBoolField(TEXT("compile_launched"),bCompileLaunched);
    Wait.Begin(System,TimeoutSeconds,PollIntervalSeconds);Phase=2;
   }
   else Phase=3;
   return false;
  }
  if(Phase==2)
  {
   if(!Wait.Tick(W))return false;if(!W.R.Error.IsEmpty())return true;
   Result->SetObjectField(TEXT("compile_wait"),Wait.Result);bCompileCompleted=Wait.Clean();CompileEffect();if(!W.R.Error.IsEmpty())return true;Phase=3;return false;
  }
  return Complete();
 }
};

TSharedPtr<FJsonObject> HandleReplace(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FString SystemPath;
    FString ReplacementHlsl;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return ErrorResponse(TEXT("Missing 'system_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("replacement_hlsl"), ReplacementHlsl))
    {
        return ErrorResponse(TEXT("Missing 'replacement_hlsl' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return ErrorResponse(
            FString::Printf(TEXT("Refusing to replace Niagara Custom HLSL outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    FString NodeGuid;
    FString SignatureName;
    FString ContainsText;
    FString ExpectedContains;
    FString ScratchPadName;
    FString SourcePathContains;
    Params->TryGetStringField(TEXT("node_guid"), NodeGuid);
    Params->TryGetStringField(TEXT("signature_name"), SignatureName);
    Params->TryGetStringField(TEXT("contains_text"), ContainsText);
    Params->TryGetStringField(TEXT("expected_contains"), ExpectedContains);
    Params->TryGetStringField(TEXT("scratch_pad_name"), ScratchPadName);
    Params->TryGetStringField(TEXT("source_path_contains"), SourcePathContains);
    NodeGuid.TrimStartAndEndInline();
    SignatureName.TrimStartAndEndInline();
    ScratchPadName.TrimStartAndEndInline();
    SourcePathContains.TrimStartAndEndInline();

    if (NodeGuid.IsEmpty() && SignatureName.IsEmpty() && ContainsText.IsEmpty())
    {
        return ErrorResponse(TEXT("replace_niagara_custom_hlsl requires 'node_guid', 'signature_name', or 'contains_text'"));
    }

    bool bDryRun = true;
    Params->TryGetBoolField(TEXT("dry_run"), bDryRun);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    double TimeoutSeconds = 30.0;
    Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
    TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 0.1, 60.0);

    double PollIntervalSeconds = 0.05;
    Params->TryGetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);
    PollIntervalSeconds = FMath::Clamp(PollIntervalSeconds, 0.02, 1.0);

    int32 MaxHlslChars = 8000;
    Params->TryGetNumberField(TEXT("max_hlsl_chars"), MaxHlslChars);
    MaxHlslChars = FMath::Clamp(MaxHlslChars, -1, 200000);

    UNiagaraSystem* System = LoadSystem(SystemPath);
    if (!System)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraScriptSource* MatchedSource = nullptr;
    UNiagaraNodeCustomHlsl* MatchedNode = nullptr;
    int32 MatchedNodeIndex = INDEX_NONE;
    int32 MatchCount = 0;
    auto& W=*Current;
    const TArray<UNiagaraScriptSource*> Sources = CollectTargetNiagaraScriptSources(System);if(!ContinueRead())return ErrorResponse(W.R.Error);
    for (UNiagaraScriptSource* Source : Sources)
    {
        if (!Source || !Source->NodeGraph)
        {
            continue;
        }
        if (!SourcePathContains.IsEmpty() && !Source->GetPathName().Contains(SourcePathContains, ESearchCase::CaseSensitive))
        {
            continue;
        }
        if (!ScratchPadName.IsEmpty())
        {
            UNiagaraScript* SourceScript = Source->GetTypedOuter<UNiagaraScript>();
            if (!SourceScript || !SourceScript->GetName().Equals(ScratchPadName, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }

        TArray<UNiagaraNodeCustomHlsl*> CustomNodes;
        Source->NodeGraph->GetNodesOfClass<UNiagaraNodeCustomHlsl>(CustomNodes);
        for (int32 NodeIndex = 0; NodeIndex < CustomNodes.Num(); ++NodeIndex)
        {
            UNiagaraNodeCustomHlsl* CustomNode = CustomNodes[NodeIndex];
            if (!NiagaraCustomHlslNodeMatches(CustomNode, NodeGuid, SignatureName, ContainsText))
            {
                if(!ContinueRead())return ErrorResponse(W.R.Error);continue;
            }
            if(!ContinueRead())return ErrorResponse(W.R.Error);
            ++MatchCount;
            MatchedSource = Source;
            MatchedNode = CustomNode;
            MatchedNodeIndex = NodeIndex;
        }
    }

    if (MatchCount == 0 || !MatchedSource || !MatchedNode)
    {
        return ErrorResponse(TEXT("No matching Niagara Custom HLSL node was found"));
    }
    if (MatchCount > 1)
    {
        return ErrorResponse(
            FString::Printf(TEXT("Matched %d Custom HLSL nodes; refine node_guid or signature_name before replacing"), MatchCount));
    }

    const FString PreviousHlsl = GetNiagaraCustomHlslString(MatchedNode);
    if(!ContinueRead())return ErrorResponse(W.R.Error);
    if (!ExpectedContains.IsEmpty() && !PreviousHlsl.Contains(ExpectedContains, ESearchCase::CaseSensitive))
    {
        return ErrorResponse(TEXT("Matched Custom HLSL node did not contain expected_contains; refusing replacement"));
    }

    const bool bValueChanged = !PreviousHlsl.Equals(ReplacementHlsl,ESearchCase::CaseSensitive);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetBoolField(TEXT("dry_run"), bDryRun);
    Result->SetStringField(TEXT("node_guid"), MatchedNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("signature_name"), MatchedNode->Signature.GetNameString());
    Result->SetStringField(TEXT("source_path"), MatchedSource->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_name_filter"), ScratchPadName);
    Result->SetStringField(TEXT("source_path_contains_filter"), SourcePathContains);
    Result->SetNumberField(TEXT("node_index"), MatchedNodeIndex);
    Result->SetNumberField(TEXT("match_count"), MatchCount);
    Result->SetNumberField(TEXT("previous_hlsl_length"), PreviousHlsl.Len());
    Result->SetNumberField(TEXT("replacement_hlsl_length"), ReplacementHlsl.Len());
    Result->SetObjectField(TEXT("before"), BuildCustomHlslNodeJson(MatchedSource, MatchedNode, MatchedNodeIndex, MaxHlslChars));
    Result->SetBoolField(TEXT("value_changed"), bValueChanged);
    Result->SetBoolField(TEXT("mutation_performed"), false);
    Result->SetBoolField(TEXT("readback_verified"), false);
    Result->SetBoolField(TEXT("node_change_id_changed"), false);
    Result->SetBoolField(TEXT("source_invalidation_verified"), false);
    Result->SetBoolField(TEXT("compile_requested"), false);
    Result->SetBoolField(TEXT("compile_launched"), false);
    Result->SetBoolField(TEXT("compile_completed"), false);
    Result->SetBoolField(TEXT("compile_effect_verified"), false);
    Result->SetBoolField(TEXT("save_requested"), bSave);
    Result->SetBoolField(TEXT("saved"), false);
    Result->SetBoolField(TEXT("verification_pass"), false);
    Result->SetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
    Result->SetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);

    W.R.Data->SetObjectField(TEXT("legacy_result"),Result);if(!ContinueRead())return Result;
    if (bDryRun)
    {
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("preflight_pass"), true);
        Result->SetBoolField(TEXT("verification_pass"), true);
        Result->SetBoolField(TEXT("effect_verification_required"), false);
        Result->SetStringField(TEXT("write_scope"), TEXT("dry_run_custom_hlsl_replace"));
        return Result;
    }

    if (!bValueChanged)
    {
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("no_op"), true);
        Result->SetStringField(TEXT("no_op_reason"), TEXT("already_equal"));
        Result->SetBoolField(TEXT("readback_verified"), true);
        Result->SetBoolField(TEXT("compile_not_required"), true);
        Result->SetBoolField(TEXT("save_not_required"), true);
        Result->SetBoolField(TEXT("verification_pass"), true);
        Result->SetObjectField(TEXT("after"), BuildCustomHlslNodeJson(MatchedSource, MatchedNode, MatchedNodeIndex, MaxHlslChars));
        Result->SetStringField(TEXT("write_scope"), TEXT("no_op_custom_hlsl_replace"));
        return Result;
    }

    if(!WritableGraph(W,MatchedSource,MatchedNode))return Result;
    auto PreexistingCompileWait=PointCompileSnapshot(W,System,TimeoutSeconds,PollIntervalSeconds);Result->SetObjectField(TEXT("preexisting_compile_wait"),PreexistingCompileWait);if(!ContinueRead())return Result;
    const bool bPreexistingCompileIdle=!PreexistingCompileWait->GetBoolField(TEXT("outstanding_compilation_requests_after"))&&!PreexistingCompileWait->GetBoolField(TEXT("active_compilations_after"));Result->SetBoolField(TEXT("preexisting_compile_idle"),bPreexistingCompileIdle);
    if(W.DeferredMode){auto State=MakeShared<FReplacementContinuation>(W,System,MatchedSource,MatchedNode,MatchedNodeIndex,MaxHlslChars,ReplacementHlsl,bSave,bRequestCompile,TimeoutSeconds,PollIntervalSeconds,Result);W.DeferredStep=[State](){return State->Tick();};return Result;}
    if(!bPreexistingCompileIdle||bRequestCompile){W.Fail(TEXT("niagara_hlsl_conversion_async_compile_effect_unsupported"),TEXT("Changing replacement needs genuine compile-idle/ID-effect wait; requested compilation or preexisting pending is rejected before Modify"));Result->SetStringField(TEXT("error"),W.R.Error);Result->SetStringField(TEXT("write_scope"),TEXT("custom_hlsl_replace_blocked_before_mutation"));return Result;}
    W.Persistent=true;W.SourceCaptured=false;if(!W.Admit(TEXT("replace_prewrite_idle")))return Result;
    const TArray<FNiagaraScriptCompileSnapshot> BeforeCompileSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
    if(!ContinueRead())return Result;
    const FGuid NodeChangeIdBefore = MatchedNode->GetChangeId();
    const FGuid GraphChangeIdBefore = MatchedSource->NodeGraph ? MatchedSource->NodeGraph->GetChangeID() : FGuid();
    Result->SetStringField(TEXT("node_change_id_before"), NodeChangeIdBefore.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("graph_change_id_before"), GraphChangeIdBefore.ToString(EGuidFormats::DigitsWithHyphens));

    if(!W.Step(TEXT("HLSL_System_Modify"),[&]{System->Modify();},true,true)||!W.Step(TEXT("HLSL_Source_Modify"),[&]{MatchedSource->Modify();},true,true))return Result;
    if (!SetNiagaraCustomHlslString(MatchedNode, ReplacementHlsl))
    {
        Result->SetStringField(TEXT("error"), TEXT("Failed to update Niagara Custom HLSL string"));
        Result->SetStringField(TEXT("write_scope"), TEXT("custom_hlsl_replace_failed_during_mutation"));
        return Result;
    }

    const FString ReadbackHlsl = GetNiagaraCustomHlslString(MatchedNode);
    if(!ContinueRead())return Result;
    const bool bReadbackVerified = ReadbackHlsl.Equals(ReplacementHlsl,ESearchCase::CaseSensitive);
    const FGuid NodeChangeIdAfter = MatchedNode->GetChangeId();
    const FGuid GraphChangeIdAfter = MatchedSource->NodeGraph ? MatchedSource->NodeGraph->GetChangeID() : FGuid();
    const bool bNodeChangeIdChanged = NodeChangeIdAfter.IsValid() && NodeChangeIdAfter != NodeChangeIdBefore;
    const bool bGraphChangeIdChanged = GraphChangeIdAfter.IsValid() && GraphChangeIdAfter != GraphChangeIdBefore;

    const TArray<FNiagaraScriptCompileSnapshot> AfterMutationSnapshots = CaptureNiagaraSystemCompileSnapshots(System);
    if(!ContinueRead())return Result;
    TArray<FString> AffectedScriptPaths;
    TArray<FString> RequiredSystemOwnerPaths;
    TArray<TSharedPtr<FJsonValue>> SourceOnlyCompileInputs;
    TArray<TSharedPtr<FJsonValue>> MutationCompileInputs;
    for (const FNiagaraScriptCompileSnapshot& AfterMutationSnapshot : AfterMutationSnapshots)
    {
        const FNiagaraScriptCompileSnapshot* BeforeSnapshot = FindNiagaraScriptCompileSnapshot(
            BeforeCompileSnapshots,
            AfterMutationSnapshot.ScriptPath);
        if (!BeforeSnapshot || BeforeSnapshot->ComputedId == AfterMutationSnapshot.ComputedId)
        {
            continue;
        }

        // UE 5.6 folds emitter spawn/update into the corresponding system script.
        // These source-only scripts never receive standalone VM executable IDs.
        // Keep their invalidation evidence and require the compiled owner below.
        if (AfterMutationSnapshot.Script && !AfterMutationSnapshot.Script->IsCompilable())
        {
            UNiagaraScript* CompiledOwner = AfterMutationSnapshot.Script->IsEmitterSpawnScript()
                ? System->GetSystemSpawnScript() : System->GetSystemUpdateScript();
            const FString OwnerPath = CompiledOwner ? CompiledOwner->GetPathName() : FString();
            RequiredSystemOwnerPaths.AddUnique(OwnerPath);
            TSharedPtr<FJsonObject> SourceObject = BuildNiagaraScriptCompileSnapshotJson(
                AfterMutationSnapshot, SourceOnlyCompileInputs.Num());
            SourceObject->SetStringField(TEXT("compiled_owner_path"), OwnerPath);
            SourceObject->SetStringField(TEXT("verification_route"), TEXT("source_only_emitter_via_system_owner"));
            SourceOnlyCompileInputs.Add(MakeShared<FJsonValueObject>(SourceObject));
            continue;
        }

        AffectedScriptPaths.Add(AfterMutationSnapshot.ScriptPath);
        TSharedPtr<FJsonObject> ScriptObject = BuildNiagaraScriptCompileSnapshotJson(
            AfterMutationSnapshot,
            MutationCompileInputs.Num());
        ScriptObject->SetStringField(TEXT("computed_id_before_fingerprint"), NiagaraCompileIdFingerprint(BeforeSnapshot->ComputedId));
        ScriptObject->SetBoolField(TEXT("computed_id_changed_by_mutation"), true);
        ScriptObject->SetBoolField(
            TEXT("source_out_of_sync_after_mutation"),
            AfterMutationSnapshot.ComputedId.IsValid() && AfterMutationSnapshot.ComputedId != AfterMutationSnapshot.CachedId);
        MutationCompileInputs.Add(MakeShared<FJsonValueObject>(ScriptObject));
    }

    bool bSourceOnlyOwnersCovered = true;
    for (const FString& OwnerPath : RequiredSystemOwnerPaths)
    {
        bSourceOnlyOwnersCovered &= !OwnerPath.IsEmpty() && AffectedScriptPaths.Contains(OwnerPath);
    }
    Result->SetArrayField(TEXT("source_only_compile_inputs"), SourceOnlyCompileInputs);
    Result->SetBoolField(TEXT("source_only_owners_covered"), bSourceOnlyOwnersCovered);

    const bool bSourceInvalidationVerified =
        bReadbackVerified &&
        bNodeChangeIdChanged &&
        bSourceOnlyOwnersCovered &&
        AffectedScriptPaths.Num() > 0;

    if(!W.Step(TEXT("HLSL_MarkPackageDirty"),[&]{System->MarkPackageDirty();},true,true))return Result;
    Result->SetBoolField(TEXT("mutation_performed"), true);
    Result->SetBoolField(TEXT("readback_verified"), bReadbackVerified);
    Result->SetBoolField(TEXT("node_change_id_changed"), bNodeChangeIdChanged);
    Result->SetBoolField(TEXT("graph_change_id_changed"), bGraphChangeIdChanged);
    Result->SetBoolField(TEXT("source_invalidation_verified"), bSourceInvalidationVerified);
    Result->SetStringField(TEXT("node_change_id_after"), NodeChangeIdAfter.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetStringField(TEXT("graph_change_id_after"), GraphChangeIdAfter.ToString(EGuidFormats::DigitsWithHyphens));
    Result->SetNumberField(TEXT("affected_script_count"), AffectedScriptPaths.Num());
    Result->SetArrayField(TEXT("mutation_compile_inputs"), MutationCompileInputs);
    Result->SetObjectField(TEXT("after"), BuildCustomHlslNodeJson(MatchedSource, MatchedNode, MatchedNodeIndex, MaxHlslChars));

    bool bCompileLaunched = false;
    bool bCompileCompleted = false;
    bool bCompileEffectVerified = false;
    int32 CompiledIdChangedCount = 0;
    int32 CompiledDataAppliedCount = 0;
    int32 StableCompileInputCount = 0;
    int32 AffectedCompileErrorCount = 0;
    TArray<TSharedPtr<FJsonValue>> CompileEffects;

    // request_compile=True was rejected before writer; compiled-id application remains explicitly unimplemented.
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("compile_launched"), bCompileLaunched);
    Result->SetBoolField(TEXT("compile_completed"), bCompileCompleted);
    Result->SetBoolField(TEXT("compile_effect_verified"), bCompileEffectVerified);
    Result->SetNumberField(TEXT("compiled_id_changed_count"), CompiledIdChangedCount);
    Result->SetNumberField(TEXT("stable_compile_input_count"), StableCompileInputCount);
    Result->SetNumberField(TEXT("compiled_data_applied_count"), CompiledDataAppliedCount);
    Result->SetNumberField(TEXT("affected_compile_error_count"), AffectedCompileErrorCount);
    Result->SetArrayField(TEXT("compile_effects"), CompileEffects);

    const bool bPreSaveVerificationPass =
        bReadbackVerified &&
        bSourceInvalidationVerified &&
        (!bRequestCompile || bCompileEffectVerified);

    bool bSaved = false;
    if (bSave && bPreSaveVerificationPass)
    {
        bSaved = SaveHlslSystem(W,System);if(!ContinueRead())return Result;
    }

    const bool bPostSaveReadbackVerified = GetNiagaraCustomHlslString(MatchedNode).Equals(ReplacementHlsl,ESearchCase::CaseSensitive);
    if(!ContinueRead())return Result;
    const bool bSaveVerified = !bSave || bSaved;
    const bool bVerificationPass = bPreSaveVerificationPass && bSaveVerified && bPostSaveReadbackVerified;

    Result->SetBoolField(TEXT("pre_save_verification_pass"), bPreSaveVerificationPass);
    Result->SetBoolField(TEXT("save_blocked_by_verification"), bSave && !bPreSaveVerificationPass);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("save_verified"), bSaveVerified);
    Result->SetBoolField(TEXT("post_save_readback_verified"), bPostSaveReadbackVerified);
    Result->SetBoolField(TEXT("package_dirty_after"), System->GetOutermost() ? System->GetOutermost()->IsDirty() : false);
    Result->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    Result->SetBoolField(TEXT("success"), bVerificationPass);
    Result->SetStringField(TEXT("write_scope"), TEXT("custom_hlsl_replace"));

    if (!bVerificationPass)
    {
        FString ErrorMessage;
        if (!bReadbackVerified)
        {
            ErrorMessage = TEXT("Custom HLSL readback did not match the requested replacement");
        }
        else if (!bNodeChangeIdChanged)
        {
            ErrorMessage = TEXT("Custom HLSL changed but the Niagara node ChangeId did not change");
        }
        else if (AffectedScriptPaths.Num() == 0)
        {
            ErrorMessage = TEXT("Custom HLSL changed but no dependent Niagara script Compile ID was invalidated");
        }
        else if (bRequestCompile && !bCompileLaunched)
        {
            ErrorMessage = TEXT("Niagara compile was requested but no compilation was launched");
        }
        else if (bRequestCompile && !bCompileCompleted)
        {
            ErrorMessage = TEXT("Niagara compilation did not complete before the timeout");
        }
        else if (bRequestCompile && !bCompileEffectVerified)
        {
            ErrorMessage = TEXT("Niagara compilation completed without applying the expected post-edit Compile IDs to every affected script");
        }
        else if (bSave && !bSaved)
        {
            ErrorMessage = TEXT("Failed to save Niagara system after verified Custom HLSL replacement");
        }
        else
        {
            ErrorMessage = TEXT("Custom HLSL replacement failed postcondition verification");
        }
        Result->SetStringField(TEXT("error"), ErrorMessage);
    }
    return Result;
#else
    return ErrorResponse(TEXT("replace_niagara_custom_hlsl requires editor-only Niagara data"));
#endif
}

struct FRecipeTransition
{
 FWork& W;UNiagaraGraph* Graph;FGraph Before,Expected;TSet<UEdGraphNode*> Flexible;TSharedPtr<FJsonObject> Row;bool Opened=false;TMap<UEdGraphPin*,UEdGraphPin*> Resolved;
 UEdGraphPin* ResolvedPin(UEdGraphPin* P)const{UEdGraphPin* const* Found=Resolved.Find(P);return Found?*Found:nullptr;}
 FRecipeTransition(FWork& InW,UNiagaraGraph* G):W(InW),Graph(G){}
 bool Begin(const TCHAR* Stage,const FString& Target)
 {
  if(!W.Admit(TEXT("recipe_transition_plan")))return false;
  const FGraph* Existing=W.Graphs.FindByPredicate([&](const FGraph& G){return G.Graph.Get()==Graph;});
  if(!Existing)return W.Fail(TEXT("niagara_graph_recipe_transition_uncaptured"),TEXT("Exact selected graph snapshot missing."));
  Before=*Existing;Expected=Before;Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetStringField(TEXT("target"),Target);Row->SetNumberField(TEXT("before_node_count"),Expected.Nodes.Num());Row->SetBoolField(TEXT("SDK_returned"),false);
  const TArray<TSharedPtr<FJsonValue>>* Prior=nullptr;TArray<TSharedPtr<FJsonValue>> Rows;if(W.R.Data->TryGetArrayField(TEXT("actual_graph_transition_prefix"),Prior)&&Prior)Rows=*Prior;Rows.Add(MakeShared<FJsonValueObject>(Row));W.R.Data->SetArrayField(TEXT("actual_graph_transition_prefix"),Rows);
  W.TransitionGraph=Graph;Opened=true;return W.SDK(Stage,true,true);
 }
 void RemoveLinks(UEdGraphPin* P)
 {
  for(auto& N:Expected.Nodes)for(auto& V:N.Pins){if(V.P==P)V.Links.Reset();else V.Links.Remove(P);}
 }
 void ConnectPlan(UEdGraphPin* A,UEdGraphPin* B,ECanCreateConnectionResponse Response)
 {
  if(Response==CONNECT_RESPONSE_BREAK_OTHERS_A||Response==CONNECT_RESPONSE_BREAK_OTHERS_AB)RemoveLinks(A);
  if(Response==CONNECT_RESPONSE_BREAK_OTHERS_B||Response==CONNECT_RESPONSE_BREAK_OTHERS_AB)RemoveLinks(B);
  for(auto& N:Expected.Nodes)for(auto& P:N.Pins){if(P.P==A)P.Links.AddUnique(B);else if(P.P==B)P.Links.AddUnique(A);}
 }
 void BreakPlan(UEdGraphPin* A,UEdGraphPin* B){for(auto& N:Expected.Nodes)for(auto& P:N.Pins){if(P.P==A)P.Links.Remove(B);else if(P.P==B)P.Links.Remove(A);}}
 void RemovePlan(UEdGraphNode* Node)
 {
  TSet<UEdGraphPin*> Pins;for(const auto& N:Expected.Nodes)if(N.Node.Get()==Node)for(const auto& P:N.Pins)Pins.Add(P.P);
  Expected.Nodes.RemoveAll([&](const FNode& N){return N.Node.Get()==Node;});
  for(auto& N:Expected.Nodes)for(auto& P:N.Pins)P.Links.RemoveAll([&](UEdGraphPin* L){return Pins.Contains(L);});
 }
 bool End(const TCHAR* Stage,UEdGraphNode* Added=nullptr,bool TargetMutation=true)
 {
  if(Row)Row->SetBoolField(TEXT("SDK_returned"),true);
  if(!W.After(Stage)){W.TransitionGraph=nullptr;return false;}
  if(Added){FNode N;N.Node=Added;if(!Live(Added)){W.TransitionGraph=nullptr;return W.Fail(TEXT("niagara_graph_recipe_created_node_unavailable"),TEXT("Returned node is not live."));}N.Id=Added->NodeGuid;N.X=Added->NodePosX;N.Y=Added->NodePosY;Expected.Nodes.Add(MoveTemp(N));}
  auto Fail=[&](const TCHAR* Code,const TCHAR* Message){W.TransitionGraph=nullptr;return W.Fail(Code,Message);};
  if(Graph->Nodes.Num()!=Expected.Nodes.Num())return Fail(TEXT("niagara_graph_recipe_transition_membership_changed"),TEXT("SDK changed graph membership beyond the exact planned node transition."));
  TSet<UEdGraphPin*> LivePins;TSet<FGuid> NodeIds;
  for(int32 I=0;I<Expected.Nodes.Num();++I)
  {
   UEdGraphNode* N=Graph->Nodes[I];const auto& E=Expected.Nodes[I];
   if(N!=E.Node.Get()||!Live(N)||N->GetOuter()!=Graph||N->GetGraph()!=Graph||N->NodeGuid!=E.Id||N->NodePosX!=E.X||N->NodePosY!=E.Y||!E.Id.IsValid()||NodeIds.Contains(E.Id))return Fail(TEXT("niagara_graph_recipe_transition_node_changed"),TEXT("Exact ordered graph node/owner/GUID changed outside the plan."));
   NodeIds.Add(E.Id);
   for(UEdGraphPin* P:N->Pins){if(!P||P->GetOwningNodeUnchecked()!=N||LivePins.Contains(P))return Fail(TEXT("niagara_graph_recipe_transition_pin_malformed"),TEXT("Current pin membership/owner is malformed."));LivePins.Add(P);}
  }
  // Map old raw pins by current membership before any old address is dereferenced.
  TMap<UEdGraphPin*,UEdGraphPin*> Remap;
  for(const auto& E:Expected.Nodes)
  {
   UEdGraphNode* N=E.Node.Get();if(N==Added){for(UEdGraphPin* P:N->Pins)if(!P->LinkedTo.IsEmpty())return Fail(TEXT("niagara_graph_recipe_spawn_autolink_unexpected"),TEXT("Spawn without FromPin unexpectedly connected existing graph."));continue;}
   if(N->Pins.Num()!=E.Pins.Num())return Fail(TEXT("niagara_graph_recipe_transition_pin_count_changed"),TEXT("SDK rebuilt an unexpected pin count."));
   for(int32 I=0;I<E.Pins.Num();++I)
   {
    const FPin& P=E.Pins[I];UEdGraphPin* Actual=nullptr;
    if(!Flexible.Contains(N)){if(N->Pins[I]!=P.P)return Fail(TEXT("niagara_graph_recipe_transition_old_pin_retired"),TEXT("Unrelated existing pin was retired by callback."));Actual=N->Pins[I];}
    else{int32 Matches=0;for(UEdGraphPin* Candidate:N->Pins)if(Candidate->PinId==P.Id&&Candidate->PersistentGuid==P.Persistent&&Candidate->PinName==P.Name&&Candidate->Direction==P.Direction){Actual=Candidate;++Matches;}if(Matches!=1)return Fail(TEXT("niagara_graph_recipe_transition_pin_ambiguous"),TEXT("Created endpoint pin did not preserve an exact unique identity."));}
    if(!LivePins.Contains(Actual))return Fail(TEXT("niagara_graph_recipe_transition_pin_lost"),TEXT("Expected pin is absent from current membership."));
    if(!Flexible.Contains(N)){if(!PinHeaderEqual(P,Actual))return Fail(TEXT("niagara_graph_recipe_transition_header_changed"),TEXT("Unrelated exact pin header/default changed."));}
    else if(Actual->bOrphanedPin!=P.Orphaned||Actual->bNotConnectable!=P.NotConnectable||Actual->DefaultObject!=P.DefaultObject.Get()||!Actual->DefaultValue.Equals(P.Default,ESearchCase::CaseSensitive))return Fail(TEXT("niagara_graph_recipe_transition_created_default_changed"),TEXT("Created endpoint reconstruction changed unrelated default/flags."));
    Remap.Add(P.P,Actual);
   }
  }
  for(const auto& E:Expected.Nodes)if(E.Node.Get()!=Added)for(const auto& P:E.Pins)
  {
   UEdGraphPin* Actual=Remap.FindChecked(P.P);TArray<UEdGraphPin*> Links;
   for(UEdGraphPin* OldLink:P.Links){UEdGraphPin* const* Mapped=Remap.Find(OldLink);UEdGraphPin* NewLink=Mapped?*Mapped:OldLink;if(!LivePins.Contains(NewLink))return Fail(TEXT("niagara_graph_recipe_transition_link_lost"),TEXT("Planned endpoint is absent; stale address is not read."));Links.Add(NewLink);}
   if(Actual->LinkedTo!=Links)return Fail(TEXT("niagara_graph_recipe_transition_links_changed"),TEXT("Ordered reciprocal links differ from the exact connection plan."));
  }
  for(UEdGraphNode* N:Graph->Nodes)for(UEdGraphPin* P:N->Pins)for(UEdGraphPin* L:P->LinkedTo)if(!LivePins.Contains(L)||!L->LinkedTo.Contains(P))return Fail(TEXT("niagara_graph_recipe_transition_link_malformed"),TEXT("Current links are not graph-local and reciprocal."));
  Resolved=Remap;FGraph Captured;Captured.Source=Expected.Source;Captured.Graph=Graph;
  for(UEdGraphNode* N:Graph->Nodes)
  {
   if(!W.Pin(N)){W.TransitionGraph=nullptr;return false;}FNode E;E.Node=N;E.Id=N->NodeGuid;E.X=N->NodePosX;E.Y=N->NodePosY;
   for(UEdGraphPin* P:N->Pins)
   {
    if(!W.Budget(1+P->LinkedTo.Num(),P->DefaultValue.Len()+P->AutogeneratedDefaultValue.Len()+P->DefaultTextValue.ToString().Len())){W.TransitionGraph=nullptr;return false;}
    FPin V;V.P=P;V.Id=P->PinId;V.Persistent=P->PersistentGuid;V.Name=P->PinName;V.Direction=P->Direction;V.Type=P->PinType;V.NotConnectable=P->bNotConnectable;V.DefaultReadOnly=P->bDefaultValueIsReadOnly;V.Orphaned=P->bOrphanedPin;V.DefaultObject=P->DefaultObject;V.Links=P->LinkedTo;V.Default=P->DefaultValue;V.AutoDefault=P->AutogeneratedDefaultValue;V.DefaultText=P->DefaultTextValue.ToString();E.Pins.Add(MoveTemp(V));
    if(P->DefaultObject&&!W.Pin(P->DefaultObject)){W.TransitionGraph=nullptr;return false;}
    if(P->PinType.PinSubCategoryObject.IsValid()&&!W.Pin(P->PinType.PinSubCategoryObject.Get())){W.TransitionGraph=nullptr;return false;}
   }
   if(auto* Call=Cast<UNiagaraNodeFunctionCall>(N))if(Call->FunctionScript&&!W.Pin(Call->FunctionScript)){W.TransitionGraph=nullptr;return false;}
   Captured.Nodes.Add(MoveTemp(E));
  }
  FGraph* Stored=W.Graphs.FindByPredicate([&](const FGraph& G){return G.Graph.Get()==Graph;});if(!Stored)return Fail(TEXT("niagara_graph_recipe_transition_snapshot_missing"),TEXT("Captured graph binding disappeared."));
  *Stored=MoveTemp(Captured);W.TransitionGraph=nullptr;Opened=false;W.TargetChanged|=TargetMutation;Row->SetBoolField(TEXT("exact_transition_verified"),true);return W.Admit(TEXT("recipe_transition_complete"));
 }
 ~FRecipeTransition(){if(Opened)W.TransitionGraph=nullptr;}
};


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

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& A){return StringArrayToJsonValues(A);}
TSharedPtr<FJsonObject> DetailedGraphNodeToJsonObject(
    const UEdGraphNode* Node,
    int32 NodeIndex,
    bool bIncludePins,
    bool bIncludeLinkedPins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!ContinueRead()||!Node)
    {
        return NodeObject;
    }

    NodeObject->SetNumberField(TEXT("node_index"), NodeIndex);
    NodeObject->SetStringField(TEXT("node_name"), Node->GetName());
    NodeObject->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
    NodeObject->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
    FText Title;if(!Current->Step(TEXT("Recipe_NodeTitle"),[&]{Title=Node->GetNodeTitle(ENodeTitleType::ListView);}))return NodeObject;const FString TitleText=Title.ToString();if(!Current->Budget(0,TitleText.Len()))return NodeObject;NodeObject->SetStringField(TEXT("node_title"),TitleText);
    FText Tooltip;if(!Current->Step(TEXT("Recipe_NodeTooltip"),[&]{Tooltip=Node->GetTooltipText();}))return NodeObject;const FString TooltipText=Tooltip.ToString();if(!Current->Budget(0,TooltipText.Len()))return NodeObject;NodeObject->SetStringField(TEXT("tooltip"),TooltipText);
    NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
    NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);
    NodeObject->SetNumberField(TEXT("width"), Node->NodeWidth);
    NodeObject->SetNumberField(TEXT("height"), Node->NodeHeight);
    NodeObject->SetNumberField(TEXT("pin_count"), Node->Pins.Num());

    if (const UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node))
    {
        FString FunctionName;if(!Current->Step(TEXT("Recipe_FunctionName"),[&]{FunctionName=FunctionCall->GetFunctionName();}))return NodeObject;
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
        ENiagaraScriptUsage Usage=ENiagaraScriptUsage::Function;if(!Current->Step(TEXT("Recipe_CalledUsage"),[&]{Usage=FunctionCall->GetCalledUsage();}))return NodeObject;NodeObject->SetStringField(TEXT("called_usage"),NiagaraScriptUsageName(Usage));
        NodeObject->SetBoolField(TEXT("is_scratch_pad"), ScriptOuter && ScriptOuter->IsA<UNiagaraScratchPadContainer>());
        NodeObject->SetStringField(TEXT("signature_name"), FunctionCall->Signature.GetNameString());
        NodeObject->SetArrayField(TEXT("signature_inputs"), NiagaraVariablesToJson(FunctionCall->Signature.Inputs));
        NodeObject->SetArrayField(TEXT("signature_outputs"), NiagaraVariableBasesToJson(FunctionCall->Signature.Outputs));
        NodeObject->SetArrayField(TEXT("control_hints"), StringsToJson(BuildNiagaraControlHints(FunctionName)));
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


struct FRecipeContinuation:public TSharedFromThis<FRecipeContinuation>
{
 FWork& W;FResolvedSystemScratchPadGraph Resolved;UNiagaraNodeCustomHlsl* CustomNode;UNiagaraGraph* TemporaryOwner;
 TArray<TSharedPtr<FJsonValue>> NodeSpecs,LinkSpecs,OutputMappings,PlannedNodeValues,CustomInputValues,CustomOutputValues;
 TArray<FString> SelectedFunctionPaths;TMap<FString,TSharedPtr<FNiagaraAction_NewNode>> ActionById;TMap<FString,UEdGraphNode*> CreatedNodes;
 int32 ScratchPadReferenceCount=0,MappedConsumedOutputCount=0,CreatedLinkCount=0,RewiredConsumerCount=0,NodeIndex=0,DefaultIndex=0,LinkIndex=0,MappingIndex=0,ConsumerIndex=0,RestorePinIndex=0,RestoreLinkIndex=-1;
 TSet<UEdGraphPin*> ConsumedCustomOutputs;TArray<UEdGraphPin*> Consumers;TArray<FString> DefaultKeys;
 FString CustomHlslNodeGuidString,ExistingHlslBodyHash,FailureMessage,FailureCode,OriginalGraphFingerprint,TargetSystemPath,TargetPackagePath,TargetFilename;
 bool bSave=true,bRemoveCustomHlsl=true,bPartialOutputMapping=false,bCustomHlslRemoved=false,bBaselineCompileLaunched=false,bCompileLaunchedBeforeRemoval=false,bCompileLaunchedFinal=false,bRestoreCompileLaunched=false,bSaved=false,MutationStarted=false,SaveStateUnknown=false,Restoring=false,RestoreCompleted=false;
 bool bPreRemovalPendingCompileIdsChanged=false,bFinalPendingCompileIdsChanged=false,bPreRemovalCallerCompileIdsChanged=false,bFinalCallerCompileIdsChanged=false;
 double TimeoutSeconds=30,PollIntervalSeconds=.05;FConversionCompileWait Wait;
 TSharedPtr<FJsonObject> Result,DependencyAudit,FinalDependencyAudit,BaselineCompileStatus,PreRemovalCompileStatus,FinalCompileStatus,BaselineScratchPadValidation,PreRemovalScratchPadValidation,FinalScratchPadValidation,PreRemovalCallerCompileIdValidation,FinalCallerCompileIdValidation,PreRemovalPendingCompileIdValidation,FinalPendingCompileIdValidation,BaselineCompileWait,PreRemovalWait,FinalWait,Rollback;
 FNiagaraRecipeScratchPadReferenceScan FinalReferenceScan;
 TArray<FNiagaraScriptCompileSnapshot> BaselineCallerSnapshots,PreRemovalPendingCallerSnapshots,FinalPendingCallerSnapshots;
 FGraph OriginalGraph;TArray<UEdGraphNode*> OriginalNodeOrder;TSet<UEdGraphNode*> OriginalNodes;TArray<FPin> OriginalCustomPins;
 enum class EPhase:uint8{BaselineRequest,BaselineWait,BaselineVerify,ModifyRoots,CreateNode,Default,Link,Rewire,PreRequest,PreWait,PreVerify,RemoveHlsl,FinalRequest,FinalWait,FinalVerify,Audit,Save,Success,RestoreRemove,RestoreAdd,RestoreLink,RestoreOrder,RestoreNotify,RestoreRequest,RestoreWait,RestoreVerify,Failed};
 EPhase Phase=EPhase::BaselineRequest;
 explicit FRecipeContinuation(FWork& InW):W(InW),CustomNode(nullptr),TemporaryOwner(nullptr),Result(MakeShared<FJsonObject>()){}
 void Init()
 {
  TargetSystemPath=Resolved.System->GetPathName();TargetPackagePath=Resolved.System->GetOutermost()->GetName();FPackageName::TryConvertLongPackageNameToFilename(TargetPackagePath,TargetFilename,FPackageName::GetAssetPackageExtension());
  Result->SetBoolField(TEXT("success"),false);Result->SetBoolField(TEXT("dry_run"),false);Result->SetBoolField(TEXT("mutation_performed"),false);Result->SetBoolField(TEXT("mutation_applied_in_memory"),false);Result->SetBoolField(TEXT("saved"),false);Result->SetStringField(TEXT("system_path"),TargetSystemPath);Result->SetStringField(TEXT("scratch_pad_script_path"),Resolved.Script->GetPathName());Result->SetStringField(TEXT("expected_hlsl_body_hash"),ExistingHlslBodyHash);Result->SetStringField(TEXT("custom_hlsl_node_guid"),CustomHlslNodeGuidString);W.R.Data->SetObjectField(TEXT("legacy_result"),Result);
 }
 bool Error(const FString& Message,const FString& Code,bool DiskUnknown=false)
 {
  if(Restoring){Result->SetStringField(TEXT("restore_error"),Message);Rollback->SetBoolField(TEXT("structural_rollback_verified"),false);Phase=EPhase::Failed;return false;}
  if(FailureCode.IsEmpty()){FailureCode=Code;FailureMessage=Message;Result->SetStringField(TEXT("failure_phase"),W.R.Stage);}SaveStateUnknown|=DiskUnknown;
  Result->SetBoolField(TEXT("success"),false);Result->SetStringField(TEXT("error"),FailureMessage);Result->SetStringField(TEXT("error_code"),FailureCode);Result->SetBoolField(TEXT("mutation_performed"),MutationStarted||W.TargetChanged);Result->SetBoolField(TEXT("mutation_applied_in_memory"),MutationStarted||W.TargetChanged);Result->SetBoolField(TEXT("saved"),W.R.bSaved);
  if(MutationStarted||W.TargetChanged)SetNiagaraGraphRecipeReloadRequiredRecoveryContract(Result);
  if(!MutationStarted){Phase=EPhase::Failed;return true;}
  if(!W.R.Error.IsEmpty()||W.CancelRequested||DeferredShutdownRequested.load())
  {
   Result->SetBoolField(TEXT("rollback_attempted"),false);Result->SetBoolField(TEXT("rollback_skipped_unsafe_or_cancelled"),true);Result->SetBoolField(TEXT("rollback_verified"),false);Phase=EPhase::Failed;return true;
  }
  Restoring=true;Rollback=MakeShared<FJsonObject>();Rollback->SetBoolField(TEXT("success"),false);Rollback->SetBoolField(TEXT("package_dirty_state_restored"),false);Result->SetBoolField(TEXT("rollback_attempted"),true);Result->SetBoolField(TEXT("rollback_verified"),false);Result->SetObjectField(TEXT("rollback"),Rollback);SetNiagaraGraphRecipeReloadRequiredRecoveryContract(Rollback);
  Phase=EPhase::RestoreRemove;return false;
 }
 void Interrupted()
 {
  if(RestoreCompleted)return;
  if(W.R.bSaveAttempted)SavedFailureContract();
  if(MutationStarted||W.TargetChanged)
  {
   Result->SetBoolField(TEXT("success"),false);Result->SetBoolField(TEXT("mutation_performed"),true);Result->SetBoolField(TEXT("mutation_applied_in_memory"),true);Result->SetBoolField(TEXT("saved"),W.R.bSaved);Result->SetBoolField(TEXT("rollback_attempted"),Restoring);Result->SetBoolField(TEXT("rollback_verified"),false);Result->SetBoolField(TEXT("rollback_skipped_unsafe_or_cancelled"),true);SetNiagaraGraphRecipeReloadRequiredRecoveryContract(Result);
  }
 }
 bool GuardFailure()
 {
  Interrupted();if(!W.R.Error.IsEmpty())Result->SetStringField(TEXT("error"),W.R.Error);Result->SetStringField(TEXT("error_code"),W.R.ErrorCode);Phase=EPhase::Failed;return true;
 }
 bool Request(const TCHAR* Stage,bool& Accepted)
 {
  if(!RequestConversionCompile(W,Resolved.System,Stage,Accepted))return false;Wait.Begin(Resolved.System,TimeoutSeconds,PollIntervalSeconds);return true;
 }
 FNiagaraRecipeScratchPadReferenceScan Scan(){return ScanNiagaraRecipeScratchPadSystemReferences(Resolved.System,Resolved.Script);}
 bool Notify(const TCHAR* Stage){return W.Step(Stage,[&]{Resolved.Graph->NotifyGraphChanged();},true,true)&&W.Step(TEXT("Recipe_MarkPackageDirty"),[&]{Resolved.System->MarkPackageDirty();},true,true);}
 bool Connect(UEdGraphPin* A,UEdGraphPin* B,const TCHAR* Stage)
 {
  const UEdGraphSchema_Niagara* Schema=GetDefault<UEdGraphSchema_Niagara>();FPinConnectionResponse Response(CONNECT_RESPONSE_DISALLOW,FString());
  if(!W.Step(TEXT("Recipe_CanCreateConnection"),[&]{Response=Schema->CanCreateConnection(A,B);}))return false;
  // Original direct-link verification rejects auto-conversion too; stop this failed branch without an untracked SDK-created node.
  if(Response.Response==CONNECT_RESPONSE_DISALLOW){Result->SetBoolField(TEXT("connection_disallowed_before_SDK"),true);return false;}
  if(Response.Response==CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE){Result->SetBoolField(TEXT("auto_conversion_direct_link_failure_detected_before_SDK"),true);return false;}
  FRecipeTransition T(W,Resolved.Graph);if(!T.Begin(Stage,TEXT("ordered_pin_connection")))return false;T.ConnectPlan(A,B,Response.Response);
  for(const auto& C:CreatedNodes){if(C.Value==A->GetOwningNodeUnchecked()||C.Value==B->GetOwningNodeUnchecked())T.Flexible.Add(C.Value);}
  const bool Returned=Schema->TryCreateConnection(A,B);T.Row->SetBoolField(TEXT("SDK_success"),Returned);
  if(!Returned){T.Expected=T.Before;T.Flexible.Reset();T.Row->SetBoolField(TEXT("false_return_requires_exact_no_change"),true);}
  if(!T.End(Stage,nullptr,Returned))return false;
  UEdGraphPin* NA=T.ResolvedPin(A);UEdGraphPin* NB=T.ResolvedPin(B);return Returned&&NA&&NB&&NA->LinkedTo.Contains(NB)&&NB->LinkedTo.Contains(NA);
 }
 bool Break(UEdGraphPin* A,UEdGraphPin* B)
 {
  FRecipeTransition T(W,Resolved.Graph);if(!T.Begin(TEXT("Recipe_BreakLink"),TEXT("mapped_custom_output")))return false;T.BreakPlan(A,B);A->BreakLinkTo(B);return T.End(TEXT("Recipe_BreakLink"));
 }
 bool Remove(UEdGraphNode* N,const TCHAR* Stage)
 {
  FRecipeTransition T(W,Resolved.Graph);if(!T.Begin(Stage,N->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens)))return false;T.RemovePlan(N);
  const bool Returned=Resolved.Graph->RemoveNode(N,true,false);T.Row->SetBoolField(TEXT("SDK_success"),Returned);return T.End(Stage);
 }
 bool CreateOne()
 {
  if(NodeIndex>=NodeSpecs.Num()){Phase=EPhase::Link;return false;}
  const auto Spec=NodeSpecs[NodeIndex]->AsObject();const FString NodeId=Spec->GetStringField(TEXT("node_id"));const FString ActionId=Spec->GetStringField(TEXT("action_id"));const auto* Found=ActionById.Find(ActionId);
  if(!Found||!Found->IsValid()||!(*Found)->WeakNodeTemplate.IsValid())return Error(TEXT("Validated Niagara action disappeared before mutation"),TEXT("niagara_graph_recipe_action_unavailable"));
  const auto Action=*Found;UEdGraphNode* Template=Action->WeakNodeTemplate.Get();
  if(!BuildNiagaraGraphActionId(*Action).Equals(ActionId,ESearchCase::CaseSensitive))return Error(TEXT("Exact action template/canonical changed"),TEXT("niagara_graph_recipe_action_changed"));
  UEdGraphNode* Copy=nullptr;if(!W.Step(TEXT("Recipe_DuplicateTemplate"),[&]{Copy=Cast<UEdGraphNode>(StaticDuplicateObject(Template,TemporaryOwner));},true,false))return GuardFailure();
  if(!Copy)return Error(TEXT("Failed to duplicate Niagara action template"),TEXT("niagara_graph_recipe_template_duplicate_failed"));
  W.TransitionHolds.Emplace(Copy);if(!BuildNiagaraGraphActionId(*Action).Equals(ActionId,ESearchCase::CaseSensitive)||Copy->GetClass()!=Template->GetClass()||!ReadNiagaraActionNameProperty(Copy,TEXT("OpName")).Equals(ReadNiagaraActionNameProperty(Template,TEXT("OpName")),ESearchCase::CaseSensitive)||!ReadNiagaraActionNameProperty(Copy,TEXT("FunctionScriptAssetObjectPath")).Equals(ReadNiagaraActionNameProperty(Template,TEXT("FunctionScriptAssetObjectPath")),ESearchCase::CaseSensitive)||!ReadNiagaraActionStringProperty(Copy,TEXT("AutowireSwizzle")).Equals(ReadNiagaraActionStringProperty(Template,TEXT("AutowireSwizzle")),ESearchCase::CaseSensitive))return Error(TEXT("Duplicated action template no longer matches the exact planned canonical"),TEXT("niagara_graph_recipe_action_changed"));bool CanAdd=false;FString CanAddError;auto* NiagaraCopy=Cast<UNiagaraNode>(Copy);
  if(!NiagaraCopy||!W.Step(TEXT("Recipe_CanAddToGraph"),[&]{CanAdd=NiagaraCopy->CanAddToGraph(Resolved.Graph,CanAddError);})||!CanAdd){if(!W.R.Error.IsEmpty())return GuardFailure();return Error(TEXT("Niagara action cannot add to selected graph"),TEXT("niagara_graph_recipe_action_rejected"));}
  FRecipeTransition T(W,Resolved.Graph);if(!T.Begin(TEXT("Recipe_SpawnNode"),NodeId))return GuardFailure();
  UEdGraphNode* Created=FNiagaraSchemaAction_NewNode::SpawnNodeFromTemplate(Resolved.Graph,Copy,FVector2D(Spec->GetNumberField(TEXT("x")),Spec->GetNumberField(TEXT("y"))),false);T.Row->SetBoolField(TEXT("SDK_nonnull"),Created!=nullptr);
  if(Created&&Created!=Copy){T.Row->SetBoolField(TEXT("SDK_returned"),true);W.After(TEXT("Recipe_SpawnNode"));W.Fail(TEXT("niagara_graph_recipe_spawn_identity_changed"),TEXT("SDK returned a different object than exact template copy; no adoption/restore of foreign callback node."));return GuardFailure();}
  if(!T.End(TEXT("Recipe_SpawnNode"),Created))return GuardFailure();
  if(!Created)return Error(TEXT("Failed to create recipe node"),TEXT("niagara_graph_recipe_node_create_failed"));
  if(Created->GetClass()!=Template->GetClass()||Created->NodePosX!=static_cast<int32>(Spec->GetNumberField(TEXT("x")))||Created->NodePosY!=static_cast<int32>(Spec->GetNumberField(TEXT("y"))))return Error(TEXT("Created node class/position differs from exact planned template"),TEXT("niagara_graph_recipe_created_readback_failed"));
  CreatedNodes.Add(NodeId,Created);T.Row->SetStringField(TEXT("node_guid"),Created->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));W.R.Data->SetNumberField(TEXT("actual_created_node_count"),CreatedNodes.Num());
  if(!W.Step(TEXT("Recipe_CreatedNode_Modify"),[&]{Created->Modify();},true,true))return GuardFailure();
  DefaultKeys.Reset();DefaultIndex=0;const TSharedPtr<FJsonObject>* Defaults=nullptr;if(Spec->TryGetObjectField(TEXT("defaults"),Defaults)&&Defaults&&Defaults->IsValid())for(const auto& Entry:(*Defaults)->Values)DefaultKeys.Add(FString(Entry.Key.ToView()));Phase=EPhase::Default;return false;
 }
 bool SetOneDefault()
 {
  if(DefaultIndex>=DefaultKeys.Num()){++NodeIndex;Phase=EPhase::CreateNode;return false;}
  const auto Spec=NodeSpecs[NodeIndex]->AsObject();UEdGraphNode* Node=CreatedNodes.FindChecked(Spec->GetStringField(TEXT("node_id")));const FString Key=DefaultKeys[DefaultIndex++];const auto Defaults=Spec->GetObjectField(TEXT("defaults"));FString Value;
  UEdGraphPin* P=FindNiagaraRecipePin(Node,EGPD_Input,Key,0);if(!P||!JsonValueToNiagaraPinDefault(Defaults->TryGetField(Key),Value))return Error(TEXT("Failed to resolve recipe default"),TEXT("niagara_graph_recipe_default_invalid"));
  FRecipeTransition T(W,Resolved.Graph);if(!T.Begin(TEXT("Recipe_SetDefault"),Key))return GuardFailure();for(auto& N:T.Expected.Nodes)for(auto& Pin:N.Pins)if(Pin.P==P)Pin.Default=Value;
  GetDefault<UEdGraphSchema_Niagara>()->TrySetDefaultValue(*P,Value,true);
  if(!T.End(TEXT("Recipe_SetDefault")))return GuardFailure();
  UEdGraphPin* Actual=T.ResolvedPin(P);if(!Actual||!Actual->DefaultValue.Equals(Value,ESearchCase::CaseSensitive))return Error(TEXT("Recipe default exact readback mismatch"),TEXT("niagara_graph_recipe_default_readback_failed"));return false;
 }
 bool LinkOne()
 {
  if(LinkIndex>=LinkSpecs.Num()){Phase=EPhase::Rewire;return false;}
  const auto Value=LinkSpecs[LinkIndex++];if(!Value.IsValid()||Value->Type!=EJson::Object)return Error(TEXT("Every recipe link requires source and target endpoint objects"),TEXT("niagara_graph_recipe_invalid_link_spec"));const auto Spec=Value->AsObject();const TSharedPtr<FJsonObject>* SourceEndpoint=nullptr;const TSharedPtr<FJsonObject>* TargetEndpoint=nullptr;if(!Spec->TryGetObjectField(TEXT("source"),SourceEndpoint)||!SourceEndpoint||!SourceEndpoint->IsValid()||!Spec->TryGetObjectField(TEXT("target"),TargetEndpoint)||!TargetEndpoint||!TargetEndpoint->IsValid())return Error(TEXT("Every recipe link requires source and target endpoint objects"),TEXT("niagara_graph_recipe_invalid_link_spec"));UEdGraphPin* A=nullptr;UEdGraphPin* B=nullptr;FString E;
  if(!ReadNiagaraRecipeEndpoint(*SourceEndpoint,CreatedNodes,CustomNode,EGPD_Output,A,E,true)||!ReadNiagaraRecipeEndpoint(*TargetEndpoint,CreatedNodes,CustomNode,EGPD_Input,B,E,true))return Error(E,TEXT("niagara_graph_recipe_endpoint_resolve_failed"));
  if(!Connect(A,B,TEXT("Recipe_CreateConnection"))){if(!W.R.Error.IsEmpty())return GuardFailure();return Error(TEXT("Niagara schema rejected recipe direct link"),TEXT("niagara_graph_recipe_link_rejected"));}
  ++CreatedLinkCount;W.R.Data->SetNumberField(TEXT("actual_created_link_count"),CreatedLinkCount);return false;
 }
 bool RewireOne()
 {
  if(MappingIndex>=OutputMappings.Num()){Phase=EPhase::PreRequest;return false;}
  const auto Mapping=OutputMappings[MappingIndex]->AsObject();const FString Name=Mapping->GetStringField(TEXT("custom_output_pin"));UEdGraphPin* CustomOutput=FindNiagaraRecipePin(CustomNode,EGPD_Output,Name,0);
  if(!CustomOutput)return Error(TEXT("Custom output disappeared"),TEXT("niagara_graph_recipe_output_mapping_ambiguous"));
  if(Consumers.IsEmpty()){Consumers=CustomOutput->LinkedTo;ConsumerIndex=0;if(Consumers.IsEmpty())return Error(TEXT("Custom output is unconsumed"),TEXT("niagara_graph_recipe_output_unconsumed"));}
  if(ConsumerIndex>=Consumers.Num()){Consumers.Reset();++MappingIndex;return false;}
  UEdGraphPin* Consumer=Consumers[ConsumerIndex++];UEdGraphPin* Source=nullptr;FString E;
  if(!ReadNiagaraRecipeEndpoint(Mapping->GetObjectField(TEXT("source")),CreatedNodes,CustomNode,EGPD_Output,Source,E,true))return Error(E,TEXT("niagara_graph_recipe_output_source_failed"));
  if(!Break(CustomOutput,Consumer))return GuardFailure();
  if(!Connect(Source,Consumer,TEXT("Recipe_RewireConsumer"))){if(!W.R.Error.IsEmpty())return GuardFailure();return Error(TEXT("Failed to reconnect Custom HLSL consumer"),TEXT("niagara_graph_recipe_output_rewire_failed"));}
  ++RewiredConsumerCount;W.R.Data->SetNumberField(TEXT("actual_rewired_consumer_count"),RewiredConsumerCount);return false;
 }
 bool Pending(bool Final)
 {
  const auto References=Scan();if(!ContinueRead())return GuardFailure();
  auto Snapshots=CaptureNiagaraRecipeCallerSnapshots(References);if(!ContinueRead())return GuardFailure();
  bool Changed=false;auto Validation=BuildNiagaraRecipePreCompileMutationValidation(BaselineCallerSnapshots,Snapshots,Final?TEXT("final_pending_compile"):TEXT("pre_removal_pending_compile"),Changed);
  if(Final){FinalPendingCallerSnapshots=MoveTemp(Snapshots);FinalPendingCompileIdValidation=Validation;bFinalPendingCompileIdsChanged=Changed;Result->SetObjectField(TEXT("final_pending_compile_id_validation"),Validation);}
  else{PreRemovalPendingCallerSnapshots=MoveTemp(Snapshots);PreRemovalPendingCompileIdValidation=Validation;bPreRemovalPendingCompileIdsChanged=Changed;Result->SetObjectField(TEXT("pre_removal_pending_compile_id_validation"),Validation);}
  if(!References.bComplete||!Changed)return Error(TEXT("Complete exact caller context did not produce new pending computed IDs"),Final?TEXT("niagara_graph_recipe_final_pending_compile_id_failed"):TEXT("niagara_graph_recipe_pre_removal_pending_compile_id_failed"));
  bool& Launched=Final?bCompileLaunchedFinal:bCompileLaunchedBeforeRemoval;if(!Request(Final?TEXT("Recipe_RequestCompile_Final"):TEXT("Recipe_RequestCompile_PreRemoval"),Launched))return GuardFailure();Phase=Final?EPhase::FinalWait:EPhase::PreWait;return false;
 }
 bool VerifyAfterCompile(bool Final)
 {
  auto Status=InspectRecipeCompileStatus(W,Resolved.System);if(!ContinueRead())return GuardFailure();FString E;
  const bool Launched=Final?bCompileLaunchedFinal:bCompileLaunchedBeforeRemoval;
  if(!Launched||!Wait.Clean()||!NiagaraCompileStatusDidNotRegress(BaselineCompileStatus,Status,E))return Error(E.IsEmpty()?TEXT("System compile did not launch or finish cleanly"):E,Final?TEXT("niagara_graph_recipe_final_compile_failed"):TEXT("niagara_graph_recipe_pre_removal_compile_failed"));
  auto References=Scan();if(!ContinueRead())return GuardFailure();auto Context=BuildNiagaraRecipeScratchPadContextValidation(Resolved.Script,Final?TEXT("final"):TEXT("pre_removal"),References,ScratchPadReferenceCount,true);if(!ContinueRead())return GuardFailure();auto CurrentSnapshots=CaptureNiagaraRecipeCallerSnapshots(References);if(!ContinueRead())return GuardFailure();bool Applied=false;
  auto Validation=BuildNiagaraRecipeCallerCompileIdValidation(BaselineCallerSnapshots,CurrentSnapshots,Final?&FinalPendingCallerSnapshots:&PreRemovalPendingCallerSnapshots,Final?TEXT("final"):TEXT("pre_removal"),true,Applied);
  if(Final){FinalCompileStatus=Status;FinalScratchPadValidation=Context;FinalCallerCompileIdValidation=Validation;bFinalCallerCompileIdsChanged=Applied;FinalReferenceScan=References;}
  else{PreRemovalCompileStatus=Status;PreRemovalScratchPadValidation=Context;PreRemovalCallerCompileIdValidation=Validation;bPreRemovalCallerCompileIdsChanged=Applied;}
  if(!Context->GetBoolField(TEXT("clean"))||!Applied)return Error(TEXT("Referenced caller context changed or failed pending ID application"),Final?TEXT("niagara_graph_recipe_final_context_compile_failed"):TEXT("niagara_graph_recipe_pre_removal_context_compile_failed"));
  Phase=Final?EPhase::Audit:EPhase::RemoveHlsl;return false;
 }
 void SavedFailureContract()
 {
  Result->SetBoolField(TEXT("save_state_unknown"),true);Result->SetBoolField(TEXT("save_attempted"),true);Result->SetBoolField(TEXT("cleanup_required_unknown"),true);Result->SetBoolField(TEXT("offline_exact_cleanup_required"),true);Result->SetBoolField(TEXT("recreate_required"),true);Result->SetBoolField(TEXT("exact_cleanup_only"),true);Result->SetBoolField(TEXT("rollback_does_not_resolve_save_state"),true);Result->SetBoolField(TEXT("persisted_state_recovery_proven"),false);Result->SetStringField(TEXT("rollback_scope"),TEXT("in_memory_only"));Result->SetStringField(TEXT("target_system_path"),TargetSystemPath);Result->SetStringField(TEXT("target_package_path"),TargetPackagePath);Result->SetStringField(TEXT("target_package_filename"),TargetFilename);Result->SetStringField(TEXT("recovery_target_package"),TargetPackagePath);Result->SetArrayField(TEXT("reload_required_packages"),StringArrayToJsonValues(TArray<FString>{TargetPackagePath}));Result->SetArrayField(TEXT("cleanup_candidate_paths"),StringArrayToJsonValues(TArray<FString>{TargetPackagePath}));Result->SetStringField(TEXT("save_recovery_policy"),TEXT("reload_then_offline_exact_cleanup_and_recreate_required"));
 }
 bool Tick()
 {
  if(!W.Admit(TEXT("recipe_continuation_phase")))return GuardFailure();
  switch(Phase)
  {
   case EPhase::BaselineRequest:
    W.Persistent=true;W.SourceCaptured=false;if(!Request(TEXT("Recipe_RequestCompile_Baseline"),bBaselineCompileLaunched))return GuardFailure();Phase=EPhase::BaselineWait;return false;
   case EPhase::BaselineWait:
    if(!Wait.Tick(W))return false;if(!W.R.Error.IsEmpty())return GuardFailure();BaselineCompileWait=Wait.Result;Phase=EPhase::BaselineVerify;return false;
   case EPhase::BaselineVerify:
   {
    BaselineCompileStatus=InspectRecipeCompileStatus(W,Resolved.System);if(!ContinueRead())return GuardFailure();FString E;
    if(!Wait.Clean()||!NiagaraCompileStatusIsClean(BaselineCompileStatus,E))return Error(E.IsEmpty()?TEXT("Baseline did not finish cleanly"):E,TEXT("niagara_graph_recipe_baseline_not_clean"));
    auto References=Scan();if(!ContinueRead())return GuardFailure();BaselineScratchPadValidation=BuildNiagaraRecipeScratchPadContextValidation(Resolved.Script,TEXT("baseline"),References,ScratchPadReferenceCount,true);if(!ContinueRead())return GuardFailure();
    if(!BaselineScratchPadValidation->GetBoolField(TEXT("clean")))return Error(TEXT("Baseline caller context not clean/synchronized"),TEXT("niagara_graph_recipe_baseline_context_not_clean"));
    BaselineCallerSnapshots=CaptureNiagaraRecipeCallerSnapshots(References);if(!ContinueRead())return GuardFailure();
    const auto* Captured=W.Graphs.FindByPredicate([&](const FGraph& G){return G.Graph.Get()==Resolved.Graph;});if(!Captured)return Error(TEXT("Exact graph capture missing"),TEXT("niagara_graph_recipe_graph_missing"));
    OriginalGraph=*Captured;for(const auto& N:OriginalGraph.Nodes){OriginalNodeOrder.Add(N.Node.Get());OriginalNodes.Add(N.Node.Get());if(N.Node.Get()==CustomNode)OriginalCustomPins=N.Pins;}
    for(const auto& N:OriginalGraph.Nodes)if(N.Node.Get()==CustomNode)for(const auto& P:N.Pins){if(P.Orphaned)return Error(TEXT("Orphaned HLSL pin is not safe to mutate"),TEXT("niagara_graph_recipe_orphaned_pin"));for(UEdGraphPin* L:P.Links)for(const auto& Other:OriginalGraph.Nodes)for(const auto& Q:Other.Pins)if(Q.P==L&&Q.Orphaned)return Error(TEXT("Orphaned HLSL consumer is not safe to mutate"),TEXT("niagara_graph_recipe_orphaned_consumer_pin"));}
    OriginalGraphFingerprint=BuildNiagaraRecipeGraphFingerprint(Resolved.Graph);if(!ContinueRead())return GuardFailure();Phase=EPhase::ModifyRoots;return false;
   }
   case EPhase::ModifyRoots:
   {
    W.Persistent=true;W.SourceCaptured=false;MutationStarted=true;
    for(UObject* O:TArray<UObject*>{Resolved.System,Resolved.Script,Resolved.Source,Resolved.Graph,CustomNode})if(!W.Step(TEXT("Recipe_Modify"),[&]{O->Modify();},true,true))return GuardFailure();
    Phase=EPhase::CreateNode;return false;
   }
   case EPhase::CreateNode:return CreateOne();
   case EPhase::Default:return SetOneDefault();
   case EPhase::Link:return LinkOne();
   case EPhase::Rewire:return RewireOne();
   case EPhase::PreRequest:if(!Notify(TEXT("Recipe_Notify_PreRemoval")))return GuardFailure();return Pending(false);
   case EPhase::PreWait:if(!Wait.Tick(W))return false;if(!W.R.Error.IsEmpty())return GuardFailure();PreRemovalWait=Wait.Result;Phase=EPhase::PreVerify;return false;
   case EPhase::PreVerify:return VerifyAfterCompile(false);
   case EPhase::RemoveHlsl:
    if(bRemoveCustomHlsl){if(!Remove(CustomNode,TEXT("Recipe_RemoveCustomHlsl")))return GuardFailure();bCustomHlslRemoved=true;if(!Notify(TEXT("Recipe_Notify_Final")))return GuardFailure();}
    Phase=EPhase::FinalRequest;return false;
   case EPhase::FinalRequest:return Pending(true);
   case EPhase::FinalWait:if(!Wait.Tick(W))return false;if(!W.R.Error.IsEmpty())return GuardFailure();FinalWait=Wait.Result;Phase=EPhase::FinalVerify;return false;
   case EPhase::FinalVerify:return VerifyAfterCompile(true);
   case EPhase::Audit:
    FinalDependencyAudit=BuildNiagaraRecipePreSaveDependencyAudit(Resolved.System,Resolved.Graph,SelectedFunctionPaths,NodeSpecs);if(!ContinueRead())return GuardFailure();if(!FinalDependencyAudit||!FinalDependencyAudit->GetBoolField(TEXT("success")))return Error(TEXT("Final dependency audit incomplete or forbidden"),TEXT("niagara_graph_recipe_final_dependency_audit_failed"));Phase=EPhase::Save;return false;
   case EPhase::Save:
    if(bSave){bSaved=SaveHlslSystem(W,Resolved.System);if(!bSaved){SaveStateUnknown=true;SavedFailureContract();if(W.R.ErrorCode==TEXT("niagara_hlsl_conversion_save_failed")){W.R.Error.Reset();W.R.ErrorCode.Reset();W.FailedStage.Reset();if(!W.Admit(TEXT("recipe_save_failure_restore_admission")))return GuardFailure();}else if(!W.R.Error.IsEmpty())return GuardFailure();return Error(TEXT("Save failed; persisted state unknown"),TEXT("niagara_graph_recipe_save_failed"),true);}}
    Phase=EPhase::Success;return false;
   case EPhase::Success:return Success();
   case EPhase::RestoreRemove:return RestoreRemove();
   case EPhase::RestoreAdd:return RestoreAdd();
   case EPhase::RestoreLink:return RestoreLink();
   case EPhase::RestoreOrder:return RestoreOrder();
   case EPhase::RestoreNotify:if(!Notify(TEXT("Recipe_Restore_Notify")))return GuardFailure();Phase=EPhase::RestoreRequest;return false;
   case EPhase::RestoreRequest:if(!Request(TEXT("Recipe_RequestCompile_Restore"),bRestoreCompileLaunched))return GuardFailure();Phase=EPhase::RestoreWait;return false;
   case EPhase::RestoreWait:if(!Wait.Tick(W))return false;if(!W.R.Error.IsEmpty())return GuardFailure();Phase=EPhase::RestoreVerify;return false;
   case EPhase::RestoreVerify:return RestoreVerify();
   case EPhase::Failed:if(!FailureCode.IsEmpty()&&W.R.Error.IsEmpty())W.Fail(*FailureCode,FailureMessage);return true;
  }
  return true;
 }
 bool Success();
 bool RestoreRemove();
 bool RestoreAdd();
 bool RestoreLink();
 bool RestoreOrder();
 bool RestoreVerify();
};


bool FRecipeContinuation::RestoreRemove()
{
 for(int32 I=Resolved.Graph->Nodes.Num()-1;I>=0;--I){UEdGraphNode* N=Resolved.Graph->Nodes[I];if(!OriginalNodes.Contains(N)){if(!Remove(N,TEXT("Recipe_Restore_RemoveCreated")))return GuardFailure();return false;}}
 Phase=EPhase::RestoreAdd;return false;
}
bool FRecipeContinuation::RestoreAdd()
{
 if(!Resolved.Graph->Nodes.Contains(CustomNode))
 {
  FRecipeTransition T(W,Resolved.Graph);if(!T.Begin(TEXT("Recipe_Restore_AddCustomHlsl"),CustomHlslNodeGuidString))return GuardFailure();Resolved.Graph->AddNode(CustomNode,false,false);if(!T.End(TEXT("Recipe_Restore_AddCustomHlsl"),CustomNode))return GuardFailure();
 }
 RestorePinIndex=0;RestoreLinkIndex=-1;Phase=EPhase::RestoreLink;return false;
}
bool FRecipeContinuation::RestoreLink()
{
 if(RestorePinIndex>=OriginalCustomPins.Num()){Phase=EPhase::RestoreOrder;return false;}
 const FPin& Original=OriginalCustomPins[RestorePinIndex];UEdGraphPin* P=Original.P;
 // Admission verified all physical arrays. A retired address is never dereferenced before exact membership.
 if(!CustomNode->Pins.Contains(P)||!PinHeaderEqual(Original,P)){return Error(TEXT("Original HLSL pin no longer has its exact identity/header"),TEXT("niagara_graph_recipe_restore_pin_changed"));}
 if(IsNiagaraDynamicAddPin(P)){++RestorePinIndex;RestoreLinkIndex=-1;return false;}
 if(RestoreLinkIndex<0)
 {
  FRecipeTransition T(W,Resolved.Graph);if(!T.Begin(TEXT("Recipe_Restore_BreakCurrent"),P->PinId.ToString(EGuidFormats::DigitsWithHyphens)))return GuardFailure();T.RemoveLinks(P);P->BreakAllPinLinks();if(!T.End(TEXT("Recipe_Restore_BreakCurrent")))return GuardFailure();RestoreLinkIndex=0;return false;
 }
 if(RestoreLinkIndex>=Original.Links.Num()){++RestorePinIndex;RestoreLinkIndex=-1;return false;}
 UEdGraphPin* L=Original.Links[RestoreLinkIndex++];
 bool Found=false;for(const auto& G:W.Graphs)if(G.Graph.Get()==Resolved.Graph)for(const auto& N:G.Nodes)for(const auto& Pin:N.Pins)if(Pin.P==L)Found=true;
 if(!Found)return Error(TEXT("Original restore endpoint is no longer in exact graph membership"),TEXT("niagara_graph_recipe_restore_endpoint_missing"));
 if(!Connect(P,L,TEXT("Recipe_Restore_Connect"))){if(!W.R.Error.IsEmpty())return GuardFailure();Rollback->SetBoolField(TEXT("links_restored_through_schema"),false);Phase=EPhase::RestoreOrder;}
 return false;
}
bool FRecipeContinuation::RestoreOrder()
{
 const FGraph* CurrentGraph=W.Graphs.FindByPredicate([&](const FGraph& G){return G.Graph.Get()==Resolved.Graph;});
 if(!CurrentGraph||CurrentGraph->Nodes.Num()!=OriginalGraph.Nodes.Num()){Rollback->SetBoolField(TEXT("original_node_membership_restored"),false);Phase=EPhase::RestoreNotify;return false;}
 FRecipeTransition T(W,Resolved.Graph);if(!T.Begin(TEXT("Recipe_Restore_ExactOrder"),TEXT("original_node_and_link_order")))return GuardFailure();
 TArray<FNode> Ordered;
 for(const auto& N:OriginalGraph.Nodes)
 {
  const FNode* Actual=T.Expected.Nodes.FindByPredicate([&](const FNode& E){return E.Node.Get()==N.Node.Get();});if(!Actual){Phase=EPhase::RestoreNotify;return false;}Ordered.Add(*Actual);
 }
 // Reorder only after schema restored the same reciprocal endpoint sets; no unobserved link is erased.
 for(auto& N:Ordered)
 {
  const FNode* Original=OriginalGraph.Nodes.FindByPredicate([&](const FNode& E){return E.Node.Get()==N.Node.Get();});
  for(auto& P:N.Pins)
  {
   const FPin* Old=Original->Pins.FindByPredicate([&](const FPin& E){return E.P==P.P;});if(!Old||P.Links.Num()!=Old->Links.Num()){Rollback->SetBoolField(TEXT("links_restored_through_schema"),false);Phase=EPhase::RestoreNotify;return false;}
   for(UEdGraphPin* L:Old->Links)if(!P.Links.Contains(L)){Rollback->SetBoolField(TEXT("links_restored_through_schema"),false);Phase=EPhase::RestoreNotify;return false;}
   P.Links=Old->Links;
  }
 }
 T.Expected.Nodes=Ordered;
 Resolved.Graph->Nodes.Reset();for(UEdGraphNode* N:OriginalNodeOrder)Resolved.Graph->Nodes.Add(N);
 for(const auto& N:Ordered)for(const auto& P:N.Pins)P.P->LinkedTo=P.Links;
 if(!T.End(TEXT("Recipe_Restore_ExactOrder")))return GuardFailure();
 Rollback->SetBoolField(TEXT("links_restored_through_schema"),true);Phase=EPhase::RestoreNotify;return false;
}
bool FRecipeContinuation::RestoreVerify()
{
 auto Status=InspectRecipeCompileStatus(W,Resolved.System);if(!ContinueRead())return GuardFailure();FString E;const bool CompileClean=Wait.Clean()&&NiagaraCompileStatusDidNotRegress(BaselineCompileStatus,Status,E);
 auto References=Scan();if(!ContinueRead())return GuardFailure();auto Context=BuildNiagaraRecipeScratchPadContextValidation(Resolved.Script,TEXT("rollback"),References,ScratchPadReferenceCount,CompileClean);if(!ContinueRead())return GuardFailure();auto Snapshots=CaptureNiagaraRecipeCallerSnapshots(References);if(!ContinueRead())return GuardFailure();bool IDsRestored=false;
 auto IDs=BuildNiagaraRecipeCallerCompileIdValidation(BaselineCallerSnapshots,Snapshots,nullptr,TEXT("rollback"),false,IDsRestored);const FString Fingerprint=BuildNiagaraRecipeGraphFingerprint(Resolved.Graph);if(!ContinueRead())return GuardFailure();
 bool Membership=Resolved.Graph->Nodes.Num()==OriginalNodeOrder.Num(),Order=Membership;for(int32 I=0;I<Resolved.Graph->Nodes.Num();++I){Membership&=OriginalNodes.Contains(Resolved.Graph->Nodes[I]);if(I<OriginalNodeOrder.Num())Order&=Resolved.Graph->Nodes[I]==OriginalNodeOrder[I];}
 const bool ReferencesRestored=References.bComplete&&References.ReferenceCount==ScratchPadReferenceCount&&References.ReferenceCount>0;bool Links=false;Rollback->TryGetBoolField(TEXT("links_restored_through_schema"),Links);
 const bool FingerprintRestored=!OriginalGraphFingerprint.IsEmpty()&&Fingerprint.Equals(OriginalGraphFingerprint,ESearchCase::CaseSensitive);const bool CustomRestored=Resolved.Graph->Nodes.Contains(CustomNode);const bool Structural=CustomRestored&&Membership&&Order&&Links&&FingerprintRestored&&ReferencesRestored;
 const bool Semantic=CompileClean&&Context->GetBoolField(TEXT("clean"))&&IDsRestored;
 if(!W.Step(TEXT("Recipe_Restore_KeepDirty"),[&]{Resolved.System->MarkPackageDirty();},true,true))return GuardFailure();
 Rollback->SetBoolField(TEXT("success"),false);Rollback->SetBoolField(TEXT("custom_hlsl_restored"),CustomRestored);Rollback->SetBoolField(TEXT("original_node_membership_restored"),Membership);Rollback->SetBoolField(TEXT("original_node_order_restored"),Order);Rollback->SetBoolField(TEXT("created_nodes_removed"),Membership);Rollback->SetBoolField(TEXT("graph_fingerprint_restored"),FingerprintRestored);Rollback->SetStringField(TEXT("expected_graph_fingerprint"),OriginalGraphFingerprint);Rollback->SetStringField(TEXT("restored_graph_fingerprint"),Fingerprint);Rollback->SetBoolField(TEXT("scratch_pad_references_restored"),ReferencesRestored);Rollback->SetNumberField(TEXT("expected_scratch_pad_reference_count"),ScratchPadReferenceCount);Rollback->SetNumberField(TEXT("restored_scratch_pad_reference_count"),References.ReferenceCount);Rollback->SetObjectField(TEXT("scratch_pad_compile_validation"),Context);Rollback->SetBoolField(TEXT("caller_compile_ids_restored"),IDsRestored);Rollback->SetObjectField(TEXT("caller_compile_id_validation"),IDs);Rollback->SetBoolField(TEXT("rollback_content_restored"),Structural);Rollback->SetBoolField(TEXT("structural_rollback_verified"),Structural);Rollback->SetBoolField(TEXT("semantic_compile_ids_restored"),Semantic);Rollback->SetBoolField(TEXT("system_compile_launched"),bRestoreCompileLaunched);Rollback->SetBoolField(TEXT("system_compile_force"),false);Rollback->SetBoolField(TEXT("system_compile_wait_clean"),Wait.Clean());Rollback->SetBoolField(TEXT("system_compile_clean"),CompileClean);Rollback->SetObjectField(TEXT("system_compile_wait"),Wait.Result);Rollback->SetObjectField(TEXT("system_compile_status"),Status);Rollback->SetStringField(TEXT("system_compile_error"),E);Rollback->SetBoolField(TEXT("package_dirty_state_restored"),false);Rollback->SetBoolField(TEXT("package_was_dirty_before_mutation"),false);Rollback->SetBoolField(TEXT("package_marked_dirty_for_recovery"),Resolved.System->GetOutermost()->IsDirty());
 RestoreCompleted=true;Result->SetBoolField(TEXT("rollback_verified"),false);if(SaveStateUnknown)SavedFailureContract();Phase=EPhase::Failed;return false;
}

bool FRecipeContinuation::Success()
{
TArray<TSharedPtr<FJsonValue>> FinalCreatedNodeValues;
for(const auto& Pair:CreatedNodes){if(!ContinueRead())return GuardFailure();if(Pair.Value&&Resolved.Graph->Nodes.Contains(Pair.Value)){auto CreatedJson=DetailedGraphNodeToJsonObject(Pair.Value,INDEX_NONE,true,true);if(!ContinueRead())return GuardFailure();CreatedJson->SetStringField(TEXT("node_id"),Pair.Key);FinalCreatedNodeValues.Add(MakeShared<FJsonValueObject>(CreatedJson));}}
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("dry_run"), false);
    Result->SetBoolField(TEXT("mutation_performed"), true);
    Result->SetBoolField(TEXT("mutation_applied_in_memory"), true);
    Result->SetStringField(TEXT("system_path"), Resolved.System->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_script_path"), Resolved.Script->GetPathName());
    Result->SetStringField(TEXT("scratch_pad_name"), Resolved.Script->GetName());
    Result->SetNumberField(TEXT("scratch_pad_reference_count"), ScratchPadReferenceCount);
    Result->SetBoolField(TEXT("scratch_pad_reference_scan_complete"), FinalReferenceScan.bComplete);
    Result->SetNumberField(TEXT("scratch_pad_caller_script_count"), FinalReferenceScan.CallerScripts.Num());
    Result->SetStringField(TEXT("scratch_pad_compile_validation_mode"), TEXT("referenced_system_compile"));
    Result->SetStringField(TEXT("scratch_pad_reference_scan_mode"), NiagaraRecipeScratchPadReferenceScanMode);
    Result->SetStringField(TEXT("converted_custom_hlsl_node_guid"), CustomHlslNodeGuidString);
    Result->SetStringField(TEXT("expected_hlsl_body_hash"), ExistingHlslBodyHash);
    Result->SetNumberField(TEXT("created_node_count"), FinalCreatedNodeValues.Num());
    Result->SetNumberField(TEXT("created_link_count"), CreatedLinkCount);
    Result->SetNumberField(TEXT("rewired_consumer_count"), RewiredConsumerCount);
    Result->SetNumberField(TEXT("consumed_output_count"), ConsumedCustomOutputs.Num());
    Result->SetNumberField(TEXT("mapped_consumed_output_count"), MappedConsumedOutputCount);
    Result->SetBoolField(TEXT("partial_output_mapping"), bPartialOutputMapping);
    Result->SetStringField(
        TEXT("output_mapping_policy"),
        bRemoveCustomHlsl ? TEXT("all_consumed_outputs_exactly_once") : TEXT("partial_mapping_allowed_custom_hlsl_retained"));
    Result->SetBoolField(TEXT("custom_hlsl_removed"), bCustomHlslRemoved);
    Result->SetBoolField(TEXT("compile_launched_before_removal"), bCompileLaunchedBeforeRemoval);
    Result->SetBoolField(TEXT("compile_launched_final"), bCompileLaunchedFinal);
    Result->SetBoolField(TEXT("baseline_compile_launched"), bBaselineCompileLaunched);
    Result->SetBoolField(TEXT("baseline_compile_force"), false);
    Result->SetBoolField(TEXT("pre_removal_compile_force"), false);
    Result->SetBoolField(TEXT("final_compile_force"), false);
    Result->SetObjectField(TEXT("baseline_compile_status"), BaselineCompileStatus);
    Result->SetObjectField(TEXT("baseline_scratch_pad_compile_validation"), BaselineScratchPadValidation);
    Result->SetObjectField(TEXT("pre_removal_compile_status"), PreRemovalCompileStatus);
    Result->SetObjectField(TEXT("final_compile_status"), FinalCompileStatus);
    Result->SetObjectField(TEXT("pre_removal_scratch_pad_compile_validation"), PreRemovalScratchPadValidation);
    Result->SetObjectField(TEXT("final_scratch_pad_compile_validation"), FinalScratchPadValidation);
    Result->SetObjectField(TEXT("pre_removal_caller_compile_id_validation"), PreRemovalCallerCompileIdValidation);
    Result->SetObjectField(TEXT("final_caller_compile_id_validation"), FinalCallerCompileIdValidation);
    Result->SetObjectField(TEXT("pre_removal_pending_compile_id_validation"), PreRemovalPendingCompileIdValidation);
    Result->SetObjectField(TEXT("final_pending_compile_id_validation"), FinalPendingCompileIdValidation);
    Result->SetBoolField(TEXT("pre_removal_pending_compile_ids_changed"), bPreRemovalPendingCompileIdsChanged);
    Result->SetBoolField(TEXT("final_pending_compile_ids_changed"), bFinalPendingCompileIdsChanged);
    Result->SetBoolField(TEXT("pre_removal_caller_compile_ids_changed"), bPreRemovalCallerCompileIdsChanged);
    Result->SetBoolField(TEXT("final_caller_compile_ids_changed"), bFinalCallerCompileIdsChanged);
    Result->SetObjectField(TEXT("baseline_compile_wait"), BaselineCompileWait);
    Result->SetObjectField(TEXT("pre_removal_compile_wait"), PreRemovalWait);
    Result->SetObjectField(TEXT("final_compile_wait"), FinalWait);
    Result->SetArrayField(TEXT("created_nodes"), FinalCreatedNodeValues);
    Result->SetObjectField(TEXT("preflight_dependency_audit"), DependencyAudit);
    Result->SetObjectField(TEXT("dependency_audit"), FinalDependencyAudit);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("temp_only_native_niagara_graph_recipe"));
    Result->SetStringField(TEXT("runtime_dependency"), TEXT("none detected by direct system and transitive selected-function pre-save audit"));
    W.R.Data->SetObjectField(TEXT("legacy_result"),Result);W.R.Data->SetBoolField(TEXT("actual_compile_completion_verified"),true);W.R.Compiled=true;return true;
}

TSharedPtr<FJsonObject> HandleRecipe(
    const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITORONLY_DATA
    FResolvedSystemScratchPadGraph Resolved;
    FString ResolveError;
    auto& W=*Current;
    if (!ResolveSystemScratchPadGraph(Params, Resolved, ResolveError))
    {
        return MakeNiagaraGraphRecipeError(ResolveError, TEXT("niagara_graph_recipe_scratch_pad_resolve_failed"));
    }
    if(!ContinueRead())return ErrorResponse(W.R.Error);
    if (!IsTempGeneratedNiagaraPath(Resolved.System->GetPathName()))
    {
        return MakeNiagaraGraphRecipeError(
            FString::Printf(TEXT("Graph recipes are restricted to %s ghost systems: %s"), NiagaraTempGenerationRoot, *Resolved.System->GetPathName()),
            TEXT("niagara_graph_recipe_non_temp_rejected"));
    }

    bool bDryRun = true;
    bool bSave = true;
    bool bRequestCompile = true;
    bool bRemoveCustomHlsl = true;
    double TimeoutSeconds = 30.0;
    double PollIntervalSeconds = 0.05;
    Params->TryGetBoolField(TEXT("dry_run"), bDryRun);
    Params->TryGetBoolField(TEXT("save"), bSave);
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    Params->TryGetBoolField(TEXT("remove_custom_hlsl"), bRemoveCustomHlsl);
    Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
    Params->TryGetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);
    TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 1.0, 60.0);
    PollIntervalSeconds = FMath::Clamp(PollIntervalSeconds, 0.02, 1.0);

    if (!bDryRun && !bRequestCompile)
    {
        return MakeNiagaraGraphRecipeError(TEXT("Apply mode requires request_compile=true"), TEXT("niagara_graph_recipe_compile_required"));
    }
    if (!bDryRun && GEditor && GEditor->GetPIEWorldContext() != nullptr)
    {
        return MakeNiagaraGraphRecipeError(TEXT("End PIE/SIE before applying a Niagara graph recipe"), TEXT("niagara_graph_recipe_play_active"));
    }

    FString CustomHlslNodeGuidString;
    FString ExpectedHlslBodyHash;
    Params->TryGetStringField(TEXT("custom_hlsl_node_guid"), CustomHlslNodeGuidString);
    Params->TryGetStringField(TEXT("expected_hlsl_body_hash"), ExpectedHlslBodyHash);
    FGuid CustomHlslNodeGuid;
    if (!FGuid::Parse(CustomHlslNodeGuidString, CustomHlslNodeGuid) || !CustomHlslNodeGuid.IsValid())
    {
        return MakeNiagaraGraphRecipeError(TEXT("A valid custom_hlsl_node_guid is required"), TEXT("niagara_graph_recipe_invalid_hlsl_guid"));
    }

    UNiagaraNodeCustomHlsl* CustomNode = nullptr;
    int32 CustomNodeMatchCount = 0;
    for (UEdGraphNode* Node : Resolved.Graph->Nodes)
    {
        UNiagaraNodeCustomHlsl* Candidate = Cast<UNiagaraNodeCustomHlsl>(Node);
        if (Candidate && Candidate->NodeGuid == CustomHlslNodeGuid)
        {
            CustomNode = Candidate;
            ++CustomNodeMatchCount;
        }
    }
    if (CustomNodeMatchCount != 1 || !CustomNode)
    {
        return MakeNiagaraGraphRecipeError(
            FString::Printf(TEXT("Custom HLSL node GUID must resolve exactly once; matched %d"), CustomNodeMatchCount),
            TEXT("niagara_graph_recipe_hlsl_not_found"));
    }
    TStrongObjectPtr<UNiagaraNodeCustomHlsl> CustomNodeGuard(CustomNode);

    const FNiagaraRecipeScratchPadReferenceScan InitialReferenceScan = ScanNiagaraRecipeScratchPadSystemReferences(
        Resolved.System,
        Resolved.Script);
    if(!ContinueRead())return ErrorResponse(W.R.Error);
    const int32 ScratchPadReferenceCount = InitialReferenceScan.ReferenceCount;
    if (!InitialReferenceScan.bComplete || ScratchPadReferenceCount <= 0)
    {
        TSharedPtr<FJsonObject> Error = MakeNiagaraGraphRecipeError(
            TEXT("Selected Scratch Pad has no direct enabled reference reachable from an active system or emitter output and cannot be validated in its real compile context"),
            TEXT("niagara_graph_recipe_scratch_pad_unreferenced"));
        Error->SetNumberField(TEXT("scratch_pad_reference_count"), ScratchPadReferenceCount);
        Error->SetBoolField(TEXT("reference_scan_complete"), InitialReferenceScan.bComplete);
        Error->SetArrayField(
            TEXT("unresolved_output_contexts"),
            StringArrayToJsonValues(InitialReferenceScan.UnresolvedOutputContexts));
        Error->SetStringField(TEXT("reference_scan_mode"), NiagaraRecipeScratchPadReferenceScanMode);
        Error->SetStringField(TEXT("indirect_reference_policy"), TEXT("reject_as_unproven"));
        Error->SetStringField(TEXT("unreferenced_policy"), TEXT("reject_before_mutation"));
        return Error;
    }

    const FString ExistingHlsl = GetNiagaraCustomHlslString(CustomNode);
    if(!ContinueRead())return ErrorResponse(W.R.Error);
    const FString ExistingHlslBodyHash = FMD5::HashAnsiString(*ExistingHlsl);
    if (!bDryRun && (ExpectedHlslBodyHash.IsEmpty() || !ExpectedHlslBodyHash.Equals(ExistingHlslBodyHash, ESearchCase::IgnoreCase)))
    {
        return MakeNiagaraGraphRecipeError(
            TEXT("Apply mode requires the current expected_hlsl_body_hash returned by dry-run or inspection"),
            TEXT("niagara_graph_recipe_hlsl_hash_changed"));
    }

    const TArray<TSharedPtr<FJsonValue>>* NodeSpecs = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* LinkSpecs = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* OutputMappings = nullptr;
    if (!Params->TryGetArrayField(TEXT("nodes"), NodeSpecs) || !NodeSpecs ||
        !Params->TryGetArrayField(TEXT("links"), LinkSpecs) || !LinkSpecs ||
        !Params->TryGetArrayField(TEXT("output_mappings"), OutputMappings) || !OutputMappings)
    {
        return MakeNiagaraGraphRecipeError(TEXT("Recipe requires nodes, links, and output_mappings arrays"), TEXT("niagara_graph_recipe_missing_arrays"));
    }
    if (NodeSpecs->Num() > 256 || LinkSpecs->Num() > 1024 || OutputMappings->Num() > 32)
    {
        return MakeNiagaraGraphRecipeError(TEXT("Recipe exceeds node/link/output safety limits"), TEXT("niagara_graph_recipe_limit_exceeded"));
    }
    if (OutputMappings->Num() == 0)
    {
        return MakeNiagaraGraphRecipeError(TEXT("Recipe must map at least one Custom HLSL output"), TEXT("niagara_graph_recipe_missing_output_mapping"));
    }

    UNiagaraGraph* TemporaryOwner = nullptr;
    const TArray<TSharedPtr<FNiagaraAction_NewNode>> Actions = CollectNiagaraGraphActions(Resolved.Graph, TemporaryOwner);
    TStrongObjectPtr<UNiagaraGraph> TemporaryOwnerGuard(TemporaryOwner);
    if(!ContinueRead())return ErrorResponse(W.R.Error);
    TMap<FString, TSharedPtr<FNiagaraAction_NewNode>> ActionById;
    for (const TSharedPtr<FNiagaraAction_NewNode>& Action : Actions)
    {
        if (!Action.IsValid() || !Action->WeakNodeTemplate.IsValid())
        {
            continue;
        }
        UEdGraphNode* Template = Action->WeakNodeTemplate.Get();
        const FString FunctionPath = ReadNiagaraActionNameProperty(Template, TEXT("FunctionScriptAssetObjectPath"));
        if (IsNiagaraGraphRecipeNodeClassAllowed(Template->GetClass()->GetName(), FunctionPath))
        {
            ActionById.Add(BuildNiagaraGraphActionId(*Action), Action);
        }
    }

    TSet<FString> NodeIds;
    TArray<TSharedPtr<FJsonValue>> PlannedNodeValues;
    TArray<FString> SelectedFunctionPaths;
    for (const TSharedPtr<FJsonValue>& NodeValue : *NodeSpecs)
    {
        const TSharedPtr<FJsonObject> NodeSpec = NodeValue.IsValid()&&NodeValue->Type==EJson::Object ? NodeValue->AsObject() : nullptr;
        FString NodeId;
        FString ActionId;
        if (!NodeSpec.IsValid() || !NodeSpec->TryGetStringField(TEXT("node_id"), NodeId) || !NodeSpec->TryGetStringField(TEXT("action_id"), ActionId))
        {
            return MakeNiagaraGraphRecipeError(TEXT("Every recipe node requires node_id and action_id"), TEXT("niagara_graph_recipe_invalid_node_spec"));
        }
        NodeId.TrimStartAndEndInline();
        ActionId.TrimStartAndEndInline();
        if (NodeId.IsEmpty() || NodeIds.Contains(NodeId))
        {
            return MakeNiagaraGraphRecipeError(FString::Printf(TEXT("Recipe node_id is empty or duplicated: %s"), *NodeId), TEXT("niagara_graph_recipe_duplicate_node_id"));
        }
        const TSharedPtr<FNiagaraAction_NewNode>* FoundAction = ActionById.Find(ActionId);
        if (!FoundAction || !FoundAction->IsValid())
        {
            return MakeNiagaraGraphRecipeError(FString::Printf(TEXT("Recipe action_id is unavailable or not allowlisted: %s"), *ActionId), TEXT("niagara_graph_recipe_action_unavailable"));
        }
        NodeIds.Add(NodeId);
        // Canonicalize the validated request in place so the mutation phase
        // cannot re-read whitespace-different identifiers and bypass this plan.
        NodeSpec->SetStringField(TEXT("node_id"), NodeId);
        NodeSpec->SetStringField(TEXT("action_id"), ActionId);

        double NodeX = CustomNode->NodePosX - 360.0;
        double NodeY = CustomNode->NodePosY;
        if ((NodeSpec->HasField(TEXT("x")) &&
             (!NodeSpec->HasTypedField<EJson::Number>(TEXT("x")) || !NodeSpec->TryGetNumberField(TEXT("x"), NodeX))) ||
            (NodeSpec->HasField(TEXT("y")) &&
             (!NodeSpec->HasTypedField<EJson::Number>(TEXT("y")) || !NodeSpec->TryGetNumberField(TEXT("y"), NodeY))) ||
            !FMath::IsFinite(NodeX) || !FMath::IsFinite(NodeY) ||
            FMath::Abs(NodeX) > NiagaraGraphRecipeMaxAbsNodeCoordinate ||
            FMath::Abs(NodeY) > NiagaraGraphRecipeMaxAbsNodeCoordinate)
        {
            return MakeNiagaraGraphRecipeError(
                FString::Printf(
                    TEXT("Recipe node position must be finite and within +/-%.0f for node_id: %s"),
                    NiagaraGraphRecipeMaxAbsNodeCoordinate,
                    *NodeId),
                TEXT("niagara_graph_recipe_invalid_node_position"));
        }
        NodeSpec->SetNumberField(TEXT("x"), NodeX);
        NodeSpec->SetNumberField(TEXT("y"), NodeY);

        TSharedPtr<FJsonObject> PlanNode = NiagaraGraphActionToJson(**FoundAction);
        const FString SelectedFunctionPath = PlanNode->GetStringField(TEXT("function_script_path"));
        if (!SelectedFunctionPath.IsEmpty())
        {
            SelectedFunctionPaths.AddUnique(SelectedFunctionPath);
        }
        PlanNode->SetStringField(TEXT("node_id"), NodeId);
        PlanNode->SetNumberField(TEXT("x"), NodeX);
        PlanNode->SetNumberField(TEXT("y"), NodeY);
        PlanNode->RemoveField(TEXT("canonical"));
        PlannedNodeValues.Add(MakeShared<FJsonValueObject>(PlanNode));
    }

    TArray<TSharedPtr<FJsonValue>> CustomInputValues;
    TArray<TSharedPtr<FJsonValue>> CustomOutputValues;
    for (UEdGraphPin* Pin : CustomNode->Pins)
    {
        if (!Pin || IsNiagaraDynamicAddPin(Pin))
        {
            continue;
        }
        TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
        PinObject->SetStringField(TEXT("name"), Pin->PinName.ToString());
        PinObject->SetStringField(TEXT("type"), NiagaraTypeName(UEdGraphSchema_Niagara::PinToTypeDefinition(Pin)));
        PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);
        PinObject->SetNumberField(TEXT("link_count"), Pin->LinkedTo.Num());
        if (Pin->Direction == EGPD_Input)
        {
            CustomInputValues.Add(MakeShared<FJsonValueObject>(PinObject));
        }
        else if (Pin->Direction == EGPD_Output)
        {
            CustomOutputValues.Add(MakeShared<FJsonValueObject>(PinObject));
        }
    }

    TSet<UEdGraphPin*> ConsumedCustomOutputs;
    for (UEdGraphPin* Pin : CustomNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && !IsNiagaraDynamicAddPin(Pin) && Pin->LinkedTo.Num() > 0)
        {
            ConsumedCustomOutputs.Add(Pin);
        }
    }

    TMap<UEdGraphPin*, int32> OutputMappingCounts;
    for (const TSharedPtr<FJsonValue>& MappingValue : *OutputMappings)
    {
        const TSharedPtr<FJsonObject> Mapping = MappingValue.IsValid()&&MappingValue->Type==EJson::Object ? MappingValue->AsObject() : nullptr;
        FString CustomOutputName;
        const TSharedPtr<FJsonObject>* SourceEndpoint = nullptr;
        if (!Mapping.IsValid() ||
            !Mapping->TryGetStringField(TEXT("custom_output_pin"), CustomOutputName) ||
            !Mapping->TryGetObjectField(TEXT("source"), SourceEndpoint) || !SourceEndpoint || !SourceEndpoint->IsValid())
        {
            return MakeNiagaraGraphRecipeError(
                TEXT("Every output mapping requires custom_output_pin and source before dry-run"),
                TEXT("niagara_graph_recipe_invalid_output_mapping"));
        }
        CustomOutputName.TrimStartAndEndInline();
        Mapping->SetStringField(TEXT("custom_output_pin"), CustomOutputName);
        UEdGraphPin* CustomOutputPin = FindNiagaraRecipePin(CustomNode, EGPD_Output, CustomOutputName, 0);
        if (!CustomOutputPin)
        {
            return MakeNiagaraGraphRecipeError(
                FString::Printf(TEXT("Custom HLSL output is missing: %s"), *CustomOutputName),
                TEXT("niagara_graph_recipe_output_mapping_missing_pin"));
        }
        if (CustomOutputPin->LinkedTo.Num() == 0)
        {
            return MakeNiagaraGraphRecipeError(
                FString::Printf(TEXT("Custom HLSL output has no consumers to preserve: %s"), *CustomOutputName),
                TEXT("niagara_graph_recipe_output_unconsumed"));
        }
        const int32 NewCount = OutputMappingCounts.FindOrAdd(CustomOutputPin) + 1;
        OutputMappingCounts.Add(CustomOutputPin, NewCount);
        if (NewCount != 1)
        {
            return MakeNiagaraGraphRecipeError(
                FString::Printf(TEXT("Custom HLSL output is mapped more than once: %s"), *CustomOutputName),
                TEXT("niagara_graph_recipe_output_mapping_ambiguous"));
        }
    }

    TArray<FString> MissingConsumedOutputNames;
    int32 MappedConsumedOutputCount = 0;
    for (UEdGraphPin* ConsumedOutput : ConsumedCustomOutputs)
    {
        const int32 MappingCount = OutputMappingCounts.FindRef(ConsumedOutput);
        if (MappingCount == 1)
        {
            ++MappedConsumedOutputCount;
        }
        else
        {
            MissingConsumedOutputNames.Add(ConsumedOutput->PinName.ToString());
        }
    }
    MissingConsumedOutputNames.Sort();
    const bool bPartialOutputMapping = MappedConsumedOutputCount != ConsumedCustomOutputs.Num();
    if (bRemoveCustomHlsl && bPartialOutputMapping)
    {
        TSharedPtr<FJsonObject> Error = MakeNiagaraGraphRecipeError(
            TEXT("remove_custom_hlsl=true requires every consumed Custom HLSL output to be mapped exactly once"),
            TEXT("niagara_graph_recipe_consumed_output_mapping_incomplete"));
        Error->SetNumberField(TEXT("consumed_output_count"), ConsumedCustomOutputs.Num());
        Error->SetNumberField(TEXT("mapped_consumed_output_count"), MappedConsumedOutputCount);
        Error->SetArrayField(TEXT("missing_consumed_outputs"), StringArrayToJsonValues(MissingConsumedOutputNames));
        return Error;
    }

    const TSharedPtr<FJsonObject> DependencyAudit = BuildNiagaraRecipePreSaveDependencyAudit(
        Resolved.System,
        Resolved.Graph,
        SelectedFunctionPaths,
        *NodeSpecs);
    if(!ContinueRead())return ErrorResponse(W.R.Error);
    if (!DependencyAudit.IsValid() || !DependencyAudit->GetBoolField(TEXT("success")))
    {
        TSharedPtr<FJsonObject> Error = MakeNiagaraGraphRecipeError(
            TEXT("Niagara graph recipe dependency preflight was incomplete or found an MCP-only dependency"),
            TEXT("niagara_graph_recipe_dependency_preflight_failed"));
        if (DependencyAudit.IsValid())
        {
            Error->SetObjectField(TEXT("dependency_audit"), DependencyAudit);
        }
        return Error;
    }

    if (bDryRun)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("dry_run"), true);
        Result->SetBoolField(TEXT("mutation_performed"), false);
        Result->SetBoolField(TEXT("mutation_applied_in_memory"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetStringField(TEXT("system_path"), Resolved.System->GetPathName());
        Result->SetStringField(TEXT("scratch_pad_script_path"), Resolved.Script->GetPathName());
        Result->SetStringField(TEXT("scratch_pad_name"), Resolved.Script->GetName());
        Result->SetNumberField(TEXT("scratch_pad_reference_count"), ScratchPadReferenceCount);
        Result->SetBoolField(TEXT("scratch_pad_reference_scan_complete"), InitialReferenceScan.bComplete);
        Result->SetNumberField(TEXT("scratch_pad_caller_script_count"), InitialReferenceScan.CallerScripts.Num());
        Result->SetStringField(TEXT("scratch_pad_compile_validation_mode"), TEXT("referenced_system_compile"));
        Result->SetStringField(TEXT("scratch_pad_reference_scan_mode"), NiagaraRecipeScratchPadReferenceScanMode);
        Result->SetStringField(TEXT("custom_hlsl_node_guid"), CustomHlslNodeGuidString);
        Result->SetStringField(TEXT("expected_hlsl_body_hash"), ExistingHlslBodyHash);
        Result->SetNumberField(TEXT("planned_node_count"), NodeSpecs->Num());
        Result->SetNumberField(TEXT("planned_link_count"), LinkSpecs->Num());
        Result->SetNumberField(TEXT("planned_output_mapping_count"), OutputMappings->Num());
        Result->SetNumberField(TEXT("consumed_output_count"), ConsumedCustomOutputs.Num());
        Result->SetNumberField(TEXT("mapped_consumed_output_count"), MappedConsumedOutputCount);
        Result->SetBoolField(TEXT("partial_output_mapping"), bPartialOutputMapping);
        Result->SetStringField(
            TEXT("output_mapping_policy"),
            bRemoveCustomHlsl ? TEXT("all_consumed_outputs_exactly_once") : TEXT("partial_mapping_allowed_custom_hlsl_retained"));
        Result->SetArrayField(TEXT("planned_nodes"), PlannedNodeValues);
        Result->SetArrayField(TEXT("custom_hlsl_inputs"), CustomInputValues);
        Result->SetArrayField(TEXT("custom_hlsl_outputs"), CustomOutputValues);
        Result->SetObjectField(TEXT("dependency_audit"), DependencyAudit);
        Result->SetBoolField(TEXT("baseline_compile_force"), false);
        Result->SetBoolField(TEXT("pre_removal_compile_force"), false);
        Result->SetBoolField(TEXT("final_compile_force"), false);
        Result->SetStringField(TEXT("write_scope"), TEXT("temp_only_native_niagara_graph_recipe_dry_run"));
        return Result;
    }


    if(!ContinueRead())return ErrorResponse(W.R.Error);
    if(Resolved.System->GetOutermost()->IsDirty())return MakeNiagaraGraphRecipeError(TEXT("Refusing to mutate ghost System with preexisting unsaved changes"),TEXT("niagara_graph_recipe_package_dirty"));
    if(!W.DeferredMode)return MakeNiagaraGraphRecipeError(TEXT("Recipe apply requires genuine baseline/pre-removal/final compile-idle, cached/pending ID proof and semantic restore. These are not implemented by a RequestCompile-only path; rejected before first source writer."),TEXT("niagara_graph_recipe_async_compile_restore_unsupported"));
    if(!WritableGraph(W,Resolved.Source,CustomNode))return ErrorResponse(W.R.Error);
    auto State=MakeShared<FRecipeContinuation>(W);State->Resolved=Resolved;State->CustomNode=CustomNode;State->TemporaryOwner=TemporaryOwner;State->NodeSpecs=*NodeSpecs;State->LinkSpecs=*LinkSpecs;State->OutputMappings=*OutputMappings;State->PlannedNodeValues=PlannedNodeValues;State->CustomInputValues=CustomInputValues;State->CustomOutputValues=CustomOutputValues;State->SelectedFunctionPaths=SelectedFunctionPaths;State->ActionById=ActionById;State->ScratchPadReferenceCount=ScratchPadReferenceCount;State->MappedConsumedOutputCount=MappedConsumedOutputCount;State->ConsumedCustomOutputs=ConsumedCustomOutputs;State->bPartialOutputMapping=bPartialOutputMapping;State->bSave=bSave;State->bRemoveCustomHlsl=bRemoveCustomHlsl;State->CustomHlslNodeGuidString=CustomHlslNodeGuidString;State->ExistingHlslBodyHash=ExistingHlslBodyHash;State->TimeoutSeconds=TimeoutSeconds;State->PollIntervalSeconds=PollIntervalSeconds;State->DependencyAudit=DependencyAudit;State->Init();W.DeferredStep=[State](){return State->Tick();};W.DeferredInterrupted=[State](){State->Interrupted();};return State->Result;
#else
    return ErrorResponse(TEXT("Graph recipe requires editor-only data"));
#endif
}


FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Leaf,const TCHAR* Operation)
{
 TSharedPtr<FJsonObject> Legacy=Leaf;if(!Legacy.IsValid())Legacy=W.Error(TEXT("Native HLSL operation returned no result"));const TSharedPtr<FJsonObject>* Prior=nullptr;if(W.R.Data->TryGetObjectField(TEXT("legacy_result"),Prior)&&Prior&&Prior->IsValid()&&Prior->Get()!=Legacy.Get()&&!W.R.Error.IsEmpty()){Legacy=*Prior;Legacy->SetStringField(TEXT("error"),W.R.Error);Legacy->SetBoolField(TEXT("success"),false);}
 bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);if(FCString::Strcmp(Operation,TEXT("apply_niagara_scratch_pad_graph_recipe"))==0){bool DeclaredMutation=false;Legacy->TryGetBoolField(TEXT("mutation_performed"),DeclaredMutation);const bool TargetMutation=W.TargetChanged||(W.DeferredMode&&DeclaredMutation);Legacy->SetBoolField(TEXT("mutation_performed"),TargetMutation);Legacy->SetBoolField(TEXT("mutation_applied_in_memory"),TargetMutation);Legacy->SetBoolField(TEXT("saved"),W.R.bSaved);}
 W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);W.R.Data->SetArrayField(TEXT("sdk_calls"),W.Calls);W.R.Data->SetObjectField(TEXT("sdk_call_counts"),W.Counts);W.R.Data->SetArrayField(TEXT("actual_save_prefix"),W.SaveRows);double ExplicitPolls=0;W.Counts->TryGetNumberField(TEXT("Conversion_PollForCompilationComplete"),ExplicitPolls);W.R.Data->SetBoolField(TEXT("explicit_wait_poll_pump_flush_sleep_invoked"),ExplicitPolls>0);W.R.Data->SetBoolField(TEXT("explicit_global_task_pump_invoked"),false);W.R.Data->SetBoolField(TEXT("explicit_sleep_flush_invoked"),false);W.R.Data->SetNumberField(TEXT("explicit_system_poll_count"),ExplicitPolls);W.R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),W.Persistent&&W.R.bMutationAttempted);W.R.Data->SetBoolField(TEXT("transient_creation_only"),!W.Persistent);W.R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);W.R.Data->SetStringField(TEXT("scoped_known_scope"),TEXT("Exact selected System/script/source/graph/object identities, physical membership and 1:1 HLSL pin/default/reciprocal links plus literal body and current dirty. Not SDK-global purity, compile completion, cold persisted closure, world/GPU effects."));W.R.Data->SetStringField(TEXT("SDK_internal_cost"),TEXT("Property lifecycle, virtual Serialize/GetGraphActions/ComputeVMCompilationId and SaveLoadedAsset/PreSave may perform synchronous internal work. Async uses owned finite System polling; no wrapper task pump/Sleep/flush or hard nonblocking guarantee."));
 if(W.R.Error.IsEmpty()&&Success&&W.Admit(TEXT("final_typed_projection"))){W.R.bSuccess=true;W.R.bPostconditionPassed=true;W.R.bChangeStateKnown=true;W.R.bChanged=W.TargetChanged;W.R.Stage=TEXT("complete");}else{if(W.R.Error.IsEmpty())W.Fail(TEXT("niagara_hlsl_conversion_failed"),TEXT("Legacy/typed postcondition failed"));W.R.bSuccess=false;W.R.bPostconditionPassed=false;if(W.R.bMutationAttempted)W.R.bChangeStateKnown=false;W.R.bChanged=W.TargetChanged;if(!W.FailedStage.IsEmpty())W.R.Stage=W.FailedStage;}
 W.R.Data->SetObjectField(TEXT("object_observation"),W.DirtyProjection());W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),W.R.bPostconditionPassed);return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(const TSharedPtr<FJsonObject>&);
FString Run(const FString& Text,const TCHAR* Operation,FHandler Handler)
{
 FWork W(false);if(Running||DeferredLeaseActive){W.Fail(TEXT("niagara_hlsl_conversion_reentrant_call"),TEXT("Nested Native dispatch refused; active Current not replaced"));return Operations::EncodeVariableResult(W.R,Operation);}TGuardValue<bool> Enter(Running,true);TGuardValue<FWork*> Scope(Current,&W);TSharedPtr<FJsonObject> Params;if(!Parse(Text,Params)){W.Fail(TEXT("niagara_hlsl_conversion_invalid_params"),TEXT("Bounded finite object with exact unique keys and no embedded NUL required"));return Finish(W,W.Error(W.R.Error),Operation);}W.R.bCompileRequested=true;W.R.bSaveRequested=true;Params->TryGetBoolField(TEXT("request_compile"),W.R.bCompileRequested);Params->TryGetBoolField(TEXT("save"),W.R.bSaveRequested);
 if(!W.Admit(TEXT("entry")))return Finish(W,W.Error(W.R.Error),Operation);return Finish(W,Handler(Params),Operation);
}
}
FString ReplaceHlsl(const FString& ParamsJson){return Run(ParamsJson,TEXT("replace_niagara_custom_hlsl"),&HandleReplace);}
FString ApplyGraphRecipe(const FString& ParamsJson){return Run(ParamsJson,TEXT("apply_niagara_scratch_pad_graph_recipe"),&HandleRecipe);}
}


namespace Cubeless::UE58::NiagaraCustomHlslConversionAsync
{
using namespace Cubeless::UE58::NiagaraCustomHlslConversion;
namespace
{
std::atomic<bool>& Stopping=DeferredShutdownRequested;
TSharedPtr<FJobState> ActiveJob;
TMap<const UCubelessNiagaraHlslConversionJob*,FString> SnapshotCache;
TMap<const UCubelessNiagaraHlslConversionJob*,TWeakObjectPtr<UCubelessNiagaraHlslConversionJob>> CacheOwners;
// Unsafe records retain exact target/Job holds; Shutdown never drops them inside SDK calls or GC.
TArray<TSharedPtr<FJobState>>& UnsafeRecords(){static auto* Records=new TArray<TSharedPtr<FJobState>>();return *Records;}
enum class EPhase:uint8 { Prepare, Continue, Final, Terminal, Quarantined };
}
struct FJobState:public TSharedFromThis<FJobState>
{
 FWork Work{false};
 TStrongObjectPtr<UCubelessNiagaraHlslConversionJob> Job{nullptr};
 UCubelessNiagaraHlslConversionJob* RawJob=nullptr;
 TSharedPtr<FJsonObject> Params,Legacy;
 FString Operation,Id,LastSnapshot;
 EPhase Phase=EPhase::Prepare;
 FTSTicker::FDelegateHandle Ticker;
 FDelegateHandle PreExit;
 uint64 LastFrame=MAX_uint64;
 int64 Revision=0;
 bool Executing=false,Dispatched=false,FinalEncoded=false;
 explicit FJobState(const TCHAR* Op):Operation(Op),Id(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens))
 {Work.DeferredMode=true;Work.R.Data->SetBoolField(TEXT("async_hlsl_conversion_continuation"),true);}
 bool PublicationSafe()const
 {
  if(!Safe()||Work.CapturedEditor.Get()!=GEditor)return false;
  if(Work.Persistent&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld))return false;
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
  auto& D=Work.R.Data;Work.R.bChanged=Work.TargetChanged;
  D->SetStringField(TEXT("native_job_id"),Id);
  D->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
  D->SetBoolField(TEXT("native_job_terminal"),Terminal);
  D->SetBoolField(TEXT("operation_complete"),Terminal);
  D->SetStringField(TEXT("operation"),Operation);
  D->SetBoolField(TEXT("automatic_retry_permitted"),false);
  D->SetBoolField(TEXT("async_core_dispatched"),Dispatched);
  
  
  
  
  
  D->SetBoolField(TEXT("timeout_does_not_imply_compile_success"),true);
  D->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);
  D->SetStringField(TEXT("continuation_scope"),TEXT("Exact existing HLSL conversion core on finite GT ticks. Original caller IDs and recipe structural/semantic restore are separate proof phases; no global task pump/Sleep/flush/replay. SDK calls may block internally."));
  D->SetObjectField(TEXT("sdk_call_counts"),Work.Counts);
  D->SetArrayField(TEXT("sdk_calls"),Work.Calls);
  
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
  Work.CancelRequested=true;Work.CancelReason=Reason;if(Work.DeferredInterrupted)Work.DeferredInterrupted();RefreshLegacy();
  if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_hlsl_conversion_async_context_quarantined"),Reason);
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
  if(!PublicationSafe()){Quarantine(TEXT("Editor/world/GC/exit unavailable while publishing pending HLSL conversion snapshot."));return;}
  Cache(Operations::EncodeVariableResult(Work.R,*Operation),false,true);
 }
 void SetFailureLegacy()
 {
  if(!Legacy)Legacy=MakeShared<FJsonObject>();
  Legacy->SetBoolField(TEXT("success"),false);
  Legacy->SetStringField(TEXT("error"),Work.R.Error.IsEmpty()?TEXT("HLSLConversion job stopped; actual prefix retained."):Work.R.Error);
 }
 void Finalize()
 {
  if(FinalEncoded||Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("HLSLConversion terminal publication lost its exact Editor world; no recovery or replay."));return;}
  if(!Work.R.Error.IsEmpty()){if(Work.DeferredInterrupted)Work.DeferredInterrupted();RefreshLegacy();SetFailureLegacy();}
  Metadata(true);
  LastSnapshot=Finish(Work,Legacy,*Operation);
  FinalEncoded=true;
  if(!PublicationSafe()){FinalEncoded=false;Quarantine(TEXT("HLSLConversion final guard lost publication context."));return;}
  Cache(LastSnapshot,true,true);
  Phase=EPhase::Terminal;Unhook();
 }
 void RefreshLegacy()
 {
  const TSharedPtr<FJsonObject>* Stored=nullptr;if(Work.R.Data->TryGetObjectField(TEXT("legacy_result"),Stored)&&Stored&&Stored->IsValid())Legacy=*Stored;
 }
 void ExecutePhase()
 {
  if(Work.CancelRequested||Stopping.load())
  {
   if(!Work.CancelRequested){Work.CancelRequested=true;Work.CancelReason=TEXT("Module shutdown stopped the owned HLSL conversion job.");}
   Work.Fail(TEXT("niagara_hlsl_conversion_cancelled"),Work.CancelReason);
  }
  if(!Work.R.Error.IsEmpty())
  {
   if(Work.DeferredInterrupted)Work.DeferredInterrupted();RefreshLegacy();Phase=EPhase::Final;Finalize();return;
  }
  if(!Work.Admit(TEXT("async_conversion_phase_admission")))
  {
   if(Work.DeferredInterrupted)Work.DeferredInterrupted();RefreshLegacy();Phase=EPhase::Final;Finalize();return;
  }
  if(Phase==EPhase::Prepare)
  {
   if(Dispatched){Work.Fail(TEXT("niagara_hlsl_conversion_async_replay_forbidden"),TEXT("Original operation already dispatched."));Phase=EPhase::Final;Finalize();return;}
   Dispatched=true;Legacy=Operation==TEXT("replace_niagara_custom_hlsl")?HandleReplace(Params):HandleRecipe(Params);if(Legacy.IsValid())Work.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
   Phase=Work.DeferredStep?EPhase::Continue:EPhase::Final;
  }
  else if(Phase==EPhase::Continue)
  {
   const bool Complete=Work.DeferredStep();RefreshLegacy();if(Complete)Phase=EPhase::Final;
  }
  else if(Phase==EPhase::Final){Finalize();return;}
  if(!Work.R.Error.IsEmpty()){if(Work.DeferredInterrupted)Work.DeferredInterrupted();RefreshLegacy();Phase=EPhase::Final;Finalize();return;}
  PublishPending();
 }
 void ReleaseSafe()
 {
  if(Executing||Phase!=EPhase::Terminal||!PublicationSafe())return;
  Work.DeferredStep=nullptr;Work.DeferredInterrupted=nullptr;
  Work.Objects.Reset();Work.TransitionHolds.Reset();Job.Reset();
  if(ActiveJob.Get()==this){ActiveJob.Reset();DeferredLeaseActive=false;}
 }
 bool Tick(float)
 {
  TSharedRef<FJobState> Hold=AsShared();
  if(Phase==EPhase::Terminal||Phase==EPhase::Quarantined)return false;
  if(Executing||Running||LastFrame==GFrameCounter)return true;
  LastFrame=GFrameCounter;
  if(!PublicationSafe()){Quarantine(TEXT("HLSLConversion ticker lost live Editor/game-thread/world context."));return false;}
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
  PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([Weak](){Stopping.store(true);if(!IsInGameThread())return;auto State=Weak.Pin();if(State.IsValid()){State->Work.CancelRequested=true;State->Work.CancelReason=TEXT("Engine pre-exit stopped HLSL conversion job.");State->Unhook();if(!State->Executing)State->Quarantine(State->Work.CancelReason);}});
 }
};
namespace
{
void PruneSnapshots()
{
 for(auto It=CacheOwners.CreateIterator();It;++It)if(!It.Value().IsValid()){SnapshotCache.Remove(It.Key());It.RemoveCurrent();}
}
UCubelessNiagaraHlslConversionJob* Start(const FString& Text,const TCHAR* Operation)
{
 if(!IsInGameThread()||Stopping.load()||Running||DeferredLeaseActive||!Safe())return nullptr;
 auto State=MakeShared<FJobState>(Operation);
 State->Work.R.bSaveRequested=true;
 if(!State->Work.Admit(TEXT("async_hlsl_conversion_start_admission")))return nullptr;
 PruneSnapshots();
 if(SnapshotCache.Num()>=256)return nullptr;
 ActiveJob=State;DeferredLeaseActive=true;
 UCubelessNiagaraHlslConversionJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(State->Executing,true);
  TGuardValue<bool> CoreLease(Running,true);
  TGuardValue<FWork*> Context(Current,&State->Work);
  if(State->Work.SDK(TEXT("NewObject_HLSLConversionJob"),true,false))
  {
   Created=NewObject<UCubelessNiagaraHlslConversionJob>(GetTransientPackage());
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_returned"),true);
   State->Work.R.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
   State->RawJob=Created;
   if(Stopping.load()||State->Work.CancelRequested||!State->PublicationSafe())
   {
    State->Quarantine(TEXT("HLSLConversion Job allocation interrupted; persistent HLSL conversion dispatch not entered."));
    return nullptr;
   }
   if(!State->Work.After(TEXT("NewObject_HLSLConversionJob"))||!Created)
   {
    State->Quarantine(TEXT("HLSLConversion Job allocation could not be safely adopted."));
    return nullptr;
   }
   State->Job.Reset(Created);CacheOwners.Add(Created,Created);
   if(!Parse(Text,State->Params))
   {
    State->Work.Fail(TEXT("niagara_hlsl_conversion_invalid_params_json"),TEXT("Finite unique-key JSON object within1Mi/depth64/262144 slots required."));
    State->Legacy=State->Work.Error(State->Work.R.Error);State->Phase=EPhase::Final;State->Finalize();
   }
   else
   {
    State->Work.R.bSaveRequested=true;State->Work.R.bCompileRequested=true;State->Params->TryGetBoolField(TEXT("save"),State->Work.R.bSaveRequested);State->Params->TryGetBoolField(TEXT("request_compile"),State->Work.R.bCompileRequested);
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
UCubelessNiagaraHlslConversionJob* StartReplace(const FString& ParamsJson){return Start(ParamsJson,TEXT("replace_niagara_custom_hlsl"));}
UCubelessNiagaraHlslConversionJob* StartRecipe(const FString& ParamsJson){return Start(ParamsJson,TEXT("apply_niagara_scratch_pad_graph_recipe"));}
FString SnapshotJson(const UCubelessNiagaraHlslConversionJob* Job)
{
 if(!IsInGameThread()||!Job)return FString();
 if(const FString* Snapshot=SnapshotCache.Find(Job))return *Snapshot;
 return FString();
}
bool Cancel(UCubelessNiagaraHlslConversionJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!Job||!ActiveJob.IsValid()||ActiveJob->RawJob!=Job)return false;
 auto State=ActiveJob;
 if(State->Phase==EPhase::Terminal||State->Phase==EPhase::Quarantined)return false;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();
 if(State->Executing)return true;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Cancellation lost HLSL conversion publication context; actual prefix retained."));return true;}
 {
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> CoreLease(Running,true);TGuardValue<FWork*> Context(Current,&State->Work);
  State->Work.Fail(TEXT("niagara_hlsl_conversion_async_cancelled"),State->Work.CancelReason);
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
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe HLSL conversion state without touching UObject publication."));return;}
 Cancel(State->RawJob,State->Work.CancelReason);
}
}
