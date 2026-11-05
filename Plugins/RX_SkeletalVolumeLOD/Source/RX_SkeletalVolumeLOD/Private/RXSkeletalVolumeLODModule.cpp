#include "ContentBrowserMenuContexts.h"
#include "Engine/SkeletalMesh.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "RXVolumeLOD.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "RXSkeletalVolumeLODModule"

namespace
{
void GenerateFromAsset(FAssetData Asset)
{
    USkeletalMesh* Source = Cast<USkeletalMesh>(Asset.GetAsset());
    if (!Source)
    {
        return;
    }

    IContentBrowserSingleton& Browser = IContentBrowserSingleton::Get();
    FSaveAssetDialogConfig Config;
    Config.DialogTitleOverride = LOCTEXT("Destination", "Choose a new asset for the volume-remeshed LOD1");
    Config.DefaultPath = Asset.PackagePath.ToString();
    Config.DefaultAssetName = Asset.AssetName.ToString() + TEXT("_VolumeLOD1");
    Config.AssetClassNames.Add(USkeletalMesh::StaticClass()->GetClassPathName());
    const FString ObjectPath = Browser.CreateModalSaveAssetDialog(Config);
    if (ObjectPath.IsEmpty())
    {
        return;
    }

    RXVolumeLOD::FResult Result;
    FText Error;
    if (!RXVolumeLOD::GenerateLOD1(Source, FPackageName::ObjectPathToPackageName(ObjectPath), {}, Result, Error))
    {
        FMessageDialog::Open(EAppMsgType::Ok, Error);
        return;
    }

    Browser.SyncBrowserToAssets(TArray<UObject*>{ Result.Mesh });
    FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("Completed",
        "Created an unsaved skeletal mesh copy. LOD1 has {0} triangles and {1}% matched texture texels. Review the output and save the new assets when ready."),
        FText::AsNumber(Result.TargetTriangles), FText::AsNumber(100.0 * Result.MatchedTexels / Result.CoveredTexels)));
}
}

class FRXSkeletalVolumeLODModule final : public IModuleInterface
{
public:
    void StartupModule() override
    {
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRXSkeletalVolumeLODModule::RegisterMenus));
    }

    void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }

private:
    void RegisterMenus()
    {
        FToolMenuOwnerScoped Owner(this);
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SkeletalMesh");
        FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
        Section.AddDynamicEntry("RXVolumeLOD1", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& DynamicSection)
        {
            const UContentBrowserAssetContextMenuContext* Context = DynamicSection.FindContext<UContentBrowserAssetContextMenuContext>();
            if (Context && Context->SelectedAssets.Num() == 1)
            {
                DynamicSection.AddMenuEntry("RXVolumeLOD1", LOCTEXT("MenuLabel", "Generate Volume LOD1 Copy"),
                    LOCTEXT("MenuTooltip", "Volume-remesh the source LOD0 and bake its materials into LOD1 on a new skeletal mesh copy."),
                    FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&GenerateFromAsset, Context->SelectedAssets[0])));
            }
        }));
    }
};

IMPLEMENT_MODULE(FRXSkeletalVolumeLODModule, RX_SkeletalVolumeLOD)

#undef LOCTEXT_NAMESPACE
