// GW Editor-only Preview Lab. Shared native viewport capture, no RPC or transport bridge.
#include "Compatibility/UE58/CubelessNiagaraPreviewWorkflowApi.h"
#include "CubelessNiagaraPreviewWorkflowJob.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessViewportCaptureApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineGlobals.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "LevelEditorViewport.h"
#include "UnrealClient.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/Crc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"
#include "HAL/FileManager.h"

#include "Compatibility/UE58/CubelessNiagaraRuntimeInspectionApi.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "UObject/StrongObjectPtr.h"
#include "PreviewScene.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "UObject/GCObject.h"
#include "Widgets/SNullWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/ActorDragDropOp.h"
#include "AssetRegistry/AssetData.h"
#include "NiagaraParameterStore.h"

namespace Cubeless::UE58::NiagaraPreviewWorkflows
{
namespace Lab
{
using Variable::FResult;
constexpr const TCHAR* LabMap=TEXT("/Game/SampleTestMap/Niagara_TestMap");
constexpr const TCHAR* LabObject=TEXT("/Game/SampleTestMap/Niagara_TestMap.Niagara_TestMap");
constexpr const TCHAR* LabPrefix=TEXT("MCP_NiagaraPreviewLab_");
constexpr int32 MaxActors=65536,MaxOverrides=1024,MaxText=1024*1024;
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool SafeText(const FString& S,int32 Max){if(S.Len()>Max)return false;for(int32 I=0;I<S.Len();++I)if(S[I]==0)return false;return true;}
bool Finite(const FVector& V){return FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::IsFinite(V.Z);}
FString Encode(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
TArray<TSharedPtr<FJsonValue>> VJson(const FVector& V){return{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
TArray<TSharedPtr<FJsonValue>> Labels(const TArray<FString>& A){TArray<TSharedPtr<FJsonValue>> R;for(const auto& X:A)R.Add(MakeShared<FJsonValueString>(X));return R;}
bool FiniteJson(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Slots){if(!V.IsValid()||Depth>16||++Slots>65536)return false;if(V->Type==EJson::Number){double D;return V->TryGetNumber(D)&&FMath::IsFinite(D);}if(V->Type==EJson::String)return SafeText(V->AsString(),MaxText);if(V->Type==EJson::Array){for(auto X:V->AsArray())if(!FiniteJson(X,Depth+1,Slots))return false;}else if(V->Type==EJson::Object){for(const auto& KV:V->AsObject()->Values)if(!SafeText(FString(KV.Key),MaxText)||!FiniteJson(KV.Value,Depth+1,Slots))return false;}return V->Type!=EJson::None;}
struct FExactKeys:DefaultKeyFuncs<FString>{static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}};
bool Parse(const FString& S,TSharedPtr<FJsonObject>& O)
{
 if(!SafeText(S,MaxText)||S.IsEmpty())return false;auto R=TJsonReaderFactory<>::Create(TEXT("[")+S+TEXT("]"));TArray<bool> Objects;TArray<TSet<FString,FExactKeys>> Keys;EJsonNotation T;int32 Slots=0;
 while(R->ReadNext(T)){if(T==EJsonNotation::Error||++Slots>65536)return false;if(T!=EJsonNotation::ObjectEnd&&T!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()){FString K(R->GetIdentifier());if(Keys.Last().Contains(K))return false;Keys.Last().Add(K);}if(T==EJsonNotation::ObjectStart||T==EJsonNotation::ArrayStart){Objects.Add(T==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>16)return false;}else if(T==EJsonNotation::ObjectEnd||T==EJsonNotation::ArrayEnd){if(Objects.IsEmpty())return false;Objects.Pop();Keys.Pop();}}
 TArray<TSharedPtr<FJsonValue>> A;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+S+TEXT("]")),A)||A.Num()!=1||!A[0].IsValid()||A[0]->Type!=EJson::Object)return false;O=A[0]->AsObject();int32 Values=0;return O.IsValid()&&FiniteJson(A[0],0,Values);
}
struct FActorPin
{
 TWeakObjectPtr<AActor> Weak;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UObject> Outer;TWeakObjectPtr<UPackage> Package,EffectivePackage;FString Path,Label;bool Removed=false;
 explicit FActorPin(AActor* A):Weak(A),Class(A->GetClass()),Outer(A->GetOuter()),Package(A->GetOutermost()),EffectivePackage(A->GetPackage()),Path(A->GetPathName()),Label(A->GetActorLabel(false)){}
 bool Matches(UWorld* W)const{auto* A=Weak.Get();return !Removed&&Live(A)&&A->GetClass()==Class.Get()&&A->GetOuter()==Outer.Get()&&A->GetOutermost()==Package.Get()&&A->GetPackage()==EffectivePackage.Get()&&A->GetPathName()==Path&&A->GetWorld()==W&&A->GetActorLabel(false)==Label;}
};
struct FWork
{
 FResult R;TSharedRef<FJsonObject> L=MakeShared<FJsonObject>(),Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> P;FString Op,FirstStage;
 TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UObject> WorldOuter;TWeakObjectPtr<UPackage> WorldPackage,WorldEffectivePackage;TWeakObjectPtr<UClass> WorldClass;FString WorldPath;
 TFunction<bool()> ExtraGuard;
 TArray<FActorPin> Actors;bool Captured=false,ScopeLost=false,ExitCleanup=false;FLevelEditorViewportClient* Client=nullptr;FViewport* Viewport=nullptr;bool ViewCaptured=false;
 TWeakObjectPtr<ANiagaraActor> Spawned;TWeakObjectPtr<UNiagaraComponent> Component;TWeakObjectPtr<UNiagaraSystem> System;FString SystemPath;bool ComponentCaptured=false,AssetAssigned=false;int32 SpawnIdentityIndex=INDEX_NONE;FString ComponentPath;TWeakObjectPtr<UClass> ComponentClass;TWeakObjectPtr<UObject> SystemOuter;TWeakObjectPtr<UPackage> SystemPackage,SystemEffectivePackage,ComponentEffectivePackage,ComponentOutermost;bool SystemCaptured=false;
 explicit FWork(const TCHAR* O):Op(O){R.Compiled=false;R.Data->SetObjectField(TEXT("legacy_result"),L);R.Data->SetObjectField(TEXT("sdk_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_partial_state"),Prefix);L->SetBoolField(TEXT("success"),false);L->SetStringField(TEXT("preview_system"),TEXT("Niagara Preview Lab"));R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("game_runtime_dependency_added"),false);R.Data->SetBoolField(TEXT("server_RPC_dependency"),false);R.Data->SetStringField(TEXT("operation_dependency"),TEXT("EngineAPI; shared UE58::ViewportCapture native operation, no Toolset/RPC forwarding"));R.Data->SetBoolField(TEXT("unique_external_pipeline_complete"),false);R.Data->SetBoolField(TEXT("warmup_async_implemented"),true);R.Data->SetBoolField(TEXT("simulation_or_GPU_completion_verified"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);R.Data->SetBoolField(TEXT("map_was_reloaded"),false);R.Data->SetBoolField(TEXT("map_was_saved"),false);R.Data->SetBoolField(TEXT("PNG_file_write_is_asset_save"),false);R.Data->SetStringField(TEXT("scope"),TEXT("Exact captured Editor world/actor identities, component setters and viewport/file receipts. Label prefix is the legacy destructive selector, not ownership proof. No full SDK-global purity, shader/GPU simulation completion, atomic rollback or map save."));}
 void Count(const TCHAR* N){double V=0;Counts->TryGetNumberField(N,V);Counts->SetNumberField(N,V+1);}
 bool Fail(const TCHAR* Code,const FString& E){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=E;FirstStage=R.Stage;}R.bSuccess=false;L->SetBoolField(TEXT("success"),false);L->SetStringField(TEXT("error"),R.Error);return false;}
 bool Init(const FString& Text){if(!Parse(Text,P))return Fail(TEXT("niagara_preview_lab_params_invalid"),TEXT("One bounded finite JSON object with unique exact keys and no embedded NUL required"));if(!IsEditorGameThread()||IsEngineExitRequested()||GExitPurge||GIsSavingPackage||IsGarbageCollecting()||!Live(GEditor))return Fail(TEXT("niagara_preview_lab_editor_unsafe"),TEXT("Live Editor game thread outside GC/exit required"));if(IsPlaySessionActive())return Fail(TEXT("niagara_preview_lab_PIE_unsupported"),TEXT("Stop PIE/SIE before Preview Lab operations"));Editor=GEditor;UWorld* W=GEditor->GetEditorWorldContext().World();if(!Live(W)||W->bIsTearingDown||W->WorldType!=EWorldType::Editor||!Live(W->GetOutermost()))return Fail(TEXT("niagara_preview_lab_world_missing"),TEXT("Failed to get exact Editor world"));World=W;WorldPath=W->GetPathName();WorldOuter=W->GetOuter();WorldPackage=W->GetOutermost();WorldEffectivePackage=W->GetPackage();WorldClass=W->GetClass();Captured=true;R.bChangeStateKnown=false;Prefix->SetStringField(TEXT("world"),WorldPath);Prefix->SetBoolField(TEXT("map_dirty_before"),W->GetOutermost()->IsDirty());return Admit();}
 bool Admit(){if(ScopeLost)return false;if(!Captured||!IsEditorGameThread()||(!ExitCleanup&&IsEngineExitRequested())||GExitPurge||GIsSavingPackage||IsGarbageCollecting()||!Live(GEditor)||Editor.Get()!=GEditor||IsPlaySessionActive()){ScopeLost=true;return Fail(TEXT("niagara_preview_lab_context_lost"),TEXT("Editor/GC/exit/PIE context changed; no more getters, cleanup or writers"));}auto* W=World.Get();if(!Live(W)||W->bIsTearingDown||W->WorldType!=EWorldType::Editor||W->GetClass()!=WorldClass.Get()||W->GetPackage()!=WorldEffectivePackage.Get()||GEditor->GetEditorWorldContext().World()!=W||W->GetPathName()!=WorldPath||W->GetOuter()!=WorldOuter.Get()||W->GetOutermost()!=WorldPackage.Get()){ScopeLost=true;return Fail(TEXT("niagara_preview_lab_world_changed"),TEXT("Captured world identity changed; no replacement-world action"));}if(ViewCaptured){if(!Viewport||GEditor->GetActiveViewport()!=Viewport||!GEditor->GetLevelViewportClients().Contains(Client)||Client->Viewport!=Viewport||Viewport->GetClient()!=static_cast<FViewportClient*>(Client)||Client->GetWorld()!=W){ScopeLost=true;return Fail(TEXT("niagara_preview_lab_viewport_changed"),TEXT("Captured level viewport/client left its surface; restoration skipped"));}}if(SystemCaptured){auto* S=System.Get();if(!Live(S)||S->GetClass()!=UNiagaraSystem::StaticClass()||S->GetPathName()!=SystemPath||S->GetOuter()!=SystemOuter.Get()||S->GetOutermost()!=SystemPackage.Get()||S->GetPackage()!=SystemEffectivePackage.Get()){ScopeLost=true;return Fail(TEXT("niagara_preview_lab_system_changed"),TEXT("Captured source System identity changed"));}}if(ComponentCaptured){auto* A=Spawned.Get();auto* C=Component.Get();if(!Live(A)||!Live(C)||C->GetClass()!=ComponentClass.Get()||C->GetPathName()!=ComponentPath||C->GetPackage()!=ComponentEffectivePackage.Get()||C->GetOutermost()!=ComponentOutermost.Get()||!Actors.IsValidIndex(SpawnIdentityIndex)||!Actors[SpawnIdentityIndex].Matches(W)||(AssetAssigned&&C->GetAsset()!=System.Get())||A->GetWorld()!=W||A->GetClass()!=ANiagaraActor::StaticClass()||A->GetNiagaraComponent()!=C||C->GetOwner()!=A||C->GetOuter()!=A){ScopeLost=true;return Fail(TEXT("niagara_preview_lab_component_changed"),TEXT("Captured preview actor/component ownership changed"));}}if(ExtraGuard&&!ExtraGuard()){ScopeLost=true;return Fail(TEXT("niagara_preview_workflow_target_changed"),TEXT("Captured workflow target changed; no retarget or unsafe restore"));}return true;}
 template<class F>bool SDK(const TCHAR* Stage,bool Write,F&& Fn){R.Stage=Stage;if(!Admit())return false;R.bChangeStateKnown=false;if(Write)R.bMutationAttempted=true;Count(Stage);Fn();return Admit();}
 bool ActorAdmit(FActorPin& A){if(!Admit())return false;if(!A.Matches(World.Get())){ScopeLost=true;return Fail(TEXT("niagara_preview_lab_actor_changed"),TEXT("Exact selected actor class/outer/package/path/label changed; destructive follow-up skipped"));}return true;}
 bool Gather(TArray<int32>& Selected){if(!Admit())return false;int32 Counted=0;for(TActorIterator<AActor> It(World.Get());It;++It){if(++Counted>MaxActors)return Fail(TEXT("niagara_preview_lab_actor_limit"),TEXT("Actor scan bound exceeded before additional writes"));auto* A=*It;if(!Live(A))continue;FActorPin Probe(A);FString Label;if(!SDK(TEXT("GetActorLabel_legacy_lazy_getter"),false,[&]{Label=Probe.Weak->GetActorLabel();}))return false;bool LabelCreated=Probe.Label.IsEmpty()&&!Label.IsEmpty();Probe.Label=Label;if(!Probe.Matches(World.Get())){ScopeLost=true;return Fail(TEXT("niagara_preview_lab_actor_changed"),TEXT("Actor changed during legacy label getter"));}if(LabelCreated){R.bChanged=true;Count(TEXT("lazy_actor_labels_created"));}if(Label.StartsWith(LabPrefix)||Label.StartsWith(TEXT("MCP_NiagaraReview_"))){if(A->GetWorld()!=World.Get()||!Live(A->GetOuter())||!Live(A->GetOutermost()))return Fail(TEXT("niagara_preview_lab_actor_unresolved"),TEXT("Prefix actor lacks exact world/outer/package"));Selected.Add(Actors.Emplace(A));}}Prefix->SetNumberField(TEXT("actor_scan_count"),Counted);return Admit();}
 bool Bool(const TCHAR* Key,bool Default,bool& Out){Out=Default;if(!P->HasField(Key))return true;if(!P->TryGetBoolField(Key,Out))return Fail(TEXT("niagara_preview_lab_bool_invalid"),FString(Key)+TEXT(" must be a JSON bool"));return true;}
 bool Number(const TCHAR* Key,double Default,double& Out){Out=Default;if(!P->HasField(Key))return true;if(!P->TryGetNumberField(Key,Out)||!FMath::IsFinite(Out))return Fail(TEXT("niagara_preview_lab_number_invalid"),FString(Key)+TEXT(" must be finite numeric"));return true;}
 bool Vector(const TCHAR* Key,const TCHAR* Alias,bool& Present,FVector& Out,bool LegacyFallback=false){const TCHAR* K=P->HasField(Key)?Key:Alias;Present=K&&P->HasField(K);if(!Present)return true;const TArray<TSharedPtr<FJsonValue>>* A=nullptr;bool Shape=P->TryGetArrayField(K,A)&&A&&A->Num()>=3;double V[3]={0,0,0};if(Shape)for(int I=0;I<3;++I)if(!(*A)[I].IsValid()||!(*A)[I]->TryGetNumber(V[I])||!FMath::IsFinite(V[I])){Shape=false;break;}if(!Shape){if(LegacyFallback){Prefix->SetBoolField(FString(Key)+TEXT("_legacy_default_fallback"),true);return true;}return Fail(TEXT("niagara_preview_lab_vector_invalid"),FString(K)+TEXT(" requires at least3 finite numbers; original first3 precedence retained"));}for(int I=0;I<3;++I)if(FMath::Abs(V[I])>FLT_MAX)return Fail(TEXT("niagara_preview_lab_vector_range"),FString(K)+TEXT(" finite storage range exceeded"));Out=FVector(V[0],V[1],V[2]);return true;}
 bool Gate(bool& Allow){if(!Bool(TEXT("allow_current_map_preview"),false,Allow))return false;if(!Allow&&WorldPackage->GetName()!=LabMap)return Fail(TEXT("niagara_preview_lab_map_required"),FString(TEXT("Requires "))+LabObject+TEXT(" to be loaded, or explicit allow_current_map_preview. No map reload/save is performed."));return true;}
 bool Path(FString& Out){FString In;if(!P->TryGetStringField(TEXT("filepath"),In)||In.IsEmpty()||!SafeText(In,32768))return Fail(TEXT("niagara_preview_lab_filepath_invalid"),TEXT("Nonempty bounded filepath required"));FString Base=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("MCP/NiagaraReviews"));FPaths::NormalizeDirectoryName(Base);Out=FPaths::IsRelative(In)?Base/In:FPaths::ConvertRelativePathToFull(In);FPaths::NormalizeFilename(Out);if(!FPaths::CollapseRelativeDirectories(Out)||!FPaths::IsUnderDirectory(Out,Base))return Fail(TEXT("niagara_preview_lab_filepath_outside_root"),TEXT("Preview PNG must remain under Saved/MCP/NiagaraReviews"));if(!Out.EndsWith(TEXT(".png"),ESearchCase::IgnoreCase))Out+=TEXT(".png");Prefix->SetStringField(TEXT("filepath"),Out);Prefix->SetBoolField(TEXT("file_existed_before"),IFileManager::Get().FileExists(*Out));R.Data->SetStringField(TEXT("filesystem_scope"),TEXT("Lexical normalized containment plus exact file receipt; no OS reparse/symlink containment or atomic overwrite guarantee."));return true;}
 bool View(){if(!Admit())return false;Viewport=GEditor->GetActiveViewport();if(!Viewport)return Fail(TEXT("niagara_preview_lab_viewport_missing"),TEXT("No active level viewport"));if(GEditor->GetLevelViewportClients().Num()>65536)return Fail(TEXT("niagara_preview_lab_client_limit"),TEXT("Viewport client bound exceeded"));for(auto* C:GEditor->GetLevelViewportClients())if(C&&C->Viewport==Viewport&&static_cast<FViewportClient*>(C)==Viewport->GetClient()){Client=C;break;}if(!Client)return Fail(TEXT("niagara_preview_lab_level_viewport_required"),TEXT("Active viewport lacks exact public level-client membership"));ViewCaptured=true;return Admit();}
 void MapFields(){if(!Admit())return;L->SetStringField(TEXT("current_map_package"),WorldPackage->GetName());L->SetBoolField(TEXT("map_was_reloaded"),false);L->SetBoolField(TEXT("map_was_saved"),false);Prefix->SetBoolField(TEXT("map_dirty_after"),WorldPackage->IsDirty());}
 FString Finish(){const bool FinalCurrent=Captured&&!ScopeLost&&Admit();R.bSuccess=R.Error.IsEmpty();R.bPostconditionPassed=R.bSuccess;L->SetBoolField(TEXT("success"),R.bSuccess);if(R.bSuccess){R.Stage=TEXT("complete");R.bChangeStateKnown=true;}else{if(!FirstStage.IsEmpty())R.Stage=FirstStage;if(R.bMutationAttempted)R.bChangeStateKnown=false;bool LexicalPathRefusal=false;if(R.Data->TryGetBoolField(TEXT("prewriter_lexical_path_refusal_current_verified"),LexicalPathRefusal)&&LexicalPathRefusal){bool BeforeDirty=false;R.bChangeStateKnown=FinalCurrent&&!ScopeLost&&R.ErrorCode==TEXT("niagara_preview_lab_filepath_outside_root")&&Counts->Values.IsEmpty()&&Prefix->Values.Num()==2&&Prefix->HasField(TEXT("world"))&&Prefix->TryGetBoolField(TEXT("map_dirty_before"),BeforeDirty)&&!R.bMutationAttempted&&!R.bChanged&&!R.bCompileRequested&&!R.bCompileAttempted&&R.Compiled.IsSet()&&!R.Compiled.GetValue()&&!R.bSaveRequested&&!R.bSaveAttempted&&!R.bSaved&&!ViewCaptured&&!SystemCaptured&&!ComponentCaptured&&Actors.IsEmpty()&&WorldPackage->IsDirty()==BeforeDirty;R.Data->SetBoolField(TEXT("prewriter_lexical_path_refusal_final_current_verified"),R.bChangeStateKnown);}}R.Data->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);R.Data->SetBoolField(TEXT("mutation_state_unknown"),!R.bChangeStateKnown);R.Data->SetBoolField(TEXT("native_mutation_attempted"),R.bMutationAttempted);R.Data->SetBoolField(TEXT("scope_lost_cleanup_skipped"),ScopeLost);R.Data->SetBoolField(TEXT("full_legacy_contract_verified"),false);bool LexicalPathFinalCurrent=false;R.Data->TryGetBoolField(TEXT("prewriter_lexical_path_refusal_final_current_verified"),LexicalPathFinalCurrent);R.Data->SetStringField(TEXT("known_scope"),LexicalPathFinalCurrent&&R.bChangeStateKnown?TEXT("Exact single outside-root lexical refusal with final captured world identity/dirty current and no tracked target SDK, file probe, load, actor/viewport/component/cleanup/warmup/capture or file write: target/file unchanged. Transient job/world bootstrap and path-cache purity are not claimed."):TEXT("Successful exact named world/actor/component/captured viewport and bounded file receipts only; no exhaustive source/cache/GPU/runtime proof."));return Operations::EncodeVariableResult(R,*Op);}
};
bool CleanupActors(FWork& W,TArray<FString>& Deleted,const TArray<int32>* Only=nullptr)
{
 TArray<int32> Selected;if(Only)Selected=*Only;else if(!W.Gather(Selected))return false;TArray<TSharedPtr<FJsonValue>> Receipts;
 // Admit the entire prefix selection before the first writer; labels are legacy scope, not ownership.
 for(int32 I:Selected)if(!W.ActorAdmit(W.Actors[I]))return false;
 for(int32 I:Selected){auto& A=W.Actors[I];if(!W.ActorAdmit(A))return false;auto* Actor=A.Weak.Get();auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("actor"),A.Path);Row->SetStringField(TEXT("label"),A.Label);Receipts.Add(MakeShared<FJsonValueObject>(Row));W.Prefix->SetArrayField(TEXT("cleanup_receipts"),Receipts);
 if(auto* NA=Cast<ANiagaraActor>(Actor)){TWeakObjectPtr<UNiagaraComponent> C=NA->GetNiagaraComponent();auto Check=[&](){return W.ActorAdmit(A)&&Live(C.Get())&&NA->GetNiagaraComponent()==C.Get()&&C->GetOwner()==NA;};if(C.IsValid()){if(!Check())return W.Fail(TEXT("niagara_preview_lab_cleanup_component_changed"),TEXT("Cleanup component changed"));if(!W.SDK(TEXT("SetRenderingEnabled_false"),true,[&]{C->SetRenderingEnabled(false);})||!Check())return false;if(!W.SDK(TEXT("DeactivateImmediate"),true,[&]{C->DeactivateImmediate();})||!Check())return false;if(W.Component.Get()==C.Get())W.ComponentCaptured=false;if(!W.SDK(TEXT("SetAsset_null"),true,[&]{C->SetAsset(nullptr);})||!Check())return false;}}
 if(!W.ActorAdmit(A))return false;bool Returned=false;W.R.Stage=TEXT("DestroyActor");W.R.bMutationAttempted=true;W.R.bChangeStateKnown=false;W.Count(TEXT("DestroyActor"));Returned=Actor->Destroy();Row->SetBoolField(TEXT("SDK_destroy_returned"),Returned);A.Removed=Returned; // Do not dereference Actor/NA/component after Destroy.
 if(!W.Admit())return false;bool Residual=false;int32 Scanned=0;for(TActorIterator<AActor> It(W.World.Get());It;++It){if(++Scanned>MaxActors)return W.Fail(TEXT("niagara_preview_lab_actor_limit"),TEXT("Post-destroy scan exceeded bound"));if(*It==A.Weak.Get()){Residual=true;break;}}Row->SetBoolField(TEXT("live_residual"),Residual);if(!Returned||Residual)return W.Fail(TEXT("niagara_preview_lab_destroy_incomplete"),TEXT("Destroy refused or selected actor remains; no cleanup replay"));W.R.bChanged=true;Deleted.Add(A.Label);Row->SetBoolField(TEXT("destroy_verified"),true);}
 return W.Admit();
}
bool Capture(FWork& W)
{
 FString File;if(!W.Path(File))return false;bool Allow=false,Auto=true;if(!W.Gate(Allow)||!W.Bool(TEXT("auto_frame"),true,Auto)||!W.View())return false;double ViewNumber;if(!W.Number(TEXT("view"),1,ViewNumber)||ViewNumber<MIN_int32||ViewNumber>MAX_int32)return W.Fail(TEXT("niagara_preview_lab_view_range"),TEXT("view exceeds int32"));int32 View=FMath::Clamp(static_cast<int32>(ViewNumber),1,3);bool HasLoc=false,HasRot=false;FVector Loc,Rot;if(!W.Vector(TEXT("camera_location"),TEXT("view_location"),HasLoc,Loc)||!W.Vector(TEXT("camera_rotation"),TEXT("view_rotation"),HasRot,Rot))return false;const TCHAR* FovKey=W.P->HasField(TEXT("camera_fov"))?TEXT("camera_fov"):TEXT("view_fov");bool HasFov=W.P->HasField(FovKey);double Fov=0;if(HasFov&&!W.Number(FovKey,90,Fov))return false;Fov=FMath::Clamp(Fov,1.0,179.0);const bool Override=HasLoc||HasRot||HasFov;
 FVector Center=FVector::ZeroVector;double Radius=0;TArray<FString> FrameLabels;bool Framed=false;if(Auto&&!Override){TArray<int32> A;if(!W.Gather(A))return false;FBox Combined(ForceInit);for(int32 I:A){auto& Pin=W.Actors[I];if(!W.ActorAdmit(Pin))return false;FBox Bounds(ForceInit);if(!W.SDK(TEXT("GetComponentsBoundingBox"),false,[&]{Bounds=Pin.Weak->GetComponentsBoundingBox(true);})||!W.ActorAdmit(Pin))return false;if(!Bounds.IsValid||!Finite(Bounds.Min)||!Finite(Bounds.Max)||Bounds.GetExtent().Size()<1||Bounds.GetExtent().Size()>3000){FVector P=Pin.Weak->GetActorLocation();if(!Finite(P))return W.Fail(TEXT("niagara_preview_lab_bounds_nonfinite"),TEXT("Actor fallback location is nonfinite"));Bounds=FBox(P-FVector(180),P+FVector(180));}Combined+=Bounds;FrameLabels.Add(Pin.Label);}if(Combined.IsValid){Center=Combined.GetCenter();Radius=FMath::Clamp(Combined.GetExtent().Size(),120.0,1400.0);double Dist=FMath::Clamp(Radius*(View==1?2.25:View==2?3.1:4.2),360.0,3600.0),Height=FMath::Clamp(Radius*0.22,20.0,280.0);Loc=Center+FVector(0,-Dist,Height);auto R=(Center-Loc).Rotation();Rot=FVector(R.Pitch,R.Yaw,R.Roll);Framed=true;}}
 if(!Framed){if(!HasLoc)Loc=View==1?FVector(0,-420,180):View==2?FVector(0,-720,260):FVector(0,-1150,380);if(!HasRot)Rot=FVector(-8,90,0);}if(!Finite(Loc)||!Finite(Rot))return W.Fail(TEXT("niagara_preview_lab_camera_nonfinite"),TEXT("Resolved camera is nonfinite"));float PreviousFov=W.Client->ViewFOV,PreviousFovAngle=W.Client->FOVAngle;bool Particles=W.Client->EngineShowFlags.Particles;if(!FMath::IsFinite(PreviousFov)||!FMath::IsFinite(PreviousFovAngle))return W.Fail(TEXT("niagara_preview_lab_previous_FOV_invalid"),TEXT("Previous FOV cannot be restored safely"));
 auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("filepath"),File);Q->SetBoolField(TEXT("clean_game_view"),true);Q->SetBoolField(TEXT("restore_view_after_capture"),false);Q->SetNumberField(TEXT("redraw_count"),0);Q->SetArrayField(TEXT("view_location"),VJson(Loc));Q->SetArrayField(TEXT("view_rotation"),VJson(Rot));if(HasFov)Q->SetNumberField(TEXT("view_fov"),Fov);
 if(!W.SDK(TEXT("show_particles_for_capture"),true,[&]{W.Client->EngineShowFlags.SetParticles(true);}))return false;FResult Child=ViewportCapture::CaptureBookmark(Encode(Q));W.R.bMutationAttempted|=Child.bMutationAttempted;W.R.bChanged|=Child.bChanged;W.Prefix->SetObjectField(TEXT("capture_native_receipt"),Child.Data);W.Prefix->SetBoolField(TEXT("capture_returned_success"),Child.bSuccess);W.Prefix->SetBoolField(TEXT("PNG_saved_not_package_save"),Child.bSuccess);
 // Restore only original transient flags/FOV, never camera or a replacement surface.
 if(!W.Admit()){W.Prefix->SetBoolField(TEXT("parent_flag_restore_skipped"),true);return false;}if(!W.SDK(TEXT("restore_particles_and_explicit_FOV"),true,[&]{W.Client->EngineShowFlags.SetParticles(Particles);if(HasFov){W.Client->ViewFOV=PreviousFov;W.Client->FOVAngle=PreviousFovAngle;}}))return false;bool Restored=W.Client->EngineShowFlags.Particles==Particles&&(!HasFov||(W.Client->ViewFOV==PreviousFov&&W.Client->FOVAngle==PreviousFovAngle));W.Prefix->SetBoolField(TEXT("particles_and_FOV_restored"),Restored);if(!Restored)return W.Fail(TEXT("niagara_preview_lab_flag_restore_failed"),TEXT("Capture flags/FOV failed exact readback"));if(!Child.bSuccess)return W.Fail(*Child.ErrorCode,Child.Error);
 auto CL=Child.Data->GetObjectField(TEXT("legacy_result"));W.L->SetStringField(TEXT("filepath"),File);W.L->SetNumberField(TEXT("width"),CL->GetNumberField(TEXT("width")));W.L->SetNumberField(TEXT("height"),CL->GetNumberField(TEXT("height")));W.L->SetNumberField(TEXT("view"),View);W.L->SetBoolField(TEXT("clean_game_view"),true);W.L->SetBoolField(TEXT("used_expected_preview_lab_map"),W.WorldPackage->GetName()==LabMap);W.L->SetBoolField(TEXT("allow_current_map_preview"),Allow);W.L->SetBoolField(TEXT("auto_frame"),Auto);W.L->SetBoolField(TEXT("camera_override_requested"),Override);W.L->SetStringField(TEXT("camera_mode"),Override?TEXT("manual_camera_override"):Framed?TEXT("auto_preview_actor_frame"):TEXT("fixed_view_fallback"));if(HasFov)W.L->SetNumberField(TEXT("camera_fov"),Fov);if(Framed){W.L->SetNumberField(TEXT("frame_radius"),Radius);W.L->SetArrayField(TEXT("frame_actor_labels"),Labels(FrameLabels));}W.L->SetArrayField(TEXT("camera_location"),VJson(Loc));W.L->SetArrayField(TEXT("camera_rotation"),VJson(Rot));if(Framed)W.L->SetArrayField(TEXT("frame_center"),VJson(Center));W.L->SetStringField(TEXT("safety_note"),TEXT("Current Editor map only; no reload/save. Camera remains as captured; clean flags/particles/explicit FOV restored on the captured viewport. PNG ReadPixels/compression/file write remain bounded synchronous SDK work, not GPU simulation completion."));W.MapFields();return W.Admit();
}

