#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/SlateBrush.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "CBMPreviewClock.h"
class UCubelessMocapSettings;
class UAnimSequence;
class UMediaPlayer;
class UMediaTexture;
class SCBMAnimationViewport;
class SCBMDebugOverlay;
class FJsonObject;
class SSplitter;
class SCBMComparison : public SCompoundWidget {
public:
 bool PrepareScreenshot();
 SLATE_BEGIN_ARGS(SCBMComparison){} SLATE_ARGUMENT(UCubelessMocapSettings*,Settings) SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 void LoadPreview(){Load();}
 void OpenSourceVideo();
 bool ShowResult(bool PreserveTime=false);
 bool Control(const FString& Action,double Value,FString& Message);
 bool EditRange(double Start,double End,FString& Message);
 TSharedRef<FJsonObject> ReadState()const;
 void SetVideoFraction(float Value);
 virtual ~SCBMComparison();
 virtual void Tick(const FGeometry&,double,float) override;
private:
 UCubelessMocapSettings* Settings=nullptr;
 TStrongObjectPtr<UMediaPlayer> Player;
 TStrongObjectPtr<UMediaTexture> Texture;
 FSlateBrush Brush;
 TSharedPtr<SCBMAnimationViewport> View;
 TSharedPtr<SCBMDebugOverlay> DebugOverlay;
 TSharedPtr<SSplitter> VideoSplitter;
 FString OpenedFile,Notice;
 float Aspect=490.f/750.f;
 FCBMPreviewClock Clock;
 double Length=0;
 double PlaybackStart=0,ResultStart=0,ResultDuration=0;
 bool ReviewSource=true;
 double SourceDuration()const;
 double SourceFPS()const;
 UAnimSequence* DisplayAnimation()const;
 bool ReviewVideo();
 bool StepFrame(int32 Direction);
 void SetRange(double Start,double End);
 bool CanEditRange()const;
 bool PlayAfterSeek=false;
 bool Waiting=false;
 bool InferenceRenderingSuspended=false;
 double SeekBegan=0,SeekTarget=0,SeekError=0;
 int32 SeekRetries=0;
 bool SeekFailed=false,SeekExact=true;
 TArray<FVector3f> ViewPoints;
 void UpdateDebugProjection(double Time,bool Synchronized);
 FReply Load();
 bool Seek(double Time);
};
