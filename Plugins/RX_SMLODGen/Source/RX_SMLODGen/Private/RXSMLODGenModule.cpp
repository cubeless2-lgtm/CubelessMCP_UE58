#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "RXLODSession.h"
#include "RXLODWindow.h"
#include "RXSMLODGenLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/SWindow.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshResources.h"
#include "ImageUtils.h"
#include "FbxMeshUtils.h"
#include "Misc/PackageName.h"

namespace
{
    TSharedPtr<FRXLODSession> ActiveSession;
    TWeakPtr<SWindow> ActiveWindow;
    TWeakPtr<SRXLODWindow> ActivePanel;
    FString Encode(const TSharedRef<FJsonObject>& Object) { FString Text; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text)); return Text; }
}

bool RXLOD::Open(UStaticMesh* Mesh, FString& Error)
{
    if (ActiveSession.IsValid())
    {
        if (Mesh && Mesh != ActiveSession->Original.Get() && !LoadAsset(Mesh, Error)) { return false; }
        if (auto Window = ActiveWindow.Pin()) { Window->BringToFront(); }
        return true;
    }
    if (!Mesh) { Mesh = LoadObject<UStaticMesh>(nullptr, DefaultMeshPath); }
    if (!Mesh) { Error = TEXT("기본 박스 에셋을 찾지 못했습니다: ") + FString(DefaultMeshPath); return false; }
    auto Session = MakeShared<FRXLODSession>();
    if (!Session->Initialize(Mesh, Error)) { return false; }
    Session->Status += TEXT(" 콘텐츠 브라우저의 스태틱 메시를 뷰포트에 드롭하면 편집 대상을 바꿀 수 있습니다.");
    // Native ownership keeps the tool above UE without becoming system-topmost.
    auto Window = SNew(SWindow).Title(FText::FromString(TEXT("RX_SMLODGen · ") + Mesh->GetName())).ClientSize(FVector2D(1400, 850));
    auto Panel = SNew(SRXLODWindow).Session(Session);
    Window->SetContent(Panel);
    Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([](const TSharedRef<SWindow>&){ ActiveSession.Reset(); ActiveWindow.Reset(); ActivePanel.Reset(); }));
    ActiveSession = Session; ActiveWindow = Window; ActivePanel = Panel;
    if (const auto Parent = FGlobalTabmanager::Get()->GetRootWindow())
    { FSlateApplication::Get().AddWindowAsNativeChild(Window, Parent.ToSharedRef()); }
    else { FSlateApplication::Get().AddWindow(Window); }
    return true;
}