bool ApplyOverrides(FWork& W,TArray<TSharedPtr<FJsonValue>>& Applied)
{
 if(!W.P->HasField(TEXT("user_parameter_overrides")))return true;const TSharedPtr<FJsonObject>* Obj=nullptr;if(!W.P->TryGetObjectField(TEXT("user_parameter_overrides"),Obj)||!Obj||!Obj->IsValid()||(*Obj)->Values.Num()>MaxOverrides)return W.Fail(TEXT("niagara_preview_lab_overrides_invalid"),TEXT("Bounded object of user_parameter_overrides required"));
 TArray<TSharedPtr<FJsonValue>> Attempts;
 for(const auto& KV:(*Obj)->Values){FString RawKey(KV.Key);FString Raw=RawKey.TrimStartAndEnd();FString Name=Raw;Name.TrimQuotesInline();if(!Name.Contains(TEXT(".")))Name=TEXT("User.")+Name;if(Name.IsEmpty()||Name==TEXT("User.")||!SafeText(Raw,4096))return W.Fail(TEXT("niagara_preview_lab_override_name_invalid"),TEXT("Empty or oversized user override name"));TArray<FName> Names;if(!Raw.IsEmpty())Names.AddUnique(FName(*Raw));Names.AddUnique(FName(*Name));FString Type;auto V=KV.Value;if(V.IsValid()&&V->Type==EJson::Object){auto O=V->AsObject();O->TryGetStringField(TEXT("type"),Type);auto Value=O->TryGetField(TEXT("value"));if(Value.IsValid())V=Value;}Type=Type.TrimStartAndEnd().ToLower();if(!V.IsValid())return W.Fail(TEXT("niagara_preview_lab_override_missing"),TEXT("Missing override value"));
 enum class EKind{Bool,Int,Float,Vec2,Vec3,Color};EKind Kind;TArray<double> N;bool B=false;int32 IV=0;
 auto Numeric=[&](const TSharedPtr<FJsonValue>& X,double& D){return X.IsValid()&&X->TryGetNumber(D)&&FMath::IsFinite(D)&&FMath::Abs(D)<=FLT_MAX;};
 if(Type==TEXT("bool")||V->Type==EJson::Boolean){if(!V->TryGetBool(B))return W.Fail(TEXT("niagara_preview_lab_override_bool_invalid"),TEXT("Typed bool requires JSON Boolean"));Kind=EKind::Bool;Type=TEXT("bool");}
 else if(Type==TEXT("int")||Type==TEXT("int32")||Type==TEXT("integer")){double D;if(!Numeric(V,D)||D<MIN_int32||D>MAX_int32)return W.Fail(TEXT("niagara_preview_lab_override_int_range"),TEXT("Finite int32 override required before RoundToInt"));IV=FMath::RoundToInt(D);Kind=EKind::Int;Type=TEXT("int");}
 else if(Type==TEXT("float")||Type==TEXT("double")||V->Type==EJson::Number){double D;if(!Numeric(V,D))return W.Fail(TEXT("niagara_preview_lab_override_float_range"),TEXT("Finite float range override required"));N.Add(D);Kind=EKind::Float;Type=TEXT("float");}
 else if(V->Type==EJson::Array){auto A=V->AsArray();for(auto X:A){double D;if(!Numeric(X,D))return W.Fail(TEXT("niagara_preview_lab_override_array_invalid"),TEXT("Override array requires finite float-range numbers"));N.Add(D);}if(N.Num()==2||Type==TEXT("vec2")||Type==TEXT("vector2d")){if(N.Num()!=2)return W.Fail(TEXT("niagara_preview_lab_override_vec2_shape"),TEXT("vec2 needs2 numbers"));Kind=EKind::Vec2;Type=TEXT("vec2");}else if(N.Num()==3||Type==TEXT("vec3")||Type==TEXT("vector")||Type==TEXT("vector3")){if(N.Num()!=3)return W.Fail(TEXT("niagara_preview_lab_override_vec3_shape"),TEXT("vec3 needs3 numbers"));Kind=EKind::Vec3;Type=TEXT("vec3");}else if(N.Num()==4||Type==TEXT("color")||Type==TEXT("linearcolor")||Type==TEXT("linear_color")){if(N.Num()!=4)return W.Fail(TEXT("niagara_preview_lab_override_color_shape"),TEXT("color needs4 numbers"));Kind=EKind::Color;Type=TEXT("linear_color");}else return W.Fail(TEXT("niagara_preview_lab_override_array_shape"),TEXT("Supported array lengths2/3/4 only"));}
 else if(V->Type==EJson::Object){auto O=V->AsObject();bool Vec=O->HasField(TEXT("x"))&&O->HasField(TEXT("y"))&&O->HasField(TEXT("z"));TArray<FString> Keys=Vec?TArray<FString>{TEXT("x"),TEXT("y"),TEXT("z")}:TArray<FString>{TEXT("r"),TEXT("g"),TEXT("b"),TEXT("a")};for(const auto& K:Keys){double D=1;if(K!=TEXT("a")||O->HasField(K)){if(!Numeric(O->TryGetField(K),D))return W.Fail(TEXT("niagara_preview_lab_override_object_invalid"),TEXT("Object override requires x/y/z or r/g/b[/a] finite fields"));}N.Add(D);}Kind=Vec?EKind::Vec3:EKind::Color;Type=Vec?TEXT("vec3"):TEXT("linear_color");}
 else return W.Fail(TEXT("niagara_preview_lab_override_unsupported"),TEXT("Supported bool/number/vector/color or typed {type,value} required"));
 auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_name"),RawKey);Row->SetStringField(TEXT("parameter_name"),Name);Row->SetStringField(TEXT("type"),Type);TArray<TSharedPtr<FJsonValue>> NameRows;Row->SetBoolField(TEXT("component_only"),true);Row->SetBoolField(TEXT("readback_verified"),false);Attempts.Add(MakeShared<FJsonValueObject>(Row));W.Prefix->SetArrayField(TEXT("override_attempts"),Attempts);
 for(FName Key:Names){auto NR=MakeShared<FJsonObject>();NR->SetStringField(TEXT("name"),Key.ToString());NR->SetBoolField(TEXT("setter_attempted"),true);NameRows.Add(MakeShared<FJsonValueObject>(NR));Row->SetArrayField(TEXT("variable_receipts"),NameRows);if(!W.SDK(TEXT("component_override_set"),true,[&]{auto* C=W.Component.Get();switch(Kind){case EKind::Bool:C->SetVariableBool(Key,B);break;case EKind::Int:C->SetVariableInt(Key,IV);break;case EKind::Float:C->SetVariableFloat(Key,static_cast<float>(N[0]));break;case EKind::Vec2:C->SetVariableVec2(Key,FVector2D(N[0],N[1]));break;case EKind::Vec3:C->SetVariableVec3(Key,FVector(N[0],N[1],N[2]));break;case EKind::Color:C->SetVariableLinearColor(Key,FLinearColor(N[0],N[1],N[2],N[3]));break;}}))return false;NR->SetBoolField(TEXT("setter_returned"),true);W.R.bChanged=true;bool Valid=false,Equal=false;if(!W.SDK(TEXT("component_override_readback"),false,[&]{auto* C=W.Component.Get();switch(Kind){case EKind::Bool:Equal=C->GetVariableBool(Key,Valid)==B;break;case EKind::Int:Equal=C->GetVariableInt(Key,Valid)==IV;break;case EKind::Float:Equal=C->GetVariableFloat(Key,Valid)==static_cast<float>(N[0]);break;case EKind::Vec2:Equal=C->GetVariableVec2(Key,Valid).Equals(FVector2D(static_cast<float>(N[0]),static_cast<float>(N[1])));break;case EKind::Vec3:Equal=C->GetVariableVec3(Key,Valid).Equals(FVector(static_cast<float>(N[0]),static_cast<float>(N[1]),static_cast<float>(N[2])));break;case EKind::Color:Equal=C->GetVariableColor(Key,Valid).Equals(FLinearColor(N[0],N[1],N[2],N[3]));break;}}))return false;NR->SetBoolField(TEXT("readback_verified"),Valid&&Equal);if(!Valid||!Equal)return W.Fail(TEXT("niagara_preview_lab_override_readback_failed"),TEXT("Component override setter returned but typed readback differs; partial prefix retained"));}
 Row->SetBoolField(TEXT("readback_verified"),true);TArray<TSharedPtr<FJsonValue>> AN;for(FName Key:Names)AN.Add(MakeShared<FJsonValueString>(Key.ToString()));Row->SetArrayField(TEXT("applied_variable_names"),AN);if(Kind==EKind::Bool)Row->SetBoolField(TEXT("value"),B);else if(Kind==EKind::Int)Row->SetNumberField(TEXT("value"),IV);else if(Kind==EKind::Float)Row->SetNumberField(TEXT("value"),static_cast<float>(N[0]));else{TArray<TSharedPtr<FJsonValue>> A;for(double D:N)A.Add(MakeShared<FJsonValueNumber>(D));Row->SetArrayField(TEXT("value"),A);}Applied.Add(MakeShared<FJsonValueObject>(Row));W.Prefix->SetArrayField(TEXT("applied_user_parameter_overrides"),Applied);
 }return W.Admit();
}

} // namespace Lab

