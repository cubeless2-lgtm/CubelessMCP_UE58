// GF dedicated Editor-only emitter configuration. Source candidate; no Engine/build proof.
#include "Compatibility/UE58/CubelessNiagaraEmitterConfigurationApi.h"
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
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "NiagaraCommon.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraPlatformSet.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEditorData.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
namespace Cubeless::UE58::NiagaraEmitterConfiguration
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running=false;
bool DeferredLeaseActive=false;
bool Safe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage;}
constexpr const TCHAR* NiagaraTempGenerationRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated/");
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O){FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));}return S;}
FString NormalizeNiagaraObjectPathForLoad(const FString&);
template<typename T>FString NiagaraEnumValueName(T Value){const UEnum* E=StaticEnum<T>();return E?E->GetNameStringByValue(static_cast<int64>(Value)):FString::FromInt(static_cast<int32>(Value));}
TSharedPtr<FJsonObject> NiagaraEmitterPropertiesToJsonObject(const FVersionedNiagaraEmitterData* Data);
TSharedPtr<FJsonObject> DataState(const FVersionedNiagaraEmitterData* D)
{
    auto O=MakeShared<FJsonObject>();if(!D){return O;}O->SetBoolField(TEXT("local_space"),D->bLocalSpace);O->SetNumberField(TEXT("sim_target"),static_cast<int32>(D->SimTarget));O->SetNumberField(TEXT("bounds_mode"),static_cast<int32>(D->CalculateBoundsMode));O->SetBoolField(TEXT("persistent_ids"),D->bRequiresPersistentIDs!=0);O->SetBoolField(TEXT("bounds_valid"),D->FixedBounds.IsValid!=0);
    TArray<TSharedPtr<FJsonValue>> Bounds;for(double V:{D->FixedBounds.Min.X,D->FixedBounds.Min.Y,D->FixedBounds.Min.Z,D->FixedBounds.Max.X,D->FixedBounds.Max.Y,D->FixedBounds.Max.Z}){Bounds.Add(MakeShared<FJsonValueNumber>(V));}O->SetArrayField(TEXT("bounds"),Bounds);O->SetBoolField(TEXT("determinism"),D->bDeterminism);O->SetNumberField(TEXT("random_seed"),D->RandomSeed);return O;
}
struct FIdentity
{
    TStrongObjectPtr<UObject> Strong{nullptr};TWeakObjectPtr<UObject> Weak,Outer;TWeakObjectPtr<UPackage> Package,EffectivePackage;TWeakObjectPtr<UClass> Class;FString Path;bool Retired=false;
    explicit FIdentity(UObject* O):Strong(O),Weak(O),Outer(O->GetOuter()),Package(O->GetOutermost()),EffectivePackage(O->GetPackage()),Class(O->GetClass()),Path(O->GetPathName()){}
    bool Exact()const{if(Retired){return true;}UObject* O=Weak.Get();return Live(O)&&Live(Package.Get())&&Live(Class.Get())&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetClass()==Class.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}
};
struct FEmitterIdentity
{FGuid Handle,Version,InstanceVersion;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;FString Name;int32 Index=INDEX_NONE;bool Enabled=false;uint8 Mode=0;TArray<TWeakObjectPtr<UNiagaraRendererProperties>> Slots;};
struct FCompileBinding{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraGraph> Graph;FGuid Version;};
struct FWork
{
 bool DeferredMode=false,CancelRequested=false,PersistentMutationAttempted=false;FString CancelReason;TWeakObjectPtr<UEditorEngine> CapturedEditor;TFunction<bool()> DeferredStep;TArray<FCompileBinding> CompileRouting;
    FResult R;bool Writer=false,WorldCaptured=false;FString FailedStage;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UNiagaraSystem> System;FString SystemPath,PackagePath;bool WriterEmitterCaptured=false;TWeakObjectPtr<UNiagaraEmitter> WriterEmitter;
    TArray<TUniquePtr<FIdentity>> Objects;TArray<FEmitterIdentity> Emitters;TMap<FGuid,TSharedPtr<FJsonObject>> Desired;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>(),SavePrefix=MakeShared<FJsonObject>(),CompilePrefix=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> Before;
    explicit FWork(bool W):Writer(W){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetObjectField(TEXT("actual_save_prefix"),SavePrefix);R.Data->SetObjectField(TEXT("actual_compile_prefix"),CompilePrefix);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("actual_render_completion_verified"),false);R.Data->SetBoolField(TEXT("wrapper_sleep_or_forced_flush"),false);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    bool Pin(UObject* O){if(!Live(O)){return Fail(TEXT("niagara_emitter_object_unavailable"),TEXT("Expected a live loaded Engine object."));}for(const auto& I:Objects){if(I->Weak.Get()==O&&!I->Retired){return I->Exact();}}Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool Admit(const FString& Stage,bool ObserveFailure=false)
    {
        R.Stage=Stage;if(DeferredMode&&(CancelRequested||NiagaraEmitterEventAuthoringAsync::IsStopping()))return Fail(TEXT("niagara_emitter_async_cancelled"),CancelReason.IsEmpty()?TEXT("Owned continuation stopped; no further SDK."):CancelReason);if(!R.Error.IsEmpty()&&!ObserveFailure){return false;}
        if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||!GEditor||IsGarbageCollecting()||GIsSavingPackage){return Fail(TEXT("niagara_emitter_context_unavailable"),TEXT("Live Editor game thread without GC/exit/package save is required; no replay."));}
        if(DeferredMode&&WorldCaptured&&CapturedEditor.Get()!=GEditor)return Fail(TEXT("niagara_emitter_editor_changed"),TEXT("Captured Editor changed before getter."));
        if(Writer&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld)){return Fail(TEXT("niagara_emitter_play_session_active"),TEXT("Emitter authoring requires stopped PIE/SIE."));}
        UWorld* Current=GEditor->GetEditorWorldContext().World();if(!Live(Current)||Current->bIsTearingDown){return Fail(TEXT("niagara_emitter_world_unavailable"),TEXT("Editor world is unavailable or tearing down."));}
        if(!WorldCaptured){World=Current;if(DeferredMode)CapturedEditor=GEditor;WorldCaptured=true;}else if(!World.IsValid()||World.Get()!=Current){return Fail(TEXT("niagara_emitter_world_changed"),TEXT("Editor world changed across SDK dispatch."));}
        for(const auto& O:Objects){if(!O->Exact()||(DeferredMode&&!O->Retired&&O->Weak->GetPackage()!=O->EffectivePackage.Get())){return Fail(TEXT("niagara_emitter_object_identity_changed"),TEXT("Captured class/outer/package/path changed; no replacement was adopted."));}}
        if(DeferredMode)for(const auto& B:CompileRouting){auto* S=B.Script.Get();if(!Live(S)||S->GetLatestSource()!=B.Source.Get()||S->GetExposedVersion().VersionGuid!=B.Version)return Fail(TEXT("niagara_emitter_compile_routing_changed"),TEXT("Captured Script/source/version changed."));if(auto* NS=Cast<UNiagaraScriptSource>(B.Source.Get()))if(NS->NodeGraph!=B.Graph.Get())return Fail(TEXT("niagara_emitter_compile_graph_changed"),TEXT("Captured source graph routing changed."));}
        if(SystemPath.IsEmpty()){return true;}UNiagaraSystem* S=System.Get();if(!Live(S)||!S->GetOutermost()->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)){return Fail(TEXT("niagara_emitter_system_changed"),TEXT("Captured system/package changed."));}
        if(WriterEmitterCaptured)
        {UNiagaraEmitter* E=WriterEmitter.Get();if(!Live(E)||!E->IsIn(S)||E->GetPackage()!=S->GetPackage()||E->GetOutermost()!=S->GetOutermost()){return Fail(TEXT("niagara_emitter_external_writer_unsupported"),TEXT("Selected emitter is not internally owned by the specified System and its package; no external emitter mutation or implicit duplication is allowed."));}}
        const auto& H=S->GetEmitterHandles();if(H.Num()!=Emitters.Num()){return Fail(TEXT("niagara_emitter_topology_changed"),TEXT("Emitter count changed outside the requested GUID removal."));}
        for(const auto& E:Emitters)
        {
            if(!H.IsValidIndex(E.Index)||H[E.Index].GetId()!=E.Handle||!H[E.Index].GetName().ToString().Equals(E.Name,ESearchCase::CaseSensitive)||H[E.Index].GetIsEnabled()!=E.Enabled||static_cast<uint8>(H[E.Index].GetEmitterMode())!=E.Mode||H[E.Index].GetEmitterData()!=E.Data||H[E.Index].GetInstance().Version!=E.InstanceVersion){return Fail(TEXT("niagara_emitter_handle_changed"),TEXT("Physical handle order/name/enabled/mode/data changed outside the admitted request."));}
            if(E.Data)
            {
                if(!Live(E.Emitter.Get())||H[E.Index].GetInstance().Emitter!=E.Emitter.Get()||E.Emitter->GetEmitterData(E.Version)!=E.Data||E.Data->Version.VersionGuid!=E.Version){return Fail(TEXT("niagara_emitter_version_changed"),TEXT("Editable emitter/version changed; stale data was not used."));}
                const auto& A=E.Data->GetRenderers();if(A.Num()!=E.Slots.Num()){return Fail(TEXT("niagara_emitter_renderer_topology_changed"),TEXT("Renderer count changed during emitter configuration."));}for(int32 I=0;I<A.Num();++I){if(A[I]!=E.Slots[I].Get()){return Fail(TEXT("niagara_emitter_renderer_topology_changed"),TEXT("Physical renderer slot identity/order changed during emitter configuration."));}}
                if(const auto* Expected=Desired.Find(E.Handle))
                {
                    auto Actual=DataState(E.Data);for(const auto& Row:(*Expected)->Values){const FString Key(Row.Key);auto V=Actual->TryGetField(Key);auto AValue=MakeShared<FJsonObject>();AValue->SetField(TEXT("value"),V);auto EValue=MakeShared<FJsonObject>();EValue->SetField(TEXT("value"),Row.Value);if(!V||!Json(AValue).Equals(Json(EValue),ESearchCase::CaseSensitive)){return Fail(TEXT("niagara_emitter_typed_readback_failed"),FString::Printf(TEXT("Requested emitter field did not remain applied: %s"),*Key));}}
                }
            }
        }
        return true;
    }
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage)){return false;}R.bChangeStateKnown=false;R.bMutationAttempted|=Mutation;if(DeferredMode&&Mutation&&FCString::Strcmp(Stage,TEXT("NewObject_EmitterEventJob"))!=0)PersistentMutationAttempted=true;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(Mutation){Prefix->SetBoolField(Stage,true);}return true;}
    bool Returned(const TCHAR* Stage){const bool OK=Admit(FString(TEXT("after_"))+Stage);if(OK&&Before&&Live(System.Get())){bool DirtyBefore=false;Before->TryGetBoolField(TEXT("dirty"),DirtyBefore);R.bChanged|=System->GetOutermost()->IsDirty()!=DirtyBefore;}return OK;}
    TSharedPtr<FJsonObject> Projection()
    {
        auto O=MakeShared<FJsonObject>();if(!Admit(TEXT("current_projection"),true)){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);return O;}if(!Live(System.Get())){return O;}O->SetStringField(TEXT("system_path"),System->GetPathName());O->SetStringField(TEXT("package"),System->GetOutermost()->GetName());O->SetBoolField(TEXT("dirty"),System->GetOutermost()->IsDirty());TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& E:Emitters){auto J=DataState(E.Data);J->SetStringField(TEXT("handle_guid"),E.Handle.ToString());J->SetStringField(TEXT("name"),E.Name);J->SetNumberField(TEXT("index"),E.Index);J->SetBoolField(TEXT("enabled"),System->GetEmitterHandles()[E.Index].GetIsEnabled());Rows.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("emitters"),Rows);return O;
    }
    UNiagaraSystem* LoadSystem(const FString& Path)
    {
        if(!SDK(TEXT("load_system"))){return nullptr;}const FString Expected=NormalizeNiagaraObjectPathForLoad(Path);auto* S=LoadObject<UNiagaraSystem>(nullptr,*Expected);R.Data->SetBoolField(TEXT("load_system_returned_nonnull"),S!=nullptr);if(!Returned(TEXT("load_system"))){return nullptr;}if(!S){return nullptr;}if(!S->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)||!Pin(S)||!Pin(S->GetOutermost())){Fail(TEXT("niagara_emitter_loaded_identity_unexpected"),TEXT("Loaded system does not match the normalized exact object path."));return nullptr;}System=S;SystemPath=S->GetPathName();PackagePath=S->GetOutermost()->GetName();const auto& H=S->GetEmitterHandles();if(H.Num()>4096){Fail(TEXT("niagara_emitter_inventory_bound"),TEXT("Emitter count exceeds bounded operation."));return nullptr;}
        for(int32 I=0;I<H.Num();++I){FEmitterIdentity E;E.Index=I;E.Handle=H[I].GetId();E.Name=H[I].GetName().ToString();E.Enabled=H[I].GetIsEnabled();E.InstanceVersion=H[I].GetInstance().Version;E.Mode=static_cast<uint8>(H[I].GetEmitterMode());E.Data=H[I].GetEmitterData();if(E.Data){E.Version=E.Data->Version.VersionGuid;E.Emitter=H[I].GetInstance().Emitter;if(!Pin(E.Emitter.Get())){return nullptr;}if(E.Data->GetRenderers().Num()>16384){Fail(TEXT("niagara_emitter_inventory_bound"),TEXT("Renderer count exceeds bounded operation."));return nullptr;}for(auto* P:E.Data->GetRenderers()){E.Slots.Add(P);if(P&&!Pin(P)){return nullptr;}}}Emitters.Add(E);}if(!CaptureCompileRouting())return nullptr;Before=Projection();return R.Error.IsEmpty()?S:nullptr;
    }

    bool CaptureCompileRouting()
    {
     if(!DeferredMode)return true;
     if(!SDK(TEXT("capture_compile_routing")))return false;
     TArray<UNiagaraScript*> All{System->GetSystemSpawnScript(),System->GetSystemUpdateScript()};
     for(const auto& E:Emitters)if(E.Data){TArray<UNiagaraScript*> Owned;E.Data->GetScripts(Owned,false,false);All.Append(Owned);}
     if(All.Num()>65536)return Fail(TEXT("niagara_emitter_script_bound"),TEXT("Compile routing inventory bound exceeded."));
     for(UNiagaraScript* S:All)
     {
      if(!S||CompileRouting.ContainsByPredicate([&](const FCompileBinding& B){return B.Script.Get()==S;}))continue;
      if(!Pin(S))return false;FCompileBinding B;B.Script=S;B.Source=S->GetLatestSource();B.Version=S->GetExposedVersion().VersionGuid;
      if(B.Source.IsValid()){if(!Pin(B.Source.Get()))return false;if(auto* NS=Cast<UNiagaraScriptSource>(B.Source.Get())){B.Graph=NS->NodeGraph;if(B.Graph.IsValid()&&!Pin(B.Graph.Get()))return false;}}
      CompileRouting.Add(B);
     }
     return Returned(TEXT("capture_compile_routing"));
    }

    FNiagaraEmitterHandle* Handle(const FGuid& Id){if(!Admit(TEXT("reacquire_handle"))){return nullptr;}return System->GetEmitterHandles().FindByPredicate([&](const auto& H){return H.GetId()==Id;});}
    bool SelectWriter(const FGuid& Id)
    {
        if(!Admit(TEXT("selected_emitter_writer_ownership"))){return false;}
        const auto* E=Emitters.FindByPredicate([&](const auto& Entry){return Entry.Handle==Id;});
        UNiagaraEmitter* Selected=E?E->Emitter.Get():nullptr;UNiagaraSystem* S=System.Get();
        if(!Live(Selected)||!Live(S)||!Selected->IsIn(S)||Selected->GetPackage()!=S->GetPackage()||Selected->GetOutermost()!=S->GetOutermost())
        {R.Data->SetBoolField(TEXT("selected_emitter_writer_owned"),false);return Fail(TEXT("niagara_emitter_external_writer_unsupported"),TEXT("Selected emitter must belong to this System's outer chain and package before Modify. allow_source_edit does not authorize a separate emitter asset; no duplication or external modification was attempted."));}
        WriterEmitter=Selected;WriterEmitterCaptured=true;R.Data->SetBoolField(TEXT("selected_emitter_writer_owned"),true);R.Data->SetStringField(TEXT("selected_emitter_writer_path"),Selected->GetPathName());return true;
    }
    bool Enable(const FGuid& Id,bool Enabled)
    {
        if(!SDK(TEXT("set_handle_enabled"),true)){return false;}auto* H=Handle(Id);auto* E=Emitters.FindByPredicate([&](const auto& E){return E.Handle==Id;});if(!H||!E){return Fail(TEXT("niagara_emitter_handle_missing"),TEXT("Exact enabled target disappeared."));}E->Enabled=Enabled;const bool Changed=H->SetIsEnabled(Enabled,*System.Get(),false);R.bChanged|=Changed;Prefix->SetBoolField(TEXT("set_handle_enabled_returned_change"),Changed);return Returned(TEXT("set_handle_enabled"));
    }
    bool Remove(const FGuid& Id)
    {
        if(!SDK(TEXT("remove_emitter_handle"),true)){return false;}const int32 Index=Emitters.IndexOfByPredicate([&](const auto& E){return E.Handle==Id;});if(Index==INDEX_NONE){return Fail(TEXT("niagara_emitter_handle_missing"),TEXT("Exact removal target disappeared."));}UNiagaraEmitter* Removed=Emitters[Index].Emitter.Get();Emitters.RemoveAt(Index);for(int32 I=0;I<Emitters.Num();++I){Emitters[I].Index=I;}if(Removed&&!Emitters.ContainsByPredicate([&](const auto& E){return E.Emitter.Get()==Removed;})){for(auto& O:Objects){if(!O->Retired&&(O->Weak.Get()==Removed||O->Weak->IsIn(Removed))){O->Retired=true;}}}Prefix->SetStringField(TEXT("requested_removed_guid"),Id.ToString());System->RemoveEmitterHandlesById({Id});Prefix->SetBoolField(TEXT("remove_emitter_handle_SDK_returned"),true);R.bChanged=true;R.Data->SetBoolField(TEXT("remaining_physical_indices_shifted"),true);return Returned(TEXT("remove_emitter_handle"));
    }
    bool Expect(FVersionedNiagaraEmitterData* Data,std::initializer_list<const TCHAR*> Fields)
    {if(!Admit(TEXT("capture_requested_fields"))){return false;}const auto* E=Emitters.FindByPredicate([&](const auto& E){return E.Data==Data;});if(!E){return Fail(TEXT("niagara_emitter_data_unresolved"),TEXT("Expected data is not a captured version."));}auto Actual=DataState(Data);auto Expected=MakeShared<FJsonObject>();for(const TCHAR* Field:Fields){Expected->SetField(Field,Actual->TryGetField(Field));}Desired.Add(E->Handle,Expected);R.Data->SetObjectField(TEXT("expected_requested_fields"),Expected);return true;}
    TSharedPtr<FJsonObject> Property(FVersionedNiagaraEmitterData* Data)
    {if(!SDK(TEXT("emitter_property_projection"))){return MakeShared<FJsonObject>();}auto O=NiagaraEmitterPropertiesToJsonObject(Data);if(!Returned(TEXT("emitter_property_projection"))){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);}return O;}
    bool Request(bool& Accepted)
    {R.bCompileRequested=true;if(!SDK(TEXT("request_compile"),true)){return false;}R.bCompileAttempted=true;R.Compiled.Reset();Accepted=System->RequestCompile(false);CompilePrefix->SetBoolField(TEXT("SDK_compile_request_returned"),true);CompilePrefix->SetBoolField(TEXT("SDK_compile_request_accepted"),Accepted);CompilePrefix->SetBoolField(TEXT("compile_completion_verified"),false);return Returned(TEXT("request_compile"));}
    bool Save()
    {R.bSaveRequested=true;if(!SDK(TEXT("save_system"),true)){return false;}R.bSaveAttempted=true;SavePrefix->SetStringField(TEXT("requested_package"),PackagePath);const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(System.Get(),false);R.bSaved|=Saved;SavePrefix->SetBoolField(TEXT("SDK_save_returned"),true);SavePrefix->SetBoolField(TEXT("SDK_save_success"),Saved);SavePrefix->SetBoolField(TEXT("actual_saved_history"),R.bSaved);if(!Returned(TEXT("save_system"))){SavePrefix->SetBoolField(TEXT("post_save_projection_skipped"),true);return false;}SavePrefix->SetBoolField(TEXT("package_dirty_after"),System->GetOutermost()->IsDirty());if(Saved&&System->GetOutermost()->IsDirty()){Fail(TEXT("niagara_emitter_saved_dirty"),TEXT("Save returned true but package is dirty; saved history was retained."));return false;}return Saved;}
    TSharedPtr<FJsonObject> Error(const FString& Message){if(R.Error.IsEmpty()){Fail(TEXT("niagara_emitter_request_failed"),Message);}auto E=MakeShared<FJsonObject>();E->SetBoolField(TEXT("success"),false);E->SetStringField(TEXT("error"),Message);return E;}
};
#define GF_STEP(Stage,Mutation,Expression) do{if(!W.SDK(TEXT(Stage),Mutation)){return W.Error(W.R.Error);}Expression;if(!W.Returned(TEXT(Stage))){return W.Error(W.R.Error);}}while(false)

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

