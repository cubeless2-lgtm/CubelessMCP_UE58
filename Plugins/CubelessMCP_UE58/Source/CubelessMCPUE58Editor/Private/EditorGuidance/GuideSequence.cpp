#include "GuideSequence.h"
#include "EditorGuidance.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Misc/CoreMisc.h"
#include "UObject/UObjectGlobals.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformProcess.h"

namespace Cubeless::EditorGuidance::Sequence
{
namespace
{
struct FCertificate
{
    FString Action, Arguments;
    TWeakPtr<SWindow> Window;
    TFunction<bool()> IsCurrent;
    double Issued = 0;
    double LastPaint = 0;
    FSlateRect PaintedBounds;
};
struct FStep { FString Token, Title, Message; };
TMap<FString,FCertificate> Certificates;
TArray<FStep> Steps;
int32 Index = INDEX_NONE;
bool Active = false, Paused = false;
bool ControlsDismissed = false;
FString Phase, Error, CurrentToken, TargetState, ControlsState;
TSharedPtr<SWindow> Controls;
TWeakPtr<SWindow> ControlsParent;
FTSTicker::FDelegateHandle Ticker;
bool NativeAlive(const TSharedPtr<SWindow>& Window)
{return Window && Window->GetNativeWindow() && Window->GetNativeWindow()->GetOSWindowHandle();}

FString Json(const TSharedRef<FJsonObject>& Object)
{
    FString Out; FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Out)); return Out;
}
TSharedPtr<FJsonObject> Parse(const FString& Text)
{
    TSharedPtr<FJsonObject> Out;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out); return Out;
}
TSharedRef<FJsonObject> State()
{
    auto Out=MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("active"),Active); Out->SetBoolField(TEXT("paused"),Paused);
    Out->SetStringField(TEXT("phase"),Phase); Out->SetStringField(TEXT("error"),Error);
    Out->SetNumberField(TEXT("current_index"),Index); Out->SetNumberField(TEXT("current_step"),Active ? Index+1 : 0);
    Out->SetNumberField(TEXT("step_count"),Steps.Num()); Out->SetStringField(TEXT("current_target_token"),CurrentToken);
    Out->SetStringField(TEXT("last_target_state"),TargetState); Out->SetBoolField(TEXT("controls_open"),NativeAlive(Controls) && Controls->GetNativeWindow()->IsVisible() && !ControlsDismissed);
    Out->SetStringField(TEXT("controls_state"),ControlsState);
    Out->SetNumberField(TEXT("process_id"),FPlatformProcess::GetCurrentProcessId());
    if(NativeAlive(Controls))Out->SetStringField(TEXT("controls_native_window_id"),LexToString(reinterpret_cast<UPTRINT>(Controls->GetNativeWindow()->GetOSWindowHandle())));
    auto Painted=Certificates.Find(CurrentToken);
    Out->SetNumberField(TEXT("last_actual_paint_age_seconds"),Painted && Painted->LastPaint>0 ? FPlatformTime::Seconds()-Painted->LastPaint : -1);
    TArray<TSharedPtr<FJsonValue>> Items;
    for (int32 I=0; I<Steps.Num(); ++I)
    {
        auto Item=MakeShared<FJsonObject>(); Item->SetNumberField(TEXT("step"),I+1); Item->SetStringField(TEXT("target_token"),Steps[I].Token);
        Item->SetStringField(TEXT("title"),Steps[I].Title); Item->SetStringField(TEXT("message"),Steps[I].Message);
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    Out->SetArrayField(TEXT("steps"),Items); return Out;
}
void CloseControls()
{
    auto Closing=MoveTemp(Controls); ControlsParent.Reset();
    if (Closing && FSlateApplication::IsInitialized())
    {
        // Release module-owned callbacks synchronously, even though native
        // window destruction itself is deferred by Slate.
        Closing->SetOnWindowClosed(FOnWindowClosed());
        Closing->SetContent(SNullWidget::NullWidget);
        Closing->RequestDestroyWindow();
    }
    ControlsState=TEXT("closed");
}
FText PositionText()
{
    return FText::FromString(Active && Steps.IsValidIndex(Index) ? FString::Printf(TEXT("%d / %d  ·  %s"),Index+1,Steps.Num(),*Steps[Index].Title) : TEXT(""));
}
FText PhaseText()
{
    if (Paused) return FText::FromString(TEXT("일시 정지 · 질문 후 같은 위치에서 이어갈 수 있습니다."));
    if (Phase==TEXT("ready")) return FText::FromString(TEXT("강조된 실제 대상을 확인하세요."));
    if (Phase==TEXT("blocked")) return FText::FromString(TEXT("이 단계에서 멈췄습니다. 대상을 확인한 뒤 이어가세요."));
    return FText::FromString(TEXT("현재 대상의 표시를 기다립니다."));
}
FText OrderText()
{
    FString Text;
    for (int32 I=0; I<Steps.Num(); ++I) Text+=FString::Printf(TEXT("%d. %s%s\n"),I+1,*Steps[I].Title,I==Index ? TEXT("  ◀") : TEXT(""));
    return FText::FromString(Text);
}
void EnsureControls(const TSharedPtr<SWindow>& Parent)
{
    if (ControlsDismissed || !NativeAlive(Parent) || Parent->IsWindowMinimized()) return;
    if (NativeAlive(Controls) && ControlsParent.Pin()==Parent) return;
    CloseControls();
    // A non-activating native child keeps keyboard focus in the actual editor.
    // Buttons are also non-focusable: opening/clicking this UI must not commit
    // an inline property edit merely because a controller stole focus.
    Controls=SNew(SWindow).Type(EWindowType::Notification).Title(FText::FromString(TEXT("튜토리얼 안내")))
        .AutoCenter(EAutoCenter::None).ScreenPosition(Parent->GetPositionInScreen()+FVector2D(20,80))
        .SizingRule(ESizingRule::Autosized).ClientSize(FVector2D(460,220))
        .FocusWhenFirstShown(false).ActivationPolicy(EWindowActivationPolicy::Never)
        .SupportsMaximize(false).SupportsMinimize(false).HasCloseButton(false)
        [
            SNew(SBorder).Padding(12).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            [
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([]{return PositionText();}).Font(FAppStyle::GetFontStyle("BoldFont")).WrapTextAt(420)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text_Lambda([]{return PhaseText();}).WrapTextAt(420)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text_Lambda([]{return FText::FromString(Steps.IsValidIndex(Index) ? Steps[Index].Message : TEXT(""));}).WrapTextAt(420)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,8)
                [
                    SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("이전"))).IsEnabled_Lambda([]{return Active && !Paused && Index>0;}).OnClicked_Lambda([]{Cubeless::EditorGuidance::Request(TEXT("sequence_previous"),TEXT("{}"));return FReply::Handled();})]
                    +SHorizontalBox::Slot().AutoWidth().Padding(6,0)[SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("다음"))).IsEnabled_Lambda([]{return Active && !Paused && Phase==TEXT("ready") && Index+1<Steps.Num();}).OnClicked_Lambda([]{Cubeless::EditorGuidance::Request(TEXT("sequence_next"),TEXT("{}"));return FReply::Handled();})]
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).Text_Lambda([]{return FText::FromString(Paused || Phase==TEXT("blocked") ? TEXT("이어가기") : TEXT("일시 정지"));}).OnClicked_Lambda([]{Cubeless::EditorGuidance::Request(Paused || Phase==TEXT("blocked") ? TEXT("sequence_resume") : TEXT("sequence_pause"),TEXT("{}"));return FReply::Handled();})]
                    +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right)[SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("종료"))).OnClicked_Lambda([]{Cubeless::EditorGuidance::Request(TEXT("sequence_end"),TEXT("{}"));return FReply::Handled();})]
                ]
                +SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SExpandableArea).InitiallyCollapsed(true)
                    .HeaderContent()[SNew(STextBlock).Text(FText::FromString(TEXT("전체 순서")))]
                    .BodyContent()[SNew(SBox).WidthOverride(420).HeightOverride(140)[SNew(SScrollBox)+SScrollBox::Slot()[SNew(STextBlock).Text_Lambda([]{return OrderText();}).WrapTextAt(400)]]]
                ]
            ]
        ];
    ControlsParent=Parent;
    Controls->SetOnWindowClosed(FOnWindowClosed::CreateLambda([](const TSharedRef<SWindow>& Closed)
    {if(Controls.Get()==&Closed.Get()){Controls.Reset();ControlsParent.Reset();ControlsDismissed=true;ControlsState=TEXT("closed");}}));
    // Do not flash at an unsafe initial position before placement is checked.
    FSlateApplication::Get().AddWindowAsNativeChild(Controls.ToSharedRef(),Parent.ToSharedRef(),false);
}
bool Overlaps(const FSlateRect& A,const FSlateRect& B)
{
    return A.Left<B.Right && A.Right>B.Left && A.Top<B.Bottom && A.Bottom>B.Top;
}
void PlaceControls(const TSharedPtr<SWindow>& Parent,const TSharedPtr<FJsonObject>& Target)
{
    if (!Controls || !Parent) return;
    const auto Pos=Parent->GetPositionInScreen(), ParentSize=Parent->GetSizeInScreen();
    FVector2D Size=Controls->GetSizeInScreen(); if (Size.X<100 || Size.Y<50) Size=FVector2D(460,220);
    const FSlateRect Work=FSlateApplication::Get().GetWorkArea(FSlateRect(Pos.X,Pos.Y,Pos.X+ParentSize.X,Pos.Y+ParentSize.Y));
    TArray<FSlateRect> Avoid;
    const TArray<TSharedPtr<FJsonValue>>* Bounds=nullptr;
    if (Target && Target->TryGetArrayField(TEXT("screen_bounds"),Bounds) && Bounds->Num()==4)
    {
        FSlateRect R((*Bounds)[0]->AsNumber(),(*Bounds)[1]->AsNumber(),(*Bounds)[2]->AsNumber(),(*Bounds)[3]->AsNumber());
        FString Kind; Target->TryGetStringField(TEXT("target_kind"),Kind);
        // For whole-panel outlines, preserve their central hit-test anchor.
        // Small semantic targets are kept entirely unobstructed.
        if (Kind==TEXT("graph") || Kind==TEXT("panel") || Kind==TEXT("selection") || Kind==TEXT("variable"))
        { const double X=(R.Left+R.Right)/2,Y=(R.Top+R.Bottom)/2; R=FSlateRect(X-16,Y-16,X+16,Y+16); }
        Avoid.Add(R);
    }
    const FVector2D Candidates[]={Pos+FVector2D(16,ParentSize.Y-Size.Y-16),Pos+FVector2D(ParentSize.X-Size.X-16,ParentSize.Y-Size.Y-16),Pos+FVector2D(16,60),Pos+FVector2D(ParentSize.X-Size.X-16,60),Pos+FVector2D(0,-Size.Y-8),Pos+FVector2D(ParentSize.X+8,0)};
    for (auto Candidate : Candidates)
    {
        Candidate.X=FMath::Clamp(Candidate.X,(double)Work.Left,FMath::Max((double)Work.Left,Work.Right-Size.X));
        Candidate.Y=FMath::Clamp(Candidate.Y,(double)Work.Top,FMath::Max((double)Work.Top,Work.Bottom-Size.Y));
        const FSlateRect Rect(Candidate.X,Candidate.Y,Candidate.X+Size.X,Candidate.Y+Size.Y);
        bool Hit=false; for (const auto& R : Avoid) Hit|=Overlaps(Rect,R);
        if (!Hit)
        {
            if (!Controls->GetPositionInScreen().Equals(Candidate,1)) Controls->MoveWindowTo(Candidate);
            if (!Controls->GetNativeWindow()->IsVisible()) Controls->ShowWindow();
            ControlsState=TEXT("shown");return;
        }
    }
    Controls->HideWindow();ControlsState=TEXT("hidden_no_safe_space");
    if (!Paused) {Phase=TEXT("blocked");Error=TEXT("no_safe_controls_space");}
}
bool ApplyStep(int32 Desired)
{
    CurrentToken.Empty(); TargetState.Empty(); Error.Empty(); Phase=TEXT("waiting"); Index=Desired;ControlsDismissed=false;
    Cubeless::EditorGuidance::Request(TEXT("clear_slots"),TEXT("{}"));
    auto Cert=Certificates.Find(Steps[Index].Token);
    if (!Cert || !Cert->IsCurrent || !Cert->IsCurrent()) {Phase=TEXT("blocked");Error=TEXT("step_target_stale");return false;}
    auto Arguments=Parse(Cert->Arguments);
    if (!Arguments) {Phase=TEXT("blocked");Error=TEXT("certificate_arguments_invalid");return false;}
    Arguments->SetStringField(TEXT("message"),Steps[Index].Message); Arguments->SetStringField(TEXT("step_label"),Steps[Index].Title);
    // The controller is the explanation card; a single outline marks the exact
    // target, without creating two competing explanation cards.
    Arguments->SetStringField(TEXT("mode"),TEXT("outline"));
    Arguments->SetNumberField(TEXT("guide_step_number"),Index+1);
    const FString Action=Cert->Action;
    auto Result=Parse(Cubeless::EditorGuidance::Request(Action,Json(Arguments.ToSharedRef())));
    bool Success=false; if (Result) Result->TryGetBoolField(TEXT("success"),Success);
    const TSharedPtr<FJsonObject>* Target=nullptr;
    if (!Success || !Result->TryGetObjectField(TEXT("target"),Target) || !(*Target)->TryGetStringField(TEXT("target_token"),CurrentToken))
    {Phase=TEXT("blocked");Error=TEXT("step_request_failed"); if(Result) Result->TryGetStringField(TEXT("error"),Error);return false;}
    return true;
}
bool Tick(float)
{
    if (!Active || IsEngineExitRequested()) {Shutdown();return false;}
    if (!IsInGameThread() || !FSlateApplication::IsInitialized() || !GEditor || IsGarbageCollecting()) return true;
    auto Status=Parse(Cubeless::EditorGuidance::Request(TEXT("status"),TEXT("{}")));
    TSharedPtr<FJsonObject> Current;
    for (const TCHAR* Slot : {TEXT("node"),TEXT("property")})
    {
        const TSharedPtr<FJsonObject>* Candidate=nullptr; FString Token;
        if (Status && Status->TryGetObjectField(Slot,Candidate) && (*Candidate)->TryGetStringField(TEXT("target_token"),Token) && !CurrentToken.IsEmpty() && Token==CurrentToken) Current=*Candidate;
    }
    bool Displayed=false;
    if (Current) {Current->TryGetStringField(TEXT("state"),TargetState);Current->TryGetBoolField(TEXT("displayed"),Displayed);}
    else if (!CurrentToken.IsEmpty()) TargetState=TEXT("guide_closed_or_replaced");
    // Core target refresh runs before this ticker, and paint runs afterwards.
    // A stable geometry_resolved frame must not erase proof of the preceding
    // actual paint. Accept it only with matching bounds, window and recent
    // paint of this exact token; never manufacture paint from resolution.
    if (!Displayed && Current && (TargetState==TEXT("geometry_resolved") || TargetState==TEXT("layout_ready")))
    {
        auto Cert=Certificates.Find(CurrentToken);const TArray<TSharedPtr<FJsonValue>>* Bounds=nullptr;FString WindowId;
        if(Cert && Cert->LastPaint>0 && FPlatformTime::Seconds()-Cert->LastPaint<.5 && Current->TryGetArrayField(TEXT("screen_bounds"),Bounds) && Bounds->Num()==4 && Current->TryGetStringField(TEXT("native_window_id"),WindowId))
        {
            auto Window=Cert->Window.Pin();
            const FString PaintedWindow=NativeAlive(Window) ? LexToString(reinterpret_cast<UPTRINT>(Window->GetNativeWindow()->GetOSWindowHandle())) : FString();
            const auto& R=Cert->PaintedBounds;
            Displayed=WindowId==PaintedWindow && FMath::IsNearlyEqual((*Bounds)[0]->AsNumber(),(double)R.Left,1.0) && FMath::IsNearlyEqual((*Bounds)[1]->AsNumber(),(double)R.Top,1.0)
                && FMath::IsNearlyEqual((*Bounds)[2]->AsNumber(),(double)R.Right,1.0) && FMath::IsNearlyEqual((*Bounds)[3]->AsNumber(),(double)R.Bottom,1.0);
        }
    }
    if (!Paused && !CurrentToken.IsEmpty())
    {
        if (Displayed) {Phase=TEXT("ready");Error.Empty();}
        else
        {
            static const TSet<FString> Temporary={TEXT("pending_layout"),TEXT("pending_details_layout"),TEXT("pending_details_selection"),TEXT("geometry_resolved"),TEXT("layout_ready"),TEXT("clipped"),TEXT("inactive_window"),TEXT("occluded_by_menu_or_modal"),TEXT("occluded_or_not_hit_testable"),TEXT("hidden"),TEXT("play_session")};
            const bool Waiting=Current && Temporary.Contains(TargetState);
            Phase=Waiting ? TEXT("waiting") : TEXT("blocked");Error=Waiting ? FString() : TargetState;
        }
    }
    if (GEditor->PlayWorld && !Paused) Phase=TEXT("waiting");
    TSharedPtr<SWindow> Parent;
    if (auto Cert=Certificates.Find(CurrentToken)) Parent=Cert->Window.Pin();
    if (!Parent && Steps.IsValidIndex(Index)) if (auto Cert=Certificates.Find(Steps[Index].Token)) Parent=Cert->Window.Pin();
    if (!NativeAlive(Parent))
    {
        CloseControls();ControlsState=TEXT("owner_window_closed");
        if(!Paused){Phase=TEXT("blocked");Error=TEXT("owner_window_closed");}
        return true;
    }
    if (Parent->IsWindowMinimized())
    {
        if(Controls)Controls->HideWindow();ControlsState=TEXT("hidden_owner_minimized");
        if(!Paused){Phase=TEXT("waiting");Error.Empty();}return true;
    }
    if (ControlsDismissed)
    {if(!Paused){Phase=TEXT("blocked");Error=TEXT("controls_closed");}return true;}
    EnsureControls(Parent); PlaceControls(Parent,Current);
    return true;
}
}

