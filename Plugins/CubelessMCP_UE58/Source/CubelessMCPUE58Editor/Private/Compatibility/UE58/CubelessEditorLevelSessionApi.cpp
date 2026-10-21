// Four dedicated Editor session APIs. No transport, gameplay module or historical asset dependency.
#include "Compatibility/UE58/CubelessEditorLevelSessionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Containers/Ticker.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"

namespace Cubeless::UE58::EditorLevelSession
{
namespace
{
using Variable::FResult;
constexpr int32 MaxJsonChars=1024*1024,MaxTargets=65536;
bool Running=false;
bool Live(const UObject* O)
{ return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& K) { return FCrc::StrCrc32(*K); }
};
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Object)
{
    if(Text.IsEmpty()||Text.Len()>MaxJsonChars) { return false; }
    for(TCHAR C:Text) { if(C==0) { return false; } }
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));
    TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>262144) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        { const FString Key(Reader->GetIdentifier());if(Keys.Last().Contains(Key)) { return false; }Keys.Last().Add(Key); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart)
        { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>16) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd)
        { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Values)||Values.Num()!=1||!Values[0].IsValid()||Values[0]->Type!=EJson::Object) { return false; }
    Object=Values[0]->AsObject();return Object.IsValid();
}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& A)
{ TArray<TSharedPtr<FJsonValue>> Out;for(const FString& S:A) { Out.Add(MakeShared<FJsonValueString>(S)); }return Out; }
TSharedRef<FJsonObject> Clone(const TSharedRef<FJsonObject>& In)
{
    FString Text;FJsonSerializer::Serialize(In,TJsonWriterFactory<>::Create(&Text));TSharedPtr<FJsonObject> Out;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out);return Out.IsValid()?Out.ToSharedRef():MakeShared<FJsonObject>();
}
FString NormalizeLevel(const FString& Input)
{
    FString S=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();S.TrimQuotesInline();S.ReplaceInline(TEXT("\\"),TEXT("/"));
    if(S.EndsWith(TEXT(".umap"),ESearchCase::IgnoreCase)) { FString Long;if(FPackageName::TryConvertFilenameToLongPackageName(S,Long)) { return Long; } }
    if(S.Contains(TEXT("."))) { S=FPackageName::ObjectPathToPackageName(S); }return S;
}
FString NormalizeDirty(const FString& Input)
{
    FString S=FPackageName::ExportTextPathToObjectPath(Input).TrimStartAndEnd();S.TrimQuotesInline();S.ReplaceInline(TEXT("\\"),TEXT("/"));
    if(S.EndsWith(TEXT(".umap"),ESearchCase::IgnoreCase)||S.EndsWith(TEXT(".uasset"),ESearchCase::IgnoreCase))
    { FString Long;if(FPackageName::TryConvertFilenameToLongPackageName(S,Long)) { return Long; } }
    if(S.Contains(TEXT("."))) { S=FPackageName::ObjectPathToPackageName(S); }
    while(S.Len()>1&&S.EndsWith(TEXT("/"))) { S.LeftChopInline(1); }return S;
}
FString NormalizeRoot(const FString& Input,bool ExportObject)
{
    FString S=(ExportObject?FPackageName::ExportTextPathToObjectPath(Input):Input).TrimStartAndEnd();S.TrimQuotesInline();S.ReplaceInline(TEXT("\\"),TEXT("/"));
    if(ExportObject&&S.Contains(TEXT("."))) { S=FPackageName::ObjectPathToPackageName(S); }
    while(S.Len()>1&&S.EndsWith(TEXT("/"))) { S.LeftChopInline(1); }return S;
}
bool ValidPackage(const FString& Name,FText& Reason)
{ return !Name.IsEmpty()&&!Name.Contains(TEXT("//"))&&FPackageName::IsValidLongPackageName(Name,true,&Reason); }
struct FDeferredState
{
    FCriticalSection Mutex;
    bool Pending=false,ShuttingDown=false,InCallback=false;
    FString RequestId;
    FTSTicker::FDelegateHandle Handle;
    // Published objects are immutable snapshots. No UObject access occurs under Mutex.
    TSharedPtr<FJsonObject> LastStatus,LastExecution;
} Deferred;
struct FSessionSnapshot
{
    bool Pending=false,ShuttingDown=false,InCallback=false;
    FString RequestId;
    TSharedPtr<FJsonObject> LastStatus,LastExecution;
};
FSessionSnapshot Session()
{
    FScopeLock Lock(&Deferred.Mutex);FSessionSnapshot S;
    S.Pending=Deferred.Pending;S.ShuttingDown=Deferred.ShuttingDown;S.InCallback=Deferred.InCallback;S.RequestId=Deferred.RequestId;S.LastStatus=Deferred.LastStatus;S.LastExecution=Deferred.LastExecution;return S;
}
struct FWork
{
    FResult Result;FString Command,FailedStage,RequestId;TSharedPtr<FJsonObject> Params;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();
    TArray<FString> Before;bool BeforeCaptured=false,WorldBeforeCaptured=false;FString BeforeWorld;TWeakObjectPtr<UEditorEngine> EditorOwner;
    uint32 LastWorldIdentity=0,BeforeWorldIdentity=0;TArray<TPair<FString,TWeakObjectPtr<UPackage>>> BeforePackages;
    FWork(const TCHAR* Name):Command(Name)
    {
        Result.Compiled=false;Legacy->SetBoolField(TEXT("success"),true);Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);
        Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);Result.Data->SetObjectField(TEXT("postconditions"),Checks);Result.Data->SetObjectField(TEXT("actual_partial_state"),Prefix);
        for(const TCHAR* Key:{TEXT("atomic_rollback_available"),TEXT("automatic_retry_permitted"),TEXT("explicit_compile_attempted"),TEXT("explicit_save_attempted"),TEXT("disk_reload_verified"),TEXT("complete_global_side_effect_inventory"),TEXT("legacy_weather_cleanup_equivalent")}) { Result.Data->SetBoolField(Key,false); }
        Result.Data->SetStringField(TEXT("scope"),TEXT("Dedicated Editor level/session APIs; loaded-package flags and observed current world/file/dirty projections. No TCP bridge, gameplay dependency, UI automation, implicit asset save or content rollback."));
        Result.Data->SetStringField(TEXT("retry_risk"),TEXT("Scheduling, package flags, directory creation, NewMap, synchronous LoadMap or SaveMap can leave changes before a failure. Do not retry an uncertain request automatically. Dirty flag clearing is not save/reload/content rollback."));
        Result.Data->SetStringField(TEXT("SDK_synchronous_limit"),TEXT("Only delayed admission uses public FTSTicker. LoadMap, NewMap and SaveMap remain synchronous game-thread SDK calls; there is no added wait/flush, worker UObject access or asynchronous-completion claim."));
        Result.Data->SetStringField(TEXT("legacy_weather_cleanup_limit"),TEXT("Historical weather driver/TargetManager ownership cleanup is not ported. No historical asset is resolved, loaded or reflected; core session behavior does not establish that auxiliary parity."));
        for(const TCHAR* Key:{TEXT("dirty_snapshot_calls"),TEXT("dirty_snapshot_returns"),TEXT("current_world_queries"),TEXT("current_world_returns"),TEXT("add_ticker_calls"),TEXT("add_ticker_returns"),TEXT("load_map_calls"),TEXT("load_map_returns"),TEXT("set_dirty_flag_calls"),TEXT("set_dirty_flag_returns"),TEXT("directory_create_calls"),TEXT("directory_create_returns"),TEXT("new_map_calls"),TEXT("new_map_returns"),TEXT("map_save_calls"),TEXT("map_save_returns"),TEXT("compile_blueprint_calls"),TEXT("asset_save_calls"),TEXT("wrapper_wait_calls")}) { Calls->SetNumberField(Key,0); }
    }
    void Count(const TCHAR* Key) { double N=0;Calls->TryGetNumberField(Key,N);Calls->SetNumberField(Key,N+1); }
    bool Fail(const TCHAR* Code,const FString& Error,bool LegacyFailure=true)
    {
        if(Result.Error.IsEmpty()) { FailedStage=Result.Stage;Result.ErrorCode=FString(TEXT("editor_level_session_"))+Code;Result.Error=Error; }
        Result.bSuccess=false;if(LegacyFailure) { Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error); }return false;
    }
    bool Request(const FString& Text)
    {
        Result.Stage=TEXT("parse_params");if(!Parse(Text,Params)) { return Fail(TEXT("params_invalid"),TEXT("This dedicated Editor session API requires one bounded JSON params object")); }
        Result.Data->SetStringField(TEXT("params_json"),Text);Result.Data->SetObjectField(TEXT("params"),Params.ToSharedRef());return true;
    }
    bool Enter()
    {
        Result.Stage=TEXT("editor_admission");if(!IsInGameThread()) { return Fail(TEXT("game_thread_required"),TEXT("Requires the Editor game thread")); }
        if(Session().ShuttingDown) { return Fail(TEXT("shutting_down"),TEXT("Editor level session module is shutting down")); }
        if(Running) { return Fail(TEXT("reentrant_call"),TEXT("Another Editor level session operation is active")); }EditorOwner=GEditor;return true;
    }
    FString World()
    {
        if(!IsInGameThread()||Session().ShuttingDown) { Result.bChangeStateKnown=false;Prefix->SetBoolField(TEXT("final_engine_observation_skipped"),true);return FString(); }
        Result.bChangeStateKnown=false;Count(TEXT("current_world_queries"));FString Name;LastWorldIdentity=0;
        if(Live(GEditor)) { UWorld* W=GEditor->GetEditorWorldContext().World();if(Live(W)&&Live(W->GetOutermost())) { Name=W->GetOutermost()->GetName();LastWorldIdentity=W->GetUniqueID(); } }
        Prefix->SetNumberField(TEXT("observed_current_world_object_id"),LastWorldIdentity);
        Count(TEXT("current_world_returns"));return Name;
    }
    TArray<FString> Dirty()
    {
        if(!IsInGameThread()||Session().ShuttingDown) { Result.bChangeStateKnown=false;Prefix->SetBoolField(TEXT("final_engine_observation_skipped"),true);return {}; }
        Result.bChangeStateKnown=false;Count(TEXT("dirty_snapshot_calls"));TArray<UPackage*> Packages;FEditorFileUtils::GetDirtyPackages(Packages);
        TArray<FString> Names;TSet<FString> Seen;for(UPackage* P:Packages) { if(Live(P)&&!Seen.Contains(P->GetName())) { Seen.Add(P->GetName());Names.Add(P->GetName()); } }
        Names.Sort();Count(TEXT("dirty_snapshot_returns"));return Names;
    }
    void Start()
    {
        BeforeWorld=World();BeforeWorldIdentity=LastWorldIdentity;WorldBeforeCaptured=true;Before=Dirty();BeforeCaptured=true;
        Prefix->SetStringField(TEXT("current_world_package_name_before"),BeforeWorld);Prefix->SetArrayField(TEXT("dirty_packages_before"),Strings(Before));
        Prefix->SetNumberField(TEXT("current_world_object_id_before"),BeforeWorldIdentity);
        for(const FString& Name:Before) { BeforePackages.Emplace(Name,FindPackage(nullptr,*Name)); }
    }
    void Delta()
    {
        if(!IsInGameThread()||Session().ShuttingDown) { Result.bChangeStateKnown=false;Prefix->SetBoolField(TEXT("final_engine_observation_skipped"),true);return; }
        if(!BeforeCaptured) { return; }const auto After=Dirty();TSet<FString> BeforeSet,AfterSet;for(const FString& S:Before) { BeforeSet.Add(S); }for(const FString& S:After) { AfterSet.Add(S); }
        TArray<FString> Added,Removed;for(const FString& S:After) { if(!BeforeSet.Contains(S)) { Added.Add(S); } }for(const FString& S:Before) { if(!AfterSet.Contains(S)) { Removed.Add(S); } }Added.Sort();Removed.Sort();
        Legacy->SetNumberField(TEXT("dirty_package_count_before"),Before.Num());Legacy->SetArrayField(TEXT("dirty_packages_before"),Strings(Before));Legacy->SetNumberField(TEXT("dirty_package_count_after"),After.Num());Legacy->SetArrayField(TEXT("dirty_packages_after"),Strings(After));
        Legacy->SetNumberField(TEXT("dirty_package_added_count"),Added.Num());Legacy->SetArrayField(TEXT("dirty_packages_added_by_command"),Strings(Added));Legacy->SetNumberField(TEXT("dirty_package_removed_count"),Removed.Num());Legacy->SetArrayField(TEXT("dirty_packages_removed_by_command"),Strings(Removed));
        Prefix->SetArrayField(TEXT("dirty_packages_after"),Strings(After));if(!Added.IsEmpty()||!Removed.IsEmpty()) { Result.bChanged=true; }
        TArray<TSharedPtr<FJsonValue>> Lifetimes;for(const auto& Entry:BeforePackages)
        {
            auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("package_name"),Entry.Key);const bool Alive=Live(Entry.Value.Get());J->SetBoolField(TEXT("original_package_still_live"),Alive);
            if(Alive) { J->SetBoolField(TEXT("actual_dirty_after"),Entry.Value->IsDirty()); }else { Result.bChanged=true; }
            Lifetimes.Add(MakeShared<FJsonValueObject>(J));
        }Prefix->SetArrayField(TEXT("before_dirty_package_lifetimes_after"),Lifetimes);
    }
    void Current()
    {
        if(!IsInGameThread()||Session().ShuttingDown) { Result.bChangeStateKnown=false;Prefix->SetBoolField(TEXT("final_engine_observation_skipped"),true);return; }
        const FString Now=World();const auto Names=Dirty();Legacy->SetStringField(TEXT("current_world_package_name_now"),Now);Legacy->SetNumberField(TEXT("dirty_package_count"),Names.Num());Legacy->SetArrayField(TEXT("dirty_packages"),Strings(Names));
        Prefix->SetStringField(TEXT("current_world_package_name_after"),Now);if(WorldBeforeCaptured&&(Now!=BeforeWorld||LastWorldIdentity!=BeforeWorldIdentity)) { Result.bChanged=true; }
    }
    bool Writable(const FString& ExpectedWorld,uint32 ExpectedIdentity=0,bool OwnPending=false)
    {
        if(!IsEditorGameThread()||!Live(GEditor)||GEditor!=EditorOwner.Get()) { return Fail(TEXT("editor_changed"),TEXT("Editor owner is unavailable or changed before the SDK write")); }
        const auto S=Session();if(S.ShuttingDown) { return Fail(TEXT("shutting_down"),TEXT("Editor session shutdown began before the SDK write")); }
        if(IsPlaySessionActive()) { return Fail(TEXT("PIE_active"),TEXT("PIE/SIE became active before the SDK write")); }
        if(S.Pending&&(!OwnPending||S.RequestId!=RequestId)) { return Fail(TEXT("open_pending"),TEXT("Another editor level open is pending")); }
        if(World()!=ExpectedWorld||(ExpectedIdentity!=0&&LastWorldIdentity!=ExpectedIdentity)) { return Fail(TEXT("world_changed"),TEXT("Current Editor world changed before the SDK write")); }return true;
    }
    void Write(const TCHAR* Stage,const TCHAR* Counter)
    { Result.Stage=Stage;Result.bMutationAttempted=true;Result.bChangeStateKnown=false;Count(Counter); }
    FResult Finish(bool Postcondition=true)
    {
        Result.Data->SetBoolField(TEXT("explicit_save_attempted"),Result.bSaveAttempted);Result.Data->SetStringField(TEXT("failed_stage"),FailedStage);
        Checks->SetBoolField(TEXT("explicit_compile_not_attempted"),!Result.bCompileAttempted);Checks->SetBoolField(TEXT("automatic_retry_disabled"),true);
        Result.bSuccess=Result.Error.IsEmpty();Result.bPostconditionPassed=Result.bSuccess&&Postcondition;if(Result.bSuccess) { Result.Stage=TEXT("complete"); }else { Result.Stage=FailedStage; }return Result;
    }
    TSharedRef<FJsonObject> Execution()
    {
        auto E=MakeShared<FJsonObject>();E->SetStringField(TEXT("request_id"),RequestId);E->SetStringField(TEXT("operation"),Command);E->SetStringField(TEXT("stage"),Result.Stage);E->SetStringField(TEXT("error_code"),Result.ErrorCode);E->SetStringField(TEXT("error"),Result.Error);E->SetStringField(TEXT("failed_stage"),FailedStage);
        E->SetBoolField(TEXT("success"),Result.bSuccess);E->SetBoolField(TEXT("mutation_attempted"),Result.bMutationAttempted);E->SetBoolField(TEXT("change_state_known"),Result.bChangeStateKnown);if(Result.bChangeStateKnown) { E->SetBoolField(TEXT("changed"),Result.bChanged); }else { E->SetField(TEXT("changed"),MakeShared<FJsonValueNull>()); }E->SetBoolField(TEXT("observed_projection_changed"),Result.bChanged);E->SetBoolField(TEXT("postcondition_passed"),Result.bPostconditionPassed);E->SetBoolField(TEXT("compile_attempted"),Result.bCompileAttempted);E->SetBoolField(TEXT("save_attempted"),Result.bSaveAttempted);E->SetBoolField(TEXT("saved"),Result.bSaved);E->SetBoolField(TEXT("automatic_retry_permitted"),false);
        E->SetObjectField(TEXT("native_call_counts"),Clone(Calls));E->SetObjectField(TEXT("postconditions"),Clone(Checks));E->SetObjectField(TEXT("actual_partial_state"),Clone(Prefix));return E;
    }
};
void Publish(const TSharedRef<FWork>& W,bool Pending)
{
    W->Result.Data->SetBoolField(TEXT("operation_complete"),!Pending);const auto L=Clone(W->Legacy),E=W->Execution();E->SetBoolField(TEXT("operation_complete"),!Pending);FScopeLock Lock(&Deferred.Mutex);
    if(Deferred.RequestId==W->RequestId) { Deferred.Pending=Pending;Deferred.LastStatus=L;Deferred.LastExecution=E; }
}
bool DeferredOpen(const TSharedRef<FWork>& W,const FString& Filename,const FString& Target,bool AllowDirty,bool AsTemplate,bool ShowProgress)
{
    {
        FScopeLock Lock(&Deferred.Mutex);
        if(Deferred.ShuttingDown||!Deferred.Pending||Deferred.RequestId!=W->RequestId) { return false; }
        Deferred.InCallback=true;Deferred.Handle.Reset();
    }
    ON_SCOPE_EXIT { FScopeLock Lock(&Deferred.Mutex);Deferred.InCallback=false; };
    W->Result.Stage=TEXT("deferred_admission");W->Legacy->SetBoolField(TEXT("load_scheduled"),false);
    if(!IsInGameThread())
    {
        W->Fail(TEXT("callback_game_thread_required"),TEXT("Deferred callback did not run on the Editor game thread; no Engine work was attempted"));
        W->Legacy->SetStringField(TEXT("open_status"),TEXT("failed"));W->Finish(false);Publish(W,false);return false;
    }
    if(Running) { W->Fail(TEXT("callback_reentrant"),TEXT("Deferred callback encountered an active session operation"));W->Legacy->SetStringField(TEXT("open_status"),TEXT("failed"));W->Finish(false);Publish(W,false);return false; }
    TGuardValue<bool> RunningGuard(Running,true);
    TArray<FString> Reasons;const auto CurrentDirty=W->Dirty();const FString CallbackWorld=W->World();const uint32 CallbackWorldIdentity=W->LastWorldIdentity;
    if(!Live(GEditor)||GEditor!=W->EditorOwner.Get()) { Reasons.Add(TEXT("editor_unavailable_or_changed")); }
    if(IsPlaySessionActive()) { Reasons.Add(TEXT("pie_or_sie_active")); }
    if(!AllowDirty&&!CurrentDirty.IsEmpty()) { Reasons.Add(TEXT("dirty_packages_present")); }
    if(!FPaths::FileExists(Filename)) { Reasons.Add(TEXT("target_map_file_missing")); }
    if(CallbackWorld!=W->BeforeWorld||CallbackWorldIdentity!=W->BeforeWorldIdentity) { Reasons.Add(TEXT("editor_world_changed_since_schedule")); }
    if(Session().ShuttingDown) { Reasons.Add(TEXT("module_shutting_down")); }
    W->Prefix->SetArrayField(TEXT("callback_dirty_packages"),Strings(CurrentDirty));W->Prefix->SetStringField(TEXT("callback_current_world_package_name"),CallbackWorld);W->Prefix->SetNumberField(TEXT("callback_current_world_object_id"),CallbackWorldIdentity);W->Prefix->SetArrayField(TEXT("callback_blocked_reasons"),Strings(Reasons));
    if(!Reasons.IsEmpty())
    {
        W->Fail(TEXT("callback_admission_failed"),TEXT("Deferred Editor level transition was blocked by the callback-time admission checks"));W->Legacy->SetStringField(TEXT("open_status"),TEXT("failed"));W->Legacy->SetArrayField(TEXT("callback_blocked_reasons"),Strings(Reasons));W->Legacy->SetStringField(TEXT("message"),TEXT("Deferred Editor level transition blocked before LoadMap"));
        W->Delta();W->Current();W->Checks->SetBoolField(TEXT("callback_load_not_attempted"),true);W->Finish(false);Publish(W,false);return false;
    }
    W->Legacy->SetStringField(TEXT("open_status"),TEXT("loading"));W->Legacy->SetBoolField(TEXT("load_attempted"),true);W->Legacy->SetStringField(TEXT("load_started_world_package_name"),W->World());W->Result.Stage=TEXT("load_map");Publish(W,true);
    W->Write(TEXT("load_map"),TEXT("load_map_calls"));
    const bool Loaded=FEditorFileUtils::LoadMap(Filename,AsTemplate,ShowProgress);W->Count(TEXT("load_map_returns"));
    if(Session().ShuttingDown)
    {
        W->Legacy->SetBoolField(TEXT("loaded"),Loaded);W->Legacy->SetStringField(TEXT("open_status"),TEXT("failed"));W->Prefix->SetBoolField(TEXT("load_map_returned"),Loaded);W->Prefix->SetBoolField(TEXT("post_load_observation_skipped_for_shutdown"),true);
        W->Fail(TEXT("shutdown_after_load"),TEXT("Module shutdown began during LoadMap; its return is preserved and no further Engine observations were made"));W->Finish(false);Publish(W,false);return false;
    }
    W->Legacy->SetBoolField(TEXT("loaded"),Loaded);const FString After=W->World();W->Legacy->SetStringField(TEXT("current_world_package_name_after"),After);W->Legacy->SetBoolField(TEXT("target_world_is_current"),After==Target);
    W->Prefix->SetBoolField(TEXT("load_map_returned"),Loaded);W->Prefix->SetBoolField(TEXT("load_as_template"),AsTemplate);W->Delta();W->Current();
    W->Legacy->SetStringField(TEXT("open_status"),Loaded?TEXT("loaded"):TEXT("failed"));W->Legacy->SetStringField(TEXT("message"),Loaded?TEXT("Editor level load completed"):TEXT("FEditorFileUtils::LoadMap returned false"));
    if(!Loaded) { W->Fail(TEXT("load_failed"),TEXT("FEditorFileUtils::LoadMap returned false")); }
    const bool WorldMatches=AsTemplate?!After.IsEmpty():After==Target;W->Checks->SetBoolField(TEXT("load_returned_true"),Loaded);W->Checks->SetBoolField(TEXT("world_projection_matches_load_mode"),WorldMatches);W->Checks->SetBoolField(TEXT("template_package_identity_not_required"),AsTemplate);
    if(Loaded&&!WorldMatches) { W->Fail(TEXT("load_world_mismatch"),TEXT("LoadMap returned true but the observed current world does not match its load mode"),false); }
    W->Finish(Loaded&&WorldMatches);Publish(W,false);return false;
}
} // namespace

