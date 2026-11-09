// Hand postprocess. Uses the existing job lifetime/cancellation and never rewrites the input asset.
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
static TArray<FName> CBMFingerNames(){
 TArray<FName> Out;
 for(const TCHAR* Side:{TEXT("l"),TEXT("r")})for(const TCHAR* Finger:{TEXT("index"),TEXT("middle"),TEXT("pinky"),TEXT("ring"),TEXT("thumb")})for(int32 I=1;I<=3;++I)
  Out.Add(FName(*FString::Printf(TEXT("%s_%02d_%s"),Finger,I,Side)));
 return Out;
}
static bool CBMWriteJson(const FString& File,const TSharedRef<FJsonObject>& O){FString S;auto W=TJsonWriterFactory<>::Create(&S);return FJsonSerializer::Serialize(O,W)&&FFileHelper::SaveStringToFile(S,*File);}
static TSharedPtr<FJsonObject> CBMReadJson(const FString& File){FString S;TSharedPtr<FJsonObject> O;if(FFileHelper::LoadFileToString(S,*File))FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O);return O;}
static bool CBMPalmFrame(const FReferenceSkeleton& Ref,const TArray<FTransform>& Global,int32 Side,FQuat& Out){
 const TCHAR* S=Side?TEXT("r"):TEXT("l");
 auto Index=[&](const TCHAR* Prefix){return Ref.FindBoneIndex(FName(*FString::Printf(TEXT("%s_%s"),Prefix,S)));};
 const int32 W=Index(TEXT("hand")),I=Index(TEXT("index_01")),M=Index(TEXT("middle_01")),P=Index(TEXT("pinky_01"));
 if(W==INDEX_NONE||I==INDEX_NONE||M==INDEX_NONE||P==INDEX_NONE)return false;
 const FVector X=(Global[I].GetTranslation()-Global[P].GetTranslation()).GetSafeNormal();
 const FVector Middle=Global[M].GetTranslation()-Global[W].GetTranslation();
 const FVector Y=(Middle-X*FVector::DotProduct(Middle,X)).GetSafeNormal();
 if(X.IsNearlyZero()||Y.IsNearlyZero())return false;
 Out=FRotationMatrix::MakeFromXY(X,Y).ToQuat().GetNormalized();return true;
}

