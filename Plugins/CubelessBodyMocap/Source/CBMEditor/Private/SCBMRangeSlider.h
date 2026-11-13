#pragma once
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"

DECLARE_DELEGATE_TwoParams(FOnCBMRangeChanged, double, double);

// Selection only. The pipeline still consumes StartSeconds/DurationSeconds.
class SCBMRangeSlider : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SCBMRangeSlider){}
  SLATE_ATTRIBUTE(double, Duration)
  SLATE_ATTRIBUTE(double, Start)
  SLATE_ATTRIBUTE(double, End)
  SLATE_EVENT(FOnCBMRangeChanged, OnChanged)
 SLATE_END_ARGS()
 void Construct(const FArguments& A){Duration=A._Duration;Start=A._Start;End=A._End;Changed=A._OnChanged;}
 virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(240,28);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool Enabled) const override{
  const FVector2D Size=G.GetLocalSize();const double W=FMath::Max(1.,double(Size.X)-16.);const double D=FMath::Max(.001,Duration.Get(0.));
  auto Box=[&](double X,double Y,double Width,double Height,FLinearColor C){FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(Width,Height),FSlateLayoutTransform(FVector2D(X,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Enabled?C:C*.4f);};
  const double A=8.+W*FMath::Clamp(Start.Get(0.)/D,0.,1.),B=8.+W*FMath::Clamp(End.Get(D)/D,0.,1.);
  Box(8,10,W,8,FLinearColor(.08f,.09f,.1f));Box(A,10,FMath::Max(0.,B-A),8,FLinearColor(.05f,.32f,.5f));
  Box(A-4,3,8,22,FLinearColor(.15f,.65f,.9f));Box(B-4,3,8,22,FLinearColor(.15f,.65f,.9f));return Layer;
 }
 virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override{
  if(E.GetEffectingButton()!=EKeys::LeftMouseButton||!IsEnabled())return FReply::Unhandled();
  const double T=TimeAt(G,E);DraggingStart=FMath::Abs(T-Start.Get(0.))<=FMath::Abs(T-End.Get(0.));Move(G,E);return FReply::Handled().CaptureMouse(SharedThis(this));
 }
 virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E) override{if(!HasMouseCapture())return FReply::Unhandled();Move(G,E);return FReply::Handled();}
 virtual FReply OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E) override{if(E.GetEffectingButton()!=EKeys::LeftMouseButton||!HasMouseCapture())return FReply::Unhandled();Move(G,E);return FReply::Handled().ReleaseMouseCapture();}
private:
 TAttribute<double> Duration,Start,End;FOnCBMRangeChanged Changed;bool DraggingStart=true;
 double TimeAt(const FGeometry& G,const FPointerEvent& E)const{return FMath::Clamp((G.AbsoluteToLocal(E.GetScreenSpacePosition()).X-8.)/FMath::Max(1.,double(G.GetLocalSize().X)-16.),0.,1.)*Duration.Get(0.);}
 void Move(const FGeometry& G,const FPointerEvent& E){const double T=TimeAt(G,E);Changed.ExecuteIfBound(DraggingStart?T:Start.Get(0.),DraggingStart?End.Get(0.):T);}
};