bool Certify(const FString& Token,const FString& Action,const FString& Arguments,TWeakPtr<SWindow> Window,TFunction<bool()> IsCurrent)
{
    if (Certificates.Contains(Token)) return true;
    if (Action!=TEXT("show_node") && Action!=TEXT("show_property") && Action!=TEXT("show_target")) return false;
    if (Certificates.Num()>=128)
    {
        FString Oldest; double Time=DBL_MAX;
        for (const auto& Pair : Certificates)
        {
            bool Retained=Pair.Key==CurrentToken;
            for (const auto& Step : Steps) Retained|=Step.Token==Pair.Key;
            if (!Retained && Pair.Value.Issued<Time) {Time=Pair.Value.Issued;Oldest=Pair.Key;}
        }
        if (Oldest.IsEmpty()) return false; Certificates.Remove(Oldest);
    }
    FCertificate Cert; Cert.Action=Action;Cert.Arguments=Arguments;Cert.Window=Window;Cert.IsCurrent=MoveTemp(IsCurrent);Cert.Issued=FPlatformTime::Seconds();
    Certificates.Add(Token,MoveTemp(Cert));return true;
}
bool HasCertificate(const FString& Token) {return Certificates.Contains(Token);}
void ObservePaint(const FString& Token,TWeakPtr<SWindow> Window,const FSlateRect& Bounds)
{
    if(auto Cert=Certificates.Find(Token)) {Cert->LastPaint=FPlatformTime::Seconds();Cert->Window=Window;Cert->PaintedBounds=Bounds;}
}
bool OwnsActiveWindowFor(const TSharedPtr<SWindow>& TargetWindow,const TSharedPtr<SWindow>& ActiveWindow)
{
    // Only our own visible controller may stand in for its exact parent. Other
    // editor windows still suppress guidance through the normal active guard.
    return Active && NativeAlive(Controls) && Controls->GetNativeWindow()->IsVisible() && !ControlsDismissed
        && ActiveWindow==Controls && ControlsParent.Pin()==TargetWindow;
}