namespace
{
using Lab::Live;using Lab::SafeText;using Lab::Encode;using Lab::VJson;
bool GZExecuting=false,GZStopping=false,GZNormalEditorExit=false;uint64 PlayerEpoch=0;FDelegateHandle EditorPreExitHandle;
bool EngineContextSafe(bool AllowExit){return IsEditorGameThread()&&(AllowExit||!IsEngineExitRequested())&&!GExitPurge&&!GIsSavingPackage&&!IsGarbageCollecting()&&Live(GEditor)&&GEngine==GEditor&&!IsPlaySessionActive();}
bool GlobalsSafe(){return EngineContextSafe(false);}
bool OwnedCleanupSafe(){return EngineContextSafe(GZNormalEditorExit);}
void EnsureEditorExitHook(){if(!EditorPreExitHandle.IsValid())EditorPreExitHandle=FEditorDelegates::OnEditorPreExit.AddLambda([]{TGuardValue<bool> Scope(GZNormalEditorExit,true);Shutdown(TEXT("editor_pre_exit"));});}
struct FIdentity
{
 TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Outermost;FString Path;
 FIdentity()=default;explicit FIdentity(UObject* O):Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetPackage()),Outermost(O->GetOutermost()),Path(O->GetPathName()){}
 bool Same()const{UObject* O=Object.Get();return Live(O)&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&O->GetPackage()==Package.Get()&&O->GetOutermost()==Outermost.Get()&&O->GetPathName()==Path;}
};
struct FPlayerOwner;
class SWorkflowViewport;
class FWorkflowViewportClient:public FEditorViewportClient
{
public:
 TWeakPtr<FPlayerOwner> Owner;
 FWorkflowViewportClient(FPreviewScene* Scene,const TWeakPtr<SEditorViewport>& Widget):FEditorViewportClient(nullptr,Scene,Widget){SetRealtime(false);SetViewMode(VMI_Lit);SetViewLocation(FVector(-360,-520,240));SetViewRotation(FRotator(-18,38,0));bSetListenerPosition=false;EngineShowFlags.SetParticles(true);EngineShowFlags.SetGrid(false);}
 virtual void Tick(float DeltaSeconds)override;
};
struct FPlayerOwner:TSharedFromThis<FPlayerOwner>
{
 TUniquePtr<FPreviewScene> Scene;TSharedPtr<SWindow> Window;TSharedPtr<SWorkflowViewport> Widget;TSharedPtr<FWorkflowViewportClient> Client;
 TStrongObjectPtr<UNiagaraComponent> ComponentHold;TStrongObjectPtr<UNiagaraSystem> SystemHold;
 UNiagaraComponent* Component=nullptr;UNiagaraSystem* System=nullptr;FIdentity ComponentId,SystemId;TWeakObjectPtr<UWorld> PreviewWorld,EditorWorld;TWeakObjectPtr<UEditorEngine> Editor;uint64 Epoch=0;
 bool Closed=false,Lost=false,Settling=false,Playing=false,Looping=true,Renderable=false,Disposed=false,TeardownAbandoned=false;int32 DisposeStage=0;int32 DropCount=0;FString DropKind,DisplayName,ObjectPath,ClassName,Playback=TEXT("none"),Analysis=TEXT("Analysis\nNo Niagara system loaded."),LastError;
 ~FPlayerOwner(){// Destructor must never opportunistically run PreviewScene teardown during GC/exit.
  if(Scene&&!Disposed)(void)Scene.Release();
 }
 bool Same()const
 {
  if(Closed||Lost||GZStopping||!GlobalsSafe()||Editor.Get()!=GEditor||!Live(EditorWorld.Get())||EditorWorld->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=EditorWorld.Get())return false;
  UWorld* PW=PreviewWorld.Get();if(!Scene||!Live(PW)||PW->bIsTearingDown||Scene->GetWorld()!=PW)return false;
  if(Component&&(!ComponentId.Same()||!SystemId.Same()||Component->GetAsset()!=System||Component->GetWorld()!=PW||!Component->GetForceSolo()))return false;return true;
 }
 template<class F>bool Call(const TCHAR* Stage,F&& Fn){if(!Same()){Lost=true;LastError=FString(Stage)+TEXT(": preview UI/context identity unavailable");return false;}Fn();if(!Same()){Lost=true;LastError=FString(Stage)+TEXT(": callback changed preview UI/context");return false;}return true;}
 void Frame(){if(!Component||!Client||!Same())return;FBoxSphereBounds B(ForceInit);if(!Call(TEXT("Preview.UpdateBounds"),[&]{Component->UpdateBounds();}))return;B=Component->Bounds;if(!Lab::Finite(B.Origin)||!Lab::Finite(B.BoxExtent))return;double Radius=FMath::Clamp(B.BoxExtent.Size(),120.0,1800.0);FVector Direction=FVector(-.56,-.74,.36).GetSafeNormal();if(!Call(TEXT("Preview.Frame"),[&]{Client->SetViewLocation(B.Origin+Direction*Radius*2.6);Client->SetViewRotation((-Direction).Rotation());Client->SetLookAtLocation(B.Origin);Client->Invalidate();}))return;}
 void Toggle(){if(Settling||!Component)return;bool Next=!Playing;if(Next){if(!Call(TEXT("Preview.Activate"),[&]{Component->Activate(false);}))return;}if(!Call(TEXT("Preview.Pause"),[&]{Component->SetPaused(!Next);}))return;Playing=Next;Playback=Playing?TEXT("playing"):TEXT("paused");if(Client)Client->SetRealtime(Playing);}
 void Loop(bool B){if(Same()){Looping=B;}}
bool TeardownSame(bool Detached=false)const
 {
  if(!OwnedCleanupSafe()||Editor.Get()!=GEditor||!Live(EditorWorld.Get())||EditorWorld->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=EditorWorld.Get())return false;
  UWorld* PW=PreviewWorld.Get();if(!Scene||!Live(PW)||PW->bIsTearingDown||Scene->GetWorld()!=PW)return false;
  if(Component){if(!ComponentId.Same()||!SystemId.Same()||Component->GetAsset()!=System)return false;UWorld* CW=Component->GetWorld();if(Detached){if(CW&&CW!=PW)return false;if(Component->IsRegistered())return false;}else if(CW!=PW)return false;}return true;
 }
 bool Dispose()
 {
  Closed=true;Playing=false;Settling=false;if(Disposed)return true;if(TeardownAbandoned)return false;
  auto Abandon=[&](){TeardownAbandoned=true;LastError=TEXT("Owned preview teardown exact identity/context unavailable; no replay or foreign-owner cleanup");return false;};
  if(!TeardownSame(DisposeStage>=2))return Abandon();if(Client)Client->SetRealtime(false);
  if(Component&&DisposeStage==0){Component->DeactivateImmediate();DisposeStage=1;if(!TeardownSame())return Abandon();}
  if(Component&&DisposeStage==1){if(!TeardownSame())return Abandon();Scene->RemoveComponent(Component);DisposeStage=2;if(!TeardownSame(true))return Abandon();}
  if(!TeardownSame(DisposeStage>=2))return Abandon();Component=nullptr;System=nullptr;ComponentHold.Reset();SystemHold.Reset();
  if(!OwnedCleanupSafe()||Editor.Get()!=GEditor)return Abandon();if(Window){Window->SetOnWindowClosed(FOnWindowClosed());Window->SetContent(SNullWidget::NullWidget);}if(!OwnedCleanupSafe()||Editor.Get()!=GEditor)return Abandon();Widget.Reset();Client.Reset();Window.Reset();
  if(!OwnedCleanupSafe()||Editor.Get()!=GEditor)return Abandon();Scene.Reset();Disposed=true;return true;
 }
 TSharedRef<FJsonObject> Metadata()const{auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),true);O->SetBoolField(TEXT("window_open"),!Closed&&Window.IsValid());O->SetStringField(TEXT("player_mode"),TEXT("Niagara Preview Player"));O->SetBoolField(TEXT("last_preview_renderable"),Renderable);O->SetNumberField(TEXT("drop_count"),DropCount);O->SetStringField(TEXT("last_drop_kind"),DropKind);O->SetStringField(TEXT("last_display_name"),DisplayName);O->SetStringField(TEXT("last_object_path"),ObjectPath);O->SetStringField(TEXT("last_class_name"),ClassName);O->SetStringField(TEXT("playback_state"),Playback);O->SetBoolField(TEXT("looping"),Looping);O->SetStringField(TEXT("analysis_summary"),Analysis.Left(4096));O->SetBoolField(TEXT("simulation_or_GPU_completion_verified"),false);O->SetBoolField(TEXT("analysis_legacy_parity_complete"),false);return O;}
};
TSharedPtr<FPlayerOwner> Player;TArray<TSharedPtr<FPlayerOwner>> RetiredPlayers;FTSTicker::FDelegateHandle RetireHandle;
void DrainRetiredPlayers();
TSharedRef<FJsonObject> LastPlayerState=MakeShared<FJsonObject>();
TArray<TSharedPtr<FPlayerOwner>>* UnresolvedOwnedUI=new TArray<TSharedPtr<FPlayerOwner>>();
void RetirePlayer(const TSharedPtr<FPlayerOwner>& P)
{
 if(!P)return;P->Closed=true;P->Playing=false;P->Settling=false;++PlayerEpoch;LastPlayerState=P->Metadata();RetiredPlayers.AddUnique(P);if(Player==P)Player.Reset();
 if(!GZStopping&&!RetireHandle.IsValid())RetireHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){if(GZExecuting||!GlobalsSafe())return true;TGuardValue<bool> Busy(GZExecuting,true);DrainRetiredPlayers();if(RetiredPlayers.IsEmpty()){RetireHandle.Reset();return false;}return true;}));
}
void DrainRetiredPlayers()
{
 // No pointer reset during unsafe exit: PreviewScene destruction itself touches UObjects.
 if(!OwnedCleanupSafe())return;for(int32 I=RetiredPlayers.Num()-1;I>=0;--I){auto P=RetiredPlayers[I];if(P->Dispose())RetiredPlayers.RemoveAt(I);else if(P->TeardownAbandoned){UnresolvedOwnedUI->Add(P);LastPlayerState->SetBoolField(TEXT("owned_UI_teardown_unresolved"),true);LastPlayerState->SetStringField(TEXT("teardown_error"),P->LastError);LastPlayerState->SetBoolField(TEXT("DLL_hot_unload_safety_proven"),false);RetiredPlayers.RemoveAt(I);}}
}
class SWorkflowViewport:public SEditorViewport
{
public:
 SLATE_BEGIN_ARGS(SWorkflowViewport){}SLATE_END_ARGS()
 TWeakPtr<FPlayerOwner> Owner;
 void Construct(const FArguments&,TSharedRef<FPlayerOwner> In){Owner=In;SEditorViewport::Construct(SEditorViewport::FArguments());}
 virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient()override{auto P=Owner.Pin();P->Client=MakeShared<FWorkflowViewportClient>(P->Scene.Get(),SharedThis(this));P->Client->Owner=P;return P->Client.ToSharedRef();}
 virtual FReply OnDrop(const FGeometry&,const FDragDropEvent& Event)override;
 virtual void OnDragEnter(const FGeometry&,const FDragDropEvent&)override{}
};
void FWorkflowViewportClient::Tick(float DeltaSeconds)
{
 auto P=Owner.Pin();if(!P||GZExecuting||!P->Same()||P->Settling||!P->Playing)return;
 TGuardValue<bool> Busy(GZExecuting,true);FEditorViewportClient::Tick(DeltaSeconds);if(!P->Same()){P->Lost=true;return;}
 if(!P->Call(TEXT("PreviewWorld.Tick"),[&]{P->PreviewWorld->Tick(LEVELTICK_All,FMath::Clamp(DeltaSeconds,0.f,.25f));}))return;
 bool Complete=false;if(P->Component&&!P->Call(TEXT("Preview.IsComplete"),[&]{Complete=P->Component->IsComplete();}))return;
 if(Complete){if(P->Looping){if(!P->Call(TEXT("Preview.ResetLoop"),[&]{P->Component->ResetSystem();}))return;}else{if(!P->Call(TEXT("Preview.Stop"),[&]{P->Component->SetPaused(true);}))return;P->Playing=false;P->Playback=TEXT("stopped");SetRealtime(false);}}
}
FReply SWorkflowViewport::OnDrop(const FGeometry&,const FDragDropEvent& E)
{
 auto P=Owner.Pin();if(!P||!P->Same()||P->Settling||GZExecuting)return FReply::Unhandled();FString Path;
 if(auto Assets=E.GetOperationAs<FAssetDragDropOp>()){if(!Assets->GetAssets().IsEmpty())Path=Assets->GetAssets()[0].GetObjectPathString();}
 else if(auto Actors=E.GetOperationAs<FActorDragDropOp>()){if(!Actors->Actors.IsEmpty()){AActor* A=Actors->Actors[0].Get();if(Live(A)){TInlineComponentArray<UNiagaraComponent*> Cs(A);for(UNiagaraComponent* C:Cs)if(Live(C)&&Live(C->GetAsset())){Path=C->GetAsset()->GetPathName();break;}}}}
 if(Path.IsEmpty())return FReply::Unhandled();auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("system_path"),Path);StartOpenPlayer(Encode(Q));return FReply::Handled();
}
bool ShowPlayer(Lab::FWork& W)
{
 if(!W.Admit()||!FSlateApplication::IsInitialized())return W.Fail(TEXT("niagara_preview_player_slate_unavailable"),TEXT("Slate initialized UI required; no fake opened success"));
 EnsureEditorExitHook();DrainRetiredPlayers();if(Player&&Player->Same()){auto P=Player;if(!W.SDK(TEXT("PresentPlayerWindow"),true,[&]{P->Window->BringToFront();}))return false;return P->Same();}
 if(Player)RetirePlayer(Player);auto P=MakeShared<FPlayerOwner>();P->Editor=GEditor;P->EditorWorld=W.World;P->Epoch=++PlayerEpoch;
 if(!W.SDK(TEXT("NewPreviewScene"),true,[&]{P->Scene=MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues().SetTransactional(false).AllowAudioPlayback(false));W.Prefix->SetBoolField(TEXT("owned_preview_scene_created"),P->Scene.IsValid());})){RetiredPlayers.Add(P);return false;}P->PreviewWorld=P->Scene->GetWorld();if(!P->Same()){RetiredPlayers.Add(P);return W.Fail(TEXT("niagara_preview_player_scene_invalid"),TEXT("Owned PreviewScene context unavailable"));}
 SAssignNew(P->Widget,SWorkflowViewport,P);TWeakPtr<FPlayerOwner> Weak=P;
 SAssignNew(P->Window,SWindow).Title(FText::FromString(TEXT("Niagara Preview Player"))).ClientSize(FVector2D(900,620)).SupportsMaximize(true)
 [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
 +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Play / Pause"))).OnClicked_Lambda([Weak]{auto O=Weak.Pin();if(O&&!GZExecuting){TGuardValue<bool>B(GZExecuting,true);O->Toggle();}return FReply::Handled();})]
 +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Frame"))).OnClicked_Lambda([Weak]{auto O=Weak.Pin();if(O&&!GZExecuting){TGuardValue<bool>B(GZExecuting,true);O->Frame();}return FReply::Handled();})]
 +SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsChecked_Lambda([Weak]{auto O=Weak.Pin();return O&&O->Looping?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([Weak](ECheckBoxState S){if(auto O=Weak.Pin())O->Loop(S==ECheckBoxState::Checked);})[SNew(STextBlock).Text(FText::FromString(TEXT("Loop")))]]]
 +SVerticalBox::Slot().FillHeight(1)[P->Widget.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([Weak]{auto O=Weak.Pin();return FText::FromString(O?O->Analysis.Left(4096):TEXT("Closed"));})]];
 P->Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([Weak](const TSharedRef<SWindow>&){if(auto O=Weak.Pin())RetirePlayer(O);}));Player=P;
 if(!W.SDK(TEXT("Slate.AddWindow"),true,[&]{FSlateApplication::Get().AddWindow(P->Window.ToSharedRef());W.Prefix->SetBoolField(TEXT("window_add_returned"),true);})||!P->Same()){RetirePlayer(P);return false;}W.R.bChanged=true;return true;
}
}

