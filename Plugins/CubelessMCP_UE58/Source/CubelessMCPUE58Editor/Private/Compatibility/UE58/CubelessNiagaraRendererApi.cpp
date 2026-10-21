// GE dedicated renderer APIs. Source candidate; build and Engine behavior are not yet verified.
// GE dedicated original renderer lifecycle through public UE5.8 Editor APIs.
#include "Compatibility/UE58/CubelessNiagaraRendererApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Compatibility/UE58/CubelessNiagaraScratchUserRecipeAsyncApi.h"
#include "CubelessNiagaraScratchUserRecipeJob.h"
#include "AssetCompilingManager.h"
#include "Containers/Ticker.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "UObject/UObjectGlobals.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraDecalRendererProperties.h"
#include "NiagaraVolumeRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraParameterBinding.h"
#include "NiagaraTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::NiagaraScratchUserRecipeAsync { bool CanPublishJobObject(); }
namespace Cubeless::UE58::NiagaraRenderers
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false;
bool HasCleanupLease();
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O){FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));}return S;}
FString NormalizeNiagaraObjectPathForLoad(const FString&);
struct FWork;
TSharedPtr<FJsonObject> Project(FWork& W);
struct FIdentity
{
    TStrongObjectPtr<UObject> Strong{nullptr};TWeakObjectPtr<UObject> Weak,Outer;TWeakObjectPtr<UPackage> Package,Effective;TWeakObjectPtr<UClass> Class;FString Path;
    explicit FIdentity(UObject* O):Strong(O),Weak(O),Outer(O->GetOuter()),Package(O->GetOutermost()),Effective(O->GetPackage()),Class(O->GetClass()),Path(O->GetPathName()){}
    bool Exact(bool Deferred=false)const{UObject* O=Weak.Get();return Live(O)&&Live(Package.Get())&&Live(Class.Get())&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&(!Deferred||(Live(Effective.Get())&&O->GetPackage()==Effective.Get()))&&O->GetClass()==Class.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}
};
struct FEmitterIdentity{FGuid Handle,Version;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;FString Name;int32 Index=INDEX_NONE;TArray<TWeakObjectPtr<UNiagaraRendererProperties>> Slots;};
struct FCleanupScriptRoute{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraGraph> Graph;FGuid Version;};
struct FWork
{
    bool RoutingCaptured=false,DeferredMode=false,CancelRequested=false,WaitDeferred=false,WaitDone=false,WaitIdle=false,WaitTimedOut=false;
    double WaitStarted=0,WaitNext=0,WaitTimeout=10,WaitPoll=.1;int32 WaitIterations=0;
    TWeakObjectPtr<UEditorEngine> CapturedEditor;TWeakObjectPtr<UNiagaraScript> SpawnScript,UpdateScript;
    TArray<FCleanupScriptRoute> CleanupRoutes;TArray<TWeakObjectPtr<UNiagaraScriptSourceBase>> EmitterSources;TArray<TArray<TWeakObjectPtr<UNiagaraScript>>> EmitterScripts;
    TFunction<TSharedPtr<FJsonObject>()> CleanupTail;TSharedPtr<FJsonObject> CleanupWait,DeferredShape;
    bool CaptureRouting();bool ValidateDeferredState(const TCHAR* Stage);void BeginWait(const TSharedPtr<FJsonObject>& Result,double Timeout,double Poll);bool AdvanceWait();
    FResult R;bool Writer=false,WorldCaptured=false;FString FailedStage,LastDispatch;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UNiagaraSystem> SystemWeak;FString SystemPath,PackagePath;
    TArray<TUniquePtr<FIdentity>> Objects;TArray<TStrongObjectPtr<UPackage>> Packages;TArray<FEmitterIdentity> Emitters;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>(),SavePrefix=MakeShared<FJsonObject>(),CompilePrefix=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> Before;
    explicit FWork(bool W):Writer(W){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetObjectField(TEXT("actual_save_prefix"),SavePrefix);R.Data->SetObjectField(TEXT("actual_compile_prefix"),CompilePrefix);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("actual_render_completion_verified"),false);R.Data->SetBoolField(TEXT("wrapper_sleep_or_forced_flush"),false);R.Data->SetBoolField(TEXT("Cubeless_runtime_asset_dependency_allowed"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    bool Admit(const FString& Stage,bool ObserveFailure=false)
    {
        R.Stage=Stage;if(!R.Error.IsEmpty()&&!ObserveFailure){return false;}
        if(DeferredMode&&(CancelRequested||NiagaraScratchUserRecipeAsync::IsStopping()))return Fail(TEXT("niagara_renderer_cleanup_cancelled"),TEXT("Concrete cleanup stopped; no later compiler processing, save or replay."));
        if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||!GEditor||IsGarbageCollecting()||UE::IsSavingPackage()){return Fail(TEXT("niagara_renderer_context_unavailable"),TEXT("Live Editor game thread, no GC/exit/package save required; no replay."));}
        if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_renderer_editor_changed"),TEXT("Captured Editor changed before any context getter."));
        if(DeferredMode&&GEditor->IsTransactionActive())return Fail(TEXT("niagara_renderer_outer_transaction_unsupported"),TEXT("A live outer transaction cannot cross deferred cleanup frames."));
        if(Writer&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld)){return Fail(TEXT("niagara_renderer_play_session_active"),TEXT("Structural renderer authoring requires stopped PIE/SIE."));}
        UWorld* Current=GEditor->GetEditorWorldContext().World();if(!Live(Current)||Current->bIsTearingDown){return Fail(TEXT("niagara_renderer_world_unavailable"),TEXT("Editor world unavailable/tearing down; UObject projection skipped."));}
        if(!WorldCaptured){World=Current;WorldCaptured=true;if(DeferredMode)CapturedEditor=GEditor;}else if(!World.IsValid()||World.Get()!=Current){return Fail(TEXT("niagara_renderer_world_changed"),TEXT("Editor world changed across SDK dispatch."));}
        for(const auto& O:Objects){if(!O->Exact(DeferredMode)){return Fail(TEXT("niagara_renderer_object_identity_changed"),TEXT("Captured object/class/outer/package/path changed; replacement not adopted."));}}
        if(!SystemPath.IsEmpty())
        {
            UNiagaraSystem* S=SystemWeak.Get();if(!Live(S)||!S->GetOutermost()->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)){return Fail(TEXT("niagara_renderer_system_identity_changed"),TEXT("Captured system package changed."));}
            const auto& H=S->GetEmitterHandles();if(H.Num()!=Emitters.Num()){return Fail(TEXT("niagara_renderer_emitter_topology_changed"),TEXT("Emitter count changed across SDK dispatch."));}
            for(const auto& E:Emitters){if(!H.IsValidIndex(E.Index)||H[E.Index].GetId()!=E.Handle||!H[E.Index].GetName().ToString().Equals(E.Name,ESearchCase::CaseSensitive)||H[E.Index].GetEmitterData()!=E.Data){return Fail(TEXT("niagara_renderer_emitter_identity_changed"),TEXT("Emitter handle/versioned data changed; old data not reused."));}if(E.Data&&(!E.Emitter.IsValid()||H[E.Index].GetInstance().Emitter!=E.Emitter.Get()||E.Emitter->GetEmitterData(E.Version)!=E.Data||E.Data->Version.VersionGuid!=E.Version)){return Fail(TEXT("niagara_renderer_version_identity_changed"),TEXT("Captured editable emitter/version changed."));}if(E.Data){const auto& Actual=E.Data->GetRenderers();if(Actual.Num()!=E.Slots.Num()){return Fail(TEXT("niagara_renderer_topology_changed"),TEXT("Physical renderer count changed outside the admitted Add/Remove."));}for(int32 I=0;I<Actual.Num();++I){if(Actual[I]!=E.Slots[I].Get()){return Fail(TEXT("niagara_renderer_topology_changed"),TEXT("Physical renderer slot identity/order changed across SDK dispatch."));}}}}
        }
        if(DeferredMode&&RoutingCaptured&&SystemWeak.IsValid())
        {auto* S=SystemWeak.Get();if(S->GetSystemSpawnScript()!=SpawnScript.Get()||S->GetSystemUpdateScript()!=UpdateScript.Get())return Fail(TEXT("niagara_renderer_script_routing_changed"),TEXT("System spawn/update Script role changed."));
         if(EmitterSources.Num()!=Emitters.Num()||EmitterScripts.Num()!=Emitters.Num())return Fail(TEXT("niagara_renderer_script_routing_changed"),TEXT("Incomplete captured emitter Script roles."));
         for(int32 I=0;I<Emitters.Num();++I){auto* D=Emitters[I].Data;if(D){if(D->GraphSource!=EmitterSources[I].Get())return Fail(TEXT("niagara_renderer_script_routing_changed"),TEXT("Emitter graph source changed."));TArray<UNiagaraScript*> Actual;D->GetScripts(Actual,false,false);if(Actual.Num()!=EmitterScripts[I].Num())return Fail(TEXT("niagara_renderer_script_routing_changed"),TEXT("Emitter Script count changed."));for(int32 J=0;J<Actual.Num();++J)if(Actual[J]!=EmitterScripts[I][J].Get())return Fail(TEXT("niagara_renderer_script_routing_changed"),TEXT("Emitter Script role/order changed."));}}
         for(const auto& E:CleanupRoutes){auto* Script=E.Script.Get();if(!Live(Script)||Script->GetLatestSource()!=E.Source.Get()||Script->GetExposedVersion().VersionGuid!=E.Version)return Fail(TEXT("niagara_renderer_script_routing_changed"),TEXT("Captured Script version/source changed."));if(auto* Source=Cast<UNiagaraScriptSource>(E.Source.Get());Source&&Source->NodeGraph!=E.Graph.Get())return Fail(TEXT("niagara_renderer_script_routing_changed"),TEXT("Captured Source NodeGraph role changed."));}
        }
        return true;
    }
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage)){return false;}LastDispatch=Stage;R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(Mutation){Prefix->SetBoolField(Stage,true);if(DeferredMode){R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),true);R.Data->SetBoolField(TEXT("transient_creation_only"),false);}}return true;}
    bool Returned(const TCHAR* Stage){const bool OK=Admit(FString(TEXT("after_"))+Stage);if(OK&&Before&&Live(SystemWeak.Get())){bool DirtyBefore=false;Before->TryGetBoolField(TEXT("dirty"),DirtyBefore);R.bChanged|=SystemWeak->GetOutermost()->IsDirty()!=DirtyBefore;}return OK;}
    bool Pin(UObject* O){if(!Live(O)){return Fail(TEXT("niagara_renderer_object_unavailable"),TEXT("Expected live loaded Engine object."));}for(const auto& I:Objects){if(I->Weak.Get()==O){return I->Exact(DeferredMode);}}Packages.Emplace(O->GetOutermost());Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool AdmitOwnedEmitter(UNiagaraEmitter* Emitter,const UObject* Target=nullptr)
    {
        if(!Admit(TEXT("selected_emitter_ownership"))){return false;}
        UNiagaraSystem* S=SystemWeak.Get();
        if(!Live(S)||!Live(Emitter)||Emitter->GetOutermost()!=S->GetOutermost()||!Emitter->IsIn(S))
        {return Fail(TEXT("niagara_renderer_external_emitter_ownership"),TEXT("Selected emitter must be internally owned by the exact requested system/package before authoring; allow_source_edit does not authorize an external emitter asset."));}
        if(Target&&(!Live(Target)||Target->GetOutermost()!=S->GetOutermost()||!Target->IsIn(Emitter)))
        {return Fail(TEXT("niagara_renderer_external_renderer_ownership"),TEXT("Selected renderer must be internally owned by the exact selected emitter/system package before authoring."));}
        R.Data->SetBoolField(TEXT("selected_emitter_internal_ownership_verified"),true);
        R.Data->SetStringField(TEXT("authoring_emitter_path"),Emitter->GetPathName());
        R.Data->SetStringField(TEXT("authoring_ownership_scope"),TEXT("Explicit writes are limited to the selected emitter and renderer internally owned by the requested system package. Read/NoOp/dry-run projections are permitted; external assets are not duplicated or implicitly edited."));
        return true;
    }
    bool AdmitOwnedEmitterAt(int32 Index,const UObject* Target)
    {
        if(!Admit(TEXT("selected_emitter_index_ownership"))){return false;}
        UNiagaraSystem* S=SystemWeak.Get();
        if(!Live(S)||!S->GetEmitterHandles().IsValidIndex(Index)){return Fail(TEXT("niagara_renderer_authoring_emitter_unresolved"),TEXT("Captured selected emitter index must remain available before ownership admission."));}
        return AdmitOwnedEmitter(S->GetEmitterHandles()[Index].GetInstance().Emitter,Target);
    }
    template<typename T>T* Load(const FString& Path,const TCHAR* Stage)
    {
        const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);if(DeferredMode&&(Path.Len()>=NAME_SIZE||Expected.Len()>=NAME_SIZE)){Fail(TEXT("niagara_renderer_path_name_bound"),TEXT("Raw and expanded object paths must fit NAME_SIZE before load."));return nullptr;}if(!SDK(Stage)){return nullptr;}T* O=LoadObject<T>(nullptr,*Expected);R.Data->SetBoolField(FString(Stage)+TEXT("_returned_nonnull"),O!=nullptr);if(!Returned(Stage)){return nullptr;}if(!O){return nullptr;}if(!Live(O)||!O->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Pin(O)){Fail(TEXT("niagara_renderer_loaded_identity_unexpected"),TEXT("Load returned unexpected renamed/replacement object."));return nullptr;}return O;
    }
    UNiagaraSystem* LoadSystem(const FString& Path)
    {
        auto S=Load<UNiagaraSystem>(Path,TEXT("load_system"));if(!S){return nullptr;}SystemWeak=S;SystemPath=S->GetPathName();PackagePath=S->GetOutermost()->GetName();const auto& H=S->GetEmitterHandles();if(H.Num()>4096){Fail(TEXT("niagara_renderer_inventory_bound"),TEXT("Emitter inventory exceeds bounded operation."));return nullptr;}
        for(int32 I=0;I<H.Num();++I){FEmitterIdentity E;E.Index=I;E.Handle=H[I].GetId();E.Name=H[I].GetName().ToString();E.Data=H[I].GetEmitterData();if(E.Data){E.Version=E.Data->Version.VersionGuid;E.Emitter=H[I].GetInstance().Emitter;if(!Pin(E.Emitter.Get())){return nullptr;}if(E.Data->GetRenderers().Num()>16384){Fail(TEXT("niagara_renderer_inventory_bound"),TEXT("Renderer inventory exceeds bounded operation."));return nullptr;}for(auto* O:E.Data->GetRenderers()){E.Slots.Add(O);if(O&&!Pin(O)){return nullptr;}}}Emitters.Add(E);}if(DeferredMode&&!CaptureRouting())return nullptr;Before=Project(*this);return R.Error.IsEmpty()?S:nullptr;
    }
    template<typename T>T* NewRenderer(UNiagaraEmitter* Outer)
    {
        if(!SDK(TEXT("new_renderer"),true)){return nullptr;}T* O=NewObject<T>(Outer,NAME_None,RF_Transactional);Prefix->SetBoolField(TEXT("new_renderer_returned_nonnull"),O!=nullptr);R.bChanged|=O!=nullptr;if(!Returned(TEXT("new_renderer"))){return nullptr;}if(Live(O)){R.Data->SetStringField(TEXT("created_renderer_path"),O->GetPathName());}if(!Live(O)||O->GetOuter()!=Outer||O->GetClass()!=T::StaticClass()||!Pin(O)){Fail(TEXT("niagara_renderer_created_identity_unexpected"),TEXT("New renderer class/owner changed; residual retained."));return nullptr;}return O;
    }
    bool ChangeTopology(UNiagaraEmitter* Emitter,const FGuid& Version,UNiagaraRendererProperties* Renderer,bool Add)
    {
        const TCHAR* Stage=Add?TEXT("add_renderer"):TEXT("remove_renderer");
        if(!SDK(Stage,true)){return false;}
        FEmitterIdentity* Identity=Emitters.FindByPredicate([&](const FEmitterIdentity& E){return E.Emitter.Get()==Emitter&&E.Version==Version;});
        if(!Identity||!Live(Renderer)){return Fail(TEXT("niagara_renderer_topology_owner_missing"),TEXT("Exact editable emitter/renderer must remain captured before topology change."));}
        if(Add){Identity->Slots.Add(Renderer);}else{const int32 Index=Identity->Slots.IndexOfByPredicate([&](const auto& R){return R.Get()==Renderer;});if(Index==INDEX_NONE){return Fail(TEXT("niagara_renderer_topology_target_missing"),TEXT("Requested renderer detached before removal."));}Identity->Slots.RemoveAt(Index);}
        // The public SDK broadcasts OnRenderersChanged; only this exact next array is admitted.
        if(Add){Emitter->AddRenderer(Renderer,Version);}else{Emitter->RemoveRenderer(Renderer,Version);}
        Prefix->SetBoolField(FString(Stage)+TEXT("_SDK_returned"),true);R.bChanged=true;
        return Returned(Stage);
    }
    bool Save(UNiagaraSystem* S)
    {
        R.bSaveRequested=true;if(!SDK(TEXT("save_system"),true)){return false;}R.bSaveAttempted=true;SavePrefix->SetStringField(TEXT("requested_package"),PackagePath);const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(S,false);R.bSaved|=Saved;SavePrefix->SetBoolField(TEXT("SDK_save_returned"),true);SavePrefix->SetBoolField(TEXT("SDK_save_success"),Saved);SavePrefix->SetBoolField(TEXT("actual_saved_history"),R.bSaved);if(!Returned(TEXT("save_system"))){SavePrefix->SetBoolField(TEXT("post_save_projection_skipped"),true);return false;}SavePrefix->SetBoolField(TEXT("package_dirty_after"),S->GetOutermost()->IsDirty());if(Saved&&S->GetOutermost()->IsDirty()){Fail(TEXT("niagara_renderer_saved_dirty"),TEXT("Save true but package dirty; saved history retained."));return false;}return Saved;
    }
    bool Request(UNiagaraSystem* S,bool& Accepted)
    {
        R.bCompileRequested=true;if(!SDK(TEXT("request_compile"),true)){return false;}R.bCompileAttempted=true;R.Compiled.Reset();Accepted=S->RequestCompile(false);CompilePrefix->SetBoolField(TEXT("SDK_compile_request_returned"),true);CompilePrefix->SetBoolField(TEXT("SDK_compile_request_accepted"),Accepted);CompilePrefix->SetBoolField(TEXT("compile_completion_verified"),false);return Returned(TEXT("request_compile"));
    }
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty()){Fail(TEXT("niagara_renderer_request_failed"),Message);}auto E=MakeShared<FJsonObject>();E->SetBoolField(TEXT("success"),false);E->SetStringField(TEXT("error"),Message);return E;}
};
#define GE_STEP(Stage,Mutation,Expression) do{if(!W.SDK(TEXT(Stage),Mutation)){return W.Error(W.R.Error);}Expression;if(!W.Returned(TEXT(Stage))){return W.Error(W.R.Error);}}while(false)
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



