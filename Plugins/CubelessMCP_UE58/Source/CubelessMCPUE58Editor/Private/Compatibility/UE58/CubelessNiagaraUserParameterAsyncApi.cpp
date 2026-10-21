// GM: dedicated Editor-only asynchronous continuation of remove_niagara_user_parameters.
// This candidate is not a server, transport, compiler pump or rollback controller.
#include "Compatibility/UE58/CubelessNiagaraUserParameterAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraUserParameterJob.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include <atomic>
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraDataInterface.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"

namespace Cubeless::UE58::NiagaraUserParameterAsync
{
using Cubeless::UE58::Variable::FResult;
struct FJobState;
namespace
{
constexpr const TCHAR* Operation=TEXT("remove_niagara_user_parameters");
constexpr const TCHAR* TempRoot=TEXT("/Game/_MCP_Temp/NiagaraGenerated");
std::atomic<bool> ShuttingDown{false};
TSharedPtr<FJobState> Pending;
FString LastTerminal;
bool SafeContext()
{return IsInGameThread()&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage&&GEditor;}
bool Live(const UObject* O)
{return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
FString Encode(const TSharedPtr<FJsonObject>& O)
{FString S;if(O){FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));}return S;}
FString NormalizePath(const FString& Path)
{FString S=FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();S.TrimQuotesInline();if(S.StartsWith(TEXT("/"))&&!S.StartsWith(TEXT("/Script/"))&&!S.Contains(TEXT("."))){S+=TEXT(".")+FPackageName::GetShortName(S);}return S;}
FString PackagePath(FString S)
{S=FPackageName::ExportTextPathToObjectPath(S).TrimStartAndEnd();S.TrimQuotesInline();const int32 Dot=S.Find(TEXT("."),ESearchCase::CaseSensitive,ESearchDir::FromStart);if(Dot!=INDEX_NONE){S.LeftInline(Dot);}return S;}
bool TempPath(const FString& Path)
{const FString P=PackagePath(Path);return P.Equals(TempRoot)||P.StartsWith(FString(TempRoot)+TEXT("/"));}
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
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& A)
{TArray<TSharedPtr<FJsonValue>> Out;for(const FString& S:A){Out.Add(MakeShared<FJsonValueString>(S));}return Out;}
TArray<FString> Names(const TSharedPtr<FJsonObject>& P)
{TArray<FString> Out;const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(P->TryGetArrayField(TEXT("parameter_names"),A)&&A){for(const auto& V:*A){FString S;if(V.IsValid()&&V->TryGetString(S)){S.TrimStartAndEndInline();S.TrimQuotesInline();if(!S.IsEmpty()){Out.AddUnique(S);}}}}FString Single;if(P->TryGetStringField(TEXT("parameter_name"),Single)){Single.TrimStartAndEndInline();Single.TrimQuotesInline();if(!Single.IsEmpty()){Out.AddUnique(Single);}}return Out;}
FString NormalizeUser(FString S)
{S.TrimStartAndEndInline();S.TrimQuotesInline();if(!S.StartsWith(TEXT("User."),ESearchCase::IgnoreCase)){S=TEXT("User.")+S;}return S;}
bool NameMatches(const FNiagaraVariable& V,const FString& N)
{FString A=V.GetName().ToString(),B=N,AShort=A;AShort.RemoveFromStart(TEXT("User."));B.RemoveFromStart(TEXT("User."));return A.Equals(N,ESearchCase::IgnoreCase)||AShort.Equals(B,ESearchCase::IgnoreCase);}
struct FIdentity
{
    TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path;
    explicit FIdentity(UObject* O):Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()){}
    bool Exact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&Live(Package.Get())&&Live(Effective.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}
};
struct FSource{TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraGraph> Graph;};
struct FScript{TWeakObjectPtr<UNiagaraScript> Script;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;FGuid Version;};
struct FEmitter
{FGuid Id,InstanceVersion,DataVersion;FName Name;ENiagaraEmitterMode Mode=ENiagaraEmitterMode::Standard;bool Enabled=false;TWeakObjectPtr<UNiagaraEmitterBase> Base;TWeakObjectPtr<UNiagaraEmitter> Emitter;FVersionedNiagaraEmitterData* Data=nullptr;TWeakObjectPtr<UNiagaraScriptSourceBase> Source;TWeakObjectPtr<UNiagaraScratchPadContainer> Scratch,Parent;TArray<TWeakObjectPtr<UNiagaraScript>> Scripts,ScratchScripts,ParentScripts;TArray<TWeakObjectPtr<UNiagaraRendererProperties>> Renderers;TArray<TWeakObjectPtr<UNiagaraSimulationStageBase>> Stages;};
struct FExecutionPins
{TArray<TStrongObjectPtr<UObject>> Objects;void Add(UObject* O){if(O){Objects.Emplace(O);}}};
}