FString NormalizeNiagaraEnumToken(FString Value)
{
    Value.TrimStartAndEndInline();
    Value.ToLowerInline();
    Value.ReplaceInline(TEXT("_"), TEXT(""));
    Value.ReplaceInline(TEXT("-"), TEXT(""));
    Value.ReplaceInline(TEXT(" "), TEXT(""));
    return Value;
}

bool ParseNiagaraSimTargetToken(const FString& Value, ENiagaraSimTarget& OutValue)
{
    const FString Token = NormalizeNiagaraEnumToken(Value);
    if (Token == TEXT("cpu") || Token == TEXT("cpusim"))
    {
        OutValue = ENiagaraSimTarget::CPUSim;
        return true;
    }
    if (Token == TEXT("gpu") || Token == TEXT("gpusim") || Token == TEXT("gpucompute") || Token == TEXT("gpucomputesim"))
    {
        OutValue = ENiagaraSimTarget::GPUComputeSim;
        return true;
    }
    return false;
}

bool ParseNiagaraBoundModeToken(const FString& Value, ENiagaraEmitterCalculateBoundMode& OutValue)
{
    const FString Token = NormalizeNiagaraEnumToken(Value);
    if (Token == TEXT("dynamic"))
    {
        OutValue = ENiagaraEmitterCalculateBoundMode::Dynamic;
        return true;
    }
    if (Token == TEXT("fixed"))
    {
        OutValue = ENiagaraEmitterCalculateBoundMode::Fixed;
        return true;
    }
    if (Token == TEXT("programmable"))
    {
        OutValue = ENiagaraEmitterCalculateBoundMode::Programmable;
        return true;
    }
    return false;
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
 if(!W.Returned(TEXT("independent_compile_status")))return nullptr;
 if(!Parsed||!Legacy||!Legacy->IsValid()){W.Fail(TEXT("niagara_emitter_compile_status_invalid"),TEXT("Dedicated status did not return a bounded typed legacy result; actual prefix retained."));return nullptr;}
 bool Health=false;Typed->TryGetBoolField(TEXT("success"),Health);
 if(!Health){auto Failure=MakeShared<FJsonObject>(**Legacy);Failure->SetBoolField(TEXT("success"),false);return Failure;}
 return *Legacy;
}