bool RXLOD::LoadAsset(UStaticMesh* Mesh, FString& Error)
{
    if (!Mesh) { Error = TEXT("콘텐츠 브라우저에서 스태틱 메시 하나를 뷰포트에 드롭하세요."); return false; }
    if (!ActiveSession.IsValid()) { return Open(Mesh, Error); }
    if (Mesh == ActiveSession->Original.Get()) { return true; }
    // Prepare the replacement first: a failed initialization never loses edits.
    auto Replacement = MakeShared<FRXLODSession>();
    if (!Replacement->Initialize(Mesh, Error)) { return false; }
    auto Panel = ActivePanel.Pin(); auto Window = ActiveWindow.Pin();
    if (!Panel || !Window) { Error = TEXT("RX LOD 창이 닫혀 있습니다."); return false; }
    const FString Folder = FPaths::ProjectSavedDir() / TEXT("RX_SMLODGen/Presets");
    const FString Recovery = Folder / (TEXT("autoswap_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json"));
    IFileManager::Get().MakeDirectory(*Folder, true);
    if (!FFileHelper::SaveStringToFile(ActiveSession->ExportJson(), *Recovery, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { Error = TEXT("기존 미리보기의 복구 JSON 저장에 실패하여 메시 교체를 중단했습니다."); return false; }
    ActiveSession = Replacement; Panel->SetSession(Replacement);
    Window->SetTitle(FText::FromString(TEXT("RX_SMLODGen · ") + Mesh->GetName()));
    ActiveSession->Status = TEXT("편집 대상: ") + Mesh->GetPathName() + TEXT(" · 이전 설정: ") + FPaths::GetCleanFilename(Recovery);
    return true;
}

FString RXLOD::Execute(const FString& Request)
{
    auto Reply = MakeShared<FJsonObject>(); Reply->SetBoolField(TEXT("success"), false); Reply->SetNumberField(TEXT("api_version"), 1);
    FString Error, Op; TSharedPtr<FJsonObject> Root;
    if (!IsInGameThread() || Request.Len() > 4 * 1024 * 1024 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Request), Root) || !Root || !Root->TryGetStringField(TEXT("op"), Op))
    { Reply->SetStringField(TEXT("error"), TEXT("Invalid JSON request or not on editor game thread.")); return Encode(Reply); }
    bool OK = false;
    if (Op == TEXT("describe"))
    {
        OK = true; Reply->SetStringField(TEXT("schema"), TEXT("RX_SMLODGen"));
        Reply->SetStringField(TEXT("ops"), TEXT("describe,open,load_asset,get,set,resize,remove_lod,import_lod,view_lod,rebuild,apply,save_json,load_json,snapshot,close"));
        Reply->SetStringField(TEXT("instructions"), TEXT("open: optional asset object path, omitted uses bundled box; load_asset: asset object path, archives prior settings as autoswap_GUID.json before replacement; get: complete settings and live viewport metrics; set: settings=complete get result, validates asset and source_id, edits preview only; rebuild: builds preview; apply: confirm=true, mutates original but does not save; save_json/load_json: filename=leaf.json in Saved/RX_SMLODGen/Presets; close: confirm=true; failures never auto-retry."));
        Reply->SetNumberField(TEXT("settings_version"), 4);
        Reply->SetStringField(TEXT("default_asset"), DefaultMeshPath);
        Reply->SetStringField(TEXT("weight_formula"), TEXT("keep = reduction.percentTriangles + (1 - reduction.percentTriangles) * weight; weight 0 = normal reduction, weight 1 = preserve; excluded -> keep 1; v1 imports invert weight; triangle percentage reduction, BaseLOD 0; unequal touching sections currently rejected."));
        Reply->SetBoolField(TEXT("session_open"), ActiveSession.IsValid());
    }
    else if (Op == TEXT("open") || Op == TEXT("load_asset"))
    {
        FString Asset;
        const bool HasAsset = Root->TryGetStringField(TEXT("asset"), Asset);
        if (!HasAsset && !Root->HasField(TEXT("asset")) && Op == TEXT("open")) { OK = Open(nullptr, Error); }
        else if (!HasAsset || !FPackageName::IsValidObjectPath(Asset)) { Error = TEXT("asset must be a Static Mesh object path."); }
        else if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Asset)) { OK = Op == TEXT("open") ? Open(Mesh, Error) : LoadAsset(Mesh, Error); }
        else { Error = TEXT("Static Mesh asset not found: ") + Asset; }
    }
    else if (!ActiveSession.IsValid()) { Error = TEXT("No session. Call open first."); }
    else if (Op == TEXT("get"))
    {
        TSharedPtr<FJsonObject> Settings; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ActiveSession->ExportJson()), Settings);
        Reply->SetObjectField(TEXT("settings"), Settings); OK = true;
        Reply->SetBoolField(TEXT("preview_current"), ActiveSession->IsPreviewCurrent());
        if (auto Panel = ActivePanel.Pin()) { Reply->SetObjectField(TEXT("viewport"), Panel->ViewportDiagnostics()); }
        if (auto Window = ActiveWindow.Pin())
        {
            Reply->SetBoolField(TEXT("window_topmost"), Window->IsTopmostWindow());
            Reply->SetBoolField(TEXT("window_owned_by_editor"), Window->GetParentWindow().IsValid());
        }
        TArray<TSharedPtr<FJsonValue>> Metrics;
        if (const auto* Data = ActiveSession->Display->GetRenderData())
            for (int32 LOD = 0; LOD < Data->LODResources.Num(); ++LOD)
            { auto Metric = MakeShared<FJsonObject>(); Metric->SetNumberField(TEXT("lod"), LOD); Metric->SetNumberField(TEXT("triangles"), Data->LODResources[LOD].GetNumTriangles()); Metric->SetNumberField(TEXT("vertices"), Data->LODResources[LOD].GetNumVertices()); Metric->SetBoolField(TEXT("impostor"), LOD == ActiveSession->ImpostorLOD); Metric->SetNumberField(TEXT("screen_size"), ActiveSession->GetScreenSize(LOD).Default); Metrics.Add(MakeShared<FJsonValueObject>(Metric)); }
        Reply->SetArrayField(TEXT("preview_lods"), Metrics);
    }
    else if (Op == TEXT("set"))
    {
        const TSharedPtr<FJsonObject>* Settings = nullptr;
        if (!Root->TryGetObjectField(TEXT("settings"), Settings)) { Error = TEXT("settings object required."); }
        else { OK = ActiveSession->ImportJson(Encode(Settings->ToSharedRef()), Error); if (OK && ActivePanel.IsValid()) { ActivePanel.Pin()->RefreshPanel(); } }
    }
    else if (Op == TEXT("resize") || Op == TEXT("remove_lod"))
    {
        int32 Index = -1;
        if (!Root->TryGetNumberField(Op == TEXT("resize") ? TEXT("count") : TEXT("lod"), Index)) { Error = TEXT("Integer count or lod required."); }
        else { OK = Op == TEXT("resize") ? ActiveSession->Resize(Index, Error) : ActiveSession->Remove(Index, Error); if (OK && ActivePanel.IsValid()) { ActivePanel.Pin()->RefreshPanel(); } }
    }
    else if (Op == TEXT("import_lod"))
    {
        int32 LOD = -1; FString Filename;
        if (!Root->TryGetNumberField(TEXT("lod"), LOD) || LOD < 1 || LOD > ActiveSession->Preview->GetNumSourceModels() || LOD >= MAX_STATIC_MESH_LODS - (ActiveSession->bImpostor ? 1 : 0) ||
            !Root->TryGetStringField(TEXT("path"), Filename) || FPaths::IsRelative(Filename) || !IFileManager::Get().FileExists(*Filename) || !(FPaths::GetExtension(Filename).Equals(TEXT("fbx"), ESearchCase::IgnoreCase) || FPaths::GetExtension(Filename).Equals(TEXT("obj"), ESearchCase::IgnoreCase)))
        { Error = TEXT("An existing absolute FBX/OBJ path and valid nonzero lod are required."); }
        else { OK = FbxMeshUtils::ImportStaticMeshLOD(ActiveSession->Preview.Get(), Filename, LOD, false); if (OK) { ActiveSession->Weights.Remove(LOD); if (ActivePanel.IsValid()) { ActivePanel.Pin()->RefreshPanel(); } } }
    }
    else if (Op == TEXT("view_lod"))
    {
        int32 LOD = -2;
        if (!Root->TryGetNumberField(TEXT("lod"), LOD) || LOD < -1 || LOD >= ActiveSession->Display->GetNumSourceModels()) { Error = TEXT("lod must be -1 (Auto) or an existing display LOD index."); }
        else if (auto Panel = ActivePanel.Pin()) { Panel->SelectLOD(LOD); OK = true; }
    }
    else if (Op == TEXT("rebuild")) { OK = ActivePanel.IsValid() && ActivePanel.Pin()->RefreshPreview(Error); }
    else if (Op == TEXT("apply"))
    {
        bool Confirm = false;
        if (!Root->TryGetBoolField(TEXT("confirm"), Confirm) || !Confirm) { Error = TEXT("Original mutation requires confirm=true. Preview edits are not applied implicitly."); }
        else { OK = ActiveSession->Apply(Error); }
    }
    else if (Op == TEXT("close"))
    {
        bool Confirm = false;
        if (!Root->TryGetBoolField(TEXT("confirm"), Confirm) || !Confirm) { Error = TEXT("Discarding preview requires confirm=true. Export JSON first to preserve settings."); }
        else if (auto Window = ActiveWindow.Pin()) { Window->RequestDestroyWindow(); OK = true; }
    }
    else if (Op == TEXT("snapshot"))
    {
        FString Filename;
        if (!Root->TryGetStringField(TEXT("filename"), Filename) || Filename != FPaths::GetCleanFilename(Filename) || !Filename.EndsWith(TEXT(".png")) || Filename.Contains(TEXT("..")) || Filename.Contains(TEXT(":"))) { Error = TEXT("filename must be a plain .png filename."); }
        else if (auto Window = ActiveWindow.Pin())
        {
            const FString Folder = FPaths::ProjectSavedDir() / TEXT("RX_SMLODGen/Previews"); const FString Path = Folder / Filename;
            if (IFileManager::Get().FileExists(*Path)) { Error = TEXT("Snapshot file already exists."); }
            else
            {
                TArray<FColor> Pixels; FIntVector Size;
                if (FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Pixels, Size))
                {
                    TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
                    IFileManager::Get().MakeDirectory(*Folder, true); OK = FFileHelper::SaveArrayToFile(PNG, *Path);
                    Reply->SetStringField(TEXT("path"), FPaths::ConvertRelativePathToFull(Path));
                }
                else { Error = TEXT("Slate screenshot failed. Wait until the window has rendered."); }
            }
        }
    }
    else if (Op == TEXT("save_json") || Op == TEXT("load_json"))
    {
        FString Filename;
        if (!Root->TryGetStringField(TEXT("filename"), Filename) || Filename != FPaths::GetCleanFilename(Filename) || !Filename.EndsWith(TEXT(".json")) || Filename.Contains(TEXT("..")) || Filename.Contains(TEXT(":"))) { Error = TEXT("filename must be a plain .json filename."); }
        else
        {
            const FString Folder = FPaths::ProjectSavedDir() / TEXT("RX_SMLODGen/Presets"); const FString Path = Folder / Filename;
            if (Op == TEXT("save_json"))
            {
                bool Overwrite = false; Root->TryGetBoolField(TEXT("overwrite"), Overwrite);
                if (IFileManager::Get().FileExists(*Path) && !Overwrite) { Error = TEXT("File exists. Specify overwrite=true."); }
                else { IFileManager::Get().MakeDirectory(*Folder, true); OK = FFileHelper::SaveStringToFile(ActiveSession->ExportJson(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM); }
            }
            else { FString Text; OK = FFileHelper::LoadFileToString(Text, *Path) && ActiveSession->ImportJson(Text, Error); if (OK && ActivePanel.IsValid()) { ActivePanel.Pin()->RefreshPanel(); } }
            Reply->SetStringField(TEXT("path"), FPaths::ConvertRelativePathToFull(Path));
        }
    }
    else { Error = TEXT("Unknown operation. Call describe."); }
    Reply->SetBoolField(TEXT("success"), OK); Reply->SetStringField(TEXT("op"), Op);
    if (!Error.IsEmpty()) { Reply->SetStringField(OK ? TEXT("message") : TEXT("error"), Error); }
    if (ActiveSession.IsValid()) { Reply->SetStringField(TEXT("status"), ActiveSession->Status); }
    return Encode(Reply);
}

