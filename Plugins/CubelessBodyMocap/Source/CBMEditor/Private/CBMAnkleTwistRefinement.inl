#include "CBMLegTwist.h"

// Reduce roll introduced by image alignment, not the source motion's ankle angle.
// Rotate the complete leg plane: unwinding only the calf would move the same
// unwanted rotation into the knee. Pelvis, knee-local rotation and feet stay fixed.
static bool CBMRefineAddedLegTwist(UAnimSequence* Anim, USkeletalMesh* Mesh,
 const FSolveData& D, const TArray<float>& Camera, const FVector& Origin,
 float Focal, FVector2f ImageCenter, int FPS,
 const TArray<TArray<FTransform>>& BeforeTracks, FString& Report)
{
 const auto* Model=Anim->GetDataModel();
 const auto& Ref=Mesh->GetRefSkeleton();
 const int N=Model->GetNumberOfKeys(), NB=Ref.GetNum();
 const TCHAR* Names[2][3]={{TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l")},
                          {TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r")}};
 const int Keys[2]={12,9}; int Id[2][3];
 for(int C=0;C<2;++C)for(int K=0;K<3;++K){
  Id[C][K]=Ref.FindBoneIndex(Names[C][K]);
  if(Id[C][K]<0){Report=TEXT("{\"applied\":false,\"reason\":\"missing leg bones\"}");return true;}
 }
 TArray<TArray<FTransform>> Tracks; Tracks.SetNum(NB);
 for(int B=0;B<NB;++B)if(Model->IsValidBoneTrackName(Ref.GetBoneName(B)))
  Model->GetBoneTrackTransforms(Ref.GetBoneName(B),Tracks[B]);
 auto Pose=[&](const TArray<TArray<FTransform>>& T,int F,TArray<FTransform>& L,TArray<FTransform>& G){
  L=Ref.GetRefBonePose();
  for(int B=0;B<NB;++B)if(T[B].Num())L[B]=T[B][FMath::Min(F,T[B].Num()-1)];
  G=L;for(int B=1;B<NB;++B)G[B]=G[B]*G[Ref.GetParentIndex(B)];
 };
 auto Project=[&](FVector P){
  P=FMetaHumanSMPLX::UE2SMPL(P)+Origin;
  const FVector V(Camera[0]*P.X+Camera[1]*P.Y+Camera[2]*P.Z+Camera[9],
                  Camera[3]*P.X+Camera[4]*P.Y+Camera[5]*P.Z+Camera[10],
                  Camera[6]*P.X+Camera[7]*P.Y+Camera[8]*P.Z+Camera[11]);
  return FVector2D(Focal*V.X/FMath::Max(.01,V.Z)+ImageCenter.X,
                   Focal*V.Y/FMath::Max(.01,V.Z)+ImageCenter.Y);
 };
 auto DirectionError=[](FVector2D A,FVector2D B){
  return FMath::Acos(FMath::Clamp(FVector2D::DotProduct(A,B)/
   FMath::Max(1.e-10,A.Size()*B.Size()),-1.,1.));
 };
 auto Step=[](FQuat A,FQuat B){
  FQuat Q=(B*A.Inverse()).GetNormalized();if(Q.W<0)Q=Q*-1.;
  return Q.ToRotationVector()*(180./UE_PI);
 };
 struct FSample {
  FVector Axis,Center,U,V,End,SourceAxis;
  FQuat InitialRoot,InitialMiddle,Foot,SourceMiddle;
  FVector2D HipUV,Target,Direction;
  double Twist=0,PointLimit=0,DirectionLimit=0;
  bool Supported=false;
 };
 TArray<FSample> Samples;Samples.SetNum(N*2);
 TArray<FVector> Position,SourcePosition;Position.SetNum(N*2);SourcePosition.SetNum(N*2);
 TArray<FQuat> Rotation[3],SourceRotation[2];
 for(auto& Q:Rotation)Q.SetNum(N*2);for(auto& Q:SourceRotation)Q.SetNum(N*2);
 TArray<double> Angles;Angles.Init(0,N*2);
 TArray<FTransform> L,G,SL,SG;
 double BeforeTwist=0,AfterTwist=0,MaxBefore=0,MaxAfter=0;int Evidence=0;
 for(int F=0;F<N;++F){
  if(D.ShouldCancel&&D.ShouldCancel())return false;
  Pose(Tracks,F,L,G);Pose(BeforeTracks,F,SL,SG);
  for(int C=0;C<2;++C){
   const int I=F*2+C,H=Id[C][0],K=Id[C][1],E=Id[C][2],O=F*75+Keys[C]*3;
   auto& S=Samples[I];const FVector Hip=G[H].GetTranslation(),Knee=G[K].GetTranslation();
   S.End=G[E].GetTranslation();S.Axis=(S.End-Hip).GetSafeNormal();
   S.Center=Hip+S.Axis*FVector::DotProduct(Knee-Hip,S.Axis);
   S.U=Knee-S.Center;S.V=FVector::CrossProduct(S.Axis,S.U);
   S.InitialRoot=G[H].GetRotation().GetNormalized();S.InitialMiddle=G[K].GetRotation().GetNormalized();
   S.Foot=G[E].GetRotation().GetNormalized();S.SourceMiddle=SG[K].GetRotation().GetNormalized();
   S.SourceAxis=SG[E].GetTranslation()-SG[K].GetTranslation();
   S.HipUV=Project(Hip);S.Target=FVector2D(D.Keypoints[O+3],D.Keypoints[O+4]);
   S.Direction=S.Target-FVector2D(D.Keypoints[O],D.Keypoints[O+1]);
   const double Point=(Project(Knee)-S.Target).Size();
   const double Direction=DirectionError(Project(Knee)-S.HipUV,S.Direction);
   const double SourcePoint=(Project(SG[K].GetTranslation())-S.Target).Size();
   const double SourceDirection=DirectionError(Project(SG[K].GetTranslation())-Project(SG[H].GetTranslation()),S.Direction);
   // Retain at least 80% of the achieved evidence improvement at each frame.
   // The raster-scaled pixel and direction budgets are image-fidelity tolerances,
   // not anatomical limits. Source motion and naturally turned feet are untouched.
   S.PointLimit=Point+FMath::Min(.005*ImageCenter.Y,.2*FMath::Max(0.,SourcePoint-Point));
   S.DirectionLimit=Direction+FMath::Min(FMath::DegreesToRadians(1.),.2*FMath::Max(0.,SourceDirection-Direction));
   S.Supported=D.ValidFrame[F]&&D.Keypoints[O+2]>.7&&D.Keypoints[O+5]>.7&&
    S.Direction.Size()>10&&S.U.Size()>2.5&&!S.Axis.IsNearlyZero()&&
    CBMLegTwist::MeasureAddedTwist(S.SourceMiddle,S.SourceAxis,S.InitialMiddle,S.End-Knee,S.Twist);
   Position[I]=Knee;SourcePosition[I]=SG[K].GetTranslation();
   Rotation[0][I]=S.InitialRoot;Rotation[1][I]=S.InitialMiddle;
   Rotation[2][I]=(S.InitialMiddle.Inverse()*S.Foot).GetNormalized();
   SourceRotation[0][I]=SG[H].GetRotation().GetNormalized();SourceRotation[1][I]=S.SourceMiddle;
   if(S.Supported){BeforeTwist+=FMath::Abs(S.Twist);MaxBefore=FMath::Max(MaxBefore,FMath::Abs(S.Twist));++Evidence;}
  }
 }
 auto Set=[&](int I,double A){
  const auto& S=Samples[I];const FQuat Q(S.Axis,A);
  Position[I]=S.Center+FMath::Cos(A)*S.U+FMath::Sin(A)*S.V;
  Rotation[0][I]=(Q*S.InitialRoot).GetNormalized();Rotation[1][I]=(Q*S.InitialMiddle).GetNormalized();
  Rotation[2][I]=(Rotation[1][I].Inverse()*S.Foot).GetNormalized();Angles[I]=A;
 };
 auto Twist=[&](int I,double& T){const auto& S=Samples[I];
  return CBMLegTwist::MeasureAddedTwist(S.SourceMiddle,S.SourceAxis,Rotation[1][I],S.End-Position[I],T);
 };
 // Compare proposals with the same normalized FK construction, including zero.
 for(int I=0;I<N*2;++I)Set(I,0.);
 // Preserve achieved clip peaks and local window energy (including its boundary)
 // for thigh, calf AND ankle-local motion. Existing source-relative guards remain.
 double Peak[2][11]={};const double Time=30./FPS;
 auto Values=[&](int F,int C,double* Out){
  for(int M=0;M<11;++M)Out[M]=0.;const int I=F*2+C;
  if(F<1)return;
  for(int B=0;B<3;++B){const auto& Q=Rotation[B];const FVector V=Step(Q[I-2],Q[I]);Out[B*3]=V.SizeSquared();
   if(F>1){const FVector A=V-Step(Q[I-4],Q[I-2]);Out[B*3+1]=A.SizeSquared();
    if(F>2)Out[B*3+2]=(A-Step(Q[I-4],Q[I-2])+Step(Q[I-6],Q[I-4])).SizeSquared();}}
  if(F>1)Out[9]=(Position[I]-2*Position[I-2]+Position[I-4]).SizeSquared();
  if(F>2)Out[10]=(Position[I]-3*Position[I-2]+3*Position[I-4]-Position[I-6]).SizeSquared();
 };
 struct FLimits {double V[5]={};};TArray<FLimits> Limits;Limits.SetNum(N*2);
 for(int F=1;F<N;++F)for(int C=0;C<2;++C){
  const int I=F*2+C;double V[11];Values(F,C,V);
  for(int M=0;M<11;++M)Peak[C][M]=FMath::Max(Peak[C][M],V[M]);
  // Earlier stages are written as float tracks. Do not reject an unchanged pose
  // merely because that round-trip already crossed a source guard by an epsilon.
  // Each allowance inherits at most the existing transition, never a new budget.
  for(int B=0;B<2;++B){const auto& Q=SourceRotation[B];
   Limits[I].V[B*2]=FMath::Max(FMath::Sqrt(V[B*3]),Step(Q[I-2],Q[I]).Size()+Time);
   if(F>1){const double A=(Step(Q[I-2],Q[I])-Step(Q[I-4],Q[I-2])).Size();
    Limits[I].V[B*2+1]=FMath::Max(FMath::Sqrt(V[B*3+1]),A+FMath::Max(.5*Time*Time,.05*A));}
  }
  if(F>1){const double A=(SourcePosition[I]-2*SourcePosition[I-2]+SourcePosition[I-4]).Size();
   Limits[I].V[4]=FMath::Max(FMath::Sqrt(V[9]),A+FMath::Max(.25*Time*Time,.05*A));}
 }
 auto Energy=[&](int Start,int End,int C,double* Out,bool Check){
  for(int M=0;M<11;++M)Out[M]=0.;
  for(int F=FMath::Max(1,Start);F<=FMath::Min(N-1,End+3);++F){double V[11];Values(F,C,V);
   for(int M=0;M<11;++M){Out[M]+=V[M];if(Check&&V[M]>Peak[C][M]+1.e-6)return false;}
   if(Check){const int I=F*2+C;
    for(int B=0;B<2;++B){
     if(FMath::Sqrt(V[B*3])>Limits[I].V[B*2]+1.e-6)return false;
     if(F>1&&FMath::Sqrt(V[B*3+1])>Limits[I].V[B*2+1]+1.e-6)return false;
    }
    if(F>1&&FMath::Sqrt(V[9])>Limits[I].V[4]+1.e-6)return false;
   }
  }return true;
 };
 int Accepted=0;TArray<int> Centers;const int Stride=FMath::Max(1,FMath::RoundToInt(.2*FPS));
 for(int F=0;F<N;F+=Stride)Centers.Add(F);Centers.AddUnique(N-1);
 // The second proposal follows the original added-roll curve. This can reduce a
 // coherent correction without adding the boundary motion of a constant angle.
 // Both shapes use the same fixed image/peak budgets and local energy checks.
 for(int Shape=0;Shape<2;++Shape)for(int Pass=0;Pass<4;++Pass){bool Changed=false;
  for(double Seconds:{.2,.4,.8,1.2}){const int Radius=FMath::Max(2,FMath::RoundToInt(Seconds*FPS));
   for(int J=0;J<Centers.Num();++J){const int Center=Centers[Pass%2?Centers.Num()-1-J:J];
    const int Start=FMath::Max(0,Center-Radius+1),End=FMath::Min(N-1,Center+Radius-1);
    for(int C=0;C<2;++C){bool Supported=true;for(int F=Start;F<=End;++F)Supported&=Samples[F*2+C].Supported;if(!Supported)continue;
     for(double Amount:{1.,.5,.25,.125,.0625})for(int Sign:{-1,1}){
      if(Shape&&Sign<0)continue;
      if(D.ShouldCancel&&D.ShouldCancel())return false;
      double EB[11];Energy(Start,End,C,EB,false);TArray<double> Old;Old.Reserve(End-Start+1);
      double CostBefore=0,CostAfter=0;bool Good=true;
      for(int F=Start;F<=End;++F){const int I=F*2+C;const auto& S=Samples[I];double T=0;
       Good&=Twist(I,T);CostBefore+=T*T;Old.Add(Angles[I]);
       const double Delta=Shape?-.1*Amount*S.Twist:FMath::DegreesToRadians(Sign*Amount);
       Set(I,Angles[I]+Delta*.5*(1+FMath::Cos(UE_PI*(F-Center)/Radius)));
       Good&=Twist(I,T);CostAfter+=T*T;
       Good&=FMath::Abs(T)<=FMath::Abs(S.Twist)+1.e-10&&T*S.Twist>=-1.e-12;
       const FVector2D UV=Project(Position[I]);
       Good&=(UV-S.Target).Size()<=S.PointLimit+1.e-8&&DirectionError(UV-S.HipUV,S.Direction)<=S.DirectionLimit+1.e-10;
      }
      Good&=CostAfter<CostBefore-1.e-10;
      if(Good){double EA[11];Good=Energy(Start,End,C,EA,true);for(int M=0;M<11;++M)Good&=EA[M]<=EB[M]+1.e-6;}
      if(Good){++Accepted;Changed=true;}
      else for(int F=Start;F<=End;++F)Set(F*2+C,Old[F-Start]);
     }
    }
   }
  }if(!Changed)break;
 }
 if(!Accepted){Report=TEXT("{\"applied\":false,\"reason\":\"no added-roll reduction inside image and motion preservation budgets\"}");return true;}
 TArray<TArray<FQuat4f>> Rot;Rot.SetNum(4);double MaxPosition=0,MaxRotation=0,MaxPlane=0;
 for(int F=0;F<N;++F){
  if(D.ShouldCancel&&D.ShouldCancel())return false;
  Pose(Tracks,F,L,G);TArray<FTransform> Check=L;
  for(int C=0;C<2;++C){const int I=F*2+C,H=Id[C][0],E=Id[C][2];
   Rot[C*2].Add(FQuat4f((G[Ref.GetParentIndex(H)].GetRotation().Inverse()*Rotation[0][I]).GetNormalized()));
   Rot[C*2+1].Add(FQuat4f(Rotation[2][I]));Check[H].SetRotation(FQuat(Rot[C*2].Last()));Check[E].SetRotation(FQuat(Rot[C*2+1].Last()));
   MaxPlane=FMath::Max(MaxPlane,FMath::Abs(Angles[I]));double T=0;
   if(Samples[I].Supported&&Twist(I,T)){AfterTwist+=FMath::Abs(T);MaxAfter=FMath::Max(MaxAfter,FMath::Abs(T));}
  }
  for(int B=1;B<NB;++B)Check[B]=Check[B]*Check[Ref.GetParentIndex(B)];
  for(int C=0;C<2;++C){const int K=Id[C][1],E=Id[C][2];
   if(!Check[K].GetTranslation().Equals(Position[F*2+C],.001)){Report=TEXT("Added-twist knee FK mismatch; no tracks written");return false;}
   MaxPosition=FMath::Max(MaxPosition,(Check[E].GetTranslation()-G[E].GetTranslation()).Size());
   MaxRotation=FMath::Max(MaxRotation,Check[E].GetRotation().GetNormalized().AngularDistance(G[E].GetRotation().GetNormalized()));
  }
 }
 if(!FMath::IsFinite(MaxPosition)||!FMath::IsFinite(MaxRotation)||MaxPosition>.001||MaxRotation>.00001){Report=TEXT("Added-twist endpoint preservation failed; no tracks written");return false;}
 auto& Ctrl=Anim->GetController();Ctrl.OpenBracket(FText::FromString(TEXT("Preserve leg roll during video alignment")),false);bool OK=true;
 for(int C=0;C<2;++C)for(int K=0;K<2;++K){const int B=Id[C][K?2:0];TArray<FVector3f>P,S;
  for(int F=0;F<N;++F){const auto& T=Tracks[B][FMath::Min(F,Tracks[B].Num()-1)];P.Add(FVector3f(T.GetTranslation()));S.Add(FVector3f(T.GetScale3D()));}
  OK=Ctrl.SetBoneTrackKeys(Ref.GetBoneName(B),P,Rot[C*2+K],S,false)&&OK;
 }Ctrl.CloseBracket(false);
 Report=FString::Printf(TEXT("{\"applied\":%s,\"temporal_window_steps\":%d,\"evidence_samples\":%d,\"mean_added_roll_before_after_degrees\":[%.8f,%.8f],\"max_added_roll_before_after_degrees\":[%.8f,%.8f],\"max_plane_change_degrees\":%.8f,\"max_endpoint_position_change_cm\":%.9f,\"max_endpoint_rotation_change_degrees\":%.9f,\"knee_local_rotation_preserved\":true,\"source_motion_clamped\":false}"),OK?TEXT("true"):TEXT("false"),Accepted,Evidence,FMath::RadiansToDegrees(BeforeTwist/FMath::Max(1,Evidence)),FMath::RadiansToDegrees(AfterTwist/FMath::Max(1,Evidence)),FMath::RadiansToDegrees(MaxBefore),FMath::RadiansToDegrees(MaxAfter),FMath::RadiansToDegrees(MaxPlane),MaxPosition,FMath::RadiansToDegrees(MaxRotation));
 return OK;
}
