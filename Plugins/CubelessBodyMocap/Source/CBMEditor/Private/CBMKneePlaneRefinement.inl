// Final knee-plane fit after target placement. Pelvis/feet and bend angles are fixed.
// Both image position and thigh direction must improve; original motion budgets stay unchanged.
static bool CBMRefineTargetKnees(UAnimSequence* Anim,USkeletalMesh* Mesh,const FSolveData& D,
 const TArray<float>& Camera,const FVector& Origin,float Focal,FVector2f ImageCenter,int FPS,
 const TArray<TArray<FTransform>>& BeforeTracks,FString& Report)
{
 const auto* Model=Anim->GetDataModel();const int N=Model->GetNumberOfKeys();const auto& Ref=Mesh->GetRefSkeleton();const int NB=Ref.GetNum();
 const TCHAR* Names[2][3]={{TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l")},{TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r")}};const int Keys[]={12,9};int Id[2][3];
 for(int C=0;C<2;++C)for(int K=0;K<3;++K)Id[C][K]=Ref.FindBoneIndex(Names[C][K]);
 TArray<TArray<FTransform>> Tracks;Tracks.SetNum(NB);for(int B=0;B<NB;++B)if(Model->IsValidBoneTrackName(Ref.GetBoneName(B)))Model->GetBoneTrackTransforms(Ref.GetBoneName(B),Tracks[B]);
 auto Pose=[&](const TArray<TArray<FTransform>>& T,int F,TArray<FTransform>& L,TArray<FTransform>& G){L=Ref.GetRefBonePose();for(int B=0;B<NB;++B)if(T[B].Num())L[B]=T[B][FMath::Min(F,T[B].Num()-1)];G=L;for(int B=1;B<NB;++B)G[B]=G[B]*G[Ref.GetParentIndex(B)];};
 auto Project=[&](FVector P){P=FMetaHumanSMPLX::UE2SMPL(P)+Origin;const FVector V(Camera[0]*P.X+Camera[1]*P.Y+Camera[2]*P.Z+Camera[9],Camera[3]*P.X+Camera[4]*P.Y+Camera[5]*P.Z+Camera[10],Camera[6]*P.X+Camera[7]*P.Y+Camera[8]*P.Z+Camera[11]);return FVector2D(Focal*V.X/FMath::Max(.01,V.Z)+ImageCenter.X,Focal*V.Y/FMath::Max(.01,V.Z)+ImageCenter.Y);};
 auto Degrees=[](FQuat A,FQuat B){return FMath::RadiansToDegrees(A.AngularDistance(B));};
 auto RotationStep=[](FQuat A,FQuat B){FQuat Q=(B*A.Inverse()).GetNormalized();if(Q.W<0)Q=Q*-1.;return Q.ToRotationVector()*(180./UE_PI);};
 struct FSample {FVector Axis,Center,U,V;FQuat Root,Middle;FVector2D HipUV,Target,Direction;double Scale=1;bool Supported=false,Strong=false;};
 TArray<FSample> Samples;Samples.SetNum(N*2);TArray<FVector> BasePosition,Position;TArray<FQuat> BaseRoot,BaseMiddle,Root,Middle;BasePosition.SetNum(N*2);BaseRoot.SetNum(N*2);BaseMiddle.SetNum(N*2);Position.SetNum(N*2);Root.SetNum(N*2);Middle.SetNum(N*2);TArray<double> Angles;Angles.Init(0,N*2);TArray<FTransform>L,G;
 for(int F=0;F<N;++F){if(D.ShouldCancel&&D.ShouldCancel())return false;Pose(BeforeTracks,F,L,G);for(int C=0;C<2;++C){const int I=F*2+C;BasePosition[I]=G[Id[C][1]].GetTranslation();BaseRoot[I]=G[Id[C][0]].GetRotation().GetNormalized();BaseMiddle[I]=G[Id[C][1]].GetRotation().GetNormalized();}Pose(Tracks,F,L,G);
  for(int C=0;C<2;++C){const int I=F*2+C;auto& S=Samples[I];const FVector H=G[Id[C][0]].GetTranslation(),K=G[Id[C][1]].GetTranslation(),E=G[Id[C][2]].GetTranslation();S.Axis=(E-H).GetSafeNormal();S.Center=H+S.Axis*FVector::DotProduct(K-H,S.Axis);S.U=K-S.Center;S.V=FVector::CrossProduct(S.Axis,S.U);S.Root=G[Id[C][0]].GetRotation().GetNormalized();S.Middle=G[Id[C][1]].GetRotation().GetNormalized();S.HipUV=Project(H);const int O=F*75+Keys[C]*3;S.Target=FVector2D(D.Keypoints[O+3],D.Keypoints[O+4]);S.Direction=S.Target-FVector2D(D.Keypoints[O],D.Keypoints[O+1]);S.Scale=FMath::Max(40.,(Project(E)-S.HipUV).Size());S.Supported=D.ValidFrame[F]&&D.Keypoints[O+2]>.7&&D.Keypoints[O+5]>.7&&S.Direction.Size()>10&&S.U.Size()>2.5&&!S.Axis.IsNearlyZero();S.Strong=S.Supported&&D.Keypoints[O+2]>.8&&D.Keypoints[O+5]>.8;Position[I]=K;Root[I]=S.Root;Middle[I]=S.Middle;}
 }
 auto Set=[&](int I,double A){const auto& S=Samples[I];const FQuat Q(S.Axis,A);Root[I]=(Q*S.Root).GetNormalized();Middle[I]=(Q*S.Middle).GetNormalized();Position[I]=S.Center+FMath::Cos(A)*S.U+FMath::Sin(A)*S.V;};
 auto Quality=[&](int I,double A,double& Point,double& Direction){const auto& S=Samples[I];const FVector2D P=Project(S.Center+FMath::Cos(A)*S.U+FMath::Sin(A)*S.V),Edge=P-S.HipUV;Point=(P-S.Target).Size();Direction=FMath::Acos(FMath::Clamp(FVector2D::DotProduct(Edge,S.Direction)/FMath::Max(1.e-10,Edge.Size()*S.Direction.Size()),-1.,1.));return FMath::Square(Point/S.Scale)+Direction*Direction;};
 const double Time=30./FPS;
 // Preserve the already-achieved clip peaks as well as the original added-motion budgets.
 double PeakRootStep[2]={},PeakMiddleStep[2]={},PeakRootAccel[2]={},PeakMiddleAccel[2]={},PeakPositionAccel[2]={};
 for(int F=1;F<N;++F)for(int C=0;C<2;++C){const int I=F*2+C,P=I-2;PeakRootStep[C]=FMath::Max(PeakRootStep[C],Degrees(Root[P],Root[I]));PeakMiddleStep[C]=FMath::Max(PeakMiddleStep[C],Degrees(Middle[P],Middle[I]));if(F>1){const int PP=I-4;PeakRootAccel[C]=FMath::Max(PeakRootAccel[C],(RotationStep(Root[P],Root[I])-RotationStep(Root[PP],Root[P])).Size());PeakMiddleAccel[C]=FMath::Max(PeakMiddleAccel[C],(RotationStep(Middle[P],Middle[I])-RotationStep(Middle[PP],Middle[P])).Size());PeakPositionAccel[C]=FMath::Max(PeakPositionAccel[C],(Position[I]-2*Position[P]+Position[PP]).Size());}}
 auto Safe=[&](int F,int C){for(int K=FMath::Max(1,F);K<=FMath::Min(N-1,F+2);++K){const int I=K*2+C,P=I-2;
   if(Degrees(Root[P],Root[I])>PeakRootStep[C]+1.e-6||Degrees(Middle[P],Middle[I])>PeakMiddleStep[C]+1.e-6)return false;
   if(Degrees(Root[P],Root[I])-Degrees(BaseRoot[P],BaseRoot[I])>Time+1.e-6||Degrees(Middle[P],Middle[I])-Degrees(BaseMiddle[P],BaseMiddle[I])>Time+1.e-6)return false;
   if(K>1){const int PP=I-4;const double B=(BasePosition[I]-2*BasePosition[P]+BasePosition[PP]).Size();if((Position[I]-2*Position[P]+Position[PP]).Size()-B>FMath::Max(.25*Time*Time,.05*B)+1.e-6)return false;
    auto Angular=[&](const TArray<FQuat>& Q){return (RotationStep(Q[P],Q[I])-RotationStep(Q[PP],Q[P])).Size();};const double BR=Angular(BaseRoot),BM=Angular(BaseMiddle);if(Angular(Root)-BR>FMath::Max(.5*Time*Time,.05*BR)+1.e-6||Angular(Middle)-BM>FMath::Max(.5*Time*Time,.05*BM)+1.e-6)return false;
    if(Angular(Root)>PeakRootAccel[C]+1.e-6||Angular(Middle)>PeakMiddleAccel[C]+1.e-6||(Position[I]-2*Position[P]+Position[PP]).Size()>PeakPositionAccel[C]+1.e-6)return false;
   }
  }return true;};
 int Accepted=0;const double Limit=FMath::DegreesToRadians(5.);
 for(int Pass=0;Pass<6;++Pass){bool Changed=false;for(int J=0;J<N*2;++J){if(D.ShouldCancel&&D.ShouldCancel())return false;const int I=Pass%2?N*2-1-J:J,F=I/2,C=I%2;if(!Samples[I].Supported)continue;const double Old=Angles[I];double BeforePoint,BeforeDirection;double BestCost=Quality(I,Old,BeforePoint,BeforeDirection),BestAngle=Old;
   for(int Sign:{-1,1})for(int Try=0;Try<6;++Try){const double A=Old+Sign*FMath::DegreesToRadians(1.)/double(1<<Try);if(FMath::Abs(A)>Limit)continue;double P,E;const double Cost=Quality(I,A,P,E);if(P>BeforePoint+1.e-8||E>BeforeDirection+1.e-10||Cost>=BestCost-1.e-10)continue;Set(I,A);if(Safe(F,C)){BestAngle=A;BestCost=Cost;}Set(I,Old);}
   if(BestAngle!=Old){Angles[I]=BestAngle;Set(I,BestAngle);++Accepted;Changed=true;}
  }if(!Changed)break;
 }
 // Joint temporal proposals escape a single-frame acceleration-boundary stall.
 // Keep v8 point/direction quality and clip peaks; never replenish the original budgets.
 // Candidate-local motion energy includes the boundary after the last changed frame.
 // This is a motion-preservation check, not an anatomical angle clamp.
 auto MotionEnergy=[&](int Start,int End,int C,double* Out){
  for(int M=0;M<8;++M)Out[M]=0.;
  for(int F=FMath::Max(1,Start);F<=FMath::Min(N-1,End+3);++F){const int I=F*2+C,P=I-2;
   const FVector VR=RotationStep(Root[P],Root[I]),VM=RotationStep(Middle[P],Middle[I]);Out[0]+=VR.SizeSquared();Out[1]+=VM.SizeSquared();
   if(F>1){const int PP=I-4;const FVector AR=VR-RotationStep(Root[PP],Root[P]),AM=VM-RotationStep(Middle[PP],Middle[P]);Out[2]+=AR.SizeSquared();Out[3]+=AM.SizeSquared();Out[6]+=(Position[I]-2*Position[P]+Position[PP]).SizeSquared();
    if(F>2){const int PPP=I-6;Out[4]+=(AR-RotationStep(Root[PP],Root[P])+RotationStep(Root[PPP],Root[PP])).SizeSquared();Out[5]+=(AM-RotationStep(Middle[PP],Middle[P])+RotationStep(Middle[PPP],Middle[PP])).SizeSquared();Out[7]+=(Position[I]-3*Position[P]+3*Position[PP]-Position[PPP]).SizeSquared();}
   }
  }
 };
 int WindowAccepted=0,ExtendedAccepted=0,NaturalAccepted=0;
 // Preserve the exact v9 result first. A second pass may exceed its conservative
 // 5-degree pose cap only in wholly high-confidence windows, with v9 quality/peaks fixed.
 // The final stage keeps the same 10-degree cap and protects v10 local motion energy.
 for(int Stage=0;Stage<3;++Stage){const double WindowLimit=Stage?FMath::DegreesToRadians(10.):Limit;
 TArray<double> WindowPoint,WindowDirection;WindowPoint.SetNum(N*2);WindowDirection.SetNum(N*2);
 for(int I=0;I<N*2;++I)Quality(I,Angles[I],WindowPoint[I],WindowDirection[I]);
 for(int C=0;C<2;++C){PeakRootStep[C]=PeakMiddleStep[C]=PeakRootAccel[C]=PeakMiddleAccel[C]=PeakPositionAccel[C]=0.;}
 for(int F=1;F<N;++F)for(int C=0;C<2;++C){const int I=F*2+C,P=I-2;PeakRootStep[C]=FMath::Max(PeakRootStep[C],Degrees(Root[P],Root[I]));PeakMiddleStep[C]=FMath::Max(PeakMiddleStep[C],Degrees(Middle[P],Middle[I]));if(F>1){const int PP=I-4;PeakRootAccel[C]=FMath::Max(PeakRootAccel[C],(RotationStep(Root[P],Root[I])-RotationStep(Root[PP],Root[P])).Size());PeakMiddleAccel[C]=FMath::Max(PeakMiddleAccel[C],(RotationStep(Middle[P],Middle[I])-RotationStep(Middle[PP],Middle[P])).Size());PeakPositionAccel[C]=FMath::Max(PeakPositionAccel[C],(Position[I]-2*Position[P]+Position[PP]).Size());}}
 const int Stride=FMath::Max(1,FMath::RoundToInt(.2*FPS));TArray<int> Centers;for(int F=0;F<N;F+=Stride)Centers.Add(F);Centers.AddUnique(N-1);
 for(int Pass=0;Pass<4;++Pass){bool Changed=false;
  for(double Seconds:{.2,.4,.8,1.2}){const int Radius=FMath::Max(2,FMath::RoundToInt(Seconds*FPS));
   for(int J=0;J<Centers.Num();++J){const int Center=Centers[Pass%2?Centers.Num()-1-J:J],Start=FMath::Max(0,Center-Radius+1),End=FMath::Min(N-1,Center+Radius-1);
    for(int C=0;C<2;++C){bool Supported=true;for(int F=Start;F<=End;++F)Supported&=Stage?Samples[F*2+C].Strong:Samples[F*2+C].Supported;if(!Supported)continue;
     for(double Step:{1.,.5,.25,.125,.0625})for(int Sign:{-1,1}){
      if(D.ShouldCancel&&D.ShouldCancel())return false;
      double EnergyBefore[8];if(Stage==2)MotionEnergy(Start,End,C,EnergyBefore);
      TArray<double> Old;Old.Reserve(End-Start+1);bool Valid=true;double CostBefore=0,CostAfter=0;
      for(int F=Start;F<=End;++F){const int I=F*2+C;double A=Angles[I]+FMath::DegreesToRadians(Sign*Step)*.5*(1.+FMath::Cos(UE_PI*(F-Center)/Radius));if(Stage==2)A=FMath::Clamp(A,-WindowLimit,WindowLimit);double P,E;Old.Add(Angles[I]);CostBefore+=Quality(I,Angles[I],P,E);CostAfter+=Quality(I,A,P,E);Valid&=FMath::Abs(A)<=WindowLimit+1.e-10&&P<=WindowPoint[I]+1.e-8&&E<=WindowDirection[I]+1.e-10;Set(I,A);Angles[I]=A;}
      if(CostAfter>=CostBefore-1.e-10)Valid=false;
      if(Valid)for(int F=Start;F<=End&&Valid;++F)Valid=Safe(F,C);
      if(Valid&&Stage==2){double EnergyAfter[8];MotionEnergy(Start,End,C,EnergyAfter);for(int M=0;M<8;++M)Valid&=EnergyAfter[M]<=EnergyBefore[M]+1.e-6;}
      if(Valid){++WindowAccepted;if(Stage==1)++ExtendedAccepted;if(Stage==2)++NaturalAccepted;++Accepted;Changed=true;}
      else for(int F=Start;F<=End;++F){const int I=F*2+C;Angles[I]=Old[F-Start];Set(I,Angles[I]);}
     }
    }
   }
  }if(!Changed)break;
 }
 }
 if(!Accepted){Report=TEXT("{\"applied\":false,\"reason\":\"no joint position and direction improvement inside motion budgets\"}");return true;}
 TArray<TArray<FQuat4f>> Rot;Rot.SetNum(4);double MaxEnd=0,MaxEndRotation=0,PointBefore=0,PointAfter=0,DirectionBefore=0,DirectionAfter=0,MaxAngle=0;int Count=0;
 for(int F=0;F<N;++F){if(D.ShouldCancel&&D.ShouldCancel())return false;Pose(Tracks,F,L,G);TArray<FTransform> NewL=L,NewG;
  for(int C=0;C<2;++C){const int I=F*2+C,H=Id[C][0],K=Id[C][1],E=Id[C][2];Rot[C*2].Add(FQuat4f((G[Ref.GetParentIndex(H)].GetRotation().Inverse()*Root[I]).GetNormalized()));Rot[C*2+1].Add(FQuat4f((Middle[I].Inverse()*G[E].GetRotation()).GetNormalized()));NewL[H].SetRotation(FQuat(Rot[C*2].Last()));NewL[E].SetRotation(FQuat(Rot[C*2+1].Last()));MaxAngle=FMath::Max(MaxAngle,FMath::Abs(Angles[I]));if(Samples[I].Supported){double P,A;Quality(I,0,P,A);PointBefore+=P;DirectionBefore+=A;Quality(I,Angles[I],P,A);PointAfter+=P;DirectionAfter+=A;++Count;}}
  NewG=NewL;for(int B=1;B<NB;++B)NewG[B]=NewG[B]*NewG[Ref.GetParentIndex(B)];for(int C=0;C<2;++C){const int K=Id[C][1],E=Id[C][2];if(!NewG[K].GetTranslation().Equals(Position[F*2+C],.001)){Report=TEXT("{\"applied\":false,\"reason\":\"knee-plane FK mismatch; no knee tracks written\"}");return false;}MaxEnd=FMath::Max(MaxEnd,(NewG[E].GetTranslation()-G[E].GetTranslation()).Size());MaxEndRotation=FMath::Max(MaxEndRotation,NewG[E].GetRotation().GetNormalized().AngularDistance(G[E].GetRotation().GetNormalized()));}
 }
 if(!FMath::IsFinite(MaxEnd)||!FMath::IsFinite(MaxEndRotation)||MaxEnd>.001||MaxEndRotation>.00001){Report=FString::Printf(TEXT("{\"applied\":false,\"reason\":\"knee-plane endpoint preservation failed; no knee tracks written\",\"position_cm\":%.9f,\"rotation_radians\":%.9f}"),MaxEnd,MaxEndRotation);return false;}
 auto& Ctrl=Anim->GetController();Ctrl.OpenBracket(FText::FromString(TEXT("Final image knee plane refinement")),false);bool OK=true;
 for(int C=0;C<2;++C)for(int K=0;K<2;++K){const int B=Id[C][K?2:0];TArray<FVector3f>P,S;for(int F=0;F<N;++F){const auto& T=Tracks[B][FMath::Min(F,Tracks[B].Num()-1)];P.Add(FVector3f(T.GetTranslation()));S.Add(FVector3f(T.GetScale3D()));}OK=Ctrl.SetBoneTrackKeys(Ref.GetBoneName(B),P,Rot[C*2+K],S,false)&&OK;}Ctrl.CloseBracket(false);
 Report=FString::Printf(TEXT("{\"applied\":%s,\"accepted_steps\":%d,\"temporal_window_steps\":%d,\"high_confidence_extended_steps\":%d,\"natural_bounded_steps\":%d,\"evidence_samples\":%d,\"mean_knee_error_before_after_px\":[%.8f,%.8f],\"mean_thigh_direction_error_before_after_degrees\":[%.8f,%.8f],\"max_plane_change_degrees\":%.8f,\"max_endpoint_position_change_cm\":%.9f,\"max_endpoint_rotation_change_degrees\":%.9f}"),OK?TEXT("true"):TEXT("false"),Accepted,WindowAccepted,ExtendedAccepted,NaturalAccepted,Count,PointBefore/FMath::Max(1,Count),PointAfter/FMath::Max(1,Count),FMath::RadiansToDegrees(DirectionBefore/FMath::Max(1,Count)),FMath::RadiansToDegrees(DirectionAfter/FMath::Max(1,Count)),FMath::RadiansToDegrees(MaxAngle),MaxEnd,FMath::RadiansToDegrees(MaxEndRotation));return OK;
}
