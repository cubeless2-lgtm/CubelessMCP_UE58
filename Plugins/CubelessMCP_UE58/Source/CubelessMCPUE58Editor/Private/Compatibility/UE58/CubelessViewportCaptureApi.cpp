// Four dedicated Editor viewport APIs. Parameters are JSON objects, never commands.
#include "Compatibility/UE58/CubelessViewportCaptureApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Bookmarks/IBookmarkTypeTools.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "LevelEditorViewport.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/PointerVariants.h"
#include "UnrealClient.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"
#include "ViewportClient.h"
#include "CoreGlobals.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::ViewportCapture
{
namespace
{
using Variable::FResult;
constexpr int32 MaxJsonChars=1024*1024,MaxBookmarks=65536,MaxClients=65536,MaxDimension=16384;
constexpr int64 MaxPixels=64LL*1024*1024;
bool Running=false;
bool Live(const UObject* O)
{ return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeEditorGlobals()
{ return IsEditorGameThread()&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage&&Live(GEditor); }
bool SafeEditorContext()
{ if(!SafeEditorGlobals())return false;auto* W=GEditor->GetEditorWorldContext().World();return !W||(Live(W)&&!W->bIsTearingDown); }
bool SafeText(const FString& S,int32 Maximum)
{ if(S.Len()>Maximum) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }return true; }
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& K) { return FCrc::StrCrc32(*K); }
};
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Object)
{
    if(Text.IsEmpty()||!SafeText(Text,MaxJsonChars)) { return false; }
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>262144) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last())
        { const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }
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
TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
TArray<TSharedPtr<FJsonValue>> RotationJson(const FRotator& R)
{ return {MakeShared<FJsonValueNumber>(R.Pitch),MakeShared<FJsonValueNumber>(R.Yaw),MakeShared<FJsonValueNumber>(R.Roll)}; }
bool Finite(const FVector& V) { return FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::IsFinite(V.Z); }
bool Finite(const FRotator& R) { return FMath::IsFinite(R.Pitch)&&FMath::IsFinite(R.Yaw)&&FMath::IsFinite(R.Roll); }
FString Json(const TSharedRef<FJsonObject>& O)
{ FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S; }
TArray<FString> DirtyPackages()
{
    TArray<UPackage*> Packages;FEditorFileUtils::GetDirtyPackages(Packages);TArray<FString> Names;
    for(UPackage* P:Packages) { if(Live(P)) { Names.AddUnique(P->GetName()); } }Names.Sort();return Names;
}
struct FWork
{
    FResult Result;FString Command,FailedStage;bool Observer=false,SparseLegacy=false;
    TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Params,InitialView;FViewport* Viewport=nullptr;FViewportClient* Client=nullptr;FEditorViewportClient* EditorClient=nullptr;FLevelEditorViewportClient* LevelClient=nullptr;
    TStrongPtrVariant<FViewportClient> ClientPin;TWeakObjectPtr<UEditorEngine> EditorOwner;TWeakObjectPtr<UWorld> World;TUniquePtr<FScopedTransaction> Transaction;
    bool Captured=false,HasWorld=false,DirtyBeforeCaptured=false,ForbidPIE=false,ContextLost=false;TArray<FString> DirtyBefore;
    bool CameraApplied=false,CleanApplied=false,RestoreCamera=true,CameraRestoreAttempted=false,CleanRestoreAttempted=false,CameraRestored=false,CleanRestored=false;
    FVector PreviousLocation=FVector::ZeroVector;FRotator PreviousRotation=FRotator::ZeroRotator;float PreviousFov=0,PreviousFovAngle=0;
    bool PreviousGameView=false,PreviousOutline=false,PreviousWidgets=false;
    FString FilePath,Directory;bool FileTracked=false,FileExistedBefore=false,PngWriteReturned=false,DirectoryTracked=false,DirectoryExisted=false;int64 FileSizeBefore=-1;
    FWork(const TCHAR* Name,bool IsObserver=false,bool Sparse=false):Command(Name),Observer(IsObserver),SparseLegacy(Sparse)
    {
        Result.Compiled=false;Legacy->SetBoolField(TEXT("success"),false);
        Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Calls);Result.Data->SetObjectField(TEXT("postconditions"),Checks);Result.Data->SetObjectField(TEXT("actual_partial_state"),Prefix);
        for(const TCHAR* K:{TEXT("atomic_rollback_available"),TEXT("automatic_retry_permitted"),TEXT("explicit_compile_attempted"),TEXT("explicit_save_attempted"),TEXT("disk_reload_verified"),TEXT("PNG_file_write_is_asset_save"),TEXT("PNG_file_readback_verified"),TEXT("wrapper_explicit_wait_performed"),TEXT("complete_filesystem_side_effect_inventory")}) { Result.Data->SetBoolField(K,false); }
        Result.Data->SetStringField(TEXT("scope"),TEXT("Captured active rendered viewport/client and observed UI/file/dirty projections only. No UI clicks, map open/save, actor/asset writes, PIE start or whole-engine rollback."));
        Result.Data->SetStringField(TEXT("global_change_scope"),TEXT("Draw/readback, custom bookmark actions, Engine callbacks/caches and filesystem effects are not a complete global mutation inventory. Direct UI/file results are observed separately; unknown never authorizes retry."));
        Result.Data->SetStringField(TEXT("SDK_synchronous_limit"),TEXT("Original bounded same-call Draw/ReadPixels remains. Public ReadPixels internally FlushRenderingCommands; PNG compression and file save are synchronous. No additional wrapper waits/flushes or worker UObject calls."));
        Result.Data->SetStringField(TEXT("retry_risk"),TEXT("Bookmark jump, camera/review override, directory creation or PNG overwrite may remain before failure. Restore only attempts original temporary state once on the captured surface; it is not atomic rollback."));
        auto Limits=MakeShared<FJsonObject>();Limits->SetNumberField(TEXT("JSON_UTF16_characters"),MaxJsonChars);Limits->SetNumberField(TEXT("JSON_depth"),16);Limits->SetNumberField(TEXT("JSON_slots"),262144);Limits->SetNumberField(TEXT("bookmark_slots"),MaxBookmarks);Limits->SetNumberField(TEXT("viewport_clients"),MaxClients);Limits->SetNumberField(TEXT("PNG_dimension"),MaxDimension);Limits->SetNumberField(TEXT("PNG_pixels"),static_cast<double>(MaxPixels));Result.Data->SetObjectField(TEXT("limits"),Limits);
        for(const TCHAR* K:{TEXT("active_viewport_queries"),TEXT("dirty_snapshot_calls"),TEXT("begin_transaction_calls"),TEXT("begin_transaction_returns"),TEXT("end_transaction_calls"),TEXT("end_transaction_returns"),TEXT("bookmark_tools_get_calls"),TEXT("bookmark_tools_get_returns"),TEXT("bookmark_max_calls"),TEXT("bookmark_max_returns"),TEXT("bookmark_check_calls"),TEXT("bookmark_check_returns"),TEXT("bookmark_jump_calls"),TEXT("bookmark_jump_returns"),TEXT("remove_realtime_override_calls"),TEXT("remove_realtime_override_returns"),TEXT("add_realtime_override_calls"),TEXT("add_realtime_override_returns"),TEXT("request_realtime_frames_calls"),TEXT("request_realtime_frames_returns"),TEXT("set_view_location_calls"),TEXT("set_view_location_returns"),TEXT("set_view_rotation_calls"),TEXT("set_view_rotation_returns"),TEXT("FOV_field_writes"),TEXT("set_game_view_calls"),TEXT("set_game_view_returns"),TEXT("show_flag_writes"),TEXT("client_invalidate_calls"),TEXT("client_invalidate_returns"),TEXT("viewport_invalidate_calls"),TEXT("viewport_invalidate_returns"),TEXT("viewport_invalidate_display_calls"),TEXT("viewport_invalidate_display_returns"),TEXT("viewport_draw_calls"),TEXT("viewport_draw_returns"),TEXT("read_pixels_calls"),TEXT("read_pixels_returns"),TEXT("PNG_compress_calls"),TEXT("PNG_compress_returns"),TEXT("directory_create_calls"),TEXT("directory_create_returns"),TEXT("PNG_save_calls"),TEXT("PNG_save_returns"),TEXT("compile_blueprint_calls"),TEXT("asset_save_calls"),TEXT("map_save_calls"),TEXT("wrapper_wait_calls"),TEXT("start_pie_calls"),TEXT("stop_pie_calls")}) { Calls->SetNumberField(K,0); }
    }
    void Count(const TCHAR* K) { double N=0;Calls->TryGetNumberField(K,N);Calls->SetNumberField(K,N+1); }
    bool Fail(const TCHAR* Code,const FString& Error)
    {
        if(Result.Error.IsEmpty()) { Result.ErrorCode=FString(TEXT("viewport_capture_"))+Code;Result.Error=Error;FailedStage=Result.Stage; }
        Result.bSuccess=false;Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),Result.Error);return false;
    }
    bool Lose(const TCHAR* Code,const FString& Error)
    { ContextLost=true;Result.bChangeStateKnown=false;Prefix->SetBoolField(TEXT("context_lost_followups_skipped"),true);return Fail(Code,Error); }
    bool Request(const FString& Text)
    {
        Result.Stage=TEXT("parse_params");if(!Parse(Text,Params)) { return Fail(TEXT("params_invalid"),TEXT("This dedicated viewport API requires one bounded JSON params object")); }
        Result.Data->SetStringField(TEXT("params_json"),Text);Result.Data->SetObjectField(TEXT("params"),Params.ToSharedRef());return true;
    }
    void DirtyStart()
    { if(SafeEditorContext()&&(!Captured||Admit())&&!DirtyBeforeCaptured) { Count(TEXT("dirty_snapshot_calls"));DirtyBefore=DirtyPackages();DirtyBeforeCaptured=true;Result.Data->SetArrayField(TEXT("dirty_packages_before"),Strings(DirtyBefore)); } }
    bool Initialize(bool RequireEditor,bool DenyPIE)
    {
        Result.Stage=TEXT("capture_active_viewport");
        if(!SafeEditorContext()) { return Fail(TEXT("editor_required"),TEXT("Requires live Editor game thread outside GC/exit/save/teardown")); }EditorOwner=GEditor;ForbidPIE=DenyPIE;
        if(DenyPIE&&IsPlaySessionActive()) { return Fail(TEXT("PIE_disabled"),Command+TEXT(" is disabled during PIE/SIE; stop PIE before capturing the active editor viewport")); }
        Count(TEXT("active_viewport_queries"));Viewport=GEditor->GetActiveViewport();if(!Viewport) { return Fail(TEXT("viewport_missing"),TEXT("No active editor viewport is available")); }
        ClientPin=Viewport->GetClientPtr();Client=Viewport->GetClient();if(!Client) { return Fail(TEXT("client_missing"),TEXT("Active viewport does not have an editor viewport client")); }
        if(GEditor->GetAllViewportClients().Num()>MaxClients||GEditor->GetLevelViewportClients().Num()>MaxClients) { return Fail(TEXT("client_limit"),TEXT("Editor viewport-client registry exceeds its bound")); }
        for(FEditorViewportClient* C:GEditor->GetAllViewportClients()) { if(C&&static_cast<FViewportClient*>(C)==Client) { EditorClient=C;break; } }
        if(RequireEditor&&!EditorClient) { return Fail(TEXT("client_type_unsupported"),TEXT("Active client is not present in the public Editor viewport-client registry")); }
        if(!EditorClient&&!ClientPin.IsValid()) { return Fail(TEXT("client_lifetime_unsupported"),TEXT("Non-editor raw viewport client has no public strong lifetime handle")); }
        if(EditorClient)
        {
            Result.bChangeStateKnown=false;Result.Data->SetBoolField(TEXT("viewport_SDK_global_purity_unverified"),true);
            UWorld* W=EditorClient->GetWorld();if(W&&!Live(W)) { return Fail(TEXT("world_unresolved"),TEXT("Viewport world is not live")); }World=W;HasWorld=W!=nullptr;
            for(FLevelEditorViewportClient* C:GEditor->GetLevelViewportClients()) { if(C&&static_cast<FEditorViewportClient*>(C)==EditorClient&&C->Viewport==Viewport) { LevelClient=C;break; } }
        }
        Captured=true;Result.bChangeStateKnown=false;Result.Data->SetBoolField(TEXT("viewport_SDK_global_purity_unverified"),true);DirtyStart();if(!Admit()) { return false; }InitialView=View();Result.Data->SetObjectField(TEXT("viewport_before"),InitialView.ToSharedRef());Result.Data->SetBoolField(TEXT("client_strong_handle_present"),ClientPin.IsValid());return true;
    }
    bool Admit()
    {
        if(ContextLost) { return false; }
        if(!Captured) { return Lose(TEXT("viewport_not_captured"),TEXT("No viewport/client identity was captured")); }
        if(!SafeEditorContext()||GEditor!=EditorOwner.Get()) { return Lose(TEXT("editor_changed"),TEXT("Editor context was lost; no further viewport/file action was dispatched")); }
        if(ForbidPIE&&IsPlaySessionActive()) { return Lose(TEXT("PIE_started"),TEXT("PIE/SIE began during capture; no further viewport/file action was dispatched")); }
        Count(TEXT("active_viewport_queries"));FViewport* Current=GEditor->GetActiveViewport();
        if(!Current||Current!=Viewport||Current->GetClient()!=Client) { return Lose(TEXT("viewport_changed"),TEXT("Captured active viewport/client changed; no stale pointer action was dispatched")); }
        if(ClientPin.IsValid()&&ClientPin.Get()!=Client) { return Lose(TEXT("client_changed"),TEXT("Captured strong viewport client differs")); }
        if(EditorClient)
        {
            if(!GEditor->GetAllViewportClients().Contains(EditorClient)) { return Lose(TEXT("client_destroyed"),TEXT("Captured editor client left the public registry")); }
            UWorld* W=EditorClient->GetWorld();if((HasWorld&&(!Live(World.Get())||World->bIsTearingDown||W!=World.Get()))||(!HasWorld&&W)) { return Lose(TEXT("world_changed"),TEXT("Viewport world changed during an SDK callback")); }
        }
        if(LevelClient&&(!GEditor->GetLevelViewportClients().Contains(LevelClient)||LevelClient->Viewport!=Viewport)) { return Lose(TEXT("level_client_changed"),TEXT("Level viewport client left its captured surface")); }return true;
    }
    TSharedRef<FJsonObject> View()
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("has_editor_client"),EditorClient!=nullptr);J->SetBoolField(TEXT("has_exact_level_client"),LevelClient!=nullptr);
        if(EditorClient)
        {
            const FVector L=EditorClient->GetViewLocation();const FRotator R=EditorClient->GetViewRotation();
            if(Finite(L)) { J->SetArrayField(TEXT("view_location"),VectorJson(L)); }else { J->SetField(TEXT("view_location"),MakeShared<FJsonValueNull>()); }
            if(Finite(R)) { J->SetArrayField(TEXT("view_rotation"),RotationJson(R)); }else { J->SetField(TEXT("view_rotation"),MakeShared<FJsonValueNull>()); }
            for(const auto& P:{TPair<const TCHAR*,float>(TEXT("view_fov"),EditorClient->ViewFOV),TPair<const TCHAR*,float>(TEXT("fov_angle"),EditorClient->FOVAngle)})
            { if(FMath::IsFinite(P.Value)) { J->SetNumberField(P.Key,P.Value); }else { J->SetField(P.Key,MakeShared<FJsonValueNull>()); } }
            J->SetBoolField(TEXT("is_perspective"),EditorClient->IsPerspective());J->SetBoolField(TEXT("is_realtime"),EditorClient->IsRealtime());J->SetBoolField(TEXT("is_realtime_override_set"),EditorClient->IsRealtimeOverrideSet());J->SetStringField(TEXT("realtime_override_message"),EditorClient->GetRealtimeOverrideMessage().BuildSourceString());J->SetBoolField(TEXT("has_review_override"),EditorClient->HasRealtimeOverride(FText::FromString(TEXT("MCPWeather Viewport Review"))));
            J->SetBoolField(TEXT("game_view"),EditorClient->IsInGameView());J->SetBoolField(TEXT("selection_outline"),EditorClient->EngineShowFlags.SelectionOutline);J->SetBoolField(TEXT("mode_widgets"),EditorClient->EngineShowFlags.ModeWidgets);
        }
        J->SetNumberField(TEXT("width"),Viewport->GetSizeXY().X);J->SetNumberField(TEXT("height"),Viewport->GetSizeXY().Y);J->SetStringField(TEXT("world"),Live(World.Get())?World->GetPathName():FString());return J;
    }
    void Observe(const TCHAR* Key)
    { if(Admit()) { const auto Current=View();Prefix->SetObjectField(Key,Current);if(InitialView.IsValid()&&Json(InitialView.ToSharedRef())!=Json(Current)) { Result.bChanged=true; } } }
    bool Write(const TCHAR* Stage,const TCHAR* Counter)
    { Result.Stage=Stage;if(!Admit()) { return false; }Result.bMutationAttempted=true;Result.bChangeStateKnown=false;Count(Counter);return true; }
    bool Begin()
    { if(!Write(TEXT("begin_transaction"),TEXT("begin_transaction_calls"))) { return false; }Transaction=MakeUnique<FScopedTransaction>(FText::FromString(TEXT("Cubeless ")+Command));Count(TEXT("begin_transaction_returns"));return Admit(); }
    void Close()
    {
        if(!Transaction) { return; }
        const bool Outstanding=Transaction->IsOutstanding();
        Prefix->SetBoolField(TEXT("transaction_close_is_required_scope_release"),true);
        Prefix->SetBoolField(TEXT("transaction_scope_outstanding_before_release"),Outstanding);
        if(Outstanding&&(!SafeEditorGlobals()||GEditor!=EditorOwner.Get()))
        {
            // Its destructor would call a missing/unsafe global Editor. Do not cancel another scope,
            // blindly EndTransaction later, or let an automatic destructor perform that SDK call.
            (void)Transaction.Release();
            Prefix->SetBoolField(TEXT("orphan_transaction_scope_abandoned_unsafe_editor"),true);
            Prefix->SetBoolField(TEXT("transaction_scope_release_unverified"),true);
            Result.bChangeStateKnown=false;ContextLost=true;
            Fail(TEXT("transaction_release_unsafe"),TEXT("Captured Editor lost or unsafe; exact outstanding scope abandoned without End/Cancel/Undo. Scope and allocation cleanup are unresolved; no replay."));
            return;
        }
        Result.Stage=TEXT("end_transaction");Result.bChangeStateKnown=false;
        if(Outstanding) { Count(TEXT("end_transaction_calls")); }
        Transaction.Reset();
        if(Outstanding) { Count(TEXT("end_transaction_returns")); }
        else { Count(TEXT("nonoutstanding_scope_destructor_no_Engine_call")); }
    }
    bool Invalidate(bool Display=false)
    {
        if(EditorClient) { if(!Write(TEXT("invalidate_client"),TEXT("client_invalidate_calls"))) { return false; }EditorClient->Invalidate();Count(TEXT("client_invalidate_returns")); }
        if(!Write(Display?TEXT("invalidate_display"):TEXT("invalidate_viewport"),Display?TEXT("viewport_invalidate_display_calls"):TEXT("viewport_invalidate_calls"))) { return false; }
        if(Display) { Viewport->InvalidateDisplay();Count(TEXT("viewport_invalidate_display_returns")); }else { Viewport->Invalidate();Count(TEXT("viewport_invalidate_returns")); }return Admit();
    }
    bool Draw()
    { if(!Write(TEXT("draw_viewport"),TEXT("viewport_draw_calls"))) { return false; }Viewport->Draw(false);Count(TEXT("viewport_draw_returns"));return Admit(); }
    void TrackFile(const FString& Name)
    {
        FilePath=Name;FileTracked=true;FileExistedBefore=IFileManager::Get().FileExists(*FilePath);FileSizeBefore=IFileManager::Get().FileSize(*FilePath);
        Prefix->SetStringField(TEXT("filepath"),FilePath);Prefix->SetStringField(TEXT("resolved_filepath"),FPaths::ConvertRelativePathToFull(FilePath));Prefix->SetBoolField(TEXT("file_existed_before"),FileExistedBefore);Prefix->SetNumberField(TEXT("file_size_before"),static_cast<double>(FileSizeBefore));
    }
    bool CleanMatches() const
    { return LevelClient&&LevelClient->IsInGameView()==PreviousGameView&&bool(LevelClient->EngineShowFlags.SelectionOutline)==PreviousOutline&&bool(LevelClient->EngineShowFlags.ModeWidgets)==PreviousWidgets; }
    bool CameraMatches() const
    { return EditorClient&&EditorClient->GetViewLocation().Equals(PreviousLocation,1e-6)&&EditorClient->GetViewRotation().Equals(PreviousRotation,1e-6)&&EditorClient->ViewFOV==PreviousFov&&EditorClient->FOVAngle==PreviousFovAngle; }
    bool Restore()
    {
        if(CleanApplied&&!CleanRestoreAttempted)
        {
            CleanRestoreAttempted=true;Prefix->SetBoolField(TEXT("clean_restore_attempted"),true);
            bool Okay=Admit()&&LevelClient;
            if(Okay&&Write(TEXT("restore_clean_flags"),TEXT("show_flag_writes"))) { LevelClient->EngineShowFlags.SetModeWidgets(PreviousWidgets);Count(TEXT("show_flag_writes"));LevelClient->EngineShowFlags.SetSelectionOutline(PreviousOutline); }else { Okay=false; }
            if(Okay&&Write(TEXT("restore_game_view"),TEXT("set_game_view_calls"))) { LevelClient->SetGameView(PreviousGameView);Count(TEXT("set_game_view_returns")); }else { Okay=false; }
            if(Okay) { Okay=Invalidate(); }if(Okay) { CleanRestored=CleanMatches();if(!CleanRestored) { Fail(TEXT("clean_restore_mismatch"),TEXT("Capture clean flags did not restore; UI changes may remain")); } }
        }
        if(CameraApplied&&RestoreCamera&&!CameraRestoreAttempted)
        {
            CameraRestoreAttempted=true;Prefix->SetBoolField(TEXT("camera_restore_attempted"),true);bool Okay=Admit()&&EditorClient;
            if(Okay&&Write(TEXT("restore_view_location"),TEXT("set_view_location_calls"))) { EditorClient->SetViewLocation(PreviousLocation);Count(TEXT("set_view_location_returns")); }else { Okay=false; }
            if(Okay&&Write(TEXT("restore_view_rotation"),TEXT("set_view_rotation_calls"))) { EditorClient->SetViewRotation(PreviousRotation);Count(TEXT("set_view_rotation_returns")); }else { Okay=false; }
            if(Okay&&Write(TEXT("restore_FOV"),TEXT("FOV_field_writes"))) { EditorClient->FOVAngle=PreviousFovAngle;Count(TEXT("FOV_field_writes"));EditorClient->ViewFOV=PreviousFov; }else { Okay=false; }
            if(Okay) { Okay=Invalidate(); }if(Okay) { CameraRestored=CameraMatches();if(!CameraRestored) { Fail(TEXT("camera_restore_mismatch"),TEXT("Temporary camera did not restore to its post-bookmark state; UI changes may remain")); } }
        }
        Prefix->SetBoolField(TEXT("camera_override_dispatched"),CameraApplied);Prefix->SetBoolField(TEXT("clean_game_view_dispatched"),CleanApplied);
        Prefix->SetBoolField(TEXT("clean_state_restored"),!CleanApplied||CleanRestored);Prefix->SetBoolField(TEXT("temporary_camera_restored"),CameraApplied&&RestoreCamera&&CameraRestored);
        return (!CleanApplied||CleanRestored)&&(!CameraApplied||!RestoreCamera||CameraRestored);
    }
    FResult Finish()
    {
        if(Captured) { Restore(); }Close();if(Result.Error.IsEmpty()) { Result.Stage=TEXT("final_state_observation"); }const bool Valid=!Captured||Admit();
        if(Captured&&Valid)
        {
            const auto After=View();Result.Data->SetObjectField(TEXT("viewport_after"),After);const bool Diff=InitialView.IsValid()&&Json(InitialView.ToSharedRef())!=Json(After);Result.Data->SetBoolField(TEXT("observed_final_viewport_projection_changed"),Diff);Result.bChanged=Result.bChanged||Diff;
            if(CleanApplied&&CleanRestoreAttempted) { CleanRestored=CleanMatches(); }if(CameraApplied&&RestoreCamera&&CameraRestoreAttempted) { CameraRestored=CameraMatches(); }
            if(Observer&&Diff) { Fail(TEXT("observer_state_changed"),TEXT("Viewport state changed during bookmark observation; global state is uncertain")); }
        }
        if(DirtyBeforeCaptured&&Valid&&SafeEditorContext()&&!ContextLost)
        {
            Count(TEXT("dirty_snapshot_calls"));const auto After=DirtyPackages();TArray<FString> Added,Removed;for(const FString& P:After) { if(!DirtyBefore.Contains(P)) { Added.Add(P); } }for(const FString& P:DirtyBefore) { if(!After.Contains(P)) { Removed.Add(P); } }
            auto D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("dirty_package_count_before"),DirtyBefore.Num());D->SetArrayField(TEXT("dirty_packages_before"),Strings(DirtyBefore));D->SetNumberField(TEXT("dirty_package_count_after"),After.Num());D->SetArrayField(TEXT("dirty_packages_after"),Strings(After));D->SetNumberField(TEXT("dirty_package_added_count"),Added.Num());D->SetArrayField(TEXT("dirty_packages_added_by_command"),Strings(Added));D->SetNumberField(TEXT("dirty_package_removed_count"),Removed.Num());D->SetArrayField(TEXT("dirty_packages_removed_by_command"),Strings(Removed));Result.Data->SetObjectField(TEXT("dirty_package_delta"),D);
            if(!Added.IsEmpty()||!Removed.IsEmpty()) { Result.bChanged=true;if(Observer) { Fail(TEXT("observer_dirty_changed"),TEXT("Dirty-package set changed during bookmark observation; global state is uncertain")); } }
            if(Result.bSuccess&&Command==TEXT("capture_viewport_bookmark_screenshot")) { for(const auto& P:D->Values) { Legacy->SetField(P.Key,P.Value); }Legacy->SetNumberField(TEXT("dirty_package_count"),After.Num());Legacy->SetArrayField(TEXT("dirty_packages"),Strings(After)); }
        }
        if(DirtyBeforeCaptured&&(!Valid||!SafeEditorContext()||ContextLost)) { Prefix->SetBoolField(TEXT("dirty_final_observation_skipped_unsafe_context"),true); }
        if(FileTracked)
        {
            const bool Exists=IFileManager::Get().FileExists(*FilePath);const int64 Bytes=IFileManager::Get().FileSize(*FilePath);const bool Changed=Exists!=FileExistedBefore||Bytes!=FileSizeBefore;
            Prefix->SetBoolField(TEXT("file_exists_after"),Exists);Prefix->SetNumberField(TEXT("file_size_after"),static_cast<double>(Bytes));Prefix->SetBoolField(TEXT("observed_file_metadata_changed"),Changed);Prefix->SetBoolField(TEXT("PNG_save_returned_true"),PngWriteReturned);Result.bChanged=Result.bChanged||PngWriteReturned||Changed;
        }
        if(DirectoryTracked) { const bool Exists=IFileManager::Get().DirectoryExists(*Directory);Prefix->SetBoolField(TEXT("directory_exists_after"),Exists);Prefix->SetBoolField(TEXT("observed_new_directory"),!DirectoryExisted&&Exists);Result.bChanged=Result.bChanged||(!DirectoryExisted&&Exists); }
        const bool Restored=(!CleanApplied||CleanRestored)&&(!CameraApplied||!RestoreCamera||CameraRestored);Checks->SetBoolField(TEXT("required_restoration_verified"),Restored);Checks->SetBoolField(TEXT("captured_viewport_identity_retained"),Captured&&Valid);Checks->SetBoolField(TEXT("clean_flags_restored"),!CleanApplied||CleanRestored);Checks->SetBoolField(TEXT("temporary_camera_restored_to_post_jump_state"),!CameraApplied||!RestoreCamera||CameraRestored);
        if(Result.bSuccess&&(!Valid||!Restored)) { Fail(TEXT("final_state_unverified"),TEXT("Captured identity or required restoration changed after callbacks; inspect partial state without retry")); }
        Prefix->SetBoolField(TEXT("clean_state_restored"),!CleanApplied||CleanRestored);Prefix->SetBoolField(TEXT("temporary_camera_restored"),CameraApplied&&RestoreCamera&&CameraRestored);
        if(Result.bSuccess) { Result.bPostconditionPassed=true;Result.Stage=TEXT("complete");if(SparseLegacy) { Legacy->RemoveField(TEXT("success"));Legacy->RemoveField(TEXT("error")); }else { Legacy->SetBoolField(TEXT("success"),true); } }
        else if(!FailedStage.IsEmpty()) { Result.Stage=FailedStage; }
        Result.Data->SetBoolField(TEXT("total_change_state_unknown"),!Result.bChangeStateKnown);Result.Data->SetBoolField(TEXT("read_only_direct_API"),Observer);return Result;
    }
};
bool Enter(FWork& W,bool RequireEditor=true,bool DenyPIE=false)
{ if(Running) { return W.Fail(TEXT("reentrant"),TEXT("Another viewport capture/review operation is active")); }Running=true;if(!W.Initialize(RequireEditor,DenyPIE)) { Running=false;return false; }return true; }
bool PathField(FWork& W,FString& Out)
{
    W.Result.Stage=TEXT("validate_filepath");if(!W.Params->TryGetStringField(TEXT("filepath"),Out)) { return W.Fail(TEXT("filepath_missing"),TEXT("Missing 'filepath' parameter")); }
    if(!SafeText(Out,65536)) { return W.Fail(TEXT("filepath_invalid"),TEXT("Screenshot filepath exceeds its bound or contains NUL")); }return true;
}
bool LegacyInteger(FWork& W,const TCHAR* Key,int32 Default,int32& Out)
{
    Out=Default;if(!W.Params->HasField(Key)) { return true; }double N=0;W.Params->TryGetNumberField(Key,N);
    if(!FMath::IsFinite(N)||N<=static_cast<double>(MIN_int32)-1.0||N>=static_cast<double>(MAX_int32)+1.0) { return W.Fail(TEXT("integer_conversion_unsafe"),FString(Key)+TEXT(" cannot safely use original int32 truncation")); }
    Out=W.Params->GetIntegerField(Key);return true;
}
bool OptionalVector(FWork& W,const TCHAR* Key,bool& Has,FVector& Out)
{
    Has=W.Params->HasField(Key);if(!Has) { return true; }const TArray<TSharedPtr<FJsonValue>>* A=nullptr;
    if(!W.Params->TryGetArrayField(Key,A)||!A||A->Num()<3) { return W.Fail(TEXT("vector_invalid"),FString(Key)+TEXT(" must be an array of 3 numbers")); }
    Out=FVector((*A)[0]->AsNumber(),(*A)[1]->AsNumber(),(*A)[2]->AsNumber());if(!Finite(Out)) { return W.Fail(TEXT("vector_nonfinite"),FString(Key)+TEXT(" is nonfinite after original AsNumber conversion")); }return true;
}
bool ReadSize(FWork& W,FIntPoint& Out)
{
    W.Result.Stage=TEXT("validate_viewport_size");if(!W.Admit()) { return false; }Out=W.Viewport->GetSizeXY();W.Prefix->SetNumberField(TEXT("readback_width"),Out.X);W.Prefix->SetNumberField(TEXT("readback_height"),Out.Y);
    if(Out.X<=0||Out.Y<=0) { return W.Fail(TEXT("viewport_size_invalid"),TEXT("Active viewport has invalid size")); }
    if(Out.X>MaxDimension||Out.Y>MaxDimension||static_cast<int64>(Out.X)*Out.Y>MaxPixels) { return W.Fail(TEXT("pixel_limit"),TEXT("Active viewport exceeds bounded screenshot dimensions/pixel count")); }return true;
}
bool SavePixels(FWork& W,const FIntPoint& Size)
{
    TArray<FColor> Pixels;if(!W.Write(TEXT("read_pixels"),TEXT("read_pixels_calls"))) { return false; }const bool Read=W.Viewport->ReadPixels(Pixels,FReadSurfaceDataFlags(),FIntRect(0,0,Size.X,Size.Y));W.Count(TEXT("read_pixels_returns"));W.Prefix->SetBoolField(TEXT("read_pixels_returned"),Read);W.Prefix->SetNumberField(TEXT("pixel_count"),Pixels.Num());W.Result.Data->SetBoolField(TEXT("default_ReadPixels_internal_render_flush_not_instrumented"),true);
    if(!Read) { W.Fail(TEXT("read_pixels_failed"),W.SparseLegacy?TEXT("Failed to take screenshot"):TEXT("Failed to read active viewport pixels")); }
    const bool Identity=W.Admit();const bool Restored=W.Restore();if(!Read||!Identity||!Restored) { return false; }
    if(Pixels.Num()!=static_cast<int64>(Size.X)*Size.Y||W.Viewport->GetSizeXY()!=Size) { W.Result.Stage=TEXT("validate_pixels");return W.Fail(TEXT("pixel_count_mismatch"),TEXT("Viewport size/pixel count changed during readback; no PNG was written")); }
    for(FColor& P:Pixels) { P.A=255; }W.Checks->SetBoolField(TEXT("pixels_match_captured_dimensions"),true);W.Checks->SetBoolField(TEXT("PNG_source_alpha_all_255"),true);
    TArray64<uint8> Compressed;if(!W.Write(TEXT("compress_PNG"),TEXT("PNG_compress_calls"))) { return false; }FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),Compressed);W.Count(TEXT("PNG_compress_returns"));W.Prefix->SetNumberField(TEXT("compressed_byte_count"),static_cast<double>(Compressed.Num()));
    if(!W.Admit()) { return false; }if(Compressed.IsEmpty()) { W.Result.Stage=TEXT("validate_PNG");return W.Fail(TEXT("PNG_compress_failed"),TEXT("PNG compression returned no bytes; no file write was dispatched")); }
    if(!W.Write(TEXT("save_PNG_file"),TEXT("PNG_save_calls"))) { return false; }W.PngWriteReturned=FFileHelper::SaveArrayToFile(Compressed,*W.FilePath);W.Count(TEXT("PNG_save_returns"));W.Prefix->SetBoolField(TEXT("PNG_save_returned_true"),W.PngWriteReturned);
    if(!W.PngWriteReturned) { return W.Fail(TEXT("PNG_save_failed"),W.SparseLegacy?TEXT("Failed to take screenshot"):TEXT("Failed to save screenshot: ")+W.FilePath); }
    W.Result.bChanged=true;const int64 Actual=IFileManager::Get().FileSize(*W.FilePath);const bool Good=IFileManager::Get().FileExists(*W.FilePath)&&Actual==Compressed.Num();W.Checks->SetBoolField(TEXT("PNG_file_size_matches_written_bytes"),Good);
    if(!Good) { return W.Fail(TEXT("PNG_file_postcondition_failed"),TEXT("PNG save returned true but observed file existence/size differs; file changes may remain")); }
    W.Legacy->SetStringField(TEXT("filepath"),W.FilePath);if(!W.SparseLegacy) { W.Legacy->SetNumberField(TEXT("width"),Size.X);W.Legacy->SetNumberField(TEXT("height"),Size.Y);W.Legacy->SetNumberField(TEXT("pixel_count"),Pixels.Num());W.Legacy->SetNumberField(TEXT("compressed_byte_count"),static_cast<double>(Compressed.Num()));W.Legacy->SetNumberField(TEXT("file_size_bytes"),static_cast<double>(Actual)); }return W.Admit();
}
}

