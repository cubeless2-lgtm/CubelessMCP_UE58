// Optional postprocess of a saved target animation. Only 30 finger rotations
// are written; body, wrists, metacarpals, translations and scales are preserved.
static bool CBMApplyJointPost(const TSharedRef<FCBMJob,ESPMode::ThreadSafe>& J,FString& Error){
 check(IsInGameThread());auto Fail=[&](const FString& M){Error=M;return false;};
 if(J->Cancel.Load()||(GEditor&&GEditor->PlayWorld))return Fail(TEXT("Cancelled or PIE active; source preserved."));
 auto* Input=J->Original.Get();const auto* Model=Input->GetDataModel();const int32 N=Model->GetNumberOfKeys();
 if(Input->GetOutermost()->IsDirty()||Model->GenerateGuid()!=J->OriginalGuid)return Fail(TEXT("Source animation changed; correction was not applied."));
 const FString Work=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run);
 const auto Data=CBMReadJson(Work/TEXT("corrected.json"));const TArray<TSharedPtr<FJsonValue>> *Values=nullptr,*JsonNames=nullptr;
 double Version=0,Frames=0,FPS=0;FString Source,Backend;const auto Names=CBMFingerNames();
 if(!Data||!Data->TryGetNumberField(TEXT("version"),Version)||Version!=1||!Data->TryGetNumberField(TEXT("frames"),Frames)||Frames!=N||!Data->TryGetNumberField(TEXT("fps"),FPS)||FPS!=J->FPS||!Data->TryGetStringField(TEXT("source_asset"),Source)||Source!=Input->GetPathName()||!Data->TryGetStringField(TEXT("backend"),Backend)||Backend!=TEXT("JointCorrection")||!Data->TryGetArrayField(TEXT("names"),JsonNames)||JsonNames->Num()!=30||!Data->TryGetArrayField(TEXT("target_local_quaternions"),Values)||Values->Num()!=N*30*4)return Fail(TEXT("Invalid joint correction output identity/schema."));
 TArray<TArray<FQuat4f>> Rotations;Rotations.SetNum(30);
 for(int32 B=0;B<30;++B){if((*JsonNames)[B]->AsString()!=Names[B].ToString())return Fail(TEXT("Joint order changed."));
  for(int32 F=0;F<N;++F){double V[4];for(int32 K=0;K<4;++K)if(!(*Values)[(F*30+B)*4+K]->TryGetNumber(V[K])||!FMath::IsFinite(V[K]))return Fail(TEXT("Nonfinite corrected rotation."));const FQuat Q(V[0],V[1],V[2],V[3]);if(FMath::Abs(Q.SizeSquared()-1)>.0001)return Fail(TEXT("Nonunit corrected rotation."));Rotations[B].Add(FQuat4f(Q));}
 }
 const FString Name=Input->GetName()+TEXT("_JointCorrected_")+J->Run.Right(32).Left(8),Package=J->OutputFolder/TEXT("JointCorrection")/Name;
 if(FPackageName::DoesPackageExist(Package))return Fail(TEXT("Output exists; preserving it."));
 auto* Output=DuplicateObject<UAnimSequence>(Input,CreatePackage(*Package),*Name);if(!Output)return Fail(TEXT("Cannot duplicate source animation."));
 Output->SetFlags(RF_Public|RF_Standalone);auto& Controller=Output->GetController();Controller.OpenBracket(FText::FromString(TEXT("Cubeless optional finger joint correction")),false);
 bool OK=true;
 for(int32 B=0;B<30;++B){TArray<FTransform> Keys;Model->GetBoneTrackTransforms(Names[B],Keys);TArray<FVector3f> P,S;for(const auto& K:Keys){P.Add(FVector3f(K.GetTranslation()));S.Add(FVector3f(K.GetScale3D()));}OK=Controller.SetBoneTrackKeys(Names[B],P,Rotations[B],S,false)&&OK;}
 Controller.CloseBracket(false);if(!OK)return Fail(TEXT("Cannot write corrected finger tracks; unsaved output retained."));
 TArray<FName> Tracks;Model->GetBoneTrackNames(Tracks);int32 Preserved=0;
 for(const FName Bone:Tracks){TArray<FTransform> A,B;Model->GetBoneTrackTransforms(Bone,A);Output->GetDataModel()->GetBoneTrackTransforms(Bone,B);if(A.Num()!=B.Num())return Fail(TEXT("Track length changed."));const bool Finger=Names.Contains(Bone);
  for(int32 I=0;I<A.Num();++I){if(!A[I].GetTranslation().Equals(B[I].GetTranslation(),0)||!A[I].GetScale3D().Equals(B[I].GetScale3D(),0)||(!Finger&&!A[I].Equals(B[I],0)))return Fail(TEXT("Preservation failed: ")+Bone.ToString());}if(!Finger)++Preserved;
 }
 if(Output->GetDataModel()->GetFrameRate()!=Model->GetFrameRate()||Output->GetDataModel()->GetNumberOfKeys()!=N)return Fail(TEXT("Animation timing changed."));
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;const FString File=FPackageName::LongPackageNameToFilename(Package,FPackageName::GetAssetPackageExtension());
 if(!UPackage::SavePackage(Output->GetOutermost(),Output,*File,Args))return Fail(TEXT("Could not save joint correction."));FAssetRegistryModule::AssetCreated(Output);
 auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("input"),Input->GetPathName());Report->SetStringField(TEXT("output"),Output->GetPathName());Report->SetNumberField(TEXT("frames"),N);Report->SetNumberField(TEXT("preserved_tracks"),Preserved);Report->SetNumberField(TEXT("edited_rotation_tracks"),30);Report->SetNumberField(TEXT("nonfinger_max_difference"),0);Report->SetBoolField(TEXT("all_translations_scales_preserved"),true);Report->SetBoolField(TEXT("timing_preserved"),true);CBMWriteJson(Work/TEXT("preservation.json"),Report);
 if(J->Settings.IsValid()){J->Settings->JointComparisonSource=Input;J->Settings->JointCorrectedAnimation=Output;J->Settings->PreviewAnimation=Output;J->Settings->JointCorrectionReport.FilePath=Work/TEXT("metrics.json");}
 Error=TEXT("Saved ")+Output->GetPathName()+TEXT(". Body, wrists, metacarpals and timing preserved; compare Before correction / Joint corrected.");return true;
}

