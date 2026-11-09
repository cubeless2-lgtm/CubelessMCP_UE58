#pragma once
#include "CoreMinimal.h"

// Pure numeric helpers: no assets, inference, image decoding or frame resampling.
namespace CBMAlignment {
struct FObservation {
 FVector2D Relative=FVector2D::ZeroVector; // middle - root, normalized by projected limb scale
 bool Supported=false;
};
inline TArray<float> TemporalReliability(const TArray<FObservation>& Obs,int32 FPS,int32 Chains=4){
 if(Chains<1||FPS<1||Obs.Num()%Chains)return {};
 const int32 N=Obs.Num()/Chains,Radius=FMath::Max(1,FMath::RoundToInt(FPS*.1));
 TArray<float> Quality;Quality.Init(.25f,Obs.Num());
 for(int F=0;F<N;++F)for(int C=0;C<Chains;++C){
  if(!Obs[F*Chains+C].Supported)continue;
  TArray<double,TInlineAllocator<12>> Scores;
  for(int K=1;K<=Radius;++K){
   int L=F-K,R=F+K;bool Extrapolate=false;
   if(L<0){L=F+K;R=F+2*K;Extrapolate=true;}
   else if(R>=N){L=F-K;R=F-2*K;Extrapolate=true;}
   if(L<0||R<0||L>=N||R>=N||!Obs[L*Chains+C].Supported||!Obs[R*Chains+C].Supported)continue;
   const auto A=Obs[L*Chains+C].Relative,B=Obs[R*Chains+C].Relative;
   const FVector2D Pred=Extrapolate?2*A-B:(A+B)*.5;
   const double Tolerance=.035+.25*(B-A).Size(),Error=(Obs[F*Chains+C].Relative-Pred).Size();
   Scores.Add(1./(1.+FMath::Pow(Error/Tolerance,4.)));
  }
  if(Scores.Num()){Scores.Sort();const int M=Scores.Num()/2;Quality[F*Chains+C]=Scores.Num()%2?Scores[M]:(Scores[M-1]+Scores[M])*.5;}
  // A normal turning point is not an isolated detection error. Predict it from
  // the closest complete curved stencil, excluding the current observation.
  // Wider stencils are used only when nearer support is missing, not to pick
  // whichever window happens to agree with an outlier.
  for(int K=1;K<=Radius;++K){
   int Indices[4]={F-2*K,F-K,F+K,F+2*K};double Weights[4]={-1./6,2./3,2./3,-1./6};int Count=4;
   if(Indices[0]<0||Indices[3]>=N){
    const int Direction=Indices[0]<0?1:-1;Count=3;
    for(int J=0;J<3;++J)Indices[J]=F+Direction*(J+1)*K;
    Weights[0]=3;Weights[1]=-3;Weights[2]=1;
   }
   bool Supported=true;
   for(int J=0;J<Count;++J)Supported=Supported&&Indices[J]>=0&&Indices[J]<N&&Obs[Indices[J]*Chains+C].Supported;
   if(!Supported)continue;
   FVector2D Pred=FVector2D::ZeroVector;
   for(int J=0;J<Count;++J)Pred+=Weights[J]*Obs[Indices[J]*Chains+C].Relative;
   const int Inner=Count==4?1:0;
   const double Tolerance=.035+.25*(Obs[Indices[Inner+1]*Chains+C].Relative-Obs[Indices[Inner]*Chains+C].Relative).Size();
   const double Error=(Obs[F*Chains+C].Relative-Pred).Size();
   Quality[F*Chains+C]=FMath::Max(Quality[F*Chains+C],float(1./(1.+FMath::Pow(Error/Tolerance,4.))));
   break;
  }
 }
 TArray<float> Result=Quality;const int Fade=FMath::Max(1,FMath::RoundToInt(FPS*.067));
 // Only lower trust around unreliable observations; never interpolate them into confidence.
 for(int F=0;F<N;++F)for(int C=0;C<Chains;++C)for(int K=1;K<=Fade;++K){
  const double W=.5*(1+FMath::Cos(UE_PI*K/(Fade+1)));
  for(int Other:{F-K,F+K})if(Other>=0&&Other<N)Result[F*Chains+C]=FMath::Min(Result[F*Chains+C],float(1-W*(1-Quality[Other*Chains+C])));
 }
 return Result;
}
struct FMotionSample {
 FVector Axis=FVector::ForwardVector,Center=FVector::ZeroVector,U=FVector::ZeroVector,V=FVector::ZeroVector;
 FQuat Root=FQuat::Identity,Middle=FQuat::Identity;
};
struct FGuardStats {
 int32 Passes=0,FallbackPasses=0,Attenuated=0,RecoveryAccepted=0;
 double MeanGain=1,MinGain=1,MaxStepBefore=0,MaxStepAfter=0,MaxAccelerationBefore=0,MaxAccelerationAfter=0,MaxAngularAccelerationBefore=0,MaxAngularAccelerationAfter=0;
};
inline bool AttenuateMotionRegression(TArray<float>& Angle,const TArray<FMotionSample>& Samples,int32 FPS,
 FGuardStats& Stats,TFunctionRef<bool()> Cancelled,int32 Chains=4,TFunction<double(int32,float)> ImageCost={},TFunction<float(int32,float)> RecoveryLimit={}){
 if(Chains<1||Samples.Num()%Chains)return false;
 const int N=Samples.Num()/Chains;if(N<3||Angle.Num()!=Samples.Num()||FPS<1)return false;
 for(float A:Angle)if(!FMath::IsFinite(A))return false;
 const double DtScale=30./FPS,StepAllowance=1.*DtScale; // extra correction only, not original motion
 const int Radius=FMath::Max(2,FMath::RoundToInt(.16*FPS));
 TArray<double> BaseAcceleration,StepExtra,AccelerationExtra,BaseRootStep,BaseMiddleStep,BaseRootAngular,BaseMiddleAngular,AngularMargin;
 BaseAcceleration.Init(0,N*Chains);StepExtra=BaseAcceleration;AccelerationExtra=BaseAcceleration;BaseRootStep=BaseAcceleration;BaseMiddleStep=BaseAcceleration;BaseRootAngular=BaseAcceleration;BaseMiddleAngular=BaseAcceleration;AngularMargin=BaseAcceleration;
 auto Degrees=[](const FQuat& A,const FQuat& B){return FMath::RadiansToDegrees(A.AngularDistance(B));};
 auto RotationStep=[](const FQuat& A,const FQuat& B){FQuat D=(B*A.Inverse()).GetNormalized();if(D.W<0)D=D*-1.;return D.ToRotationVector()*(180./UE_PI);};
 auto AngularAcceleration=[&](const FQuat& A,const FQuat& B,const FQuat& C){return (RotationStep(B,C)-RotationStep(A,B)).Size();};
 auto AngularAllowance=[&](double Base){return FMath::Max(.5*DtScale*DtScale,.05*Base);};
 for(int F=1;F<N;++F)for(int C=0;C<Chains;++C){const int I=F*Chains+C;
  BaseRootStep[I]=Degrees(Samples[I-Chains].Root,Samples[I].Root);BaseMiddleStep[I]=Degrees(Samples[I-Chains].Middle,Samples[I].Middle);
  if(F>1){BaseRootAngular[I]=AngularAcceleration(Samples[I-2*Chains].Root,Samples[I-Chains].Root,Samples[I].Root);BaseMiddleAngular[I]=AngularAcceleration(Samples[I-2*Chains].Middle,Samples[I-Chains].Middle,Samples[I].Middle);const FVector A=Samples[I].Center+Samples[I].U,B=Samples[I-Chains].Center+Samples[I-Chains].U,D=Samples[I-2*Chains].Center+Samples[I-2*Chains].U;BaseAcceleration[I]=(A-2*B+D).Size();}
 }
 TArray<float> Gain;Gain.Init(1,Angle.Num());
 TArray<FQuat> Roots,Middles;Roots.SetNum(Angle.Num());Middles.SetNum(Angle.Num());TArray<FVector> Positions;Positions.SetNum(Angle.Num());
 auto Evaluate=[&](int First=0,int Last=-1){
  if(Last<0)Last=N-1;
  for(int I=First*Chains;I<(Last+1)*Chains;++I){const auto& S=Samples[I];const double A=Angle[I]*Gain[I];const FQuat Q(S.Axis,A);
   Roots[I]=(Q*S.Root).GetNormalized();Middles[I]=(Q*S.Middle).GetNormalized();Positions[I]=S.Center+FMath::Cos(A)*S.U+FMath::Sin(A)*S.V;}
  double MaxStep=0,MaxAcceleration=0,MaxAngular=0;
  for(int F=FMath::Max(1,First);F<=FMath::Min(N-1,Last+2);++F)for(int C=0;C<Chains;++C){const int I=F*Chains+C,P=I-Chains;
   StepExtra[I]=FMath::Max(Degrees(Roots[P],Roots[I])-BaseRootStep[I],Degrees(Middles[P],Middles[I])-BaseMiddleStep[I]);MaxStep=FMath::Max(MaxStep,StepExtra[I]);
   if(F>1){AccelerationExtra[I]=(Positions[I]-2*Positions[P]+Positions[I-2*Chains]).Size()-BaseAcceleration[I];MaxAcceleration=FMath::Max(MaxAcceleration,AccelerationExtra[I]);
    const double RootExtra=AngularAcceleration(Roots[I-2*Chains],Roots[P],Roots[I])-BaseRootAngular[I];
    const double MiddleExtra=AngularAcceleration(Middles[I-2*Chains],Middles[P],Middles[I])-BaseMiddleAngular[I];
    AngularMargin[I]=FMath::Max(RootExtra-AngularAllowance(BaseRootAngular[I]),MiddleExtra-AngularAllowance(BaseMiddleAngular[I]));
    MaxAngular=FMath::Max(MaxAngular,FMath::Max(RootExtra,MiddleExtra));
   }
  }
  Stats.MaxStepAfter=MaxStep;Stats.MaxAccelerationAfter=MaxAcceleration;Stats.MaxAngularAccelerationAfter=MaxAngular;
 };
 auto AccAllowance=[&](int I){return FMath::Max(.25*DtScale*DtScale,.05*BaseAcceleration[I]);};
 auto BadStep=[&](int I){return StepExtra[I]>StepAllowance+1.e-6;};
 auto BadAngular=[&](int I){return AngularMargin[I]>1.e-6;};
 auto BadAcceleration=[&](int I){return AccelerationExtra[I]>AccAllowance(I)+1.e-6;};
 Evaluate();Stats.MaxStepBefore=Stats.MaxStepAfter;Stats.MaxAccelerationBefore=Stats.MaxAccelerationAfter;Stats.MaxAngularAccelerationBefore=Stats.MaxAngularAccelerationAfter;
 // Small reductions avoid overshooting. A cosine neighbourhood avoids abrupt gain boundaries.
 for(int Pass=0;Pass<32;++Pass){
  if(Cancelled())return false;
  TArray<float> Factors;Factors.Init(1,Angle.Num());bool Bad=false;
  auto Fade=[&](int F,int C){for(int Other=FMath::Max(0,F-Radius);Other<=FMath::Min(N-1,F+Radius);++Other){const double W=.5*(1+FMath::Cos(UE_PI*(Other-F)/(Radius+1)));Factors[Other*Chains+C]=FMath::Min(Factors[Other*Chains+C],float(1-.15*W));}};
  for(int F=1;F<N;++F)for(int C=0;C<Chains;++C){const int I=F*Chains+C;
   if(BadStep(I)){Bad=true;Fade(F,C);Fade(F-1,C);}
   if(F>1&&(BadAcceleration(I)||BadAngular(I))){Bad=true;Fade(F,C);Fade(F-1,C);Fade(F-2,C);}
  }
  if(!Bad)break;
  for(int I=0;I<Gain.Num();++I)Gain[I]*=Factors[I];++Stats.Passes;Evaluate();
 }
 // Rare fallback reduces a whole affected chain, so it introduces no new gain boundaries.
 for(int Pass=0;Pass<18;++Pass){
  if(Cancelled())return false;
  TArray<bool> Bad;Bad.Init(false,Chains);
  for(int F=1;F<N;++F)for(int C=0;C<Chains;++C){const int I=F*Chains+C;Bad[C]=Bad[C]||BadStep(I)||(F>1&&(BadAcceleration(I)||BadAngular(I)));}
  if(!Bad.Contains(true))break;
  for(int I=0;I<Gain.Num();++I)if(Bad[I%Chains])Gain[I]=Pass==17?0.f:Gain[I]*.5f;
  ++Stats.FallbackPasses;Evaluate();
 }
 for(int F=1;F<N;++F)for(int C=0;C<Chains;++C){const int I=F*Chains+C;if(BadStep(I)||(F>1&&(BadAcceleration(I)||BadAngular(I))))return false;}
 // Recover image fit inside the feasible set, without relaxing any motion budget.
 // One coordinate changes only adjacent pairs/triples; evaluate that stencil only.
 if(ImageCost){
  for(int Pass=0;Pass<(RecoveryLimit?8:4);++Pass){
   bool Changed=false;
   for(int J=0;J<Angle.Num();++J){
    if(Cancelled())return false;
    const int I=Pass%2?Angle.Num()-1-J:J,F=I/Chains,C=I%Chains;
    const float Old=Gain[I],Limit=Pass>=4&&RecoveryLimit?RecoveryLimit(I,Angle[I]):1.f;if(!FMath::IsFinite(Limit)||Limit<1.f||Old>=Limit-.0001f||(Pass>=4&&Limit<=1.f))continue;
    const double Cost=ImageCost(I,Angle[I]*Old);if(!FMath::IsFinite(Cost))continue;
    float Step=(Limit-Old)*.5f;
    for(int Try=0;Try<8;++Try,Step*=.5f){
     const float Candidate=Old+Step;const double Next=ImageCost(I,Angle[I]*Candidate);
     if(!FMath::IsFinite(Next)||Next>=Cost-1.e-10)continue;
     Gain[I]=Candidate;Evaluate(F,F);bool Valid=true;
     for(int K=FMath::Max(1,F);K<=FMath::Min(N-1,F+2);++K){const int V=K*Chains+C;Valid=Valid&&!BadStep(V)&&!(K>1&&(BadAcceleration(V)||BadAngular(V)));}
     if(Valid){Changed=true;++Stats.RecoveryAccepted;break;}
     Gain[I]=Old;Evaluate(F,F);
    }
   }
   if(!Changed){if(RecoveryLimit&&Pass<4){Pass=3;continue;}break;}
  }
  Evaluate();
 }
 double Sum=0;for(int I=0;I<Angle.Num();++I){Sum+=Gain[I];Stats.MinGain=FMath::Min(Stats.MinGain,double(Gain[I]));Stats.Attenuated+=Gain[I]<.999f;Angle[I]*=Gain[I];}Stats.MeanGain=Sum/Gain.Num();
 return true;
}
}
