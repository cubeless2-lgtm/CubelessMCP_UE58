#pragma once
#include "CoreMinimal.h"

namespace CBMFootSupport {
inline bool MatchesEvidenceSource(const FString& SourceName,const FString& RunName){
 FGuid Run;if(!FGuid::ParseExact(RunName,EGuidFormats::Digits,Run))return false;
 const FString Prefix=TEXT("Body_")+RunName;
 return SourceName.Equals(Prefix,ESearchCase::CaseSensitive)||SourceName.StartsWith(Prefix+TEXT("_"),ESearchCase::CaseSensitive);
}
struct FWeight {int Bone;double Weight;FVector Local;};
struct FPoint {int Side;FVector Rest;TArray<FWeight> Weights;};
struct FInput {
 TArray<int> Parent;TArray<FTransform> Bind;TArray<TArray<FTransform>> Local;
 TArray<FPoint> Points;TArray<float> Contacts,Keypoints;double FPS=30;
 int Thigh[2],Calf[2],Foot[2],Ball[2];FGuid Guid;
};
struct FResult {TArray<TArray<FTransform>> Local;int Supported[2]={0,0},Candidates[2]={0,0},Accepted[2]={0,0};double MaxAngle=0,MaxAnkle=0,Seconds=0;bool Cancelled=false;};
inline bool Descendant(const FInput& I,int B,int Root){while(B>=0){if(B==Root)return true;B=I.Parent[B];}return false;}
inline TArray<FTransform> World(const FInput& I,const TArray<FTransform>& L){auto G=L;for(int B=1;B<G.Num();++B)G[B]=G[B]*G[I.Parent[B]];return G;}
inline TArray<FVector> Skin(const FInput& I,const TArray<FTransform>& G,int Side){TArray<FVector> P;for(const auto& V:I.Points)if(V.Side==Side){FVector X=FVector::ZeroVector;for(const auto& W:V.Weights)X+=G[W.Bone].TransformPosition(W.Local)*W.Weight;P.Add(X);}return P;}
inline double MinZ(const TArray<FVector>& P){double Z=DBL_MAX;for(const auto& V:P)Z=FMath::Min(Z,V.Z);return Z;}
inline FVector Center(const TArray<FVector>& P,const TArray<int>& Indices){FVector X=FVector::ZeroVector;for(int K:Indices)X+=P[K];return X/Indices.Num();}
inline double P95(TArray<double> V){if(V.IsEmpty())return 0;V.Sort();const double K=.95*(V.Num()-1);const int A=FMath::FloorToInt(K);return FMath::Lerp(V[A],V[FMath::Min(A+1,V.Num()-1)],K-A);}
inline void AddStats(const TArray<double>& V,TArray<double>& M,bool Mean=false){double Max=0,Sum=0;for(double X:V){Max=FMath::Max(Max,X);Sum+=X;}M.Add(Max);M.Add(P95(V));if(Mean)M.Add(Sum/FMath::Max(1,V.Num()));}
inline bool NoWorse(const TArray<double>& A,const TArray<double>& B,double Epsilon=1.e-7){if(A.Num()!=B.Num()||!FMath::IsFinite(Epsilon)||Epsilon<0)return false;for(int K=0;K<A.Num();++K)if(!FMath::IsFinite(A[K])||!FMath::IsFinite(B[K])||A[K]>B[K]+Epsilon)return false;return true;}
inline TArray<double> Rotations(const FInput& I,const TArray<TArray<FTransform>>& G,int Side){
 TArray<double> M;for(int Bone:{I.Thigh[Side],I.Calf[Side],I.Foot[Side],I.Ball[Side]})for(bool Local:{false,true}){
  TArray<FVector> V;FQuat Previous=FQuat::Identity;
  for(int F=0;F<G.Num();++F){FQuat Q=G[F][Bone].GetRotation();if(Local)Q=G[F][I.Parent[Bone]].GetRotation().Inverse()*Q;if(F){FQuat D=Previous.Inverse()*Q;if(D.W<0)D=D*-1.;V.Add(D.ToRotationVector());}Previous=Q;}
  for(int D=0;D<3;++D){TArray<double> Lengths;for(auto X:V)Lengths.Add(X.Size());AddStats(Lengths,M);TArray<FVector> Next;for(int F=1;F<V.Num();++F)Next.Add(V[F]-V[F-1]);V=MoveTemp(Next);}
 }return M;
}
inline void MoveBranch(const FInput& I,const TArray<FTransform>& Old,TArray<FTransform>& New,int Root,const FVector& P,const FQuat& Q){const FQuat Delta=Q*Old[Root].GetRotation().Inverse();for(int B=0;B<Old.Num();++B)if(Descendant(I,B,Root)){New[B].SetTranslation(P+Delta.RotateVector(Old[B].GetTranslation()-Old[Root].GetTranslation()));New[B].SetRotation((Delta*Old[B].GetRotation()).GetNormalized());}}
// No stretching or anatomical limits. Keep the original knee bending plane.
inline bool LegIK(const FInput& I,const TArray<FTransform>& Old,int Side,const FVector& Target,const FQuat& FootQ,TArray<FTransform>& New){
 const int T=I.Thigh[Side],C=I.Calf[Side],F=I.Foot[Side];if(Target.ContainsNaN()||FootQ.ContainsNaN()||Old[T].ContainsNaN()||Old[C].ContainsNaN()||Old[F].ContainsNaN())return false;const FVector H=Old[T].GetTranslation(),K=Old[C].GetTranslation(),A=Old[F].GetTranslation();
 const double L1=(K-H).Size(),L2=(A-K).Size(),D=(Target-H).Size();if(D>=L1+L2-1.e-4||D<=FMath::Abs(L1-L2)+1.e-4)return false;
 const FVector Axis=(Target-H)/D;FVector Pole=K-H;Pole-=Axis*FVector::DotProduct(Pole,Axis);if(!Pole.Normalize(1.e-8))return false;
 const double X=(L1*L1-L2*L2+D*D)/(2*D);const FVector NK=H+Axis*X+Pole*FMath::Sqrt(FMath::Max(0.,L1*L1-X*X));
 New=Old;MoveBranch(I,Old,New,T,H,FQuat::FindBetweenVectors(K-H,NK-H)*Old[T].GetRotation());MoveBranch(I,Old,New,C,NK,FQuat::FindBetweenVectors(A-K,Target-NK)*Old[C].GetRotation());MoveBranch(I,Old,New,F,Target,FootQ);return true;
}
inline FResult Solve(const FInput& I,TFunctionRef<bool()> Cancelled){
 const double Started=FPlatformTime::Seconds();FResult R;R.Local=I.Local;const int N=I.Local.Num();
 if(N<7||I.Contacts.Num()!=N*6||I.Keypoints.Num()!=N*75)return R;
 for(float X:I.Contacts)if(!FMath::IsFinite(X))return R;for(float X:I.Keypoints)if(!FMath::IsFinite(X))return R;
 TArray<TArray<FTransform>> G;for(const auto& L:I.Local)G.Add(World(I,L));
 for(int Side=0;Side<2;++Side){
  TArray<TArray<FVector>> Baseline;TArray<double> BaselineHeight;for(const auto& Frame:G){Baseline.Add(Skin(I,Frame,Side));BaselineHeight.Add(MinZ(Baseline.Last()));}
  TArray<int> Sole;double RestMin=DBL_MAX;for(const auto& P:I.Points)if(P.Side==Side)RestMin=FMath::Min(RestMin,P.Rest.Z);int PointIndex=0;for(const auto& P:I.Points)if(P.Side==Side){if(P.Rest.Z<RestMin+1.5)Sole.Add(PointIndex);++PointIndex;}if(Sole.Num()<4)continue;
  const auto Limit=Rotations(I,G,Side);TArray<bool> Support;Support.Init(false,N);
  for(int F=0;F<N;++F){double Confidence=1;for(int J=0;J<2;++J)Confidence=FMath::Min(Confidence,1./(1.+FMath::Exp(-FMath::Clamp(double(I.Contacts[F*6+Side*2+J]),-50.,50.))));double Video=1;for(int K=19+Side*3;K<22+Side*3;++K)Video=FMath::Min(Video,double(I.Keypoints[F*75+K*3+2]));const int A=FMath::Max(0,F-1),B=FMath::Min(N-1,F+1);const double Speed=(G[B][I.Foot[Side]].GetTranslation()-G[A][I.Foot[Side]].GetTranslation()).Size()*I.FPS/(B-A);Support[F]=Confidence>.8&&Video>.5&&BaselineHeight[F]<2.&&Speed<25.;if(Support[F])++R.Supported[Side];}
  for(int Start=0;Start<N;){if(Cancelled()){R.Cancelled=true;R.Local=I.Local;return R;}if(!Support[Start]){++Start;continue;}int End=Start;while(End<N&&Support[End])++End;const int Begin=Start;Start=End;if(End-Begin<FMath::Max(7,FMath::CeilToInt(.2*I.FPS)))continue;++R.Candidates[Side];
   const int Lo=FMath::Max(0,Begin-3),Hi=FMath::Min(N,End+3);TArray<int> Patch;for(int P=0;P<Baseline[0].Num();++P){double Height=0;for(int F=Lo;F<Hi;++F)Height+=Baseline[F][P].Z-BaselineHeight[F];if(Height/(Hi-Lo)<.5)Patch.Add(P);}if(Patch.Num()<4)continue;
   TArray<double> BaseSlip;for(int F=Lo+1;F<Hi;++F)BaseSlip.Add((Center(Baseline[F],Sole)-Center(Baseline[F-1],Sole)).Size2D()*I.FPS);TArray<double> SlipLimit;AddStats(BaseSlip,SlipLimit,true);
   for(double Scale:{1.,.75,.5,.25,.125}){
    auto Candidate=G;bool Good=true;double MaxAngle=0,MaxMove=0;TArray<TArray<FVector>> SkinCandidate;SkinCandidate.SetNum(Hi-Lo);
    for(int F=Lo;F<Hi;++F){if(F<Begin||F>=End){SkinCandidate[F-Lo]=Baseline[F];continue;}
     const double Weight=FMath::Pow(FMath::Sin(PI*(F-Begin+1)/(End-Begin+1)),4.);const FVector Normal=(G[F][I.Foot[Side]].GetRotation()*I.Bind[I.Foot[Side]].GetRotation().Inverse()).RotateVector(FVector::UpVector);
     if(Normal.Z<=0){Good=false;break;}const FQuat Swing=FQuat::FindBetweenNormals(Normal,FVector::UpVector);const FQuat Delta=FQuat::Slerp(FQuat::Identity,Swing,Weight*Scale).GetNormalized();const FQuat Q=(Delta*G[F][I.Foot[Side]].GetRotation()).GetNormalized();
     const FVector Original=G[F][I.Foot[Side]].GetTranslation();FVector Target=Original;const FVector Anchor=Center(Baseline[F],Patch);const double Height=BaselineHeight[F];bool Converged=false;
     for(int Iter=0;Iter<6;++Iter){if(!LegIK(I,G[F],Side,Target,Q,Candidate[F]))break;auto Points=Skin(I,Candidate[F],Side);const FVector C=Center(Points,Patch);const FVector Error(Anchor.X-C.X,Anchor.Y-C.Y,Height-MinZ(Points));SkinCandidate[F-Lo]=MoveTemp(Points);if(Error.GetAbsMax()<1.e-5){Converged=true;break;}Target+=Error;}
     MaxMove=FMath::Max(MaxMove,(Target-Original).Size());MaxAngle=FMath::Max(MaxAngle,Delta.GetAngle());if(!Converged||MaxMove>3.){Good=false;break;}
    }
    if(!Good||MaxAngle<1.e-5)continue;
    TArray<double> Slip,SlipStats;for(int F=1;F<SkinCandidate.Num();++F)Slip.Add((Center(SkinCandidate[F],Sole)-Center(SkinCandidate[F-1],Sole)).Size2D()*I.FPS);AddStats(Slip,SlipStats,true);
    if(!NoWorse(SlipStats,SlipLimit,1.e-6)||!NoWorse(Rotations(I,Candidate,Side),Limit))continue;
    // Keep the target skeleton's local translations/scales exactly. Check FK
    // after converting IK rotations rather than serializing independent joints.
    auto Proposed=R.Local;for(int F=Begin;F<End;++F)for(int B:{I.Thigh[Side],I.Calf[Side],I.Foot[Side]})Proposed[F][B].SetRotation((Candidate[F][I.Parent[B]].GetRotation().Inverse()*Candidate[F][B].GetRotation()).GetNormalized());
    for(int F=Begin;F<End;++F){const auto FK=World(I,Proposed[F]);if(!FK[I.Foot[Side]].GetTranslation().Equals(Candidate[F][I.Foot[Side]].GetTranslation(),1.e-4)){Good=false;break;}}
    if(!Good)continue;R.Local=MoveTemp(Proposed);G=MoveTemp(Candidate);for(int F=Lo;F<Hi;++F){Baseline[F]=MoveTemp(SkinCandidate[F-Lo]);BaselineHeight[F]=MinZ(Baseline[F]);}++R.Accepted[Side];R.MaxAngle=FMath::Max(R.MaxAngle,MaxAngle);R.MaxAnkle=FMath::Max(R.MaxAnkle,MaxMove);break;
   }
  }
 }
 R.Seconds=FPlatformTime::Seconds()-Started;return R;
}
}