// With ContactContext, prepare read-only target geometry/calibration and return
// before inference validation, diagnostics or asset creation. All UObject reads
// stay on the game thread; the contact optimization runs in the external worker.
static bool CBMBakeHands(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,const TSharedPtr<FJsonObject>& O,FString& Error,FJsonObject* ContactContext=nullptr){
 check(IsInGameThread());
 auto Fail=[&](const FString& S){Error=S;return false;};
 if(J->Cancel.Load()||(GEditor&&GEditor->PlayWorld))return Fail(TEXT("Cancelled or PIE started; original preserved."));
 auto* Original=J->Original.Get();const auto* Model=Original->GetDataModel();const int32 N=Model->GetNumberOfKeys();
 if(Original->GetOutermost()->IsDirty()||Model->GenerateGuid()!=J->OriginalGuid)return Fail(TEXT("Original animation was edited during inference; no result applied."));
 const TArray<TSharedPtr<FJsonValue>> *Q=nullptr,*Weights=nullptr;double Version=0,Count=0,FPS=0;FString SourceAsset;
 if(!ContactContext&&(!O||!O->TryGetNumberField(TEXT("version"),Version)||Version!=1||!O->TryGetNumberField(TEXT("frames"),Count)||Count!=N||!O->TryGetNumberField(TEXT("fps"),FPS)||!FMath::IsNearlyEqual(FPS,Model->GetFrameRate().AsDecimal(),.001)||!O->TryGetStringField(TEXT("source_asset"),SourceAsset)||SourceAsset!=Original->GetPathName()||!O->TryGetArrayField(TEXT("quaternions_ue"),Q)||Q->Num()!=N*30*4||!O->TryGetArrayField(TEXT("weights"),Weights)||Weights->Num()!=N*2))
  return Fail(TEXT("Invalid hand result schema, source identity or frame count."));
 TArray<FQuat> Quats;TArray<float> Blend;Quats.Reserve(N*30);Blend.Reserve(N*2);
 if(!ContactContext){FString Backend;if(!O->TryGetStringField(TEXT("backend"),Backend)||Backend!=J->HandBackend)return Fail(TEXT("Hand backend metadata mismatch."));
  for(int32 I=0;I<Q->Num();I+=4){double V[4];for(int32 K=0;K<4;++K)if(!(*Q)[I+K]->TryGetNumber(V[K])||!FMath::IsFinite(V[K]))return Fail(TEXT("Non-finite hand rotation."));FQuat R(V[0],V[1],V[2],V[3]);if(FMath::Abs(R.SizeSquared()-1)>.02)return Fail(TEXT("Non-unit hand rotation."));Quats.Add(R.GetNormalized());}
  for(const auto& X:*Weights){double V;if(!X->TryGetNumber(V)||!FMath::IsFinite(V)||V<0||V>1)return Fail(TEXT("Invalid hand blend weight."));Blend.Add(V*J->HandStrength);}
 }
 auto Names=CBMFingerNames();const auto& Ref=J->Source->GetRefSkeleton();
 TArray<int32> SourceIndices,TargetIndices;
 for(const FName Name:Names){const int32 S=Ref.FindBoneIndex(Name),T=J->Target->GetRefSkeleton().FindBoneIndex(Name);if(S==INDEX_NONE||T==INDEX_NONE||!Model->IsValidBoneTrackName(Name))return Fail(TEXT("Missing source/target finger track: ")+Name.ToString());SourceIndices.Add(S);TargetIndices.Add(T);}
 TArray<int32> HandSides;for(int32 B=0;B<30;++B)HandSides.Add(B/15);
 // Manny has metacarpals absent from SMPL. They are part of the hand output:
 // retaining their old fuzzy-mapped MCP animation moves finger roots into the
 // wrist even when all three phalanx rotations are subsequently corrected.
 for(int32 Side=0;Side<2;++Side)for(const TCHAR* Finger:{TEXT("index"),TEXT("middle"),TEXT("pinky"),TEXT("ring")}){
  const FName Name(*FString::Printf(TEXT("%s_metacarpal_%s"),Finger,Side?TEXT("r"):TEXT("l")));
  const int32 T=J->Target->GetRefSkeleton().FindBoneIndex(Name);if(T==INDEX_NONE)continue;
  if(!Model->IsValidBoneTrackName(Name))return Fail(TEXT("Original lacks palm track: ")+Name.ToString());
  Names.Add(Name);TargetIndices.Add(T);HandSides.Add(Side);
 }
 FRetargetProfile Profile;Profile.FillProfileWithAssetSettings(J->Retargeter.Get());
 TArray<FIKRetargetFKChainsOpSettings*> FKSettings;Profile.GetOpSettingsByTypeInProfile(FKSettings);
 for(auto* FK:FKSettings)for(auto& Chain:FK->ChainsToRetarget){
  const FString Name=Chain.TargetChainName.ToString();
  // Fuzzy auto-mapping also maps Manny's metacarpal chains to the SAME SMPL
  // finger chain. This applies MCP motion twice. Clear the mapping itself;
  // removing its settings is insufficient because IK Rig recreates defaults.
  if(Name.Contains(TEXT("Metacarpal")))FK->ChainMapping.SetChainMapping(Chain.TargetChainName,NAME_None);
  else for(const TCHAR* Side:{TEXT("Left"),TEXT("Right")})for(const TCHAR* Finger:{TEXT("Index"),TEXT("Middle"),TEXT("Pinky"),TEXT("Ring"),TEXT("Thumb")})
   if(Name.Equals(FString(Side)+Finger,ESearchCase::IgnoreCase))Chain.RotationMode=EFKChainRotationMode::OneToOne;
 }
 FIKRetargetProcessor Processor;
 FRetargetInitParameters Init;Init.SourceSkeletalMesh=J->Source.Get();Init.TargetSkeletalMesh=J->Target.Get();Init.RetargeterAsset=J->Retargeter.Get();Init.CustomProfile=&Profile;Processor.Initialize(Init);
 if(!Processor.IsInitialized())return Fail(TEXT("Hand IK Retargeter initialization failed."));
 TArray<TArray<FVector3f>> Positions,Scales;TArray<TArray<FQuat4f>> Rotations;Positions.SetNum(Names.Num());Scales.SetNum(Names.Num());Rotations.SetNum(Names.Num());
 for(int32 B=0;B<Names.Num();++B){TArray<FTransform> Keys;Model->GetBoneTrackTransforms(Names[B],Keys);if(Keys.Num()!=N)return Fail(TEXT("Unexpected original key count."));for(const auto& K:Keys){Positions[B].Add(FVector3f(K.GetTranslation()));Scales[B].Add(FVector3f(K.GetScale3D()));Rotations[B].Add(FQuat4f(K.GetRotation()));}}
 const auto& TargetSkeleton=Processor.GetTargetSkeleton();
 const auto& TargetRef=J->Target->GetRefSkeleton();
 // FK auto-alignment matches segment directions in world space, but the two
 // skeletons' palms can still be rolled differently. Transfer hand motion in
 // measured palm frames, leaving wrist/body and metacarpal anchors untouched.
 TArray<FTransform> SourceRestGlobal=Ref.GetRefBonePose();
 for(int32 B=1;B<Ref.GetNum();++B)SourceRestGlobal[B]=SourceRestGlobal[B]*SourceRestGlobal[Ref.GetParentIndex(B)];
 const auto& TargetRestGlobal=TargetSkeleton.RetargetPoses.GetGlobalRetargetPose();
 FQuat PalmCorrection[2];
 for(int32 Side=0;Side<2;++Side){FQuat SourcePalm,TargetPalm;
  if(!CBMPalmFrame(Ref,SourceRestGlobal,Side,SourcePalm)||!CBMPalmFrame(TargetRef,TargetRestGlobal,Side,TargetPalm))return Fail(TEXT("Cannot resolve source/target palm frame."));
  PalmCorrection[Side]=(TargetPalm*SourcePalm.Inverse()).GetNormalized();
 }
 TArray<FQuat> TerminalCorrection;TerminalCorrection.Init(FQuat::Identity,Names.Num());
 for(int32 B=2;B<30;B+=3){
  // Terminal phalanges have no child for FK auto-align. Manny's longitudinal
  // axis is the signed local X axis of its incoming bind translation. Match
  // the source's terminal direction convention without adding a curl limit.
  const FVector LocalAxis=TargetRef.GetRefBonePose()[TargetIndices[B]].GetTranslation().GetSafeNormal();
  const FQuat TargetRestQ=TargetRestGlobal[TargetIndices[B]].GetRotation();
  const FVector SourceDirection=(SourceRestGlobal[SourceIndices[B]].GetTranslation()-SourceRestGlobal[SourceIndices[B-1]].GetTranslation()).GetSafeNormal();
  if(LocalAxis.IsNearlyZero()||SourceDirection.IsNearlyZero())return Fail(TEXT("Degenerate terminal finger axis."));
  const FQuat Align=FQuat::FindBetweenNormals(TargetRestQ.RotateVector(LocalAxis),SourceDirection);
  TerminalCorrection[B]=(TargetRestQ.Inverse()*Align*TargetRestQ).GetNormalized();
 }
 TArray<TArray<FTransform>> OriginalTracks;OriginalTracks.SetNum(TargetRef.GetNum());
 for(int32 B=0;B<TargetRef.GetNum();++B){const FName Bone=TargetRef.GetBoneName(B);if(Model->IsValidBoneTrackName(Bone))Model->GetBoneTrackTransforms(Bone,OriginalTracks[B]);}
 if(ContactContext){
  ContactContext->SetNumberField(TEXT("version"),1);ContactContext->SetNumberField(TEXT("strength"),J->HandStrength);ContactContext->SetStringField(TEXT("target"),J->Target->GetPathName());
  auto Values=[](std::initializer_list<double> V){TArray<TSharedPtr<FJsonValue>> A;for(double X:V)A.Add(MakeShared<FJsonValueNumber>(X));return A;};
  auto QV=[&](const FQuat& Qr){return MakeShared<FJsonValueArray>(Values({Qr.X,Qr.Y,Qr.Z,Qr.W}));};
  auto PV=[&](const FVector& P){return MakeShared<FJsonValueArray>(Values({P.X,P.Y,P.Z}));};
  TArray<FTransform> Bind=SourceRestGlobal;Processor.ApplySourceScaleToPose(Bind);FRetargetRunParameters Run;Run.SourceGlobalPose=&Bind;Run.Profile=&Profile;Run.DeltaTime=1.f/J->FPS;
  const TArray<FTransform> TargetBind=Processor.RunRetargeter(Run);
  int32 Hands[2]={TargetRef.FindBoneIndex(TEXT("hand_l")),TargetRef.FindBoneIndex(TEXT("hand_r"))};
  TArray<TSharedPtr<FJsonValue>> SourceParents,SourceLocal,SourceParentQ,Left,Right,Meta,Parents,JsonNames,Ends,Radii;
  for(int32 Side=0;Side<2;++Side)Left.Add(QV((TargetBind[Hands[Side]].GetRotation().Inverse()*PalmCorrection[Side]).GetNormalized()));
  TArray<FTransform> TargetBindMesh=TargetRef.GetRefBonePose();for(int32 B=1;B<TargetRef.GetNum();++B)TargetBindMesh[B]=TargetBindMesh[B]*TargetBindMesh[TargetRef.GetParentIndex(B)];
  TArray<TArray<FVector>> Samples;Samples.SetNum(30);
  const FSkeletalMeshModel* Imported=J->Target->GetImportedModel();
  if(!Imported||Imported->LODModels.IsEmpty())return Fail(TEXT("Contact geometry needs target LOD0 source data."));
  for(const auto& Section:Imported->LODModels[0].Sections)for(const auto& V:Section.SoftVertices){
   int32 Best=0;for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K)if(V.InfluenceWeights[K]>V.InfluenceWeights[Best])Best=K;
   if(!Section.BoneMap.IsValidIndex(V.InfluenceBones[Best]))continue;
   const int32 Bone=Section.BoneMap[V.InfluenceBones[Best]],B=TargetIndices.Find(Bone);
   if(B>=0&&B<30)Samples[B].Add(TargetBindMesh[Bone].InverseTransformPosition(FVector(V.Position)));
  }
  for(int32 B=0;B<Names.Num();++B){
   JsonNames.Add(MakeShared<FJsonValueString>(Names[B].ToString()));const int32 Parent=TargetRef.GetParentIndex(TargetIndices[B]);int32 ParentB=TargetIndices.Find(Parent);
   if(ParentB==INDEX_NONE){if(Parent==Hands[0])ParentB=38;else if(Parent==Hands[1])ParentB=39;else return Fail(TEXT("Unsupported hand contact hierarchy."));}Parents.Add(MakeShared<FJsonValueNumber>(ParentB));
   if(B>=30){Meta.Add(QV((TargetBind[Hands[HandSides[B]]].GetRotation().Inverse()*TargetBind[TargetIndices[B]].GetRotation()).GetNormalized()));continue;}
   const int32 SP=Ref.GetParentIndex(SourceIndices[B]);SourceParents.Add(MakeShared<FJsonValueNumber>(SourceIndices.Find(SP)));SourceLocal.Add(QV(Ref.GetRefBonePose()[SourceIndices[B]].GetRotation()));SourceParentQ.Add(QV(SourceRestGlobal[SP].GetRotation()));
   Right.Add(QV((Bind[SourceIndices[B]].GetRotation().Inverse()*TargetBind[TargetIndices[B]].GetRotation()*TerminalCorrection[B]).GetNormalized()));
   FVector End=B%3<2?TargetRef.GetRefBonePose()[TargetIndices[B+1]].GetTranslation():TargetRef.GetRefBonePose()[TargetIndices[B]].GetTranslation();FVector Axis=End.GetSafeNormal();
   if(Samples[B].Num()<8||Axis.IsNearlyZero())return Fail(TEXT("Insufficient target finger surface data."));
   if(B%3==2){TArray<double> Along;for(const auto& P:Samples[B])Along.Add(FVector::DotProduct(P,Axis));Along.Sort();End=Axis*Along[FMath::FloorToInt(.95*(Along.Num()-1))];}
   TArray<double> Width;for(const auto& P:Samples[B]){double T=FVector::DotProduct(P,Axis);if(T>0&&T<End.Length())Width.Add((P-Axis*T).Length());}
   if(Width.IsEmpty()||End.Length()<.01)return Fail(TEXT("Degenerate target finger contact geometry."));Width.Sort();Ends.Add(PV(End));Radii.Add(MakeShared<FJsonValueNumber>(Width[FMath::FloorToInt(.65*(Width.Num()-1))]));
  }
  if(Names.Num()!=38)return Fail(TEXT("Contact calibration requires the complete metacarpal hierarchy."));
  // Verify non-identity, combined-chain probes against the actual retargeter.
  // A changed retarget profile must not silently use an invalid analytic FK.
  double MappingError=0;
  for(int32 Probe=0;Probe<3;++Probe){TArray<FTransform> Local=Ref.GetRefBonePose();
   for(int32 B=0;B<30;++B){FVector Axis=Probe==0?FVector::ForwardVector:Probe==1?FVector::RightVector:FVector::UpVector;Local[SourceIndices[B]].SetRotation((Local[SourceIndices[B]].GetRotation()*FQuat(Axis,.15+.013*B)).GetNormalized());}
   TArray<FTransform> Global=Local;for(int32 B=1;B<Ref.GetNum();++B)Global[B]=Local[B]*Global[Ref.GetParentIndex(B)];Processor.ApplySourceScaleToPose(Global);Run.SourceGlobalPose=&Global;
   const TArray<FTransform> Result=Processor.RunRetargeter(Run);
   for(int32 B=0;B<30;++B){const int32 Side=B/15;const FQuat A=TargetBind[Hands[Side]].GetRotation().Inverse()*PalmCorrection[Side];const FQuat C=Bind[SourceIndices[B]].GetRotation().Inverse()*TargetBind[TargetIndices[B]].GetRotation()*TerminalCorrection[B];
    const FQuat Predicted=(A*Global[SourceIndices[B]].GetRotation()*C).GetNormalized();const FQuat Actual=(Result[Hands[Side]].GetRotation().Inverse()*PalmCorrection[Side]*Result[TargetIndices[B]].GetRotation()*TerminalCorrection[B]).GetNormalized();MappingError=FMath::Max(MappingError,Predicted.AngularDistance(Actual));}
  }
  if(MappingError>.0001)return Fail(TEXT("Target contact FK does not match the active retarget profile."));ContactContext->SetNumberField(TEXT("calibration_probe_max_radians"),MappingError);
  ContactContext->SetArrayField(TEXT("names"),JsonNames);ContactContext->SetArrayField(TEXT("parents"),Parents);ContactContext->SetArrayField(TEXT("source_parents"),SourceParents);ContactContext->SetArrayField(TEXT("source_local_q"),SourceLocal);ContactContext->SetArrayField(TEXT("source_parent_q"),SourceParentQ);ContactContext->SetArrayField(TEXT("left_q"),Left);ContactContext->SetArrayField(TEXT("right_q"),Right);ContactContext->SetArrayField(TEXT("meta_q"),Meta);ContactContext->SetArrayField(TEXT("ends"),Ends);ContactContext->SetArrayField(TEXT("radii"),Radii);
  TArray<TSharedPtr<FJsonValue>> FramePositions,WristsP,WristsQ;
  for(int32 I=0;I<N;++I){TArray<FTransform> Global=TargetRef.GetRefBonePose();TArray<TSharedPtr<FJsonValue>> P,WP,WQ;
   for(int32 B=0;B<TargetRef.GetNum();++B){if(OriginalTracks[B].Num())Global[B]=OriginalTracks[B][FMath::Min(I,OriginalTracks[B].Num()-1)];if(!Global[B].GetScale3D().Equals(FVector::OneVector,.0001))return Fail(TEXT("Contact calibration does not support scaled bones."));if(B>0)Global[B]=Global[B]*Global[TargetRef.GetParentIndex(B)];}
   for(int32 B=0;B<Names.Num();++B)P.Add(PV(FVector(Positions[B][I])));
   for(int32 Side=0;Side<2;++Side){WP.Add(PV(Global[Hands[Side]].GetTranslation()));WQ.Add(QV(Global[Hands[Side]].GetRotation()));}
   FramePositions.Add(MakeShared<FJsonValueArray>(P));WristsP.Add(MakeShared<FJsonValueArray>(WP));WristsQ.Add(MakeShared<FJsonValueArray>(WQ));
  }
  ContactContext->SetArrayField(TEXT("positions"),FramePositions);ContactContext->SetArrayField(TEXT("wrists_p"),WristsP);ContactContext->SetArrayField(TEXT("wrists_q"),WristsQ);return true;
 }
 bool Diagnostics=false;O->TryGetBoolField(TEXT("diagnostics"),Diagnostics);
 TArray<TSharedPtr<FJsonValue>> DiagnosticFrames;
 TArray<FQuat> PreviousDelta;PreviousDelta.Init(FQuat::Identity,Names.Num());
 for(int32 I=0;I<N;++I){
  TArray<FTransform> Local=Ref.GetRefBonePose();
  for(int32 B=0;B<30;++B)Local[SourceIndices[B]].SetRotation((Local[SourceIndices[B]].GetRotation()*Quats[I*30+B]).GetNormalized());
  TArray<FTransform> Global=Local;
  for(int32 B=1;B<Ref.GetNum();++B)Global[B]=Local[B]*Global[Ref.GetParentIndex(B)];
  Processor.ApplySourceScaleToPose(Global);FRetargetRunParameters Run;Run.SourceGlobalPose=&Global;Run.Profile=&Profile;Run.DeltaTime=1.f/J->FPS;
  const auto& TargetGlobal=Processor.RunRetargeter(Run);TArray<FTransform> TargetLocal=TargetGlobal;TargetSkeleton.UpdateLocalTransformsBelowBone(0,TargetLocal,TargetGlobal);
  TArray<FTransform> OriginalGlobal=TargetRef.GetRefBonePose();
  for(int32 B=0;B<TargetRef.GetNum();++B){if(OriginalTracks[B].Num())OriginalGlobal[B]=OriginalTracks[B][FMath::Min(I,OriginalTracks[B].Num()-1)];if(B>0)OriginalGlobal[B]=OriginalGlobal[B]*OriginalGlobal[TargetRef.GetParentIndex(B)];}
  TArray<FQuat> DesiredGlobal;DesiredGlobal.SetNum(Names.Num());
  for(int32 B=0;B<Names.Num();++B){const int32 Hand=TargetRef.FindBoneIndex(HandSides[B]==0?TEXT("hand_l"):TEXT("hand_r"));
   const FQuat FingerGlobal=(B<30?PalmCorrection[HandSides[B]]:FQuat::Identity)*TargetGlobal[TargetIndices[B]].GetRotation()*TerminalCorrection[B];
   DesiredGlobal[B]=(OriginalGlobal[Hand].GetRotation()*TargetGlobal[Hand].GetRotation().Inverse()*FingerGlobal).GetNormalized();
  }
  TArray<TSharedPtr<FJsonValue>> FrameDiagnostic;
  for(int32 B=0;B<Names.Num();++B){
   const float A=Blend[I*2+HandSides[B]];
   const int32 Parent=TargetRef.GetParentIndex(TargetIndices[B]),FingerParent=TargetIndices.Find(Parent);
   const FQuat ParentRotation=FingerParent!=INDEX_NONE?DesiredGlobal[FingerParent]:OriginalGlobal[Parent].GetRotation();
   // Retargeter locals refer to its temporary wrist/metacarpal pose. The output
   // preserves the original wrist and corrects mapped metacarpal tracks, so rebase the desired
   // hand-relative global rotations onto those ACTUAL output parents.
   const FQuat R=(ParentRotation.Inverse()*DesiredGlobal[B]).GetNormalized();
   if(Diagnostics){auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("bone"),Names[B].ToString());
    auto QJson=[](const FQuat& Qr){TArray<TSharedPtr<FJsonValue>> V;for(double X:{Qr.X,Qr.Y,Qr.Z,Qr.W})V.Add(MakeShared<FJsonValueNumber>(X));return V;};
    D->SetArrayField(TEXT("source_global_q"),QJson(B<30?Global[SourceIndices[B]].GetRotation():FQuat::Identity));D->SetArrayField(TEXT("retarget_global_q"),QJson(TargetGlobal[TargetIndices[B]].GetRotation()));D->SetArrayField(TEXT("retarget_hand_q"),QJson(TargetGlobal[TargetRef.FindBoneIndex(HandSides[B]==0?TEXT("hand_l"):TEXT("hand_r"))].GetRotation()));D->SetArrayField(TEXT("old_local_q"),QJson(TargetLocal[TargetIndices[B]].GetRotation()));D->SetArrayField(TEXT("rebased_local_q"),QJson(R));
    const int32 H=TargetRef.FindBoneIndex(HandSides[B]==0?TEXT("hand_l"):TEXT("hand_r"));D->SetArrayField(TEXT("calibrated_hand_relative_q"),QJson((OriginalGlobal[H].GetRotation().Inverse()*DesiredGlobal[B]).GetNormalized()));FrameDiagnostic.Add(MakeShared<FJsonValueObject>(D));}
   if(R.ContainsNaN())return Fail(TEXT("Retargeter returned an invalid finger rotation."));
   const FQuat OriginalRotation(Rotations[B][I]);
   FQuat Delta=(OriginalRotation.Inverse()*R).GetNormalized();
   // Per-frame shortest-path blending switches direction at a 180-degree
   // original/enhanced difference. Preserve the delta's temporal hemisphere
   // instead. Re-anchor only at zero influence, where either branch produces
   // the exact original and no accumulated full turn needs to be retained.
   const bool Reanchor=I==0||A<=0||Blend[(I-1)*2+HandSides[B]]<=0;
   if((Reanchor?Delta.W:(Delta|PreviousDelta[B]))<0)Delta=Delta*-1.;
   const FQuat PriorDelta=PreviousDelta[B];
   PreviousDelta[B]=Delta;
   if(A<=0)continue;
   // Quaternion power is full-path identity interpolation without division by
   // sin(pi). Retain the previous axis at the negative-identity singularity.
   FVector Axis(Delta.X,Delta.Y,Delta.Z);const double SinHalf=Axis.Length();
   if(SinHalf>1.e-8)Axis/=SinHalf;
   else{Axis=FVector(PriorDelta.X,PriorDelta.Y,PriorDelta.Z).GetSafeNormal();if(Axis.IsNearlyZero())Axis=FVector::ForwardVector;}
   const FQuat Partial(Axis,2.*FMath::Atan2(SinHalf,Delta.W)*A);
   Rotations[B][I]=FQuat4f((OriginalRotation*Partial).GetNormalized());
  }
  if(Diagnostics)DiagnosticFrames.Add(MakeShared<FJsonValueArray>(FrameDiagnostic));
 }
 if(Diagnostics){auto D=MakeShared<FJsonObject>();D->SetArrayField(TEXT("frames"),DiagnosticFrames);CBMWriteJson(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run/TEXT("retarget_stages.json"),D);}
 const FString Name=Original->GetName()+TEXT("_")+J->HandBackend+TEXT("_")+J->Run.Right(32).Left(8),PackageName=J->OutputFolder/TEXT("Hands")/Name;
 if(FPackageName::DoesPackageExist(PackageName))return Fail(TEXT("Output already exists; preserving it."));
 UAnimSequence* Output=DuplicateObject<UAnimSequence>(Original,CreatePackage(*PackageName),*Name);
 if(!Output)return Fail(TEXT("Cannot duplicate original animation."));
 Output->SetFlags(RF_Public|RF_Standalone);auto& Controller=Output->GetController();Controller.OpenBracket(FText::FromString(TEXT("Cubeless hand rotations")),false);
 bool Applied=true;for(int32 B=0;B<Names.Num();++B)Applied=Controller.SetBoneTrackKeys(Names[B],Positions[B],Rotations[B],Scales[B],false)&&Applied;Controller.CloseBracket(false);
 if(!Applied)return Fail(TEXT("Could not write finger keys. Unsaved output retained for inspection."));
 // Inspect EVERY untouched track, including wrists, root, pelvis, IK and auxiliary bones.
 TArray<FName> AllNames;Model->GetBoneTrackNames(AllNames);int32 Preserved=0;
 for(const auto& Bone:AllNames){if(Names.Contains(Bone))continue;TArray<FTransform> A,B;Model->GetBoneTrackTransforms(Bone,A);Output->GetDataModel()->GetBoneTrackTransforms(Bone,B);if(A.Num()!=B.Num())return Fail(TEXT("Body preservation check failed."));for(int32 I=0;I<A.Num();++I)if(!A[I].Equals(B[I],0))return Fail(TEXT("Non-finger track changed: ")+Bone.ToString());++Preserved;}
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;const auto File=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
 if(!UPackage::SavePackage(Output->GetOutermost(),Output,*File,Args))return Fail(TEXT("Cannot save hand-enhanced output."));FAssetRegistryModule::AssetCreated(Output);
 auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("original"),Original->GetPathName());Report->SetStringField(TEXT("enhanced"),Output->GetPathName());Report->SetNumberField(TEXT("preserved_nonfinger_tracks"),Preserved);Report->SetNumberField(TEXT("frames"),N);Report->SetNumberField(TEXT("nonfinger_max_difference"),0);Report->SetNumberField(TEXT("edited_hand_tracks"),Names.Num());Report->SetStringField(TEXT("preservation_scope"),TEXT("All tracks outside fingers and metacarpals"));
 Report->SetStringField(TEXT("hand_calibration"),TEXT("palm_frame_terminal_axis_v1"));Report->SetStringField(TEXT("backend"),J->HandBackend);if(O->HasTypedField<EJson::Object>(TEXT("provenance")))Report->SetObjectField(TEXT("provenance"),O->GetObjectField(TEXT("provenance")));
 CBMWriteJson(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run/TEXT("preservation.json"),Report);
 if(J->Settings.IsValid()){
  J->Settings->JointComparisonSource=nullptr;J->Settings->JointCorrectedAnimation=nullptr;J->Settings->JointCorrectionSource=nullptr;J->Settings->JointCorrectionReport.FilePath.Empty();
  if(J->Settings->HandComparisonBase!=Original){J->Settings->WiLoRAnimation=nullptr;J->Settings->HandFlowAnimation=nullptr;J->Settings->MediaPipeAnimation=nullptr;}
  J->Settings->HandComparisonBase=Original;
  if(J->HandBackend==TEXT("WiLoR"))J->Settings->WiLoRAnimation=Output;
  else if(J->HandBackend==TEXT("HandFlow"))J->Settings->HandFlowAnimation=Output;
  else J->Settings->MediaPipeAnimation=Output;
  J->Settings->OriginalAnimation=Original;J->Settings->HandEnhancedAnimation=Output;J->Settings->PreviewAnimation=Output;J->Settings->HandEnhancedBackend=J->HandBackend;}
 Error=TEXT("Saved ")+Output->GetPathName()+TEXT(". Original body/wrists preserved; use comparison buttons.");return true;
}

