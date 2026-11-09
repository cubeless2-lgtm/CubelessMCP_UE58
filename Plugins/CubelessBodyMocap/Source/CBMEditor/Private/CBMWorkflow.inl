// All state transitions run on the game thread. Reentrant ticks never dispatch
// another stage while a native asset operation pumps Slate.
UAnimSequence* UCubelessMocapLibrary::ResolveSaveAnimation(UCubelessMocapSettings* S){
 if(!S)return nullptr;
 if(S->SaveBoneCorrection)return S->SaveHandFlow?S->CorrectedHandAnimation.Get():S->CorrectedBodyAnimation.Get();
 return S->SaveHandFlow?S->HandFlowAnimation.Get():S->OriginalAnimation.Get();
}
bool UCubelessMocapLibrary::StartMocap(UCubelessMocapSettings* S,FString& Result){
 if(!S||IsBusy()){Result=TEXT("진행 중인 작업이 끝난 뒤 시작하세요.");return false;}
 auto Work=TStrongObjectPtr<UCubelessMocapSettings>(DuplicateObject<UCubelessMocapSettings>(S,GetTransientPackage()));
 Work->OutputFolder=TEXT("/Game/_MCP_Temp/CBMWork/Take_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 if(Work->AutoBoneCorrection&&!FPaths::FileExists(Work->HandFlowPython.FilePath)){Result=TEXT("본 보정에 필요한 HandFlow Python 환경을 준비하세요.");return false;}
 Work->OriginalAnimation=nullptr;Work->HandFlowAnimation=nullptr;Work->HandEnhancedAnimation=nullptr;Work->HandComparisonBase=nullptr;Work->JointCorrectedAnimation=nullptr;
 if(!Extract(Work.Get(),Result))return false;
 S->OriginalAnimation=nullptr;S->HandFlowAnimation=nullptr;S->HandComparisonBase=nullptr;S->PreviewAnimation=nullptr;
 S->SaveHandFlow=S->EnhanceHands;S->SaveBoneCorrection=S->AutoBoneCorrection;S->CorrectedBodyAnimation=nullptr;S->CorrectedHandAnimation=nullptr;
 CBMPipelineStart=FPlatformTime::Seconds();CBMPipelineEnd=0;CBMPipelineActive=true;CBMPipelineCancel=false;
 auto Owner=TWeakObjectPtr<UCubelessMocapSettings>(S);
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Work=MoveTemp(Work),Owner,Step=0,Pumping=false](float) mutable {
  if(Pumping)return true;TGuardValue<bool> Guard(Pumping,true);
  if(CurrentJob&&!CurrentJob->Done.Load())return true;
  auto* Target=Owner.Get();
  auto Finish=[&](){CBMPipelineEnd=FPlatformTime::Seconds();CBMPipelineActive=false;if(Target){Target->PreviewAnimation=ResolveSaveAnimation(Target);if(!Target->PreviewAnimation)Target->PreviewAnimation=Target->HandFlowAnimation?Target->HandFlowAnimation:Target->OriginalAnimation;}return false;};
  if(!Target)return Finish();
  if(Step==0&&Work->OriginalAnimation){Target->OriginalAnimation=Work->OriginalAnimation;Target->HandFlowAnimation=Work->HandFlowAnimation;Target->HandComparisonBase=Work->HandComparisonBase;Target->HandResult=Work->HandResult;Target->HandEnhancedAnimation=Work->HandEnhancedAnimation;Target->JointCorrectedAnimation=nullptr;Target->JointComparisonSource=nullptr;}
  if(CBMPipelineCancel||!CurrentJob||!CurrentJob->Success)return Finish();
  if(Step==1){if(Work->EnhanceHands)Target->CorrectedHandAnimation=Work->JointCorrectedAnimation;else Target->CorrectedBodyAnimation=Work->JointCorrectedAnimation;}
  if(!Work->AutoBoneCorrection||Step==1){
   Target->JointCorrectedAnimation=Work->EnhanceHands?Target->CorrectedHandAnimation:Target->CorrectedBodyAnimation;
   Target->JointComparisonSource=Work->EnhanceHands?Target->HandFlowAnimation:Target->OriginalAnimation;
   Target->JointCorrectionReport=Work->JointCorrectionReport;
   return Finish();
  }
  Work->EnableJointCorrection=true;Work->JointCorrectionSource=Work->EnhanceHands?Work->HandFlowAnimation:Work->OriginalAnimation;
  if(Work->JointCorrectionStrength==0)Work->JointCorrectedAnimation=Work->JointCorrectionSource;
  ++Step;FString Error;TGuardValue<bool> Dispatch(CBMPipelineDispatch,true);
  if(!CorrectFingerJoints(Work.Get(),Error)){CBMPipelineEnd=FPlatformTime::Seconds();CBMPipelineActive=false;if(CurrentJob){FScopeLock L(&CurrentJob->Mutex);CurrentJob->Success=false;CurrentJob->Stage=TEXT("Failed");CurrentJob->Message=Error;}return false;}
  return true;
 }),.1f);
 LoadPreview();
 return true;
}