Variable::FResult Open(const FString& ParamsJson)
{
    auto W=MakeShared<FWork>(TEXT("open_editor_level"));if(!W->Request(ParamsJson)||!W->Enter()) { return W->Finish(false); }TGuardValue<bool> RunningGuard(Running,true);
    FString Requested;if(!W->Params->TryGetStringField(TEXT("level_path"),Requested)&&!W->Params->TryGetStringField(TEXT("map_path"),Requested)) { W->Fail(TEXT("level_path_missing"),TEXT("Missing 'level_path' parameter"));return W->Finish(false); }
    bool Dry=true,AllowDirty=false,AsTemplate=false,ShowProgress=true;W->Params->TryGetBoolField(TEXT("dry_run"),Dry);W->Params->TryGetBoolField(TEXT("allow_dirty_packages"),AllowDirty);W->Params->TryGetBoolField(TEXT("load_as_template"),AsTemplate);W->Params->TryGetBoolField(TEXT("show_progress"),ShowProgress);
    double Delay=1;W->Params->TryGetNumberField(TEXT("deferred_load_delay_seconds"),Delay);if(!FMath::IsFinite(Delay)) { W->Fail(TEXT("delay_nonfinite"),TEXT("deferred_load_delay_seconds must resolve to a finite number"));return W->Finish(false); }Delay=FMath::Clamp(Delay,0.0,30.0);
    const FString Target=NormalizeLevel(Requested);FText Reason;if(!ValidPackage(Target,Reason)) { W->Fail(TEXT("level_path_invalid"),FString::Printf(TEXT("Invalid level package path '%s': %s"),*Requested,*Reason.ToString()));return W->Finish(false); }
    const FString Filename=FPackageName::LongPackageNameToFilename(Target,FPackageName::GetMapPackageExtension());const bool Exists=FPaths::FileExists(Filename);W->Start();const bool Already=W->BeforeWorld==Target;const auto S=Session();TArray<FString> Reasons;
    if(!Exists) { Reasons.Add(TEXT("target_map_file_missing")); }if(!Already&&!AllowDirty&&!W->Before.IsEmpty()) { Reasons.Add(TEXT("dirty_packages_present")); }if(!Already&&IsPlaySessionActive()) { Reasons.Add(TEXT("pie_or_sie_active")); }if(!Already&&S.Pending) { Reasons.Add(TEXT("editor_level_open_already_pending")); }if(!Already&&!Live(GEditor)) { Reasons.Add(TEXT("editor_unavailable")); }
    auto L=W->Legacy;L->SetStringField(TEXT("requested_level_path"),Requested);L->SetStringField(TEXT("target_long_package_name"),Target);L->SetStringField(TEXT("target_filename"),Filename);L->SetBoolField(TEXT("target_exists"),Exists);L->SetStringField(TEXT("current_world_package_name"),W->BeforeWorld);L->SetBoolField(TEXT("already_open"),Already);L->SetBoolField(TEXT("dry_run"),Dry);L->SetBoolField(TEXT("allow_dirty_packages"),AllowDirty);L->SetBoolField(TEXT("can_load"),Reasons.IsEmpty()||Already);L->SetBoolField(TEXT("deferred_real_load"),true);L->SetNumberField(TEXT("deferred_load_delay_seconds"),Delay);L->SetArrayField(TEXT("blocked_reasons"),Strings(Reasons));L->SetNumberField(TEXT("dirty_package_count_before"),W->Before.Num());L->SetArrayField(TEXT("dirty_packages_before"),Strings(W->Before));
    if(Dry||Already||!Reasons.IsEmpty())
    {
        L->SetBoolField(TEXT("load_attempted"),false);L->SetBoolField(TEXT("loaded"),Already);L->SetStringField(TEXT("message"),Dry?TEXT("Dry run only; no editor level transition was attempted"):(Already?TEXT("Target level is already open"):TEXT("Editor level transition blocked")));W->Checks->SetBoolField(TEXT("load_not_attempted"),true);return W->Finish();
    }
    W->RequestId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);L->SetStringField(TEXT("request_id"),W->RequestId);L->SetStringField(TEXT("open_status"),TEXT("scheduled"));L->SetBoolField(TEXT("load_attempted"),false);L->SetBoolField(TEXT("load_scheduled"),true);L->SetBoolField(TEXT("loaded"),false);L->SetStringField(TEXT("message"),TEXT("Editor level load was scheduled for a later editor tick. Poll get_editor_level_open_status for completion."));
    W->Write(TEXT("schedule_open"),TEXT("add_ticker_calls"));W->Result.bChanged=true;W->Prefix->SetBoolField(TEXT("request_state_written"),true);W->Prefix->SetStringField(TEXT("request_id"),W->RequestId);
    { FScopeLock Lock(&Deferred.Mutex);Deferred.Pending=true;Deferred.RequestId=W->RequestId; }
    const auto Handle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([W,Filename,Target,AllowDirty,AsTemplate,ShowProgress](float){ return DeferredOpen(W,Filename,Target,AllowDirty,AsTemplate,ShowProgress); }),static_cast<float>(Delay));W->Count(TEXT("add_ticker_returns"));
    { FScopeLock Lock(&Deferred.Mutex);Deferred.Handle=Handle; }
    W->Prefix->SetBoolField(TEXT("owned_ticker_handle_valid"),Handle.IsValid());W->Checks->SetBoolField(TEXT("scheduled_not_loaded"),true);W->Checks->SetBoolField(TEXT("owned_ticker_created"),Handle.IsValid());
    if(!Handle.IsValid()) { W->Fail(TEXT("ticker_schedule_failed"),TEXT("FTSTicker did not return a live owned handle"));L->SetStringField(TEXT("open_status"),TEXT("failed"));L->SetBoolField(TEXT("load_scheduled"),false);W->Finish(false);Publish(W,false);return W->Result; }
    W->Finish();Publish(W,true);return W->Result;
}