#include "CBMHandInputCache.inl"

static bool CBMSolveHands(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,FString& Message){
 const FString Work=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run);
 auto Fail=[&](const FString& S){Message=S;return false;};ON_SCOPE_EXIT{ReleasePackages();};
 if(!J->SharedHandFrames){
 J->Dir=Work/TEXT("Frames");IFileManager::Get().MakeDirectory(*J->Dir,true);
 {FCBMStage Stage(J,TEXT("Hand frame decode"),.02);
  const FString Args=FString::Printf(TEXT("-nostdin -v error -ss %.6f -i \"%s\" -vf fps=%d -frames:v %d -start_number 0 \"%s/%%08d.png\""),J->StartSeconds,*J->Video,J->FPS,J->FrameCount,*J->Dir);
  FProcHandle Proc=FPlatformProcess::CreateProc(*J->FFmpeg,*Args,false,true,true,nullptr,0,nullptr,nullptr);if(!Proc.IsValid())return Fail(TEXT("FFmpeg could not start."));ON_SCOPE_EXIT{FPlatformProcess::CloseProc(Proc);};
  while(FPlatformProcess::IsProcRunning(Proc)){if(J->Cancel.Load()){FPlatformProcess::TerminateProc(Proc,true);FPlatformProcess::WaitForProc(Proc);return Fail(TEXT("Cancelled."));}FPlatformProcess::Sleep(.05f);}int32 Code=-1;FPlatformProcess::GetProcReturnCode(Proc,&Code);if(Code)return Fail(TEXT("Hand frame decode failed."));
 }
 }
 TArray<FString> Files;IFileManager::Get().FindFiles(Files,*(J->Dir/TEXT("*.png")),true,false);Files.Sort();if(Files.Num()!=J->FrameCount)return Fail(TEXT("Video does not contain the original animation's full frame range."));
 for(int32 I=0;I<Files.Num();++I)if(Files[I]!=FString::Printf(TEXT("%08d.png"),I))return Fail(TEXT("Hand input frames must use sequential eight-digit names starting at zero."));
 {FCBMStage Stage(J,TEXT("Hand cue fingerprint"),.07);CBMPrepareHandCueCache(J,Files);}
 if(J->Cancel.Load())return Fail(TEXT("Cancelled."));
 TArray64<float> Keys;
 if(!CBMHandCueCache(J,Keys,false)){
 {FCBMStage Stage(J,TEXT("Hand assignment keypoints"),.08);Cubeless::FModelFactory A,B;const auto PA=ModelPath(TEXT("ViTPose")),PB=ModelPath(TEXT("ViTPosePost"));
  if(!Loaded(J,{PA,PB},[&](){A=Cubeless::PrepareModel(Cast<UNNEModelData>(PA.ResolveObject()),true);B=Cubeless::PrepareModel(Cast<UNNEModelData>(PB.ResolveObject()),true);}))return Fail(TEXT("Cancelled loading keypoints."));
  auto Est=UE::BodyTracker::FEstimateKeypoints::FromInstances(A?A():nullptr,B?B():nullptr);A.Reset();B.Reset();if(!Est)return Fail(TEXT("Hand assignment model failed to initialize."));
  for(int32 I=0;I<Files.Num();++I){if(J->Cancel.Load())return Fail(TEXT("Cancelled."));Cubeless::FImageData Frame;if(!ReadFrame(J->Dir/Files[I],Frame))return Fail(TEXT("Cannot read hand frame."));const float Scale=FMath::Min(896.f/Frame.Height,1.f);Est->Run(Frame,FIntRect(0,0,FMath::RoundToInt(Frame.Width*Scale),FMath::RoundToInt(Frame.Height*Scale)),Keys);J->Update(FString::Printf(TEXT("Hand assignment %d/%d"),I+1,Files.Num()),.08+.25*I/Files.Num());}
  Est.Reset();
 }ReleasePackages();
 CBMHandCueCache(J,Keys,true);}
 J->StageHashes.Add(TEXT("HandAssignmentKeypoints"),FMD5::HashBytes(reinterpret_cast<const uint8*>(Keys.GetData()),Keys.Num()*sizeof(float)));
 if(Keys.Num()!=J->FrameCount*399||!FFileHelper::SaveArrayToFile(MakeArrayView(reinterpret_cast<const uint8*>(Keys.GetData()),Keys.Num()*int64(sizeof(float))),*(Work/TEXT("keypoints.f32"))))return Fail(TEXT("Cannot save hand assignment cues."));
 Keys.Empty();auto Config=MakeShared<FJsonObject>();Config->SetStringField(TEXT("repo"),J->WiLoRRepo);Config->SetStringField(TEXT("work"),Work);Config->SetStringField(TEXT("frame_dir"),J->Dir);
 OnGame([&](){
  Config->SetStringField(TEXT("source_asset"),J->Original->GetPathName());
  const FString SourcePath=J->Original->GetPathName();const int32 Take=SourcePath.Find(TEXT("/Take_"));
  if(Take!=INDEX_NONE){const FString Run=SourcePath.Mid(Take+6,32);if(auto P=CBMReadJson(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/Run/TEXT("profile.json"))){double F=0;if(P->TryGetNumberField(TEXT("camera_focal_pixels"),F))Config->SetNumberField(TEXT("camera_focal_pixels"),F);}}

  const auto& Ref=J->Source->GetRefSkeleton();TArray<FTransform> Global=Ref.GetRefBonePose();for(int32 B=1;B<Ref.GetNum();++B)Global[B]=Global[B]*Global[Ref.GetParentIndex(B)];
  const auto Names=CBMFingerNames();TArray<TSharedPtr<FJsonValue>> Rest;
  for(int32 Side=0;Side<2;++Side){const FVector Wrist=Global[Ref.FindBoneIndex(Side?TEXT("hand_r"):TEXT("hand_l"))].GetTranslation();for(int32 B=0;B<15;++B){const FVector P=Global[Ref.FindBoneIndex(Names[Side*15+B])].GetTranslation()-Wrist;for(double V:{P.X,P.Y,P.Z})Rest.Add(MakeShared<FJsonValueNumber>(V));}}
  Config->SetArrayField(TEXT("source_rest_fingers_ue"),Rest);
  if(J->HandBackend==TEXT("HandFlow")){auto Contact=MakeShared<FJsonObject>();FString Reason;
   if(CBMBakeHands(J,nullptr,Reason,&Contact.Get()))Config->SetObjectField(TEXT("hand_contact"),Contact);
   else Config->SetStringField(TEXT("hand_contact_unavailable"),Reason);
  }
 });Config->SetNumberField(TEXT("frames"),J->FrameCount);Config->SetNumberField(TEXT("fps"),J->FPS);Config->SetStringField(TEXT("model"),J->HandModel);
 Config->SetStringField(TEXT("backend"),J->HandBackend);Config->SetStringField(TEXT("video"),J->Video);Config->SetNumberField(TEXT("start_seconds"),J->StartSeconds);
 Config->SetStringField(TEXT("cache_root"),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap/HandCache")));Config->SetBoolField(TEXT("reuse_cache"),J->Reuse);
 Config->SetStringField(TEXT("handflow_root"),J->HandFlowRoot);Config->SetNumberField(TEXT("handflow_steps"),J->HandFlowSteps);Config->SetNumberField(TEXT("handflow_batch"),J->HandFlowBatch);Config->SetNumberField(TEXT("handflow_seed"),J->HandFlowSeed);Config->SetBoolField(TEXT("diagnostics"),J->HandDiagnostics);
 if(!CBMWriteJson(Work/TEXT("request.json"),Config))return Fail(TEXT("Cannot write hand request."));
 {FCBMStage Stage(J,TEXT("Finger inference"),.35);const FString Args=FString::Printf(TEXT("-I \"%s\" --config \"%s\""),*J->HandScript,*(Work/TEXT("request.json")));
  FProcHandle Proc=FPlatformProcess::CreateProc(*J->HandPython,*Args,false,true,true,nullptr,0,*Work,nullptr);if(!Proc.IsValid())return Fail(TEXT("Hand Python could not start."));ON_SCOPE_EXIT{FPlatformProcess::CloseProc(Proc);};
  while(FPlatformProcess::IsProcRunning(Proc)){if(J->Cancel.Load()){FPlatformProcess::TerminateProc(Proc,true);FPlatformProcess::WaitForProc(Proc);return Fail(TEXT("Cancelled; original preserved."));}if(auto State=CBMReadJson(Work/TEXT("worker_status.json"))){FString StageName;double Done=0;if(State->TryGetStringField(TEXT("stage"),StageName)){State->TryGetNumberField(TEXT("done"),Done);J->Update(TEXT("Finger worker: ")+StageName,.35+.5*Done/J->FrameCount);}}FPlatformProcess::Sleep(.25f);}
  int32 Code=-1;FPlatformProcess::GetProcReturnCode(Proc,&Code);if(Code){FString Error=TEXT("See ")+Work/TEXT("worker.log");if(auto State=CBMReadJson(Work/TEXT("worker_status.json")))State->TryGetStringField(TEXT("error"),Error);return Fail(TEXT("Hand worker failed: ")+Error);}
 }
 if(J->Cancel.Load())return Fail(TEXT("Cancelled; original preserved."));auto O=CBMReadJson(Work/TEXT("hands.json"));bool OK=false;
 {FCBMStage Stage(J,TEXT("Apply fingers and verify body"),.9);OnGame([&](){OK=CBMBakeHands(J,O,Message);if(OK&&J->Settings.IsValid())J->Settings->HandResult.FilePath=Work/TEXT("hands.json");});}return OK;
}