// Missing export correction is computed once, on demand, from the frozen
// selection. Body/HandFlow inference is never repeated by Save.
static bool CBMCorrectAndSave(UCubelessMocapSettings* S,UAnimSequence* Source,FString& Result){
 const bool Hands=S->SaveHandFlow;
 if(S->JointCorrectionStrength==0){if(Hands)S->CorrectedHandAnimation=Source;else S->CorrectedBodyAnimation=Source;return UCubelessMocapLibrary::SaveAnimationSequence(S,Result);}
 auto Work=TStrongObjectPtr<UCubelessMocapSettings>(DuplicateObject<UCubelessMocapSettings>(S,GetTransientPackage()));
 const FString FinalFolder=Work->OutputFolder;
 Work->OutputFolder=TEXT("/Game/_MCP_Temp/CBMWork/Save_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 Work->EnableJointCorrection=true;Work->JointCorrectionSource=Source;
 if(!UCubelessMocapLibrary::CorrectFingerJoints(Work.Get(),Result))return false;
 Work->OutputFolder=FinalFolder;
 CBMPipelineActive=true;CBMPipelineCancel=false;CBMPipelineStart=FPlatformTime::Seconds();CBMPipelineEnd=0;
 auto Owner=TWeakObjectPtr<UCubelessMocapSettings>(S);
 FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Work=MoveTemp(Work),Owner,Hands,Pumping=false](float) mutable {
  if(Pumping)return true;TGuardValue<bool> Guard(Pumping,true);
  if(CurrentJob&&!CurrentJob->Done.Load())return true;
  CBMPipelineActive=false;
  if(CBMPipelineCancel||!CurrentJob||!CurrentJob->Success||!Owner.IsValid()){CBMPipelineEnd=FPlatformTime::Seconds();return false;}
  if(Hands)Work->CorrectedHandAnimation=Work->JointCorrectedAnimation;else Work->CorrectedBodyAnimation=Work->JointCorrectedAnimation;
  auto* Target=Owner.Get();
  if((Hands?Target->HandFlowAnimation:Target->OriginalAnimation)==Work->JointCorrectionSource){
   if(Hands)Target->CorrectedHandAnimation=Work->JointCorrectedAnimation;else Target->CorrectedBodyAnimation=Work->JointCorrectedAnimation;
   Target->PreviewAnimation=UCubelessMocapLibrary::ResolveSaveAnimation(Target);
  }
  FString Message;const bool OK=UCubelessMocapLibrary::SaveAnimationSequence(Work.Get(),Message);
  if(OK)Target->ExportedSequence=Work->ExportedSequence;
  CBMPipelineEnd=FPlatformTime::Seconds();
  {FScopeLock L(&CurrentJob->Mutex);CurrentJob->Success=OK;CurrentJob->Stage=OK?TEXT("Completed"):TEXT("Failed");CurrentJob->Message=Message;}
  return false;
 }),.1f);
 Result=TEXT("선택한 결과의 본 보정을 시작했습니다. 완료 후 지정한 이름으로 저장합니다.");return true;
}