FString Request(const FString& Action,const FString& Arguments)
{
    auto Out=MakeShared<FJsonObject>();Out->SetBoolField(TEXT("success"),false);
    auto Finish=[&](bool Success,const TCHAR* Failure=nullptr)
    {Out->SetBoolField(TEXT("success"),Success);if(Failure)Out->SetStringField(TEXT("error"),Failure);Out->SetObjectField(TEXT("sequence"),State());return Json(Out);};
    if (Action==TEXT("sequence_status")) return Finish(true);
    if (Action==TEXT("sequence_end")) {Cubeless::EditorGuidance::Request(TEXT("clear"),TEXT("{}"));return Finish(true);}
    if (Action==TEXT("sequence_start"))
    {
        auto Input=Arguments.Len()<=16384 ? Parse(Arguments) : nullptr;
        const TArray<TSharedPtr<FJsonValue>>* List=nullptr;
        if (!Input || !Input->TryGetArrayField(TEXT("steps"),List) || List->IsEmpty() || List->Num()>32) return Finish(false,TEXT("one_to_32_verified_steps_required"));
        TArray<FStep> Prepared;
        for (int32 I=0;I<List->Num();++I)
        {
            auto Item=(*List)[I]->Type==EJson::Object ? (*List)[I]->AsObject() : nullptr; FStep Step;
            if (!Item || !Item->TryGetStringField(TEXT("target_token"),Step.Token) || !Item->TryGetStringField(TEXT("title"),Step.Title) || !Item->TryGetStringField(TEXT("message"),Step.Message)
                || Step.Title.TrimStartAndEnd().IsEmpty() || Step.Title.Len()>60 || Step.Message.TrimStartAndEnd().IsEmpty() || Step.Message.Len()>200)
            {Out->SetNumberField(TEXT("failed_step"),I+1);return Finish(false,TEXT("invalid_step"));}
            auto Cert=Certificates.Find(Step.Token);
            if (!Cert || !Cert->IsCurrent || !Cert->IsCurrent()) {Out->SetNumberField(TEXT("failed_step"),I+1);return Finish(false,TEXT("painted_current_process_target_required"));}
            Prepared.Add(MoveTemp(Step));
        }
        CloseControls();Steps=MoveTemp(Prepared);Active=true;Paused=false;ControlsDismissed=false;
        if (!Ticker.IsValid()) Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick),.1f);
        const bool Applied=ApplyStep(0);return Finish(Applied,Applied ? nullptr : TEXT("first_step_blocked"));
    }
    if (!Active) return Finish(false,TEXT("no_active_sequence"));
    if (Action==TEXT("sequence_pause")) {Paused=true;Phase=TEXT("paused");return Finish(true);}
    if (Action==TEXT("sequence_resume")) {Paused=false;const bool Applied=ApplyStep(Index);return Finish(Applied,Applied ? nullptr : TEXT("step_blocked"));}
    if (Action!=TEXT("sequence_previous") && Action!=TEXT("sequence_next")) return Finish(false,TEXT("unknown_sequence_action"));
    if (Paused) return Finish(false,TEXT("sequence_paused"));
    if (Action==TEXT("sequence_next") && Phase!=TEXT("ready")) return Finish(false,TEXT("current_step_not_displayed"));
    const int32 Desired=Index+(Action==TEXT("sequence_next") ? 1 : -1);
    if (!Steps.IsValidIndex(Desired)) return Finish(false,TEXT("sequence_boundary"));
    const bool Applied=ApplyStep(Desired);return Finish(Applied,Applied ? nullptr : TEXT("step_blocked"));
}

void Shutdown()
{
    if (Ticker.IsValid()) {FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}
    CloseControls();Steps.Empty();Certificates.Empty();Index=INDEX_NONE;Active=false;Paused=false;
    Phase.Empty();Error.Empty();CurrentToken.Empty();TargetState.Empty();ControlsDismissed=false;ControlsState=TEXT("closed");
}
}
