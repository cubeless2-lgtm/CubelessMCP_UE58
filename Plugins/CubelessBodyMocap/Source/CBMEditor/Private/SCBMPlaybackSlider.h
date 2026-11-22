#pragma once
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"

DECLARE_DELEGATE_OneParam(FOnCBMPlaybackChanged,double);
DECLARE_DELEGATE_OneParam(FOnCBMMarkerSelected,int32);
struct FCBMPlaybackMarker{int32 Index=0;double Seconds=0;FString Label;bool Automatic=false,Selected=false,Edited=false;};

// A single playhead, with the same rail geometry as the blue selection range.
class SCBMPlaybackSlider : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SCBMPlaybackSlider){}
  SLATE_ATTRIBUTE(double,Duration)
  SLATE_ATTRIBUTE(double,Position)
  SLATE_ATTRIBUTE(double,FrameDuration)
  SLATE_ATTRIBUTE(TArray<FCBMPlaybackMarker>,Markers)
  SLATE_EVENT(FOnCBMMarkerSelected,OnMarkerSelected)
  SLATE_EVENT(FOnCBMPlaybackChanged,OnChanged)
  SLATE_EVENT(FSimpleDelegate,OnCaptureBegin)
  SLATE_EVENT(FSimpleDelegate,OnCaptureEnd)
 SLATE_END_ARGS()
 void Construct(const FArguments& A){Markers=A._Markers;MarkerSelected=A._OnMarkerSelected;Duration=A._Duration;Position=A._Position;FrameDuration=A._FrameDuration;Changed=A._OnChanged;CaptureBegin=A._OnCaptureBegin;CaptureEnd=A._OnCaptureEnd;}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(240,52);}
 virtual bool SupportsKeyboardFocus()const override{return true;}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool Enabled)const override{
  const double W=FMath::Max(1.,double(G.GetLocalSize().X)-16.);
  const double X=8.+W*FMath::Clamp(Position.Get(0.)/FMath::Max(.001,Duration.Get(0.)),0.,1.);
  auto Box=[&](double A,double Y,double Width,double Height,FLinearColor C){FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(Width,Height),FSlateLayoutTransform(FVector2D(A,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Enabled?C:C*.4f);};
  Box(8,34,W,8,FLinearColor(.08f,.09f,.1f));Box(8,34,X-8,8,FLinearColor(.55f,.035f,.04f));
  Box(X-4,27,8,22,FLinearColor(.95f,.12f,.14f));
  double LastLabel=-100;
  for(const auto& M:Markers.Get(TArray<FCBMPlaybackMarker>())){if(M.Seconds<0||M.Seconds>Duration.Get(0.)+.001)continue;const double P=8+W*M.Seconds/FMath::Max(.001,Duration.Get(0.));const FLinearColor Color=M.Selected?FLinearColor(1,.9f,.1f):M.Automatic?FLinearColor(.05f,.85f,1.f):FLinearColor(1.f,.43f,.04f);Box(P-1,22,2,21,Color);
   TArray<FVector2D> Shape=M.Automatic?TArray<FVector2D>{{P,15},{P+5,21},{P,27},{P-5,21},{P,15}}:TArray<FVector2D>{{P-5,16},{P+5,16},{P+5,26},{P-5,26},{P-5,16}};
   FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Shape,ESlateDrawEffect::None,Color,true,2.f);
   if(M.Edited)Box(P-3,29,6,4,FLinearColor::White);
   const double LabelX=FMath::Clamp(P-10.,0.,double(G.GetLocalSize().X)-24.);if(LabelX>LastLabel+24){FSlateDrawElement::MakeText(Out,Layer+1,G.ToPaintGeometry(FVector2D(24,13),FSlateLayoutTransform(FVector2D(LabelX,0))),M.Label,FCoreStyle::GetDefaultFontStyle("Regular",8),ESlateDrawEffect::None,Color);LastLabel=LabelX;}
  }return Layer+1;
 }
 virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)override{
  if(E.GetEffectingButton()!=EKeys::LeftMouseButton||!IsEnabled())return FReply::Unhandled();
  const int Marker=HitMarker(G,E);if(Marker!=INDEX_NONE){MarkerSelected.ExecuteIfBound(Marker);return FReply::Handled().SetUserFocus(SharedThis(this));}
  Capturing=true;CaptureBegin.ExecuteIfBound();Move(G,E);return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this)).PreventThrottling();
 }
 virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E)override{if(!HasMouseCapture()){const int I=HitMarker(G,E);for(const auto& M:Markers.Get(TArray<FCBMPlaybackMarker>()))if(M.Index==I){SetToolTipText(FText::FromString(FString::Printf(TEXT("%s | %.3fs | %s%s"),*M.Label,M.Seconds,M.Automatic?TEXT("자동 마커"):TEXT("수동 마커"),M.Edited?TEXT(" | 보정 있음"):TEXT(""))));return FReply::Handled();}SetToolTipText(FText::FromString(TEXT("마커 클릭: 자세 선택 | 아래 빨간 막대: 탐색")));return FReply::Unhandled();}Move(G,E);return FReply::Handled();}
 virtual FReply OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)override{
  if(E.GetEffectingButton()!=EKeys::LeftMouseButton||!HasMouseCapture())return FReply::Unhandled();Move(G,E);EndCapture();return FReply::Handled().ReleaseMouseCapture();
 }
 virtual void OnMouseCaptureLost(const FCaptureLostEvent&)override{EndCapture();}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& E)override{
  if(!IsEnabled())return FReply::Unhandled();double T=Position.Get(0.);
  if(E.GetKey()==EKeys::Left)T-=FrameDuration.Get(1./30.);
  else if(E.GetKey()==EKeys::Right)T+=FrameDuration.Get(1./30.);
  else if(E.GetKey()==EKeys::Home)T=0;
  else if(E.GetKey()==EKeys::End)T=Duration.Get(0.);
  else return FReply::Unhandled();
  CaptureBegin.ExecuteIfBound();Changed.ExecuteIfBound(FMath::Clamp(T,0.,Duration.Get(0.)));CaptureEnd.ExecuteIfBound();return FReply::Handled();
 }
private:
 TAttribute<TArray<FCBMPlaybackMarker>> Markers;FOnCBMMarkerSelected MarkerSelected;
 int HitMarker(const FGeometry& G,const FPointerEvent& E)const{const auto Local=G.AbsoluteToLocal(E.GetScreenSpacePosition());if(Local.Y>28)return INDEX_NONE;const double W=FMath::Max(1.,double(G.GetLocalSize().X)-16.);double Best=9.;int Result=INDEX_NONE;for(const auto& M:Markers.Get(TArray<FCBMPlaybackMarker>())){if(M.Seconds<0||M.Seconds>Duration.Get(0.)+.001)continue;const double D=FMath::Abs(Local.X-8.-W*M.Seconds/FMath::Max(.001,Duration.Get(0.)));if(D<Best){Best=D;Result=M.Index;}}return Result;}
 TAttribute<double> Duration,Position,FrameDuration;FOnCBMPlaybackChanged Changed;FSimpleDelegate CaptureBegin,CaptureEnd;bool Capturing=false;
 void EndCapture(){if(Capturing){Capturing=false;CaptureEnd.ExecuteIfBound();}}
 void Move(const FGeometry& G,const FPointerEvent& E){Changed.ExecuteIfBound(FMath::Clamp((G.AbsoluteToLocal(E.GetScreenSpacePosition()).X-8.)/FMath::Max(1.,double(G.GetLocalSize().X)-16.),0.,1.)*Duration.Get(0.));}
};