struct FJobState:TSharedFromThis<FJobState>
{
    enum class EPhase:uint8{Prepare,Remove,Request,Wait,Verify,Save,Final,Terminal};
    EPhase Phase=EPhase::Prepare;FResult R;TStrongObjectPtr<UCubelessNiagaraUserParameterJob> Job;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Counts=MakeShared<FJsonObject>(),Mutation=MakeShared<FJsonObject>(),Compile=MakeShared<FJsonObject>(),SavePrefix=MakeShared<FJsonObject>(),Wait=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Results,Recreated;TSharedPtr<FJsonObject> Requested,Before,After;
    FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits),SystemPath,ActualPath,FailedStage,CancelReason,LastJson,ExpectedStore;
    TArray<FString> RequestedNames;TArray<FNiagaraVariable> Existing;struct FRemoved{FNiagaraVariable Variable;TSharedPtr<FJsonObject> Row;};TArray<FRemoved> RemovedReadbacks;
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UNiagaraScript> Spawn,Update;TArray<TWeakObjectPtr<UNiagaraScript>> Scratch;
    TArray<FIdentity> Identities;TArray<FSource> Sources;TArray<FScript> Scripts;TArray<FEmitter> Emitters;FTSTicker::FDelegateHandle Ticker;
    bool Dry=true,Cleanup=true,SaveRequested=true,CompileRequested=true,WaitRequested=false,ContinueOnError=false,AllowSource=false;
    bool Captured=false,WorldCaptured=false,Executing=false,CancelRequested=false,Changed=false,Idle=false,CompileGate=false,CompileCalled=false,Saved=false;
    bool HasIdleObservation=false,LastOutstanding=false,LastActive=false;double Timeout=10.0,Poll=0.1,WaitStarted=0.0,NextPoll=0.0,Started=FPlatformTime::Seconds();
    int32 Index=0,Found=0,Removed=0,Failed=0,CleanupCalls=0,RecreatedCount=0,Iterations=0;int64 Revision=0;uint64 LastFrame=0;
    explicit FJobState():Job(NewObject<UCubelessNiagaraUserParameterJob>())
    {
        R.Compiled=false;R.Data->SetStringField(TEXT("native_job_id"),Id);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);
        R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Mutation);R.Data->SetObjectField(TEXT("actual_compile_prefix"),Compile);R.Data->SetObjectField(TEXT("actual_save_prefix"),SavePrefix);
        R.Data->SetStringField(TEXT("scoped_known_scope"),TEXT("Exact captured System/context and requested User name/type presence with complete user-name/type/dirty projection only. Other graph content, parameter values, SDK/compiler global effects and cold persistence are not proven."));
        R.Data->SetStringField(TEXT("compile_scope"),TEXT("Actual System requested/pending compile including GPU-shader idle and requested name/type absence only. No script-error/render/cold proof; no wrapper compiler pump or blocking wait."));
        R.Data->SetStringField(TEXT("save_scope"),TEXT("SDK save true is actual OR history before post-Guard; clean/current readback is separate. No atomicity or rollback, compiler cancellation, retry or cold persistence guarantee."));
        R.Data->SetBoolField(TEXT("native_transaction_started"),false);R.Data->SetBoolField(TEXT("external_transaction_across_yield_allowed"),false);R.Data->SetBoolField(TEXT("other_five_cleanup_wait_implemented"),false);
        R.Data->SetBoolField(TEXT("all_SDK_calls_nonblocking_proven"),false);R.Data->SetBoolField(TEXT("wrapper_sleep_pump_flush_called"),false);
    }
    void Bump(const TCHAR* K){double N=0;Counts->TryGetNumberField(K,N);Counts->SetNumberField(K,N+1);}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;R.bPostconditionPassed=false;return false;}
    bool Own(UObject* O)const
    {auto* S=System.Get();return Live(O)&&Live(S)&&(O==S||O->IsIn(S))&&O->GetOutermost()==S->GetOutermost()&&O->GetPackage()==S->GetPackage();}
    bool Track(UObject* O,FExecutionPins& Pins)
    {if(!Live(O)){return Fail(TEXT("niagara_user_async_object_unavailable"),TEXT("Expected loaded Engine object unavailable."));}for(const auto& I:Identities){if(I.Object.Get()==O){return I.Exact()||Fail(TEXT("niagara_user_async_identity_changed"),TEXT("Captured identity changed."));}}if(Identities.Num()>=65536){return Fail(TEXT("niagara_user_async_object_bound"),TEXT("Captured object budget exceeded."));}Pins.Add(O);Identities.Emplace(O);return true;}
    bool TrackSource(UNiagaraScriptSourceBase* S,FExecutionPins& Pins)
    {if(!S){return true;}for(const auto& X:Sources){if(X.Source.Get()==S){return true;}}if(!Track(S,Pins)){return false;}FSource X;X.Source=S;if(auto* Typed=Cast<UNiagaraScriptSource>(S)){X.Graph=Typed->NodeGraph;if(Typed->NodeGraph&&!Track(Typed->NodeGraph,Pins)){return false;}}Sources.Add(X);return true;}
    bool TrackScript(UNiagaraScript* S,FExecutionPins& Pins)
    {if(!S){return true;}for(const auto& X:Scripts){if(X.Script.Get()==S){return true;}}if(!Track(S,Pins)){return false;}FScript X;X.Script=S;X.Source=S->GetLatestSource();X.Version=S->GetExposedVersion().VersionGuid;if(!TrackSource(X.Source.Get(),Pins)){return false;}Scripts.Add(X);return true;}
    bool Acquire(FExecutionPins& Pins)
    {if(!SafeContext()||ShuttingDown){return Fail(TEXT("niagara_user_async_context_unavailable"),TEXT("Editor game thread without GC/exit/save is required; metadata-only terminal, no retry."));}for(const auto& I:Identities){auto* O=I.Object.Get();if(!Live(O)){return Fail(TEXT("niagara_user_async_target_expired"),TEXT("Owned target unloaded between phases; no replacement adopted."));}Pins.Add(O);}if(!Guard(TEXT("phase_admission"))){return false;}auto* W=World.Get();if(!Live(W)){return Fail(TEXT("niagara_user_async_world_expired"),TEXT("Captured Editor world expired."));}Pins.Add(W);return true;}
    bool Guard(const FString& Stage)
    {
        R.Stage=Stage;if(!R.Error.IsEmpty()){return false;}if(CancelRequested){return Fail(TEXT("niagara_user_async_cancelled"),CancelReason);}
        if(!SafeContext()||ShuttingDown){return Fail(TEXT("niagara_user_async_context_unavailable"),TEXT("Unsafe Editor lifetime; no more Engine observations, writer, save or rollback."));}
        if(GEditor->PlayWorld||GEditor->GetPIEWorldContext()){return Fail(TEXT("niagara_user_async_play_session_active"),TEXT("PIE/SIE changed during Editor job."));}
        auto* W=GEditor->GetEditorWorldContext().World();if(!Live(W)||W->bIsTearingDown){return Fail(TEXT("niagara_user_async_world_unavailable"),TEXT("Editor world unavailable or tearing down."));}if(WorldCaptured&&World.Get()!=W){return Fail(TEXT("niagara_user_async_world_changed"),TEXT("Captured Editor world changed; no replacement adopted."));}if(!WorldCaptured){World=W;WorldCaptured=true;}
        for(const auto& I:Identities){if(!I.Exact()){return Fail(TEXT("niagara_user_async_identity_changed"),TEXT("Captured class/outer/effective package/path changed; partial prefix retained."));}}
        if(!Captured){return true;}auto* S=System.Get();if(!Live(S)||S->GetSystemSpawnScript()!=Spawn.Get()||S->GetSystemUpdateScript()!=Update.Get()||S->ScratchPadScripts.Num()!=Scratch.Num()||S->GetEmitterHandles().Num()!=Emitters.Num()){return Fail(TEXT("niagara_user_async_system_routing_changed"),TEXT("System emitter/script routing changed."));}for(int32 I=0;I<Scratch.Num();++I){if(S->ScratchPadScripts[I]!=Scratch[I].Get()){return Fail(TEXT("niagara_user_async_scratch_changed"),TEXT("System scratch membership changed."));}}
        auto Same=[](const UNiagaraScratchPadContainer* C,const TArray<TWeakObjectPtr<UNiagaraScript>>& A){if(!C){return A.IsEmpty();}if(C->Scripts.Num()!=A.Num()){return false;}for(int32 J=0;J<A.Num();++J){if(C->Scripts[J]!=A[J].Get()){return false;}}return true;};
        for(int32 I=0;I<Emitters.Num();++I){const auto& H=S->GetEmitterHandles()[I];const auto& E=Emitters[I];if(H.GetId()!=E.Id||H.GetName()!=E.Name||H.GetEmitterMode()!=E.Mode||H.GetIsEnabled()!=E.Enabled||H.GetEmitterBase()!=E.Base.Get()||H.GetInstance().Emitter!=E.Emitter.Get()||H.GetInstance().Version!=E.InstanceVersion||H.GetEmitterData()!=E.Data||(E.Data&&E.Data->Version.VersionGuid!=E.DataVersion)){return Fail(TEXT("niagara_user_async_emitter_changed"),TEXT("Exact physical emitter instance/version changed."));}if(E.Data){if(E.Data->GraphSource!=E.Source.Get()||E.Data->ScratchPads!=E.Scratch.Get()||E.Data->ParentScratchPads!=E.Parent.Get()||!Same(E.Scratch.Get(),E.ScratchScripts)||!Same(E.Parent.Get(),E.ParentScripts)){return Fail(TEXT("niagara_user_async_emitter_routing_changed"),TEXT("Emitter source/scratch binding changed."));}TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);if(A.Num()!=E.Scripts.Num()){return Fail(TEXT("niagara_user_async_scripts_changed"),TEXT("Emitter script membership changed."));}for(int32 J=0;J<A.Num();++J){if(A[J]!=E.Scripts[J].Get()){return Fail(TEXT("niagara_user_async_scripts_changed"),TEXT("Emitter script order changed."));}}const auto& Renderers=E.Data->GetRenderers();const auto& Stages=E.Data->GetSimulationStages();if(Renderers.Num()!=E.Renderers.Num()||Stages.Num()!=E.Stages.Num()){return Fail(TEXT("niagara_user_async_cleanup_targets_changed"),TEXT("Renderer/stage membership changed."));}for(int32 J=0;J<Renderers.Num();++J){if(Renderers[J]!=E.Renderers[J].Get()){return Fail(TEXT("niagara_user_async_cleanup_targets_changed"),TEXT("Renderer order changed."));}}for(int32 J=0;J<Stages.Num();++J){if(Stages[J]!=E.Stages[J].Get()){return Fail(TEXT("niagara_user_async_cleanup_targets_changed"),TEXT("Stage order changed."));}}}}
        for(const auto& X:Sources){if(!Live(X.Source.Get())){return Fail(TEXT("niagara_user_async_source_expired"),TEXT("Captured script source expired."));}if(auto* CapturedSource=Cast<UNiagaraScriptSource>(X.Source.Get())){if(CapturedSource->NodeGraph!=X.Graph.Get()){return Fail(TEXT("niagara_user_async_graph_replaced"),TEXT("Captured source graph binding changed."));}}}
        for(const auto& X:Scripts){if(!Live(X.Script.Get())||X.Script->GetLatestSource()!=X.Source.Get()||X.Script->GetExposedVersion().VersionGuid!=X.Version){return Fail(TEXT("niagara_user_async_script_changed"),TEXT("Script source/exposed version changed."));}}return true;
    }
    bool SDK(const TCHAR* Stage,bool IsMutation=false)
    {if(!Guard(Stage)){return false;}R.bChangeStateKnown=false;R.bMutationAttempted|=IsMutation;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);Bump(Stage);return true;}
    bool Returned(const TCHAR* Stage)
    {Mutation->SetBoolField(FString(Stage)+TEXT("_SDK_returned"),true);return Guard(FString(TEXT("after_"))+Stage);}
    bool NoOuterTransaction()
    {return !GEditor->IsTransactionActive()||Fail(TEXT("niagara_user_async_external_transaction"),TEXT("No active outer transaction may cross an asynchronous removal phase. No automatic cancellation/undo of another transaction."));}
    bool ReadStore(TArray<FNiagaraVariable>& Out)
    {if(!SDK(TEXT("GetUserParameters"))){return false;}System->GetExposedParameters().GetUserParameters(Out);if(!Returned(TEXT("GetUserParameters"))){return false;}return Out.Num()<=20000||Fail(TEXT("niagara_user_async_store_bound"),TEXT("User parameter inventory exceeds 20000."));}
    FString StoreShape(const TArray<FNiagaraVariable>& A)
    {TArray<FString> Keys;for(const auto& V:A){auto K=MakeShared<FJsonObject>();K->SetStringField(TEXT("name"),V.GetName().ToString());K->SetNumberField(TEXT("flags"),V.GetType().GetFlags());K->SetStringField(TEXT("enum"),GetPathNameSafe(V.GetType().GetEnum()));K->SetStringField(TEXT("class"),GetPathNameSafe(V.GetType().GetClass()));K->SetStringField(TEXT("struct"),GetPathNameSafe(V.GetType().GetStruct()));Keys.Add(Encode(K));}Keys.Sort([](const FString& A,const FString& B){return A.Compare(B,ESearchCase::CaseSensitive)<0;});return FString::Join(Keys,TEXT("\n"));}
    TSharedPtr<FJsonObject> Observe()
    {
        auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("projection_complete"),false);if(!Guard(TEXT("target_observation"))){return O;}TArray<FNiagaraVariable> A;if(!ReadStore(A)){return O;}TArray<TSharedPtr<FJsonValue>> V;for(const auto& X:A){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),X.GetName().ToString());J->SetStringField(TEXT("type"),X.GetType().GetNameText().ToString());V.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("user_parameters"),V);O->SetStringField(TEXT("system_path"),ActualPath);O->SetBoolField(TEXT("package_dirty"),System->GetOutermost()->IsDirty());O->SetBoolField(TEXT("projection_complete"),true);return O;
    }
    void RefreshLegacy(bool IsTerminal)
    {
        Legacy->SetBoolField(TEXT("success"),IsTerminal&&R.Error.IsEmpty()&&Failed==0);Legacy->SetStringField(TEXT("system_path"),ActualPath.IsEmpty()?SystemPath:ActualPath);Legacy->SetArrayField(TEXT("parameter_names"),Strings(RequestedNames));Legacy->SetNumberField(TEXT("requested_count"),RequestedNames.Num());Legacy->SetNumberField(TEXT("processed_count"),Results.Num());Legacy->SetNumberField(TEXT("found_count"),Found);Legacy->SetNumberField(TEXT("removed_count"),Removed);Legacy->SetNumberField(TEXT("failed_count"),Failed);Legacy->SetBoolField(TEXT("changed"),Changed);Legacy->SetBoolField(TEXT("dry_run"),Dry);Legacy->SetBoolField(TEXT("remove_graph_references"),Cleanup);Legacy->SetStringField(TEXT("graph_cleanup_mode"),Cleanup?TEXT("system_handle_variable_removed"):TEXT("none"));Legacy->SetNumberField(TEXT("graph_cleanup_call_count"),CleanupCalls);Legacy->SetBoolField(TEXT("compile_requested"),CompileCalled);Legacy->SetBoolField(TEXT("compile_idle"),Idle);Legacy->SetBoolField(TEXT("compile_gate_pass"),CompileGate);Legacy->SetObjectField(TEXT("compile_wait"),Wait);Legacy->SetNumberField(TEXT("post_compile_recreated_count"),RecreatedCount);Legacy->SetArrayField(TEXT("post_compile_recreated_parameters"),Recreated);Legacy->SetBoolField(TEXT("saved"),Saved);Legacy->SetBoolField(TEXT("mutation_applied_in_memory"),!Dry&&Changed);Legacy->SetBoolField(TEXT("requires_reload_or_undo"),!Dry&&Changed&&(Failed>0||!R.Error.IsEmpty()));Legacy->SetBoolField(TEXT("save_blocked_by_failure"),SaveRequested&&Changed&&(Failed>0||!R.Error.IsEmpty()));Legacy->SetBoolField(TEXT("continue_on_error"),ContinueOnError);Legacy->SetStringField(TEXT("write_scope"),TEXT("niagara_user_parameter_removal"));Legacy->SetArrayField(TEXT("results"),Results);if(!R.Error.IsEmpty()){Legacy->SetStringField(TEXT("error_code"),R.ErrorCode);Legacy->SetStringField(TEXT("error"),R.Error);}
        Wait->SetBoolField(TEXT("wait_for_completion"),CompileCalled?WaitRequested:false);Wait->SetNumberField(TEXT("timeout_seconds"),Timeout);Wait->SetNumberField(TEXT("poll_interval_seconds"),Poll);Wait->SetNumberField(TEXT("wait_iterations"),Iterations);Wait->SetNumberField(TEXT("wait_elapsed_seconds"),WaitStarted>0.0?FPlatformTime::Seconds()-WaitStarted:0.0);if(!Wait->HasField(TEXT("wait_timed_out"))){Wait->SetBoolField(TEXT("wait_timed_out"),false);}Wait->SetBoolField(TEXT("idle_observation_known"),HasIdleObservation);Wait->SetBoolField(TEXT("outstanding_compilation_requests_after"),HasIdleObservation?LastOutstanding:false);Wait->SetBoolField(TEXT("active_compilations_after"),HasIdleObservation?LastActive:false);
    }
    void Publish()
    {
        const bool IsTerminal=Phase==EPhase::Terminal;RefreshLegacy(IsTerminal);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetStringField(TEXT("native_job_id"),Id);R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);R.Data->SetBoolField(TEXT("native_job_terminal"),IsTerminal);R.Data->SetBoolField(TEXT("operation_complete"),IsTerminal);R.Data->SetNumberField(TEXT("job_elapsed_seconds"),FPlatformTime::Seconds()-Started);R.Data->SetBoolField(TEXT("actual_direct_changed_prefix"),Changed);R.Data->SetBoolField(TEXT("compiler_may_continue_after_job"),CompileCalled&&(!Idle||!R.bSuccess));R.Data->SetBoolField(TEXT("compiler_cancel_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("full_migration_complete"),false);
        if(Before){R.Data->SetObjectField(TEXT("before"),Before);}if(After){R.Data->SetObjectField(TEXT("after"),After);}LastJson=Cubeless::Operations::EncodeVariableResult(R,Operation);
        // Never dereference even the transient Job during unsafe GC/purge/exit lifetime.
        if(SafeContext()&&Live(Job.Get())){Job->SnapshotJson=LastJson;Job->bIsTerminal=IsTerminal;}
    }
    void Unhook()
    {if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();Bump(TEXT("ticker_remove"));}}
    void Terminal(bool Success)
    {if(Phase==EPhase::Terminal){return;}Unhook();Phase=EPhase::Terminal;R.bSuccess=Success&&R.Error.IsEmpty()&&Failed==0;R.bPostconditionPassed=R.bSuccess;R.bChanged|=Changed;if(!R.bSuccess&&R.Error.IsEmpty()){Fail(TEXT("niagara_user_parameter_removal_failed"),TEXT("One or more original removal/absence checks failed; partial rows retained."));}if(R.bSuccess){R.Stage=TEXT("complete");}else{R.Stage=FailedStage.IsEmpty()?R.Stage:FailedStage;}Publish();LastTerminal=LastJson;if(Pending.Get()==this){Pending.Reset();}}
    void CancelPlain(const FString& Reason)
    {if(Phase==EPhase::Terminal){return;}CancelRequested=true;CancelReason=Reason;Bump(TEXT("cancel_calls"));if(Executing){Unhook();return;}Fail(TEXT("niagara_user_async_cancelled"),Reason);Terminal(false);}
    bool Capture(FExecutionPins& Pins)
    {
        auto* S=System.Get();if(!Track(S,Pins)||!Track(S->GetOutermost(),Pins)||!Track(S->GetPackage(),Pins)){return false;}ActualPath=S->GetPathName();
        Spawn=S->GetSystemSpawnScript();Update=S->GetSystemUpdateScript();if(!TrackScript(Spawn.Get(),Pins)||!TrackScript(Update.Get(),Pins)){return false;}for(UNiagaraScript* X:S->ScratchPadScripts){Scratch.Add(X);if(!TrackScript(X,Pins)){return false;}}
        if(S->GetEmitterHandles().Num()>4096){return Fail(TEXT("niagara_user_async_emitter_bound"),TEXT("Emitter bound exceeded."));}
        for(const auto& H:S->GetEmitterHandles())
        {FEmitter E;E.Id=H.GetId();E.Name=H.GetName();E.Mode=H.GetEmitterMode();E.Enabled=H.GetIsEnabled();E.Base=H.GetEmitterBase();if(E.Base.IsValid()&&!Track(E.Base.Get(),Pins)){return false;}E.InstanceVersion=H.GetInstance().Version;E.Emitter=H.GetInstance().Emitter;E.Data=H.GetEmitterData();if(E.Data){if(!Track(E.Emitter.Get(),Pins)){return false;}E.DataVersion=E.Data->Version.VersionGuid;E.Source=E.Data->GraphSource;E.Scratch=E.Data->ScratchPads;E.Parent=E.Data->ParentScratchPads;if(!TrackSource(E.Source.Get(),Pins)){return false;}TArray<UNiagaraScript*> A;E.Data->GetScripts(A,false,false);if(A.Num()>8192){return Fail(TEXT("niagara_user_async_script_bound"),TEXT("Script bound exceeded."));}for(auto* X:A){E.Scripts.Add(X);if(!TrackScript(X,Pins)){return false;}}auto AddScratch=[&](UNiagaraScratchPadContainer* C,TArray<TWeakObjectPtr<UNiagaraScript>>& Out){if(!C){return true;}if(!Track(C,Pins)||C->Scripts.Num()>8192){return false;}for(UNiagaraScript* X:C->Scripts){Out.Add(X);if(!TrackScript(X,Pins)){return false;}}return true;};if(!AddScratch(E.Scratch.Get(),E.ScratchScripts)||!AddScratch(E.Parent.Get(),E.ParentScripts)){return false;}for(auto* X:E.Data->GetRenderers()){if(!X&&!Dry&&Cleanup){return Fail(TEXT("niagara_user_async_renderer_invalid"),TEXT("Cleanup SDK dereferences renderer slots; malformed slot refused."));}if(X&&!Track(X,Pins)){return false;}E.Renderers.Add(X);}for(auto* X:E.Data->GetSimulationStages()){if(X&&!Track(X,Pins)){return false;}E.Stages.Add(X);}}else if(!Dry&&Cleanup){return Fail(TEXT("niagara_user_async_stateless_cleanup_unsupported"),TEXT("No safe stateful renderer/stage cleanup target for stateless handle; no writer dispatched."));}Emitters.Add(MoveTemp(E));}
        Captured=true;return Guard(TEXT("captured_target"));
    }
    bool CleanupAdmission(FExecutionPins& Pins)
    {
        if(!Guard(TEXT("cleanup_ownership"))){return false;}if(System->GetPackage()!=System->GetOutermost()){return Fail(TEXT("niagara_user_async_external_package"),TEXT("Externally packaged System is outside exact original package save scope."));}
        for(const auto& E:Emitters){if(E.Data){if(!Own(E.Emitter.Get())){return Fail(TEXT("niagara_user_async_external_cleanup"),TEXT("Referenced emitter is outside selected System containment/package."));}for(const auto& X:E.Renderers){if(!Own(X.Get())){return Fail(TEXT("niagara_user_async_external_cleanup"),TEXT("Renderer is outside selected System containment/package."));}}for(const auto& X:E.Stages){if(X.IsValid()&&!Own(X.Get())){return Fail(TEXT("niagara_user_async_external_cleanup"),TEXT("Stage is outside selected System containment/package."));}}}}
        bool Valid=true;int32 Count=0;ForEachObjectWithOuter(System.Get(),[&](UObject* O){if(!Valid){return;}if(++Count>65536){Valid=false;return;}if(O&&O->IsA<UNiagaraDataInterface>()){if(!Own(O)){Valid=false;return;}Pins.Add(O);}},true);
        return Valid||Fail(TEXT("niagara_user_async_DI_admission"),TEXT("Nested DI cleanup ownership or inventory bound failed before SDK writer."));
    }
    void Prepare(FExecutionPins& Pins)
    {
        if(!Requested||!Requested->TryGetStringField(TEXT("system_path"),SystemPath)){Fail(TEXT("niagara_user_parameter_missing_system_path"),TEXT("Missing 'system_path' parameter"));Terminal(false);return;}RequestedNames=Names(Requested);if(RequestedNames.IsEmpty()){Fail(TEXT("niagara_user_parameter_names_required"),TEXT("remove_niagara_user_parameters requires parameter_names or parameter_name"));Terminal(false);return;}
        Requested->TryGetBoolField(TEXT("dry_run"),Dry);Requested->TryGetBoolField(TEXT("remove_graph_references"),Cleanup);Requested->TryGetBoolField(TEXT("save"),SaveRequested);Requested->TryGetBoolField(TEXT("request_compile"),CompileRequested);Requested->TryGetBoolField(TEXT("wait_for_completion"),WaitRequested);Requested->TryGetBoolField(TEXT("continue_on_error"),ContinueOnError);Requested->TryGetBoolField(TEXT("allow_source_edit"),AllowSource);Requested->TryGetNumberField(TEXT("timeout_seconds"),Timeout);Requested->TryGetNumberField(TEXT("poll_interval_seconds"),Poll);Timeout=FMath::Clamp(Timeout,0.0,60.0);Poll=FMath::Clamp(Poll,0.02,1.0);
        if(!Dry&&(!CompileRequested||!WaitRequested)){Fail(TEXT("niagara_user_parameter_compile_required"),TEXT("Apply mode requires request_compile=true and wait_for_completion=true"));Terminal(false);return;}if(!AllowSource&&!TempPath(SystemPath)){Fail(TEXT("niagara_user_parameter_source_edit_not_allowed"),TEXT("Refusing User removal outside the original NiagaraGenerated root."));Terminal(false);return;}
        if(!SDK(TEXT("LoadObject_System"))){Terminal(false);return;}const FString Expected=NormalizePath(SystemPath);auto* S=LoadObject<UNiagaraSystem>(nullptr,*Expected);Mutation->SetBoolField(TEXT("load_returned_nonnull"),S!=nullptr);if(!Returned(TEXT("LoadObject_System"))){Terminal(false);return;}if(!Live(S)||!S->GetPathName().Equals(Expected,ESearchCase::CaseSensitive)){Fail(TEXT("niagara_user_async_load_failed"),TEXT("Loaded System does not match exact normalized path; no redirect replacement adopted."));Terminal(false);return;}System=S;Pins.Add(S);if(!Capture(Pins)){Terminal(false);return;}
        if(!AllowSource&&!TempPath(ActualPath)){Fail(TEXT("niagara_user_parameter_resolved_source_edit_not_allowed"),TEXT("Loaded System resolves outside the original root."));Terminal(false);return;}
        if(!Dry&&(!NoOuterTransaction()||System->GetOutermost()->IsDirty())){if(R.Error.IsEmpty()){Fail(TEXT("niagara_user_parameter_dirty_package_refused"),TEXT("Niagara package must be clean before removing exposed User parameters"));}Terminal(false);return;}
        if(!ReadStore(Existing)){Terminal(false);return;}for(const auto& V:Existing){if(!V.GetType().IsValid()){Fail(TEXT("niagara_user_async_invalid_parameter_type"),TEXT("Captured user parameter has an invalid Engine type; no writer dispatched."));Terminal(false);return;}if(UObject* EnumType=V.GetType().GetEnum()){if(!Track(EnumType,Pins)){Terminal(false);return;}}else if(UObject* StructType=V.GetType().GetStruct()){if(!Track(StructType,Pins)){Terminal(false);return;}}else if(UObject* ClassType=V.GetType().GetClass()){if(!Track(ClassType,Pins)){Terminal(false);return;}}}
        ExpectedStore=StoreShape(Existing);Before=Observe();if(!R.Error.IsEmpty()){Terminal(false);return;}if(!Dry&&Cleanup&&!CleanupAdmission(Pins)){Terminal(false);return;}Phase=EPhase::Remove;R.Stage=TEXT("prepared_remove_next_editor_frame");Publish();
    }
    void ItemFailure(const TSharedPtr<FJsonObject>& Row,const TCHAR* Code,const FString& Message)
    {++Failed;Row->SetBoolField(TEXT("success"),false);Row->SetStringField(TEXT("error_code"),Code);Row->SetStringField(TEXT("error"),Message);Results.Add(MakeShared<FJsonValueObject>(Row));if(!ContinueOnError){Index=RequestedNames.Num();}}
    void RemoveOne(FExecutionPins& Pins)
    {
        if(Index>=RequestedNames.Num()){Phase=Changed?EPhase::Request:EPhase::Final;Idle=!Changed;CompileGate=!Changed;Publish();return;}
        TArray<FNiagaraVariable> Current;if(!ReadStore(Current)){Terminal(false);return;}if(!StoreShape(Current).Equals(ExpectedStore,ESearchCase::CaseSensitive)){Fail(TEXT("niagara_user_async_store_changed_between_phases"),TEXT("User name/type inventory changed between removal phases; no further writer/replay."));Terminal(false);return;}
        const FString Name=RequestedNames[Index],Normalized=NormalizeUser(Name);auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),Index++);Row->SetStringField(TEXT("requested_name"),Name);Row->SetStringField(TEXT("normalized_name"),Normalized);R.Data->SetObjectField(TEXT("partial_current_item"),Row);
        const FNiagaraVariable* FoundVariable=Existing.FindByPredicate([&](const auto& V){return NameMatches(V,Normalized);});if(!FoundVariable){Row->SetBoolField(TEXT("found"),false);ItemFailure(Row,TEXT("niagara_user_parameter_not_found"),FString::Printf(TEXT("No matching Niagara user parameter was found: %s"),*Name));Publish();return;}
        ++Found;const FNiagaraVariable V(*FoundVariable);Row->SetBoolField(TEXT("found"),true);Row->SetStringField(TEXT("parameter_name"),V.GetName().ToString());Row->SetStringField(TEXT("type"),V.GetType().GetNameText().ToString());Row->SetBoolField(TEXT("dry_run"),Dry);Row->SetBoolField(TEXT("remove_graph_references"),Cleanup);Row->SetStringField(TEXT("graph_cleanup_mode"),Cleanup?TEXT("system_handle_variable_removed"):TEXT("none"));
        if(Dry){Row->SetBoolField(TEXT("success"),true);Row->SetBoolField(TEXT("would_remove"),true);Results.Add(MakeShared<FJsonValueObject>(Row));Publish();return;}
        if(!NoOuterTransaction()||(Cleanup&&!CleanupAdmission(Pins))){Terminal(false);return;}if(!SDK(TEXT("System_Modify"),true)){Terminal(false);return;}System->Modify();Row->SetBoolField(TEXT("Modify_SDK_returned"),true);if(!Returned(TEXT("System_Modify"))){Terminal(false);return;}
        FNiagaraVariable Qualified=V;
        if(Cleanup){if(!SDK(TEXT("RedirectUserVariable"))){Terminal(false);return;}const bool Redirected=System->GetExposedParameters().RedirectUserVariable(Qualified);const int32 FullCount=System->GetExposedParameters().Num();Row->SetNumberField(TEXT("graph_cleanup_full_declaration_count"),FullCount);if(FullCount<0||FullCount>65536){if(!Returned(TEXT("RedirectUserVariable"))){Terminal(false);return;}Fail(TEXT("niagara_user_async_full_store_bound"),TEXT("Full declaration inventory exceeds the bounded cleanup preflight before removal."));Terminal(false);return;}TArray<FNiagaraVariable> FullParameters;Bump(TEXT("GetParameters_full"));System->GetExposedParameters().GetParameters(FullParameters);FNiagaraVariable CleanupProbe=Qualified;Bump(TEXT("RedirectUserVariable_cleanup_probe"));const bool ProbeRedirected=System->GetExposedParameters().RedirectUserVariable(CleanupProbe);const bool CleanupCollision=ProbeRedirected&&(CleanupProbe.GetName()!=Qualified.GetName()||CleanupProbe.GetType()!=Qualified.GetType());Row->SetStringField(TEXT("graph_cleanup_relookup_parameter_name"),CleanupProbe.GetName().ToString());Row->SetBoolField(TEXT("graph_cleanup_redirect_collision"),CleanupCollision);Row->SetBoolField(TEXT("graph_cleanup_redirect_returned"),Redirected);Row->SetStringField(TEXT("graph_cleanup_parameter_name"),Qualified.GetName().ToString());if(!Returned(TEXT("RedirectUserVariable"))){Terminal(false);return;}if(FullParameters.Num()>65536||!Redirected||Qualified.GetType()!=V.GetType()||!FullParameters.ContainsByPredicate([&](const auto& Full){return Full.GetName()==Qualified.GetName()&&Full.GetType()==Qualified.GetType();})){Fail(TEXT("niagara_user_async_redirect_identity_invalid"),TEXT("Current alias did not resolve to an exact typed full declaration before removal; no removal or cleanup dispatched."));Terminal(false);return;}if(CleanupCollision){Fail(TEXT("niagara_user_async_nested_cleanup_redirect_unsupported"),TEXT("Public HandleVariableRemoved would re-redirect the full name to another typed declaration; no removal or cleanup dispatched."));Terminal(false);return;}}
        if(!SDK(TEXT("RemoveParameter"),true)){Terminal(false);return;}Row->SetBoolField(TEXT("remove_attempted"),true);const bool RemovedNow=System->GetExposedParameters().RemoveParameter(V);Row->SetBoolField(TEXT("RemoveParameter_SDK_returned"),true);Row->SetBoolField(TEXT("RemoveParameter_returned"),RemovedNow);Changed|=RemovedNow;R.bChanged|=RemovedNow;if(!Returned(TEXT("RemoveParameter"))){Terminal(false);return;}
        if(!RemovedNow){Row->SetBoolField(TEXT("removed"),false);ItemFailure(Row,TEXT("niagara_user_parameter_remove_failed"),TEXT("Failed to remove Niagara User parameter"));Publish();return;}
        if(Cleanup){if(!NoOuterTransaction()||!CleanupAdmission(Pins)){Terminal(false);return;}if(!SDK(TEXT("RedirectUserVariable_post_remove"))){Terminal(false);return;}FNiagaraVariable PostRemoveProbe=Qualified;const bool PostRedirected=System->GetExposedParameters().RedirectUserVariable(PostRemoveProbe);Row->SetBoolField(TEXT("graph_cleanup_post_remove_redirect_returned"),PostRedirected);Row->SetStringField(TEXT("graph_cleanup_post_remove_parameter_name"),PostRemoveProbe.GetName().ToString());if(!Returned(TEXT("RedirectUserVariable_post_remove"))){Terminal(false);return;}if(!PostRedirected||PostRemoveProbe.GetName()!=Qualified.GetName()||PostRemoveProbe.GetType()!=Qualified.GetType()){Fail(TEXT("niagara_user_async_post_remove_cleanup_redirect_changed"),TEXT("Removal callback changed the full cleanup redirect; actual removal prefix retained without cleanup, rollback or replay."));Terminal(false);return;}if(!SDK(TEXT("HandleVariableRemoved"),true)){Terminal(false);return;}Row->SetBoolField(TEXT("graph_cleanup_attempted"),true);System->HandleVariableRemoved(Qualified,true);++CleanupCalls;Row->SetBoolField(TEXT("HandleVariableRemoved_SDK_returned"),true);if(!Returned(TEXT("HandleVariableRemoved"))){Terminal(false);return;}}
        TArray<FNiagaraVariable> Post;if(!ReadStore(Post)){Terminal(false);return;}ExpectedStore=StoreShape(Post);const bool RecreatedNow=Post.ContainsByPredicate([&](const auto& X){return X.GetName()==V.GetName()&&X.GetType()==V.GetType();});if(RecreatedNow){Existing=Post;Row->SetBoolField(TEXT("removed"),false);Row->SetBoolField(TEXT("graph_cleanup_incomplete"),true);Row->SetBoolField(TEXT("mutation_applied_in_memory"),true);Row->SetBoolField(TEXT("requires_reload_or_undo"),true);ItemFailure(Row,TEXT("niagara_user_parameter_recreated_from_graph_references"),TEXT("User parameter was recreated because unsupported graph references remain; use guarded module-input topology cleanup."));Publish();return;}
        Existing.RemoveAll([&](const auto& X){return X.GetName()==V.GetName()&&X.GetType()==V.GetType();});++Removed;RemovedReadbacks.Add({V,Row});Row->SetBoolField(TEXT("success"),true);Row->SetBoolField(TEXT("removed"),true);Results.Add(MakeShared<FJsonValueObject>(Row));Publish();
    }
    void RequestCompilation()
    {
        if(!NoOuterTransaction()||!SDK(TEXT("MarkPackageDirty"),true)){Terminal(false);return;}const bool Dirty=System->MarkPackageDirty();Mutation->SetBoolField(TEXT("MarkPackageDirty_returned"),Dirty);if(!Returned(TEXT("MarkPackageDirty"))){Terminal(false);return;}
        R.bCompileRequested=true;if(!NoOuterTransaction()||!SDK(TEXT("RequestCompile"),true)){Terminal(false);return;}R.bCompileAttempted=true;R.Compiled.Reset();Compile->SetBoolField(TEXT("attempted"),true);const bool Accepted=System->RequestCompile(false);CompileCalled=true;Compile->SetBoolField(TEXT("SDK_returned"),true);Compile->SetBoolField(TEXT("accepted"),Accepted);if(!Returned(TEXT("RequestCompile"))){Terminal(false);return;}WaitStarted=FPlatformTime::Seconds();NextPoll=WaitStarted;Phase=EPhase::Wait;R.Stage=TEXT("awaiting_engine_compile_idle");Publish();
    }
    bool ObserveIdle()
    {
        if(!SDK(TEXT("ObserveCompileIdle"))){return false;}LastOutstanding=System->HasOutstandingCompilationRequests(true);LastActive=System->HasActiveCompilations();HasIdleObservation=true;++Iterations;if(!Returned(TEXT("ObserveCompileIdle"))){return false;}Idle=!LastOutstanding&&!LastActive;Compile->SetBoolField(TEXT("idle_observed"),Idle);return true;
    }
    void WaitCompilation()
    {
        const double Now=FPlatformTime::Seconds();if(Now<NextPoll){return;}NextPoll=Now+Poll;if(!ObserveIdle()){Terminal(false);return;}if(Idle){Phase=EPhase::Verify;R.Stage=TEXT("compile_idle_verify_absence_next_frame");Publish();return;}if(Now-WaitStarted>=Timeout){Wait->SetBoolField(TEXT("wait_timed_out"),true);++Failed;auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("index"),-1);Row->SetBoolField(TEXT("success"),false);Row->SetStringField(TEXT("stage"),TEXT("post_compile_readback"));Row->SetStringField(TEXT("error_code"),TEXT("niagara_user_parameter_compile_wait_failed"));Row->SetStringField(TEXT("error"),TEXT("Niagara compile did not become idle; removal remains unsaved in memory"));Row->SetBoolField(TEXT("mutation_applied_in_memory"),true);Row->SetBoolField(TEXT("requires_reload_or_undo"),true);Results.Add(MakeShared<FJsonValueObject>(Row));Fail(TEXT("niagara_user_parameter_compile_wait_failed"),TEXT("Actual monotonic compile-idle timeout; shared compiler not cancelled and no save/retry."));Terminal(false);return;}R.Stage=TEXT("awaiting_engine_compile_idle");Publish();
    }
    bool VerifyAbsence()
    {
        TArray<FNiagaraVariable> A;if(!ReadStore(A)){return false;}
        for(auto& X:RemovedReadbacks){const bool Exists=A.ContainsByPredicate([&](const auto& V){return V.GetName()==X.Variable.GetName()&&V.GetType()==X.Variable.GetType();});X.Row->SetBoolField(TEXT("post_compile_absence_verified"),!Exists);if(Exists){bool Already=false;X.Row->TryGetBoolField(TEXT("post_compile_recreation_counted"),Already);if(!Already){++Failed;++RecreatedCount;--Removed;Recreated.Add(MakeShared<FJsonValueString>(X.Variable.GetName().ToString()));X.Row->SetBoolField(TEXT("post_compile_recreation_counted"),true);}X.Row->SetBoolField(TEXT("success"),false);X.Row->SetBoolField(TEXT("removed"),false);X.Row->SetBoolField(TEXT("graph_cleanup_incomplete"),true);X.Row->SetBoolField(TEXT("mutation_applied_in_memory"),true);X.Row->SetBoolField(TEXT("requires_reload_or_undo"),true);X.Row->SetStringField(TEXT("error_code"),TEXT("niagara_user_parameter_recreated_after_compile"));X.Row->SetStringField(TEXT("error"),TEXT("Niagara User parameter was recreated during compile; mutation was not saved"));}}
        CompileGate=Idle&&RecreatedCount==0;Compile->SetBoolField(TEXT("postcompile_absence_verified"),CompileGate);return true;
    }
    void Verify()
    {if(!ObserveIdle()){Terminal(false);return;}if(!Idle){Phase=EPhase::Wait;Publish();return;}if(!VerifyAbsence()){Terminal(false);return;}Phase=SaveRequested&&Changed&&Failed==0&&CompileGate?EPhase::Save:EPhase::Final;Publish();}
    void Save()
    {
        if(!NoOuterTransaction()||!ObserveIdle()){Terminal(false);return;}if(!Idle){Phase=EPhase::Wait;Publish();return;}if(!VerifyAbsence()){Terminal(false);return;}if(Failed>0||!CompileGate){Phase=EPhase::Final;Publish();return;}
        R.bSaveRequested=true;if(!SDK(TEXT("SaveLoadedAsset"),true)){Terminal(false);return;}R.bSaveAttempted=true;SavePrefix->SetStringField(TEXT("package"),System->GetOutermost()->GetName());SavePrefix->SetStringField(TEXT("asset"),ActualPath);SavePrefix->SetBoolField(TEXT("attempted"),true);const bool ReturnedSave=UEditorAssetLibrary::SaveLoadedAsset(System.Get(),false);Saved|=ReturnedSave;R.bSaved|=ReturnedSave;SavePrefix->SetBoolField(TEXT("SDK_returned"),true);SavePrefix->SetBoolField(TEXT("SDK_save_success"),ReturnedSave);SavePrefix->SetBoolField(TEXT("actual_saved_history"),Saved);if(!Returned(TEXT("SaveLoadedAsset"))){Terminal(false);return;}const bool DirtyAfter=System->GetOutermost()->IsDirty();SavePrefix->SetBoolField(TEXT("dirty_after"),DirtyAfter);if(!ReturnedSave||DirtyAfter){Fail(TEXT("niagara_user_async_save_failed"),TEXT("SDK save failed or target dirty after callback; actual returnedTrue history preserved."));Terminal(false);return;}if(!ObserveIdle()||!VerifyAbsence()){Terminal(false);return;}if(!Idle||!CompileGate||Failed>0){Fail(TEXT("niagara_user_async_postsave_changed"),TEXT("Compile/absence changed after actual save; no automatic re-save or rollback."));Terminal(false);return;}Phase=EPhase::Final;Publish();
    }
    void Final()
    {
        if(Changed){if(!ObserveIdle()){Terminal(false);return;}if(!Idle){Fail(TEXT("niagara_user_async_final_compile_pending"),TEXT("Compiler became pending after verification/save; no replay."));Terminal(false);return;}if(!VerifyAbsence()){Terminal(false);return;}}
        After=Observe();if(!R.Error.IsEmpty()){Terminal(false);return;}bool B=false,A=false;if(Before){Before->TryGetBoolField(TEXT("projection_complete"),B);}After->TryGetBoolField(TEXT("projection_complete"),A);const bool Complete=B&&A&&Guard(TEXT("final_exact_target_admission"));R.Data->SetBoolField(TEXT("scoped_target_observation_complete"),Complete);if(Complete&&Failed==0&&(Dry||!Changed||CompileGate)){R.bChangeStateKnown=true;R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),true);Terminal(true);}else{Terminal(false);}
    }
    bool Tick(float)
    {
        if(Phase==EPhase::Terminal){return false;}if(!IsInGameThread()){return true;}if(Executing){return true;}auto KeepAlive=AsShared();TGuardValue<bool> Scope(Executing,true);FExecutionPins Pins;
        if(GFrameCounter==LastFrame){return true;}LastFrame=GFrameCounter;Bump(TEXT("ticker_visits"));if(!Acquire(Pins)){Terminal(false);return false;}
        switch(Phase){case EPhase::Prepare:Prepare(Pins);break;case EPhase::Remove:RemoveOne(Pins);break;case EPhase::Request:RequestCompilation();break;case EPhase::Wait:WaitCompilation();break;case EPhase::Verify:Verify();break;case EPhase::Save:Save();break;case EPhase::Final:Final();break;default:break;}
        if(CancelRequested&&Phase!=EPhase::Terminal){Fail(TEXT("niagara_user_async_cancelled"),CancelReason);Terminal(false);}return Phase!=EPhase::Terminal;
    }
};