Variable::FResult ListBookmarks()
{
    FWork W(TEXT("list_viewport_bookmarks"),true);if(!Enter(W)) { return W.Finish(); }ON_SCOPE_EXIT { Running=false; };
    W.Result.Stage=TEXT("list_bookmark_slots");W.Result.bChangeStateKnown=false;W.Count(TEXT("bookmark_tools_get_calls"));IBookmarkTypeTools& Tools=IBookmarkTypeTools::Get();W.Count(TEXT("bookmark_tools_get_returns"));if(!W.Admit()) { return W.Finish(); }
    W.Count(TEXT("bookmark_max_calls"));const uint32 Max=Tools.GetMaxNumberOfBookmarks(W.EditorClient);W.Count(TEXT("bookmark_max_returns"));if(!W.Admit()) { return W.Finish(); }if(Max>MaxBookmarks) { W.Fail(TEXT("bookmark_limit"),TEXT("Configured bookmark slots exceed listing bound"));return W.Finish(); }
    TArray<TSharedPtr<FJsonValue>> Rows,Existing;
    for(uint32 I=0;I<Max;++I) { W.Count(TEXT("bookmark_check_calls"));const bool Exists=Tools.CheckBookmark(I,W.EditorClient);W.Count(TEXT("bookmark_check_returns"));if(!W.Admit()) { return W.Finish(); }auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),I);J->SetBoolField(TEXT("exists"),Exists);Rows.Add(MakeShared<FJsonValueObject>(J));if(Exists) { Existing.Add(MakeShared<FJsonValueNumber>(I)); } }
    const FVector L=W.EditorClient->GetViewLocation();const FRotator R=W.EditorClient->GetViewRotation();if(!Finite(L)||!Finite(R)) { W.Fail(TEXT("view_nonfinite"),TEXT("Observed camera transform is nonfinite"));return W.Finish(); }
    W.Legacy->SetNumberField(TEXT("max_bookmark_count"),Max);W.Legacy->SetArrayField(TEXT("bookmarks"),Rows);W.Legacy->SetArrayField(TEXT("existing_indices"),Existing);W.Legacy->SetArrayField(TEXT("view_location"),VectorJson(L));W.Legacy->SetArrayField(TEXT("view_rotation"),RotationJson(R));W.Legacy->SetBoolField(TEXT("is_perspective"),W.EditorClient->IsPerspective());W.Checks->SetBoolField(TEXT("all_configured_slots_projected"),Rows.Num()==static_cast<int32>(Max));W.Result.bSuccess=true;return W.Finish();
}