Variable::FResult Status(const FString& ParamsJson)
{
    FWork W(TEXT("get_editor_level_open_status"));if(!W.Request(ParamsJson)||!W.Enter()) { return W.Finish(false); }TGuardValue<bool> RunningGuard(Running,true);
    FString Requested;W.Params->TryGetStringField(TEXT("request_id"),Requested);const auto S=Session();W.Legacy->SetBoolField(TEXT("has_request"),S.LastStatus.IsValid());W.Legacy->SetBoolField(TEXT("pending"),S.Pending);W.Current();
    if(!S.LastStatus.IsValid()) { W.Legacy->SetStringField(TEXT("open_status"),TEXT("none"));W.Checks->SetBoolField(TEXT("status_observer_only"),true);return W.Finish(); }
    const bool Matches=Requested.IsEmpty()||Requested==S.RequestId;W.Legacy->SetBoolField(TEXT("request_matches"),Matches);W.Legacy->SetStringField(TEXT("request_id"),S.RequestId);if(!Matches) { W.Legacy->SetStringField(TEXT("open_status"),TEXT("request_id_mismatch")); }
    W.Legacy->SetObjectField(TEXT("last_open_status"),S.LastStatus);if(S.LastExecution.IsValid()) { W.Result.Data->SetObjectField(TEXT("last_open_execution"),S.LastExecution); }
    W.Checks->SetBoolField(TEXT("status_observer_only"),true);W.Checks->SetBoolField(TEXT("latest_request_only"),true);W.Result.Data->SetStringField(TEXT("journal_policy"),TEXT("The callback updates only this in-memory status. It does not reopen or update a completed Python mutation checkpoint. Correlate the scheduling receipt and this snapshot by request_id."));return W.Finish();
}

