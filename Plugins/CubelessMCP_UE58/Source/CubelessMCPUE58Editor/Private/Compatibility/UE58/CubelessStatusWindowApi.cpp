#include "Compatibility/UE58/CubelessStatusWindowApi.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CoreMisc.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::StatusWindow
{
namespace
{
TSharedPtr<SWindow> Window;
TSharedPtr<STextBlock> Body;
FTSTicker::FDelegateHandle HideTicker;
FDelegateHandle PreExit;
uint64 Generation = 0;
bool Retired = false, Busy = false;
void RemoveTimer()
{
    if (HideTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(HideTicker); HideTicker.Reset(); }
}
void Closed(const TSharedRef<SWindow>& ClosedWindow)
{
    if (Window != ClosedWindow) { return; }
    ClosedWindow->SetOnWindowClosed(FOnWindowClosed());
    ++Generation; RemoveTimer(); Body.Reset(); Window.Reset();
}
bool Current(uint64 Token, const TSharedPtr<SWindow>& Expected, UEditorEngine* RequestEditor)
{
    return IsInGameThread() && GIsEditor && IsValid(RequestEditor) && GEditor == RequestEditor
        && !RequestEditor->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) && !IsEngineExitRequested() && !Retired
        && FSlateApplication::IsInitialized() && Token == Generation && Window == Expected;
}
FString Encode(const TSharedRef<FJsonObject>& Result)
{
    FString Encoded;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded));
    return Encoded;
}
}
void Close()
{
    if (!IsInGameThread()) { return; } // Owned lifecycle entrypoints run on the Editor game thread.
    ++Generation; RemoveTimer(); auto Owned = Window; Body.Reset(); Window.Reset();
    if (Owned.IsValid())
    {
        // Remove DLL callbacks before immediate native teardown; never close another window.
        Owned->SetOnWindowClosed(FOnWindowClosed());
        if (FSlateApplication::IsInitialized()) { FSlateApplication::Get().DestroyWindowImmediately(Owned.ToSharedRef()); }
    }
}
void Shutdown()
{
    Retired = true;
    if (PreExit.IsValid()) { FCoreDelegates::OnEnginePreExit.Remove(PreExit); PreExit.Reset(); }
    Close();
}
FString Show(const FString& Text, const FString& Command)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false); Result->SetStringField(TEXT("stage"), TEXT("status_preflight"));
    Result->SetStringField(TEXT("error_code"), TEXT("")); Result->SetStringField(TEXT("error"), TEXT(""));
    Result->SetBoolField(TEXT("ui_attempted"), false); Result->SetBoolField(TEXT("ui_changed"), false);
    Result->SetBoolField(TEXT("ui_state_known"), true); Result->SetBoolField(TEXT("window_created"), false);
    Result->SetBoolField(TEXT("window_present"), false); Result->SetBoolField(TEXT("progress_applied"), false);
    Result->SetBoolField(TEXT("asset_mutation_attempted"), false);
    const auto Fail = [&](const TCHAR* Code, const TCHAR* Message)
    { Result->SetStringField(TEXT("error_code"), Code); Result->SetStringField(TEXT("error"), Message); return Encode(Result); };
    if (!IsInGameThread() || !GIsEditor || !IsValid(GEditor) || IsRunningCommandlet() || IsEngineExitRequested() || Retired || Busy)
    { return Fail(TEXT("status_window_editor_state_invalid"), TEXT("Status UI requires a live nonreentrant Editor game-thread call.")); }
    if (FApp::IsUnattended() || !FApp::CanEverRender())
    { return Fail(TEXT("status_window_unattended"), TEXT("Unattended/non-rendering sessions refuse window creation; no popup was requested.")); }
    if (!FSlateApplication::IsInitialized())
    { return Fail(TEXT("status_window_slate_unavailable"), TEXT("Slate is not initialized; no window was requested.")); }
    if (Text.Len() > 65536 || Command.Len() > 1024)
    { return Fail(TEXT("status_window_text_bound"), TEXT("Status text/label exceed the bounded UI profile.")); }
    TGuardValue<bool> Reentry(Busy, true);
    TStrongObjectPtr<UEditorEngine> EditorHold(GEditor);
    RemoveTimer(); const uint64 Token = ++Generation;
    const FString EffectiveText = Text.IsEmpty() ? TEXT("이에타가 작업 상태를 확인하는 중이야.") : Text;
    const FString EffectiveCommand = Command.IsEmpty() ? TEXT("ieta_status") : Command;
    Result->SetStringField(TEXT("text"), EffectiveText); Result->SetStringField(TEXT("command"), EffectiveCommand);
    Result->SetBoolField(TEXT("original_command_label_filter_repaired"), true);
    if (!PreExit.IsValid()) { PreExit = FCoreDelegates::OnEnginePreExit.AddStatic(&Shutdown); }
    if (!Window.IsValid())
    {
        const FSlateRect Area = FSlateApplication::Get().GetPreferredWorkArea();
        if (!Current(Token, nullptr, EditorHold.Get())) { return Fail(TEXT("status_window_context_changed"), TEXT("UI context retired while querying the work area.")); }
        TSharedPtr<STextBlock> NewBody;
        TSharedRef<SWindow> NewWindow = SNew(SWindow)
            .Title(FText::FromString(TEXT("Cubeless MCP 상태")))
            .AutoCenter(EAutoCenter::None)
            .ScreenPosition(FVector2D(Area.Left + FMath::Max(0.0f, Area.Right-Area.Left-544.0f), Area.Top + FMath::Max(0.0f, Area.Bottom-Area.Top-194.0f)))
            .ClientSize(FVector2D(520.0f, 170.0f)).SizingRule(ESizingRule::FixedSize)
            .SupportsMaximize(false).SupportsMinimize(false).HasCloseButton(true)
            .IsTopmostWindow(true).FocusWhenFirstShown(false)
            [ SNew(SBorder).Padding(14.0f)
              [ SAssignNew(NewBody, STextBlock).AutoWrapText(true) ] ];
        if (!Current(Token, nullptr, EditorHold.Get())) { return Fail(TEXT("status_window_context_changed"), TEXT("UI context retired during widget construction; no native window was added.")); }
        Window = NewWindow; Body = NewBody;
        NewWindow->SetOnWindowClosed(FOnWindowClosed::CreateStatic(&Closed));
        Result->SetStringField(TEXT("stage"), TEXT("add_status_window"));
        Result->SetBoolField(TEXT("ui_attempted"), true); Result->SetBoolField(TEXT("ui_state_known"), false);
        FSlateApplication::Get().AddWindow(NewWindow, true);
        Result->SetBoolField(TEXT("window_created"), true); Result->SetBoolField(TEXT("ui_changed"), true);
        if (!Current(Token, NewWindow, EditorHold.Get())) { return Fail(TEXT("status_window_changed_during_add"), TEXT("Actual window-add prefix retained; window/context changed, no text write or retry.")); }
    }
    const auto OwnedWindow = Window; const auto OwnedBody = Body;
    if (!Current(Token, OwnedWindow, EditorHold.Get()) || !OwnedBody.IsValid())
    { return Fail(TEXT("status_window_ownership_changed"), TEXT("Owned window/text receiver is unavailable; no further UI call.")); }
    Result->SetStringField(TEXT("stage"), TEXT("update_status_text"));
    Result->SetBoolField(TEXT("ui_attempted"), true); Result->SetBoolField(TEXT("ui_state_known"), false);
    OwnedBody->SetText(FText::FromString(EffectiveText + TEXT("\n명령: ") + EffectiveCommand));
    Result->SetBoolField(TEXT("ui_changed"), true);
    if (!Current(Token, OwnedWindow, EditorHold.Get()) || Body != OwnedBody)
    { return Fail(TEXT("status_window_changed_during_text"), TEXT("Text assignment returned but receiver/context changed; no timer or retry.")); }
    HideTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Token](float)
    {
        if (Token != Generation) { return false; } // A stale callback must not clear a newer timer handle.
        HideTicker.Reset();
        if (IsInGameThread()) { Close(); }
        return false;
    }), 5.0f);
    if (!Current(Token, OwnedWindow, EditorHold.Get()) || !HideTicker.IsValid())
    { return Fail(TEXT("status_window_timer_unavailable"), TEXT("UI prefix retained but owned close timer/context is unavailable.")); }
    Result->SetBoolField(TEXT("window_present"), true); Result->SetBoolField(TEXT("ui_state_known"), true);
    Result->SetNumberField(TEXT("completion_visible_seconds"), 5.0);
    Result->SetStringField(TEXT("stage"), TEXT("complete")); Result->SetBoolField(TEXT("success"), true);
    return Encode(Result);
}
}