FString NiagaraObjectPathOrEmpty(const UObject* Object)
{
    return Object ? Object->GetPathName() : FString();
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



FString NiagaraTypeName(const FNiagaraTypeDefinition& TypeDef){return TypeDef.GetName();}

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



TArray<TSharedPtr<FJsonValue>> NiagaraMeshPropertiesBaseArrayToJsonValues(const TArray<FNiagaraMeshRendererMeshPropertiesBase>& MeshProperties)
{
    TArray<TSharedPtr<FJsonValue>> MeshValues;
    for (const FNiagaraMeshRendererMeshPropertiesBase& MeshProperty : MeshProperties)
    {
        MeshValues.Add(MakeShared<FJsonValueObject>(NiagaraMeshPropertiesBaseToJsonObject(MeshProperty)));
    }
    return MeshValues;
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



FString NormalizeNiagaraEnumToken(FString Value)
{
    Value.TrimStartAndEndInline();
    Value.ToLowerInline();
    Value.ReplaceInline(TEXT("_"), TEXT(""));
    Value.ReplaceInline(TEXT("-"), TEXT(""));
    Value.ReplaceInline(TEXT(" "), TEXT(""));
    return Value;
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



bool TryParseJsonVector3Array(const TArray<TSharedPtr<FJsonValue>>* Arr, FVector& OutVector)
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



bool TryParseNiagaraMeshPivotOffsetSpaceJsonValue(
    const TSharedPtr<FJsonValue>& JsonValue,
    ENiagaraMeshPivotOffsetSpace& OutSpace,
    FString& OutError)
{
    if (!JsonValue.IsValid())
    {
        OutError = TEXT("pivot_offset_space must not be null");
        return false;
    }

    double NumberValue = 0.0;
    if (JsonValue->TryGetNumber(NumberValue))
    {
        if (!FMath::IsFinite(NumberValue) || !FMath::IsNearlyEqual(NumberValue, FMath::RoundToDouble(NumberValue), KINDA_SMALL_NUMBER))
        {
            OutError = TEXT("pivot_offset_space numeric value must be a finite integer");
            return false;
        }
        if(NumberValue<MIN_int32||NumberValue>MAX_int32){OutError=TEXT("pivot_offset_space outside safe int32 range");return false;}
        const int32 EnumValue = static_cast<int32>(FMath::RoundToDouble(NumberValue));
        if (const UEnum* PivotOffsetSpaceEnum = StaticEnum<ENiagaraMeshPivotOffsetSpace>())
        {
            const int32 EnumIndex = PivotOffsetSpaceEnum->GetIndexByValue(EnumValue);
            if (EnumIndex != INDEX_NONE && !PivotOffsetSpaceEnum->HasMetaData(TEXT("Hidden"), EnumIndex))
            {
                OutSpace = static_cast<ENiagaraMeshPivotOffsetSpace>(EnumValue);
                return true;
            }
        }
        OutError = FString::Printf(TEXT("Unknown pivot_offset_space enum value: %d"), EnumValue);
        return false;
    }

    FString SpaceString;
    if (!JsonValue->TryGetString(SpaceString))
    {
        OutError = TEXT("pivot_offset_space must be a string enum name or integer enum value");
        return false;
    }
    SpaceString.TrimStartAndEndInline();
    if (SpaceString.IsEmpty())
    {
        OutError = TEXT("pivot_offset_space cannot be empty");
        return false;
    }

    if (const UEnum* PivotOffsetSpaceEnum = StaticEnum<ENiagaraMeshPivotOffsetSpace>())
    {
        for (int32 Index = 0; Index < PivotOffsetSpaceEnum->NumEnums(); ++Index)
        {
            if (PivotOffsetSpaceEnum->HasMetaData(TEXT("Hidden"), Index))
            {
                continue;
            }
            const FString CandidateName = PivotOffsetSpaceEnum->GetNameStringByIndex(Index);
            if (CandidateName.Equals(SpaceString, ESearchCase::IgnoreCase))
            {
                OutSpace = static_cast<ENiagaraMeshPivotOffsetSpace>(PivotOffsetSpaceEnum->GetValueByIndex(Index));
                return true;
            }
        }
    }

    OutError = FString::Printf(TEXT("Unknown pivot_offset_space: %s"), *SpaceString);
    return false;
}




bool IsMCPDependencyReference(const FString& Value)
{
    FString S=Value.TrimStartAndEnd().ToLower();S.ReplaceInline(TEXT("\\"),TEXT("/"));for(const TCHAR* T:{TEXT("unrealmcp"),TEXT("mcpunreal"),TEXT("mcp_unreal"),TEXT("cubelesscontrolrigtools"),TEXT("cubelessmcp"),TEXT("cubeless_mcp"),TEXT("modelcontextprotocol"),TEXT("toolsetregistry"),TEXT("/engine/pythontypes")}){if(S.Contains(T)){return true;}}return false;
}
FString GetMCPDependencyReferenceError(const TCHAR* Field,const FString& Value){return FString::Printf(TEXT("Refusing MCP-only reference in %s: %s"),Field,*Value);}
UClass* MeshArrayClass(FWork& W)
{
    UClass* C=W.Load<UClass>(TEXT("/Script/Niagara.NiagaraDataInterfaceArrayMesh"),TEXT("load_array_mesh_class"));if(!C){return nullptr;}
    FArrayProperty* P=FindFProperty<FArrayProperty>(C,TEXT("MeshData"));FStructProperty* S=P?CastField<FStructProperty>(P->Inner):nullptr;
    if(!C->HasAnyClassFlags(CLASS_Native)||!C->IsChildOf(UNiagaraDataInterface::StaticClass())||!C->GetOutermost()->GetName().Equals(TEXT("/Script/Niagara"),ESearchCase::CaseSensitive)||!C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)||!P||P->GetOwnerStruct()!=C||P->ArrayDim!=1||!P->HasAllPropertyFlags(CPF_Edit|CPF_NativeAccessSpecifierPublic)||!S||!Live(S->Struct)||!S->Struct->GetPathName().Equals(TEXT("/Script/Niagara.NiagaraMeshRendererMeshPropertiesBase"),ESearchCase::CaseSensitive)||S->Struct->GetStructureSize()!=sizeof(FNiagaraMeshRendererMeshPropertiesBase)||S->Struct->GetMinAlignment()!=alignof(FNiagaraMeshRendererMeshPropertiesBase))
    {W.Fail(TEXT("niagara_renderer_array_mesh_binding_unavailable"),TEXT("Exact native Engine ArrayMesh/public MeshData/base struct exposure required; no Internal header or private write."));return nullptr;}
    W.R.Data->SetStringField(TEXT("array_mesh_binding_class"),C->GetPathName());W.R.Data->SetBoolField(TEXT("private_ArrayMesh_header_used"),false);return C;
}
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
TSharedPtr<FJsonObject> Project(FWork& W)
{
    auto O=MakeShared<FJsonObject>();if(!W.Admit(TEXT("current_projection"),true)){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);return O;}
    UNiagaraSystem* S=W.SystemWeak.Get();if(!Live(S)){O->SetBoolField(TEXT("system_loaded"),false);return O;}
    O->SetStringField(TEXT("system_path"),S->GetPathName());O->SetStringField(TEXT("package"),S->GetOutermost()->GetName());O->SetBoolField(TEXT("dirty"),S->GetOutermost()->IsDirty());TArray<TSharedPtr<FJsonValue>> Values;
    const auto& Handles=S->GetEmitterHandles();for(int32 I=0;I<Handles.Num();++I)
    {
        auto E=MakeShared<FJsonObject>();E->SetNumberField(TEXT("emitter_index"),I);E->SetStringField(TEXT("emitter_name"),Handles[I].GetName().ToString());E->SetBoolField(TEXT("emitter_enabled"),Handles[I].GetIsEnabled());TArray<TSharedPtr<FJsonValue>> Rows;
        if(auto* Data=Handles[I].GetEmitterData())
        {
            int32 J=0;for(const auto* P:Data->GetRenderers()){auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("renderer_index"),J++);V->SetBoolField(TEXT("valid"),Live(P));if(Live(P)){V->SetStringField(TEXT("path"),P->GetPathName());V->SetStringField(TEXT("class"),P->GetClass()->GetPathName());V->SetBoolField(TEXT("enabled"),P->GetIsEnabled());if(const auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(P)){V->SetStringField(TEXT("material"),GetPathNameSafe(Sprite->Material));V->SetStringField(TEXT("MIC"),GetPathNameSafe(Sprite->MICMaterial));V->SetNumberField(TEXT("alignment"),static_cast<int32>(Sprite->Alignment));V->SetNumberField(TEXT("facing"),static_cast<int32>(Sprite->FacingMode));V->SetNumberField(TEXT("sub_x"),Sprite->SubImageSize.X);V->SetNumberField(TEXT("sub_y"),Sprite->SubImageSize.Y);}else if(const auto* Mesh=Cast<UNiagaraMeshRendererProperties>(P)){V->SetStringField(TEXT("mesh_binding"),Mesh->MeshesBinding.ResolvedParameter.GetName().ToString());V->SetStringField(TEXT("mesh_index_binding"),Mesh->MeshIndexBinding.GetDataSetBindableVariable().GetName().ToString());V->SetBoolField(TEXT("override_materials"),Mesh->bOverrideMaterials);TArray<TSharedPtr<FJsonValue>> LocalMeshes;for(const auto& Entry:Mesh->Meshes){LocalMeshes.Add(MakeShared<FJsonValueObject>(NiagaraMeshPropertiesBaseToJsonObject(Entry)));}V->SetArrayField(TEXT("local_meshes"),LocalMeshes);V->SetStringField(TEXT("mesh_binding_alias"),Mesh->MeshesBinding.AliasedParameter.GetName().ToString());TArray<TSharedPtr<FJsonValue>> Mats;for(const auto& M:Mesh->OverrideMaterials){Mats.Add(MakeShared<FJsonValueString>(GetPathNameSafe(M.ExplicitMat)));}V->SetArrayField(TEXT("materials"),Mats);}else if(const auto* Ribbon=Cast<UNiagaraRibbonRendererProperties>(P)){V->SetStringField(TEXT("material"),GetPathNameSafe(Ribbon->Material));V->SetStringField(TEXT("MIC"),GetPathNameSafe(Ribbon->MICMaterial));}else if(const auto* Decal=Cast<UNiagaraDecalRendererProperties>(P)){V->SetStringField(TEXT("material"),GetPathNameSafe(Decal->Material));V->SetStringField(TEXT("MIC"),GetPathNameSafe(Decal->MICMaterial));}else if(const auto* Volume=Cast<UNiagaraVolumeRendererProperties>(P)){V->SetStringField(TEXT("material"),GetPathNameSafe(Volume->Material));V->SetStringField(TEXT("MIC"),GetPathNameSafe(Volume->MICMaterial));}}Rows.Add(MakeShared<FJsonValueObject>(V));}
        }
        else{E->SetStringField(TEXT("scope"),TEXT("No versioned editable emitter data; original UE5.6 authoring does not support Stateless."));}
        E->SetArrayField(TEXT("renderers"),Rows);Values.Add(MakeShared<FJsonValueObject>(E));
    }
    O->SetArrayField(TEXT("emitters"),Values);return O;
}