UCubelessNiagaraUserParameterJob* Start(const FString& ParamsJson)
{
    if(!SafeContext()){return nullptr;}auto S=MakeShared<FJobState>();if(ShuttingDown||Pending.IsValid()){S->Fail(TEXT("niagara_user_async_controller_busy"),TEXT("Controller inactive or one owned job is already pending; no duplicate dispatch."));S->Terminal(false);return S->Job.Get();}
    if(!Parse(ParamsJson,S->Requested)){S->Fail(TEXT("niagara_user_async_invalid_params"),TEXT("Finite JSON object within 1 Mi UTF16/depth 64/262144 tokens or visits without embedded NUL or duplicate keys required."));S->Terminal(false);return S->Job.Get();}
    S->R.Data->SetObjectField(TEXT("requested_params"),S->Requested);Pending=S;TWeakPtr<FJobState> Weak=S;S->Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){if(auto P=Weak.Pin()){return P->Tick(Delta);}return false;}),0.0f);S->Bump(TEXT("ticker_add"));if(!S->Ticker.IsValid()){S->Fail(TEXT("niagara_user_async_ticker_unavailable"),TEXT("Owned ticker registration failed before writer."));S->Terminal(false);}else{S->R.Stage=TEXT("queued_prepare_next_editor_frame");S->Publish();}return S->Job.Get();
}
bool Cancel(UCubelessNiagaraUserParameterJob* Job,const FString& Reason)
{if(!IsInGameThread()||!Pending.IsValid()||Pending->Job.Get()!=Job){return false;}auto S=Pending;S->CancelPlain(Reason);return true;}
void Shutdown(const FString& Reason)
{ShuttingDown=true;if(!IsInGameThread()){return;}if(auto S=Pending){S->Unhook();S->CancelPlain(Reason);}}
FString StatusJson()
{if(!IsInGameThread()){return TEXT("{}");}auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("active"),!ShuttingDown);O->SetNumberField(TEXT("pending_count"),Pending.IsValid()?1:0);if(Pending.IsValid()){O->SetStringField(TEXT("native_job_id"),Pending->Id);O->SetStringField(TEXT("latest_snapshot"),Pending->LastJson);}O->SetStringField(TEXT("last_terminal_snapshot"),LastTerminal);O->SetBoolField(TEXT("compiler_cancel_supported"),false);return Encode(O);}
}
