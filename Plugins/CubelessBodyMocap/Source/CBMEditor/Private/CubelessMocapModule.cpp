#include "Modules/ModuleManager.h"
#include "CubelessMocapSettings.h"
#include "CubelessMocapWindowLibrary.h"
#include "UObject/CoreRedirects.h"
#include "UObject/StrongObjectPtr.h"
#include "ToolMenus.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "SCBMComparison.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Animation/AnimSequence.h"
class FCubelessMocapModule : public IModuleInterface {
 friend class UCubelessMocapWindowLibrary;
 TStrongObjectPtr<UCubelessMocapSettings> Settings;
 FDelegateHandle CanCloseHandle;TWeakPtr<SWindow> ExistingWindow;
 TWeakPtr<SCBMComparison> Comparison;
 TWeakPtr<SSplitter> LayoutSplitter;
 FString SaveNotice;
public:
 UCubelessMocapSettings* GetWindowSettings();
 bool SetVideo(const FString& Path,FString& Message);
 TSharedRef<FJsonObject> ReadWindowState()const;
 FString Reply(const FString& Action,bool Success,const FString& Message)const;
 FString ChangeLayout(float PreviewFraction,float VideoFraction,FVector2D Size);
 FString CloseWindow();
 FText ResultLabel()const{return FText::FromString(Settings->SaveHandFlow?(Settings->SaveBoneCorrection?TEXT("HandFlow + 손가락 보정"):TEXT("HandFlow")):(Settings->SaveBoneCorrection?TEXT("전신 + 손가락 보정"):TEXT("전신 원본")));}
 bool SelectResult(bool Hands,bool Correct,FString* Message=nullptr){
  if(UCubelessMocapLibrary::IsBusy()||!Settings.IsValid()||!(Hands?Settings->HandFlowAnimation:Settings->OriginalAnimation)){if(Message)*Message=TEXT("busy_or_source_missing");return false;}Settings->SaveHandFlow=Hands;Settings->SaveBoneCorrection=Correct;
  Settings->PreviewAnimation=UCubelessMocapLibrary::ResolveSaveAnimation(Settings.Get());
  if(!Settings->PreviewAnimation)Settings->PreviewAnimation=Hands?Settings->HandFlowAnimation:Settings->OriginalAnimation;
  if(auto P=Comparison.Pin())P->ShowResult(true);
  if(Message)*Message=UCubelessMocapLibrary::ResolveSaveAnimation(Settings.Get())?TEXT("selected"):TEXT("selected_uncorrected_preview; correction runs on export");return true;
 }
 TSharedRef<SWidget> ResultMenu(){
  FMenuBuilder Menu(true,nullptr);
  for(int32 I=0;I<4;++I){const bool Hands=(I&2)!=0,Correct=(I&1)!=0;
   const TCHAR* Labels[]={TEXT("전신 원본"),TEXT("전신 + 손가락 보정"),TEXT("HandFlow"),TEXT("HandFlow + 손가락 보정")};
   Menu.AddMenuEntry(FText::FromString(Labels[I]),FText::FromString(TEXT("선택한 결과를 미리 보고 내보냅니다. 없는 손가락 보정은 내보낼 때만 계산합니다.")),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this,Hands,Correct](){SelectResult(Hands,Correct);}),FCanExecuteAction::CreateLambda([this,Hands](){return !UCubelessMocapLibrary::IsBusy()&&(Hands?Settings->HandFlowAnimation!=nullptr:Settings->OriginalAnimation!=nullptr);}),FIsActionChecked::CreateLambda([this,Hands,Correct](){return Settings->SaveHandFlow==Hands&&Settings->SaveBoneCorrection==Correct;})),NAME_None,EUserInterfaceActionType::RadioButton);
  }return Menu.MakeWidget();
 }
 FReply ChooseVideo(){
  TArray<FString> Files;auto* Desktop=FDesktopPlatformModule::Get();
  if(Desktop&&Desktop->OpenFileDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(ExistingWindow.Pin()),TEXT("모캡 영상 열기"),FPaths::GetPath(Settings->Video.FilePath),TEXT(""),TEXT("동영상|*.mp4;*.mov;*.mkv;*.avi"),0,Files)&&Files.Num()){
   FString Message;if(!SetVideo(Files[0],Message))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(Message));
  }return FReply::Handled();
 }
 static FText Summary(){FString Full=UCubelessMocapLibrary::GetJobStatus(),First;return FText::FromString(Full.Split(TEXT("\n"),&First,nullptr)?First:Full);}
 static TOptional<float> Progress(){FString Full=UCubelessMocapLibrary::GetJobStatus();int32 End=Full.Find(TEXT("%"));if(End==INDEX_NONE)return 0.f;int32 Start=Full.Find(TEXT("|"),ESearchCase::CaseSensitive,ESearchDir::FromEnd,End);return FMath::Clamp(FCString::Atof(*Full.Mid(Start+1,End-Start-1))/100.f,0.f,1.f);}
 void LoadPreview(){if(auto P=Comparison.Pin())P->LoadPreview();}
 bool SavePreviewScreenshot(){
  auto P=Comparison.Pin();if(!P||!P->PrepareScreenshot())return false;
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(P.ToSharedRef(),Pixels,Size))return false;
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap");IFileManager::Get().MakeDirectory(*Folder,true);
  const bool Saved=FFileHelper::SaveArrayToFile(PNG,*(Folder/TEXT("ComparisonPreview.png")));
  if(auto W=ExistingWindow.Pin()){Pixels.Reset();if(FSlateApplication::Get().TakeScreenshot(W.ToSharedRef(),Pixels,Size)){PNG.Reset();FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Folder/TEXT("WorkflowPreview.png")));}}
  return Saved;
 }
 virtual void StartupModule() override {
  TArray<FCoreRedirect> Redirects;
  Redirects.Emplace(ECoreRedirectFlags::Type_Class,TEXT("/Script/MetaHumanBodyTracker.MetaHumanSMPLXData"),TEXT("/Script/CBMEditor.MetaHumanSMPLXData"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Struct,TEXT("/Script/Segmentation.SAM2WeightRow"),TEXT("/Script/CBMSegmentation.SAM2WeightRow"));
  // Redirect only copied asset packages, leaving any other installed assets untouched.
  for(const TCHAR* Name:{TEXT("SKEL_SMPL"),TEXT("SKM_SMPL"),TEXT("IK_SMPL"),TEXT("PA_SMPL"),TEXT("SMPLX_NEUTRAL_2020_locked_head_array_f32")})
   Redirects.Emplace(ECoreRedirectFlags::Type_Package,FString(TEXT("/MetaHumanBodyTracker/"))+Name,FString(TEXT("/CubelessBodyMocap/"))+Name);
  FCoreRedirects::AddRedirectList(Redirects,TEXT("CubelessBodyMocap"));
  CanCloseHandle=FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame")).RegisterCanCloseEditor(IMainFrameModule::FMainFrameCanCloseEditor::CreateLambda([](){if(UCubelessMocapLibrary::IsBusy()){UCubelessMocapLibrary::Cancel();UE_LOG(LogTemp,Warning,TEXT("Cubeless Body Mocap: cancelling. Close again after the current native call returns."));return false;}return true;}));
  UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FCubelessMocapModule::Menus));
 }
 void Menus(){FToolMenuOwnerScoped Owner(this);auto* Menu=UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");Menu->FindOrAddSection("Cubeless").AddMenuEntry("CubelessBodyMocap",FText::FromString(TEXT("Cubeless Body Mocap")),FText::FromString(TEXT("Extract body motion from video")),FSlateIcon(),FUIAction(FExecuteAction::CreateRaw(this,&FCubelessMocapModule::Open)));}
 void Open(){
  if(auto W=ExistingWindow.Pin()){W->ShowWindow();W->BringToFront(true);return;}
  if (const auto Main=FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame")).GetParentWindow()) {
   Main->ShowWindow();Main->BringToFront(true);
  }
  if(!Settings.IsValid()) {
   Settings.Reset(NewObject<UCubelessMocapSettings>());
   if (const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("CubelessBodyMocap"))){
    Settings->FFmpeg.FilePath=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("ThirdParty/FFmpeg/Win64/ffmpeg.exe"));
    Settings->HandPython.FilePath=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("ThirdParty/WiLoR/venv/Scripts/python.exe"));
    Settings->WiLoRDirectory.Path=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("ThirdParty/WiLoR/repo"));
    Settings->MediaPipePython.FilePath=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("ThirdParty/MediaPipe/venv/Scripts/python.exe"));
    Settings->HandFlowPython.FilePath=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("ThirdParty/HandFlow/venv/Scripts/python.exe"));
    Settings->HandFlowDirectory.Path=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("ThirdParty/HandFlow"));
    Settings->MediaPipeModel.FilePath=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("ThirdParty/MediaPipe/hand_landmarker.task"));
   }
   Settings->TargetMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/UE5_Mannequins/Meshes/SKM_Manny.SKM_Manny"));
   Settings->Retargeter=LoadObject<UIKRetargeter>(nullptr,TEXT("/Game/CubelessMocap/Setup/RTG_CBM_SMPL_to_Manny.RTG_CBM_SMPL_to_Manny"));
  }
  TSet<FName> VisibleProperties;
  VisibleProperties.Add(TEXT("Video"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("Video"))){P->SetMetaData(TEXT("DisplayName"),TEXT("영상 파일"));P->SetMetaData(TEXT("ToolTip"),TEXT("분석할 동영상입니다. 영상 변경 후 비교 뷰에서 영상 열기를 누르세요."));}
  VisibleProperties.Add(TEXT("FFmpeg"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("FFmpeg"))){P->SetMetaData(TEXT("DisplayName"),TEXT("FFmpeg 경로"));P->SetMetaData(TEXT("ToolTip"),TEXT("영상 프레임 추출에 사용할 플러그인 내 FFmpeg 실행 파일입니다."));}
  VisibleProperties.Add(TEXT("FrameDirectory"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("FrameDirectory"))){P->SetMetaData(TEXT("DisplayName"),TEXT("기존 프레임 폴더"));P->SetMetaData(TEXT("ToolTip"),TEXT("PNG 프레임을 직접 사용할 때만 지정합니다. 일반 영상 작업은 비워 두세요."));}
  VisibleProperties.Add(TEXT("StartSeconds"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("StartSeconds"))){P->SetMetaData(TEXT("DisplayName"),TEXT("시작 시간"));P->SetMetaData(TEXT("ToolTip"),TEXT("영상에서 모캡을 시작할 시점입니다. 단위는 초입니다."));}
  VisibleProperties.Add(TEXT("DurationSeconds"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("DurationSeconds"))){P->SetMetaData(TEXT("DisplayName"),TEXT("분석 길이"));P->SetMetaData(TEXT("ToolTip"),TEXT("시작 시간부터 분석할 길이입니다. 단위는 초이며 최대 120초입니다."));}
  VisibleProperties.Add(TEXT("FrameRate"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("FrameRate"))){P->SetMetaData(TEXT("DisplayName"),TEXT("프레임 레이트"));P->SetMetaData(TEXT("ToolTip"),TEXT("분석 및 애니메이션 생성 FPS입니다. 저장 플레이 레이트와는 별개입니다."));}
  VisibleProperties.Add(TEXT("PauseEditorRendering"));
  VisibleProperties.Add(TEXT("SoleGrounding"));
  VisibleProperties.Add(TEXT("FootLocking"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("FootLocking"))){P->SetMetaData(TEXT("DisplayName"),TEXT("기본 발 고정"));P->SetMetaData(TEXT("ToolTip"),TEXT("전신 추출 단계에서 접지한 발의 움직임을 억제합니다."));}
  VisibleProperties.Add(TEXT("ReuseCheckpoints"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("ReuseCheckpoints"))){P->SetMetaData(TEXT("DisplayName"),TEXT("계산 캐시 재사용"));P->SetMetaData(TEXT("ToolTip"),TEXT("동일 입력과 설정의 완료된 계산을 재사용합니다. 끄면 다시 계산합니다."));}
  VisibleProperties.Add(TEXT("UseFusedCHMR"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("UseFusedCHMR"))){P->SetMetaData(TEXT("DisplayName"),TEXT("고속 모드"));P->SetMetaData(TEXT("ToolTip"),TEXT("켜면 최적화된 CHMR와 SAM2 경로를 사용합니다. 끄면 원본 경로를 사용하며 작은 수치 차이가 있을 수 있습니다."));}
  VisibleProperties.Add(TEXT("TargetMesh"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("TargetMesh"))){P->SetMetaData(TEXT("DisplayName"),TEXT("대상 스켈레탈 메시"));P->SetMetaData(TEXT("ToolTip"),TEXT("추출한 움직임을 적용할 대상 메시입니다."));}
  VisibleProperties.Add(TEXT("Retargeter"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("Retargeter"))){P->SetMetaData(TEXT("DisplayName"),TEXT("IK 리타기터"));P->SetMetaData(TEXT("ToolTip"),TEXT("전신 모션을 대상 스켈레톤으로 변환하는 리타기터입니다."));}
  VisibleProperties.Add(TEXT("OutputFolder"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("OutputFolder"))){P->SetMetaData(TEXT("DisplayName"),TEXT("최종 저장 폴더"));P->SetMetaData(TEXT("ToolTip"),TEXT("저장 버튼이 갱신할 최종 에셋 폴더입니다. /Game 아래 경로를 사용하세요. 계산 중간 결과는 별도 내부 작업 폴더에 보관됩니다."));}
  VisibleProperties.Add(TEXT("SequenceName"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("SequenceName"))){P->SetMetaData(TEXT("DisplayName"),TEXT("애니메이션 이름"));P->SetMetaData(TEXT("ToolTip"),TEXT("저장 옵션을 변경해도 이 이름의 같은 에셋을 갱신합니다. 새 에셋이 필요할 때만 이름을 바꾸세요."));}
  VisibleProperties.Add(TEXT("HandFlowPython"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowPython"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow Python"));P->SetMetaData(TEXT("ToolTip"),TEXT("손 추론·본 보정·전체 시간축 영상 보완에 사용할 준비된 Python 실행 환경입니다."));}
  VisibleProperties.Add(TEXT("HandFlowDirectory"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowDirectory"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow 모델 폴더"));P->SetMetaData(TEXT("ToolTip"),TEXT("HaMeR와 HandFlow 모델 및 실행 파일이 준비된 폴더입니다."));}
  VisibleProperties.Add(TEXT("HandFlowSteps"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowSteps"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow 추론 스텝"));P->SetMetaData(TEXT("ToolTip"),TEXT("시간축 추론 반복 수입니다. 기본 3이며 변경하면 처리 시간과 결과가 달라집니다."));}
  VisibleProperties.Add(TEXT("HandFlowBatch"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowBatch"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow 배치"));P->SetMetaData(TEXT("ToolTip"),TEXT("동시에 처리할 손 이미지 수입니다. 높이면 메모리 사용량이 늘어납니다."));}
  VisibleProperties.Add(TEXT("HandFlowSeed"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowSeed"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow 시드"));P->SetMetaData(TEXT("ToolTip"),TEXT("동일 설정의 추론을 재현하기 위한 난수 시드입니다."));}
  VisibleProperties.Add(TEXT("ImageLimbAlignment"));VisibleProperties.Add(TEXT("ImageAlignmentMode"));VisibleProperties.Add(TEXT("JointCorrectionStrength"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("JointCorrectionStrength"))){P->SetMetaData(TEXT("DisplayName"),TEXT("손가락 보정 강도"));P->SetMetaData(TEXT("ToolTip"),TEXT("손가락 관절 범위와 관통 보정의 강도입니다. 1은 전체 적용, 0은 원본입니다. 전신과 손목은 바꾸지 않습니다."));}
  VisibleProperties.Add(TEXT("MatchVideoCamera"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("MatchVideoCamera"))){P->SetMetaData(TEXT("DisplayName"),TEXT("영상 카메라 맞춤"));P->SetMetaData(TEXT("ToolTip"),TEXT("영상의 추정 카메라와 구도를 비교 뷰에 적용합니다. 애니메이션 데이터는 변경하지 않습니다."));}
  // Environment and inference tuning remain available, but do not obscure daily work.
  TSet<FName> Advanced;for(const TCHAR* N:{TEXT("FFmpeg"),TEXT("FrameDirectory"),TEXT("HandFlowPython"),TEXT("HandFlowDirectory"),TEXT("HandFlowSteps"),TEXT("HandFlowBatch"),TEXT("HandFlowSeed"),TEXT("UseFusedCHMR"),TEXT("PauseEditorRendering"),TEXT("JointCorrectionStrength")})Advanced.Add(N);
  VisibleProperties.Remove(TEXT("StartSeconds"));VisibleProperties.Remove(TEXT("DurationSeconds"));
  auto& Properties=FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
  auto MakeDetails=[&](bool IsAdvanced){FDetailsViewArgs Args;Args.bHideSelectionTip=true;Args.bAllowSearch=true;auto D=Properties.CreateDetailView(Args);
   D->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([VisibleProperties,Advanced,IsAdvanced](const FPropertyAndParent& P){return P.Property.GetOwnerClass()!=UCubelessMocapSettings::StaticClass()||(VisibleProperties.Contains(P.Property.GetFName())&&Advanced.Contains(P.Property.GetFName())==IsAdvanced);}));
   D->SetObject(Settings.Get());D->SetEnabled(TAttribute<bool>::CreateLambda([](){return !UCubelessMocapLibrary::IsBusy();}));return D;};
  auto Basic=MakeDetails(false),AdvancedDetails=MakeDetails(true);
  auto Check=[this](const TCHAR* Label,bool UCubelessMocapSettings::* Field){return SNew(SCheckBox).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).IsChecked_Lambda([this,Field](){return Settings.Get()->*Field?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this,Field](ECheckBoxState V){Settings.Get()->*Field=V==ECheckBoxState::Checked;})[SNew(STextBlock).Text(FText::FromString(Label))];};
  auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Cubeless Body Mocap"))).ClientSize(FVector2D(1440,960)).MinWidth(1000).MinHeight(720);TSharedPtr<SCBMComparison> ComparisonWidget;TSharedPtr<SSplitter> LayoutWidget;
  Window->SetContent(SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(6)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("1  영상 열기"))).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).OnClicked_Raw(this,&FCubelessMocapModule::ChooseVideo)]
    +SHorizontalBox::Slot().AutoWidth().Padding(8,2)[SNew(SButton).Text(FText::FromString(TEXT("2  모캡 실행"))).ToolTipText(FText::FromString(TEXT("선택한 구간과 계산 옵션으로 모캡을 실행합니다. 결과는 검토 후 별도로 내보냅니다."))).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){SaveNotice.Empty();FString R;if(!UCubelessMocapLibrary::StartMocap(Settings.Get(),R))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(R));return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("취소"))).IsEnabled_Lambda([](){return UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([](){UCubelessMocapLibrary::Cancel();return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth().Padding(12,2)[SNew(SButton).Text(FText::FromString(TEXT("3  선택 결과 내보내기"))).ToolTipText(FText::FromString(TEXT("선택한 결과를 출력 경로에 저장합니다. 없는 손가락 보정만 추가 계산하며 전신·손 추론을 반복하지 않습니다."))).IsEnabled_Lambda([this](){return !UCubelessMocapLibrary::IsBusy()&&(Settings->SaveHandFlow?Settings->HandFlowAnimation!=nullptr:Settings->OriginalAnimation!=nullptr);}).OnClicked_Lambda([this](){if(!UCubelessMocapLibrary::SaveAnimationSequence(Settings.Get(),SaveNotice))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(SaveNotice));return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("저장 에셋 열기"))).IsEnabled_Lambda([this](){return !UCubelessMocapLibrary::IsBusy()&&Settings->ExportedSequence!=nullptr;}).OnClicked_Lambda([this](){GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Settings->ExportedSequence);return FReply::Handled();})]
    +SHorizontalBox::Slot().FillWidth(1).Padding(12,4)[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(FPaths::GetCleanFilename(Settings->Video.FilePath));}).ToolTipText_Lambda([this](){return FText::FromString(Settings->Video.FilePath);})]]]
   +SVerticalBox::Slot().AutoHeight().Padding(10,2)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("표시 / 내보낼 결과")))]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SComboButton).OnGetMenuContent_Raw(this,&FCubelessMocapModule::ResultMenu).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).ButtonContent()[SNew(STextBlock).Text_Lambda([this](){return ResultLabel();})]]
    +SHorizontalBox::Slot().AutoWidth().Padding(20,0,6,0)[SNew(STextBlock).Text(FText::FromString(TEXT("저장 플레이 레이트")))]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(80)[SNew(SSpinBox<float>).MinValue(.1f).MaxValue(4.f).Delta(.05f).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).ToolTipText(FText::FromString(TEXT("내보낼 애니메이션의 RateScale입니다. 아래 재생 속도와 별개이며 키프레임을 재샘플링하지 않습니다."))).Value_Lambda([this](){return Settings->SavePlayRate;}).OnValueChanged_Lambda([this](float V){Settings->SavePlayRate=V;})]]
    +SHorizontalBox::Slot().FillWidth(1).Padding(12,0)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){if(!UCubelessMocapLibrary::IsBusy()&&!UCubelessMocapLibrary::ResolveSaveAnimation(Settings.Get()))return FText::FromString((Settings->SaveHandFlow?Settings->HandFlowAnimation!=nullptr:Settings->OriginalAnimation!=nullptr)?TEXT("손가락 보정은 내보낼 때 계산합니다. 현재는 보정 전 결과입니다."):TEXT("계산 후 결과를 선택할 수 있습니다."));return FText::FromString(SaveNotice);})]]
   +SVerticalBox::Slot().FillHeight(1).Padding(6)[SAssignNew(LayoutWidget,SSplitter)
    +SSplitter::Slot().Value(.75f)[SAssignNew(ComparisonWidget,SCBMComparison).Settings(Settings.Get())]
    +SSplitter::Slot().Value(.25f)[SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).Text(FText::FromString(TEXT("계산할 옵션"))).Font(FAppStyle::GetFontStyle("BoldFont"))]
     +SVerticalBox::Slot().AutoHeight().Padding(8,2)[Check(TEXT("HandFlow 손 보강"),&UCubelessMocapSettings::EnhanceHands)]
     +SVerticalBox::Slot().AutoHeight().Padding(8,2)[Check(TEXT("손가락 관절 보정"),&UCubelessMocapSettings::AutoBoneCorrection)]
     +SVerticalBox::Slot().FillHeight(1)[Basic]
     +SVerticalBox::Slot().AutoHeight()[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNew(STextBlock).Text(FText::FromString(TEXT("고급 / 실행 환경")))].BodyContent()[SNew(SBox).MaxDesiredHeight(300)[AdvancedDetails]]]]]
   +SVerticalBox::Slot().AutoHeight().Padding(10,3)[SNew(STextBlock).Text_Static(&FCubelessMocapModule::Summary).ToolTipText_Lambda([](){return FText::FromString(UCubelessMocapLibrary::GetJobStatus());})]
   +SVerticalBox::Slot().AutoHeight().Padding(10,3)[SNew(SProgressBar).Percent_Static(&FCubelessMocapModule::Progress)]
   +SVerticalBox::Slot().AutoHeight().Padding(10,3)[SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNew(STextBlock).Text(FText::FromString(TEXT("상세 처리 로그")))].BodyContent()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([](){return FText::FromString(UCubelessMocapLibrary::GetJobStatus());})]]);
  Comparison=ComparisonWidget;ExistingWindow=Window;LayoutSplitter=LayoutWidget;
  FSlateApplication::Get().AddWindow(Window);
 }
 virtual bool SupportsDynamicReloading() override {return false;}
 virtual void ShutdownModule() override {UCubelessMocapLibrary::ShutdownJobs(); if(FModuleManager::Get().IsModuleLoaded(TEXT("MainFrame")))FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame")).UnregisterCanCloseEditor(CanCloseHandle);if(auto W=ExistingWindow.Pin())W->RequestDestroyWindow();UToolMenus::UnRegisterStartupCallback(this);UToolMenus::UnregisterOwner(this);Settings.Reset();}
};
IMPLEMENT_MODULE(FCubelessMocapModule,CBMEditor)

void UCubelessMocapLibrary::OpenWindow() { FModuleManager::LoadModuleChecked<FCubelessMocapModule>(TEXT("CBMEditor")).Open(); }
void UCubelessMocapLibrary::LoadPreview() { FModuleManager::LoadModuleChecked<FCubelessMocapModule>(TEXT("CBMEditor")).LoadPreview(); }
bool UCubelessMocapLibrary::SavePreviewScreenshot() { return FModuleManager::LoadModuleChecked<FCubelessMocapModule>(TEXT("CBMEditor")).SavePreviewScreenshot(); }

#include "CBMWindowControl.inl"