bool FWork::CaptureRouting()
{
    auto AddScript=[&](UNiagaraScript* Script)->bool
    {
        if(!Script)return true;for(const auto& E:CleanupRoutes)if(E.Script.Get()==Script)return true;
        if(!Pin(Script))return false;FCleanupScriptRoute E;E.Script=Script;E.Source=Script->GetLatestSource();E.Version=Script->GetExposedVersion().VersionGuid;
        if(E.Source.IsValid()){if(!Pin(E.Source.Get()))return false;if(auto* S=Cast<UNiagaraScriptSource>(E.Source.Get())){E.Graph=S->NodeGraph;if(E.Graph.IsValid()&&!Pin(E.Graph.Get()))return false;}}
        CleanupRoutes.Add(E);return true;
    };
    auto* S=SystemWeak.Get();if(!Live(S))return Fail(TEXT("niagara_renderer_routing_unavailable"),TEXT("Exact live System required for deferred renderer routing."));
    SpawnScript=S->GetSystemSpawnScript();UpdateScript=S->GetSystemUpdateScript();if(!AddScript(SpawnScript.Get())||!AddScript(UpdateScript.Get()))return false;
    for(const auto& E:Emitters){TArray<TWeakObjectPtr<UNiagaraScript>> Scripts;UNiagaraScriptSourceBase* Source=E.Data?E.Data->GraphSource.Get():nullptr;EmitterSources.Add(Source);if(Source&&!Pin(Source))return false;if(E.Data){TArray<UNiagaraScript*> Actual;E.Data->GetScripts(Actual,false,false);for(auto* Script:Actual){Scripts.Add(Script);if(!AddScript(Script))return false;}}EmitterScripts.Add(MoveTemp(Scripts));}
    RoutingCaptured=true;return Admit(TEXT("renderer_cleanup_routing_captured"));
}
bool FWork::ValidateDeferredState(const TCHAR* Stage)
{
    if(!Admit(Stage))return false;if(!DeferredShape)return true;
    auto Actual=Project(*this);if(!R.Error.IsEmpty())return false;Actual->RemoveField(TEXT("dirty"));
    if(!Json(Actual).Equals(Json(DeferredShape),ESearchCase::CaseSensitive))return Fail(TEXT("niagara_renderer_retained_state_changed"),TEXT("Captured renderer typed state changed outside the exact cleanup plan; no later compiler pump/save/replay."));
    return Admit(TEXT("renderer_cleanup_retained_state_verified"));
}
void FWork::BeginWait(const TSharedPtr<FJsonObject>& Result,double Timeout,double Poll)
{
    if(!Admit(TEXT("renderer_cleanup_wait_begin")))return;CleanupWait=Result;WaitDeferred=true;WaitStarted=FPlatformTime::Seconds();WaitNext=WaitStarted;WaitTimeout=FMath::Clamp(Timeout,0.0,60.0);WaitPoll=FMath::Clamp(Poll,0.02,1.0);WaitIterations=0;
    CleanupWait->SetBoolField(TEXT("wait_performed"),true);R.Data->SetBoolField(TEXT("genuine_compile_wait_implemented"),true);
}
bool FWork::AdvanceWait()
{
    if(!ValidateDeferredState(TEXT("renderer_cleanup_wait_frame_admission")))return false;auto* S=SystemWeak.Get();bool Busy=false,Active=false;
    auto Observe=[&]()->bool
    {if(!SDK(TEXT("cleanup_wait_outstanding")))return false;Busy=S->HasOutstandingCompilationRequests(true);if(!Returned(TEXT("cleanup_wait_outstanding")))return false;
     if(!SDK(TEXT("cleanup_wait_active")))return false;Active=S->HasActiveCompilations();if(!Returned(TEXT("cleanup_wait_active")))return false;return ValidateDeferredState(TEXT("renderer_cleanup_after_wait_observation"));};
    if(!Observe())return false;double Now=FPlatformTime::Seconds();
    if(!Busy&&!Active){WaitDone=WaitIdle=true;}
    else if(Now>=WaitNext)
    {
        // Original busy -> Poll(default flush) -> Process -> reobserve -> idle -> deadline.
        // Even timeout=0 gets one completion-application opportunity; at most one pump per GT frame.
        if(!SDK(TEXT("cleanup_wait_PollForCompilationComplete"),true))return false;R.Data->SetBoolField(TEXT("poll_may_request_compile"),true);S->PollForCompilationComplete();if(!Returned(TEXT("cleanup_wait_PollForCompilationComplete"))||!ValidateDeferredState(TEXT("renderer_cleanup_after_poll")))return false;
        if(!SDK(TEXT("cleanup_wait_ProcessAsyncTasks"),true))return false;FAssetCompilingManager::Get().ProcessAsyncTasks(true);if(!Returned(TEXT("cleanup_wait_ProcessAsyncTasks"))||!Observe())return false;
        Now=FPlatformTime::Seconds();if(!Busy&&!Active){WaitDone=WaitIdle=true;}else if(Now>=WaitStarted+WaitTimeout){WaitDone=WaitTimedOut=true;}else{++WaitIterations;WaitNext=Now+WaitPoll;}
    }
    CleanupWait->SetBoolField(TEXT("wait_timed_out"),WaitTimedOut);CleanupWait->SetNumberField(TEXT("wait_iterations"),WaitIterations);CleanupWait->SetNumberField(TEXT("wait_elapsed_seconds"),Now-WaitStarted);CleanupWait->SetBoolField(TEXT("outstanding_compilation_requests_after"),Busy);CleanupWait->SetBoolField(TEXT("active_compilations_after"),Active);
    R.Data->SetObjectField(TEXT("compile_wait"),CleanupWait);R.Data->SetBoolField(TEXT("required_wait_idle_observed"),WaitIdle);return true;
}

TSharedPtr<FJsonObject> WaitForNiagaraCompileJson(FWork& W,UNiagaraSystem* S,bool Wait,double Timeout,double Poll)
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0);O->SetBoolField(TEXT("wait_performed"),false);
    if(W.DeferredMode&&Wait){W.BeginWait(O,Timeout,Poll);return O;}
    if(!W.Admit(TEXT("compile_request_snapshot"))){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);return O;}
    O->SetBoolField(TEXT("outstanding_compilation_requests_after"),S?S->HasOutstandingCompilationRequests(true):false);O->SetBoolField(TEXT("active_compilations_after"),S?S->HasActiveCompilations():false);return O;
}
bool TryParseNiagaraMeshArrayJsonValue(FWork& W,
    const TSharedPtr<FJsonValue>& JsonValue,
    TArray<FNiagaraMeshRendererMeshPropertiesBase>& OutMeshes,
    FString& OutError)
{
    if (!JsonValue.IsValid() || JsonValue->Type != EJson::Array)
    {
        OutError = TEXT("Expected array value for Mesh Array Niagara user parameter");
        return false;
    }

    int32 Index = 0;
    for (const TSharedPtr<FJsonValue>& Item : JsonValue->AsArray())
    {
        FString MeshPath;
        FNiagaraMeshRendererMeshPropertiesBase MeshProperties;

        if (!Item.IsValid())
        {
            OutError = FString::Printf(TEXT("Mesh array entry %d is null"), Index);
            return false;
        }

        if (Item->TryGetString(MeshPath))
        {
            MeshPath.TrimStartAndEndInline();
        }
        else if (Item->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> MeshObject = Item->AsObject();
            if (!MeshObject.IsValid() ||
                (!MeshObject->TryGetStringField(TEXT("mesh"), MeshPath) &&
                 !MeshObject->TryGetStringField(TEXT("mesh_path"), MeshPath) &&
                 !MeshObject->TryGetStringField(TEXT("static_mesh"), MeshPath)))
            {
                OutError = FString::Printf(TEXT("Mesh array entry %d is missing mesh/mesh_path/static_mesh"), Index);
                return false;
            }
            MeshPath.TrimStartAndEndInline();

            const TArray<TSharedPtr<FJsonValue>>* ScaleArray = nullptr;
            if (MeshObject->TryGetArrayField(TEXT("scale"), ScaleArray))
            {
                FVector Scale = FVector::OneVector;
                if (!TryParseJsonVector3Array(ScaleArray, Scale))
                {
                    OutError = FString::Printf(TEXT("Mesh array entry %d scale must be [x, y, z]"), Index);
                    return false;
                }
                MeshProperties.Scale = Scale;
            }

            const TArray<TSharedPtr<FJsonValue>>* RotationArray = nullptr;
            if (MeshObject->TryGetArrayField(TEXT("rotation"), RotationArray))
            {
                FVector Rotation = FVector::ZeroVector;
                if (!TryParseJsonVector3Array(RotationArray, Rotation))
                {
                    OutError = FString::Printf(TEXT("Mesh array entry %d rotation must be [pitch, yaw, roll]"), Index);
                    return false;
                }
                MeshProperties.Rotation = FRotator(Rotation.X, Rotation.Y, Rotation.Z);
            }

            const TArray<TSharedPtr<FJsonValue>>* PivotOffsetArray = nullptr;
            if (MeshObject->TryGetArrayField(TEXT("pivot_offset"), PivotOffsetArray))
            {
                FVector PivotOffset = FVector::ZeroVector;
                if (!TryParseJsonVector3Array(PivotOffsetArray, PivotOffset))
                {
                    OutError = FString::Printf(TEXT("Mesh array entry %d pivot_offset must be [x, y, z]"), Index);
                    return false;
                }
                MeshProperties.PivotOffset = PivotOffset;
            }

            const TSharedPtr<FJsonValue> PivotOffsetSpaceValue = MeshObject->TryGetField(TEXT("pivot_offset_space"));
            if (PivotOffsetSpaceValue.IsValid())
            {
                ENiagaraMeshPivotOffsetSpace PivotOffsetSpace = MeshProperties.PivotOffsetSpace;
                if (!TryParseNiagaraMeshPivotOffsetSpaceJsonValue(PivotOffsetSpaceValue, PivotOffsetSpace, OutError))
                {
                    OutError = FString::Printf(TEXT("Mesh array entry %d %s"), Index, *OutError);
                    return false;
                }
                MeshProperties.PivotOffsetSpace = PivotOffsetSpace;
            }
        }
        else
        {
            OutError = FString::Printf(TEXT("Mesh array entry %d must be an asset path string or object"), Index);
            return false;
        }

        if (MeshPath.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Mesh array entry %d has an empty mesh path"), Index);
            return false;
        }
        if (IsMCPDependencyReference(MeshPath))
        {
            OutError = FString::Printf(TEXT("Refusing to store MCP-only mesh reference in Niagara content: %s"), *MeshPath);
            return false;
        }

        UStaticMesh* Mesh = W.Load<UStaticMesh>(MeshPath,TEXT("load_mesh_entry"));
        if (!Mesh)
        {
            OutError = FString::Printf(TEXT("Failed to load StaticMesh for Mesh Array entry %d: %s"), Index, *MeshPath);
            return false;
        }

        MeshProperties.Mesh = Mesh;
        OutMeshes.Add(MeshProperties);
        ++Index;
    }

    return true;
}

struct FResolvedNiagaraEmitterTarget{UNiagaraEmitter* Emitter=nullptr;FVersionedNiagaraEmitterData* Data=nullptr;FGuid Version;FString Name;int32 Index=INDEX_NONE;};
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

