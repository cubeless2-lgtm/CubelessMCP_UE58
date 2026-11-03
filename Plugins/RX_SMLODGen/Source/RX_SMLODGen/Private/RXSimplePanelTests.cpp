#if WITH_DEV_AUTOMATION_TESTS
#include "RXLODWindow.h"
#include "RXLODSession.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshCompiler.h"
#include "RHI.h"

namespace
{
    TSharedPtr<FJsonObject> SimpleCall(const FString& Json)
    {
        TSharedPtr<FJsonObject> Result;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RXLOD::Execute(Json)), Result);
        return Result;
    }
    TSharedPtr<SRXLODWindow> SimpleTestPanel()
    {
        TArray<TSharedRef<SWindow>> Windows; FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
        for (const auto& Window : Windows)
            if (Window->GetTitle().ToString().StartsWith(TEXT("RX_SMLODGen ·")) && Window->GetContent()->GetTypeAsString() == TEXT("SRXLODWindow"))
            { return StaticCastSharedRef<SRXLODWindow>(Window->GetContent()); }
        return nullptr;
    }
    FString SimpleEncode(const TSharedRef<FJsonObject>& Json)
    { FString Result; FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Result)); return Result; }
    void SimpleLabels(SWidget& Widget, TSet<FString>& Labels)
    {
        if (Widget.GetTypeAsString() == TEXT("STextBlock")) { Labels.Add(static_cast<const STextBlock&>(Widget).GetText().ToString()); }
        FChildren* Children = Widget.GetChildren();
        for (int32 Index = 0; Children && Index < Children->Num(); ++Index) { SimpleLabels(*Children->GetChildAt(Index), Labels); }
    }
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRXSimplePanelCapture, FString, Filename);
bool FRXSimplePanelCapture::Update()
{
    SimpleCall(TEXT("{\"op\":\"snapshot\",\"filename\":\"") + Filename + TEXT(".png\"}"));
    SimpleCall(TEXT("{\"op\":\"close\",\"confirm\":true}"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXSimplePanelTest, "RX.SMLODGen.SimpleAdvancedPanels", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXSimplePanelTest::RunTest(const FString&)
{
    if (GUsingNullRHI) { AddError(TEXT("Simple/advanced UI test requires rendering.")); return false; }
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/_MCP_Temp/RX_SMLODGen/CubelessTests/") + Id + TEXT("/SM_Panel")));
    UStaticMesh* Mesh = DuplicateObject<UStaticMesh>(Cube, Package, TEXT("SM_Panel"));
    Mesh->SetFlags(RF_Public | RF_Standalone);
    TArray<UStaticMesh*> Pending = { Mesh }; FStaticMeshCompilingManager::Get().FinishCompilation(Pending); Package->SetDirtyFlag(false);
    auto Open = MakeShared<FJsonObject>(); Open->SetStringField(TEXT("op"), TEXT("open")); Open->SetStringField(TEXT("asset"), Mesh->GetPathName());
    if (!TestTrue(TEXT("Open panel fixture"), SimpleCall(SimpleEncode(Open))->GetBoolField(TEXT("success")))) { return false; }
    const auto Panel = SimpleTestPanel();
    if (!TestTrue(TEXT("Find RX panel"), Panel.IsValid())) { SimpleCall(TEXT("{\"op\":\"close\",\"confirm\":true}")); return false; }
    SimpleCall(TEXT("{\"op\":\"resize\",\"count\":3}"));
    SimpleCall(TEXT("{\"op\":\"rebuild\"}"));
    TestTrue(TEXT("Simple tab opens by default"), Panel->IsSimpleMode());
    TSet<FString> Labels; SimpleLabels(*Panel, Labels);
    for (const FString Label : { TEXT("LOD 개수 (임포스터 제외)"), TEXT("리듀스율 (%)"), TEXT("스크린 사이즈"), TEXT("머티리얼 슬롯"), TEXT("열린 외곽 경계 보존"), TEXT("충돌 활성화") })
    { TestTrue(TEXT("Simple panel includes ") + Label, Labels.Contains(Label)); }
    TestFalse(TEXT("Simple tab omits advanced group setting"), Labels.Contains(TEXT("LOD 그룹")));
    TestTrue(TEXT("Simple tab includes last impostor controls"), Labels.Contains(TEXT("마지막 LOD에 임포스터")) && Labels.Contains(TEXT("임포스터 방향")));
    auto Get = SimpleCall(TEXT("{\"op\":\"get\"}")); const FString Before = SimpleEncode(Get->GetObjectField(TEXT("settings")).ToSharedRef());
    TestTrue(TEXT("Switch to advanced"), SimpleCall(TEXT("{\"op\":\"panel_mode\",\"mode\":\"advanced\"}"))->GetBoolField(TEXT("success")));
    Labels.Reset(); SimpleLabels(*Panel, Labels); TestTrue(TEXT("Advanced options remain available"), Labels.Contains(TEXT("LOD 그룹")));
    TestEqual(TEXT("Tab switch does not change authoring settings"), SimpleEncode(SimpleCall(TEXT("{\"op\":\"get\"}"))->GetObjectField(TEXT("settings")).ToSharedRef()), Before);
    SimpleCall(TEXT("{\"op\":\"panel_mode\",\"mode\":\"simple\"}"));
    TestTrue(TEXT("Simple triangle reduction edit"), Panel->SetSimpleReduction(1, 60));
    auto Settings = SimpleCall(TEXT("{\"op\":\"get\"}"))->GetObjectField(TEXT("settings"));
    auto LOD = Settings->GetArrayField(TEXT("lods"))[1]->AsObject();
    TestTrue(TEXT("60 percent removed shares advanced 40 percent retained"), FMath::IsNearlyEqual(LOD->GetObjectField(TEXT("reduction"))->GetNumberField(TEXT("percentTriangles")), 0.4, 1e-6));
    auto Platforms = MakeShared<FJsonObject>(); Platforms->SetNumberField(TEXT("Windows"), 0.37);
    LOD->GetObjectField(TEXT("screen_size"))->SetObjectField(TEXT("perPlatform"), Platforms);
    auto Set = MakeShared<FJsonObject>(); Set->SetStringField(TEXT("op"), TEXT("set")); Set->SetObjectField(TEXT("settings"), Settings);
    TestTrue(TEXT("Advanced JSON change shares same session"), SimpleCall(SimpleEncode(Set))->GetBoolField(TEXT("success")));
    TestTrue(TEXT("Simple screen-size edit"), Panel->SetSimpleScreenSize(1, 0.63f));
    Get = SimpleCall(TEXT("{\"op\":\"get\"}")); Settings = Get->GetObjectField(TEXT("settings")); LOD = Settings->GetArrayField(TEXT("lods"))[1]->AsObject();
    TestFalse(TEXT("Screen edit disables auto computation"), Settings->GetBoolField(TEXT("auto_screen_size")));
    TestTrue(TEXT("Simple screen value reaches shared settings"), FMath::IsNearlyEqual(LOD->GetObjectField(TEXT("screen_size"))->GetNumberField(TEXT("default")), 0.63, 1e-6));
    TestTrue(TEXT("Hidden platform override survives simple edit"), FMath::IsNearlyEqual(LOD->GetObjectField(TEXT("screen_size"))->GetObjectField(TEXT("perPlatform"))->GetNumberField(TEXT("Windows")), 0.37, 1e-6));
    LOD->GetObjectField(TEXT("reduction"))->SetStringField(TEXT("terminationCriterion"), TEXT("Any"));
    Set->SetObjectField(TEXT("settings"), Settings);
    TestTrue(TEXT("Keep combined advanced reduction criterion"), SimpleCall(SimpleEncode(Set))->GetBoolField(TEXT("success")));
    Labels.Reset(); SimpleLabels(*Panel, Labels);
    TestTrue(TEXT("Combined criterion exposes both simple rates"), Labels.Contains(TEXT("리듀스율 (삼각형, %)")) && Labels.Contains(TEXT("리듀스율 (버텍스, %)")));
    TestTrue(TEXT("Simple vertex reduction edit"), Panel->SetSimpleReduction(1, 30, true));
    auto Combined = SimpleCall(TEXT("{\"op\":\"get\"}"))->GetObjectField(TEXT("settings"))->GetArrayField(TEXT("lods"))[1]->AsObject()->GetObjectField(TEXT("reduction"));
    TestTrue(TEXT("Vertex rate shares the existing vertex setting"), FMath::IsNearlyEqual(Combined->GetNumberField(TEXT("percentVertices")), 0.7, 1e-6));
    TestTrue(TEXT("Vertex edit keeps the triangle rate"), FMath::IsNearlyEqual(Combined->GetNumberField(TEXT("percentTriangles")), 0.4, 1e-6));
    TestFalse(TEXT("Reject invalid simple reduction"), Panel->SetSimpleReduction(1, -1));
    TestFalse(TEXT("Reject invalid simple screen size"), Panel->SetSimpleScreenSize(1, -1));
    TestFalse(TEXT("Reject unknown tab name"), SimpleCall(TEXT("{\"op\":\"panel_mode\",\"mode\":\"other\"}"))->GetBoolField(TEXT("success")));
    Panel->SetSimpleScreenSize(1, 0.63f);
    Panel->SetSimpleMode(false); Panel->SetSimpleMode(true);
    TestTrue(TEXT("Simple edits survive tab round trip"), Panel->IsSimpleMode());
    TestEqual(TEXT("Source asset LOD count is unchanged"), Mesh->GetNumSourceModels(), 1);
    TestFalse(TEXT("Source package stays clean"), Package->IsDirty());
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FRXSimplePanelCapture(TEXT("simple_panel_") + Id));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
    return true;
}
#endif