bool UCubelessMocapLibrary::CorrectFingerJoints(UCubelessMocapSettings* S,FString& Result){
 check(IsInGameThread());auto Fail=[&](const FString& M){Result=M;return false;};
 if(!S||IsBusy()||(GEditor&&GEditor->PlayWorld))return Fail(TEXT("End the current job and PIE first."));
 if(!S->EnableJointCorrection)return Fail(TEXT("Enable Joint Correction in Postprocess first. Start Mocap never applies it automatically."));
 auto* Input=S->JointCorrectionSource?S->JointCorrectionSource.Get():S->HandFlowAnimation?S->HandFlowAnimation.Get():S->PreviewAnimation==S->JointCorrectedAnimation?S->JointComparisonSource.Get():S->PreviewAnimation.Get();
 if(!Input||!Input->GetDataModel()||Input->GetOutermost()->IsDirty()||!S->TargetMesh||Input->GetSkeleton()!=S->TargetMesh->GetSkeleton()||!S->Retargeter)return Fail(TEXT("Select a saved uncorrected Manny animation and matching mesh/retargeter."));
 if(Input->GetName().Contains(TEXT("_JointCorrected_")))return Fail(TEXT("Choose the uncorrected source; corrections are not stacked."));
 if(!FMath::IsFinite(S->JointCorrectionStrength)||S->JointCorrectionStrength<0||S->JointCorrectionStrength>1||!S->OutputFolder.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(S->OutputFolder))return Fail(TEXT("Invalid correction strength or output folder."));
 if(S->JointCorrectionStrength==0){S->PreviewAnimation=Input;Result=TEXT("Zero strength: selected the unchanged source; no new asset.");return true;}
 const auto* Model=Input->GetDataModel();const FFrameRate Rate=Model->GetFrameRate();const int32 N=Model->GetNumberOfKeys();if(N<2||N>14400||Rate.Denominator!=1||Rate.Numerator<1||Rate.Numerator>120)return Fail(TEXT("Expected 2..14400 frames at integer 1..120 FPS."));
 const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("CubelessBodyMocap"));const FString Script=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("Tools/run_joint_post.py"));
 for(const auto& File:{S->HandFlowPython.FilePath,Script})if(File.Contains(TEXT("\""))||!FPaths::FileExists(File))return Fail(TEXT("Missing postprocess Python/script: ")+File+TEXT(". Uses NumPy/SciPy only; no model inference."));
 auto* Source=LoadObject<USkeletalMesh>(nullptr,TEXT("/CubelessBodyMocap/SKM_SMPL.SKM_SMPL"));if(!Source)return Fail(TEXT("Source skeleton calibration unavailable."));
 if(CurrentJob&&CurrentJob->Worker.IsValid())CurrentJob->Worker.Wait();
 auto J=MakeShared<FCBMJob,ESPMode::ThreadSafe>();J->Run=TEXT("JointPost_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);J->Original.Reset(Input);J->OriginalGuid=Model->GenerateGuid();J->Target.Reset(S->TargetMesh);J->Source.Reset(Source);J->Retargeter.Reset(S->Retargeter);J->Settings=S;J->FPS=Rate.Numerator;J->FrameCount=N;J->HandBackend=TEXT("JointCorrection");J->HandPython=S->HandFlowPython.FilePath;J->HandScript=Script;J->OutputFolder=S->OutputFolder;
 auto Calibration=MakeShared<FJsonObject>();if(!CBMBakeHands(J,nullptr,Result,&Calibration.Get()))return false;
 auto Values=[](std::initializer_list<double> V){TArray<TSharedPtr<FJsonValue>> A;for(double X:V)A.Add(MakeShared<FJsonValueNumber>(X));return A;};
 auto QV=[&](const FQuat& Q){return MakeShared<FJsonValueArray>(Values({Q.X,Q.Y,Q.Z,Q.W}));};auto PV=[&](const FVector& P){return MakeShared<FJsonValueArray>(Values({P.X,P.Y,P.Z}));};
 const auto Names=CBMFingerNames();const auto& SourceRef=Source->GetRefSkeleton();TArray<FTransform> SourceGlobal=SourceRef.GetRefBonePose();for(int32 B=1;B<SourceRef.GetNum();++B)SourceGlobal[B]=SourceGlobal[B]*SourceGlobal[SourceRef.GetParentIndex(B)];
 TArray<TSharedPtr<FJsonValue>> RestPositions,RestRotations,RestWrists;
 for(FName Name:Names){const auto& T=SourceGlobal[SourceRef.FindBoneIndex(Name)];RestPositions.Add(PV(T.GetTranslation()));RestRotations.Add(QV(T.GetRotation()));}for(const TCHAR* Side:{TEXT("hand_l"),TEXT("hand_r")})RestWrists.Add(PV(SourceGlobal[SourceRef.FindBoneIndex(Side)].GetTranslation()));
 Calibration->SetArrayField(TEXT("source_rest_positions"),RestPositions);Calibration->SetArrayField(TEXT("source_rest_rotations"),RestRotations);Calibration->SetArrayField(TEXT("source_rest_wrists"),RestWrists);
 const auto& Ref=S->TargetMesh->GetRefSkeleton();TArray<TArray<FTransform>> Tracks;Tracks.SetNum(Ref.GetNum());for(int32 B=0;B<Ref.GetNum();++B)if(Model->IsValidBoneTrackName(Ref.GetBoneName(B)))Model->GetBoneTrackTransforms(Ref.GetBoneName(B),Tracks[B]);
 const auto& TargetNames=Calibration->GetArrayField(TEXT("names"));TArray<int32> Indices;for(const auto& Name:TargetNames)Indices.Add(Ref.FindBoneIndex(FName(*Name->AsString())));
 TArray<TSharedPtr<FJsonValue>> WorldFrames,LocalFrames;
 for(int32 F=0;F<N;++F){TArray<FTransform> Local=Ref.GetRefBonePose();for(int32 B=0;B<Ref.GetNum();++B)if(Tracks[B].Num())Local[B]=Tracks[B][FMath::Min(F,Tracks[B].Num()-1)];TArray<FTransform> World=Local;for(int32 B=1;B<Ref.GetNum();++B)World[B]=World[B]*World[Ref.GetParentIndex(B)];TArray<TSharedPtr<FJsonValue>> W,L;for(int32 B=0;B<Indices.Num();++B){W.Add(QV(World[Indices[B]].GetRotation()));if(B<30)L.Add(QV(Local[Indices[B]].GetRotation()));}WorldFrames.Add(MakeShared<FJsonValueArray>(W));LocalFrames.Add(MakeShared<FJsonValueArray>(L));}
 Calibration->SetArrayField(TEXT("input_world_q"),WorldFrames);Calibration->SetArrayField(TEXT("input_local_q"),LocalFrames);
 const FString Work=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/J->Run);IFileManager::Get().MakeDirectory(*Work,true);
 auto Request=MakeShared<FJsonObject>();Request->SetObjectField(TEXT("calibration"),Calibration);Request->SetStringField(TEXT("work"),Work);Request->SetStringField(TEXT("source_asset"),Input->GetPathName());Request->SetNumberField(TEXT("frames"),N);Request->SetNumberField(TEXT("fps"),J->FPS);Request->SetNumberField(TEXT("strength"),S->JointCorrectionStrength);
 if(Input==S->HandFlowAnimation&&S->OriginalAnimation&&S->HandComparisonBase==S->OriginalAnimation){auto Hand=CBMReadJson(S->HandResult.FilePath);FString Body;double Frames=0;const FString Evidence=FPaths::GetPath(S->HandResult.FilePath)/TEXT("model_evidence.npz");if(Hand&&Hand->TryGetStringField(TEXT("source_asset"),Body)&&Body==S->OriginalAnimation->GetPathName()&&Hand->TryGetNumberField(TEXT("frames"),Frames)&&Frames==N&&FPaths::FileExists(Evidence))Request->SetStringField(TEXT("confidence_file"),Evidence);}
 if(!CBMWriteJson(Work/TEXT("request.json"),Request))return Fail(TEXT("Cannot write joint postprocess request."));
 CurrentJob=J;
 J->Worker=Async(EAsyncExecution::Thread,[J,Work](){FString Message;bool OK=false;
  auto Process=[&](){FCBMStage Stage(J,TEXT("Finger joint postprocess (CPU, no inference)"),.1);const FString Args=FString::Printf(TEXT("-I \"%s\" --config \"%s\""),*J->HandScript,*(Work/TEXT("request.json")));FProcHandle Proc=FPlatformProcess::CreateProc(*J->HandPython,*Args,false,true,true,nullptr,0,*Work,nullptr);if(!Proc.IsValid()){Message=TEXT("Postprocess Python could not start.");return false;}ON_SCOPE_EXIT{FPlatformProcess::CloseProc(Proc);};
   while(FPlatformProcess::IsProcRunning(Proc)){if(J->Cancel.Load()){FPlatformProcess::TerminateProc(Proc,true);FPlatformProcess::WaitForProc(Proc);Message=TEXT("Cancelled; source animation preserved.");return false;}if(auto State=CBMReadJson(Work/TEXT("worker_status.json"))){FString StageName;double Done=0;if(State->TryGetStringField(TEXT("stage"),StageName)){State->TryGetNumberField(TEXT("done"),Done);J->Update(TEXT("Finger postprocess: ")+StageName,.1+.7*FMath::Clamp(Done/J->FrameCount,0.,1.));}}FPlatformProcess::Sleep(.05f);}int32 Code=-1;FPlatformProcess::GetProcReturnCode(Proc,&Code);if(Code){Message=TEXT("Joint postprocess failed. See ")+Work/TEXT("worker.log");return false;}return true;};
  if(Process()&&!J->Cancel.Load()){FCBMStage Stage(J,TEXT("Save fingers and verify preserved tracks"),.9);OnGame([&](){OK=CBMApplyJointPost(J,Message);});}
  if(J->Cancel.Load()&&Message.IsEmpty())Message=TEXT("Cancelled; source animation preserved.");
  {FScopeLock L(&J->Mutex);J->Success=OK;J->Finished=FPlatformTime::Seconds();J->Stage=OK?TEXT("Completed"):J->Cancel.Load()?TEXT("Cancelled"):TEXT("Failed");J->Message=Message;if(OK)J->Progress=1;}J->Profile();J->Done.Store(true);
 });Result=TEXT("Started optional joint correction ")+J->Run;return true;
}