FString URXSMLODGenLibrary::ExecuteRequest(const FString& RequestJson) { return RXLOD::Execute(RequestJson); }

class FRXSMLODGenModule final : public IModuleInterface
{
public:
    bool SupportsDynamicReloading() override { return false; }
    void StartupModule() override { UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRXSMLODGenModule::RegisterMenus)); }
    void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this); UToolMenus::UnregisterOwner(this);
        if (auto Window = ActiveWindow.Pin()) { Window->SetOnWindowClosed(FOnWindowClosed()); Window->RequestDestroyWindow(); }
        ActivePanel.Reset(); ActiveWindow.Reset(); ActiveSession.Reset();
    }
private:
    void RegisterMenus()
    {
        FToolMenuOwnerScoped Owner(this);
        UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window")->FindOrAddSection("WindowLayout").AddMenuEntry("RXSMLODGenOpenPreview",
            NSLOCTEXT("RXSMLODGen", "OpenRXWindow", "RX_SMLODGen · LOD 편집"), NSLOCTEXT("RXSMLODGen", "OpenRXWindowTip", "선택한 메시를 편집합니다. 선택한 메시가 없으면 기본 박스로 열립니다. 콘텐츠 브라우저의 스태틱 메시를 뷰포트에 드롭해 편집 대상을 바꿀 수 있습니다."),
            FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FRXSMLODGenModule::OpenSelection)));
    }
    void OpenSelection()
    {
        TArray<FAssetData> Assets; FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Assets);
        UStaticMesh* Mesh = Assets.Num() == 1 ? Cast<UStaticMesh>(Assets[0].GetAsset()) : nullptr;
        if (!Mesh && GEditor && GEditor->GetSelectedActors() && GEditor->GetSelectedActors()->Num() == 1)
        {
            if (AActor* Actor = Cast<AActor>(GEditor->GetSelectedActors()->GetSelectedObject(0)))
            {
                TInlineComponentArray<UStaticMeshComponent*> Components; Actor->GetComponents(Components); TSet<UStaticMesh*> Meshes;
                for (auto* Component : Components) { if (Component->GetStaticMesh()) { Meshes.Add(Component->GetStaticMesh()); } }
                if (Meshes.Num() == 1) { Mesh = *Meshes.CreateConstIterator(); }
            }
        }
        FString Error;
        if (RXLOD::Open(Mesh, Error)) { return; }
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
    }
};
IMPLEMENT_MODULE(FRXSMLODGenModule, RX_SMLODGen)
