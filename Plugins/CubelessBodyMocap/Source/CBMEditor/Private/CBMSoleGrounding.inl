#include "CBMSoleClearance.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"

static bool CBMGroundTargetSoles(UAnimSequence* Anim,USkeletalMesh* Mesh,FString& Report) {
 const double Started=FPlatformTime::Seconds();
 auto Skip=[&](const TCHAR* Reason){Report=FString::Printf(TEXT("{\"applied\":false,\"reason\":\"%s\"}"),Reason);return true;};
 if(!Anim||!Mesh||Anim->GetSkeleton()!=Mesh->GetSkeleton())return Skip(TEXT("unsupported target"));
 auto* Imported=Mesh->GetImportedModel();if(!Imported||Imported->LODModels.Num()==0)return Skip(TEXT("missing editor mesh vertices"));
 const auto& Ref=Mesh->GetRefSkeleton();const auto* Model=Anim->GetDataModel();const int NB=Ref.GetNum(),N=Model->GetNumberOfKeys();
 if(N<2||NB<1||Ref.GetBoneName(0)!=TEXT("root"))return Skip(TEXT("unsupported root"));
 int Feet[2]={Ref.FindBoneIndex(TEXT("foot_l")),Ref.FindBoneIndex(TEXT("foot_r"))};
 int Calves[2]={Ref.FindBoneIndex(TEXT("calf_l")),Ref.FindBoneIndex(TEXT("calf_r"))};
 int Balls[2]={Ref.FindBoneIndex(TEXT("ball_l")),Ref.FindBoneIndex(TEXT("ball_r"))};
 for(int S=0;S<2;++S)if(Feet[S]<0||Calves[S]<0||Balls[S]<0)return Skip(TEXT("missing foot bones"));
 TArray<FTransform> Bind=Ref.GetRefBonePose();for(int B=1;B<NB;++B)Bind[B]=Bind[B]*Bind[Ref.GetParentIndex(B)];
 struct FInfluence {int Bone;double Weight;FVector Local;};struct FPoint {TArray<FInfluence,TInlineAllocator<8>> Weights;};
 TArray<FPoint> Points;int SideCount[2]={0,0};
 for(const auto& Section:Imported->LODModels[0].Sections)for(const auto& V:Section.SoftVertices) {
  const FVector Rest(V.Position);int Side=FVector::DistSquared(Rest,Bind[Feet[0]].GetTranslation())<FVector::DistSquared(Rest,Bind[Feet[1]].GetTranslation())?0:1;
  const double Length=FVector::Distance(Bind[Feet[Side]].GetTranslation(),Bind[Balls[Side]].GetTranslation());
  if(Length<1||Rest.Z>Bind[Feet[Side]].GetTranslation().Z+.75*Length||FVector::Distance(Rest,Bind[Feet[Side]].GetTranslation())>2.5*Length)continue;
  FPoint Point;double LegWeight=0,Total=0;
  for(int K=0;K<MAX_TOTAL_INFLUENCES;++K)if(V.InfluenceWeights[K]) {
   if(!Section.BoneMap.IsValidIndex(V.InfluenceBones[K]))return Skip(TEXT("invalid skin bone map"));
   const int Bone=Section.BoneMap[V.InfluenceBones[K]];if(!Bind.IsValidIndex(Bone))return Skip(TEXT("invalid skin bone"));
   const double Weight=V.InfluenceWeights[K]/65535.;int Parent=Bone;
   while(Parent>=0&&Parent!=Calves[Side])Parent=Ref.GetParentIndex(Parent);
   if(Parent==Calves[Side])LegWeight+=Weight;
   Point.Weights.Add({Bone,Weight,Bind[Bone].InverseTransformPosition(Rest)});Total+=Weight;
  }
  if(LegWeight<.75||FMath::Abs(Total-1.)>.002)continue;
  for(auto& W:Point.Weights)W.Weight/=Total;
  Points.Add(MoveTemp(Point));++SideCount[Side];
 }
 if(SideCount[0]<20||SideCount[1]<20)return Skip(TEXT("insufficient shoe surface"));
 TArray<TArray<FTransform>> Tracks;Tracks.SetNum(NB);
 for(int B=0;B<NB;++B)if(Model->IsValidBoneTrackName(Ref.GetBoneName(B))){Model->GetBoneTrackTransforms(Ref.GetBoneName(B),Tracks[B]);if(Tracks[B].Num()!=N)return Skip(TEXT("inconsistent track keys"));}
 if(Tracks[0].Num()!=N)return Skip(TEXT("missing root track"));
 TArray<double> MinZ;MinZ.SetNum(N);
 for(int F=0;F<N;++F) {
  TArray<FTransform> G=Ref.GetRefBonePose();for(int B=0;B<NB;++B)if(Tracks[B].Num())G[B]=Tracks[B][F];
  for(int B=1;B<NB;++B)G[B]=G[B]*G[Ref.GetParentIndex(B)];
  double Z=DBL_MAX;
  for(const auto& Point:Points){FVector P=FVector::ZeroVector;for(const auto& W:Point.Weights)P+=G[W.Bone].TransformPosition(W.Local)*W.Weight;Z=FMath::Min(Z,P.Z);}
  MinZ[F]=Z;
 }
 const double FPS=Model->GetFrameRate().AsDecimal();const auto Lift=CBMSole::Lift(MinZ,FPS);
 if(Lift.Num()!=N)return Skip(TEXT("nonfinite surface or clearance over 5 cm; original preserved"));
 double MaxLift=0,MeanLift=0,Before=DBL_MAX,After=DBL_MAX,MaxSpeed=0;
 TArray<FVector3f> P,S;TArray<FQuat4f> Q;
 for(int F=0;F<N;++F){const auto& T=Tracks[0][F];auto Position=T.GetTranslation();Position.Z+=Lift[F];P.Add(FVector3f(Position));Q.Add(FQuat4f(T.GetRotation()));S.Add(FVector3f(T.GetScale3D()));MaxLift=FMath::Max(MaxLift,Lift[F]);MeanLift+=Lift[F]/N;Before=FMath::Min(Before,MinZ[F]);After=FMath::Min(After,MinZ[F]+Lift[F]);if(F)MaxSpeed=FMath::Max(MaxSpeed,FMath::Abs(Lift[F]-Lift[F-1])*FPS);}
 if(MaxLift>0){auto& C=Anim->GetController();C.OpenBracket(FText::FromString(TEXT("Shoe surface ground clearance")),false);const bool OK=C.SetBoneTrackKeys(Ref.GetBoneName(0),P,Q,S,false);C.CloseBracket(false);if(!OK){Report=TEXT("Root clearance track write failed");return false;}}
 // No endpoint rotations, local limb pose, X/Y trajectory or timing may change.
 for(int B=0;B<NB;++B)if(Tracks[B].Num()){TArray<FTransform> Written;Model->GetBoneTrackTransforms(Ref.GetBoneName(B),Written);if(Written.Num()!=N)return false;
  for(int F=0;F<N;++F){FTransform Expected=Tracks[B][F];if(B==0){FVector V=Expected.GetTranslation();V.Z+=Lift[F];Expected.SetTranslation(V);}if(!Expected.Equals(Written[F],1.e-5))return false;}}
 Report=FString::Printf(TEXT("{\"applied\":%s,\"version\":\"sole_root_clearance_v2_local\",\"frames\":%d,\"shoe_vertices\":%d,\"minimum_before_cm\":%.6f,\"minimum_after_cm\":%.6f,\"maximum_lift_cm\":%.6f,\"mean_lift_cm\":%.6f,\"maximum_added_vertical_speed_cm_s\":%.6f,\"horizontal_motion_preserved\":true,\"local_rotations_preserved\":true,\"milliseconds\":%.3f}"),MaxLift>0?TEXT("true"):TEXT("false"),N,Points.Num(),Before,After,MaxLift,MeanLift,MaxSpeed,(FPlatformTime::Seconds()-Started)*1000);
 return true;
}
