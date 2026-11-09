#include "Modules/ModuleManager.h"
#include "CubelessMocapSettings.h"
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
class FCubelessMocapModule : public IModuleInterface {
 TStrongObjectPtr<UCubelessMocapSettings> Settings;
 FDelegateHandle CanCloseHandle;TWeakPtr<SWindow> ExistingWindow;
 TWeakPtr<SCBMComparison> Comparison;
 FString SaveNotice;
public:
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
  VisibleProperties.Add(TEXT("FootLocking"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("FootLocking"))){P->SetMetaData(TEXT("DisplayName"),TEXT("기본 발 고정"));P->SetMetaData(TEXT("ToolTip"),TEXT("전신 추출 단계에서 접지한 발의 움직임을 억제합니다."));}
  VisibleProperties.Add(TEXT("ReuseCheckpoints"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("ReuseCheckpoints"))){P->SetMetaData(TEXT("DisplayName"),TEXT("계산 캐시 재사용"));P->SetMetaData(TEXT("ToolTip"),TEXT("동일 입력과 설정의 완료된 계산을 재사용합니다. 끄면 다시 계산합니다."));}
  VisibleProperties.Add(TEXT("UseFusedCHMR"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("UseFusedCHMR"))){P->SetMetaData(TEXT("DisplayName"),TEXT("고속 모드"));P->SetMetaData(TEXT("ToolTip"),TEXT("켜면 최적화된 CHMR와 SAM2 경로를 사용합니다. 끄면 원본 경로를 사용하며 작은 수치 차이가 있을 수 있습니다."));}
  VisibleProperties.Add(TEXT("TargetMesh"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("TargetMesh"))){P->SetMetaData(TEXT("DisplayName"),TEXT("대상 스켈레탈 메시"));P->SetMetaData(TEXT("ToolTip"),TEXT("추출한 움직임을 적용할 대상 메시입니다."));}
  VisibleProperties.Add(TEXT("Retargeter"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("Retargeter"))){P->SetMetaData(TEXT("DisplayName"),TEXT("IK 리타기터"));P->SetMetaData(TEXT("ToolTip"),TEXT("전신 모션을 대상 스켈레톤으로 변환하는 리타기터입니다."));}
  VisibleProperties.Add(TEXT("OutputFolder"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("OutputFolder"))){P->SetMetaData(TEXT("DisplayName"),TEXT("최종 저장 폴더"));P->SetMetaData(TEXT("ToolTip"),TEXT("저장 버튼이 갱신할 최종 에셋 폴더입니다. /Game 아래 경로를 사용하세요. 계산 중간 결과는 별도 내부 작업 폴더에 보관됩니다."));}
  VisibleProperties.Add(TEXT("SequenceName"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("SequenceName"))){P->SetMetaData(TEXT("DisplayName"),TEXT("애니메이션 이름"));P->SetMetaData(TEXT("ToolTip"),TEXT("저장 옵션을 변경해도 이 이름의 같은 에셋을 갱신합니다. 새 에셋이 필요할 때만 이름을 바꾸세요."));}
  VisibleProperties.Add(TEXT("HandFlowPython"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowPython"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow Python"));P->SetMetaData(TEXT("ToolTip"),TEXT("손 추론과 본 보정에 사용할 준비된 Python 실행 환경입니다."));}
  VisibleProperties.Add(TEXT("HandFlowDirectory"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowDirectory"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow 모델 폴더"));P->SetMetaData(TEXT("ToolTip"),TEXT("HaMeR와 HandFlow 모델 및 실행 파일이 준비된 폴더입니다."));}
  VisibleProperties.Add(TEXT("HandFlowSteps"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowSteps"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow 추론 스텝"));P->SetMetaData(TEXT("ToolTip"),TEXT("시간축 추론 반복 수입니다. 기본 3이며 변경하면 처리 시간과 결과가 달라집니다."));}
  VisibleProperties.Add(TEXT("HandFlowBatch"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowBatch"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow 배치"));P->SetMetaData(TEXT("ToolTip"),TEXT("동시에 처리할 손 이미지 수입니다. 높이면 메모리 사용량이 늘어납니다."));}
  VisibleProperties.Add(TEXT("HandFlowSeed"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("HandFlowSeed"))){P->SetMetaData(TEXT("DisplayName"),TEXT("HandFlow 시드"));P->SetMetaData(TEXT("ToolTip"),TEXT("동일 설정의 추론을 재현하기 위한 난수 시드입니다."));}
  VisibleProperties.Add(TEXT("JointCorrectionStrength"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("JointCorrectionStrength"))){P->SetMetaData(TEXT("DisplayName"),TEXT("본 보정 강도"));P->SetMetaData(TEXT("ToolTip"),TEXT("손가락 관절 범위와 관통 보정의 강도입니다. 1은 전체 적용, 0은 원본입니다. 전신과 손목은 바꾸지 않습니다."));}
  VisibleProperties.Add(TEXT("MatchVideoCamera"));if(auto* P=FindFProperty<FProperty>(UCubelessMocapSettings::StaticClass(),TEXT("MatchVideoCamera"))){P->SetMetaData(TEXT("DisplayName"),TEXT("영상 카메라 맞춤"));P->SetMetaData(TEXT("ToolTip"),TEXT("영상의 추정 카메라와 구도를 비교 뷰에 적용합니다. 애니메이션 데이터는 변경하지 않습니다."));}
  FDetailsViewArgs Args;Args.bHideSelectionTip=true;Args.bAllowSearch=true;auto& Properties=FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");auto Details=Properties.CreateDetailView(Args);
  Details->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([VisibleProperties](const FPropertyAndParent& P){return P.Property.GetOwnerClass()!=UCubelessMocapSettings::StaticClass()||VisibleProperties.Contains(P.Property.GetFName());}));
  Details->SetObject(Settings.Get());Details->SetEnabled(TAttribute<bool>::CreateLambda([](){return !UCubelessMocapLibrary::IsBusy();}));
  auto Check=[this](const TCHAR* Label,const TCHAR* Tip,bool UCubelessMocapSettings::* Field,bool Preview){return SNew(SCheckBox).ToolTipText(FText::FromString(Tip)).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).IsChecked_Lambda([this,Field](){return Settings.Get()->*Field?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this,Field,Preview](ECheckBoxState V){Settings.Get()->*Field=V==ECheckBoxState::Checked;if(Preview)Settings->PreviewAnimation=UCubelessMocapLibrary::ResolveSaveAnimation(Settings.Get());})[SNew(STextBlock).Text(FText::FromString(Label))];};
  auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Cubeless Body Mocap"))).ClientSize(FVector2D(1280,900));TSharedPtr<SCBMComparison> ComparisonWidget;
  Window->SetContent(SNew(SVerticalBox)
   +SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(.30f)[Details]
    +SHorizontalBox::Slot().FillWidth(.70f)[SAssignNew(ComparisonWidget,SCBMComparison).Settings(Settings.Get())]]
   +SVerticalBox::Slot().AutoHeight().Padding(8,4)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([](){return FText::FromString(UCubelessMocapLibrary::GetJobStatus());})]
   +SVerticalBox::Slot().AutoHeight().Padding(8,4)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,16,0)[SNew(STextBlock).Text(FText::FromString(TEXT("모캡 실행 옵션")))]
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,16,0)[Check(TEXT("HandFlow"),TEXT("전신 추출 후 HandFlow 손 추론과 영상 보강을 실행합니다. 기본 켜짐입니다."),&UCubelessMocapSettings::EnhanceHands,false)]
    +SHorizontalBox::Slot().AutoWidth()[Check(TEXT("본 보정"),TEXT("모캡 후 손가락 관절 범위·관통을 보정합니다. 전신·손목 보정은 아닙니다. 저장 조합 선택을 위해 전신 원본과 HandFlow 결과를 각각 보정합니다. 기본 켜짐입니다."),&UCubelessMocapSettings::AutoBoneCorrection,false)]]
   +SVerticalBox::Slot().AutoHeight().Padding(8,4)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("Start Mocap"))).ToolTipText(FText::FromString(TEXT("위 실행 옵션에 따라 전신 → HandFlow → 본 보정을 진행합니다. 최종 에셋은 아래 저장 버튼으로 저장하세요."))).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){SaveNotice.Empty();FString R;if(!UCubelessMocapLibrary::StartMocap(Settings.Get(),R))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(R));return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString(TEXT("취소"))).ToolTipText(FText::FromString(TEXT("진행 중인 계산과 이후 단계를 취소합니다. 이미 완료한 작업 결과는 남습니다."))).IsEnabled_Lambda([](){return UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([](){UCubelessMocapLibrary::Cancel();return FReply::Handled();})]]
   +SVerticalBox::Slot().AutoHeight().Padding(8,6)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("애니메이션 시퀀스 저장"))).ToolTipText(FText::FromString(TEXT("옆의 저장 옵션으로 같은 경로의 최종 애니메이션을 생성하거나 갱신합니다. 재추론하지 않습니다."))).IsEnabled_Lambda([this](){return !UCubelessMocapLibrary::IsBusy()&&UCubelessMocapLibrary::ResolveSaveAnimation(Settings.Get());}).OnClicked_Lambda([this](){if(!UCubelessMocapLibrary::SaveAnimationSequence(Settings.Get(),SaveNotice))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(SaveNotice));return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth().Padding(12,0)[Check(TEXT("HandFlow"),TEXT("계산된 HandFlow 손 결과를 저장에 포함합니다. 바꾸면 오른쪽 뷰도 해당 결과로 바뀝니다."),&UCubelessMocapSettings::SaveHandFlow,true)]
    +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[Check(TEXT("본 보정"),TEXT("계산된 손가락 관절·관통 보정을 저장에 포함합니다. 실행하지 않은 조합은 저장할 수 없습니다."),&UCubelessMocapSettings::SaveBoneCorrection,true)]
    +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("플레이 레이트")))]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(90)[SNew(SSpinBox<float>).MinValue(.1f).MaxValue(4.f).Delta(.05f).ToolTipText(FText::FromString(TEXT("저장할 애니메이션의 RateScale입니다. 1은 원래 속도, 0.5는 절반 속도입니다. 키프레임은 재샘플링하지 않으며 아래 미리보기 속도와 별개입니다."))).Value_Lambda([this](){return Settings->SavePlayRate;}).OnValueChanged_Lambda([this](float V){Settings->SavePlayRate=V;})]]]
   +SVerticalBox::Slot().AutoHeight().Padding(8,4)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){if(!UCubelessMocapLibrary::IsBusy()&&!UCubelessMocapLibrary::ResolveSaveAnimation(Settings.Get()))return FText::FromString(TEXT("선택한 저장 조합의 결과가 없습니다. 모캡 실행 옵션을 켜고 실행하거나 저장 옵션을 해제하세요."));return FText::FromString(SaveNotice);})]);
  Comparison=ComparisonWidget;ExistingWindow=Window;
  FSlateApplication::Get().AddWindow(Window);
 }
 virtual bool SupportsDynamicReloading() override {return false;}
 virtual void ShutdownModule() override {UCubelessMocapLibrary::ShutdownJobs(); if(FModuleManager::Get().IsModuleLoaded(TEXT("MainFrame")))FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame")).UnregisterCanCloseEditor(CanCloseHandle);if(auto W=ExistingWindow.Pin())W->RequestDestroyWindow();UToolMenus::UnRegisterStartupCallback(this);UToolMenus::UnregisterOwner(this);Settings.Reset();}
};
IMPLEMENT_MODULE(FCubelessMocapModule,CBMEditor)

void UCubelessMocapLibrary::OpenWindow() { FModuleManager::LoadModuleChecked<FCubelessMocapModule>(TEXT("CBMEditor")).Open(); }
void UCubelessMocapLibrary::LoadPreview() { FModuleManager::LoadModuleChecked<FCubelessMocapModule>(TEXT("CBMEditor")).LoadPreview(); }
bool UCubelessMocapLibrary::SavePreviewScreenshot() { return FModuleManager::LoadModuleChecked<FCubelessMocapModule>(TEXT("CBMEditor")).SavePreviewScreenshot(); }
