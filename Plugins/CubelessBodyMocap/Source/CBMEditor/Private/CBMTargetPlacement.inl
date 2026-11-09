// Conservative target placement with smooth, image-conditioned root depth/height.
// Constant horizontal foot offsets preserve every foot velocity/contact height.
static bool CBMAlignTargetPlacement(UAnimSequence* Anim,USkeletalMesh* Mesh,const FSolveData& D,
 const TArray<float>& Camera,const FVector& Origin,float Focal,FVector2f ImageCenter,int FPS,
 const TArray<TArray<FTransform>>& BeforeTracks,FString& Report)
{
 const auto* Model=Anim->GetDataModel();const int N=Model->GetNumberOfKeys();const auto& Ref=Mesh->GetRefSkeleton();const int NB=Ref.GetNum();
 const TCHAR* Names[]={TEXT("pelvis"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")};
 const int Keys[]={8,12,13,14,9,10,11,5,6,7,2,3,4};int Id[13];
 for(int I=0;I<13;++I){Id[I]=Ref.FindBoneIndex(Names[I]);if(Id[I]<0){Report=TEXT("{\"applied\":false,\"reason\":\"missing target bones\"}");return true;}}
 TArray<TArray<FTransform>> Tracks;Tracks.SetNum(NB);
 for(int B=0;B<NB;++B)if(Model->IsValidBoneTrackName(Ref.GetBoneName(B)))Model->GetBoneTrackTransforms(Ref.GetBoneName(B),Tracks[B]);
 if(Tracks[0].Num()==0){Report=TEXT("{\"applied\":false,\"reason\":\"missing root track\"}");return true;}
 struct FPose {FVector P[13];FQuat Q[13];};TArray<FPose> Current,Base;Current.SetNum(N);Base.SetNum(N);
 auto FullPose=[&](const TArray<TArray<FTransform>>& T,int F,TArray<FTransform>& L,TArray<FTransform>& G){L=Ref.GetRefBonePose();for(int B=0;B<NB;++B)if(T[B].Num())L[B]=T[B][FMath::Min(F,T[B].Num()-1)];G=L;for(int B=1;B<NB;++B)G[B]=G[B]*G[Ref.GetParentIndex(B)];};
 TArray<FTransform> L,G;
 for(int F=0;F<N;++F){FullPose(Tracks,F,L,G);for(int I=0;I<13;++I){Current[F].P[I]=G[Id[I]].GetTranslation();Current[F].Q[I]=G[Id[I]].GetRotation().GetNormalized();}FullPose(BeforeTracks,F,L,G);for(int I=0;I<13;++I){Base[F].P[I]=G[Id[I]].GetTranslation();Base[F].Q[I]=G[Id[I]].GetRotation().GetNormalized();}}
 auto Project=[&](FVector P){P=FMetaHumanSMPLX::UE2SMPL(P)+Origin;FVector V(Camera[0]*P.X+Camera[1]*P.Y+Camera[2]*P.Z+Camera[9],Camera[3]*P.X+Camera[4]*P.Y+Camera[5]*P.Z+Camera[10],Camera[6]*P.X+Camera[7]*P.Y+Camera[8]*P.Z+Camera[11]);return FVector2D(Focal*V.X/FMath::Max(.01,V.Z)+ImageCenter.X,Focal*V.Y/FMath::Max(.01,V.Z)+ImageCenter.Y);};
 auto Angle=[](FQuat A,FQuat B){return FMath::RadiansToDegrees(A.AngularDistance(B));};
 auto Step=[](FQuat A,FQuat B){FQuat Q=(B*A.Inverse()).GetNormalized();if(Q.W<0)Q=Q*-1.;return Q.ToRotationVector()*(180./UE_PI);};
 auto Acc=[&](const TArray<FPose>& P,int F,int I){return (Step(P[F-1].Q[I],P[F].Q[I])-Step(P[F-2].Q[I],P[F-1].Q[I])).Size();};
 struct FOffsets {double V[7]={};};
 TArray<FVector> Drift;Drift.Init(FVector::ZeroVector,N);
 auto Make=[&](const FOffsets& V,TArray<FPose>& Out,bool Linearize=false,const TArray<FPose>* Basis=nullptr){const auto& Source=Basis?*Basis:Current;Out=Source;const FVector Shift(V.V[0],V.V[1],V.V[2]);
  for(int F=0;F<N;++F){auto& Z=Out[F];const auto& A=Source[F];for(int I=0;I<13;++I)Z.P[I]+=Shift+Drift[F];
   for(int C=0;C<2;++C){const int H=1+C*3,K=H+1,E=H+2;Z.P[E]=A.P[E]+FVector(V.V[3+C*2],V.V[4+C*2],0);FVector Axis=Z.P[E]-Z.P[H];const double Dist=Axis.Size(),L1=(A.P[K]-A.P[H]).Size(),L2=(A.P[E]-A.P[K]).Size();if(Dist<.001||(!Linearize&&(Dist>L1+L2+1.e-6||Dist<FMath::Abs(L1-L2)+.001)))return false;Axis/=Dist;const double Along=(L1*L1-L2*L2+Dist*Dist)/(2*Dist);const FVector Old=A.P[K]-A.P[H],U=(Old-Axis*FVector::DotProduct(Old,Axis)).GetSafeNormal();if(U.IsNearlyZero())return false;Z.P[K]=Z.P[H]+Axis*Along+U*FMath::Sqrt(FMath::Max(0.,L1*L1-Along*Along));
    Z.Q[H]=(FQuat::FindBetweenVectors(A.P[K]-A.P[H],Z.P[K]-Z.P[H])*A.Q[H]).GetNormalized();Z.Q[K]=(FQuat::FindBetweenVectors(A.P[E]-A.P[K],Z.P[E]-Z.P[K])*A.Q[K]).GetNormalized();
   }
  }return true;
 };
 auto Errors=[&](const TArray<FPose>& P,double* Group){double Sum[4]={};int Count[4]={};double Cost=0;int Evidence=0;
  for(int F=0;F<N;++F)if(D.ValidFrame[F])for(int I=1;I<13;++I){const int O=F*75+Keys[I]*3;const double Conf=D.Keypoints[O+2];if(Conf<=.7)continue;const double Error=(Project(P[F].P[I])-FVector2D(D.Keypoints[O],D.Keypoints[O+1])).Size();const int C=I>=7?3:(I==1||I==4?0:(I==3||I==6?1:2));Sum[C]+=Error;++Count[C];if(C<2){const double E=Error*FMath::Clamp((Conf-.7)/.25,0.,1.)/10.;Cost+=200*(FMath::Sqrt(1+E*E)-1);++Evidence;}}
  for(int C=0;C<4;++C)Group[C]=Count[C]?Sum[C]/Count[C]:0.;return Evidence?Cost/Evidence:TNumericLimits<double>::Max();
 };
 const double Time=30./FPS;double OldGroup[4];Errors(Current,OldGroup);
 auto Safe=[&](const TArray<FPose>& P){
  for(int F=1;F<N;++F)for(int I:{1,2,4,5}){if(Angle(P[F-1].Q[I],P[F].Q[I])-Angle(Base[F-1].Q[I],Base[F].Q[I])>Time+1.e-6)return false;if(F>1){const double B=Acc(Base,F,I);if(Acc(P,F,I)-B>FMath::Max(.5*Time*Time,.05*B)+1.e-6)return false;}}
  for(int F=2;F<N;++F)for(int I:{2,5,8,11}){const double B=(Base[F].P[I]-2*Base[F-1].P[I]+Base[F-2].P[I]).Size();if((P[F].P[I]-2*P[F-1].P[I]+P[F-2].P[I]).Size()-B>FMath::Max(.25*Time*Time,.05*B)+1.e-6)return false;}
  return true;
 };
 // Bounded deterministic directions. No dependency on a Python/model environment.
 TArray<FOffsets> Directions;for(int I=0;I<7;++I)for(int Sign:{-1,1}){FOffsets V;V.V[I]=Sign;Directions.Add(V);}FRandomStream Random(4107);
 for(int I=0;I<96;++I){FOffsets V;double Norm=0;for(double& X:V.V){X=Random.FRand()*2-1;Norm+=X*X;}for(double& X:V.V)X/=FMath::Sqrt(Norm);Directions.Add(V);}
 FOffsets Best;TArray<FPose> Candidate,Final=Current;double BestCost=Errors(Current,OldGroup);int Accepted=0;
 for(double Length:{.5,.2,.1,.05,.02})for(int Pass=0;Pass<2;++Pass){bool Changed=false;FOffsets Choice=Best;double ChoiceCost=BestCost;
  for(const auto& Dir:Directions){if(D.ShouldCancel&&D.ShouldCancel())return false;FOffsets V=Best;bool Bound=true;double Penalty=0;for(int I=0;I<7;++I){V.V[I]+=Dir.V[I]*Length;Bound&=FMath::Abs(V.V[I])<=(I<3?3.:5.);Penalty+=V.V[I]*V.V[I];}if(!Bound||!Make(V,Candidate))continue;double Group[4];const double Cost=Errors(Candidate,Group)+.01*Penalty;if(Cost>=ChoiceCost-1.e-6)continue;bool Good=true;for(int I=0;I<4;++I)Good&=Group[I]<=OldGroup[I]+1.e-6;if(!Good||!Safe(Candidate))continue;Choice=V;ChoiceCost=Cost;Changed=true;}
  if(!Changed)break;Best=Choice;BestCost=ChoiceCost;++Accepted;
 }
 // Improve along a projected cost gradient inside linearized feasibility half-spaces.
 // Every candidate is still accepted only by the exact nonlinear checks above.
 auto Constraints=[&](const FOffsets& V,TArray<double>& Out){TArray<FPose> P;if(!Make(V,P,true))return false;Out.Reset();double Group[4];Errors(P,Group);for(int I=0;I<4;++I)Out.Add(OldGroup[I]-Group[I]+1.e-7);
  for(int F=0;F<N;++F)for(int H:{1,4}){const int K=H+1,E=H+2;const double L1=(Current[F].P[K]-Current[F].P[H]).Size(),L2=(Current[F].P[E]-Current[F].P[K]).Size(),Dist=(P[F].P[E]-P[F].P[H]).Size();Out.Add(L1+L2-Dist);Out.Add(Dist-FMath::Abs(L1-L2)-.001);}
  for(int F=1;F<N;++F)for(int I:{1,2,4,5}){Out.Add(Time-Angle(P[F-1].Q[I],P[F].Q[I])+Angle(Base[F-1].Q[I],Base[F].Q[I])+1.e-7);if(F>1){const double B=Acc(Base,F,I);Out.Add(FMath::Max(.5*Time*Time,.05*B)-Acc(P,F,I)+B+1.e-7);}}
  for(int F=2;F<N;++F)for(int I:{2,5,8,11}){const double B=(Base[F].P[I]-2*Base[F-1].P[I]+Base[F-2].P[I]).Size();Out.Add(FMath::Max(.25*Time*Time,.05*B)-(P[F].P[I]-2*P[F-1].P[I]+P[F-2].P[I]).Size()+B+1.e-7);}
  for(int I=0;I<7;++I){const double Limit=I<3?3.:5.;Out.Add(Limit-V.V[I]);Out.Add(Limit+V.V[I]);}return true;
 };
 for(int Iter=0;Iter<16;++Iter){if(D.ShouldCancel&&D.ShouldCancel())return false;TArray<double> Margins;if(!Constraints(Best,Margins))break;TArray<FOffsets> Jac;Jac.SetNum(Margins.Num());FOffsets Gradient;const double Eps=.001;
  for(int I=0;I<7;++I){FOffsets V=Best;V.V[I]+=Eps;TArray<double> M;if(!Constraints(V,M)){Report=TEXT("{\"applied\":false,\"reason\":\"placement linearization unavailable\"}");return true;}for(int J=0;J<M.Num();++J)Jac[J].V[I]=(M[J]-Margins[J])/Eps;Make(V,Candidate,true);double Group[4],Penalty=0;for(double X:V.V)Penalty+=X*X;Gradient.V[I]=(Errors(Candidate,Group)+.01*Penalty-BestCost)/Eps;}
  double Norm=0;for(double X:Gradient.V)Norm+=X*X;if(Norm<1.e-12)break;FOffsets Direction;for(int I=0;I<7;++I)Direction.V[I]=-.5*Gradient.V[I]/FMath::Sqrt(Norm);
  for(int Pass=0;Pass<48;++Pass){bool Changed=false;for(int J=0;J<Margins.Num();++J){double Margin=Margins[J],Den=0;for(int I=0;I<7;++I){Margin+=Jac[J].V[I]*Direction.V[I];Den+=FMath::Square(Jac[J].V[I]);}if(Margin<0&&Den>1.e-12){for(int I=0;I<7;++I)Direction.V[I]-=Margin*Jac[J].V[I]/Den;Changed=true;}}if(!Changed)break;}
  bool Changed=false;for(double Gain=1;Gain>=1./2048;Gain*=.5){FOffsets V=Best;double Penalty=0;bool Bound=true;for(int I=0;I<7;++I){V.V[I]+=Direction.V[I]*Gain;Penalty+=V.V[I]*V.V[I];Bound&=FMath::Abs(V.V[I])<=(I<3?3.:5.);}if(!Bound||!Make(V,Candidate))continue;double Group[4];const double Cost=Errors(Candidate,Group)+.01*Penalty;bool Good=Cost<BestCost-1.e-6;for(int I=0;I<4;++I)Good&=Group[I]<=OldGroup[I]+1.e-6;if(Good&&Safe(Candidate)){Best=V;BestCost=Cost;++Accepted;Changed=true;break;}}
  if(!Changed)break;
 }
 Make(Best,Final);const TArray<FPose> FixedPose=Final;FOffsets Zero;double DynamicGroup[4];Errors(FixedPose,DynamicGroup);double FixedGroup[4];for(int I=0;I<4;++I)FixedGroup[I]=DynamicGroup[I];int DriftAccepted=0;
 auto EdgeError=[](FVector2D A,FVector2D B){return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(A,B)/FMath::Max(1.e-10,A.Size()*B.Size()),-1.,1.)));};
 const double PointBudget=FMath::Max(.25,.005*ImageCenter.Y);
 TArray<double> FixedPointError,FixedEdgeError;FixedPointError.Init(-1,N*4);FixedEdgeError.Init(-1,N*2);
 for(int F=0;F<N;++F)if(D.ValidFrame[F])for(int C=0;C<2;++C){const int H=1+C*3,K=H+1;FVector2D Target[2];bool Both=true;
  for(int J=0;J<2;++J){const int I=H+J,O=F*75+Keys[I]*3;Target[J]=FVector2D(D.Keypoints[O],D.Keypoints[O+1]);const bool Valid=D.Keypoints[O+2]>.7;Both&=Valid;if(Valid)FixedPointError[F*4+C*2+J]=(Project(FixedPose[F].P[I])-Target[J]).Size();}
  if(Both&&(Target[1]-Target[0]).Size()>10)FixedEdgeError[F*2+C]=EdgeError(Project(FixedPose[F].P[K])-Project(FixedPose[F].P[H]),Target[1]-Target[0]);
 }
 auto ImageSafe=[&](const TArray<FPose>& P){for(int F=0;F<N;++F)for(int C=0;C<2;++C){const int H=1+C*3,K=H+1;FVector2D Target[2],UV[2];for(int J=0;J<2;++J){const int I=H+J,O=F*75+Keys[I]*3;if(Angle(FixedPose[F].Q[I],P[F].Q[I])>5.)return false;Target[J]=FVector2D(D.Keypoints[O],D.Keypoints[O+1]);UV[J]=Project(P[F].P[I]);const double Before=FixedPointError[F*4+C*2+J];if(Before>=0&&(UV[J]-Target[J]).Size()>Before+PointBudget)return false;}
   const double Before=FixedEdgeError[F*2+C];if(Before>=0&&EdgeError(UV[1]-UV[0],Target[1]-Target[0])>Before+.5)return false;
  }return true;};
 FVector Depth(Camera[6],Camera[8],0);Depth.Normalize();const FVector Axes[]={Depth,FVector::UpVector};
 const int Radius=FMath::Max(3,FMath::RoundToInt(.6*FPS)),Spacing=FMath::Max(1,FMath::RoundToInt(.4*FPS));TArray<int> Centers;for(int F=0;F<N;F+=Spacing)Centers.Add(F);Centers.AddUnique(N-1);
 // Smooth image-conditioned root correction, with the foot trajectories held exactly.
 // Pelvis rotation is not observable reliably enough from the hip pair: keep it unchanged.
 for(double Amount:{.75,.35,.15})for(int Center:Centers)for(const FVector& Axis:Axes){
  if(D.ShouldCancel&&D.ShouldCancel())return false;const TArray<FVector> Previous=Drift;TArray<FVector> Choice;double ChoiceGroup[4];double Cost=DynamicGroup[0];
  for(int Sign:{-1,1}){Drift=Previous;for(int F=FMath::Max(0,Center-Radius);F<=FMath::Min(N-1,Center+Radius);++F)Drift[F]+=Axis*(Sign*Amount*.5*(1+FMath::Cos(UE_PI*(F-Center)/Radius)));
   bool Good=true;for(int F=0;F<N;++F){Good&=Drift[F].Size()<=3.;if(F>0)Good&=(Drift[F]-Drift[F-1]).Size()<=.2*Time;if(F>1)Good&=(Drift[F]-2*Drift[F-1]+Drift[F-2]).Size()<=.03*Time*Time;}if(!Good||!Make(Zero,Candidate,false,&FixedPose)||!Safe(Candidate)||!ImageSafe(Candidate))continue;
   double Group[4];Errors(Candidate,Group);Good=Group[0]<Cost-1.e-5;for(int I=0;I<4;++I)Good&=Group[I]<=FixedGroup[I]+1.e-6;
   if(Good){Choice=Drift;Cost=Group[0];for(int I=0;I<4;++I)ChoiceGroup[I]=Group[I];}
  }
  if(Choice.Num()){Drift=MoveTemp(Choice);for(int I=0;I<4;++I)DynamicGroup[I]=ChoiceGroup[I];++DriftAccepted;}else Drift=Previous;
 }
 Make(Zero,Final,false,&FixedPose);double NewGroup[4];Errors(Final,NewGroup);double MaxDrift=0;for(const FVector& X:Drift)MaxDrift=FMath::Max(MaxDrift,X.Size());

 // Reject sub-pixel noise-only gains rather than changing an otherwise good take.
 if(OldGroup[0]+OldGroup[1]-NewGroup[0]-NewGroup[1]<.05){Report=TEXT("{\"applied\":false,\"reason\":\"no meaningful safe placement improvement\"}");return true;}
 TArray<int> Bones={0,Id[1],Id[2],Id[3],Id[4],Id[5],Id[6]};const int IKL=Ref.FindBoneIndex(TEXT("ik_foot_l")),IKR=Ref.FindBoneIndex(TEXT("ik_foot_r"));for(int B:{IKL,IKR})if(B>=0&&Tracks[B].Num())Bones.AddUnique(B);
 TArray<TArray<FTransform>> Output;Output.SetNum(Bones.Num());
 for(int F=0;F<N;++F){FullPose(Tracks,F,L,G);const FVector Shift=FVector(Best.V[0],Best.V[1],Best.V[2])+Drift[F];L[0].AddToTranslation(Shift);for(int B=0;B<NB;++B)G[B].AddToTranslation(Shift);
  for(int C=0;C<2;++C){const int H=1+C*3;for(int I=H;I<H+3;++I){FTransform T=G[Id[I]];T.SetTranslation(Final[F].P[I]);T.SetRotation(Final[F].Q[I]);const int Parent=Ref.GetParentIndex(Id[I]);L[Id[I]].SetRotation((G[Parent].GetRotation().Inverse()*T.GetRotation()).GetNormalized());G[Id[I]]=T;}}
  for(int C=0;C<2;++C){const int B=C?IKR:IKL;if(B>=0&&Tracks[B].Num()){FTransform T=G[B];T.AddToTranslation(FVector(Best.V[3+C*2],Best.V[4+C*2],0)-Shift);L[B]=T.GetRelativeTransform(G[Ref.GetParentIndex(B)]);}}
  TArray<FTransform> Check=L;for(int B=1;B<NB;++B)Check[B]=Check[B]*Check[Ref.GetParentIndex(B)];
  for(int I=0;I<13;++I)if(!Check[Id[I]].GetTranslation().Equals(Final[F].P[I],.001)||Check[Id[I]].GetRotation().AngularDistance(Final[F].Q[I])>.00001){Report=TEXT("{\"applied\":false,\"reason\":\"target FK preservation check failed\"}");return true;}
  for(int I=0;I<Bones.Num();++I)Output[I].Add(L[Bones[I]]);
 }
 auto& Ctrl=Anim->GetController();Ctrl.OpenBracket(FText::FromString(TEXT("Contact preserving target placement")),false);bool OK=true;
 for(int I=0;I<Bones.Num();++I){TArray<FVector3f>P,S;TArray<FQuat4f>Q;for(const auto& T:Output[I]){P.Add(FVector3f(T.GetTranslation()));S.Add(FVector3f(T.GetScale3D()));Q.Add(FQuat4f(T.GetRotation()));}OK=Ctrl.SetBoneTrackKeys(Ref.GetBoneName(Bones[I]),P,Q,S,false)&&OK;}Ctrl.CloseBracket(false);
 Report=FString::Printf(TEXT("{\"applied\":%s,\"accepted_steps\":%d,\"root_shift_cm\":[%.8f,%.8f,%.8f],\"foot_horizontal_shift_cm\":[%.8f,%.8f,%.8f,%.8f],\"hip_error_before_after_px\":[%.8f,%.8f],\"foot_error_before_after_px\":[%.8f,%.8f]}"),OK?TEXT("true"):TEXT("false"),Accepted,Best.V[0],Best.V[1],Best.V[2],Best.V[3],Best.V[4],Best.V[5],Best.V[6],OldGroup[0],NewGroup[0],OldGroup[1],NewGroup[1]);Report.LeftChopInline(1);Report+=FString::Printf(TEXT(",\"dynamic_root_accepted_steps\":%d,\"max_dynamic_root_offset_cm\":%.8f,\"pelvis_rotation_preserved\":true}"),DriftAccepted,MaxDrift);return OK;
}