bool SetRendererMaterial(FWork& W,
    UNiagaraRendererProperties* Renderer,
    UMaterialInterface* Material,
    int32 MaterialSlotIndex,
    FString& OutChangedField,
    FString& OutError)
{
    if (!Renderer)
    {
        OutError = TEXT("Renderer is null");
        return false;
    }

    if (!Material)
    {
        OutError = TEXT("Material is null");
        return false;
    }

    if (UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Renderer))
    {
        if(!W.SDK(TEXT("modify_sprite_material"),true)){return false;}SpriteRenderer->Modify();if(!W.Returned(TEXT("modify_sprite_material"))){return false;}
        SpriteRenderer->Material = Material;
        SpriteRenderer->MICMaterial = nullptr;
        OutChangedField = TEXT("Material");
        return true;
    }

    if (UNiagaraRibbonRendererProperties* RibbonRenderer = Cast<UNiagaraRibbonRendererProperties>(Renderer))
    {
        if(!W.SDK(TEXT("modify_ribbon_material"),true)){return false;}RibbonRenderer->Modify();if(!W.Returned(TEXT("modify_ribbon_material"))){return false;}
        RibbonRenderer->Material = Material;
        RibbonRenderer->MICMaterial = nullptr;
        OutChangedField = TEXT("Material");
        return true;
    }

    if (UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
    {
        const int32 SafeMaterialSlot = FMath::Max(0, MaterialSlotIndex);
        if(SafeMaterialSlot>65535){OutError=TEXT("Material slot exceeds bounded allocation");return false;}if(!W.SDK(TEXT("modify_mesh_material"),true)){return false;}MeshRenderer->Modify();if(!W.Returned(TEXT("modify_mesh_material"))){return false;}
        MeshRenderer->bOverrideMaterials = true;
        if (MeshRenderer->OverrideMaterials.Num() <= SafeMaterialSlot)
        {
            MeshRenderer->OverrideMaterials.SetNum(SafeMaterialSlot + 1);
        }
        MeshRenderer->OverrideMaterials[SafeMaterialSlot].ExplicitMat = Material;
        OutChangedField = FString::Printf(TEXT("OverrideMaterials[%d].ExplicitMat"), SafeMaterialSlot);
        return true;
    }

    if (UNiagaraDecalRendererProperties* DecalRenderer = Cast<UNiagaraDecalRendererProperties>(Renderer))
    {
        if(!W.SDK(TEXT("modify_decal_material"),true)){return false;}DecalRenderer->Modify();if(!W.Returned(TEXT("modify_decal_material"))){return false;}
        DecalRenderer->Material = Material;
        DecalRenderer->MICMaterial = nullptr;
        OutChangedField = TEXT("Material");
        return true;
    }

    if (UNiagaraVolumeRendererProperties* VolumeRenderer = Cast<UNiagaraVolumeRendererProperties>(Renderer))
    {
        if(!W.SDK(TEXT("modify_volume_material"),true)){return false;}VolumeRenderer->Modify();if(!W.Returned(TEXT("modify_volume_material"))){return false;}
        VolumeRenderer->Material = Material;
        VolumeRenderer->MICMaterial = nullptr;
        OutChangedField = TEXT("Material");
        return true;
    }

    OutError = FString::Printf(TEXT("Unsupported Niagara renderer class: %s"), *Renderer->GetClass()->GetName());
    return false;
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
        if(!W.SDK(TEXT("get_used_materials"))){return RendererObject;}Renderer->GetUsedMaterials(nullptr, UsedMaterials);if(!W.Returned(TEXT("get_used_materials"))){return RendererObject;}
    }
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





FString GetMCPDependencyReferenceError(const FString& FieldName,const FString& Value)
{return FString::Printf(TEXT("Refusing to persist MCP-only reference in '%s': %s. CubelessMCP is an Editor authoring extension; finished assets must not depend on MCP."),*(FieldName.IsEmpty()?TEXT("value"):FieldName),*Value);}
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}
    static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}
};
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
bool VerifyMaterial(FWork& W,UNiagaraRendererProperties* Renderer,UMaterialInterface* Material,int32 Slot)
{
    if(!W.Admit(TEXT("material_readback"))){return false;}bool Pass=false;
    if(const auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(Renderer)){Pass=Sprite->Material==Material;}
    else if(const auto* Ribbon=Cast<UNiagaraRibbonRendererProperties>(Renderer)){Pass=Ribbon->Material==Material;}
    else if(const auto* Mesh=Cast<UNiagaraMeshRendererProperties>(Renderer)){const int32 Index=FMath::Max(0,Slot);Pass=Mesh->bOverrideMaterials&&Mesh->OverrideMaterials.IsValidIndex(Index)&&Mesh->OverrideMaterials[Index].ExplicitMat==Material;}
    else if(const auto* Decal=Cast<UNiagaraDecalRendererProperties>(Renderer)){Pass=Decal->Material==Material;}
    else if(const auto* Volume=Cast<UNiagaraVolumeRendererProperties>(Renderer)){Pass=Volume->Material==Material;}
    W.R.Data->SetBoolField(TEXT("typed_readback_pass"),Pass);return Pass||W.Fail(TEXT("niagara_renderer_material_readback_failed"),TEXT("Requested material did not remain applied after SDK/compile/save callbacks."));
}
bool VerifySprite(FWork& W,UNiagaraSpriteRendererProperties* Sprite,bool HasAlignment,ENiagaraSpriteAlignment Alignment,bool HasFacing,ENiagaraSpriteFacingMode Facing,bool HasSubImage,const FVector2D& SubImage)
{
    if(!W.Admit(TEXT("sprite_readback"))){return false;}
    const bool Pass=Live(Sprite)&&(!HasAlignment||Sprite->Alignment==Alignment)&&(!HasFacing||Sprite->FacingMode==Facing)&&(!HasSubImage||Sprite->SubImageSize==SubImage);
    W.R.Data->SetBoolField(TEXT("typed_readback_pass"),Pass);return Pass||W.Fail(TEXT("niagara_renderer_sprite_readback_failed"),TEXT("Requested sprite settings did not remain applied after SDK/compile/save callbacks."));
}
bool VerifyMesh(FWork& W,UNiagaraMeshRendererProperties* Mesh,const FString& Binding,const FString& IndexBinding,const TArray<FNiagaraMeshRendererMeshPropertiesBase>& Entries)
{
    if(!W.Admit(TEXT("mesh_readback"))){return false;}bool Pass=Live(Mesh)&&Mesh->GetIsEnabled()&&Mesh->Meshes.Num()==Entries.Num();
    if(Pass){Pass=(Mesh->MeshesBinding.ResolvedParameter.GetName().ToString().Equals(Binding,ESearchCase::IgnoreCase)||Mesh->MeshesBinding.AliasedParameter.GetName().ToString().Equals(Binding,ESearchCase::IgnoreCase))&&(Mesh->MeshIndexBinding.GetName().ToString().Equals(IndexBinding,ESearchCase::IgnoreCase)||Mesh->MeshIndexBinding.GetParamMapBindableVariable().GetName().ToString().Equals(IndexBinding,ESearchCase::IgnoreCase));}
    for(int32 I=0;Pass&&I<Entries.Num();++I){Pass=Json(NiagaraMeshPropertiesBaseToJsonObject(Mesh->Meshes[I])).Equals(Json(NiagaraMeshPropertiesBaseToJsonObject(Entries[I])),ESearchCase::CaseSensitive);}
    W.R.Data->SetBoolField(TEXT("typed_readback_pass"),Pass);return Pass||W.Fail(TEXT("niagara_renderer_mesh_readback_failed"),TEXT("Requested mesh/binding/local fallback did not remain applied after SDK/compile/save callbacks."));
}

TSharedPtr<FJsonObject> HandleInspectNiagaraRenderers(FWork& W, const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> RendererValues;
    int32 EmitterIndex = 0;
    const TArray<FNiagaraEmitterHandle> EmitterHandlesSnapshot = System->GetEmitterHandles();
    for (const FNiagaraEmitterHandle& EmitterHandle : EmitterHandlesSnapshot)
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        if (EmitterHandle.GetIsEnabled())
        {
            if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
            {
                int32 RendererIndex = 0;
                const TArray<UNiagaraRendererProperties*> RendererSlotsSnapshot = EmitterData->GetRenderers();
                for (const UNiagaraRendererProperties* Renderer : RendererSlotsSnapshot)
                {
                    if (Renderer && Renderer->GetIsEnabled())
                    {
                        RendererValues.Add(MakeShared<FJsonValueObject>(BuildRendererJson(W, System, EmitterName, EmitterIndex, Renderer, RendererIndex)));
        if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
                    }
                    ++RendererIndex;
                }
            }
        }
        ++EmitterIndex;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetNumberField(TEXT("emitter_count"), System->GetEmitterHandles().Num());
    Result->SetNumberField(TEXT("renderer_count"), RendererValues.Num());
    Result->SetArrayField(TEXT("renderers"), RendererValues);
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraRendererMaterial(FWork& W, const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    FString MaterialPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return W.Error(TEXT("Missing 'material_path' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara system outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    int32 TargetRendererIndex = INDEX_NONE;
    int32 MaterialSlotIndex = 0;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetNumberField(TEXT("renderer_index"), TargetRendererIndex);
    Params->TryGetNumberField(TEXT("material_slot_index"), MaterialSlotIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    W.R.bSaveRequested=bSave;
    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    if (IsMCPDependencyReference(MaterialPath)) return W.Error(GetMCPDependencyReferenceError(TEXT("material_path"),MaterialPath));
    UMaterialInterface* Material = W.Load<UMaterialInterface>(MaterialPath,TEXT("load_material"));
    if (!Material)
    {
        return W.Error(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }

    UNiagaraRendererProperties* MatchedRenderer = nullptr;
    int32 MatchedEmitterIndex = INDEX_NONE;
    int32 MatchedRendererIndex = INDEX_NONE;
    FString MatchedEmitterName;

    int32 CurrentEmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));

        if (bEmitterMatches)
        {
            if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
            {
                int32 RendererIndex = 0;
                for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
                {
                    if (Renderer && Renderer->GetIsEnabled() && MatchedRenderer == nullptr && (TargetRendererIndex == INDEX_NONE || TargetRendererIndex == RendererIndex))
                    {
                        MatchedRenderer = Renderer;
                        MatchedEmitterIndex = CurrentEmitterIndex;
                        MatchedRendererIndex = RendererIndex;
                        MatchedEmitterName = EmitterName;
                    }
                    ++RendererIndex;
                }
            }
        }

        ++CurrentEmitterIndex;
    }

    if (!MatchedRenderer)
    {
        return W.Error(TEXT("No matching enabled Niagara renderer was found"));
    }

    if (!W.AdmitOwnedEmitterAt(MatchedEmitterIndex,MatchedRenderer)) return W.Error(W.R.Error);
    FString ChangedField;
    FString ErrorMessage;
    if (!SetRendererMaterial(W, MatchedRenderer, Material, MaterialSlotIndex, ChangedField, ErrorMessage))
    {
        return W.Error(ErrorMessage);
    }

    GE_STEP("System_Modify",true,System->Modify());
    GE_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    bool CompileAccepted = false; if (!W.Request(System,CompileAccepted)) return W.Error(W.R.Error);

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(System);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after renderer material update"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("material_path"), Material->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), MatchedEmitterName);
    Result->SetNumberField(TEXT("emitter_index"), MatchedEmitterIndex);
    Result->SetNumberField(TEXT("renderer_index"), MatchedRendererIndex);
    Result->SetStringField(TEXT("renderer_class"), MatchedRenderer->GetClass()->GetName());
    Result->SetStringField(TEXT("changed_field"), ChangedField);
    Result->SetBoolField(TEXT("saved"), bSaved);
    VerifyMaterial(W,MatchedRenderer,Material,MaterialSlotIndex);
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraSpriteRendererSettings(FWork& W, const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    FString AlignmentText;
    const bool bHasAlignment = Params->TryGetStringField(TEXT("alignment"), AlignmentText);
    ENiagaraSpriteAlignment NewAlignment = ENiagaraSpriteAlignment::Automatic;
    if (bHasAlignment && !ParseNiagaraSpriteAlignmentToken(AlignmentText, NewAlignment))
    {
        return W.Error(FString::Printf(TEXT("Unsupported alignment: %s (Unaligned|VelocityAligned|CustomAlignment|Automatic)"), *AlignmentText));
    }

    FString FacingText;
    const bool bHasFacing = Params->TryGetStringField(TEXT("facing_mode"), FacingText);
    ENiagaraSpriteFacingMode NewFacing = ENiagaraSpriteFacingMode::Automatic;
    if (bHasFacing && !ParseNiagaraSpriteFacingModeToken(FacingText, NewFacing))
    {
        return W.Error(FString::Printf(TEXT("Unsupported facing_mode: %s"), *FacingText));
    }

    const TArray<TSharedPtr<FJsonValue>>* SubImageArray = nullptr;
    FVector2D SubImageVec = FVector2D::ZeroVector;
    bool bHasSubImage = false;
    if (Params->TryGetArrayField(TEXT("sub_image_size"), SubImageArray))
    {
        if (!TryParseVector2FromJsonArray(SubImageArray, SubImageVec))
        {
            return W.Error(TEXT("'sub_image_size' must be [x, y] finite numbers"));
        }
        bHasSubImage = true;
    }

    if (!bHasAlignment && !bHasFacing && !bHasSubImage)
    {
        return W.Error(TEXT("Provide at least one of 'alignment', 'facing_mode', 'sub_image_size'"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara renderer outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    int32 TargetRendererIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetNumberField(TEXT("renderer_index"), TargetRendererIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    W.R.bSaveRequested=bSave;
    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraRendererProperties* MatchedRenderer = nullptr;
    int32 MatchedEmitterIndex = INDEX_NONE;
    int32 MatchedRendererIndex = INDEX_NONE;
    FString MatchedEmitterName;

    int32 CurrentEmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));
        if (bEmitterMatches)
        {
            if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
            {
                int32 RendererIndex = 0;
                for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
                {
                    if (Renderer && Renderer->GetIsEnabled() && MatchedRenderer == nullptr &&
                        (TargetRendererIndex == INDEX_NONE || TargetRendererIndex == RendererIndex) &&
                        Renderer->IsA<UNiagaraSpriteRendererProperties>())
                    {
                        MatchedRenderer = Renderer;
                        MatchedEmitterIndex = CurrentEmitterIndex;
                        MatchedRendererIndex = RendererIndex;
                        MatchedEmitterName = EmitterName;
                    }
                    ++RendererIndex;
                }
            }
        }
        ++CurrentEmitterIndex;
    }

    UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(MatchedRenderer);
    if (!Sprite)
    {
        return W.Error(TEXT("No matching enabled Niagara sprite renderer was found"));
    }

    TSharedPtr<FJsonObject> Before = MakeShared<FJsonObject>();
    Before->SetStringField(TEXT("alignment"), NiagaraEnumValueName(Sprite->Alignment));
    Before->SetStringField(TEXT("facing_mode"), NiagaraEnumValueName(Sprite->FacingMode));
    Before->SetNumberField(TEXT("sub_image_size_x"), Sprite->SubImageSize.X);
    Before->SetNumberField(TEXT("sub_image_size_y"), Sprite->SubImageSize.Y);

    if (!W.AdmitOwnedEmitterAt(MatchedEmitterIndex,Sprite)) return W.Error(W.R.Error);
    GE_STEP("System_Modify",true,System->Modify());
    GE_STEP("Sprite_Modify",true,Sprite->Modify());
    if (bHasAlignment)
    {
        Sprite->Alignment = NewAlignment;
    }
    if (bHasFacing)
    {
        Sprite->FacingMode = NewFacing;
    }
    if (bHasSubImage)
    {
        Sprite->SubImageSize = FVector2D(SubImageVec.X, SubImageVec.Y);
    }
    GE_STEP("Sprite_PostEditChange",true,Sprite->PostEditChange());
    GE_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(System,CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(System);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after sprite renderer update"));
        }
    }

    TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();
    After->SetStringField(TEXT("alignment"), NiagaraEnumValueName(Sprite->Alignment));
    After->SetStringField(TEXT("facing_mode"), NiagaraEnumValueName(Sprite->FacingMode));
    After->SetNumberField(TEXT("sub_image_size_x"), Sprite->SubImageSize.X);
    After->SetNumberField(TEXT("sub_image_size_y"), Sprite->SubImageSize.Y);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), MatchedEmitterName);
    Result->SetNumberField(TEXT("emitter_index"), MatchedEmitterIndex);
    Result->SetNumberField(TEXT("renderer_index"), MatchedRendererIndex);
    Result->SetObjectField(TEXT("before"), Before);
    Result->SetObjectField(TEXT("after"), After);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("sprite_renderer_settings"));
    VerifySprite(W,Sprite,bHasAlignment,NewAlignment,bHasFacing,NewFacing,bHasSubImage,SubImageVec);
    return Result;
}

