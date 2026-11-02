#if WITH_DEV_AUTOMATION_TESTS
#include "RXLODSession.h"
#include "RXLODWindow.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "AssetRegistry/AssetData.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RHI.h"

namespace
{
    TSharedPtr<FJsonObject> CallRX(const FString& Request)
    {
        TSharedPtr<FJsonObject> Reply;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RXLOD::Execute(Request)), Reply);
        return Reply;
    }
    TSharedPtr<SRXLODWindow> FindRXPanel()
    {
        TArray<TSharedRef<SWindow>> Windows; FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
        for (const auto& Window : Windows)
            if (Window->GetTitle().ToString().StartsWith(TEXT("RX_SMLODGen ·")))
                if (Window->GetContent()->GetTypeAsString() == TEXT("SRXLODWindow")) { return StaticCastSharedRef<SRXLODWindow>(Window->GetContent()); }
        return nullptr;
    }
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRXCheckViewportStatistics, FAutomationTestBase*, Test);
bool FRXCheckViewportStatistics::Update()
{
    const auto Reply = CallRX(TEXT("{\"op\":\"get\"}"));
    if (Test->TestTrue(TEXT("Session is available"), Reply && Reply->GetBoolField(TEXT("success"))))
    {
        const auto Viewport = Reply->GetObjectField(TEXT("viewport"));
        Test->TestFalse(TEXT("Tool window does not cover other applications"), Reply->GetBoolField(TEXT("window_topmost")));
        Test->TestTrue(TEXT("Tool window is owned by the main editor"), Reply->GetBoolField(TEXT("window_owned_by_editor")));
        Test->TestEqual(TEXT("Statistics follow forced display LOD"), Viewport->GetIntegerField(TEXT("current_lod")), 1);
        Test->TestTrue(TEXT("Camera screen size is positive"), Viewport->GetNumberField(TEXT("current_screen_size")) > 0);
        const auto Metrics = Reply->GetArrayField(TEXT("preview_lods"))[1]->AsObject();
        Test->TestEqual(TEXT("Overlay triangles match built LOD"), Viewport->GetIntegerField(TEXT("triangles")), Metrics->GetIntegerField(TEXT("triangles")));
        Test->TestEqual(TEXT("Overlay vertices match built LOD"), Viewport->GetIntegerField(TEXT("vertices")), Metrics->GetIntegerField(TEXT("vertices")));
        uint32 SumTriangles = 0;
        for (const auto& Value : Viewport->GetArrayField(TEXT("sections")))
        {
            const auto Section = Value->AsObject(); SumTriangles += Section->GetIntegerField(TEXT("triangles"));
            Test->TestTrue(TEXT("Nonempty section has referenced vertices"), Section->GetNumberField(TEXT("vertices")) > 0);
        }
        Test->TestEqual(TEXT("Section triangle sum matches LOD"), SumTriangles, uint32(Metrics->GetIntegerField(TEXT("triangles"))));
        Test->TestTrue(TEXT("Overlay includes Korean current screen size label"), Viewport->GetStringField(TEXT("statistics_text")).Contains(TEXT("현재 스크린 크기")));
        CallRX(TEXT("{\"op\":\"snapshot\",\"filename\":\"asset_drop_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".png\"}"));
    }
    CallRX(TEXT("{\"op\":\"close\",\"confirm\":true}"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXDefaultAndDropTest, "RX.SMLODGen.DefaultAssetAndDrop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXDefaultAndDropTest::RunTest(const FString&)
{
    if (GUsingNullRHI) { AddError(TEXT("This UI test requires rendering.")); return false; }
    UStaticMesh* Default = LoadObject<UStaticMesh>(nullptr, RXLOD::DefaultMeshPath);
    if (!TestNotNull(TEXT("Bundled default box loads from plugin content"), Default)) { return false; }
    const auto Open = CallRX(TEXT("{\"op\":\"open\"}"));
    if (!TestTrue(TEXT("Open without selected asset"), Open && Open->GetBoolField(TEXT("success")))) { return false; }
    const auto Panel = FindRXPanel();
    if (!TestTrue(TEXT("Native RX panel found"), Panel.IsValid())) { CallRX(TEXT("{\"op\":\"close\",\"confirm\":true}")); return false; }
    const auto Initial = CallRX(TEXT("{\"op\":\"get\"}"))->GetObjectField(TEXT("settings"));
    TestEqual(TEXT("Default session uses bundled box"), Initial->GetStringField(TEXT("asset")), FString(RXLOD::DefaultMeshPath));
    TestTrue(TEXT("Invalid asset drag is consumed"), Panel->DropAssetsForTest({}));
    TestTrue(TEXT("Multiple meshes are rejected"), Panel->DropAssetsForTest({ FAssetData(Default), FAssetData(Default) }));
    TestEqual(TEXT("Invalid drop preserves session"), CallRX(TEXT("{\"op\":\"get\"}"))->GetObjectField(TEXT("settings"))->GetStringField(TEXT("asset")), FString(RXLOD::DefaultMeshPath));
    CallRX(TEXT("{\"op\":\"resize\",\"count\":3}"));
    const auto Old = CallRX(TEXT("{\"op\":\"get\"}"))->GetObjectField(TEXT("settings"));
    const FString Folder = FPaths::ProjectSavedDir() / TEXT("RX_SMLODGen/Presets");
    TArray<FString> Before; IFileManager::Get().FindFiles(Before, *(Folder / TEXT("autoswap_*.json")), true, false);
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/_MCP_Temp/RX_SMLODGen/CubelessTests/") + Unique + TEXT("/SM_DropCylinder")));
    UStaticMesh* Fixture = DuplicateObject<UStaticMesh>(Cylinder, Package, TEXT("SM_DropCylinder"));
    Fixture->SetFlags(RF_Public | RF_Standalone);
    TArray<UStaticMesh*> Pending = { Fixture }; FStaticMeshCompilingManager::Get().FinishCompilation(Pending); Package->SetDirtyFlag(false);
    TestTrue(TEXT("Static mesh drag/drop event is handled"), Panel->DropAssetsForTest({ FAssetData(Fixture) }));
    const auto Get = CallRX(TEXT("{\"op\":\"get\"}"));
    TestEqual(TEXT("Dropped mesh becomes editable original"), Get->GetObjectField(TEXT("settings"))->GetStringField(TEXT("asset")), Fixture->GetPathName());
    TestEqual(TEXT("Dropped source mesh is untouched"), Fixture->GetNumSourceModels(), 1);
    TestEqual(TEXT("Default source mesh is untouched"), Default->GetNumSourceModels(), 1);
    TestFalse(TEXT("Dropped source package stays clean"), Package->IsDirty());
    TArray<FString> After; IFileManager::Get().FindFiles(After, *(Folder / TEXT("autoswap_*.json")), true, false);
    bool FoundRecovery = false;
    for (const FString& Filename : After)
        if (!Before.Contains(Filename))
        {
            FString Text; FFileHelper::LoadFileToString(Text, *(Folder / Filename)); TSharedPtr<FJsonObject> Recovery;
            if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recovery))
            { FoundRecovery |= Recovery->GetStringField(TEXT("asset")) == Old->GetStringField(TEXT("asset")) && Recovery->GetArrayField(TEXT("lods")).Num() == 3; }
        }
    TestTrue(TEXT("Previous unsaved preview settings are recoverable"), FoundRecovery);
    CallRX(TEXT("{\"op\":\"resize\",\"count\":2}"));
    TestTrue(TEXT("Build dropped mesh LOD preview"), CallRX(TEXT("{\"op\":\"rebuild\"}"))->GetBoolField(TEXT("success")));
    CallRX(TEXT("{\"op\":\"view_lod\",\"lod\":1}"));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.7f));
    ADD_LATENT_AUTOMATION_COMMAND(FRXCheckViewportStatistics(this));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
    return true;
}
#endif
