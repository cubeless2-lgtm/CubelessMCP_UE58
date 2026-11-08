// Snapshot target geometry and immutable numeric animation data on the game
// thread. Contact optimization itself runs only in the external worker.
static bool CBMFootContactContext(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,FString& Error){
 check(IsInGameThread());auto Fail=[&](const FString& M){Error=M;return false;};
 const auto& Ref=J->Target->GetRefSkeleton();const auto* Model=J->Original->GetDataModel();const int32 N=J->FrameCount;
 const FSkeletalMeshModel* Imported=J->Target->GetImportedModel();if(!Imported||Imported->LODModels.IsEmpty())return Fail(TEXT("Contact fit requires target LOD0 source geometry."));
 int32 Feet[2]={Ref.FindBoneIndex(TEXT("foot_l")),Ref.FindBoneIndex(TEXT("foot_r"))};if(Feet[0]<0||Feet[1]<0)return Fail(TEXT("Contact fit requires both ankle bones."));
 TArray<FTransform> Bind=Ref.GetRefBonePose();for(int32 B=1;B<Ref.GetNum();++B)Bind[B]=Bind[B]*Bind[Ref.GetParentIndex(B)];
 struct FWeight{int32 Bone;double Weight;FVector Point;};struct FVertex{FVector Rest;TArray<FWeight> Weights;};TArray<FVertex> Vertices[2];TArray<int32> Used={Feet[0],Feet[1]};
 // Include the shoe and low ankle skin. No deformation or joint-angle bound.
 const double Cutoff=FMath::Max(Bind[Feet[0]].GetTranslation().Z,Bind[Feet[1]].GetTranslation().Z)+8.;
 for(const auto& Section:Imported->LODModels[0].Sections)for(const auto& V:Section.SoftVertices){
  const FVector P(V.Position);if(P.Z>Cutoff)continue;double Total=0;for(int32 K=0;K<MAX_TOTAL_INFLUENCES;++K)Total+=V.InfluenceWeights[K];if(Total<=0)continue;
  const int32 Side=FVector::DistSquared(P,Bind[Feet[0]].GetTranslation())<=FVector::DistSquared(P,Bind[Feet[1]].GetTranslation())?0:1;FVertex Item;Item.Rest=P;
  for(int32 K=0;K<MAX_TOTAL_INFLUENCES;++K){if(!V.InfluenceWeights[K])continue;if(!Section.BoneMap.IsValidIndex(V.InfluenceBones[K]))return Fail(TEXT("Invalid shoe influence."));const int32 B=Section.BoneMap[V.InfluenceBones[K]];if(!Bind.IsValidIndex(B))return Fail(TEXT("Invalid shoe bone."));Item.Weights.Add({B,double(V.InfluenceWeights[K])/Total,Bind[B].InverseTransformPosition(P)});Used.AddUnique(B);}
  Vertices[Side].Add(MoveTemp(Item));
 }
 if(Vertices[0].Num()<20||Vertices[1].Num()<20)return Fail(TEXT("Insufficient target shoe vertices."));
 auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("version"),1);if(J->FootSupportRotation)O->SetBoolField(TEXT("support_rotation"),true);if(J->FootTemporalCleanup)O->SetBoolField(TEXT("temporal_cleanup"),true);O->SetNumberField(TEXT("frames"),N);O->SetNumberField(TEXT("ground_z_cm"),J->FootGroundZ);O->SetStringField(TEXT("target"),J->Target->GetPathName());
 auto Values=[](std::initializer_list<double> V){TArray<TSharedPtr<FJsonValue>> A;for(double X:V)A.Add(MakeShared<FJsonValueNumber>(X));return A;};
 auto PV=[&](const FVector& P){return MakeShared<FJsonValueArray>(Values({P.X,P.Y,P.Z}));};
 TArray<TSharedPtr<FJsonValue>> Names,Sides,Descendants,Bases;
 for(int32 B:Used)Names.Add(MakeShared<FJsonValueString>(Ref.GetBoneName(B).ToString()));O->SetArrayField(TEXT("bones"),Names);
 for(int32 Side=0;Side<2;++Side){
  TArray<TSharedPtr<FJsonValue>> VV,DD;const FQuat C=J->FootBasis[Side];Bases.Add(MakeShared<FJsonValueArray>(Values({C.X,C.Y,C.Z,C.W})));
  for(int32 B=0;B<Used.Num();++B){int32 Parent=Used[B];while(Parent>=0&&Parent!=Feet[Side])Parent=Ref.GetParentIndex(Parent);if(Parent==Feet[Side])DD.Add(MakeShared<FJsonValueNumber>(B));}
  Descendants.Add(MakeShared<FJsonValueArray>(DD));
  for(const auto& V:Vertices[Side]){auto Vertex=MakeShared<FJsonObject>();Vertex->SetArrayField(TEXT("rest"),Values({V.Rest.X,V.Rest.Y,V.Rest.Z}));TArray<TSharedPtr<FJsonValue>> WW;
   for(const auto& W:V.Weights){TArray<TSharedPtr<FJsonValue>> A;A.Add(MakeShared<FJsonValueNumber>(Used.Find(W.Bone)));A.Add(MakeShared<FJsonValueNumber>(W.Weight));A.Add(PV(W.Point));WW.Add(MakeShared<FJsonValueArray>(A));}Vertex->SetArrayField(TEXT("weights"),WW);VV.Add(MakeShared<FJsonValueObject>(Vertex));
  }Sides.Add(MakeShared<FJsonValueArray>(VV));
 }
 O->SetArrayField(TEXT("vertices"),Sides);O->SetArrayField(TEXT("descendants"),Descendants);O->SetArrayField(TEXT("basis"),Bases);
 TArray<TArray<FTransform>> Tracks;Tracks.SetNum(Ref.GetNum());for(int32 B=0;B<Ref.GetNum();++B)if(Model->IsValidBoneTrackName(Ref.GetBoneName(B)))Model->GetBoneTrackTransforms(Ref.GetBoneName(B),Tracks[B]);
 TArray<TSharedPtr<FJsonValue>> World,AllLocal;World.Reserve(N*Used.Num()*7);
 if(J->FootLowerBody){
  for(const TCHAR* Name:{TEXT("pelvis"),TEXT("spine_01"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("thigh_r"),TEXT("calf_r")})if(!Model->IsValidBoneTrackName(Name))return Fail(TEXT("Video lower-body refinement requires complete Manny pelvis/leg/spine tracks."));
  AllLocal.Reserve(N*Ref.GetNum()*7);
 }
 for(int32 I=0;I<N;++I){TArray<FTransform> Global=Ref.GetRefBonePose();for(int32 B=0;B<Ref.GetNum();++B){if(Tracks[B].Num())Global[B]=Tracks[B][FMath::Min(I,Tracks[B].Num()-1)];if(!Global[B].GetScale3D().Equals(FVector::OneVector,.00001))return Fail(TEXT("Contact fit does not support scaled bones."));if(B>0)Global[B]=Global[B]*Global[Ref.GetParentIndex(B)];}
  for(int32 B:Used){const FVector P=Global[B].GetTranslation();const FQuat Q=Global[B].GetRotation();for(double X:{P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W})World.Add(MakeShared<FJsonValueNumber>(X));}
  if(J->FootLowerBody)for(int32 B=0;B<Ref.GetNum();++B){const FTransform T=Tracks[B].Num()?Tracks[B][FMath::Min(I,Tracks[B].Num()-1)]:Ref.GetRefBonePose()[B];const FVector P=T.GetTranslation();const FQuat Q=T.GetRotation();for(double X:{P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W})AllLocal.Add(MakeShared<FJsonValueNumber>(X));}
 }
 if(J->FootLowerBody){auto Lower=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Bones,Parents;for(int32 B=0;B<Ref.GetNum();++B){Bones.Add(MakeShared<FJsonValueString>(Ref.GetBoneName(B).ToString()));Parents.Add(MakeShared<FJsonValueNumber>(Ref.GetParentIndex(B)));}Lower->SetArrayField(TEXT("bones"),Bones);Lower->SetArrayField(TEXT("parents"),Parents);Lower->SetArrayField(TEXT("local"),AllLocal);O->SetObjectField(TEXT("lower_body"),Lower);}
 O->SetArrayField(TEXT("world"),World);FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));J->FootContextSignature=FMD5::HashAnsiString(*Text);J->FootContext=O;return true;
}