TSharedPtr<FJsonObject> HandleAddNiagaraSpriteRenderer(FWork& W, const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return W.Error(TEXT("Missing 'material_path' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara renderer outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);

    FString AlignmentText;
    Params->TryGetStringField(TEXT("alignment"), AlignmentText);
    ENiagaraSpriteAlignment NewAlignment = ENiagaraSpriteAlignment::Automatic;
    if (!AlignmentText.IsEmpty() && !ParseNiagaraSpriteAlignmentToken(AlignmentText, NewAlignment))
    {
        return W.Error(FString::Printf(TEXT("Unsupported alignment: %s (Unaligned|VelocityAligned|CustomAlignment|Automatic)"), *AlignmentText));
    }

    FString FacingText;
    Params->TryGetStringField(TEXT("facing_mode"), FacingText);
    ENiagaraSpriteFacingMode NewFacing = ENiagaraSpriteFacingMode::Automatic;
    if (!FacingText.IsEmpty() && !ParseNiagaraSpriteFacingModeToken(FacingText, NewFacing))
    {
        return W.Error(FString::Printf(TEXT("Unsupported facing_mode: %s"), *FacingText));
    }

    FVector2D SubImageSize(1.0f, 1.0f);
    const TArray<TSharedPtr<FJsonValue>>* SubImageArray = nullptr;
    if (Params->TryGetArrayField(TEXT("sub_image_size"), SubImageArray))
    {
        if (!TryParseVector2FromJsonArray(SubImageArray, SubImageSize))
        {
            return W.Error(TEXT("'sub_image_size' must be [x, y] finite numbers"));
        }
    }

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bSkipIfExisting = true;
    Params->TryGetBoolField(TEXT("skip_if_existing"), bSkipIfExisting);

    W.R.bSaveRequested=bSave;
    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    if (IsMCPDependencyReference(MaterialPath)) return W.Error(GetMCPDependencyReferenceError(TEXT("material_path"),MaterialPath));
    UMaterialInterface* Material = W.Load<UMaterialInterface>(MaterialPath,TEXT("load_material"));
    if (!Material)
    {
        return W.Error(FString::Printf(TEXT("Failed to load material: %s"), *MaterialPath));
    }

    FResolvedNiagaraEmitterTarget Target;
    int32 MatchCount = 0;
    if (!ResolveSingleNiagaraEmitterTarget(System, TargetEmitterIndex, TargetEmitterName, Target, MatchCount))
    {
        if (MatchCount > 1)
        {
            return W.Error(TEXT("Emitter selector matched more than one emitter"));
        }
        return W.Error(TEXT("No matching Niagara emitter was found"));
    }
    if (!Target.Emitter || !Target.Data)
    {
        return W.Error(TEXT("Matched Niagara emitter is not editable"));
    }

    if (bSkipIfExisting)
    {
        int32 ExistingRendererIndex = 0;
        for (UNiagaraRendererProperties* Renderer : Target.Data->GetRenderers())
        {
            if (const UNiagaraSpriteRendererProperties* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Renderer))
            {
                if (SpriteRenderer->Material == Material)
                {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetBoolField(TEXT("success"), true);
                    Result->SetStringField(TEXT("system_path"), System->GetPathName());
                    Result->SetStringField(TEXT("emitter_name"), Target.Name);
                    Result->SetNumberField(TEXT("emitter_index"), Target.Index);
                    Result->SetNumberField(TEXT("renderer_index"), ExistingRendererIndex);
                    Result->SetStringField(TEXT("material_path"), Material->GetPathName());
                    Result->SetBoolField(TEXT("added"), false);
                    Result->SetBoolField(TEXT("compile_requested"), false);
                    Result->SetBoolField(TEXT("saved"), false);
                    Result->SetStringField(TEXT("write_scope"), TEXT("sprite_renderer_add"));
                    Result->SetObjectField(TEXT("renderer"), BuildRendererJson(W, System, Target.Name, Target.Index, SpriteRenderer, ExistingRendererIndex));
        if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
                    return Result;
                }
            }
            ++ExistingRendererIndex;
        }
    }

    if (!W.AdmitOwnedEmitter(Target.Emitter)) return W.Error(W.R.Error);
    GE_STEP("System_Modify",true,System->Modify());
    GE_STEP("TargetEmitter_Modify",true,Target.Emitter->Modify());
    UNiagaraSpriteRendererProperties* NewRenderer = W.NewRenderer<UNiagaraSpriteRendererProperties>(Target.Emitter);
    if (!NewRenderer) return W.Error(W.R.Error);
    GE_STEP("NewRenderer_Modify",true,NewRenderer->Modify());
    NewRenderer->Material = Material;
    NewRenderer->Alignment = NewAlignment;
    NewRenderer->FacingMode = NewFacing;
    NewRenderer->SubImageSize = SubImageSize;
    GE_STEP("NewRenderer_SetIsEnabled",true,NewRenderer->SetIsEnabled(true));
    if (!W.ChangeTopology(Target.Emitter,Target.Version,NewRenderer,true)) return W.Error(W.R.Error);
    GE_STEP("NewRenderer_PostEditChange",true,NewRenderer->PostEditChange());

    if (UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
    {
        if (!W.Pin(SystemEditorData)) return W.Error(W.R.Error);
        GE_STEP("synchronize_overview",true,SystemEditorData->SynchronizeOverviewGraphWithSystem(*System));
    }

    GE_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(System,CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(System);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after sprite renderer add"));
        }
    }

    const int32 NewRendererIndex = Target.Data->GetRenderers().IndexOfByKey(NewRenderer);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), Target.Name);
    Result->SetNumberField(TEXT("emitter_index"), Target.Index);
    Result->SetNumberField(TEXT("renderer_index"), NewRendererIndex);
    Result->SetStringField(TEXT("material_path"), Material->GetPathName());
    Result->SetStringField(TEXT("alignment"), NiagaraEnumValueName(NewRenderer->Alignment));
    Result->SetStringField(TEXT("facing_mode"), NiagaraEnumValueName(NewRenderer->FacingMode));
    Result->SetNumberField(TEXT("sub_image_size_x"), NewRenderer->SubImageSize.X);
    Result->SetNumberField(TEXT("sub_image_size_y"), NewRenderer->SubImageSize.Y);
    Result->SetBoolField(TEXT("added"), true);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("sprite_renderer_add"));
    Result->SetObjectField(TEXT("renderer"), BuildRendererJson(W, System, Target.Name, Target.Index, NewRenderer, NewRendererIndex));
        if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    VerifyMaterial(W,NewRenderer,Material,0);
    VerifySprite(W,NewRenderer,true,NewAlignment,true,NewFacing,true,SubImageSize);
    return Result;
}