struct FEmitterCompileWait
{
 TSharedPtr<FJsonObject> Result=MakeShared<FJsonObject>();
 double Started=0,Next=0,Timeout=0,Poll=.1;int32 Iterations=0;bool Done=false;
 FEmitterCompileWait(double Seconds,double Interval):Started(FPlatformTime::Seconds()),Timeout(FMath::Clamp(Seconds,0.0,60.0)),Poll(FMath::Clamp(Interval,.02,1.0))
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
   if(!W.Returned(TEXT("async_wait_HasOutstandingCompilationRequests")))return false;
   if(!W.SDK(TEXT("async_wait_HasActiveCompilations")))return false;
   Active=System->HasActiveCompilations();Result->SetBoolField(TEXT("active_compilations_after"),Active);
   return W.Returned(TEXT("async_wait_HasActiveCompilations"));
  };
  if(!Observe()){Done=true;return true;}
  if(Outstanding||Active)
  {
   if(!W.SDK(TEXT("async_wait_PollForCompilationComplete"))){Done=true;return true;}
   System->PollForCompilationComplete();
   if(!W.Returned(TEXT("async_wait_PollForCompilationComplete"))||!Observe()){Done=true;return true;}
  }
  const double Elapsed=FPlatformTime::Seconds()-Started;
  const bool TimedOut=(Outstanding||Active)&&Elapsed>=Timeout;
  Result->SetNumberField(TEXT("wait_elapsed_seconds"),Elapsed);Result->SetBoolField(TEXT("wait_timed_out"),TimedOut);
  if((!Outstanding&&!Active)||TimedOut){Done=true;Result->SetNumberField(TEXT("wait_iterations"),Iterations);Result->SetStringField(TEXT("wait_status"),TimedOut?TEXT("bounded_timeout_observed"):TEXT("idle_observed"));return true;}
  ++Iterations;Result->SetNumberField(TEXT("wait_iterations"),Iterations);
  Next=FPlatformTime::Seconds()+Poll;return false;
 }
};
void BeginEmitterWait(FWork& W,UNiagaraSystem* System,double Timeout,double Poll,TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&)> Completion)
{
 auto Wait=MakeShared<FEmitterCompileWait>(Timeout,Poll);
 W.R.Data->SetObjectField(TEXT("compile_wait_partial"),Wait->Result);
 W.DeferredStep=[&W,System,Wait,Completion=MoveTemp(Completion)]()mutable
 {
  if(!Wait->Tick(W,System))return false;
  if(!W.Admit(TEXT("emitter_wait_completion_admission")))return true;
  TSharedPtr<FJsonObject> Legacy=Completion(Wait->Result);
  if(Legacy.IsValid())W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
  return true;
 };
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


TSharedPtr<FJsonObject> WaitForNiagaraCompileJson(FWork& W,UNiagaraSystem* S,bool Wait,double Timeout,double Poll)
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("wait_for_completion"),Wait);O->SetNumberField(TEXT("timeout_seconds"),Timeout);O->SetNumberField(TEXT("poll_interval_seconds"),Poll);O->SetBoolField(TEXT("wait_timed_out"),false);O->SetNumberField(TEXT("wait_iterations"),0);O->SetNumberField(TEXT("wait_elapsed_seconds"),0);O->SetBoolField(TEXT("wait_performed"),false);
    if(!W.SDK(TEXT("compile_state_snapshot"))){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);return O;}const bool Outstanding=S&&S->HasOutstandingCompilationRequests(true);const bool Active=S&&S->HasActiveCompilations();if(!W.Returned(TEXT("compile_state_snapshot"))){O->SetBoolField(TEXT("projection_skipped_unsafe"),true);return O;}O->SetBoolField(TEXT("outstanding_compilation_requests_after"),Outstanding);O->SetBoolField(TEXT("active_compilations_after"),Active);return O;
}
TSharedPtr<FJsonObject> MakeNiagaraEventMutationRecoveryError(FWork& W,const FString& Message,const FString& ErrorCode,const UNiagaraSystem*,const TSharedPtr<FJsonObject>& CompileWait=nullptr,bool SaveStateUnknown=false)
{
    W.Fail(*ErrorCode,Message);auto Error=W.Error(Message);Error->SetStringField(TEXT("error_code"),ErrorCode);Error->SetBoolField(TEXT("saved"),false);Error->SetBoolField(TEXT("mutation_performed"),true);Error->SetBoolField(TEXT("mutation_applied_in_memory"),true);Error->SetBoolField(TEXT("save_state_unknown"),SaveStateUnknown);Error->SetBoolField(TEXT("requires_package_reload"),true);Error->SetBoolField(TEXT("requires_reload_or_undo"),true);Error->SetBoolField(TEXT("undo_recovery_unproven"),true);Error->SetBoolField(TEXT("do_not_retry"),true);Error->SetStringField(TEXT("recovery_policy"),TEXT("reload_exact_package_before_retry"));Error->SetStringField(TEXT("system_path"),W.SystemPath);if(CompileWait){Error->SetObjectField(TEXT("compile_wait"),CompileWait);}return Error;
}

