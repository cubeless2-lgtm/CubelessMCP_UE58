#pragma once
#include "CoreMinimal.h"

// A delayed media seek must not overwrite the user's latest scrub position.
struct FCBMPreviewClock {
 double Position=0;
 bool Scrubbing=false,Pending=false;
 bool Advancing=false;
 double LastMedia=0;
 void Request(double Time,double Length){Position=FMath::Clamp(Time,0.,Length);Pending=true;Advancing=false;}
 void Observe(double MediaTime,double Length,double Tolerance){
  if(Scrubbing)return;
  if(Pending&&FMath::Abs(MediaTime-Position)>Tolerance)return;
  Pending=false;Position=FMath::Clamp(MediaTime,0.,Length);
 }
 void Advance(double MediaTime,double Length,double FrameDuration,double Delta,bool Playing){
  const double Frame=FMath::Clamp(FrameDuration,1./240.,1.);
  if(!Playing||Scrubbing||Pending){Observe(MediaTime,Length,Frame*.5);Advancing=false;LastMedia=MediaTime;return;}
  if(!Advancing||MediaTime<LastMedia-Frame*.5){Position=FMath::Clamp(MediaTime,0.,Length);Advancing=true;}
  else{
   // Some media clocks expose frame timestamps. Fill the interval between them
   // without running more than one source frame ahead or reversing on a late tick.
   Position=FMath::Clamp(FMath::Min(MediaTime+Frame,FMath::Max(MediaTime,Position+FMath::Max(Delta,0.))),0.,Length);
  }
  LastMedia=MediaTime;
 }
 void Reset(){Position=0;Scrubbing=false;Pending=false;Advancing=false;LastMedia=0;}
};