Variable::FResult ClearDirty(const FString& ParamsJson)
{
    FWork W(TEXT("clear_dirty_package_flags"));if(!W.Request(ParamsJson)||!W.Enter()) { return W.Finish(false); }TGuardValue<bool> RunningGuard(Running,true);
    bool Dry=true,All=false,NonTemp=false;W.Params->TryGetBoolField(TEXT("dry_run"),Dry);W.Params->TryGetBoolField(TEXT("clear_all_dirty"),All);W.Params->TryGetBoolField(TEXT("allow_non_temp_path"),NonTemp);FString Root=TEXT("/Game/_MCP_Temp");W.Params->TryGetStringField(TEXT("required_root"),Root);Root=NormalizeRoot(Root,true);
    TArray<FString> Requested;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
    if(W.Params->TryGetArrayField(TEXT("package_paths"),Values)) { for(const auto& V:*Values) { FString S;if(!V.IsValid()||!V->TryGetString(S)) { W.Fail(TEXT("package_paths_invalid"),TEXT("'package_paths' must contain only strings"));return W.Finish(false); }Requested.Add(S); } }
    FString Single;if(W.Params->TryGetStringField(TEXT("package_path"),Single)&&!Single.IsEmpty()) { Requested.Add(Single); }
    W.Start();if(!All&&Requested.IsEmpty()) { W.Fail(TEXT("targets_missing"),TEXT("Provide 'package_paths', 'package_path', or clear_all_dirty=true"));return W.Finish(false); }
    if(Requested.Num()+W.Before.Num()>MaxTargets) { W.Fail(TEXT("targets_limit"),TEXT("Requested/dirty target input exceeds the bounded target limit"));return W.Finish(false); }
    TSet<FString> DirtySet,Seen;for(const FString& Name:W.Before) { DirtySet.Add(Name); }TArray<FString> Global;
    if(!Live(GEditor)) { Global.Add(TEXT("editor_unavailable")); }if(IsPlaySessionActive()) { Global.Add(TEXT("pie_or_sie_active")); }if(Session().Pending) { Global.Add(TEXT("editor_level_open_already_pending")); }
    struct FTarget { FString Requested,Name;TWeakObjectPtr<UPackage> Package;bool WasDirty=false,Loaded=false,Under=false,Can=false,Called=false,ActualBefore=false,AfterKnown=false,ActualAfter=false;TArray<FString> Blocked; };
    TArray<FTarget> Targets;
    auto Add=[&](const FString& Input)
    {
        FTarget T;T.Requested=Input;T.Name=NormalizeDirty(Input);FText Reason;
        if(!ValidPackage(T.Name,Reason)) { T.Blocked.Add(FString::Printf(TEXT("invalid_package_path: %s"),*Reason.ToString()));Targets.Add(MoveTemp(T));return; }
        if(Seen.Contains(T.Name)) { return; }Seen.Add(T.Name);T.WasDirty=DirtySet.Contains(T.Name);T.Package=FindPackage(nullptr,*T.Name);T.Loaded=Live(T.Package.Get());T.Under=Root.IsEmpty()||T.Name==Root||T.Name.StartsWith(Root+TEXT("/"));
        if(!NonTemp&&!T.Under) { T.Blocked.Add(TEXT("target_outside_required_root")); }if(!T.Loaded) { T.Blocked.Add(TEXT("package_not_loaded")); }else { T.ActualBefore=T.Package->IsDirty(); }
        T.Can=T.WasDirty&&T.Loaded&&T.Blocked.IsEmpty();Targets.Add(MoveTemp(T));
    };
    if(All) { for(const FString& S:W.Before) { Add(S); } }for(const FString& S:Requested) { Add(S); }
    int32 Blocked=0,Clearable=0;for(const auto& T:Targets) { Blocked+=T.Blocked.IsEmpty()?0:1;Clearable+=T.Can?1:0; }
    const bool CanExecute=!Dry&&Global.IsEmpty()&&Blocked==0;const bool Attempt=CanExecute&&Clearable>0;int32 Called=0,Verified=0;TArray<FString> Cleared;
    W.Checks->SetBoolField(TEXT("all_target_admission_passed"),Global.IsEmpty()&&Blocked==0);
    // Recheck every clearable package before the first write; later callbacks can still leave a partial prefix.
    bool AllStable=true;if(Attempt)
    {
        W.Result.Stage=TEXT("clear_all_target_recheck");AllStable=W.Writable(W.BeforeWorld,W.BeforeWorldIdentity);
        for(auto& T:Targets) { if(T.Can&&(!Live(T.Package.Get())||T.Package->IsDirty()!=T.ActualBefore)) { AllStable=false;W.Fail(TEXT("target_changed"),TEXT("A package lifetime or dirty flag changed before the first clear")); } }
    }
    W.Checks->SetBoolField(TEXT("all_target_recheck_passed"),!Attempt||AllStable);
    if(Attempt&&AllStable) { for(auto& T:Targets)
    {
        if(!T.Can) { continue; }W.Result.Stage=TEXT("clear_dirty_flag");if(!W.Writable(W.BeforeWorld,W.BeforeWorldIdentity)||!Live(T.Package.Get())||T.Package->IsDirty()!=T.ActualBefore) { W.Fail(TEXT("target_changed"),TEXT("A package or session changed during dirty clearing; earlier flag changes remain"));break; }
        W.Write(TEXT("clear_dirty_flag"),TEXT("set_dirty_flag_calls"));T.Package->SetDirtyFlag(false);W.Count(TEXT("set_dirty_flag_returns"));T.Called=true;++Called;Cleared.Add(T.Name);
        T.AfterKnown=!Session().ShuttingDown&&Live(T.Package.Get());if(T.AfterKnown) { T.ActualAfter=T.Package->IsDirty();if(T.ActualBefore!=T.ActualAfter) { W.Result.bChanged=true; }if(!T.ActualAfter) { ++Verified; } }
        if(!T.AfterKnown||T.ActualAfter) { W.Fail(TEXT("dirty_clear_postcondition_failed"),TEXT("SetDirtyFlag returned but the package is unavailable or remains dirty; no retry was attempted"),false);break; }
    } }
    Verified=0;TArray<TSharedPtr<FJsonValue>> TargetValues;for(auto& T:Targets)
    {
        T.AfterKnown=!Session().ShuttingDown&&Live(T.Package.Get());if(T.AfterKnown) { T.ActualAfter=T.Package->IsDirty();if(T.Called&&!T.ActualAfter) { ++Verified; } }
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_package_path"),T.Requested);J->SetStringField(TEXT("package_name"),T.Name);J->SetBoolField(TEXT("was_dirty"),T.WasDirty);J->SetBoolField(TEXT("package_loaded"),T.Loaded);J->SetBoolField(TEXT("target_under_required_root"),T.Under);J->SetBoolField(TEXT("can_clear"),T.Can);J->SetBoolField(TEXT("cleared"),T.Called);J->SetArrayField(TEXT("blocked_reasons"),Strings(T.Blocked));if(!T.WasDirty&&T.Blocked.IsEmpty()) { J->SetStringField(TEXT("no_action_reason"),TEXT("package_was_not_dirty")); }
        J->SetBoolField(TEXT("actual_dirty_before_known"),T.Loaded);if(T.Loaded) { J->SetBoolField(TEXT("actual_dirty_before"),T.ActualBefore); }J->SetBoolField(TEXT("actual_dirty_after_known"),T.AfterKnown);if(T.AfterKnown) { J->SetBoolField(TEXT("actual_dirty_after"),T.ActualAfter); }TargetValues.Add(MakeShared<FJsonValueObject>(J));
    }
    auto L=W.Legacy;L->SetBoolField(TEXT("dry_run"),Dry);L->SetBoolField(TEXT("clear_all_dirty"),All);L->SetBoolField(TEXT("allow_non_temp_path"),NonTemp);L->SetStringField(TEXT("required_root"),Root);L->SetBoolField(TEXT("runtime_safe_editor_only"),true);L->SetBoolField(TEXT("loads_or_saves_packages"),false);L->SetBoolField(TEXT("reloads_packages"),false);L->SetBoolField(TEXT("can_clear"),Global.IsEmpty()&&Blocked==0&&Clearable>0);L->SetBoolField(TEXT("clear_attempted"),Called>0);L->SetNumberField(TEXT("target_count"),Targets.Num());L->SetNumberField(TEXT("blocked_target_count"),Blocked);L->SetNumberField(TEXT("clearable_target_count"),Clearable);L->SetNumberField(TEXT("cleared_count"),Called);L->SetArrayField(TEXT("cleared_packages"),Strings(Cleared));L->SetArrayField(TEXT("blocked_reasons"),Strings(Global));L->SetArrayField(TEXT("targets"),TargetValues);
    L->SetStringField(TEXT("message"),Dry?TEXT("Dry run only; no dirty package flags were changed"):(!Global.IsEmpty()||Blocked>0?TEXT("Dirty package flag clear blocked"):(Called==0?TEXT("No matching dirty package flags needed clearing"):TEXT("Dirty package flags cleared without loading, saving, or reloading packages"))));
    if(Called!=Verified&&W.Result.Error.IsEmpty()) { W.Fail(TEXT("final_dirty_clear_mismatch"),TEXT("At least one called package was unavailable or dirty at the final target observation; no retry was attempted"),false); }
    if(!W.Result.Error.IsEmpty()) { L->SetStringField(TEXT("message"),TEXT("Dirty package flag clearing stopped; inspect actual targets and failed_stage for any retained prefix")); }
    W.Prefix->SetBoolField(TEXT("clear_admitted_before_health_recheck"),Attempt);W.Prefix->SetArrayField(TEXT("targets"),TargetValues);W.Prefix->SetNumberField(TEXT("legacy_set_dirty_call_count"),Called);W.Prefix->SetNumberField(TEXT("verified_cleared_count"),Verified);W.Delta();W.Current();W.Checks->SetBoolField(TEXT("all_called_flags_observed_clear"),Called==Verified);return W.Finish(Called==Verified);
}