bool UCubelessMocapLibrary::SaveAnimationSequence(UCubelessMocapSettings* S,FString& Result){
 auto Fail=[&](const FString& M){Result=M;UE_LOG(LogCubelessMocap,Warning,TEXT("%s"),*M);return false;};
 if(!S||IsBusy()||(GEditor&&GEditor->PlayWorld))return Fail(TEXT("작업과 PIE가 끝난 뒤 저장하세요."));
 UAnimSequence* Source=ResolveSaveAnimation(S);
 const bool NeedsCorrection=!Source&&S->SaveBoneCorrection;
 if(NeedsCorrection)Source=S->SaveHandFlow?S->HandFlowAnimation.Get():S->OriginalAnimation.Get();
 if(!Source||!Source->GetDataModel())return Fail(TEXT("선택한 조합의 계산 결과가 없습니다. 위 실행 옵션을 켜고 모캡을 다시 실행하세요."));
 if(!FMath::IsFinite(S->SavePlayRate)||S->SavePlayRate<.1f||S->SavePlayRate>4.f)return Fail(TEXT("플레이 레이트는 0.1~4.0 범위입니다."));
 const FString PackageName=S->OutputFolder/S->SequenceName;
 if(S->SequenceName.IsEmpty()||!FName::IsValidXName(S->SequenceName,INVALID_OBJECTNAME_CHARACTERS)||!S->OutputFolder.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(PackageName))return Fail(TEXT("저장 폴더와 애니메이션 이름을 확인하세요."));
 const FString ObjectPath=PackageName+TEXT(".")+S->SequenceName;
 UObject* Existing=LoadObject<UObject>(nullptr,*ObjectPath);auto* Output=Cast<UAnimSequence>(Existing);
 if(Existing&&(!Output||Output->GetPackage()->GetMetaData().GetValue(Output,TEXT("CubelessMocapExport"))!=TEXT("1")))return Fail(TEXT("같은 이름의 다른 에셋이 있습니다. 저장 이름을 바꿔 주세요."));
 if(Output&&(Output==Source||Output->GetOutermost()->IsDirty()||Output->GetSkeleton()!=Source->GetSkeleton()))return Fail(TEXT("저장 대상이 편집 중이거나 스켈레톤이 다릅니다. 해당 에셋을 먼저 확인하세요."));
 if(Source->GetOutermost()->IsDirty())return Fail(TEXT("입력 애니메이션에 미저장 변경이 있습니다."));
 if(NeedsCorrection)return CBMCorrectAndSave(S,Source,Result);
 const auto* Model=Source->GetDataModel();TArray<FName> Names;Model->GetBoneTrackNames(Names);
 TArray<TArray<FTransform>> Tracks;for(FName Name:Names){TArray<FTransform> T;Model->GetBoneTrackTransforms(Name,T);if(T.Num()!=Model->GetNumberOfKeys())return Fail(TEXT("입력 트랙 길이가 올바르지 않습니다."));Tracks.Add(MoveTemp(T));}
 TGuardValue<bool> Saving(CBMExportActive,true);
 const bool New=!Output;
 if(New){Output=DuplicateObject<UAnimSequence>(Source,CreatePackage(*PackageName),*S->SequenceName);Output->SetFlags(RF_Public|RF_Standalone);}
 else{
  Output->Modify();auto& Controller=Output->GetController();Controller.OpenBracket(FText::FromString(TEXT("모캡 결과 갱신")),false);
  Controller.RemoveAllBoneTracks(false);Controller.SetFrameRate(Model->GetFrameRate(),false);Controller.SetNumberOfFrames(FFrameNumber(Model->GetNumberOfFrames()),false);
  bool OK=true;
  for(int32 I=0;I<Names.Num();++I){TArray<FVector3f> P,Z;TArray<FQuat4f> Q;for(const auto& T:Tracks[I]){P.Add(FVector3f(T.GetTranslation()));Q.Add(FQuat4f(T.GetRotation()));Z.Add(FVector3f(T.GetScale3D()));}OK=Controller.AddBoneCurve(Names[I],false)&&OK;OK=Controller.SetBoneTrackKeys(Names[I],P,Q,Z,false)&&OK;}
  Controller.CloseBracket(false);if(!OK)return Fail(TEXT("에셋 트랙 갱신 실패. 미저장 결과를 확인하세요."));
 }
 if(Output->GetDataModel()->GetFrameRate()!=Model->GetFrameRate()||Output->GetDataModel()->GetNumberOfKeys()!=Model->GetNumberOfKeys())return Fail(TEXT("저장 시간축 검증 실패."));
 // Controllers normalize float quaternions: permit roundoff, never resample keys.
 for(int32 I=0;I<Names.Num();++I){TArray<FTransform> Written;Output->GetDataModel()->GetBoneTrackTransforms(Names[I],Written);if(Written.Num()!=Tracks[I].Num())return Fail(TEXT("저장 트랙 길이 검증 실패."));for(int32 F=0;F<Written.Num();++F)if(!Written[F].Equals(Tracks[I][F],1.e-5))return Fail(TEXT("저장 트랙 보존 검증 실패: ")+Names[I].ToString());}
 Output->RateScale=S->SavePlayRate;Output->GetPackage()->GetMetaData().SetValue(Output,TEXT("CubelessMocapExport"),TEXT("1"));Output->MarkPackageDirty();
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;const FString File=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
 if(!UPackage::SavePackage(Output->GetOutermost(),Output,*File,Args))return Fail(TEXT("저장 실패. 미저장 에셋을 확인하세요."));
 if(New)FAssetRegistryModule::AssetCreated(Output);S->ExportedSequence=Output;Result=TEXT("저장 완료: ")+ObjectPath;return true;
}