bool UCubelessMocapLibrary::ApplyHandResult(UCubelessMocapSettings* S,FString& Result){
 check(IsInGameThread());auto Fail=[&](const FString& M){Result=M;return false;};
 if(!S||IsBusy()||(GEditor&&GEditor->PlayWorld))return Fail(TEXT("End the current job and PIE first."));
 UAnimSequence* Original=S->OriginalAnimation.Get();
 if(!Original||!Original->GetDataModel()||Original->GetOutermost()->IsDirty()||!S->TargetMesh||Original->GetSkeleton()!=S->TargetMesh->GetSkeleton()||!S->Retargeter)return Fail(TEXT("Select a saved original and matching target/retargeter."));
 if(!S->OutputFolder.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(S->OutputFolder)||!FMath::IsFinite(S->HandStrength)||S->HandStrength<0||S->HandStrength>1)return Fail(TEXT("Invalid output folder or Hand Strength."));
 auto O=CBMReadJson(S->HandResult.FilePath);FString Backend;if(!O||!O->TryGetStringField(TEXT("backend"),Backend)||(Backend!=TEXT("MediaPipe")&&Backend!=TEXT("WiLoR")&&Backend!=TEXT("HandFlow")))return Fail(TEXT("Choose a generated hands.json with backend metadata."));
 if(Backend!=TEXT("HandFlow")&&!S->EnableLegacyHandDiagnostics)return Fail(TEXT("Enable Legacy Hand Diagnostics to reapply WiLoR/MediaPipe caches. Existing animation assets can still be opened normally."));
 auto* Source=LoadObject<USkeletalMesh>(nullptr,TEXT("/CubelessBodyMocap/SKM_SMPL.SKM_SMPL"));if(!Source)return Fail(TEXT("Missing SMPL source mesh."));
 auto J=MakeShared<FCBMJob,ESPMode::ThreadSafe>();J->Run=TEXT("HandsApply_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);J->Original.Reset(Original);J->OriginalGuid=Original->GetDataModel()->GenerateGuid();J->Target.Reset(S->TargetMesh);J->Source.Reset(Source);J->Retargeter.Reset(S->Retargeter);J->Settings=S;J->HandBackend=Backend;J->HandStrength=S->HandStrength;J->OutputFolder=S->OutputFolder;J->FPS=FMath::RoundToInt(Original->GetDataModel()->GetFrameRate().AsDecimal());
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run),true);
 return CBMBakeHands(J,O,Result);
}