TSharedPtr<FJsonObject> HandleInspectNiagaraEmitterProperties(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    TargetEmitterName.TrimStartAndEndInline();

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<TSharedPtr<FJsonValue>> EmitterValues;
    int32 CurrentEmitterIndex = 0;
    const TArray<FNiagaraEmitterHandle> HandlesSnapshot = System->GetEmitterHandles();
    for (const FNiagaraEmitterHandle& EmitterHandle : HandlesSnapshot)
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));
        if (bMatches)
        {
            TSharedPtr<FJsonObject> EmitterObject = W.Property(EmitterHandle.GetEmitterData());
    if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
            EmitterObject->SetStringField(TEXT("name"), EmitterName);
            EmitterObject->SetNumberField(TEXT("emitter_index"), CurrentEmitterIndex);
            EmitterObject->SetBoolField(TEXT("enabled"), EmitterHandle.GetIsEnabled());
            EmitterValues.Add(MakeShared<FJsonValueObject>(EmitterObject));
        }
        ++CurrentEmitterIndex;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetNumberField(TEXT("emitter_count"), CurrentEmitterIndex);
    Result->SetNumberField(TEXT("matched_count"), EmitterValues.Num());
    Result->SetArrayField(TEXT("emitters"), EmitterValues);

    // System-level execution-gate diagnostics (silent emitter-disable triage):
    // an EmitterCompiledData array shorter than the handle list disables every emitter at
    // FNiagaraEmitterInstanceImpl::Init, and it stays empty when either system script's VM
    // executable data is invalid on load.
    Result->SetNumberField(TEXT("emitter_compiled_data_count"), System->GetEmitterCompiledData().Num());
    Result->SetNumberField(TEXT("active_niagara_quality_level"), FNiagaraPlatformSet::GetQualityLevel());
    const UNiagaraScript* SystemSpawnScript = System->GetSystemSpawnScript();
    const UNiagaraScript* SystemUpdateScript = System->GetSystemUpdateScript();
    Result->SetBoolField(TEXT("system_spawn_vm_valid"),
        SystemSpawnScript != nullptr && SystemSpawnScript->GetVMExecutableData().IsValid());
    Result->SetBoolField(TEXT("system_update_vm_valid"),
        SystemUpdateScript != nullptr && SystemUpdateScript->GetVMExecutableData().IsValid());
    Result->SetBoolField(TEXT("read_only"), true);
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraEmitterEnabled(FWork& W,const TSharedPtr<FJsonObject>& Params)
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
            FString::Printf(TEXT("Refusing to edit Niagara emitter outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    FNiagaraEmitterHandle* MatchedHandle = nullptr;
    int32 MatchCount = 0;
    int32 CurrentEmitterIndex = 0;
    for (FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
    {
        const FString EmitterName = EmitterHandle.GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));
        if (bEmitterMatches)
        {
            ++MatchCount;
            if (MatchCount == 1)
            {
                MatchedHandle = &EmitterHandle;
            }
        }
        ++CurrentEmitterIndex;
    }

    if (!MatchedHandle)
    {
        return W.Error(TEXT("No matching Niagara emitter handle was found"));
    }
    if (MatchCount > 1)
    {
        return W.Error(TEXT("Emitter selector matched more than one emitter handle"));
    }

    const bool bWasEnabled = MatchedHandle->GetIsEnabled();
    const FGuid SelectedHandleId = MatchedHandle->GetId();
    const FString SelectedHandleName = MatchedHandle->GetName().ToString();
    if (!W.SelectWriter(SelectedHandleId)) return W.Error(W.R.Error);
    GF_STEP("System_Modify",true,System->Modify());
    if (!W.Enable(SelectedHandleId,bEnabled)) return W.Error(W.R.Error);

    if (UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
    {
        if (!W.Pin(SystemEditorData)) return W.Error(W.R.Error);
        GF_STEP("synchronize_overview",true,SystemEditorData->SynchronizeOverviewGraphWithSystem(*System));
    }

    GF_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save();
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after emitter enabled update"));
        }
    }

    MatchedHandle = W.Handle(SelectedHandleId);
    if (!MatchedHandle) return W.Error(W.R.Error);
    W.R.Data->SetStringField(TEXT("actual_selected_guid"),SelectedHandleId.ToString());
    const auto* ActualIdentity = W.Emitters.FindByPredicate([&](const auto& E){return E.Handle==SelectedHandleId;});
    if (ActualIdentity) W.R.Data->SetNumberField(TEXT("actual_emitter_index"),ActualIdentity->Index);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), SelectedHandleName);
    Result->SetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Result->SetBoolField(TEXT("previous_enabled"), bWasEnabled);
    Result->SetBoolField(TEXT("enabled"), MatchedHandle->GetIsEnabled());
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("emitter_enabled"));
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraEmitterLocalSpace(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bValue = false;
    if (!Params->TryGetBoolField(TEXT("value"), bValue))
    {
        return W.Error(TEXT("Missing 'value' (bool) parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara emitter outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    TargetEmitterName.TrimStartAndEndInline();

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    FResolvedNiagaraEmitterTarget Resolved;
    int32 MatchCount = 0;
    if (!ResolveSingleNiagaraEmitterTarget(System, TargetEmitterIndex, TargetEmitterName, Resolved, MatchCount))
    {
        if (MatchCount == 0)
        {
            return W.Error(TEXT("No matching Niagara emitter was found"));
        }
        if (MatchCount > 1)
        {
            return W.Error(TEXT("Emitter selector matched more than one emitter; provide emitter_index or emitter_name"));
        }
        return W.Error(TEXT("Matched emitter has no editable emitter data"));
    }

    TSharedPtr<FJsonObject> Before = W.Property(Resolved.Data);
    if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);

    if (!W.SelectWriter(W.Emitters[Resolved.Index].Handle)) return W.Error(W.R.Error);
    GF_STEP("System_Modify",true,System->Modify());
    if (Resolved.Emitter)
    {
        GF_STEP("Resolved_Emitter_Modify",true,Resolved.Emitter->Modify());
    }
    Resolved.Data->bLocalSpace = bValue;
    if (!W.Expect(Resolved.Data,{TEXT("local_space")})) return W.Error(W.R.Error);
    if (Resolved.Emitter)
    {
        GF_STEP("Resolved_Emitter_PostEditChange",true,Resolved.Emitter->PostEditChange());
    }
    GF_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save();
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after local space update"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), Resolved.Name);
    Result->SetNumberField(TEXT("emitter_index"), Resolved.Index);
    Result->SetObjectField(TEXT("before"), Before);
    Result->SetObjectField(TEXT("after"), W.Property(Resolved.Data));
    if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("emitter_local_space"));
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraEmitterSimTarget(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    FString SimTargetText;
    if (!Params->TryGetStringField(TEXT("sim_target"), SimTargetText))
    {
        return W.Error(TEXT("Missing 'sim_target' parameter (\"CPU\" or \"GPU\")"));
    }
    ENiagaraSimTarget NewSimTarget = ENiagaraSimTarget::CPUSim;
    if (!ParseNiagaraSimTargetToken(SimTargetText, NewSimTarget))
    {
        return W.Error(FString::Printf(TEXT("Unsupported sim_target: %s (use \"CPU\" or \"GPU\")"), *SimTargetText));
    }

    bool bAutoFixedBounds = true;
    Params->TryGetBoolField(TEXT("auto_fixed_bounds"), bAutoFixedBounds);

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara emitter outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    TargetEmitterName.TrimStartAndEndInline();

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    FResolvedNiagaraEmitterTarget Resolved;
    int32 MatchCount = 0;
    if (!ResolveSingleNiagaraEmitterTarget(System, TargetEmitterIndex, TargetEmitterName, Resolved, MatchCount))
    {
        if (MatchCount == 0)
        {
            return W.Error(TEXT("No matching Niagara emitter was found"));
        }
        if (MatchCount > 1)
        {
            return W.Error(TEXT("Emitter selector matched more than one emitter; provide emitter_index or emitter_name"));
        }
        return W.Error(TEXT("Matched emitter has no editable emitter data"));
    }

    // GPU sim cannot use Dynamic bounds; decide before mutating.
    const bool bNeedsBoundsFix =
        (NewSimTarget == ENiagaraSimTarget::GPUComputeSim) &&
        (Resolved.Data->CalculateBoundsMode == ENiagaraEmitterCalculateBoundMode::Dynamic);
    if (bNeedsBoundsFix && !bAutoFixedBounds)
    {
        return W.Error(
            TEXT("GPU sim cannot use Dynamic bounds. Set fixed bounds first via set_niagara_emitter_fixed_bounds, or pass auto_fixed_bounds=true."));
    }

    TSharedPtr<FJsonObject> Before = W.Property(Resolved.Data);
    if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);

    if (!W.SelectWriter(W.Emitters[Resolved.Index].Handle)) return W.Error(W.R.Error);
    GF_STEP("System_Modify",true,System->Modify());
    if (Resolved.Emitter)
    {
        GF_STEP("Resolved_Emitter_Modify",true,Resolved.Emitter->Modify());
    }
    Resolved.Data->SimTarget = NewSimTarget;
    bool bAutoSetFixedBounds = false;
    if (bNeedsBoundsFix)
    {
        Resolved.Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
        if (Resolved.Data->FixedBounds.IsValid == 0)
        {
            Resolved.Data->FixedBounds = FVersionedNiagaraEmitterData::GetDefaultFixedBounds();
        }
        bAutoSetFixedBounds = true;
    }
    if (bNeedsBoundsFix)
    { if (!W.Expect(Resolved.Data,{TEXT("sim_target"),TEXT("bounds_mode"),TEXT("bounds_valid"),TEXT("bounds")})) return W.Error(W.R.Error); }
    else if (!W.Expect(Resolved.Data,{TEXT("sim_target")})) return W.Error(W.R.Error);
    if (Resolved.Emitter)
    {
        GF_STEP("Resolved_Emitter_PostEditChange",true,Resolved.Emitter->PostEditChange());
    }
    GF_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save();
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after sim target update"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), Resolved.Name);
    Result->SetNumberField(TEXT("emitter_index"), Resolved.Index);
    Result->SetBoolField(TEXT("auto_set_fixed_bounds"), bAutoSetFixedBounds);
    Result->SetObjectField(TEXT("before"), Before);
    Result->SetObjectField(TEXT("after"), W.Property(Resolved.Data));
    if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("emitter_sim_target"));
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraEmitterFixedBounds(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    const TArray<TSharedPtr<FJsonValue>>* MinArray = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* MaxArray = nullptr;
    const bool bHasMin = Params->TryGetArrayField(TEXT("min"), MinArray);
    const bool bHasMax = Params->TryGetArrayField(TEXT("max"), MaxArray);

    FString ModeText;
    const bool bHasMode = Params->TryGetStringField(TEXT("mode"), ModeText);
    ENiagaraEmitterCalculateBoundMode NewMode = ENiagaraEmitterCalculateBoundMode::Fixed;
    if (bHasMode && !ParseNiagaraBoundModeToken(ModeText, NewMode))
    {
        return W.Error(FString::Printf(TEXT("Unsupported mode: %s (use Dynamic, Fixed, or Programmable)"), *ModeText));
    }

    FVector MinVector = FVector::ZeroVector;
    FVector MaxVector = FVector::ZeroVector;
    bool bHasBounds = false;
    if (bHasMin || bHasMax)
    {
        if (!bHasMin || !bHasMax)
        {
            return W.Error(TEXT("Both 'min' and 'max' are required to set fixed bounds"));
        }
        if (!TryParseVector3FromJsonArray(MinArray, MinVector) || !TryParseVector3FromJsonArray(MaxArray, MaxVector))
        {
            return W.Error(TEXT("'min' and 'max' must be [x, y, z] finite numbers"));
        }
        bHasBounds = true;
    }

    if (!bHasBounds && !bHasMode)
    {
        return W.Error(TEXT("Provide 'min'+'max' and/or 'mode'"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara emitter outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    TargetEmitterName.TrimStartAndEndInline();

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    FResolvedNiagaraEmitterTarget Resolved;
    int32 MatchCount = 0;
    if (!ResolveSingleNiagaraEmitterTarget(System, TargetEmitterIndex, TargetEmitterName, Resolved, MatchCount))
    {
        if (MatchCount == 0)
        {
            return W.Error(TEXT("No matching Niagara emitter was found"));
        }
        if (MatchCount > 1)
        {
            return W.Error(TEXT("Emitter selector matched more than one emitter; provide emitter_index or emitter_name"));
        }
        return W.Error(TEXT("Matched emitter has no editable emitter data"));
    }

    TSharedPtr<FJsonObject> Before = W.Property(Resolved.Data);
    if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);

    if (!W.SelectWriter(W.Emitters[Resolved.Index].Handle)) return W.Error(W.R.Error);
    GF_STEP("System_Modify",true,System->Modify());
    if (Resolved.Emitter)
    {
        GF_STEP("Resolved_Emitter_Modify",true,Resolved.Emitter->Modify());
    }
    if (bHasBounds)
    {
        Resolved.Data->FixedBounds = FBox(MinVector, MaxVector);
        if (!bHasMode)
        {
            Resolved.Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
        }
    }
    if (bHasMode)
    {
        Resolved.Data->CalculateBoundsMode = NewMode;
    }
    if (bHasBounds)
    { if (!W.Expect(Resolved.Data,{TEXT("bounds_mode"),TEXT("bounds_valid"),TEXT("bounds")})) return W.Error(W.R.Error); }
    else if (!W.Expect(Resolved.Data,{TEXT("bounds_mode")})) return W.Error(W.R.Error);
    if (Resolved.Emitter)
    {
        GF_STEP("Resolved_Emitter_PostEditChange",true,Resolved.Emitter->PostEditChange());
    }
    GF_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save();
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after fixed bounds update"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), Resolved.Name);
    Result->SetNumberField(TEXT("emitter_index"), Resolved.Index);
    Result->SetObjectField(TEXT("before"), Before);
    Result->SetObjectField(TEXT("after"), W.Property(Resolved.Data));
    if (!W.R.Error.IsEmpty()) return W.Error(W.R.Error);
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("emitter_fixed_bounds"));
    return Result;
}

TSharedPtr<FJsonObject> HandleSetNiagaraEmitterPersistentIds(FWork& W,const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params.IsValid() || !Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return W.Error(TEXT("Missing 'system_path' parameter"));
    }

    bool bEnabled = true;
    if (!Params->TryGetBoolField(TEXT("enabled"), bEnabled) &&
        !Params->TryGetBoolField(TEXT("value"), bEnabled))
    {
        return W.Error(TEXT("Missing 'enabled' (bool) parameter"));
    }

    bool bAllowSourceEdit = false;
    Params->TryGetBoolField(TEXT("allow_source_edit"), bAllowSourceEdit);
    if (!bAllowSourceEdit && !IsTempGeneratedNiagaraPath(SystemPath))
    {
        return W.Error(
            FString::Printf(TEXT("Refusing to edit Niagara emitter outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    TargetEmitterName.TrimStartAndEndInline();

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);
    bool bWaitForCompletion = false;
    double TimeoutSeconds = 10.0;
    double PollIntervalSeconds = 0.1;
    ReadNiagaraCompileWaitParams(Params, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds);
    if (bSave && (!bRequestCompile || !bWaitForCompletion))
    {
        return W.Error(
            TEXT("save=true requires request_compile=true and wait_for_completion=true for Persistent IDs updates"));
    }

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
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
    int32 MatchCount = 0;
    if (!ResolveSingleNiagaraEmitterTarget(System, TargetEmitterIndex, TargetEmitterName, Target, MatchCount))
    {
        if (MatchCount == 0)
        {
            return W.Error(TEXT("No matching Niagara emitter was found"));
        }
        return W.Error(TEXT("Emitter selector matched more than one emitter; provide emitter_index or emitter_name"));
    }
    if (!Target.Emitter || !Target.Data)
    {
        return W.Error(TEXT("Matched Niagara emitter is invalid"));
    }

    const bool bBefore = Target.Data->bRequiresPersistentIDs != 0;
    if (bBefore == bEnabled)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetBoolField(TEXT("skipped_unchanged"), true);
        Result->SetStringField(TEXT("system_path"), System->GetPathName());
        Result->SetStringField(TEXT("emitter_name"), Target.Name);
        Result->SetNumberField(TEXT("emitter_index"), Target.Index);
        Result->SetBoolField(TEXT("before"), bBefore);
        Result->SetBoolField(TEXT("after"), bEnabled);
        Result->SetBoolField(TEXT("compile_requested"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetStringField(TEXT("write_scope"), TEXT("emitter_persistent_ids_unchanged"));
        return Result;
    }

    if (!W.SelectWriter(W.Emitters[Target.Index].Handle)) return W.Error(W.R.Error);
    if (!W.DeferredMode && bRequestCompile && bWaitForCompletion)
    { W.Fail(TEXT("niagara_emitter_async_wait_unsupported"),TEXT("Changing Persistent IDs with compile+wait requires an unimplemented continuation and independent compile-status save gate; no emitter mutation/compile/save was attempted.")); return W.Error(W.R.Error); }
    GF_STEP("System_Modify",true,System->Modify());
    GF_STEP("Target_Emitter_Modify",true,Target.Emitter->Modify());
    Target.Data->bRequiresPersistentIDs = bEnabled;
    if (!W.Expect(Target.Data,{TEXT("persistent_ids")})) return W.Error(W.R.Error);
    GF_STEP("Target_Emitter_PostEditChange",true,Target.Emitter->PostEditChange());
    GF_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    bool bCompileRequested = false;
    if (bRequestCompile && !W.Request(bCompileRequested)) return W.Error(W.R.Error);
    auto CompletePersistent=[=,&W](const TSharedPtr<FJsonObject>& CompileWait)->TSharedPtr<FJsonObject>
    {
     if(!W.Admit(TEXT("persistent_post_wait_admission")))return W.Error(W.R.Error);
     if(bRequestCompile&&bWaitForCompletion)
     {
      auto Status=IndependentCompileStatus(W,System);if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);if(Status)CompileWait->SetObjectField(TEXT("compile_status"),Status);
      FString Reason;if(!NiagaraCompileWaitAllowsSave(CompileWait,Reason))return MakeNiagaraEventMutationRecoveryError(W,TEXT("Persistent IDs compile gate failed; mutation remains unsaved in memory: ")+Reason,TEXT("niagara_persistent_ids_compile_gate_failed"),System,CompileWait);
     }
    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save();
        if (!bSaved)
        {
            return MakeNiagaraEventMutationRecoveryError(W,
                TEXT("Failed to save Niagara system after Persistent IDs update; persisted state is unknown"),
                TEXT("niagara_persistent_ids_save_state_unknown"),
                System,
                CompileWait,
                true);
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("skipped_unchanged"), false);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("emitter_name"), Target.Name);
    Result->SetNumberField(TEXT("emitter_index"), Target.Index);
    Result->SetBoolField(TEXT("before"), bBefore);
    Result->SetBoolField(TEXT("after"), Target.Data->bRequiresPersistentIDs != 0);
    Result->SetBoolField(TEXT("compile_requested"), bCompileRequested);
    Result->SetObjectField(TEXT("compile_wait"), CompileWait);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("emitter_persistent_ids"));
    return Result;
    };
    if(W.DeferredMode&&bRequestCompile&&bWaitForCompletion){BeginEmitterWait(W,System,TimeoutSeconds,PollIntervalSeconds,MoveTemp(CompletePersistent));auto Pending=MakeShared<FJsonObject>();Pending->SetBoolField(TEXT("success"),false);Pending->SetBoolField(TEXT("operation_complete"),false);return Pending;}
    auto CompileWait=WaitForNiagaraCompileJson(W,System,bRequestCompile&&bWaitForCompletion,TimeoutSeconds,PollIntervalSeconds);if(!W.R.Error.IsEmpty())return W.Error(W.R.Error);return CompletePersistent(CompileWait);
}

TSharedPtr<FJsonObject> HandleRemoveNiagaraEmitter(FWork& W,const TSharedPtr<FJsonObject>& Params)
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
            FString::Printf(TEXT("Refusing to remove Niagara emitter outside %s: %s"), NiagaraTempGenerationRoot, *SystemPath));
    }

    int32 TargetEmitterIndex = INDEX_NONE;
    FString TargetEmitterName;
    Params->TryGetNumberField(TEXT("emitter_index"), TargetEmitterIndex);
    Params->TryGetStringField(TEXT("emitter_name"), TargetEmitterName);
    if (TargetEmitterIndex == INDEX_NONE && TargetEmitterName.IsEmpty())
    {
        return W.Error(TEXT("remove_niagara_emitter requires emitter_index or emitter_name"));
    }

    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);
    W.R.bSaveRequested=bSave;
    bool bRequestCompile = true;
    Params->TryGetBoolField(TEXT("request_compile"), bRequestCompile);

    UNiagaraSystem* System = W.LoadSystem(SystemPath);
    if (!System)
    {
        return W.Error(FString::Printf(TEXT("Failed to load Niagara system: %s"), *SystemPath));
    }

    TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
    int32 MatchedIndex = INDEX_NONE;
    FString MatchedName;
    int32 MatchCount = 0;
    for (int32 CurrentEmitterIndex = 0; CurrentEmitterIndex < EmitterHandles.Num(); ++CurrentEmitterIndex)
    {
        const FString EmitterName = EmitterHandles[CurrentEmitterIndex].GetName().ToString();
        const bool bEmitterMatches =
            (TargetEmitterIndex == INDEX_NONE || TargetEmitterIndex == CurrentEmitterIndex) &&
            (TargetEmitterName.IsEmpty() || TargetEmitterName.Equals(EmitterName, ESearchCase::IgnoreCase));
        if (bEmitterMatches)
        {
            ++MatchCount;
            if (MatchCount == 1)
            {
                MatchedIndex = CurrentEmitterIndex;
                MatchedName = EmitterName;
            }
        }
    }

    if (MatchedIndex == INDEX_NONE)
    {
        return W.Error(TEXT("No matching Niagara emitter handle was found"));
    }
    if (MatchCount > 1)
    {
        return W.Error(TEXT("Emitter selector matched more than one emitter handle"));
    }

    const int32 EmitterCountBefore = EmitterHandles.Num();
    const FGuid MatchedEmitterHandleId = EmitterHandles[MatchedIndex].GetId();
    if (!W.SelectWriter(MatchedEmitterHandleId)) return W.Error(W.R.Error);
    GF_STEP("System_Modify",true,System->Modify());
    if (!W.Remove(MatchedEmitterHandleId)) return W.Error(W.R.Error);

    if (UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
    {
        if (!W.Pin(SystemEditorData)) return W.Error(W.R.Error);
        GF_STEP("synchronize_overview",true,SystemEditorData->SynchronizeOverviewGraphWithSystem(*System));
    }

    GF_STEP("System_MarkPackageDirty",true,System->MarkPackageDirty());
    if (bRequestCompile)
    {
        bool CompileAccepted = false; if (!W.Request(CompileAccepted)) return W.Error(W.R.Error);
    }

    bool bSaved = false;
    if (bSave)
    {
        bSaved = W.Save();
        if (!bSaved)
        {
            return W.Error(TEXT("Failed to save Niagara system after emitter removal"));
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("system_path"), System->GetPathName());
    Result->SetStringField(TEXT("removed_emitter_name"), MatchedName);
    Result->SetNumberField(TEXT("removed_emitter_index"), MatchedIndex);
    Result->SetNumberField(TEXT("emitter_count_before"), EmitterCountBefore);
    Result->SetNumberField(TEXT("emitter_count_after"), System->GetEmitterHandles().Num());
    Result->SetBoolField(TEXT("compile_requested"), bRequestCompile);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("write_scope"), TEXT("emitter_remove"));
    return Result;
}

FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation)
{
    bool Success=false;if(Legacy){Legacy->TryGetBoolField(TEXT("success"),Success);}
    bool ScopedObservationComplete=false;
    if(W.Before)
    {
        auto After=W.Projection();W.R.Data->SetObjectField(TEXT("before"),W.Before);W.R.Data->SetObjectField(TEXT("after"),After);
        bool Skipped=false;After->TryGetBoolField(TEXT("projection_skipped_unsafe"),Skipped);
        if(!Skipped)
        {
            W.R.bChanged|=!Json(W.Before).Equals(Json(After),ESearchCase::CaseSensitive);
            ScopedObservationComplete=After->HasField(TEXT("system_path"))&&After->HasField(TEXT("package"))&&After->HasField(TEXT("dirty"))&&After->HasField(TEXT("emitters"))&&W.R.Error.IsEmpty()&&W.Admit(TEXT("final_scoped_observation"));
        }
    }
    if(!Success&&W.R.Error.IsEmpty()){FString E;if(!Legacy||!Legacy->TryGetStringField(TEXT("error"),E)){E=TEXT("Original emitter operation returned failure; see legacy_result.");}W.Fail(TEXT("niagara_emitter_request_failed"),E);}
    W.R.bSuccess=Success&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;
    // Desired typed fields and expected-next handle topology were rechecked by the final Admit.
    if(!W.Desired.IsEmpty()){W.R.Data->SetBoolField(TEXT("typed_readback_pass"),ScopedObservationComplete&&W.R.Error.IsEmpty());}
    bool TypedReadbackPassed=true;W.R.Data->TryGetBoolField(TEXT("typed_readback_pass"),TypedReadbackPassed);
    const bool ScopedKnown=W.R.bSuccess&&W.R.bPostconditionPassed&&TypedReadbackPassed&&ScopedObservationComplete;
    if(ScopedKnown){W.R.bChangeStateKnown=true;}
    W.R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),ScopedObservationComplete);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),ScopedKnown);
    W.R.Data->SetStringField(TEXT("change_state_known_scope"),TEXT("Known only for captured target handle GUID/order/name/enabled/mode/version/ref, typed emitter settings, package dirtiness and actual removal prefix after successful postconditions/final admission. SDK/global cache side effects and async compile/render completion remain separate and unverified."));
    W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("postcondition_passed"),W.R.bPostconditionPassed);
    W.R.Data->SetStringField(TEXT("selection_scope"),TEXT("Original physical handle indices and case-insensitive AND selector are preserved, including disabled handles. No-op/trim rules remain API-specific. Versioned-emitter property authoring does not pretend Stateless data is editable."));
    W.R.Data->SetStringField(TEXT("changed_observation_scope"),TEXT("Captured handle order/GUID/version/ref, typed emitter properties and package dirtiness plus actual removal prefix; SDK/global effects remain unknown."));
    W.R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("Current-request SaveLoadedAsset true return history is preserved before post-Guard failure. Legacy save-state-unknown and actual standard saved may differ; no automatic reload/retry or atomic rollback."));
    W.R.Data->SetStringField(TEXT("compile_scope"),W.DeferredMode?TEXT("Persistent IDs preserves compile -> finite actual wait -> independent strict status/error/dirty/missing gate -> optional save. Timeout is original mutation recovery failure; compiled remains null after request. No global pump/Sleep/flush/replay; SDK internal costs may block."):TEXT("Direct/raw actual compile+wait remains unsupported before writer. RequestCompile acceptance is not compile/shader/GPU/render completion."));
    W.R.Data->SetBoolField(TEXT("compile_wait_implemented"),W.DeferredMode);W.R.Data->SetBoolField(TEXT("Cubeless_runtime_asset_dependency_allowed"),false);return Operations::EncodeVariableResult(W.R,Operation);
}
using FHandler=TSharedPtr<FJsonObject>(*)(FWork&,const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& Text,const TCHAR* Operation,bool Writer,FHandler Handler)
{
    FWork W(Writer);TSharedPtr<FJsonObject> Params;if(!ParseParams(Text,Params)){W.Fail(TEXT("niagara_emitter_invalid_params_json"),TEXT("Expected one finite JSON object within 1 Mi UTF16 units/depth64/262144 visits and notation tokens, without duplicate case-sensitive keys or embedded NUL."));return Finish(W,W.Error(W.R.Error),Operation);}if(Running||DeferredLeaseActive){W.Fail(TEXT("niagara_emitter_reentrant_request"),TEXT("Another dedicated emitter request is active; no replay."));return Finish(W,W.Error(W.R.Error),Operation);}TGuardValue<bool> Lease(Running,true);return Finish(W,Handler(W,Params),Operation);
}
#undef GF_STEP
}

