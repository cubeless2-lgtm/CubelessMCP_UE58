#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/SlateBrush.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "CBMPreviewClock.h"
class UCubelessMocapSettings;
class UMediaPlayer;
class UMediaTexture;
class SCBMAnimationViewport;
class SCBMDebugOverlay;
class SCBMComparison : public SCompoundWidget {
public:
 bool PrepareScreenshot();
 SLATE_BEGIN_ARGS(SCBMComparison){} SLATE_ARGUMENT(UCubelessMocapSettings*,Settings) SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 void LoadPreview(){Load();}
 virtual ~SCBMComparison();
 virtual void Tick(const FGeometry&,double,float) override;
private:
 UCubelessMocapSettings* Settings=nullptr;
 TStrongObjectPtr<UMediaPlayer> Player;
 TStrongObjectPtr<UMediaTexture> Texture;
 FSlateBrush Brush;
 TSharedPtr<SCBMAnimationViewport> View;
 TSharedPtr<SCBMDebugOverlay> DebugOverlay;
 FString OpenedFile,Notice;
 float Aspect=490.f/750.f;
 FCBMPreviewClock Clock;
 double Length=0;
 bool PlayAfterSeek=false;
 bool Waiting=false;
 FReply Load();
 void Seek(double Time);
};