static FString CBMHandReadiness(UCubelessMocapSettings* S,ECBMFingerBackend Backend);
bool UCubelessMocapLibrary::RefineHands(UCubelessMocapSettings* S,FString& Result){
 check(IsInGameThread());auto Fail=[&](const FString& M){Result=M;return false;};if(!S||IsBusy()||(GEditor&&GEditor->PlayWorld))return Fail(TEXT("End the current job and PIE first."));
 auto* Original=S->OriginalAnimation?S->OriginalAnimation.Get():S->PreviewAnimation.Get();
 if(S->SelectedFingerBackend()==ECBMFingerBackend::None){Original=S->OriginalAnimation.Get();if(!Original)return Fail(TEXT("Choose the original animation for no hand enhancement."));S->PreviewAnimation=Original;Result=TEXT("Original hands selected; no inference or asset change.");return true;}
 if(!S->EnableLegacyHandDiagnostics&&S->SelectedFingerBackend()!=ECBMFingerBackend::HandFlow)return Fail(TEXT("Legacy hand inference is diagnostic only. Enable Legacy Hand Diagnostics or choose HandFlow."));
 if(!Original||!Original->GetDataModel()||Original->GetOutermost()->IsDirty()||!S->TargetMesh||Original->GetSkeleton()!=S->TargetMesh->GetSkeleton()||!S->Retargeter)return Fail(TEXT("Choose a saved Original Animation, matching target mesh and retargeter."));
 if(!S->OutputFolder.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(S->OutputFolder)||!FMath::IsFinite(S->StartSeconds)||S->StartSeconds<0||!FMath::IsFinite(S->HandStrength)||S->HandStrength<0||S->HandStrength>1)return Fail(TEXT("Invalid hand output, start time or strength."));
 const FFrameRate Rate=Original->GetDataModel()->GetFrameRate();const int32 Count=Original->GetDataModel()->GetNumberOfKeys();if(Rate.Denominator!=1||Rate.Numerator<1||Rate.Numerator>120||Count<2||Count>14400)return Fail(TEXT("Expected 2..14400 frames at an integer FPS (1..120)."));
 const bool MediaPipe=S->SelectedFingerBackend()==ECBMFingerBackend::MediaPipe,HandFlow=S->SelectedFingerBackend()==ECBMFingerBackend::HandFlow;
 const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("CubelessBodyMocap"));const FString Script=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("Tools/run_hand_worker.py"));
 const FString Python=HandFlow?S->HandFlowPython.FilePath:MediaPipe?S->MediaPipePython.FilePath:S->HandPython.FilePath;
 const FString Repo=FPaths::ConvertRelativePathToFull(S->WiLoRDirectory.Path);
 const FString Ready=CBMHandReadiness(S,S->SelectedFingerBackend());if(Ready.StartsWith(TEXT("Missing")))return Fail(Ready);
 for(const auto& File:TArray<FString>{S->Video.FilePath,S->FFmpeg.FilePath,Python,Script})if(File.Contains(TEXT("\""))||!FPaths::FileExists(File))return Fail(TEXT("Missing required file: ")+File);
 if(S->HandFlowSteps<1||S->HandFlowSteps>32||S->HandFlowBatch<1||S->HandFlowBatch>8||S->HandFlowSeed<0)return Fail(TEXT("Invalid HandFlow settings."));
 const auto* Model=Original->GetDataModel();for(const auto& Bone:CBMFingerNames())if(!Model->IsValidBoneTrackName(Bone))return Fail(TEXT("Original lacks finger track: ")+Bone.ToString());
 auto* Source=LoadObject<USkeletalMesh>(nullptr,TEXT("/CubelessBodyMocap/SKM_SMPL.SKM_SMPL"));if(!Source)return Fail(TEXT("SMPL source mesh unavailable."));
 if(CurrentJob&&CurrentJob->Worker.IsValid())CurrentJob->Worker.Wait();
 auto J=MakeShared<FCBMJob,ESPMode::ThreadSafe>();J->Run=TEXT("Hands_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);J->Original.Reset(Original);J->Source.Reset(Source);J->Target.Reset(S->TargetMesh);J->Retargeter.Reset(S->Retargeter);J->Settings=S;J->Video=FPaths::ConvertRelativePathToFull(S->Video.FilePath);J->FFmpeg=S->FFmpeg.FilePath;J->HandPython=S->HandPython.FilePath;J->WiLoRRepo=Repo;J->HandScript=Script;J->FPS=Rate.Numerator;J->FrameCount=Count;J->StartSeconds=S->StartSeconds;J->HandStrength=S->HandStrength;J->OutputFolder=S->OutputFolder;CurrentJob=J;
 IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run),true);
 J->OriginalGuid=Model->GenerateGuid();
 J->HandPython=Python;J->HandBackend=HandFlow?TEXT("HandFlow"):MediaPipe?TEXT("MediaPipe"):TEXT("WiLoR");J->HandFlowRoot=FPaths::ConvertRelativePathToFull(S->HandFlowDirectory.Path);J->HandFlowSteps=S->HandFlowSteps;J->HandFlowBatch=S->HandFlowBatch;J->HandFlowSeed=S->HandFlowSeed;J->HandDiagnostics=S->HandDiagnostics;J->HandModel=S->MediaPipeModel.FilePath;J->Reuse=S->ReuseCheckpoints;J->HandKeypointsCode=CBMHandCueCodePaths();
 J->Worker=Async(EAsyncExecution::Thread,[J](){FString Message;const bool OK=CBMSolveHands(J,Message);{FScopeLock L(&J->Mutex);J->Success=OK;J->Finished=FPlatformTime::Seconds();J->Message=Message;J->Stage=OK?TEXT("Completed"):J->Cancel.Load()?TEXT("Cancelled"):TEXT("Failed");if(OK)J->Progress=1;}J->Profile();J->Done.Store(true);});Result=TEXT("Started finger-only job ")+J->Run;return true;
}

