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
public:
 void LoadPreview(){if(auto P=Comparison.Pin())P->LoadPreview();}
 bool SavePreviewScreenshot(){
  auto P=Comparison.Pin();if(!P||!P->PrepareScreenshot())return false;
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(P.ToSharedRef(),Pixels,Size))return false;
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap");IFileManager::Get().MakeDirectory(*Folder,true);
  return FFileHelper::SaveArrayToFile(PNG,*(Folder/TEXT("ComparisonPreview.png")));
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
 void Menus(){FToolMenuOwnerScoped Owner(this);auto* Menu=UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");Menu->FindOrAddSection("Cubeless").AddMenuEntry("CubelessBodyMocap",FText::FromString("Cubeless Body Mocap"),FText::FromString("Extract body motion from video"),FSlateIcon(),FUIAction(FExecuteAction::CreateRaw(this,&FCubelessMocapModule::Open)));}
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
  FDetailsViewArgs Args;Args.bAllowSearch=true;auto& Properties=FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");auto Details=Properties.CreateDetailView(Args);Details->SetObject(Settings.Get());Details->SetEnabled(TAttribute<bool>::CreateLambda([](){return !UCubelessMocapLibrary::IsBusy();}));
  auto Window=SNew(SWindow).Title(FText::FromString("Cubeless Body Mocap")).ClientSize(FVector2D(1220,850));
  TSharedPtr<SCBMComparison> ComparisonWidget;
  Window->SetContent(SNew(SVerticalBox)
   +SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(.32f)[Details]
    +SHorizontalBox::Slot().FillWidth(.68f)[SAssignNew(ComparisonWidget,SCBMComparison).Settings(Settings.Get())]]
   +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(UCubelessMocapLibrary::GetHandReadiness(Settings.Get()));})]
   +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([](){return FText::FromString(UCubelessMocapLibrary::GetJobStatus());})]
   +SVerticalBox::Slot().AutoHeight().Padding(8,0)[SNew(SButton).Text(FText::FromString("Apply joint correction (optional postprocess)")).ToolTipText(FText::FromString("Enable Joint Correction under Postprocess first. Saves a separate finger-only result; Start Mocap stays unchanged.")).IsEnabled_Lambda([this](){return Settings->EnableJointCorrection&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){FString Result;if(!UCubelessMocapLibrary::CorrectFingerJoints(Settings.Get(),Result))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(Result));return FReply::Handled();})]
   +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SButton).Text(FText::FromString("Start Mocap")).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){FString Result;if(!UCubelessMocapLibrary::Extract(Settings.Get(),Result))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(Result));return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString("Run hand diagnostic")).Visibility_Lambda([this](){return Settings->EnableLegacyHandDiagnostics?EVisibility::Visible:EVisibility::Collapsed;}).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){FString Result;if(!UCubelessMocapLibrary::RefineHands(Settings.Get(),Result))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(Result));return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString("Apply cached fingers")).Visibility_Lambda([this](){return Settings->EnableLegacyHandDiagnostics?EVisibility::Visible:EVisibility::Collapsed;}).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){FString Result;if(!UCubelessMocapLibrary::ApplyHandResult(Settings.Get(),Result))FMessageDialog::Open(EAppMsgType::Ok,FText::FromString(Result));return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString("Cancel")).IsEnabled_Lambda([](){return UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([](){UCubelessMocapLibrary::Cancel();return FReply::Handled();})]]);
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