TSharedPtr<FJsonObject> HandleAddNiagaraMeshRenderer(FWork& W, const TSharedPtr<FJsonObject>& Params)
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
            FString::Printf(TEXT("Refusing to edit Niagara renderer outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);

    FString MeshesBindingName = TEXT("User.MeshList");
    Params->TryGetStringField(TEXT("meshes_binding_parameter"), MeshesBindingName);
    MeshesBindingName.TrimStartAndEndInline();
    if (MeshesBindingName.IsEmpty())
    {
        return W.Error(TEXT("'meshes_binding_parameter' cannot be empty"));
    }
    if (!MeshesBindingName.Contains(TEXT(".")))
    {
        MeshesBindingName = FString::Printf(TEXT("User.%s"), *MeshesBindingName);
    }
    if (IsMCPDependencyReference(MeshesBindingName))
    {
        return W.Error(
            GetMCPDependencyReferenceError(TEXT("meshes_binding_parameter"), MeshesBindingName));
    }

    FString MeshIndexBindingName = TEXT("Particles.MeshIndex");
    Params->TryGetStringField(TEXT("mesh_index_binding"), MeshIndexBindingName);
    MeshIndexBindingName.TrimStartAndEndInline();
    if (MeshIndexBindingName.IsEmpty())
    {
        return W.Error(TEXT("'mesh_index_binding' cannot be empty"));
    }

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bSkipIfExisting = true;
    Params->TryGetBoolField(TEXT("skip_if_existing"), bSkipIfExisting);

    TArray<FNiagaraMeshRendererMeshPropertiesBase> MeshEntries;
    const TSharedPtr<FJsonValue> MeshesValue = Params->TryGetField(TEXT("mesh_paths")).IsValid()
        ? Params->TryGetField(TEXT("mesh_paths"))
        : Params->TryGetField(TEXT("meshes"));
    if (MeshesValue.IsValid())
    {
        FString MeshParseError;
        if (!TryParseNiagaraMeshArrayJsonValue(W, MeshesValue, MeshEntries, MeshParseError))
        {
            return W.Error(MeshParseError);
        }
    }
    else
    {
        UStaticMesh* DefaultMesh = W.Load<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube.Cube"),TEXT("load_default_mesh"));
        if (!DefaultMesh)
        {
            return W.Error(TEXT("Failed to load default Mesh Renderer fallback mesh: /Engine/BasicShapes/Cube.Cube"));
        }
        FNiagaraMeshRendererMeshPropertiesBase DefaultMeshProperties;
        DefaultMeshProperties.Mesh = DefaultMesh;
        MeshEntries.Add(DefaultMeshProperties);
    }

    W.R.bSaveRequested=bSave;
    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    FResolvedNiagaraEmitterTarget Target;
    int32 MatchCount = 0;
    if (!ResolveSingleNiagaraEmitterTarget(System, TargetEmitterIndex, TargetEmitterName, Target, MatchCount))
    {
        if (MatchCount > 1)
        {
            return W.Error(TEXT("Emitter selector matched more than one emitter"));
        }
        return W.Error(TEXT("No matching Niagara emitter was found"));
    }
    if (!Target.Emitter || !Target.Data)
    {
        return W.Error(TEXT("Matched Niagara emitter is not editable"));
    }

    if (bSkipIfExisting)
    {
        int32 ExistingRendererIndex = 0;
        for (UNiagaraRendererProperties* Renderer : Target.Data->GetRenderers())
        {
            if (const UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
            {
                const FString ResolvedName = MeshRenderer->MeshesBinding.ResolvedParameter.GetName().ToString();
                const FString AliasedName = MeshRenderer->MeshesBinding.AliasedParameter.GetName().ToString();
                FString RequestedDataSetName = MeshIndexBindingName;
                RequestedDataSetName.RemoveFromStart(TEXT("Particles."), ESearchCase::IgnoreCase);
                const FString ExistingMeshIndexName = MeshRenderer->MeshIndexBinding.GetName().ToString();
                const FString ExistingParamMapName = MeshRenderer->MeshIndexBinding.GetParamMapBindableVariable().GetName().ToString();
                const FString ExistingDataSetName = MeshRenderer->MeshIndexBinding.GetDataSetBindableVariable().GetName().ToString();
                const bool bMeshesBindingMatches =
                    ResolvedName.Equals(MeshesBindingName, ESearchCase::IgnoreCase) ||
                    AliasedName.Equals(MeshesBindingName, ESearchCase::IgnoreCase);
                const bool bMeshIndexBindingMatches =
                    ExistingMeshIndexName.Equals(MeshIndexBindingName, ESearchCase::IgnoreCase) ||
                    ExistingParamMapName.Equals(MeshIndexBindingName, ESearchCase::IgnoreCase) ||
                    ExistingDataSetName.Equals(RequestedDataSetName, ESearchCase::IgnoreCase);
                if (bMeshesBindingMatches && bMeshIndexBindingMatches)
                {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetBoolField(TEXT("success"), true);
                    Result->SetStringField(TEXT("system_path"), System->GetPathName());
                    Result->SetStringField(TEXT("emitter_name"), Target.Name);
                    Result->SetNumberField(TEXT("emitter_index"), Target.Index);
                    Result->SetNumberField(TEXT("renderer_index"), ExistingRendererIndex);
                    Result->SetStringField(TEXT("meshes_binding_parameter"), MeshesBindingName);
                    Result->SetStringField(TEXT("mesh_index_binding"), MeshIndexBindingName);
                    Result->SetBoolField(TEXT("added"), false);
                    Result->SetBoolField(TEXT("compile_requested"), false);
                    Result->SetBoolField(TEXT("saved"), false);
                    Result->SetStringField(TEXT("write_scope"), TEXT("mesh_renderer_add"));
                    Result->SetObjectField(TEXT("renderer"), BuildRendererJson(W, System, Target.Name, Target.Index, MeshRenderer, ExistingRendererIndex));
        if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
                    return Result;
                }
            }
            ++ExistingRendererIndex;
        }
    }

    UClass* ArrayMeshType = MeshArrayClass(W);
    if (!ArrayMeshType) return W.Error(W.R.Error);
    if (!W.AdmitOwnedEmitter(Target.Emitter)) return W.Error(W.R.Error);
    GE_STEP("System_Modify",true,System->Modify());
    GE_STEP("TargetEmitter_Modify",true,Target.Emitter->Modify());
    UNiagaraMeshRendererProperties* NewRenderer = W.NewRenderer<UNiagaraMeshRendererProperties>(Target.Emitter);
    if (!NewRenderer) return W.Error(W.R.Error);
    GE_STEP("NewRenderer_Modify",true,NewRenderer->Modify());
    NewRenderer->Meshes.Reset();
    for (const FNiagaraMeshRendererMeshPropertiesBase& MeshEntry : MeshEntries)
    {
        FNiagaraMeshRendererMeshProperties RendererMeshEntry;
        RendererMeshEntry.Mesh = MeshEntry.Mesh;
        RendererMeshEntry.Scale = MeshEntry.Scale;
        RendererMeshEntry.Rotation = MeshEntry.Rotation;
        RendererMeshEntry.PivotOffset = MeshEntry.PivotOffset;
        RendererMeshEntry.PivotOffsetSpace = MeshEntry.PivotOffsetSpace;
        NewRenderer->Meshes.Add(RendererMeshEntry);
    }

    GE_STEP("set_meshes_default_binding",true,NewRenderer->MeshesBinding.SetDefaultParameter(
        FNiagaraVariable(FNiagaraTypeDefinition(ArrayMeshType), FName(*MeshesBindingName))));

    FNiagaraVariable MeshIndexVariable(FNiagaraTypeDefinition::GetIntDef(), FName(*MeshIndexBindingName));
    MeshIndexVariable.SetValue<int32>(0);
    GE_STEP("setup_mesh_index_binding",true,NewRenderer->MeshIndexBinding.Setup(MeshIndexVariable, MeshIndexVariable, NewRenderer->SourceMode));
    GE_STEP("NewRenderer_SetIsEnabled",true,NewRenderer->SetIsEnabled(true));

    if (!W.ChangeTopology(Target.Emitter,Target.Version,NewRenderer,true)) return W.Error(W.R.Error);
    GE_STEP("NewRenderer_PostEditChange",true,NewRenderer->PostEditChange());

    if (UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
    {
        if (!W.Pin(SystemEditorData)) return W.Error(W.R.Error);
        GE_STEP("synchronize_overview",true,SystemEditorData->SynchronizeOverviewGraphWithSystem(*System));
    }

    GE_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(System,CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(System);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after mesh renderer add"));
        }
    }

    const int32 NewRendererIndex = Target.Data->GetRenderers().IndexOfByKey(NewRenderer);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), Target.Name);
    Result->SetNumberField(TEXT("emitter_index"), Target.Index);
    Result->SetNumberField(TEXT("renderer_index"), NewRendererIndex);
    Result->SetStringField(TEXT("meshes_binding_parameter"), MeshesBindingName);
    Result->SetStringField(TEXT("mesh_index_binding"), MeshIndexBindingName);
    Result->SetArrayField(TEXT("meshes"), NiagaraMeshPropertiesBaseArrayToJsonValues(MeshEntries));
    Result->SetBoolField(TEXT("added"), true);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("mesh_renderer_add"));
    Result->SetObjectField(TEXT("renderer"), BuildRendererJson(W, System, Target.Name, Target.Index, NewRenderer, NewRendererIndex));
        if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    VerifyMesh(W,NewRenderer,MeshesBindingName,MeshIndexBindingName,MeshEntries);
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraRendererEnabled(FWork& W, const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bEnabled = true;
    if (!Params->TryGetBoolField(TEXT("enabled"), bEnabled))
    {
        return W.Error(TEXT("Missing 'enabled' parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara renderer outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    int32 TargetRendererIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetNumberField(TEXT("renderer_index"), TargetRendererIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    W.R.bSaveRequested=bSave;
    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    UNiagaraRendererProperties* MatchedRenderer = nullptr;
    FString MatchedEmitterName;
    int32 MatchedEmitterIndex = INDEX_NONE;
    int32 MatchedRendererIndex = INDEX_NONE;
    int32 MatchCount = 0;

    int32 CurrentEmitterIndex = 0;
    for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));
        if (bEmitterMatches)
        {
            if (FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData())
            {
                int32 CurrentRendererIndex = 0;
                for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
                {
                    const bool bRendererMatches =
                        (TargetRendererIndex == INDEX_NONE || TargetRendererIndex == CurrentRendererIndex);
                    if (Renderer && bRendererMatches)
                    {
                        ++MatchCount;
                        if (MatchCount == 1)
                        {
                            MatchedRenderer = Renderer;
                            MatchedEmitterName = EmitterName;
                            MatchedEmitterIndex = CurrentEmitterIndex;
                            MatchedRendererIndex = CurrentRendererIndex;
                        }
                    }
                    ++CurrentRendererIndex;
                }
            }
        }
        ++CurrentEmitterIndex;
    }

    if (!MatchedRenderer)
    {
        return W.Error(TEXT("No matching Niagara renderer was found"));
    }
    if (MatchCount > 1)
    {
        return W.Error(TEXT("Renderer selector matched more than one renderer"));
    }

    const bool bWasEnabled = MatchedRenderer->GetIsEnabled();
    if (!W.AdmitOwnedEmitterAt(MatchedEmitterIndex,MatchedRenderer)) return W.Error(W.R.Error);
    GE_STEP("System_Modify",true,System->Modify());
    GE_STEP("MatchedRenderer_Modify",true,MatchedRenderer->Modify());
    GE_STEP("MatchedRenderer_SetIsEnabled",true,MatchedRenderer->SetIsEnabled(bEnabled));
    GE_STEP("MatchedRenderer_PostEditChange",true,MatchedRenderer->PostEditChange());

    if (UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
    {
        if (!W.Pin(SystemEditorData)) return W.Error(W.R.Error);
        GE_STEP("synchronize_overview",true,SystemEditorData->SynchronizeOverviewGraphWithSystem(*System));
    }

    GE_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(System,CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save(System);
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after renderer enabled update"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), MatchedEmitterName);
    Result->SetNumberField(TEXT("emitter_index"), MatchedEmitterIndex);
    Result->SetNumberField(TEXT("renderer_index"), MatchedRendererIndex);
    Result->SetBoolField(TEXT("previous_enabled"), bWasEnabled);
    Result->SetBoolField(TEXT("enabled"), MatchedRenderer->GetIsEnabled());
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("renderer_enabled"));
    Result->SetObjectField(TEXT("renderer"), BuildRendererJson(W, System, MatchedEmitterName, MatchedEmitterIndex, MatchedRenderer, MatchedRendererIndex));
        if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    const bool EnabledReadbackPass = W.Admit(TEXT("enabled_readback")) && MatchedRenderer->GetIsEnabled()==bEnabled;
    W.R.Data->SetBoolField(TEXT("typed_readback_pass"),EnabledReadbackPass);
    if (!EnabledReadbackPass && W.R.Error.IsEmpty()) W.Fail(TEXT("niagara_renderer_enabled_readback_failed"),TEXT("Requested enabled flag did not remain applied."));
    return Result;
}

TSharedPtr<FJsonObject> HandleCleanupNiagaraRenderers(FWork& W, const TSharedPtr<FJsonObject>& Params)
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
            FString::Printf(TEXT("Refusing to clean Niagara renderers outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }
    if (GEditor && GEditor->GetPIEWorldContext() != nullptr)
    {
        return W.Error(
            TEXT("Refusing Niagara renderer structure cleanup while PIE or SIE is active"));
    }

    auto ParseRendererIndices = [&Params](const TCHAR* FieldName, TArray<int32>& OutIndices, FString& OutError)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Params->TryGetArrayField(FieldName, Values) || !Values)
        {
            return true;
        }

        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            double Number = 0.0;
            if (!Value.IsValid() || !Value->TryGetNumber(Number))
            {
                OutError = FString::Printf(TEXT("'%s' must contain only renderer index numbers"), FieldName);
                return false;
            }
            if (!FMath::IsFinite(Number) || Number < 0 || Number > static_cast<double>(MAX_int32))
            { OutError = FString::Printf(TEXT("'%s' index is outside finite int32 bounds"),FieldName); return false; }
            const int32 RendererIndex = FMath::RoundToInt(Number);
            if (!FMath::IsNearlyEqual(Number, static_cast<double>(RendererIndex)) || RendererIndex < 0)
            {
                OutError = FString::Printf(TEXT("'%s' contains an invalid renderer index: %g"), FieldName, Number);
                return false;
            }
            if (OutIndices.Contains(RendererIndex))
            {
                OutError = FString::Printf(TEXT("'%s' contains duplicate renderer index %d"), FieldName, RendererIndex);
                return false;
            }
            OutIndices.Add(RendererIndex);
        }
        return true;
    };

    TArray<int32> RemoveRendererIndices;
    TArray<int32> ClearMeshesBindingRendererIndices;
    FString ParseError;
    if (!ParseRendererIndices(TEXT("remove_renderer_indices"), RemoveRendererIndices, ParseError) ||
        !ParseRendererIndices(TEXT("clear_meshes_binding_renderer_indices"), ClearMeshesBindingRendererIndices, ParseError))
    {
        return W.Error(ParseError);
    }
    if (RemoveRendererIndices.IsEmpty() && ClearMeshesBindingRendererIndices.IsEmpty())
    {
        return W.Error(
            TEXT("cleanup_niagara_renderers requires remove_renderer_indices or clear_meshes_binding_renderer_indices"));
    }
    for (int32 RendererIndex : ClearMeshesBindingRendererIndices)
    {
        if (RemoveRendererIndices.Contains(RendererIndex))
        {
            return W.Error(
                FString::Printf(TEXT("Renderer index %d cannot be removed and have its MeshesBinding cleared in the same request"), RendererIndex));
        }
    }

    struct FExpectedRendererSpec
    {
        int32 RendererIndex = INDEX_NONE;
        FString RendererClass;
        bool bEnabled = false;
        FString MeshesBindingParameter;
        bool bHasMeshesBindingParameter = false;
    };

    TMap<int32, FExpectedRendererSpec> ExpectedSpecs;
    const TArray<TSharedPtr<FJsonValue>>* ExpectedRendererValues = nullptr;
    if (!Params->TryGetArrayField(TEXT("expected_renderers"), ExpectedRendererValues) || !ExpectedRendererValues)
    {
        return W.Error(TEXT("Missing 'expected_renderers' array"));
    }
    for (const TSharedPtr<FJsonValue>& Value : *ExpectedRendererValues)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            return W.Error(TEXT("Each expected_renderers entry must be an object"));
        }
        const TSharedPtr<FJsonObject> SpecObject = Value->AsObject();
        FExpectedRendererSpec Spec;
        if (!SpecObject.IsValid() ||
            !SpecObject->TryGetNumberField(TEXT("renderer_index"), Spec.RendererIndex) ||
            !SpecObject->TryGetStringField(TEXT("renderer_class"), Spec.RendererClass) ||
            !SpecObject->TryGetBoolField(TEXT("enabled"), Spec.bEnabled))
        {
            return W.Error(
                TEXT("Each expected_renderers entry requires renderer_index, renderer_class, and enabled"));
        }
        Spec.RendererClass.TrimStartAndEndInline();
        Spec.bHasMeshesBindingParameter = SpecObject->TryGetStringField(
            TEXT("meshes_binding_parameter"), Spec.MeshesBindingParameter);
        Spec.MeshesBindingParameter.TrimStartAndEndInline();
        if (Spec.RendererIndex < 0 || Spec.RendererClass.IsEmpty() || ExpectedSpecs.Contains(Spec.RendererIndex))
        {
            return W.Error(TEXT("expected_renderers contains an invalid or duplicate renderer specification"));
        }
        if (Spec.bHasMeshesBindingParameter && Spec.MeshesBindingParameter.IsEmpty())
        {
            return W.Error(
                TEXT("expected_renderers meshes_binding_parameter cannot be empty when provided"));
        }
        ExpectedSpecs.Add(Spec.RendererIndex, Spec);
    }

    TSet<int32> TargetIndices;
    for (int32 RendererIndex : RemoveRendererIndices)
    {
        TargetIndices.Add(RendererIndex);
    }
    for (int32 RendererIndex : ClearMeshesBindingRendererIndices)
    {
        TargetIndices.Add(RendererIndex);
    }
    for (int32 TargetIndex : TargetIndices)
    {
        if (!ExpectedSpecs.Contains(TargetIndex))
        {
            return W.Error(
                FString::Printf(TEXT("Renderer index %d has no matching expected_renderers safety contract"), TargetIndex));
        }
    }
    for (int32 RendererIndex : ClearMeshesBindingRendererIndices)
    {
        const FExpectedRendererSpec& Spec = ExpectedSpecs.FindChecked(RendererIndex);
        if (!Spec.bHasMeshesBindingParameter)
        {
            return W.Error(
                FString::Printf(
                    TEXT("Renderer index %d requires expected_renderers.meshes_binding_parameter before MeshesBinding can be cleared"),
                    RendererIndex));
        }
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    TargetEmitterName.TrimStartAndEndInline();

    int32 ExpectedRendererCount = INDEX_NONE;
    Params->TryGetNumberField(TEXT("expected_renderer_count"), ExpectedRendererCount);

    bool bDryRun = true;
    Params->TryGetBoolField(TEXT("dry_run"), bDryRun);
    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bWaitForCompletion = false;
    Params->TryGetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
    double TimeoutSeconds = 10.0;
    Params->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
    double PollIntervalSeconds = 0.1;
    Params->TryGetNumberField(TEXT("poll_interval_seconds"), PollIntervalSeconds);

    W.R.bSaveRequested=bSave; W.Writer=!bDryRun;
    if (!bDryRun && bRequestCompile && bWaitForCompletion && !W.DeferredMode)
    { W.Fail(TEXT("niagara_renderer_async_wait_unsupported"),TEXT("Synchronous wait_for_completion=True is not implemented; no renderer mutation/compile/save was attempted. The original wait contract remains incomplete.")); return W.Error(W.R.Error); }
    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    FResolvedNiagaraEmitterTarget Target;
    int32 MatchCount = 0;
    if (!ResolveSingleNiagaraEmitterTarget(System, TargetEmitterIndex, TargetEmitterName, Target, MatchCount))
    {
        return W.Error(
            MatchCount > 1 ? TEXT("Emitter selector matched more than one emitter") : TEXT("No matching Niagara emitter was found"));
    }
    if (!Target.Emitter || !Target.Data)
    {
        return W.Error(TEXT("Matched Niagara emitter is not editable"));
    }

    const TArray<UNiagaraRendererProperties*> RenderersBefore = Target.Data->GetRenderers();
    if (ExpectedRendererCount != INDEX_NONE && ExpectedRendererCount != RenderersBefore.Num())
    {
        return W.Error(
            FString::Printf(TEXT("Renderer count changed before cleanup. Expected %d, found %d"), ExpectedRendererCount, RenderersBefore.Num()));
    }

    for (const TPair<int32, FExpectedRendererSpec>& Pair : ExpectedSpecs)
    {
        const int32 RendererIndex = Pair.Key;
        if (!RenderersBefore.IsValidIndex(RendererIndex) || !RenderersBefore[RendererIndex])
        {
            return W.Error(
                FString::Printf(TEXT("Expected renderer index %d is missing"), RendererIndex));
        }

        const UNiagaraRendererProperties* Renderer = RenderersBefore[RendererIndex];
        const FString ActualClass = Renderer->GetClass()->GetName();
        const FString& ExpectedClass = Pair.Value.RendererClass;
        const bool bClassMatches =
            ActualClass.Equals(ExpectedClass, ESearchCase::IgnoreCase) ||
            ExpectedClass.EndsWith(FString::Printf(TEXT(".%s"), *ActualClass), ESearchCase::IgnoreCase);
        if (!bClassMatches || Renderer->GetIsEnabled() != Pair.Value.bEnabled)
        {
            return W.Error(
                FString::Printf(
                    TEXT("Renderer %d safety contract mismatch. Expected class=%s enabled=%s, found class=%s enabled=%s"),
                    RendererIndex,
                    *ExpectedClass,
                    Pair.Value.bEnabled ? TEXT("true") : TEXT("false"),
                    *ActualClass,
                    Renderer->GetIsEnabled() ? TEXT("true") : TEXT("false")));
        }
    }

    for (int32 RendererIndex : ClearMeshesBindingRendererIndices)
    {
        const UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(RenderersBefore[RendererIndex]);
        if (!MeshRenderer)
        {
            return W.Error(
                FString::Printf(TEXT("Renderer index %d is not a Niagara Mesh Renderer"), RendererIndex));
        }
        const FString ResolvedBindingName = MeshRenderer->MeshesBinding.ResolvedParameter.GetName().ToString();
        const FString AliasedBindingName = MeshRenderer->MeshesBinding.AliasedParameter.GetName().ToString();
        const FString& ExpectedBindingName = ExpectedSpecs.FindChecked(RendererIndex).MeshesBindingParameter;
        if (!ResolvedBindingName.Equals(ExpectedBindingName, ESearchCase::IgnoreCase) &&
            !AliasedBindingName.Equals(ExpectedBindingName, ESearchCase::IgnoreCase))
        {
            return W.Error(
                FString::Printf(
                    TEXT("Renderer %d MeshesBinding safety contract mismatch. Expected %s, found resolved=%s aliased=%s"),
                    RendererIndex,
                    *ExpectedBindingName,
                    *ResolvedBindingName,
                    *AliasedBindingName));
        }
        const bool bHasValidLocalMesh = MeshRenderer->Meshes.ContainsByPredicate(
            [](const FNiagaraMeshRendererMeshProperties& MeshEntry)
            {
                return MeshEntry.Mesh != nullptr;
            });
        if (!bHasValidLocalMesh)
        {
            return W.Error(
                FString::Printf(TEXT("Renderer index %d has no valid renderer-local fallback mesh; refusing to clear MeshesBinding"), RendererIndex));
        }
    }

    TArray<TSharedPtr<FJsonValue>> BeforeValues;
    TArray<UNiagaraRendererProperties*> RemovedRendererObjects;
    TArray<UNiagaraMeshRendererProperties*> ClearedMeshRendererObjects;
    for (int32 RendererIndex = 0; RendererIndex < RenderersBefore.Num(); ++RendererIndex)
    {
        BeforeValues.Add(MakeShared<FJsonValueObject>(
            BuildRendererJson(W, System, Target.Name, Target.Index, RenderersBefore[RendererIndex], RendererIndex)));
        if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
        if (RemoveRendererIndices.Contains(RendererIndex))
        {
            RemovedRendererObjects.Add(RenderersBefore[RendererIndex]);
        }
        if (ClearMeshesBindingRendererIndices.Contains(RendererIndex))
        {
            ClearedMeshRendererObjects.Add(CastChecked<UNiagaraMeshRendererProperties>(RenderersBefore[RendererIndex]));
        }
    }

    TSharedPtr<FJsonObject> WaitObject = MakeShared<FJsonObject>();
    WaitObject->SetBoolField(TEXT("wait_for_completion"), bWaitForCompletion);
    bool bCompileRequested = false;
    bool bCompileWaitPass = true;
    bool bSaved = false;

    if (!bDryRun)
    {
        if (!W.AdmitOwnedEmitter(Target.Emitter)) return W.Error(W.R.Error);
        for (const auto* Renderer : RemovedRendererObjects) if (!W.AdmitOwnedEmitter(Target.Emitter,Renderer)) return W.Error(W.R.Error);
        for (const auto* Renderer : ClearedMeshRendererObjects) if (!W.AdmitOwnedEmitter(Target.Emitter,Renderer)) return W.Error(W.R.Error);
        GE_STEP("System_Modify",true,System->Modify());
        GE_STEP("TargetEmitter_Modify",true,Target.Emitter->Modify());

        for (int32 RendererIndex : ClearMeshesBindingRendererIndices)
        {
            UNiagaraMeshRendererProperties* MeshRenderer = CastChecked<UNiagaraMeshRendererProperties>(RenderersBefore[RendererIndex]);
            GE_STEP("MeshRenderer_Modify",true,MeshRenderer->Modify());
            GE_STEP("clear_meshes_binding",true,MeshRenderer->MeshesBinding.SetDefaultParameter(FNiagaraVariable()));
            GE_STEP("MeshRenderer_PostEditChange",true,MeshRenderer->PostEditChange());
        }

        RemoveRendererIndices.Sort(TGreater<int32>());
        const FGuid EmitterVersionGuid = Target.Data->Version.VersionGuid;
        for (int32 RendererIndex : RemoveRendererIndices)
        {
            UNiagaraRendererProperties* Renderer = Target.Data->GetRenderers()[RendererIndex];
            GE_STEP("Renderer_Modify",true,Renderer->Modify());
            if (!W.ChangeTopology(Target.Emitter,EmitterVersionGuid,Renderer,false)) return W.Error(W.R.Error);
        }
        GE_STEP("TargetEmitter_PostEditChange",true,Target.Emitter->PostEditChange());

        if (UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
        {
            if (!W.Pin(SystemEditorData)) return W.Error(W.R.Error);
        GE_STEP("synchronize_overview",true,SystemEditorData->SynchronizeOverviewGraphWithSystem(*System));
        }

        GE_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
        if(W.DeferredMode){W.DeferredShape=Project(W);if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);W.DeferredShape->RemoveField(TEXT("dirty"));}
        if (bRequestCompile)
        {
            if (!W.Request(System,bCompileRequested)) return W.Error(W.R.Error);
            WaitObject = WaitForNiagaraCompileJson(W, System, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds);
            if (bWaitForCompletion && !W.DeferredMode)
            {
                bCompileWaitPass =
                    !WaitObject->GetBoolField(TEXT("wait_timed_out")) &&
                    !WaitObject->GetBoolField(TEXT("outstanding_compilation_requests_after")) &&
                    !WaitObject->GetBoolField(TEXT("active_compilations_after"));
            }
        }

    }

    auto CompleteCleanup=[&W,System,Target,BeforeValues,RemovedRendererObjects,ClearedMeshRendererObjects,RemoveRendererIndices,ClearMeshesBindingRendererIndices,WaitObject,bCompileRequested,bCompileWaitPass,bSave,bDryRun,bWaitForCompletion,bRequestCompile,bSaved]() mutable -> TSharedPtr<FJsonObject>
    {
        if(W.DeferredMode&&!W.ValidateDeferredState(TEXT("renderer_cleanup_resume")))return W.Error(W.R.Error);
        if(W.DeferredMode&&bRequestCompile&&bWaitForCompletion)bCompileWaitPass=!WaitObject->GetBoolField(TEXT("wait_timed_out"))&&!WaitObject->GetBoolField(TEXT("outstanding_compilation_requests_after"))&&!WaitObject->GetBoolField(TEXT("active_compilations_after"));
    TArray<TSharedPtr<FJsonValue>> AfterValues;
    const TArray<UNiagaraRendererProperties*> RenderersAfter = Target.Data->GetRenderers();
    for (int32 RendererIndex = 0; RendererIndex < RenderersAfter.Num(); ++RendererIndex)
    {
        AfterValues.Add(MakeShared<FJsonValueObject>(
            BuildRendererJson(W, System, Target.Name, Target.Index, RenderersAfter[RendererIndex], RendererIndex)));
        if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    }

    bool bVerificationPass = true;
    FString VerificationError;
    if (!bDryRun)
    {
        const int32 ExpectedCountAfter = BeforeValues.Num() - RemovedRendererObjects.Num();
        if (RenderersAfter.Num() != ExpectedCountAfter)
        {
            bVerificationPass = false;
            VerificationError = FString::Printf(
                TEXT("Renderer cleanup count mismatch. Expected %d, found %d"), ExpectedCountAfter, RenderersAfter.Num());
        }
        for (UNiagaraRendererProperties* RemovedRenderer : RemovedRendererObjects)
        {
            if (RenderersAfter.Contains(RemovedRenderer))
            {
                bVerificationPass = false;
                VerificationError = TEXT("A requested renderer remains attached after cleanup");
                break;
            }
        }
        for (UNiagaraMeshRendererProperties* ClearedRenderer : ClearedMeshRendererObjects)
        {
            if (!RenderersAfter.Contains(ClearedRenderer) || ClearedRenderer->MeshesBinding.ResolvedParameter.IsValid())
            {
                bVerificationPass = false;
                VerificationError = TEXT("A requested Mesh Renderer binding was not cleared or its renderer was removed unexpectedly");
                break;
            }
        }
        if (!bVerificationPass)
        {
            return W.Error(VerificationError);
        }
        if (!bCompileWaitPass)
        {
            TSharedPtr<FJsonObject> Error = W.Error(
                TEXT("Niagara compile did not reach an idle state before timeout; renderer changes were not saved"));
            Error->SetBoolField(TEXT("compile_requested"), bCompileRequested);
            Error->SetBoolField(TEXT("compile_wait_pass"), false);
            Error->SetObjectField(TEXT("compile_wait"), WaitObject);
            Error->SetBoolField(TEXT("saved"), false);
            Error->SetStringField(TEXT("write_scope"), TEXT("renderer_cleanup_unsaved_compile_failure"));
            return Error;
        }
        if (bSave)
        {
            bSaved = W.Save(System);
            if (!bSaved)
            {
                return W.Error(TEXT("Failed to save Niagara system after renderer cleanup"));
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    auto RendererIndicesToJsonValues = [](const TArray<int32>& RendererIndices)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Reserve(RendererIndices.Num());
        for (int32 RendererIndex : RendererIndices)
        {
            Values.Add(MakeShared<FJsonValueNumber>(RendererIndex));
        }
        return Values;
    };
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("dry_run"), bDryRun);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), Target.Name);
    Result->SetNumberField(TEXT("emitter_index"), Target.Index);
    Result->SetNumberField(TEXT("renderer_count_before"), BeforeValues.Num());
    Result->SetNumberField(TEXT("renderer_count_after"), AfterValues.Num());
    Result->SetArrayField(TEXT("remove_renderer_indices"), RendererIndicesToJsonValues(RemoveRendererIndices));
    Result->SetArrayField(TEXT("clear_meshes_binding_renderer_indices"), RendererIndicesToJsonValues(ClearMeshesBindingRendererIndices));
    Result->SetArrayField(TEXT("renderers_before"), BeforeValues);
    Result->SetArrayField(TEXT("renderers_after"), AfterValues);
    Result->SetNumberField(TEXT("removed_count"), bDryRun ? 0 : RemovedRendererObjects.Num());
    Result->SetNumberField(TEXT("cleared_count"), bDryRun ? 0 : ClearedMeshRendererObjects.Num());
    Result->SetNumberField(TEXT("would_remove_count"), bDryRun ? RemovedRendererObjects.Num() : 0);
    Result->SetNumberField(TEXT("would_clear_count"), bDryRun ? ClearedMeshRendererObjects.Num() : 0);
    Result->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    Result->SetStringField(TEXT("verification_error"), VerificationError);
    Result->SetBoolField(TEXT("compile_requested"), bCompileRequested);
    Result->SetBoolField(TEXT("compile_wait_pass"), bCompileWaitPass);
    Result->SetObjectField(TEXT("compile_wait"), WaitObject);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), bDryRun ? TEXT("renderer_cleanup_dry_run") : TEXT("renderer_cleanup"));
    if (!bDryRun && W.Admit(TEXT("cleanup_final_readback")))
    {
        const auto& FinalRenderers = Target.Data->GetRenderers();
        bool FinalPass = FinalRenderers.Num()==BeforeValues.Num()-RemovedRendererObjects.Num();
        for (auto* Removed : RemovedRendererObjects) FinalPass &= !FinalRenderers.Contains(Removed);
        for (auto* Cleared : ClearedMeshRendererObjects)
            FinalPass &= FinalRenderers.Contains(Cleared) && !Cleared->MeshesBinding.ResolvedParameter.IsValid() && !Cleared->MeshesBinding.AliasedParameter.IsValid();
        W.R.Data->SetBoolField(TEXT("typed_readback_pass"),FinalPass);
        if (!FinalPass) W.Fail(TEXT("niagara_renderer_cleanup_readback_failed"),TEXT("Renderer detach/binding cleanup did not remain applied after save callbacks; saved history is retained."));
    }
    return Result;
    };
    if(W.WaitDeferred){W.CleanupTail=MoveTemp(CompleteCleanup);auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);Pending->SetBoolField(TEXT("dry_run"),false);Pending->SetObjectField(TEXT("compile_wait"),WaitObject);Pending->SetBoolField(TEXT("saved"),W.R.bSaved);return Pending;}
    return CompleteCleanup();
}

FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
    bool Success=false,Verification=true;if(Legacy){Legacy->TryGetBoolField(TEXT("success"),Success);Legacy->TryGetBoolField(TEXT("verification_pass"),Verification);}
    bool ScopedObservationComplete=false;
    if(W.Before)
    {
        auto After=Project(W);W.R.Data->SetObjectField(TEXT("before"),W.Before);W.R.Data->SetObjectField(TEXT("after"),After);
        bool Skipped=false;After->TryGetBoolField(TEXT("projection_skipped_unsafe"),Skipped);
        if(!Skipped)
        {
            W.R.bChanged|=!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive);
            // Project is the dedicated typed target snapshot. A final admission keeps callback/identity failures unknown.
            ScopedObservationComplete=After->HasField(TEXT("system_path"))&&After->HasField(TEXT("package"))&&After->HasField(TEXT("dirty"))&&After->HasField(TEXT("emitters"))&&W.R.Error.IsEmpty()&&W.Admit(TEXT("final_scoped_observation"));
        }
    }
    if(!Success&&W.R.Error.IsEmpty())
    {FString Error;if(!Legacy||!Legacy->TryGetStringField(TEXT("error"),Error)){Error=TEXT("Original renderer operation returned failure; see legacy_result.");}W.Fail(TEXT("niagara_renderer_request_failed"),Error);}
    W.R.bSuccess=Success&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess&&Verification;
    bool TypedReadbackPassed=true;W.R.Data->TryGetBoolField(TEXT("typed_readback_pass"),TypedReadbackPassed);
    const bool ScopedKnown=W.R.bSuccess&&W.R.bPostconditionPassed&&TypedReadbackPassed&&ScopedObservationComplete;
    if(ScopedKnown){W.R.bChangeStateKnown=true;}
    W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),ScopedObservationComplete);
    W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),ScopedKnown);
    W.R.Data->SetStringField(TEXT("change_state_known_scope"),TEXT("Known only for captured target renderer topology/typed properties/package dirtiness and returned creation prefix after successful postconditions and final admission. SDK/global side effects and asynchronous compile/render completion are separate and remain unverified."));
    W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;
    W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());
    W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);
    W.R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("Current request SaveLoadedAsset returned true; this history survives a later post-Guard/readback failure. It is not an atomic or whole-SDK persistence guarantee."));
    W.R.Data->SetStringField(TEXT("changed_observation_scope"),TEXT("Captured renderer topology/typed properties/package dirtiness and returned creation prefix; SDK/global side effects remain unknown."));
    W.R.Data->SetStringField(TEXT("compile_scope"),W.DeferredMode?TEXT("Original requested compile wait uses finite GT frames: busy Poll/Process then current GPU-inclusive status and idle/deadline. Timeout fails the original save gate; idle is not VM/shader success or render readiness. SDK bodies may block; no replay, global cancellation or GT Sleep."):TEXT("RequestCompile acceptance is distinct from completion; no shader, GPU, render readiness, or wait completion is asserted."));
    W.R.Data->SetStringField(TEXT("selection_scope"),TEXT("Original UE5.6 editable versioned emitter predicates and physical indices are preserved; Stateless authoring is outside that original contract."));
    W.R.Data->SetStringField(TEXT("legacy_binding_projection_scope"),TEXT("Legacy binding fields retain the original UE5.6 is_set_to_default unavailable note; it is not a claim that UE5.8 lacks a public getter."));
    W.R.Data->SetBoolField(TEXT("postcondition_passed"),W.R.bPostconditionPassed);
    return Operations::EncodeVariableResult(W.R,Operation);
}