static FString CBMHandReadiness(UCubelessMocapSettings* S,ECBMFingerBackend Backend){
 if(!S)return TEXT("Missing settings");
 if(Backend==ECBMFingerBackend::None)return TEXT("Hand enhancement off. Start Mocap saves body motion only; existing hand results stay saved.");
 if(!S->EnableLegacyHandDiagnostics&&Backend!=ECBMFingerBackend::HandFlow)return TEXT("Legacy hand selection is diagnostic only. Choose HandFlow or enable Legacy Hand Diagnostics.");
 TArray<FString> Required;FString Description;
 if(Backend==ECBMFingerBackend::HandFlow){
  const FString Root=S->HandFlowDirectory.Path;
  Required.Add(S->HandFlowPython.FilePath);Required.Add(Root/TEXT("manifest.json"));
  for(const TCHAR* Name:{TEXT("handflow_denoiser.pt"),TEXT("normalization_stats.npz"),TEXT("hamer.ckpt"),TEXT("model_config.yaml"),TEXT("mano_mean_params.npz"),TEXT("detector.pt"),TEXT("MANO_RIGHT.pkl")})Required.Add(Root/TEXT("models")/Name);
  Required.Add(Root/TEXT("runtime/HandFlow/model/flow_matching/denoiser.py"));Required.Add(Root/TEXT("runtime/HaMeR/hamer/models/hamer.py"));
  Description=TEXT("HandFlow / HaMeR: image features + offline temporal inference (16-frame windows). No WiLoR pose input or estimator mixing. Missing detections remain flagged predictions.");
 }else if(Backend==ECBMFingerBackend::WiLoR){
  Required.Add(S->HandPython.FilePath);for(const TCHAR* Name:{TEXT("mano_data/MANO_RIGHT.pkl"),TEXT("mano_data/mano_mean_params.npz"),TEXT("pretrained_models/wilor_final.ckpt"),TEXT("pretrained_models/detector.pt"),TEXT("pretrained_models/model_config.yaml")})Required.Add(S->WiLoRDirectory.Path/Name);
  Description=TEXT("DIAGNOSTIC: WiLoR estimates rotations. MediaPipe observations assist hand identity and crop selection; its rotations are not mixed in. Existing gap filtering is retained.");
  const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("CubelessBodyMocap"));const FString MP=Plugin->GetBaseDir()/TEXT("ThirdParty/MediaPipe");
  if(!FPaths::FileExists(MP/TEXT("venv/Scripts/python.exe"))||!FPaths::FileExists(MP/TEXT("hand_landmarker.task")))Description+=TEXT(" MediaPipe helper unavailable: existing WiLoR-only fallback will be reported.");
 }else{Required={S->MediaPipePython.FilePath,S->MediaPipeModel.FilePath};Description=TEXT("Legacy MediaPipe only: landmarks converted to finger rotations. Default preserved; available for independent diagnosis.");}
 FString Missing;for(const auto& File:Required)if(File.Contains(TEXT("\""))||!FPaths::FileExists(File))Missing+=TEXT("\n")+File;
 if(!Missing.IsEmpty())return TEXT("Missing optional hand files:")+Missing+TEXT("\nHandFlow setup: Tools/prepare_handflow.py --weights and ThirdParty/HandFlow/README.md. Original hands remain available; legacy backends are under Diagnostics.");
 return TEXT("Files ready (runtime/hash validation on start). ")+Description;
}


