#pragma once
#include "CoreMinimal.h"

// UI selection bounds, in source seconds; End is exclusive. No solver state.
struct FCBMClipRange {
 static FVector2D Normalize(double Start,double End,double OldStart,double Duration,int32 FPS){
  const double F=FMath::Max(FPS,1),Min=FMath::CeilToDouble(.1*F)/F;
  if(!FMath::IsFinite(Start)||!FMath::IsFinite(End)||!FMath::IsFinite(Duration))return FVector2D(0,0);
  Duration=FMath::FloorToDouble(Duration*F+1.e-6)/F;
  if(Duration<Min)return FVector2D(0,0);
  Start=FMath::RoundToDouble(Start*F)/F;End=FMath::RoundToDouble(End*F)/F;
  if(!FMath::IsNearlyEqual(Start,OldStart,1.e-5)){End=FMath::Clamp(End,Min,Duration);Start=FMath::Clamp(Start,FMath::Max(0.,End-120.),End-Min);}
  else{Start=FMath::Clamp(Start,0.,Duration-Min);End=FMath::Clamp(End,Start+Min,FMath::Min(Duration,Start+120.));}
  return FVector2D(Start,End);
 }
};