Variable::FResult SetRealtime(const FString& ParamsJson)
{
    FWork W(TEXT("set_active_viewport_realtime"));if(!W.Request(ParamsJson)||!Enter(W)) { return W.Finish(); }ON_SCOPE_EXIT { Running=false; };
    bool Realtime=true;W.Params->TryGetBoolField(TEXT("realtime"),Realtime);int32 Frames=300;W.Params->TryGetNumberField(TEXT("realtime_frame_count"),Frames);Frames=FMath::Max(0,Frames);
    const FText Name=FText::FromString(TEXT("MCPWeather Viewport Review"));const bool Previous=W.EditorClient->IsRealtime(),Had=W.EditorClient->HasRealtimeOverride(Name);const FString Message=W.EditorClient->GetRealtimeOverrideMessage().BuildSourceString();
    W.Prefix->SetBoolField(TEXT("previous_realtime"),Previous);W.Prefix->SetBoolField(TEXT("had_review_override"),Had);W.Prefix->SetStringField(TEXT("previous_override_message"),Message);W.Prefix->SetBoolField(TEXT("effective_realtime"),Realtime);W.Prefix->SetNumberField(TEXT("effective_realtime_frame_count"),Frames);
    if(!W.Begin()||!W.Write(TEXT("remove_review_override"),TEXT("remove_realtime_override_calls"))) { return W.Finish(); }const bool Removed=W.EditorClient->RemoveRealtimeOverride(Name,false);W.Count(TEXT("remove_realtime_override_returns"));W.Prefix->SetBoolField(TEXT("removed_review_override"),Removed);W.Result.bChanged=W.Result.bChanged||Removed;
    if(Realtime)
    {
        if(!W.Write(TEXT("add_review_override"),TEXT("add_realtime_override_calls"))) { return W.Finish(); }W.EditorClient->AddRealtimeOverride(true,Name);W.Count(TEXT("add_realtime_override_returns"));W.Result.bChanged=true;
        if(Frames>0) { if(!W.Write(TEXT("request_realtime_frames"),TEXT("request_realtime_frames_calls"))) { return W.Finish(); }W.EditorClient->RequestRealTimeFrames(static_cast<uint64>(Frames));W.Count(TEXT("request_realtime_frames_returns")); }
    }
    if(!W.Invalidate(true)) { return W.Finish(); }W.Close();if(!W.Admit()) { return W.Finish(); }
    const bool Has=W.EditorClient->HasRealtimeOverride(Name),Effective=!Realtime||W.EditorClient->IsRealtime();W.Checks->SetBoolField(TEXT("review_override_presence_matches"),Has==Realtime);W.Checks->SetBoolField(TEXT("enabled_realtime_is_effective"),Effective);W.Checks->SetBoolField(TEXT("positive_frame_request_dispatched"),!Realtime||Frames<=0||W.Calls->GetNumberField(TEXT("request_realtime_frames_returns"))==1);
    if(Has!=Realtime||!Effective) { W.Fail(TEXT("realtime_postcondition_failed"),TEXT("Review override presence/effective state differs after callbacks; other overrides and pending frames were not globally cancelled"));return W.Finish(); }
    const FVector L=W.EditorClient->GetViewLocation();const FRotator R=W.EditorClient->GetViewRotation();if(!Finite(L)||!Finite(R)) { W.Fail(TEXT("view_nonfinite"),TEXT("Observed camera transform is nonfinite after override changes"));return W.Finish(); }
    W.Legacy->SetBoolField(TEXT("previous_realtime"),Previous);W.Legacy->SetBoolField(TEXT("had_review_override"),Had);W.Legacy->SetBoolField(TEXT("removed_review_override"),Removed);W.Legacy->SetStringField(TEXT("previous_override_message"),Message);W.Legacy->SetBoolField(TEXT("requested_realtime"),Realtime);W.Legacy->SetNumberField(TEXT("requested_realtime_frame_count"),Frames);W.Legacy->SetBoolField(TEXT("is_realtime"),W.EditorClient->IsRealtime());W.Legacy->SetBoolField(TEXT("is_realtime_override_set"),W.EditorClient->IsRealtimeOverrideSet());W.Legacy->SetStringField(TEXT("realtime_override_message"),W.EditorClient->GetRealtimeOverrideMessage().BuildSourceString());W.Legacy->SetArrayField(TEXT("view_location"),VectorJson(L));W.Legacy->SetArrayField(TEXT("view_rotation"),RotationJson(R));W.Legacy->SetBoolField(TEXT("is_perspective"),W.EditorClient->IsPerspective());W.Result.bSuccess=true;return W.Finish();
}