FString UCubelessMocapLibrary::GetHandReadiness(UCubelessMocapSettings* S){
 if(!S)return TEXT("Missing settings");
 FString Status=CBMHandReadiness(S,S->EnhanceHands?ECBMFingerBackend::HandFlow:ECBMFingerBackend::None);
 if(S->EnableLegacyHandDiagnostics)Status+=TEXT("\nSeparate diagnostic action: ")+CBMHandReadiness(S,S->SelectedFingerBackend());
 return Status;
}

// Capture all hand settings before launching the existing worker. Normal Start
// always uses HandFlow, even when a legacy diagnostic panel is visible.
static bool CBMConfigureAutoHands(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,UCubelessMocapSettings* S,FString& Error){
 const FString Ready=CBMHandReadiness(S,ECBMFingerBackend::HandFlow);
 if(Ready.StartsWith(TEXT("Missing"))){Error=Ready+TEXT("\nUncheck Enhance Hands for body only.");return false;}
 if(!FMath::IsFinite(S->HandStrength)||S->HandStrength<0||S->HandStrength>1||S->HandFlowSteps<1||S->HandFlowSteps>32||S->HandFlowBatch<1||S->HandFlowBatch>8||S->HandFlowSeed<0){Error=TEXT("Invalid HandFlow settings.");return false;}
 const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("CubelessBodyMocap"));
 J->HandScript=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("Tools/run_hand_worker.py"));
 J->HandPython=S->HandFlowPython.FilePath;J->HandFlowRoot=FPaths::ConvertRelativePathToFull(S->HandFlowDirectory.Path);
 for(const FString& File:{J->HandScript,J->HandPython})if(File.Contains(TEXT("\""))||!FPaths::FileExists(File)){Error=TEXT("Missing or invalid hand file: ")+File;return false;}
 J->HandFlowSteps=S->HandFlowSteps;J->HandFlowBatch=S->HandFlowBatch;J->HandFlowSeed=S->HandFlowSeed;J->HandDiagnostics=S->HandDiagnostics;J->HandStrength=S->HandStrength;
 J->WiLoRRepo=FPaths::ConvertRelativePathToFull(S->WiLoRDirectory.Path);J->HandModel=S->MediaPipeModel.FilePath;J->HandKeypointsCode=CBMHandCueCodePaths();
 return true;
}