Variable::FResult NewPreview(const FString& ParamsJson)
{
    FWork W(TEXT("safe_new_preview_map"));if(!W.Request(ParamsJson)||!W.Enter()) { return W.Finish(false); }TGuardValue<bool> RunningGuard(Running,true);
    FString Requested;if(!W.Params->TryGetStringField(TEXT("target_path"),Requested)&&!W.Params->TryGetStringField(TEXT("map_path"),Requested)&&!W.Params->TryGetStringField(TEXT("level_path"),Requested)) { W.Fail(TEXT("target_path_missing"),TEXT("Missing 'target_path' parameter"));return W.Finish(false); }
    bool Dry=true,AllowDirty=false,Overwrite=false,NonTemp=false,Partitioned=false;W.Params->TryGetBoolField(TEXT("dry_run"),Dry);W.Params->TryGetBoolField(TEXT("allow_dirty_packages"),AllowDirty);W.Params->TryGetBoolField(TEXT("overwrite_existing"),Overwrite);W.Params->TryGetBoolField(TEXT("allow_non_temp_path"),NonTemp);W.Params->TryGetBoolField(TEXT("is_partitioned_world"),Partitioned);
    FString Root=TEXT("/Game/_MCP_Temp");W.Params->TryGetStringField(TEXT("required_root"),Root);Root=NormalizeRoot(Root,false);const FString Target=NormalizeLevel(Requested);FText Reason;if(!ValidPackage(Target,Reason)) { W.Fail(TEXT("target_path_invalid"),FString::Printf(TEXT("Invalid preview map package path '%s': %s"),*Requested,*Reason.ToString()));return W.Finish(false); }
    W.Result.bSaveRequested=!Dry;const FString Filename=FPackageName::LongPackageNameToFilename(Target,FPackageName::GetMapPackageExtension());const bool PackageExists=FPackageName::DoesPackageExist(Target),FileExists=FPaths::FileExists(Filename),Exists=PackageExists||FileExists;const bool Under=Root.IsEmpty()||Target.StartsWith(Root+TEXT("/"));W.Start();TArray<FString> Reasons;
    if(!NonTemp&&!Under) { Reasons.Add(TEXT("target_outside_required_root")); }if(Exists&&!Overwrite) { Reasons.Add(TEXT("target_map_already_exists")); }if(!AllowDirty&&!W.Before.IsEmpty()) { Reasons.Add(TEXT("dirty_packages_present")); }if(!Live(GEditor)) { Reasons.Add(TEXT("editor_unavailable")); }if(IsPlaySessionActive()) { Reasons.Add(TEXT("pie_or_sie_active")); }if(Session().Pending) { Reasons.Add(TEXT("editor_level_open_already_pending")); }
    auto L=W.Legacy;L->SetStringField(TEXT("requested_map_path"),Requested);L->SetStringField(TEXT("target_long_package_name"),Target);L->SetStringField(TEXT("target_filename"),Filename);L->SetStringField(TEXT("required_root"),Root);L->SetBoolField(TEXT("target_exists"),Exists);L->SetBoolField(TEXT("target_package_exists"),PackageExists);L->SetBoolField(TEXT("target_file_exists"),FileExists);L->SetBoolField(TEXT("overwrite_existing"),Overwrite);L->SetBoolField(TEXT("allow_non_temp_path"),NonTemp);L->SetBoolField(TEXT("target_under_required_root"),Under);L->SetStringField(TEXT("current_world_package_name"),W.BeforeWorld);L->SetBoolField(TEXT("dry_run"),Dry);L->SetBoolField(TEXT("allow_dirty_packages"),AllowDirty);L->SetBoolField(TEXT("is_partitioned_world"),Partitioned);L->SetBoolField(TEXT("can_create"),Reasons.IsEmpty());L->SetArrayField(TEXT("blocked_reasons"),Strings(Reasons));L->SetNumberField(TEXT("dirty_package_count_before"),W.Before.Num());L->SetArrayField(TEXT("dirty_packages_before"),Strings(W.Before));L->SetBoolField(TEXT("create_attempted"),false);L->SetBoolField(TEXT("created"),false);L->SetBoolField(TEXT("saved"),false);
    W.Prefix->SetStringField(TEXT("target_filename"),Filename);W.Prefix->SetBoolField(TEXT("target_file_existed_before"),FileExists);W.Result.Data->SetBoolField(TEXT("allow_dirty_transition_explicit"),AllowDirty);W.Result.Data->SetStringField(TEXT("dirty_preservation_limit"),TEXT("Default admission refuses all dirty packages. Explicit allow_dirty_packages preserves the legacy transition override; SDK unloading can discard an unsaved world. No automatic save, flag clearing or recovery is performed."));
    if(Dry||!Reasons.IsEmpty()) { L->SetStringField(TEXT("message"),Dry?TEXT("Dry run only; no new preview map was created"):TEXT("New preview map creation blocked"));W.Checks->SetBoolField(TEXT("new_map_not_attempted"),true);return W.Finish(); }
    W.Result.Stage=TEXT("new_map_recheck");if(!W.Writable(W.BeforeWorld,W.BeforeWorldIdentity)) { return W.Finish(false); }const auto CurrentDirty=W.Dirty();if(!AllowDirty&&!CurrentDirty.IsEmpty()) { W.Fail(TEXT("dirty_changed"),TEXT("Dirty packages appeared before new-map creation"));return W.Finish(false); }
    const FString Directory=FPaths::GetPath(Filename);const bool DirectoryBefore=IFileManager::Get().DirectoryExists(*Directory);W.Prefix->SetStringField(TEXT("directory"),Directory);W.Prefix->SetBoolField(TEXT("directory_existed_before"),DirectoryBefore);
    W.Write(TEXT("create_directory"),TEXT("directory_create_calls"));const bool DirectoryReturned=IFileManager::Get().MakeDirectory(*Directory,true);W.Count(TEXT("directory_create_returns"));const bool DirectoryAfter=IFileManager::Get().DirectoryExists(*Directory);W.Prefix->SetBoolField(TEXT("directory_create_returned"),DirectoryReturned);W.Prefix->SetBoolField(TEXT("directory_exists_after"),DirectoryAfter);if(!DirectoryBefore&&DirectoryAfter) { W.Result.bChanged=true; }
    // Preserve the original attempt even if MakeDirectory returns false; report that prefix separately.
    W.Result.Stage=TEXT("new_map_recheck_after_directory");if(!W.Writable(W.BeforeWorld,W.BeforeWorldIdentity)) { W.Delta();W.Current();return W.Finish(false); }
    if(!AllowDirty&&!W.Dirty().IsEmpty()) { W.Fail(TEXT("dirty_changed"),TEXT("Dirty packages appeared after directory creation; NewMap was not attempted"));W.Delta();W.Current();return W.Finish(false); }
    TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript,true);W.Write(TEXT("new_map"),TEXT("new_map_calls"));L->SetBoolField(TEXT("create_attempted"),true);UWorld* NewWorld=GEditor->NewMap(Partitioned);W.Count(TEXT("new_map_returns"));L->SetBoolField(TEXT("created"),NewWorld!=nullptr);W.Prefix->SetBoolField(TEXT("new_world_returned"),NewWorld!=nullptr);
    if(Session().ShuttingDown) { W.Fail(TEXT("shutdown_after_new_map"),TEXT("Module shutdown began during NewMap; its return is preserved and no SaveMap or Engine observation follows"));return W.Finish(false); }
    TWeakObjectPtr<UWorld> Created=NewWorld;const bool CreatedLive=Live(Created.Get());const FString AfterNew=W.World();const uint32 AfterNewIdentity=W.LastWorldIdentity;W.Prefix->SetStringField(TEXT("world_after_new_map"),AfterNew);if(AfterNew!=W.BeforeWorld) { W.Result.bChanged=true; }
    if(CreatedLive) { W.Result.bChanged=true; }
    if(!CreatedLive) { L->SetStringField(TEXT("message"),TEXT("GEditor->NewMap returned null"));W.Fail(TEXT("new_map_failed"),TEXT("GEditor->NewMap returned null or a non-live world"));W.Delta();W.Current();return W.Finish(false); }
    W.Result.Stage=TEXT("save_map_admission");if(!W.Writable(AfterNew,AfterNewIdentity)||!Live(Created.Get())||GEditor->GetEditorWorldContext().World()!=Created.Get()) { W.Fail(TEXT("new_world_changed"),TEXT("New world or session changed before SaveMap; creation remains"));W.Delta();W.Current();return W.Finish(false); }
    if(!Overwrite&&(FPackageName::DoesPackageExist(Target)||FPaths::FileExists(Filename))) { W.Fail(TEXT("target_appeared_before_save"),TEXT("The target appeared after initial admission; SaveMap was not attempted and the new world remains"));W.Delta();W.Current();return W.Finish(false); }
    W.Result.bSaveAttempted=true;W.Write(TEXT("save_map"),TEXT("map_save_calls"));const bool Saved=UEditorLoadingAndSavingUtils::SaveMap(Created.Get(),Target);W.Count(TEXT("map_save_returns"));W.Result.bSaved=Saved;L->SetBoolField(TEXT("saved"),Saved);
    if(Session().ShuttingDown) { W.Fail(TEXT("shutdown_after_save_map"),TEXT("Module shutdown began during SaveMap; its return is preserved and no further Engine observations were made"));return W.Finish(false); }
    const FString After=W.World();L->SetStringField(TEXT("current_world_package_name_after"),After);W.Delta();W.Current();
    const bool FileAfter=FPaths::FileExists(Filename);W.Prefix->SetBoolField(TEXT("target_file_exists_after"),FileAfter);W.Prefix->SetBoolField(TEXT("save_map_returned"),Saved);W.Checks->SetBoolField(TEXT("map_save_returned_true"),Saved);W.Checks->SetBoolField(TEXT("target_file_exists_after"),FileAfter);W.Checks->SetBoolField(TEXT("current_world_is_target_after_save"),After==Target);W.Checks->SetBoolField(TEXT("directory_create_returned"),DirectoryReturned);
    L->SetStringField(TEXT("message"),Saved?TEXT("New preview map created and saved"):TEXT("UEditorLoadingAndSavingUtils::SaveMap returned false"));if(!Saved) { W.Fail(TEXT("save_map_failed"),TEXT("UEditorLoadingAndSavingUtils::SaveMap returned false")); }else if(!FileAfter||After!=Target) { W.Fail(TEXT("save_map_observation_mismatch"),TEXT("SaveMap returned true but target file/current-world postconditions did not match"),false); }
    return W.Finish(Saved&&FileAfter&&After==Target);
}