Variable::FResult CaptureBookmark(const FString& ParamsJson)
{
    FWork W(TEXT("capture_viewport_bookmark_screenshot"));W.DirtyStart();if(!W.Request(ParamsJson)) { return W.Finish(); }FString File;if(!PathField(W,File)||!Enter(W,true,true)) { return W.Finish(); }ON_SCOPE_EXIT { Running=false; };
    W.Result.Stage=TEXT("decode_capture_options");int32 Redraw=1;if(!LegacyInteger(W,TEXT("redraw_count"),1,Redraw)) { return W.Finish(); }bool Clean=false;W.Params->TryGetBoolField(TEXT("clean_game_view"),Clean);W.Params->TryGetBoolField(TEXT("restore_view_after_capture"),W.RestoreCamera);
    bool HasLocation=false,HasRotation=false;FVector Location=FVector::ZeroVector,Rotation=FVector::ZeroVector;if(!OptionalVector(W,TEXT("view_location"),HasLocation,Location)||!OptionalVector(W,TEXT("view_rotation"),HasRotation,Rotation)) { return W.Finish(); }
    const bool HasFov=W.Params->HasField(TEXT("view_fov"))||W.Params->HasField(TEXT("fov"));double Fov=0;
    if(HasFov)
    {
        const TCHAR* Key=W.Params->HasField(TEXT("view_fov"))?TEXT("view_fov"):TEXT("fov");if(!W.Params->TryGetNumberField(Key,Fov)) { W.Fail(TEXT("FOV_invalid"),FString(Key)+TEXT(" must be a number"));return W.Finish(); }
        if(!FMath::IsFinite(Fov)||Fov<=0.0||Fov>=180.0) { W.Fail(TEXT("FOV_range"),TEXT("view_fov must be greater than 0 and less than 180 degrees"));return W.Finish(); }
    }
    const bool BookmarkRequested=W.Params->HasField(TEXT("bookmark_index"));int32 Bookmark=INDEX_NONE;bool Exists=false;IBookmarkTypeTools* Tools=nullptr;
    if(BookmarkRequested)
    {
        if(!LegacyInteger(W,TEXT("bookmark_index"),INDEX_NONE,Bookmark)) { return W.Finish(); }if(Bookmark<0) { W.Fail(TEXT("bookmark_negative"),TEXT("bookmark_index must be >= 0"));return W.Finish(); }
        W.Result.bChangeStateKnown=false;W.Count(TEXT("bookmark_tools_get_calls"));Tools=&IBookmarkTypeTools::Get();W.Count(TEXT("bookmark_tools_get_returns"));if(!W.Admit()) { return W.Finish(); }
        W.Count(TEXT("bookmark_max_calls"));const uint32 Max=Tools->GetMaxNumberOfBookmarks(W.EditorClient);W.Count(TEXT("bookmark_max_returns"));if(!W.Admit()) { return W.Finish(); }
        if(static_cast<uint32>(Bookmark)>=Max) { W.Fail(TEXT("bookmark_range"),FString::Printf(TEXT("bookmark_index %d is outside the valid range 0-%u"),Bookmark,Max>0?Max-1:0));return W.Finish(); }
        W.Count(TEXT("bookmark_check_calls"));Exists=Tools->CheckBookmark(static_cast<uint32>(Bookmark),W.EditorClient);W.Count(TEXT("bookmark_check_returns"));if(!W.Admit()) { return W.Finish(); }if(!Exists) { W.Fail(TEXT("bookmark_missing"),FString::Printf(TEXT("No bookmark exists at index %d"),Bookmark));return W.Finish(); }
    }
    W.Prefix->SetBoolField(TEXT("bookmark_requested"),BookmarkRequested);W.Prefix->SetBoolField(TEXT("bookmark_exists"),Exists);W.Prefix->SetNumberField(TEXT("bookmark_index"),Bookmark);W.Prefix->SetBoolField(TEXT("bookmark_camera_target_verified"),false);W.Prefix->SetBoolField(TEXT("bookmark_action_registered_verified"),false);
    if(!W.Begin()) { return W.Finish(); }
    if(BookmarkRequested)
    {
        if(!W.Write(TEXT("jump_to_bookmark"),TEXT("bookmark_jump_calls"))) { return W.Finish(); }Tools->JumpToBookmark(static_cast<uint32>(Bookmark),TSharedPtr<FBookmarkBaseJumpToSettings>(),W.EditorClient);W.Count(TEXT("bookmark_jump_returns"));W.Prefix->SetBoolField(TEXT("bookmark_jump_dispatched"),true);W.Observe(TEXT("post_bookmark_view"));if(!W.Admit()||!W.Invalidate()) { return W.Finish(); }
    }
    if(!File.EndsWith(TEXT(".png"),ESearchCase::IgnoreCase)) { File+=TEXT(".png"); }FPaths::NormalizeFilename(File);W.TrackFile(File);W.Directory=FPaths::GetPath(File);
    if(!W.Directory.IsEmpty())
    {
        W.DirectoryTracked=true;W.DirectoryExisted=IFileManager::Get().DirectoryExists(*W.Directory);W.Prefix->SetBoolField(TEXT("directory_existed_before"),W.DirectoryExisted);
        if(!W.Write(TEXT("create_output_directory"),TEXT("directory_create_calls"))) { return W.Finish(); }const bool Made=IFileManager::Get().MakeDirectory(*W.Directory,true);W.Count(TEXT("directory_create_returns"));W.Prefix->SetBoolField(TEXT("directory_create_returned"),Made);if(!W.DirectoryExisted&&IFileManager::Get().DirectoryExists(*W.Directory)) { W.Result.bChanged=true; }
    }
    if(!W.Admit()) { return W.Finish(); }W.PreviousLocation=W.EditorClient->GetViewLocation();W.PreviousRotation=W.EditorClient->GetViewRotation();W.PreviousFov=W.EditorClient->ViewFOV;W.PreviousFovAngle=W.EditorClient->FOVAngle;
    if(!Finite(W.PreviousLocation)||!Finite(W.PreviousRotation)||!FMath::IsFinite(W.PreviousFov)||!FMath::IsFinite(W.PreviousFovAngle)) { W.Fail(TEXT("previous_view_nonfinite"),TEXT("Post-bookmark camera is nonfinite and cannot be safely restored"));return W.Finish(); }
    W.Prefix->SetObjectField(TEXT("post_jump_pre_override_view"),W.View());W.Prefix->SetBoolField(TEXT("camera_override_requested"),HasLocation||HasRotation||HasFov);W.Prefix->SetBoolField(TEXT("restore_view_after_capture"),W.RestoreCamera);
    if(HasLocation) { if(!W.Write(TEXT("set_capture_location"),TEXT("set_view_location_calls"))) { return W.Finish(); }W.CameraApplied=true;W.EditorClient->SetViewLocation(Location);W.Count(TEXT("set_view_location_returns")); }
    if(HasRotation) { if(!W.Write(TEXT("set_capture_rotation"),TEXT("set_view_rotation_calls"))) { return W.Finish(); }W.CameraApplied=true;W.EditorClient->SetViewRotation(FRotator(Rotation.X,Rotation.Y,Rotation.Z));W.Count(TEXT("set_view_rotation_returns")); }
    if(HasFov) { if(!W.Write(TEXT("set_capture_FOV"),TEXT("FOV_field_writes"))) { return W.Finish(); }W.CameraApplied=true;W.EditorClient->FOVAngle=FMath::Clamp(static_cast<float>(Fov),1.0f,179.0f);W.Count(TEXT("FOV_field_writes"));W.EditorClient->ViewFOV=W.EditorClient->FOVAngle; }
    W.Observe(TEXT("capture_override_view"));if(!W.Admit()||!W.Invalidate()) { return W.Finish(); }
    const int32 Draws=FMath::Clamp(Redraw,0,20);for(int32 I=0;I<Draws;++I) { if(!W.Invalidate()||!W.Draw()) { return W.Finish(); } }
    FIntPoint Dimensions;if(!ReadSize(W,Dimensions)) { return W.Finish(); }W.Prefix->SetBoolField(TEXT("clean_game_view_requested"),Clean);
    if(Clean&&W.LevelClient)
    {
        W.PreviousGameView=W.LevelClient->IsInGameView();W.PreviousOutline=W.LevelClient->EngineShowFlags.SelectionOutline;W.PreviousWidgets=W.LevelClient->EngineShowFlags.ModeWidgets;
        if(!W.Write(TEXT("enable_clean_game_view"),TEXT("set_game_view_calls"))) { return W.Finish(); }W.CleanApplied=true;W.LevelClient->SetGameView(true);W.Count(TEXT("set_game_view_returns"));
        if(!W.Write(TEXT("hide_capture_overlays"),TEXT("show_flag_writes"))) { return W.Finish(); }W.LevelClient->EngineShowFlags.SetSelectionOutline(false);W.Count(TEXT("show_flag_writes"));W.LevelClient->EngineShowFlags.SetModeWidgets(false);W.Observe(TEXT("clean_capture_view"));
        if(!W.Admit()||!W.Invalidate()||!W.Draw()) { return W.Finish(); }
    }
    W.Prefix->SetBoolField(TEXT("clean_game_view_applied"),W.CleanApplied);if(!W.Admit()) { return W.Finish(); }const FVector L=W.EditorClient->GetViewLocation();const FRotator R=W.EditorClient->GetViewRotation();const float F=W.EditorClient->ViewFOV;W.Prefix->SetObjectField(TEXT("pixel_capture_view"),W.View());
    if(!Finite(L)||!Finite(R)||!FMath::IsFinite(F)) { W.Fail(TEXT("capture_view_nonfinite"),TEXT("Captured camera projection is nonfinite; no PNG was written"));return W.Finish(); }
    const bool CameraMatches=(!HasLocation||L.Equals(Location,1e-6))&&(!HasRotation||R.Equals(FRotator(Rotation.X,Rotation.Y,Rotation.Z),1e-6))&&(!HasFov||F==FMath::Clamp(static_cast<float>(Fov),1.0f,179.0f));
    const bool CleanMatches=!W.CleanApplied||(W.LevelClient->IsInGameView()&&!W.LevelClient->EngineShowFlags.SelectionOutline&&!W.LevelClient->EngineShowFlags.ModeWidgets);
    W.Checks->SetBoolField(TEXT("capture_override_matches_requested_values"),CameraMatches);W.Checks->SetBoolField(TEXT("clean_capture_flags_match"),CleanMatches);
    if(!CameraMatches||!CleanMatches) { W.Fail(TEXT("capture_view_postcondition_failed"),TEXT("Actual capture camera/clean state differs from requested overrides; no PNG was written and UI changes may remain"));return W.Finish(); }
    if(!SavePixels(W,Dimensions)) { return W.Finish(); }W.Close();if(!W.Admit()) { return W.Finish(); }
    W.Legacy->SetNumberField(TEXT("redraw_count"),Draws);W.Legacy->SetBoolField(TEXT("clean_game_view"),Clean);W.Legacy->SetBoolField(TEXT("clean_game_view_applied"),W.CleanApplied);W.Legacy->SetArrayField(TEXT("view_location"),VectorJson(L));W.Legacy->SetArrayField(TEXT("view_rotation"),RotationJson(R));W.Legacy->SetNumberField(TEXT("view_fov"),F);W.Legacy->SetBoolField(TEXT("camera_override_requested"),HasLocation||HasRotation||HasFov);W.Legacy->SetBoolField(TEXT("camera_override_applied"),W.CameraApplied);W.Legacy->SetBoolField(TEXT("restore_view_after_capture"),W.RestoreCamera);W.Legacy->SetBoolField(TEXT("view_restored_after_capture"),W.CameraApplied&&W.RestoreCamera&&W.CameraRestored);W.Legacy->SetBoolField(TEXT("is_perspective"),W.EditorClient->IsPerspective());W.Legacy->SetBoolField(TEXT("bookmark_requested"),BookmarkRequested);W.Legacy->SetBoolField(TEXT("bookmark_exists"),Exists);W.Legacy->SetNumberField(TEXT("bookmark_index"),Bookmark);W.Legacy->SetStringField(TEXT("capture_mode"),BookmarkRequested?TEXT("bookmark"):TEXT("active_viewport"));W.Checks->SetBoolField(TEXT("original_camera_restore_scope_preserved"),true);W.Checks->SetBoolField(TEXT("captured_transform_recorded_before_readback"),true);W.Result.bSuccess=true;return W.Finish();
}

Variable::FResult Take(const FString& ParamsJson)
{
    FWork W(TEXT("take_screenshot"),false,true);if(!W.Request(ParamsJson)) { return W.Finish(); }FString File;if(!PathField(W,File)) { return W.Finish(); }
    if(!File.EndsWith(TEXT(".png"))) { File+=TEXT(".png"); }if(!Enter(W,false,true)) { if(W.Result.ErrorCode==TEXT("viewport_capture_editor_required")||W.Result.ErrorCode==TEXT("viewport_capture_viewport_missing")) { W.Legacy->SetStringField(TEXT("error"),TEXT("Failed to take screenshot")); }return W.Finish(); }ON_SCOPE_EXIT { Running=false; };W.TrackFile(File);
    if(!W.Begin()) { return W.Finish(); }FIntPoint Dimensions;if(!ReadSize(W,Dimensions)||!SavePixels(W,Dimensions)) { return W.Finish(); }
    W.Close();if(!W.Admit()) { return W.Finish(); }W.Checks->SetBoolField(TEXT("legacy_filepath_only_success_shape"),true);W.Result.bSuccess=true;return W.Finish();
}
}
