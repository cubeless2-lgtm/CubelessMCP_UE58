#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Async/Future.h"
class USkeletalMeshComponent;
class UAnimSequence;
class SCBMDebugOverlay : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SCBMDebugOverlay){} SLATE_END_ARGS()
 void Construct(const FArguments&){}
 void Update(bool Enabled,UAnimSequence* Anim,USkeletalMeshComponent* Mesh,const FString& Video,double Start,double Time,bool Synchronized);
 FString Status;
 virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(1,1);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
private:
 struct FData {bool Valid=false;FString Video,Stamp;double Size=0,Start=0,FPS=0;int32 Frames=0;TArray<float> Camera,Points;};
 TFuture<TSharedPtr<FData>> Pending;
 TSharedPtr<FData> Data;
 FString Key;
 bool Show=false;
 FVector2D Canvas;
 TArray<FVector3f> Observed,Model,Applied;
};
