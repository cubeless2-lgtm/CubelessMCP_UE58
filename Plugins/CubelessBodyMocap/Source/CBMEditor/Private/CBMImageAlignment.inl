#include "CBMAlignmentSafety.h"
#include "CBMTargetPlacement.inl"
#include "CBMKneePlaneRefinement.inl"
#include "CBMAnkleTwistRefinement.inl"
// Fits residual limb-plane rotation in the ACTUAL target skeleton after IK retargeting.
// Plane fitting preserves endpoints and bend angles; optional clip placement below
// preserves contact velocities/heights, bone lengths, endpoint rotations and finger locals.
// This is an image-conditioned pose prior, not a joint-angle clamp or new model inference.
static bool CBMAlignTargetLimbPlanes(UAnimSequence* Anim, USkeletalMesh* Mesh,
 const FSolveData& D, const TArray<float>& Camera, const FVector& Origin,
 float Focal, FVector2f ImageCenter, int32 FPS, FString& Report)
{
 check(IsInGameThread());
 auto Skip=[&](const TCHAR* Why){Report=FString::Printf(TEXT("{\"version\":12,\"applied\":false,\"reason\":\"%s\"}"),Why);return true;};
 const auto* Model=Anim->GetDataModel(); const int32 N=Model->GetNumberOfKeys();
 if(N<3||Camera.Num()!=12||D.Keypoints.Num()!=N*75||D.ValidFrame.Num()!=N)
  return Skip(TEXT("Missing calibrated camera or matching observations"));
 const auto& Ref=Mesh->GetRefSkeleton(); const int32 BCount=Ref.GetNum();
 const TCHAR* Names[4][3]={{TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l")},
  {TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r")},
  {TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l")},
  {TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")}};
 const int32 Keys[]={13,10,6,3}; int32 Ids[4][3];
 for(int C=0;C<4;++C){
  for(int K=0;K<3;++K){Ids[C][K]=Ref.FindBoneIndex(Names[C][K]);
   if(Ids[C][K]==INDEX_NONE||!Model->IsValidBoneTrackName(Names[C][K]))return Skip(TEXT("Target limb tracks unavailable"));}
  if(Ref.GetParentIndex(Ids[C][1])!=Ids[C][0]||Ref.GetParentIndex(Ids[C][2])!=Ids[C][1]||Ref.GetParentIndex(Ids[C][0])<0)
   return Skip(TEXT("Target limb hierarchy differs from a direct two-bone chain"));
 }
 TArray<TArray<FTransform>> Tracks; Tracks.SetNum(BCount);
 for(int B=0;B<BCount;++B)if(Model->IsValidBoneTrackName(Ref.GetBoneName(B)))Model->GetBoneTrackTransforms(Ref.GetBoneName(B),Tracks[B]);
 auto Pose=[&](int F,TArray<FTransform>& Local,TArray<FTransform>& Global){
  Local=Ref.GetRefBonePose(); for(int B=0;B<BCount;++B)if(Tracks[B].Num())Local[B]=Tracks[B][FMath::Min(F,Tracks[B].Num()-1)];
  Global=Local; for(int B=1;B<BCount;++B)Global[B]=Global[B]*Global[Ref.GetParentIndex(B)];
 };
 Eigen::Matrix3f Cam; Eigen::Vector3f CamT;
 for(int I=0;I<9;++I)Cam(I/3,I%3)=Camera[I]; for(int I=0;I<3;++I)CamT[I]=Camera[I+9];
 auto ToWorld=[&](FVector P){P=FMetaHumanSMPLX::UE2SMPL(P)+Origin;return Eigen::Vector3f(P.X,P.Y,P.Z);};
 auto ToVector=[](FVector P){P=FMetaHumanSMPLX::UE2SMPL(P);return Eigen::Vector3f(P.X,P.Y,P.Z);};
 struct Sample{Eigen::Vector3f Center,U,V; FVector Axis; float X,Y,Scale,Weight;};
 TArray<Sample> Samples; Samples.SetNum(N*4);
 TArray<CBMAlignment::FMotionSample> Motion;Motion.SetNum(N*4);TArray<CBMAlignment::FObservation> Observations;Observations.SetNum(N*4);
 TArray<FTransform> Local,Global;
 for(int F=0;F<N;++F){
  if(D.ShouldCancel&&D.ShouldCancel())return false;
  Pose(F,Local,Global);
  for(int C=0;C<4;++C){
   const int A=Ids[C][0],B=Ids[C][1],E=Ids[C][2];
   if(!Global[A].GetScale3D().Equals(FVector::OneVector,.0001)||!Global[B].GetScale3D().Equals(FVector::OneVector,.0001))
    return Skip(TEXT("Non-unit target limb scale is unsupported"));
   auto& S=Samples[F*4+C];const FVector Start=Global[A].GetTranslation(),Mid=Global[B].GetTranslation(),End=Global[E].GetTranslation();
   S.Axis=(End-Start).GetSafeNormal();if(S.Axis.IsNearlyZero())S.Axis=FVector::ForwardVector;const FVector Center=Start+S.Axis*FVector::DotProduct(Mid-Start,S.Axis),U=Mid-Center,V=FVector::CrossProduct(S.Axis,U);
   S.Center=Cam*ToWorld(Center)+CamT;S.U=Cam*ToVector(U);S.V=Cam*ToVector(V);
   const int K=F*75+Keys[C]*3;S.X=D.Keypoints[K]-ImageCenter.X;S.Y=D.Keypoints[K+1]-ImageCenter.Y;
   const float Depth=(Cam*ToWorld(Start)+CamT).z();
   S.Scale=FMath::Max(40.f,float((End-Start).Size()*.01)*Focal/FMath::Max(.01f,Depth));
   const float W=FMath::Clamp((D.Keypoints[K+2]-.65f)/.3f,0.f,1.f);
   S.Weight=D.ValidFrame[F]&&U.Size()>2.5&&Depth>.01f?W*W:0.f;
   auto& M=Motion[F*4+C];M.Axis=S.Axis;M.Center=Center;M.U=U;M.V=V;M.Root=Global[A].GetRotation().GetNormalized();M.Middle=Global[B].GetRotation().GetNormalized();
   const int RootK=F*75+(Keys[C]-1)*3;auto& O=Observations[F*4+C];O.Relative=FVector2D(D.Keypoints[K]-D.Keypoints[RootK],D.Keypoints[K+1]-D.Keypoints[RootK+1])/S.Scale;O.Supported=D.ValidFrame[F]&&D.Keypoints[K+2]>.5f&&D.Keypoints[RootK+2]>.5f;

  }
 }
 const TArray<float> Reliability=CBMAlignment::TemporalReliability(Observations,FPS);double ReliabilitySum=0;int LowReliability=0;
 for(int I=0;I<Samples.Num();++I){Samples[I].Weight*=Reliability[I];ReliabilitySum+=Reliability[I];LowReliability+=Reliability[I]<.5f;}
 TArray<float> Angle,Moment,Variance,Gradient;Angle.Init(0,N*4);Moment=Angle;Variance=Angle;Gradient=Angle;
 float Beta1=1,Beta2=1;const float TimeScale=float(FPS)/30.f;
 for(int Step=0;Step<200;++Step){
  if(D.ShouldCancel&&D.ShouldCancel())return false;
  for(int I=0;I<Angle.Num();++I){
   const auto& S=Samples[I];const float A=Angle[I],C=FMath::Cos(A),Sn=FMath::Sin(A);
   const Eigen::Vector3f Q=S.Center+C*S.U+Sn*S.V,J=-Sn*S.U+C*S.V;float G=0;
   if(Q.z()>.01f){const float EX=Focal*Q.x()/Q.z()-S.X,EY=Focal*Q.y()/Q.z()-S.Y;
    const float DX=Focal*(J.x()*Q.z()-Q.x()*J.z())/FMath::Square(Q.z()),DY=Focal*(J.y()*Q.z()-Q.y()*J.z())/FMath::Square(Q.z());
    G=2*S.Weight*(EX*DX+EY*DY)/FMath::Square(S.Scale);}
   Gradient[I]=(G+.16f*A)/Angle.Num();
  }
  // Smooth the WORLD residual rotation vector, including its moving axis.
  // Scalar angle smoothing alone can amplify turns when the limb axis changes.
  for(int F=1;F<N;++F)for(int C=0;C<4;++C){const int I=F*4+C;const FVector DQ=Angle[I]*Samples[I].Axis-Angle[I-4]*Samples[I-4].Axis;const float W=8*FMath::Square(TimeScale)/((N-1)*4);Gradient[I]+=W*FVector::DotProduct(DQ,Samples[I].Axis);Gradient[I-4]-=W*FVector::DotProduct(DQ,Samples[I-4].Axis);}
  for(int F=2;F<N;++F)for(int C=0;C<4;++C){const int I=F*4+C;const FVector DQ=Angle[I]*Samples[I].Axis-2*Angle[I-4]*Samples[I-4].Axis+Angle[I-8]*Samples[I-8].Axis;const float W=32*FMath::Pow(TimeScale,4)/((N-2)*4);Gradient[I]+=W*FVector::DotProduct(DQ,Samples[I].Axis);Gradient[I-4]-=2*W*FVector::DotProduct(DQ,Samples[I-4].Axis);Gradient[I-8]+=W*FVector::DotProduct(DQ,Samples[I-8].Axis);}
  Beta1*=.9f;Beta2*=.999f;
  for(int I=0;I<Angle.Num();++I){Moment[I]=.9f*Moment[I]+.1f*Gradient[I];Variance[I]=.999f*Variance[I]+.001f*FMath::Square(Gradient[I]);Angle[I]-=.015f*(Moment[I]/(1-Beta1))/(FMath::Sqrt(Variance[I]/(1-Beta2))+1.e-8f);}
 }
 CBMAlignment::FGuardStats Guard;
 if(!CBMAlignment::AttenuateMotionRegression(Angle,Motion,FPS,Guard,[&](){return D.ShouldCancel&&D.ShouldCancel();},4,[&](int I,float A){const auto& S=Samples[I];const Eigen::Vector3f P=S.Center+FMath::Cos(A)*S.U+FMath::Sin(A)*S.V;if(P.z()<=.01f)return TNumericLimits<double>::Max();return double(S.Weight)*(FMath::Square(Focal*P.x()/P.z()-S.X)+FMath::Square(Focal*P.y()/P.z()-S.Y))/FMath::Square(S.Scale);},[](int I,float A){return I%4<2?1.f+FMath::Min(.5f,FMath::DegreesToRadians(5.f)/FMath::Max(1.e-6f,FMath::Abs(A))):1.f;})){Report=TEXT("Cancelled or motion-regression guard failed; no tracks written.");return false;}
 TArray<TArray<FQuat4f>> Rot;Rot.SetNum(8);for(auto& R:Rot)R.Reserve(N);
 double MaxEndCM=0,MaxEndAngle=0,Before=0,After=0;int EvidenceCount=0;
 for(int F=0;F<N;++F){
  if(D.ShouldCancel&&D.ShouldCancel())return false;
  Pose(F,Local,Global);TArray<FTransform> NewLocal=Local,NewGlobal;
  for(int C=0;C<4;++C){const int A=Ids[C][0],B=Ids[C][1],E=Ids[C][2];const auto& S=Samples[F*4+C];
   const FQuat Q(S.Axis,double(Angle[F*4+C]));
   const FQuat RA=(Global[Ref.GetParentIndex(A)].GetRotation().Inverse()*Q*Global[A].GetRotation()).GetNormalized();
   const FQuat RE=((Q*Global[B].GetRotation()).Inverse()*Global[E].GetRotation()).GetNormalized();
   Rot[C*2].Add(FQuat4f(RA));Rot[C*2+1].Add(FQuat4f(RE));
   NewLocal[A].SetRotation(FQuat(Rot[C*2].Last()));NewLocal[E].SetRotation(FQuat(Rot[C*2+1].Last()));
   if(S.Weight>0){auto Error=[&](float T){const Eigen::Vector3f P=S.Center+FMath::Cos(T)*S.U+FMath::Sin(T)*S.V;return P.z()>.01f?double(FMath::Sqrt(FMath::Square(Focal*P.x()/P.z()-S.X)+FMath::Square(Focal*P.y()/P.z()-S.Y))):0.;};Before+=Error(0);After+=Error(Angle[F*4+C]);++EvidenceCount;}
  }
  NewGlobal=NewLocal;for(int B=1;B<BCount;++B)NewGlobal[B]=NewGlobal[B]*NewGlobal[Ref.GetParentIndex(B)];
  for(int C=0;C<4;++C){const int E=Ids[C][2];MaxEndCM=FMath::Max(MaxEndCM,FVector::Distance(NewGlobal[E].GetTranslation(),Global[E].GetTranslation()));MaxEndAngle=FMath::Max(MaxEndAngle,NewGlobal[E].GetRotation().GetNormalized().AngularDistance(Global[E].GetRotation().GetNormalized()));}
 }
 if(!FMath::IsFinite(MaxEndCM)||!FMath::IsFinite(MaxEndAngle)||MaxEndCM>.001||MaxEndAngle>.00001){Report=TEXT("Target limb alignment preservation check failed; no tracks written.");return false;}
 auto& Controller=Anim->GetController();Controller.OpenBracket(FText::FromString(TEXT("Video limb direction alignment")),false);bool OK=true;
 for(int C=0;C<4;++C)for(int K=0;K<2;++K){const int B=Ids[C][K?2:0];TArray<FVector3f> P,S;P.Reserve(N);S.Reserve(N);
  for(int F=0;F<N;++F){const auto& T=Tracks[B][FMath::Min(F,Tracks[B].Num()-1)];P.Add(FVector3f(T.GetTranslation()));S.Add(FVector3f(T.GetScale3D()));}
  OK=Controller.SetBoneTrackKeys(Ref.GetBoneName(B),P,Rot[C*2+K],S,false)&&OK;
 }
 Controller.CloseBracket(false);
 Report=FString::Printf(TEXT("{\"version\":12,\"applied\":%s,\"space\":\"target_after_ik\",\"frames\":%d,\"evidence_samples\":%d,\"mean_evidence_error_before_px\":%.6f,\"mean_evidence_error_after_px\":%.6f,\"max_endpoint_position_change_cm\":%.9f,\"max_endpoint_rotation_change_degrees\":%.9f,\"original_finger_locals_preserved\":true,\"mean_temporal_reliability\":%.6f,\"low_reliability_samples\":%d,\"guard_passes\":%d,\"guard_fallback_passes\":%d,\"attenuated_samples\":%d,\"mean_guard_gain\":%.6f,\"min_guard_gain\":%.6f,\"max_added_rotation_step_before_guard_degrees\":%.6f,\"max_added_rotation_step_after_guard_degrees\":%.6f,\"max_added_acceleration_before_guard_cm_frame2\":%.6f,\"max_added_acceleration_after_guard_cm_frame2\":%.6f,\"max_added_angular_acceleration_before_guard_degrees_frame2\":%.6f,\"max_added_angular_acceleration_after_guard_degrees_frame2\":%.6f}"),OK?TEXT("true"):TEXT("false"),N,EvidenceCount,Before/FMath::Max(1,EvidenceCount),After/FMath::Max(1,EvidenceCount),MaxEndCM,FMath::RadiansToDegrees(MaxEndAngle),ReliabilitySum/Reliability.Num(),LowReliability,Guard.Passes,Guard.FallbackPasses,Guard.Attenuated,Guard.MeanGain,Guard.MinGain,Guard.MaxStepBefore,Guard.MaxStepAfter,Guard.MaxAccelerationBefore,Guard.MaxAccelerationAfter,Guard.MaxAngularAccelerationBefore,Guard.MaxAngularAccelerationAfter);
 FString Placement;if(OK)OK=CBMAlignTargetPlacement(Anim,Mesh,D,Camera,Origin,Focal,ImageCenter,FPS,Tracks,Placement);
 if(!Placement.IsEmpty()){Report.LeftChopInline(1);Report+=TEXT(",\"placement\":")+Placement+TEXT("}");}
 FString Knees;if(OK)OK=CBMRefineTargetKnees(Anim,Mesh,D,Camera,Origin,Focal,ImageCenter,FPS,Tracks,Knees);
 if(!Knees.IsEmpty()){Report.LeftChopInline(1);Report+=TEXT(",\"final_knee_plane\":")+Knees+TEXT("}");}
 FString Twist;if(OK)OK=CBMRefineAddedLegTwist(Anim,Mesh,D,Camera,Origin,Focal,ImageCenter,FPS,Tracks,Twist);
 if(!Twist.IsEmpty()){Report.LeftChopInline(1);Report+=TEXT(",\"added_leg_twist\":")+Twist+TEXT("}");}
 return OK;
}
