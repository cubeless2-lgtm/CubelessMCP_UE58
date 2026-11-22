#include "KimodoSession.h"
#include "IPythonScriptPlugin.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "ToolMenus.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

class FKimodoAuthoringModule : public IModuleInterface
{
    TWeakPtr<SWindow> Window;
    IConsoleObject* OpenCommand=nullptr;
    void Open()
    {
        if(auto W=Window.Pin()){W->BringToFront();return;}
        UKimodoSession* S=UKimodoSession::GetSession();
        FDetailsViewArgs Args;
        Args.bAllowSearch=false;
        Args.bShowOptions=false;
        Args.NameAreaSettings=FDetailsViewArgs::HideNameArea;
        auto Details=FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(Args);
        Details->SetObject(S);
        auto Bar=SNew(SWrapBox).UseAllottedSize(true);
        auto Button=[Bar,Details](const TCHAR* Label,TFunction<void()> Fn){
            Bar->AddSlot().Padding(2)[SNew(SButton).Text(FText::FromString(Label)).OnClicked_Lambda([Fn,Details](){
                Fn(); Details->ForceRefresh(); return FReply::Handled();})];
        };
        auto Py=[](const TCHAR* Action){
            auto* P=IPythonScriptPlugin::Get();
            if(P) P->ExecPythonCommand(*FString::Printf(TEXT("import kimodo_native; kimodo_native.dispatch('%s')"),Action));
        };
        Button(TEXT("모션 생성"),[Py]{Py(TEXT("generate"));});
        Button(TEXT("원본 · 퀸 비교 재생"),[Py]{Py(TEXT("compare"));});
        Button(TEXT("결과 열기"),[S]{S->Preview(S->PreviewClip);});
        Button(TEXT("재생 / 일시정지"),[S]{S->TogglePlayback();});
        Button(TEXT("현재 프레임에 마커"),[S]{S->MarkCurrentFrame();});
        Button(TEXT("IK 적용"),[Py]{Py(TEXT("apply"));});
        Button(TEXT("JSON 저장"),[Py]{Py(TEXT("save"));});
        Button(TEXT("JSON 불러오기"),[Py]{Py(TEXT("load"));});
        auto W=SNew(SWindow).Title(FText::FromString(TEXT("Kimodo · 모션 편집"))).ClientSize(FVector2D(535,680))
          [SNew(SVerticalBox)
           +SVerticalBox::Slot().AutoHeight().Padding(4)[Bar]
           +SVerticalBox::Slot().AutoHeight().Padding(8,4)[SNew(STextBlock).Text_Lambda([S]{
               return FText::FromString(FString::Printf(TEXT("프레임 %d    %s"),S->CurrentFrame(),*S->Status));})]
           +SVerticalBox::Slot().FillHeight(1)[Details]];
        Window=W;
        FSlateApplication::Get().AddWindow(W);
    }
public:
    void StartupModule() override
    {
        const FString Work=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Kimodo/EditorCache/"));
        IFileManager::Get().MakeDirectory(*Work,true);
        FPackageName::RegisterMountPoint(TEXT("/KimodoWork/"),Work);
        OpenCommand=IConsoleManager::Get().RegisterConsoleCommand(TEXT("Kimodo.Open"),TEXT("Open Kimodo motion editor"),
            FConsoleCommandDelegate::CreateRaw(this,&FKimodoAuthoringModule::Open),ECVF_Default);
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]{
            FToolMenuOwnerScoped Owner(this);
            UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window")->FindOrAddSection("WindowLayout")
                .AddMenuEntry("KimodoAuthoring",FText::FromString(TEXT("Kimodo · 모션 편집")),FText(),FSlateIcon(),
                    FUIAction(FExecuteAction::CreateRaw(this,&FKimodoAuthoringModule::Open)));
        }));
    }
    void ShutdownModule() override
    {
        FPackageName::UnRegisterMountPoint(TEXT("/KimodoWork/"),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Kimodo/EditorCache/")));
        if(auto W=Window.Pin()) W->RequestDestroyWindow();
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        if(OpenCommand) IConsoleManager::Get().UnregisterConsoleObject(OpenCommand);
    }
};
IMPLEMENT_MODULE(FKimodoAuthoringModule,KimodoAuthoring)