FString Inspect(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("inspect_niagara_emitter_properties"),false,&HandleInspectNiagaraEmitterProperties);}
FString SetEnabled(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_emitter_enabled"),true,&HandleSetNiagaraEmitterEnabled);}
FString SetLocalSpace(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_emitter_local_space"),true,&HandleSetNiagaraEmitterLocalSpace);}
FString SetSimTarget(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_emitter_sim_target"),true,&HandleSetNiagaraEmitterSimTarget);}
FString SetFixedBounds(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_emitter_fixed_bounds"),true,&HandleSetNiagaraEmitterFixedBounds);}
FString SetPersistentIds(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("set_niagara_emitter_persistent_ids"),true,&HandleSetNiagaraEmitterPersistentIds);}
FString Remove(const FString& ParamsJson){return RunDedicated(ParamsJson,TEXT("remove_niagara_emitter"),true,&HandleRemoveNiagaraEmitter);}
}

namespace Cubeless::UE58::NiagaraEmitterConfiguration
{
namespace PersistentIdsAsync
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
 FWork Work{true};
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
  if(Work.Writer&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld))return false;
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
  D->SetStringField(TEXT("continuation_scope"),TEXT("Exact existing PersistentIds core on finite GT ticks. Compile then genuine bounded wait then independent original strict status gate then optional save. Timeout/pending/status gate failure remains original mutation recovery failure, including save=false. No global pump/Sleep/replay; SDK calls may block internally."));
  D->SetObjectField(TEXT("native_call_counts"),Work.Counts);
  
  
  D->SetObjectField(TEXT("actual_save_prefix"),Work.SavePrefix);D->SetObjectField(TEXT("actual_compile_prefix"),Work.CompilePrefix);D->SetObjectField(TEXT("actual_mutation_prefix"),Work.Prefix);
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
  if(Work.R.Error.IsEmpty())Work.Fail(TEXT("niagara_emitter_async_context_quarantined"),Reason);
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
  LastSnapshot=Finish(Work,Legacy,*Operation);
  FinalEncoded=true;
  if(!PublicationSafe()){FinalEncoded=false;Quarantine(TEXT("EmitterEvent final guard lost publication context."));return;}
  Cache(LastSnapshot,true,true);
  Phase=EPhase::Terminal;Unhook();
 }
 void ObserveChangedPrefix()
 {
  if(!Work.R.Error.IsEmpty()||!Work.Before.IsValid()||!Work.Admit(TEXT("async_changed_prefix")))return;
  auto Projection=Work.Projection();bool Complete=false;bool Skipped=false;Projection->TryGetBoolField(TEXT("projection_skipped_unsafe"),Skipped);Complete=!Skipped&&Projection->HasField(TEXT("system_path"))&&Work.R.Error.IsEmpty();
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
   Work.Fail(TEXT("niagara_emitter_cancelled"),Work.CancelReason);
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
   if(Dispatched){Work.Fail(TEXT("niagara_emitter_async_replay_forbidden"),TEXT("Original operation already dispatched."));Phase=EPhase::Final;Finalize();return;}
   Dispatched=true;Legacy=HandleSetNiagaraEmitterPersistentIds(Work,Params);if(Legacy.IsValid())Work.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
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
   if(!State->Work.Returned(TEXT("NewObject_EmitterEventJob"))||!Created)
   {
    State->Quarantine(TEXT("EmitterEvent Job allocation could not be safely adopted."));
    return nullptr;
   }
   State->Job.Reset(Created);FGuid RegisteredId;FGuid::Parse(State->Id,RegisteredId);NiagaraEmitterEventAuthoringAsync::RegisterJob(Created,NiagaraEmitterEventAuthoringAsync::EJobKind::PersistentIds,RegisteredId);
   if(!ParseParams(Text,State->Params))
   {
    State->Work.Fail(TEXT("niagara_emitter_invalid_params_json"),TEXT("Finite JSON object within1Mi/depth64/262144 slots required; raw presence/defaults are preserved."));
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
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> CoreLease(Running,true);
  State->Work.Fail(TEXT("niagara_emitter_async_cancelled"),State->Work.CancelReason);
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
bool HasPersistentIdsContinuationLease(){return DeferredLeaseActive;}
UCubelessNiagaraEmitterEventAuthoringJob* StartPersistentIdsContinuation(const FString& P){return PersistentIdsAsync::Start(P,TEXT("set_niagara_emitter_persistent_ids"));}
bool CancelPersistentIdsContinuation(UCubelessNiagaraEmitterEventAuthoringJob* J,const FString& R){return PersistentIdsAsync::Cancel(J,R);}
void ShutdownPersistentIdsContinuation(const FString& R){PersistentIdsAsync::Shutdown(R);}
}