namespace HN=NiagaraScratchUserRecipeAsync;
using FCleanupJob=UCubelessNiagaraScratchUserRecipeJob;
struct FRendererCleanupContinuation;
TSharedPtr<FRendererCleanupContinuation> CleanupPending;
TArray<TSharedPtr<FRendererCleanupContinuation>> CleanupUnsafe;
bool CleanupStarting=false;
bool HasCleanupLease(){return CleanupStarting||CleanupPending.IsValid()||!CleanupUnsafe.IsEmpty();}
struct FRendererCleanupContinuation:TSharedFromThis<FRendererCleanupContinuation>
{
    FWork W{true};TWeakObjectPtr<FCleanupJob> Job;FCleanupJob* Key=nullptr;FGuid Id=FGuid::NewGuid();
    FString JobPath,LastJson,CancelReason;TSharedPtr<FJsonObject> Params,Legacy;
    FTSTicker::FDelegateHandle Ticker;int64 Revision=0;uint64 LastFrame=MAX_uint64;
    bool Executing=false,Dispatched=false,Terminal=false,Quarantined=false;
    const TCHAR* Operation=TEXT("cleanup_niagara_renderers");
    FRendererCleanupContinuation(){W.DeferredMode=true;W.R.Data->SetBoolField(TEXT("all_SDK_calls_nonblocking_proven"),false);W.R.Data->SetBoolField(TEXT("external_transaction_across_yield_allowed"),false);}
    void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
    bool PublicationSafe()const
    {if(!HN::CanPublishJobObject())return false;if(!W.WorldCaptured)return true;if(W.CapturedEditor.Get()!=GEditor)return false;auto* World=GEditor->GetEditorWorldContext().World();return Live(World)&&!World->bIsTearingDown&&W.World.Get()==World&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext();}
    bool JobExact()const
    {return PublicationSafe()&&Job.Get()==Key&&Live(Key)&&Key->GetClass()==FCleanupJob::StaticClass()&&Key->GetOuter()==GetTransientPackage()&&Key->GetOutermost()==GetTransientPackage()&&Key->GetPackage()==GetTransientPackage()&&Key->HasAnyFlags(RF_Transient)&&Key->GetPathName().Equals(JobPath,ESearchCase::CaseSensitive);}
    void Prefix(bool Final)
    {
        auto& D=W.R.Data;D->SetStringField(TEXT("operation"),Operation);D->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::Digits));D->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);D->SetBoolField(TEXT("native_job_terminal"),Final);D->SetBoolField(TEXT("operation_complete"),Final);D->SetBoolField(TEXT("native_writers_stopped"),Final||Quarantined);
        D->SetBoolField(TEXT("genuine_compile_wait_implemented"),true);D->SetBoolField(TEXT("required_wait_idle_observed"),W.WaitDeferred&&W.WaitIdle);D->SetBoolField(TEXT("compiler_may_continue_after_job"),W.R.bCompileAttempted&&!W.WaitIdle);D->SetBoolField(TEXT("global_compiler_cancel_attempted"),false);D->SetBoolField(TEXT("single_core_dispatch"),Dispatched);D->SetBoolField(TEXT("automatic_retry_permitted"),false);
        if(Legacy)D->SetObjectField(TEXT("legacy_result"),Legacy);if(W.Before)D->SetObjectField(TEXT("before"),W.Before);if(W.CleanupWait)D->SetObjectField(TEXT("compile_wait"),W.CleanupWait);
        D->SetObjectField(TEXT("actual_mutation_prefix"),W.Prefix);D->SetObjectField(TEXT("actual_compile_prefix"),W.CompilePrefix);D->SetObjectField(TEXT("actual_save_prefix"),W.SavePrefix);
    }
    void Pending()
    {W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;Prefix(false);LastJson=Operations::EncodeVariableResult(W.R,Operation);HN::Publish(Key,LastJson,false,JobExact());}
    void Unsafe(const FString& Reason)
    {if(Terminal||Quarantined)return;Unhook();Quarantined=true;W.CancelRequested=true;W.Fail(TEXT("niagara_renderer_cleanup_unsafe_publication"),Reason);W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;W.R.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);Prefix(false);LastJson=Operations::EncodeVariableResult(W.R,Operation);HN::Publish(Key,LastJson,false,false);if(!CleanupUnsafe.Contains(AsShared()))CleanupUnsafe.Add(AsShared());}
    void ReleaseSafe()
    {if(!PublicationSafe())return;for(auto& I:W.Objects)I->Strong.Reset();W.Packages.Empty();}
    void Final()
    {
        if(Terminal||Quarantined)return;if(!JobExact()){Unsafe(TEXT("Exact Job/Editor/world unavailable; no new target observation or terminal property publication."));return;}
        if(W.R.Error.IsEmpty()&&!W.ValidateDeferredState(TEXT("renderer_cleanup_final_retained_state"))){}
        Prefix(true);
        if(!W.R.Error.IsEmpty())
        {W.R.bSuccess=W.R.bPostconditionPassed=false;W.R.bChangeStateKnown=false;W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),false);auto Skipped=MakeShared<FJsonObject>();Skipped->SetBoolField(TEXT("projection_skipped_after_error"),true);W.R.Data->SetObjectField(TEXT("after"),Skipped);LastJson=Operations::EncodeVariableResult(W.R,Operation);}
        else LastJson=Finish(W,Legacy,Operation);
        if(!JobExact()){Unsafe(TEXT("Final observation returned across unsafe context; actual prefix cached without terminal publication."));return;}
        Terminal=true;Unhook();HN::Publish(Key,LastJson,true,true);ReleaseSafe();if(CleanupPending.Get()==this)CleanupPending.Reset();
    }
    void CancelPlain(const FString& Reason)
    {if(Terminal||Quarantined)return;W.CancelRequested=true;CancelReason=Reason.Left(8192);if(Executing)return;Unhook();W.Fail(TEXT("niagara_renderer_cleanup_cancelled"),CancelReason);if(!PublicationSafe()){Unsafe(CancelReason);return;}Final();}
    bool Step()
    {
        if(Terminal||Quarantined)return false;if(!PublicationSafe()){Unsafe(TEXT("Unsafe GT/GC/exit/save/Editor/world; no later target/compiler SDK."));return false;}
        if(Executing||LastFrame==GFrameCounter)return true;LastFrame=GFrameCounter;
        if(Running){W.Fail(TEXT("niagara_renderer_cleanup_core_busy"),TEXT("Another renderer request owns the exact core; no nested target SDK."));Final();return false;}
        if(HN::IsStopping()||W.CancelRequested){CancelPlain(TEXT("Concrete renderer cleanup stopped; shared compiler queue was not cancelled."));return false;}
        if(!JobExact()){Unsafe(TEXT("Exact Job identity changed; no restart."));return false;}
        TGuardValue<bool> Execute(Executing,true);TGuardValue<bool> Lease(Running,true);
        if(!W.Admit(TEXT("renderer_cleanup_frame_admission"))){Final();return false;}
        if(!Dispatched){Dispatched=true;Legacy=HandleCleanupNiagaraRenderers(W,Params);}
        else if(W.WaitDeferred)
        {
            if(!W.AdvanceWait()){Final();return false;}
            if(W.WaitDone){auto Tail=MoveTemp(W.CleanupTail);if(Tail)Legacy=Tail();else W.Fail(TEXT("niagara_renderer_cleanup_tail_missing"),TEXT("One original cleanup suffix was required; no replay or synthetic success."));}
        }
        if(!PublicationSafe()){Unsafe(TEXT("SDK callback invalidated publication; current actual history retained."));return false;}
        if(!W.R.Error.IsEmpty()||W.CancelRequested){Final();return false;}
        if(!W.WaitDeferred||W.WaitDone){Final();return false;}Pending();return !Quarantined;
    }
};

using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,bool Writer,FHandler Handler)
{
    FWork W(Writer);TSharedPtr<FJsonObject> Params;
    if(!ParseParams(Text,Params))
    {W.Fail(TEXT("niagara_renderer_invalid_params_json"),TEXT("Expected one finite JSON object: maximum 1 Mi UTF16 units, depth64, 262144 visits/tokens, no duplicate case-sensitive keys or embedded NUL."));return Finish(W,W.Error(W.R.Error),Operation);}
    if(Running||HasCleanupLease()){W.Fail(TEXT("niagara_renderer_reentrant_request"),TEXT("Another dedicated renderer request is active; no replay."));return Finish(W,W.Error(W.R.Error),Operation);}
    TGuardValue<bool> Lease(Running,true);return Finish(W,Handler(W,Params),Operation);
}
#undef GE_STEP
}

UCubelessNiagaraScratchUserRecipeJob* StartCleanupContinuation(const FString& Text)
{
    using namespace NiagaraScratchUserRecipeAsync;
    if(!CanPublishJobObject()||IsStopping()||HasCleanupLease())return nullptr;
    TGuardValue<bool> Starting(CleanupStarting,true);auto State=MakeShared<FRendererCleanupContinuation>();
    if(!ParseParams(Text,State->Params))State->W.Fail(TEXT("niagara_renderer_invalid_params_json"),TEXT("Expected original finite renderer params within1Mi UTF16/depth64/262144 notation tokens and values."));
    if(Running)State->W.Fail(TEXT("niagara_renderer_reentrant_request"),TEXT("Existing synchronous renderer core owns the lease; no nested target SDK."));
    if(State->W.R.Error.IsEmpty())State->W.Admit(TEXT("renderer_cleanup_before_job_creation"));
    if(!CanPublishJobObject())return nullptr;
    auto* Created=NewObject<UCubelessNiagaraScratchUserRecipeJob>(GetTransientPackage(),NAME_None,RF_Transient);State->Key=Created;
    State->W.R.bMutationAttempted=true;State->W.R.Data->SetBoolField(TEXT("transient_job_allocation_returned"),Created!=nullptr);State->W.R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);State->W.R.Data->SetBoolField(TEXT("transient_creation_only"),true);State->W.R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);
    CleanupPending=State;if(!State->PublicationSafe()||!Live(Created)){State->Unsafe(TEXT("Job allocation returned across unsafe context; raw cache/lease retained without weak adoption or target SDK."));return Created;}
    State->Job=Created;State->JobPath=Created->GetPathName();RegisterJob(Created,EJobKind::RendererCleanup,State->Id);if(!State->JobExact()){State->Unsafe(TEXT("Exact Job/context unavailable after registration; no target dispatch."));return Created;}
    if(!State->W.R.Error.IsEmpty()){State->Legacy=State->W.Error(State->W.R.Error);State->Final();return Created;}
    State->Pending();State->Step();if(!State->Terminal&&!State->Quarantined){TWeakPtr<FRendererCleanupContinuation> Weak=State;State->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float){auto Strong=Weak.Pin();return Strong.IsValid()&&Strong->Step();}));}return Created;
}
bool CancelCleanupContinuation(UCubelessNiagaraScratchUserRecipeJob* Job,const FString& Reason)
{if(!IsInGameThread()||!CleanupPending.IsValid()||CleanupPending->Key!=Job)return false;auto State=CleanupPending;State->CancelPlain(Reason);return true;}
void ShutdownCleanupContinuation(const FString& Reason)
{if(!IsInGameThread())return;if(auto State=CleanupPending){State->Unhook();State->CancelPlain(Reason);}}
bool HasCleanupContinuationLease(){return HasCleanupLease();}

FString Inspect(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_renderers"),false,&HandleInspectNiagaraRenderers);}
FString SetMaterial(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_renderer_material"),true,&HandleSetNiagaraRendererMaterial);}
FString SetSpriteSettings(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_sprite_renderer_settings"),true,&HandleSetNiagaraSpriteRendererSettings);}
FString AddSprite(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("add_niagara_sprite_renderer"),true,&HandleAddNiagaraSpriteRenderer);}
FString AddMesh(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("add_niagara_mesh_renderer"),true,&HandleAddNiagaraMeshRenderer);}
FString SetEnabled(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_renderer_enabled"),true,&HandleSetNiagaraRendererEnabled);}
FString Cleanup(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("cleanup_niagara_renderers"),true,&HandleCleanupNiagaraRenderers);}
}