struct FJobState:public TSharedFromThis<FJobState>
{
 enum class EKind{Open,Swap,Sample,Preview};EKind Kind;Lab::FWork W;TWeakObjectPtr<UCubelessNiagaraPreviewWorkflowJob> Job;TStrongObjectPtr<UCubelessNiagaraPreviewWorkflowJob> JobPin;
 FTSTicker::FDelegateHandle Handle;FString FinalSnapshot;bool PublicationQuarantined=false;FString ActorLabel;
 FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits),CancelReason;uint64 Revision=0,LastFrame=MAX_uint64;int32 Stage=0,TickCount=0,TicksDone=0;double Delta=1.0/30.0,Warmup=0,Started=FPlatformTime::Seconds(),Deadline=0;bool CleanupMode=false;bool Terminal=false,CancelRequested=false,InTick=false,SwapBound=false,SwapTouched=false,Restored=false,TrackControls=true,OriginalActive=false,OriginalTick=false,OriginalPaused=false,OriginalSolo=false,ExpectedTick=false,ExpectedPaused=false,ExpectedSolo=false;
 FIdentity ActorId,ComponentId,OriginalId,ReplacementId;TWeakObjectPtr<AActor> Actor;TWeakObjectPtr<UNiagaraComponent> Borrowed;TWeakObjectPtr<UNiagaraSystem> Original,Replacement,ExpectedAsset;
 TWeakPtr<FPlayerOwner> UI;uint64 UIEpoch=0;TArray<TSharedPtr<FJsonValue>> SampleRows;TArray<double> Warmups;TArray<int32> Views;int32 SampleIndex=0,SuccessCount=0,FailureCount=0;bool CleanupEach=true,CleanupAll=true;FString OutputDir,BaseLabel,PreviewFile,PreviewLabel;double PreviewView=1;bool PreviewCleanupAfter=true,PreviewWarming=false,PreviewOriginalSolo=false,PreviewOriginalTick=false,PreviewOriginalPaused=false;TSharedPtr<FJsonObject> Input,CurrentSample;TArray<FString> BeforeCleanup,FinalCleanup;TArray<TSharedPtr<FJsonValue>> Overrides;int32 CurrentOwned=INDEX_NONE;
 FJobState(EKind K,const TCHAR* Op):Kind(K),W(Op){W.R.Data->SetBoolField(TEXT("warmup_async_implemented"),true);W.R.Data->SetBoolField(TEXT("one_manual_step_per_distinct_GT_frame"),true);W.R.Data->SetBoolField(TEXT("AdvanceSimulation_SDK_may_synchronously_wait"),true);W.R.Data->SetBoolField(TEXT("override_full_bytes_and_simulation_time_restored"),false);W.R.Data->SetBoolField(TEXT("PNG_readback_is_bounded_synchronous_SDK"),true);if(Kind==EKind::Preview)W.R.Data->SetBoolField(TEXT("single_preview_lab_async"),true);}
 struct FPins
 {
  TArray<TStrongObjectPtr<UObject>> Pins;void Add(UObject* O){if(O)Pins.Emplace(O);}explicit FPins(FJobState& S){Add(S.W.Editor.Get());Add(S.W.World.Get());Add(S.W.System.Get());Add(S.W.Component.Get());Add(S.W.Spawned.Get());for(auto& A:S.W.Actors){Add(A.Weak.Get());Add(A.Class.Get());Add(A.Outer.Get());Add(A.Package.Get());Add(A.EffectivePackage.Get());}Add(S.Actor.Get());Add(S.Borrowed.Get());Add(S.Original.Get());Add(S.Replacement.Get());for(FIdentity* I:{&S.ActorId,&S.ComponentId,&S.OriginalId,&S.ReplacementId}){Add(I->Object.Get());Add(I->Class.Get());Add(I->Outer.Get());Add(I->Package.Get());Add(I->Outermost.Get());}} 
 };
 bool TargetSame()
 {
  if(GZStopping&&!CleanupMode)return false;
  if(SwapBound){UNiagaraComponent* C=Borrowed.Get();AActor* A=Actor.Get();if(!ActorId.Same()||!ComponentId.Same()||!Live(A)||!Live(C)||C->GetOwner()!=A||C->GetWorld()!=W.World.Get()||A->GetWorld()!=W.World.Get()||C->GetAsset()!=ExpectedAsset.Get()||(TrackControls&&(C->IsComponentTickEnabled()!=ExpectedTick||C->IsPaused()!=ExpectedPaused))||C->GetForceSolo()!=ExpectedSolo||A->GetActorLabel(false)!=ActorLabel)return false;
   if(!ReplacementId.Same()||(!OriginalId.Path.IsEmpty()&&!OriginalId.Same()))return false;TInlineComponentArray<UNiagaraComponent*> Cs(A);if(!Cs.Contains(C))return false;}
  if((Kind==EKind::Sample||(Kind==EKind::Preview&&PreviewWarming))&&W.ComponentCaptured&&Stage==11){UNiagaraComponent* C=W.Component.Get();if(!Live(C)||C->IsComponentTickEnabled()||C->IsPaused()||!C->GetForceSolo())return false;}
  if(UIEpoch){auto P=UI.Pin();if(!P||P->Epoch!=UIEpoch||!P->Same())return false;}return true;
 }
 bool Init(const FString& Text,UCubelessNiagaraPreviewWorkflowJob* In)
 {
  Job=In;JobPin.Reset(In);if(!W.Init(Text))return false;Input=W.P;W.ExtraGuard=[this]{return TargetSame();};Deadline=Started+900.0;W.R.Data->SetNumberField(TEXT("internal_deadline_seconds"),900);W.R.Data->SetStringField(TEXT("internal_deadline_scope"),TEXT("Continuation safety deadline, not an original public compile/warmup timeout"));return true;
 }
 void Publish(bool Final)
 {
  ++Revision;W.R.Data->SetStringField(TEXT("native_job_id"),Id);W.R.Data->SetBoolField(TEXT("native_job_terminal"),Final);W.R.Data->SetBoolField(TEXT("operation_complete"),Final);W.R.Data->SetNumberField(TEXT("native_checkpoint_revision"),static_cast<double>(Revision));W.R.Data->SetStringField(TEXT("native_job_phase"),W.R.Stage);W.R.Data->SetNumberField(TEXT("manual_step_calls_returned"),TicksDone);W.R.Data->SetNumberField(TEXT("manual_step_calls_requested"),TickCount);W.R.Data->SetBoolField(TEXT("manual_step_return_proves_particle_progress"),false);W.R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
  FString Snapshot;if(Final){W.Finish();if(Kind==EKind::Sample&&!Warmups.IsEmpty())W.L->SetBoolField(TEXT("success"),SuccessCount>0);Snapshot=Operations::EncodeVariableResult(W.R,*W.Op);}else Snapshot=Operations::EncodeVariableResult(W.R,*W.Op);FinalSnapshot=Snapshot;
  if(GlobalsSafe()||(CleanupMode&&GZNormalEditorExit&&OwnedCleanupSafe())){if(auto* J=Job.Get()){J->SnapshotJson=Snapshot;J->bIsTerminal=Final;}}else{PublicationQuarantined=true;W.R.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);W.R.Data->SetBoolField(TEXT("native_writers_stopped"),Final);W.R.Data->SetBoolField(TEXT("native_job_terminal"),false);W.R.Data->SetBoolField(TEXT("operation_complete"),false);W.R.bSuccess=false;W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;FinalSnapshot=Operations::EncodeVariableResult(W.R,*W.Op);/* Plain retained cache exposes actual prefix without touching the Job UObject or claiming final publication. */}
 }
 bool String(const TCHAR* K,FString& Out,bool Required=false){if(!W.P->HasField(K)){if(Required)return W.Fail(TEXT("niagara_preview_workflow_argument_missing"),FString(K)+TEXT(" required"));return true;}if(!W.P->TryGetStringField(K,Out)||!SafeText(Out,32768)||(Required&&Out.IsEmpty()))return W.Fail(TEXT("niagara_preview_workflow_string_invalid"),FString(K)+TEXT(" must be a bounded string"));return true;}
 bool Load(const TCHAR* Key,UNiagaraSystem*& Out)
 {
  FString Path;if(!String(Key,Path,true))return false;Path=Path.TrimStartAndEnd();Path.TrimQuotesInline();if(!Path.Contains(TEXT(".")))Path+=TEXT(".")+FPackageName::GetLongPackageAssetName(Path);if(!FPackageName::IsValidObjectPath(Path))return W.Fail(TEXT("niagara_preview_workflow_system_path_invalid"),TEXT("Valid Niagara System object path required"));
  if(!W.SDK(TEXT("LoadObject_System"),false,[&]{Out=LoadObject<UNiagaraSystem>(nullptr,*Path);W.Prefix->SetBoolField(TEXT("last_load_returned_nonnull"),Out!=nullptr);} ))return false;if(!Live(Out)||Out->GetClass()!=UNiagaraSystem::StaticClass())return W.Fail(TEXT("niagara_preview_workflow_system_missing"),TEXT("Exact stock Niagara System missing"));return W.Admit();
 }
 void SetWarmup(double Time,double Step){Warmup=FMath::Clamp(Time,0.,5.);Delta=FMath::Clamp(Step,1./240.,.25);TickCount=Kind==EKind::Preview?FMath::FloorToInt(static_cast<float>(Warmup)/static_cast<float>(Delta)):FMath::FloorToInt(Warmup/Delta);TicksDone=0;if(Kind==EKind::Preview){W.Prefix->SetNumberField(TEXT("warmup_float_time"),static_cast<float>(Warmup));W.Prefix->SetNumberField(TEXT("warmup_float_delta"),static_cast<float>(Delta));W.Prefix->SetNumberField(TEXT("warmup_float_floor_tick_count"),TickCount);}W.Prefix->SetNumberField(TEXT("warmup_requested"),Warmup);W.Prefix->SetNumberField(TEXT("warmup_delta"),Delta);}
 bool Advance(UNiagaraComponent* C)
 {
  if(TicksDone>=TickCount)return true;if(!W.Admit())return false;
  if(!W.SDK(TEXT("AdvanceSimulation_one_GT_step"),true,[&]{C->AdvanceSimulation(1,static_cast<float>(Delta));++TicksDone;W.Prefix->SetNumberField(TEXT("manual_step_calls_returned"),TicksDone);} ))return false;W.R.bChanged=true;return true;
 }
 bool PrepareOpen()
 {
  if(!ShowPlayer(W))return false;auto P=Player;UI=P;UIEpoch=P->Epoch;W.L=P->Metadata();W.R.Data->SetObjectField(TEXT("legacy_result"),W.L);W.L->SetBoolField(TEXT("opened"),true);FString Path;if(!String(TEXT("system_path"),Path))return false;if(Path.IsEmpty()){Stage=3;return true;}
  UNiagaraSystem* S=nullptr;if(!Load(TEXT("system_path"),S)){W.L->SetBoolField(TEXT("preview_loaded"),false);W.L->SetStringField(TEXT("preview_error"),W.R.Error);return false;}
  FIdentity Source(S);TStrongObjectPtr<UNiagaraSystem> PinS(S);if(P->Component){if(!P->Call(TEXT("Preview.DeactivateOld"),[&]{P->Component->DeactivateImmediate();}))return W.Fail(TEXT("niagara_preview_player_attach_interrupted"),P->LastError);UNiagaraComponent* Old=P->Component;FIdentity OldId(Old);TStrongObjectPtr<UNiagaraComponent> HoldOld(Old);P->Component=nullptr;P->System=nullptr;P->ComponentHold.Reset();P->SystemHold.Reset();
   if(!OldId.Same()||!W.SDK(TEXT("Preview.RemoveOld"),true,[&]{P->Scene->RemoveComponent(Old);})||!OldId.Same())return W.Fail(TEXT("niagara_preview_player_attach_interrupted"),TEXT("Old exact owned component changed during detach"));}
  UNiagaraComponent* C=nullptr;if(!W.SDK(TEXT("NewObject_preview_component"),true,[&]{C=NewObject<UNiagaraComponent>(GetTransientPackage(),NAME_None,RF_Transient);W.Prefix->SetBoolField(TEXT("transient_component_returned_nonnull"),C!=nullptr);} ))return false;if(!Live(C)||!Source.Same())return W.Fail(TEXT("niagara_preview_player_component_invalid"),TEXT("Preview component/source changed"));TStrongObjectPtr<UNiagaraComponent> Hold(C);FIdentity NewComponent(C);
  // Bind after expected setters, checking source and UI after every public SDK. No old pointer survives a callback unexamined.
  auto Setup=[&](const TCHAR* N,auto&& Fn){if(!W.Admit()||!Source.Same()||!NewComponent.Same())return false;if(!W.SDK(N,true,Fn))return false;return Source.Same()&&NewComponent.Same();};
  if(!Setup(TEXT("Preview.SetAsset"),[&]{C->SetAsset(S);})||C->GetAsset()!=S)return false;
  if(!Setup(TEXT("Preview.SetForceSolo"),[&]{C->SetForceSolo(true);}))return false;
  if(!Setup(TEXT("Preview.AddComponent"),[&]{P->Scene->AddComponent(C,FTransform::Identity);}))return false;
  P->Component=C;P->System=S;P->ComponentHold.Reset(C);P->SystemHold.Reset(S);P->ComponentId=FIdentity(C);P->SystemId=Source;P->Settling=true;P->Playing=false;P->Renderable=false;
  if(!W.SDK(TEXT("Preview.Activate"),true,[&]{C->Activate(true);})||!W.SDK(TEXT("Preview.UnpauseWarmup"),true,[&]{C->SetPaused(false);}))return false;
  P->DropKind=TEXT("asset");P->DisplayName=S->GetName();P->ObjectPath=Source.Path;P->ClassName=S->GetClass()->GetName();++P->DropCount;P->Playback=TEXT("settling");P->Analysis=TEXT("Analysis\nPublic component preview; full renderer/user/stack/graph/module diagnostic text parity is incomplete. GPU completion is not proven.");SetWarmup(.25,1./30.);Stage=1;W.R.bChanged=true;return W.Admit();
 }
 bool CompleteOpen(){auto P=UI.Pin();if(!P||!W.Admit())return false;P->Settling=false;P->Playing=true;P->Renderable=true;P->Playback=TEXT("playing");if(P->Client)P->Client->SetRealtime(true);P->Frame();if(!W.Admit())return false;W.L=P->Metadata();W.R.Data->SetObjectField(TEXT("legacy_result"),W.L);W.L->SetBoolField(TEXT("opened"),true);W.L->SetBoolField(TEXT("preview_loaded"),true);LastPlayerState=W.L;Stage=3;return true;}
 bool PrepareSwap()
 {
  bool Restore=true,Preserve=true,Activate=true;if(!W.Bool(TEXT("restore_original_asset"),true,Restore)||!W.Bool(TEXT("preserve_override_parameters"),true,Preserve)||!W.Bool(TEXT("activate_after_swap"),true,Activate))return false;if(!Restore||!Preserve)return W.Fail(TEXT("niagara_preview_swap_restore_required"),TEXT("restore_original_asset and preserve_override_parameters must both be true"));
  UNiagaraSystem* New=nullptr;if(!Load(TEXT("replacement_system_path"),New))return false;Replacement=New;ReplacementId=FIdentity(New);TStrongObjectPtr<UNiagaraSystem> NewPin(New);
  FString AN,AL,CN,SF;if(!String(TEXT("actor_name"),AN)||!String(TEXT("actor_label"),AL)||!String(TEXT("component_name"),CN)||!String(TEXT("system_path"),SF))return false;
  auto Filters=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("actor_name"),TEXT("actor_label"),TEXT("component_name"),TEXT("system_path"),TEXT("parameter_name_contains")}){FString V;if(!String(K,V))return false;Filters->SetStringField(K,V);}W.L->SetObjectField(TEXT("filters"),Filters);
  auto Match=[](const FString& S,const FString& F){return F.IsEmpty()||S.Contains(F,ESearchCase::IgnoreCase);};int32 Actors=0,Scanned=0,Matches=0;
  for(TActorIterator<AActor> It(W.World.Get());It;++It){if(++Actors>Lab::MaxActors)return W.Fail(TEXT("niagara_preview_swap_scan_limit"),TEXT("Bounded actor scan exceeded"));AActor* A=*It;if(!Live(A))continue;FIdentity AP(A);FString OldLabel=A->GetActorLabel(false),Label;if(!W.SDK(TEXT("Swap.GetActorLabel"),false,[&]{Label=A->GetActorLabel();if(Label!=OldLabel){W.R.bChanged=true;W.Count(TEXT("lazy_actor_labels_created"));}})||!AP.Same())return false;if(!Label.IsEmpty()&&A->GetActorLabel(false)==Label){W.Prefix->SetBoolField(TEXT("legacy_label_getter_may_create_labels"),true);}
   FString Search=A->GetName()+TEXT("|")+Label+TEXT("|")+AP.Path;if(!Match(Search,AN)||!Match(Label,AL))continue;TInlineComponentArray<UNiagaraComponent*> Cs(A);if(Cs.Num()>65536)return W.Fail(TEXT("niagara_preview_swap_component_limit"),TEXT("Component bound exceeded"));for(UNiagaraComponent* C:Cs){if(!Live(C))continue;++Scanned;UNiagaraSystem* Asset=C->GetAsset();if(!Match(C->GetName()+TEXT("|")+C->GetPathName(),CN)||!Match(Asset?Asset->GetPathName():FString(),SF))continue;++Matches;Actor=A;Borrowed=C;}}
  W.L->SetNumberField(TEXT("actor_count_scanned"),Actors);W.L->SetNumberField(TEXT("niagara_component_count_scanned"),Scanned);W.L->SetNumberField(TEXT("matching_niagara_component_count"),Matches);if(Matches!=1)return W.Fail(TEXT("niagara_preview_swap_unique_target_required"),TEXT("Filters must match exactly one Niagara component"));
  UNiagaraComponent* C=Borrowed.Get();if(!Live(C)||!Live(Actor.Get())||C->GetOwner()!=Actor.Get()||C->GetWorld()!=W.World.Get())return W.Fail(TEXT("niagara_preview_swap_target_expired"),TEXT("Matched weak target expired during scan"));ActorLabel=Actor->GetActorLabel(false);ActorId=FIdentity(Actor.Get());ComponentId=FIdentity(C);Original=C->GetAsset();if(Original.IsValid())OriginalId=FIdentity(Original.Get());ExpectedAsset=Original;OriginalActive=C->IsActive();OriginalTick=ExpectedTick=C->IsComponentTickEnabled();OriginalPaused=ExpectedPaused=C->IsPaused();OriginalSolo=ExpectedSolo=C->GetForceSolo();SwapBound=true;if(!W.Admit())return false;
  W.L->SetStringField(TEXT("original_system_path"),OriginalId.Path);W.L->SetStringField(TEXT("replacement_system_path"),ReplacementId.Path);W.L->SetBoolField(TEXT("restore_original_asset"),true);W.L->SetBoolField(TEXT("preserve_override_parameters"),true);W.L->SetBoolField(TEXT("activate_after_swap"),Activate);W.L->SetBoolField(TEXT("was_active_before_swap"),OriginalActive);W.L->SetBoolField(TEXT("map_dirty_before"),W.WorldPackage->IsDirty());W.L->SetStringField(TEXT("current_map_package"),W.WorldPackage->GetName());W.L->SetBoolField(TEXT("restored_clean_map_dirty_flag"),false);W.L->SetBoolField(TEXT("map_was_reloaded"),false);W.L->SetBoolField(TEXT("map_was_saved"),false);W.L->SetBoolField(TEXT("asset_mutation_admitted"),false);W.L->SetBoolField(TEXT("component_asset_swap_persisted"),false);
  double Time,Step;if(!W.Number(TEXT("advance_time"),.35,Time)||!W.Number(TEXT("advance_tick_delta"),1./30.,Step))return false;SetWarmup(Time,Step);W.L->SetNumberField(TEXT("advance_time"),Warmup);W.L->SetNumberField(TEXT("advance_tick_delta"),Delta);
  if(!Projection(TEXT("before_swap")))return false;
  if(!AssetStep(Replacement.Get(),TEXT("Swap.SetAsset_replacement")))return false;
  if(!Lifecycle(TEXT("Swap.ResetSystem"),[&]{Borrowed->ResetSystem();})||!Lifecycle(TEXT("Swap.ReinitializeSystem"),[&]{Borrowed->ReinitializeSystem();}))return false;
  if(Activate&&!Lifecycle(TEXT("Swap.Activate"),[&]{Borrowed->Activate(true);}))return false;
  // Pause and natural component tick are restored separately. A paused System does not execute AdvanceSimulation.
  if(!SoloStep(true)||!TickStep(false)||!PauseStep(false))return false;Stage=1;return true;
 }
 bool AssetStep(UNiagaraSystem* S,const TCHAR* N){if(!W.Admit())return false;W.R.Stage=N;W.R.bMutationAttempted=true;W.R.bChangeStateKnown=false;W.Count(N);SwapTouched=true;ExpectedAsset=S;TrackControls=false;Borrowed->SetAsset(S,false);W.Prefix->SetBoolField(TEXT("component_asset_setter_returned"),true);W.R.bChanged=true;if(!W.Admit())return false;ExpectedTick=Borrowed->IsComponentTickEnabled();ExpectedPaused=Borrowed->IsPaused();TrackControls=true;return W.Admit();}
 template<class F>bool Lifecycle(const TCHAR* N,F&& Fn){if(!W.Admit())return false;W.R.Stage=N;W.R.bMutationAttempted=true;W.R.bChangeStateKnown=false;W.Count(N);TrackControls=false;Fn();if(!W.Admit())return false;ExpectedTick=Borrowed->IsComponentTickEnabled();ExpectedPaused=Borrowed->IsPaused();TrackControls=true;return W.Admit();}
 bool TickStep(bool B){if(!W.Admit())return false;W.R.Stage=TEXT("Component.SetComponentTickEnabled");W.R.bMutationAttempted=true;W.R.bChangeStateKnown=false;ExpectedTick=B;Borrowed->SetComponentTickEnabled(B);return W.Admit();}
 bool SoloStep(bool B){if(!W.Admit())return false;W.R.Stage=TEXT("Component.SetForceSolo");W.R.bMutationAttempted=true;W.R.bChangeStateKnown=false;TrackControls=false;ExpectedSolo=B;Borrowed->SetForceSolo(B);if(!W.Admit())return false;ExpectedTick=Borrowed->IsComponentTickEnabled();ExpectedPaused=Borrowed->IsPaused();TrackControls=true;return W.Admit();}
 bool PauseStep(bool B){if(!W.Admit())return false;W.R.Stage=TEXT("Component.SetPaused");W.R.bMutationAttempted=true;W.R.bChangeStateKnown=false;ExpectedPaused=B;Borrowed->SetPaused(B);return W.Admit();}
 bool Projection(const TCHAR* Phase)
 {
  if(!W.Admit())return false;auto O=MakeShared<FJsonObject>();UNiagaraComponent* C=Borrowed.Get();AActor* A=Actor.Get();O->SetStringField(TEXT("phase"),Phase);O->SetBoolField(TEXT("has_component"),true);O->SetStringField(TEXT("actor_name"),A->GetName());O->SetStringField(TEXT("actor_label"),A->GetActorLabel(false));O->SetStringField(TEXT("actor_path"),ActorId.Path);O->SetStringField(TEXT("component_name"),C->GetName());O->SetStringField(TEXT("component_path"),ComponentId.Path);O->SetStringField(TEXT("component_class"),C->GetClass()->GetName());O->SetBoolField(TEXT("registered"),C->IsRegistered());O->SetBoolField(TEXT("active"),C->IsActive());O->SetBoolField(TEXT("visible"),C->IsVisible());O->SetBoolField(TEXT("component_tick_enabled"),C->IsComponentTickEnabled());O->SetStringField(TEXT("asset_path"),ExpectedAsset.IsValid()?ExpectedAsset->GetPathName():FString());O->SetStringField(TEXT("asset_class"),ExpectedAsset.IsValid()?ExpectedAsset->GetClass()->GetName():FString());O->SetArrayField(TEXT("world_location"),VJson(C->GetComponentLocation()));O->SetArrayField(TEXT("world_scale"),VJson(C->GetComponentScale()));O->SetBoolField(TEXT("internal_emitter_runtime_available"),false);
  auto Q=MakeShared<FJsonObject>();Q->Values=Input->Values;Q->SetStringField(TEXT("actor_name"),ActorId.Path);Q->SetStringField(TEXT("actor_label"),TEXT(""));Q->SetStringField(TEXT("component_name"),ComponentId.Path);Q->SetStringField(TEXT("system_path"),TEXT(""));Q->SetBoolField(TEXT("include_instance_parameters"),false);Q->SetNumberField(TEXT("max_components"),2);FString Encoded;
  if(!W.SDK(TEXT("GO.public_runtime_projection"),false,[&]{Encoded=NiagaraRuntimeInspection::InspectRuntimeState(Encode(Q));}))return false;
  if(Encoded.Len()>32*1024*1024)return W.Fail(TEXT("niagara_preview_swap_projection_limit"),TEXT("Runtime receipt exceeds bounded32Mi UTF16"));TSharedPtr<FJsonObject> Child;auto Reader=TJsonReaderFactory<>::Create(Encoded);if(!FJsonSerializer::Deserialize(Reader,Child)||!Child.IsValid())return W.Fail(TEXT("niagara_preview_swap_projection_invalid"),Reader->GetErrorMessage());bool ChildSuccess=false,ChildPost=false;if(!Child->TryGetBoolField(TEXT("success"),ChildSuccess)||!ChildSuccess||!Child->TryGetBoolField(TEXT("postcondition_passed"),ChildPost)||!ChildPost)return W.Fail(TEXT("niagara_preview_swap_projection_failed"),TEXT("Public runtime observation did not pass its own health guard"));const TSharedPtr<FJsonObject>* CD=nullptr;const TSharedPtr<FJsonObject>* CL=nullptr;const TArray<TSharedPtr<FJsonValue>>* Components=nullptr;
  if(!Child->TryGetObjectField(TEXT("data"),CD)||!CD||!(*CD)->TryGetObjectField(TEXT("legacy_result"),CL)||!CL||!(*CL)->TryGetArrayField(TEXT("components"),Components)||!Components||Components->Num()!=1||!(*Components)[0].IsValid()||(*Components)[0]->Type!=EJson::Object)return W.Fail(TEXT("niagara_preview_swap_projection_identity_invalid"),TEXT("Runtime projection must contain exactly the captured component"));
  auto CR=(*Components)[0]->AsObject();FString CP,AP;if(!CR->TryGetStringField(TEXT("component_path"),CP)||CP!=ComponentId.Path||!CR->TryGetStringField(TEXT("actor_path"),AP)||AP!=ActorId.Path)return W.Fail(TEXT("niagara_preview_swap_projection_identity_invalid"),TEXT("Child component/actor differs from captured target"));
  for(const auto& KV:CR->Values)O->SetField(FString(KV.Key),KV.Value);O->SetStringField(TEXT("phase"),Phase);O->SetBoolField(TEXT("full_original_internal_projection_available"),false);O->SetBoolField(TEXT("public_runtime_health_passed"),true);W.L->SetObjectField(Phase,O);
  // Preserve original observation options and clamp counts without claiming internal instance availability.
  for(const TCHAR* K:{TEXT("include_parameter_values"),TEXT("include_raw_bytes"),TEXT("include_parameter_object_properties"),TEXT("include_emitter_runtime")}){bool B=false;if(!W.Bool(K,FCString::Strcmp(K,TEXT("include_raw_bytes"))!=0&&FCString::Strcmp(K,TEXT("include_parameter_object_properties"))!=0,B))return false;W.L->SetBoolField(K,B);}
  struct FCap{const TCHAR* K;double D;int32 M;};for(const FCap& Cap:TArray<FCap>{{TEXT("max_parameters"),256,4096},{TEXT("max_emitter_instances"),128,256},{TEXT("max_raw_bytes"),64,4096},{TEXT("max_object_properties"),80,512},{TEXT("max_array_elements"),16,256},{TEXT("max_property_value_chars"),512,16384}}){double D;if(!W.Number(Cap.K,Cap.D,D))return false;W.L->SetNumberField(Cap.K,FMath::Clamp(D,0.,static_cast<double>(Cap.M)));}
  return W.Admit();
 }
 bool RestoreSwap()
 {
  if(!SwapTouched){Restored=true;return true;}W.Prefix->SetBoolField(TEXT("restore_attempted"),true);if(!W.Admit()){W.Prefix->SetBoolField(TEXT("restore_skipped_unsafe_or_target_changed"),true);return false;}
  if(!AssetStep(Original.Get(),TEXT("Restore.SetAsset_original"))||!Lifecycle(TEXT("Restore.ResetSystem"),[&]{Borrowed->ResetSystem();})||!Lifecycle(TEXT("Restore.ReinitializeSystem"),[&]{Borrowed->ReinitializeSystem();}))return false;
  if(OriginalActive){if(!Lifecycle(TEXT("Restore.Activate"),[&]{Borrowed->Activate(true);}))return false;}else if(!Lifecycle(TEXT("Restore.DeactivateImmediate"),[&]{Borrowed->DeactivateImmediate();}))return false;
  if(!SoloStep(OriginalSolo)||!PauseStep(OriginalPaused)||!TickStep(OriginalTick))return false;Restored=Borrowed->GetAsset()==Original.Get()&&Borrowed->IsActive()==OriginalActive&&Borrowed->IsComponentTickEnabled()==OriginalTick&&Borrowed->IsPaused()==OriginalPaused&&Borrowed->GetForceSolo()==OriginalSolo;
  W.Prefix->SetBoolField(TEXT("restore_asset_active_tick_pause_verified"),Restored);W.L->SetBoolField(TEXT("restored_original_asset"),Restored);if(!Restored)return W.Fail(TEXT("niagara_preview_swap_restore_incomplete"),TEXT("Original asset/active/tick/pause readback differs; no restore replay"));if(!Projection(TEXT("after_restore")))return false;bool Dirty=W.WorldPackage->IsDirty();W.L->SetBoolField(TEXT("map_dirty_after_restore_before_cleanup"),Dirty);W.L->SetBoolField(TEXT("map_dirty_after"),Dirty);return W.Admit();
 }

 bool PreparePreview()
 {
  bool Allow=false,Before=true,Preserve=true,Auto=true;double Time,Step,View;
  // Only the first single-preview lexical containment refusal precedes every target SDK/file probe.
  bool InitialMapDirty=false;
  const bool LexicalEntry=Kind==EKind::Preview&&Stage==0&&!W.ScopeLost&&W.R.Error.IsEmpty()&&W.Counts->Values.IsEmpty()&&W.Prefix->Values.Num()==2&&W.Prefix->HasField(TEXT("world"))&&W.Prefix->TryGetBoolField(TEXT("map_dirty_before"),InitialMapDirty)&&!W.R.bMutationAttempted&&!W.R.bChanged&&!W.R.bCompileRequested&&!W.R.bCompileAttempted&&W.R.Compiled.IsSet()&&!W.R.Compiled.GetValue()&&!W.R.bSaveRequested&&!W.R.bSaveAttempted&&!W.R.bSaved&&!W.ViewCaptured&&!W.SystemCaptured&&!W.ComponentCaptured&&W.Actors.IsEmpty()&&CurrentOwned==INDEX_NONE;
  if(!W.Path(PreviewFile))
  {
   if(LexicalEntry&&W.R.ErrorCode==TEXT("niagara_preview_lab_filepath_outside_root")&&W.Counts->Values.IsEmpty()&&W.Prefix->Values.Num()==2&&!W.Prefix->HasField(TEXT("filepath"))&&!W.Prefix->HasField(TEXT("file_existed_before"))&&W.Admit()&&!W.ScopeLost&&W.WorldPackage->IsDirty()==InitialMapDirty&&W.Admit())
   {
    // Do not publish known yet: Finish must close the same world/current/dirty boundary again.
    W.R.Data->SetBoolField(TEXT("prewriter_lexical_path_refusal_current_verified"),true);
    W.R.Data->SetStringField(TEXT("prewriter_lexical_path_refusal_normalized_filepath"),PreviewFile);
    W.R.Data->SetStringField(TEXT("prewriter_lexical_path_refusal_scope"),TEXT("Exact single-preview outside-root lexical refusal before tracked target calls, FileExists, load, actor scan, viewport, cleanup, component/warmup/capture or file writes. Existing job/world bootstrap and path cache are not global SDK purity proof; final world identity/dirty admission remains required."));
   }
   return false;
  }
  if(!W.Gate(Allow)||!W.Number(TEXT("warmup_time"),.35,Time)||!W.Number(TEXT("warmup_tick_delta"),1./30.,Step)||!W.Number(TEXT("view"),1,View)||!W.Bool(TEXT("cleanup_before"),true,Before)||!W.Bool(TEXT("cleanup_after"),true,PreviewCleanupAfter)||!W.Bool(TEXT("preserve_override_parameters"),true,Preserve)||!W.Bool(TEXT("auto_frame"),true,Auto))return false;
  bool Has=false;FVector Value;for(const TCHAR* K:{TEXT("location"),TEXT("scale")})if(!W.Vector(K,nullptr,Has,Value,true))return false;
  if(!W.Vector(TEXT("camera_location"),TEXT("view_location"),Has,Value)||!W.Vector(TEXT("camera_rotation"),TEXT("view_rotation"),Has,Value))return false;
  const TCHAR* FK=Input->HasField(TEXT("camera_fov"))?TEXT("camera_fov"):TEXT("view_fov");double Fov;if(Input->HasField(FK)&&!W.Number(FK,90,Fov))return false;
  UNiagaraSystem* S=nullptr;if(!Load(TEXT("system_path"),S))return false;
  W.System=S;W.SystemPath=S->GetPathName();W.SystemOuter=S->GetOuter();W.SystemPackage=S->GetOutermost();W.SystemEffectivePackage=S->GetPackage();W.SystemCaptured=true;
  W.Prefix->SetStringField(TEXT("source_system"),W.SystemPath);W.Prefix->SetBoolField(TEXT("source_package_dirty_before"),S->GetOutermost()->IsDirty());
  FString Suffix=FPaths::GetBaseFilename(W.SystemPath);if(!String(TEXT("label"),Suffix)||!SafeText(Suffix,4096))return W.Fail(TEXT("niagara_preview_lab_label_invalid"),TEXT("Bounded original label required"));PreviewLabel=FString(Lab::LabPrefix)+Suffix;PreviewView=FMath::Clamp(View,1.,3.);SetWarmup(Time,Step);
  W.L->SetStringField(TEXT("system_path"),W.SystemPath);W.L->SetStringField(TEXT("actor_label"),PreviewLabel);W.L->SetStringField(TEXT("filepath"),PreviewFile);
  W.L->SetBoolField(TEXT("allow_current_map_preview"),Allow);W.L->SetBoolField(TEXT("used_expected_preview_lab_map"),W.WorldPackage->GetName()==Lab::LabMap);W.L->SetNumberField(TEXT("view"),PreviewView);W.L->SetNumberField(TEXT("warmup_time"),Warmup);W.L->SetNumberField(TEXT("warmup_tick_delta"),Delta);W.L->SetBoolField(TEXT("auto_frame"),Auto);W.L->SetBoolField(TEXT("preserve_override_parameters"),Preserve);W.L->SetBoolField(TEXT("cleanup_after"),PreviewCleanupAfter);
  if(Before&&!Lab::CleanupActors(W,BeforeCleanup))return false;W.L->SetArrayField(TEXT("cleanup_before_labels"),Lab::Labels(BeforeCleanup));Stage=20;return W.Admit();
 }
 bool CompletePreview()
 {
  auto Parent=W.L;auto Capture=MakeShared<FJsonObject>();W.L=Capture;W.Prefix->RemoveField(TEXT("capture_native_receipt"));bool Good=Lab::Capture(W);W.L=Parent;
  W.L->SetObjectField(TEXT("capture"),Capture);W.Prefix->SetObjectField(TEXT("capture"),Capture);CurrentSample->SetBoolField(TEXT("capture_returned_success"),Good);
  CurrentSample->SetNumberField(TEXT("manual_step_calls_returned"),TicksDone);CurrentSample->SetBoolField(TEXT("GPU_simulation_completion_verified"),false);
  if(!W.Admit())return false;
  if(PreviewCleanupAfter||!Good)
  {
   TArray<int32> Only{CurrentOwned};bool Clean=Lab::CleanupActors(W,FinalCleanup,Good?nullptr:&Only);W.L->SetArrayField(TEXT("cleanup_after_labels"),Lab::Labels(FinalCleanup));W.Prefix->SetArrayField(TEXT("cleanup_after_labels"),Lab::Labels(FinalCleanup));if(!Clean)return false;CurrentOwned=INDEX_NONE;PreviewWarming=false;
  }
  else
  {
   // This actor remains by explicit request. Restore the real controls changed only for cooperative warmup.
   PreviewWarming=false;UNiagaraComponent* C=W.Component.Get();
   if(!W.SDK(TEXT("Preview.RestoreForceSolo"),true,[&]{C->SetForceSolo(PreviewOriginalSolo);})||!W.SDK(TEXT("Preview.RestorePause"),true,[&]{C->SetPaused(PreviewOriginalPaused);})||!W.SDK(TEXT("Preview.RestoreNaturalTick"),true,[&]{C->SetComponentTickEnabled(PreviewOriginalTick);}))return false;
   const bool ActualSolo=C->GetForceSolo(),ActualPaused=C->IsPaused(),ActualTick=C->IsComponentTickEnabled();const bool PreviewControlsRestored=ActualSolo==PreviewOriginalSolo&&ActualPaused==PreviewOriginalPaused&&ActualTick==PreviewOriginalTick;W.Prefix->SetBoolField(TEXT("retained_preview_controls_restored"),PreviewControlsRestored);
   if(!PreviewControlsRestored){auto M=MakeShared<FJsonObject>();M->SetStringField(TEXT("object"),W.ComponentPath);M->SetStringField(TEXT("property"),TEXT("ForceSolo/Paused/NaturalTick"));M->SetStringField(TEXT("expected"),FString::Printf(TEXT("%d/%d/%d"),PreviewOriginalSolo,PreviewOriginalPaused,PreviewOriginalTick));M->SetStringField(TEXT("actual"),FString::Printf(TEXT("%d/%d/%d"),ActualSolo,ActualPaused,ActualTick));W.R.Data->SetObjectField(TEXT("preview_first_mismatch"),M);return W.Fail(TEXT("niagara_preview_lab_controls_restore_failed"),TEXT("Retained exact preview component controls differ after restore; no retry"));}W.L->SetArrayField(TEXT("cleanup_after_labels"),Lab::Labels(FinalCleanup));
  }
  if(!Good)return false;
  W.L->SetNumberField(TEXT("user_parameter_override_count"),Overrides.Num());W.L->SetArrayField(TEXT("user_parameter_overrides"),Overrides);W.L->SetBoolField(TEXT("map_dirty_after_preview"),W.WorldPackage->IsDirty());
  bool Has=false;FVector V;if(!W.Vector(TEXT("camera_location"),TEXT("view_location"),Has,V))return false;if(Has)W.L->SetArrayField(TEXT("camera_location_override"),VJson(V));if(!W.Vector(TEXT("camera_rotation"),TEXT("view_rotation"),Has,V))return false;if(Has)W.L->SetArrayField(TEXT("camera_rotation_override"),VJson(V));const TCHAR* FK=Input->HasField(TEXT("camera_fov"))?TEXT("camera_fov"):TEXT("view_fov");if(Input->HasField(FK)){double Fov;if(!W.Number(FK,90,Fov))return false;W.L->SetNumberField(TEXT("camera_fov_override"),FMath::Clamp(Fov,1.,179.));}
  W.L->SetStringField(TEXT("safety_note"),TEXT("Single original filepath/label and cleanup contract; float floor warmup scheduled one manual SDK step per Editor frame. Internal AdvanceSimulation and PNG capture can synchronously wait. No source compile/save, map reload/save, GPU completion or rendered-particle proof."));W.MapFields();Stage=3;return W.Admit();
 }

 bool PrepareSamples()
 {
  bool Allow=false,Before=true;if(!W.Gate(Allow))return false;UNiagaraSystem* S=nullptr;if(!Load(TEXT("system_path"),S))return false;W.System=S;W.SystemPath=S->GetPathName();W.SystemOuter=S->GetOuter();W.SystemPackage=S->GetOutermost();W.SystemEffectivePackage=S->GetPackage();W.SystemCaptured=true;
  BaseLabel=FPaths::GetBaseFilename(W.SystemPath);if(!String(TEXT("label"),BaseLabel))return false;BaseLabel=FPaths::MakeValidFileName(BaseLabel);if(BaseLabel.IsEmpty())BaseLabel=TEXT("NiagaraSample");OutputDir=TEXT("Samples/")+BaseLabel;if(!String(TEXT("output_dir"),OutputDir))return false;if(OutputDir.IsEmpty())OutputDir=TEXT("Samples/")+BaseLabel;
  Warmups={.1,.35,.7};const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(Input->TryGetArrayField(TEXT("warmup_times"),A)&&A&&!A->IsEmpty()){Warmups.Reset();for(int32 I=0;I<FMath::Min(A->Num(),12);++I)Warmups.Add(FMath::Clamp((*A)[I]->AsNumber(),0.,5.));}
  Views={1};if(Input->TryGetArrayField(TEXT("views"),A)&&A&&!A->IsEmpty()){Views.Reset();for(int32 I=0;I<FMath::Min(A->Num(),3);++I){double D=FMath::Clamp((*A)[I]->AsNumber(),1.,3.);Views.AddUnique(static_cast<int32>(D));}}
  double D;if(!W.Number(TEXT("warmup_tick_delta"),1./30.,D)||!W.Bool(TEXT("cleanup_before"),true,Before)||!W.Bool(TEXT("cleanup_after_each"),true,CleanupEach)||!W.Bool(TEXT("cleanup_after_all"),true,CleanupAll))return false;Delta=FMath::Clamp(D,1./240.,.25);Deadline=Started+120.+static_cast<double>(Warmups.Num()*Views.Num())*130.;W.R.Data->SetNumberField(TEXT("internal_deadline_seconds"),Deadline-Started);
  // Validate every sample path/camera/default before first cleanup/spawn.
  bool Has=false;FVector V;for(const TCHAR* K:{TEXT("location"),TEXT("scale")})if(!W.Vector(K,nullptr,Has,V,true))return false;
  if(!W.Vector(TEXT("camera_location"),TEXT("view_location"),Has,V)||!W.Vector(TEXT("camera_rotation"),TEXT("view_rotation"),Has,V))return false;const TCHAR* FK=Input->HasField(TEXT("camera_fov"))?TEXT("camera_fov"):TEXT("view_fov");if(Input->HasField(FK)&&!W.Number(FK,90,D))return false;
  for(double Time:Warmups)for(int32 View:Views){FString Path=OutputDir/FString::Printf(TEXT("%s_t%04d_v%d.png"),*BaseLabel,FMath::RoundToInt(Time*1000),View);auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("filepath"),Path);W.P=P;FString Normalized;if(!W.Path(Normalized)){W.P=Input;return false;}}W.P=Input;
  W.L->SetStringField(TEXT("system_path"),W.SystemPath);W.L->SetStringField(TEXT("label"),BaseLabel);W.L->SetStringField(TEXT("output_dir"),OutputDir);W.L->SetBoolField(TEXT("allow_current_map_preview"),Allow);W.L->SetBoolField(TEXT("used_expected_preview_lab_map"),W.WorldPackage->GetName()==Lab::LabMap);W.L->SetStringField(TEXT("current_map_package"),W.WorldPackage->GetName());W.L->SetNumberField(TEXT("warmup_tick_delta"),Delta);W.L->SetBoolField(TEXT("has_user_parameter_overrides"),Input->HasField(TEXT("user_parameter_overrides")));W.L->SetBoolField(TEXT("map_was_reloaded"),false);W.L->SetBoolField(TEXT("map_was_saved"),false);
  if(Before&&!Lab::CleanupActors(W,BeforeCleanup))return false;W.L->SetArrayField(TEXT("cleanup_before_labels"),Lab::Labels(BeforeCleanup));Stage=10;return W.Admit();
 }
 bool BeginSample()
 {
  const bool Single=Kind==EKind::Preview;int32 VI=0,Ms=0;double SelectedView=PreviewView;FString File=PreviewFile;
  if(!Single){VI=SampleIndex%Views.Num();const int32 TI=SampleIndex/Views.Num();if(TI>=Warmups.Num()){Stage=13;return true;}SetWarmup(Warmups[TI],Delta);Ms=FMath::RoundToInt(Warmup*1000);File=OutputDir/FString::Printf(TEXT("%s_t%04d_v%d.png"),*BaseLabel,Ms,Views[VI]);SelectedView=Views[VI];}
  CurrentSample=MakeShared<FJsonObject>();CurrentSample->SetNumberField(TEXT("sample_index"),SampleIndex);CurrentSample->SetNumberField(TEXT("warmup_time"),Warmup);CurrentSample->SetNumberField(TEXT("view"),SelectedView);CurrentSample->SetStringField(TEXT("requested_filepath"),File);CurrentSample->SetBoolField(TEXT("success"),false);if(Single)W.Prefix->SetObjectField(TEXT("single_preview_state"),CurrentSample);else{SampleRows.Add(MakeShared<FJsonValueObject>(CurrentSample));W.L->SetArrayField(TEXT("samples"),SampleRows);}
  auto Q=MakeShared<FJsonObject>();Q->Values=Input->Values;Q->SetStringField(TEXT("filepath"),File);Q->SetNumberField(TEXT("view"),SelectedView);Q->SetBoolField(TEXT("cleanup_before"),false);W.P=Q;FString Label=Single?PreviewLabel:FString(Lab::LabPrefix)+FString::Printf(TEXT("%s_S%02d_T%04d_V%d"),*BaseLabel,SampleIndex,Ms,Views[VI]);bool Has=false;FVector Loc(0,0,120),Scale(1);if(!W.Vector(TEXT("location"),nullptr,Has,Loc,true)||!W.Vector(TEXT("scale"),nullptr,Has,Scale,true))return false;bool Preserve=true;if(!W.Bool(TEXT("preserve_override_parameters"),true,Preserve))return false;
  W.ComponentCaptured=false;ANiagaraActor* A=nullptr;if(!W.SDK(TEXT("Sample.SpawnActor"),true,[&]{FActorSpawnParameters SP;SP.ObjectFlags=RF_Transient;A=W.World->SpawnActor<ANiagaraActor>(ANiagaraActor::StaticClass(),Loc,FRotator::ZeroRotator,SP);W.Prefix->SetBoolField(TEXT("spawn_returned_nonnull"),A!=nullptr);CurrentSample->SetBoolField(TEXT("spawn_returned_nonnull"),A!=nullptr);} ))return false;
  if(!Live(A)||A->GetClass()!=ANiagaraActor::StaticClass()||A->GetWorld()!=W.World.Get())return W.Fail(TEXT("niagara_preview_sample_spawn_invalid"),TEXT("No exact stock transient Niagara actor returned"));TStrongObjectPtr<ANiagaraActor> Hold(A);W.R.bChanged=true;W.Spawned=A;CurrentOwned=W.Actors.Emplace(A);auto& AP=W.Actors[CurrentOwned];CurrentSample->SetStringField(TEXT("spawned_actor"),AP.Path);AP.Label=Label;
  if(!W.SDK(TEXT("Sample.SetActorLabel"),true,[&]{A->SetActorLabel(Label,false);})||!W.ActorAdmit(AP))return false;
  if(!W.SDK(TEXT("Sample.SetScale"),true,[&]{A->SetActorScale3D(Scale);})||!W.ActorAdmit(AP))return false;
  if(Single&&(!W.SDK(TEXT("Preview.SetFolderPath"),true,[&]{A->SetFolderPath(TEXT("MCP/NiagaraPreviewLab"));})||!W.ActorAdmit(AP)))return false;
  UNiagaraComponent* C=A->GetNiagaraComponent();if(!Live(C)||C->GetOwner()!=A||C->GetOuter()!=A)return W.Fail(TEXT("niagara_preview_sample_component_invalid"),TEXT("Spawned actor lacks exact component"));W.Component=C;W.ComponentClass=C->GetClass();W.ComponentPath=C->GetPathName();W.ComponentEffectivePackage=C->GetPackage();W.ComponentOutermost=C->GetOutermost();W.SpawnIdentityIndex=CurrentOwned;W.ComponentCaptured=true;W.AssetAssigned=false;
  if(!W.SDK(TEXT("Sample.SetAsset"),true,[&]{C->SetAsset(W.System.Get(),!Preserve);}))return false;if(C->GetAsset()!=W.System.Get())return W.Fail(TEXT("niagara_preview_sample_asset_mismatch"),TEXT("SetAsset did not preserve requested System"));W.AssetAssigned=true;Overrides.Reset();if(!Lab::ApplyOverrides(W,Overrides))return false;
  if(Single)
  {
   if(!W.SDK(TEXT("Preview.Activate"),true,[&]{C->Activate(true);}))return false;
   PreviewOriginalSolo=C->GetForceSolo();PreviewOriginalTick=C->IsComponentTickEnabled();PreviewOriginalPaused=C->IsPaused();W.Prefix->SetBoolField(TEXT("preview_original_force_solo"),PreviewOriginalSolo);W.Prefix->SetBoolField(TEXT("preview_original_natural_tick"),PreviewOriginalTick);W.Prefix->SetBoolField(TEXT("preview_original_paused"),PreviewOriginalPaused);
   if(!W.SDK(TEXT("Preview.SetForceSolo"),true,[&]{C->SetForceSolo(true);})||!W.SDK(TEXT("Preview.DisableNaturalTick"),true,[&]{C->SetComponentTickEnabled(false);})||!W.SDK(TEXT("Preview.Unpause"),true,[&]{C->SetPaused(false);}))return false;PreviewWarming=true;
  }
  else{
  if(!W.SDK(TEXT("Sample.SetForceSolo"),true,[&]{C->SetForceSolo(true);})||!W.SDK(TEXT("Sample.Activate"),true,[&]{C->Activate(true);})||!W.SDK(TEXT("Sample.DisableNaturalTick"),true,[&]{C->SetComponentTickEnabled(false);})||!W.SDK(TEXT("Sample.Unpause"),true,[&]{C->SetPaused(false);}))return false;
  }
  CurrentSample->SetArrayField(TEXT("user_parameter_overrides"),Overrides);CurrentSample->SetArrayField(TEXT("spawn_location"),VJson(Loc));CurrentSample->SetArrayField(TEXT("spawn_scale"),VJson(Scale));W.L->SetArrayField(TEXT("spawn_location"),VJson(Loc));W.L->SetArrayField(TEXT("spawn_scale"),VJson(Scale));W.L->SetBoolField(TEXT("preserve_override_parameters"),Preserve);bool Auto=true;if(!W.Bool(TEXT("auto_frame"),true,Auto))return false;W.L->SetBoolField(TEXT("auto_frame"),Auto);Stage=11;return W.Admit();
 }
 bool CompleteSample()
 {
  auto Parent=W.L;auto Capture=MakeShared<FJsonObject>();W.L=Capture;W.Prefix->RemoveField(TEXT("capture_native_receipt"));bool Good=Lab::Capture(W);W.L=Parent;const TSharedPtr<FJsonObject>* CR=nullptr;if(W.Prefix->TryGetObjectField(TEXT("capture_native_receipt"),CR)&&CR)CurrentSample->SetObjectField(TEXT("native_capture_receipt"),*CR);
  CurrentSample->SetObjectField(TEXT("preview"),Capture);CurrentSample->SetBoolField(TEXT("success"),Good);CurrentSample->SetNumberField(TEXT("manual_step_calls_returned"),TicksDone);CurrentSample->SetBoolField(TEXT("GPU_simulation_completion_verified"),false);if(Good)++SuccessCount;else{++FailureCount;CurrentSample->SetStringField(TEXT("error"),W.R.Error);}
  if(!W.Admit())return false;TArray<FString> Deleted;if(CleanupEach||!Good){TArray<int32> Only{CurrentOwned};if(!Lab::CleanupActors(W,Deleted,Good?nullptr:&Only))return false;CurrentSample->SetArrayField(TEXT("cleanup_after_labels"),Lab::Labels(Deleted));}
  W.ComponentCaptured=false;W.Component.Reset();W.Spawned.Reset();CurrentOwned=INDEX_NONE;W.P=Input;++SampleIndex;Stage=10;
  if(!Good){ // A safe bounded sample failure can continue; source/context identity loss never resumes.
   if(W.ScopeLost)return false;W.Prefix->SetStringField(TEXT("last_sample_error"),W.R.Error);W.R.Error.Empty();W.R.ErrorCode.Empty();W.FirstStage.Empty();
  }return W.Admit();
 }
 bool CompleteSamples()
 {
  W.P=Input;if(CleanupAll&&!Lab::CleanupActors(W,FinalCleanup))return false;W.L->SetArrayField(TEXT("cleanup_after_all_labels"),Lab::Labels(FinalCleanup));W.L->SetArrayField(TEXT("samples"),SampleRows);W.L->SetNumberField(TEXT("sample_count"),SampleRows.Num());W.L->SetNumberField(TEXT("success_count"),SuccessCount);W.L->SetNumberField(TEXT("failure_count"),FailureCount);W.L->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Started);W.L->SetBoolField(TEXT("map_dirty_after_sampling"),W.WorldPackage->IsDirty());W.L->SetStringField(TEXT("safety_note"),TEXT("Original ordered warmup/view sequence; one public manual step per GT frame, no map reload/save. PNG/capture SDK may synchronously flush; returned steps and images do not prove GPU completion."));Stage=3;if(FailureCount||!SuccessCount)return W.Fail(TEXT("niagara_preview_samples_partial_failure"),TEXT("One or more samples failed; completed files and actual prefix retained"));return W.Admit();
 }
 void Stop(const TCHAR* Code,const FString& Reason)
 {
  if(Terminal)return;W.Fail(Code,Reason);if(Kind==EKind::Sample){for(int32 I=SampleRows.Num();I<Warmups.Num()*Views.Num();++I){auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("sample_index"),I);Row->SetNumberField(TEXT("warmup_time"),Warmups[I/Views.Num()]);Row->SetNumberField(TEXT("view"),Views[I%Views.Num()]);Row->SetBoolField(TEXT("success"),false);Row->SetBoolField(TEXT("skipped"),true);Row->SetStringField(TEXT("error"),Reason);SampleRows.Add(MakeShared<FJsonValueObject>(Row));}W.L->SetArrayField(TEXT("samples"),SampleRows);W.L->SetNumberField(TEXT("sample_count"),SampleRows.Num());W.L->SetNumberField(TEXT("success_count"),SuccessCount);W.L->SetNumberField(TEXT("failure_count"),SampleRows.Num()-SuccessCount);W.L->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Started);}W.R.Data->SetBoolField(TEXT("cancel_requested"),CancelRequested);W.R.Data->SetStringField(TEXT("cancel_reason"),CancelReason);
  if(Kind==EKind::Swap&&SwapTouched&&!Restored){if(!RestoreSwap())W.Prefix->SetBoolField(TEXT("borrowed_restore_incomplete"),true);}
  if((Kind==EKind::Sample||Kind==EKind::Preview)&&CurrentOwned!=INDEX_NONE&&W.Admit()){TArray<int32> Only{CurrentOwned};TArray<FString> Gone;if(!Lab::CleanupActors(W,Gone,&Only))W.Prefix->SetBoolField(TEXT("owned_sample_cleanup_incomplete"),true);}
  if(Kind==EKind::Open){auto P=UI.Pin();if(P){P->Settling=false;P->Playing=false;P->Renderable=false;P->Playback=TEXT("interrupted");if(GlobalsSafe()&&P->Same()&&P->Client)P->Client->SetRealtime(false);}}
  W.R.Data->SetBoolField(TEXT("remaining_samples_skipped"),Kind==EKind::Sample&&SampleIndex<Warmups.Num()*Views.Num());Terminal=true;Publish(true);
 }
 bool Step()
 {
  if(Terminal)return false;if(InTick)return true;TGuardValue<bool> Reentry(InTick,true);
  if(!GlobalsSafe()||!W.Admit()){Stop(TEXT("niagara_preview_workflow_context_lost"),TEXT("Unsafe context before execution pins; metadata-only failure/quarantine"));return false;}FPins Pins(*this);
  if(CancelRequested){Stop(TEXT("niagara_preview_workflow_cancelled"),CancelReason);return false;}if(FPlatformTime::Seconds()>Deadline){Stop(TEXT("niagara_preview_workflow_deadline"),TEXT("Internal continuation deadline reached; no replay"));return false;}
  if(!W.Admit()){Stop(TEXT("niagara_preview_workflow_context_lost"),TEXT("Captured context is unsafe or changed; no replacement target or unsafe restore"));return false;}
  bool Good=true;if(Stage==0){Good=Kind==EKind::Open?PrepareOpen():Kind==EKind::Swap?PrepareSwap():Kind==EKind::Preview?PreparePreview():PrepareSamples();}
  else if(Stage==1){UNiagaraComponent* C=Kind==EKind::Swap?Borrowed.Get():UI.Pin()->Component;if(TicksDone<TickCount)Good=Advance(C);else if(Kind==EKind::Open)Good=CompleteOpen();else{Good=Projection(TEXT("after_swap"))&&RestoreSwap();if(Good)Stage=3;}}
  else if(Stage==10||Stage==20)Good=BeginSample();else if(Stage==11){if(TicksDone<TickCount)Good=Advance(W.Component.Get());else Good=Kind==EKind::Preview?CompletePreview():CompleteSample();}else if(Stage==13)Good=CompleteSamples();
  if(!Good){Stop(TEXT("niagara_preview_workflow_incomplete"),TEXT("Workflow stage failed; actual partial prefix retained"));return false;}
  if(Stage==3){Terminal=true;Publish(true);return false;}Publish(false);return true;
 }
};
namespace
{
TMap<UCubelessNiagaraPreviewWorkflowJob*,TSharedPtr<FJobState>> Jobs;
bool TickJob(TWeakPtr<FJobState> Weak)
{
 auto S=Weak.Pin();if(!S)return false;if(GZExecuting||S->LastFrame==GFrameCounter)return true;TGuardValue<bool> Busy(GZExecuting,true);S->LastFrame=GFrameCounter;DrainRetiredPlayers();bool More=S->Step();if(!More){S->Handle.Reset();if(GlobalsSafe()){S->JobPin.Reset();Jobs.Remove(S->Job.Get());}else{S->W.R.Data->SetBoolField(TEXT("unsafe_terminal_publication_quarantined"),true);}}return More;
}
UCubelessNiagaraPreviewWorkflowJob* Start(const FString& Text,FJobState::EKind Kind,const TCHAR* Op)
{
 if(!GlobalsSafe()||GZExecuting||GZStopping)return nullptr;EnsureEditorExitHook();TGuardValue<bool> Busy(GZExecuting,true);auto S=MakeShared<FJobState>(Kind,Op);auto* J=NewObject<UCubelessNiagaraPreviewWorkflowJob>(GetTransientPackage(),NAME_None,RF_Transient);if(!J)return nullptr;
 Jobs.Add(J,S);if(!S->Init(Text,J)){S->Terminal=true;S->Publish(true);S->JobPin.Reset();Jobs.Remove(J);return J;}
 // Domain-specific admission prevents simultaneous use of one borrowed world/UI. It does not own a global server slot.
 for(const auto& KV:Jobs)if(KV.Key!=J&&!KV.Value->Terminal&&((Kind==FJobState::EKind::Open&&KV.Value->Kind==Kind)||(Kind!=FJobState::EKind::Open&&KV.Value->Kind!=FJobState::EKind::Open&&KV.Value->W.World==S->W.World))){S->W.Fail(TEXT("niagara_preview_workflow_busy"),TEXT("An existing preview workflow owns this world/UI continuation"));S->Terminal=true;S->Publish(true);S->JobPin.Reset();Jobs.Remove(J);return J;}
 S->W.R.Stage=TEXT("queued");S->Publish(false);TWeakPtr<FJobState> Weak=S;S->Handle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float){return TickJob(Weak);}));return J;
}
FString SyncReject(const FString& Text,const TCHAR* Op,int32 Kind)
{
 Lab::FWork W(Op);if(GZExecuting){W.Fail(TEXT("niagara_preview_workflow_reentrant_call"),TEXT("Nested native workflow rejected"));return W.Finish();}TGuardValue<bool> Busy(GZExecuting,true);if(!W.Init(Text))return W.Finish();
 if(Kind==0){FString Path;W.P->TryGetStringField(TEXT("system_path"),Path);if(Path.IsEmpty()){if(ShowPlayer(W)&&Player){W.L=Player->Metadata();W.R.Data->SetObjectField(TEXT("legacy_result"),W.L);W.L->SetBoolField(TEXT("opened"),true);}return W.Finish();}}
 bool Zero=false;
 if(Kind==1){double D=.35;if(!W.Number(TEXT("advance_time"),.35,D))return W.Finish();Zero=FMath::Clamp(D,0.,5.)==0;}
 if(Kind==2){const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(W.P->TryGetArrayField(TEXT("warmup_times"),A)&&A&&!A->IsEmpty()){Zero=true;for(int32 I=0;I<FMath::Min(A->Num(),12);++I)if(FMath::Clamp((*A)[I]->AsNumber(),0.,5.)!=0)Zero=false;}}
 if(Zero){FJobState Z(Kind==1?FJobState::EKind::Swap:FJobState::EKind::Sample,Op);if(!Z.Init(Text,nullptr))return Z.W.Finish();for(int32 I=0;I<256&&!Z.Terminal;++I)Z.Step();if(!Z.Terminal)Z.Stop(TEXT("niagara_preview_zero_step_bound"),TEXT("Zero-only synchronous stage bound exceeded; no replay"));return Z.FinalSnapshot;}
 W.R.Data->SetBoolField(TEXT("continuation_connected"),false);W.R.Data->SetBoolField(TEXT("named_job_supported"),true);W.Fail(TEXT("niagara_preview_workflow_continuation_required"),TEXT("Direct/ordinary RawBatch continuation is not connected. Named Job preserves positive/default warmup and exact restoration; no first writer or synthetic completion occurs."));return W.Finish();
}
}
UCubelessNiagaraPreviewWorkflowJob* StartOpenPlayer(const FString& S){return Start(S,FJobState::EKind::Open,TEXT("open_niagara_preview_player"));}
UCubelessNiagaraPreviewWorkflowJob* StartComponentSwap(const FString& S){return Start(S,FJobState::EKind::Swap,TEXT("preview_niagara_component_asset_swap"));}
UCubelessNiagaraPreviewWorkflowJob* StartSampleLab(const FString& S){return Start(S,FJobState::EKind::Sample,TEXT("sample_niagara_system_in_preview_lab"));}
UCubelessNiagaraPreviewWorkflowJob* StartPreviewLab(const FString& S){return Start(S,FJobState::EKind::Preview,TEXT("preview_niagara_system_in_preview_lab"));}
FString ReadRetainedSnapshot(const UCubelessNiagaraPreviewWorkflowJob* J){if(!IsInGameThread())return FString();auto* S=Jobs.Find(const_cast<UCubelessNiagaraPreviewWorkflowJob*>(J));return S?(*S)->FinalSnapshot:FString();}
bool Cancel(UCubelessNiagaraPreviewWorkflowJob* J,const FString& Reason)
{
 auto* Found=Jobs.Find(J);if(!IsInGameThread()||!Found||(*Found)->Terminal)return false;auto S=*Found;S->CancelRequested=true;S->CancelReason=SafeText(Reason,4096)?Reason.Left(4096):TEXT("cancel_requested");return true;
}
void Shutdown(const FString& Reason)
{
 if(EditorPreExitHandle.IsValid())FEditorDelegates::OnEditorPreExit.Remove(EditorPreExitHandle);EditorPreExitHandle.Reset();
 auto SceneCount=[](){int32 N=Player&&Player->Scene?1:0;for(const auto& P:RetiredPlayers)if(P->Scene)++N;for(const auto& P:*UnresolvedOwnedUI)if(P->Scene)++N;return N;};
 UE_LOG(LogTemp,Log,TEXT("GZ.Shutdown begin reason=%s normal_preexit=%d jobs=%d scene_handles=%d owned_window=%d in_native_call=%d"),*Reason.Left(128),GZNormalEditorExit?1:0,Jobs.Num(),SceneCount(),Player.IsValid()?1:0,GZExecuting?1:0);
 auto LogEnd=[&](){UE_LOG(LogTemp,Log,TEXT("GZ.Shutdown end stopping=%d jobs_retained=%d scene_handles_retained=%d retired_UI=%d unresolved_UI=%d preexit_handle=%d"),GZStopping?1:0,Jobs.Num(),SceneCount(),RetiredPlayers.Num(),UnresolvedOwnedUI->Num(),EditorPreExitHandle.IsValid()?1:0);};
 if(GZStopping){LogEnd();return;}GZStopping=true;
 if(RetireHandle.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(RetireHandle);RetireHandle.Reset();
 TArray<TSharedPtr<FJobState>> Active;Jobs.GenerateValueArray(Active);for(auto S:Active){if(S->Handle.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(S->Handle);S->Handle.Reset();S->CancelRequested=true;S->CancelReason=Reason;}
 for(auto S:Active){S->CleanupMode=true;S->W.ExitCleanup=GZNormalEditorExit;if(OwnedCleanupSafe()&&!S->InTick&&S->W.Admit()){FJobState::FPins Hold(*S);S->Stop(TEXT("niagara_preview_workflow_shutdown"),Reason);}else{S->W.R.Data->SetBoolField(TEXT("shutdown_restore_and_terminal_publication_unresolved"),true);S->W.R.bChangeStateKnown=false;if(!S->InTick)S->Stop(TEXT("niagara_preview_workflow_shutdown_unsafe"),Reason);}
  S->CleanupMode=false;S->W.ExitCleanup=false;if(OwnedCleanupSafe()&&!S->InTick&&!S->PublicationQuarantined){S->JobPin.Reset();Jobs.Remove(S->Job.Get());}}
 auto P=Player;if(P){RetirePlayer(P);if(OwnedCleanupSafe()&&!GZExecuting&&FSlateApplication::IsInitialized()&&P->Window){P->Window->SetOnWindowClosed(FOnWindowClosed());if(OwnedCleanupSafe())FSlateApplication::Get().RequestDestroyWindow(P->Window.ToSharedRef());}}
 if(OwnedCleanupSafe()&&!GZExecuting)DrainRetiredPlayers();else if(GZExecuting)LastPlayerState->SetBoolField(TEXT("owned_UI_teardown_inflight_unresolved"),true);
 LogEnd();
 // Unsafe owner/Job references are retained as unresolved. Never reset PreviewScene on GC/exit or claim hot-unload proof.
}
FString OpenPlayer(const FString& S){return SyncReject(S,TEXT("open_niagara_preview_player"),0);}
FString PreviewComponentSwap(const FString& S){return SyncReject(S,TEXT("preview_niagara_component_asset_swap"),1);}
FString SampleLab(const FString& S){return SyncReject(S,TEXT("sample_niagara_system_in_preview_lab"),2);}
FString GetPlayerState(const FString& S)
{
 Variable::FResult R;R.Compiled=false;TSharedPtr<FJsonObject> Params;if(!Lab::Parse(S,Params)){R.ErrorCode=TEXT("niagara_preview_workflow_params_invalid");R.Error=TEXT("Bounded finite object required");return Operations::EncodeVariableResult(R,TEXT("get_niagara_preview_player_state"));}
 auto M=Player?Player->Metadata():LastPlayerState;auto O=MakeShared<FJsonObject>();O->Values=M->Values;if(!O->HasField(TEXT("window_open"))){O->SetBoolField(TEXT("window_open"),false);O->SetStringField(TEXT("player_mode"),TEXT("Niagara Preview Player"));O->SetBoolField(TEXT("last_preview_renderable"),false);O->SetNumberField(TEXT("drop_count"),0);for(const TCHAR* K:{TEXT("last_drop_kind"),TEXT("last_display_name"),TEXT("last_object_path"),TEXT("last_class_name"),TEXT("analysis_summary")})O->SetStringField(K,TEXT(""));O->SetStringField(TEXT("playback_state"),TEXT("none"));O->SetBoolField(TEXT("looping"),true);}O->SetBoolField(TEXT("success"),true);R.Data->SetObjectField(TEXT("legacy_result"),O);R.Data->SetBoolField(TEXT("metadata_only_no_UObject_getters"),true);R.bSuccess=true;R.bChangeStateKnown=true;R.bPostconditionPassed=true;R.Stage=TEXT("complete");return Operations::EncodeVariableResult(R,TEXT("get_niagara_preview_player_state"));
}
} // namespace Cubeless::UE58::NiagaraPreviewWorkflows