void Shutdown()
{
    // Module owner calls this on the game thread; do not wait for this callback from itself.
    FTSTicker::FDelegateHandle Handle;FString Id;TSharedPtr<FJsonObject> Last,Execution;bool CallbackActive=false;
    { FScopeLock Lock(&Deferred.Mutex);Deferred.ShuttingDown=true;Handle=Deferred.Handle;Deferred.Handle.Reset();CallbackActive=Deferred.InCallback;Id=Deferred.RequestId;Last=Deferred.LastStatus;Execution=Deferred.LastExecution;Deferred.Pending=false; }
    if(Handle.IsValid()&&!CallbackActive) { FTSTicker::RemoveTicker(Handle); }
    if(Last.IsValid()&&!CallbackActive)
    {
        auto Cancelled=Clone(Last.ToSharedRef());FString Status;Cancelled->TryGetStringField(TEXT("open_status"),Status);
        if(Status==TEXT("scheduled"))
        {
            Cancelled->SetBoolField(TEXT("success"),false);Cancelled->SetBoolField(TEXT("load_scheduled"),false);Cancelled->SetStringField(TEXT("open_status"),TEXT("cancelled"));Cancelled->SetStringField(TEXT("message"),TEXT("Owned deferred Editor load cancelled during module shutdown; LoadMap was not attempted"));
            auto Receipt=Execution.IsValid()?Clone(Execution.ToSharedRef()):MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("request_id"),Id);Receipt->SetBoolField(TEXT("success"),false);Receipt->SetBoolField(TEXT("postcondition_passed"),false);Receipt->SetBoolField(TEXT("operation_complete"),true);Receipt->SetStringField(TEXT("stage"),TEXT("module_shutdown"));Receipt->SetStringField(TEXT("failed_stage"),TEXT("module_shutdown"));Receipt->SetStringField(TEXT("error_code"),TEXT("editor_level_session_cancelled_shutdown"));Receipt->SetStringField(TEXT("error"),TEXT("Owned deferred load was cancelled before LoadMap"));Receipt->SetBoolField(TEXT("automatic_retry_permitted"),false);
            FScopeLock Lock(&Deferred.Mutex);Deferred.LastStatus=Cancelled;Deferred.LastExecution=Receipt;
        }
    }
}
} // namespace Cubeless::UE58::EditorLevelSession
