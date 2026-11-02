#if WITH_DEV_AUTOMATION_TESTS
#include "RXLODSession.h"
#include "Misc/AutomationTest.h"
#include "RHI.h"
#include "StaticMeshCompiler.h"
#include "RXSMLODGenLibrary.h"
#include "Tests/AutomationCommon.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "RXLODTooltips.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "StaticMeshResources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXSessionJsonTest, "RX.SMLODGen.SessionJSON", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXSessionJsonTest::RunTest(const FString& Parameters)
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Cube available"), Cube)) { return false; }
    TArray<UStaticMesh*> Pending = { Cube }; FStaticMeshCompilingManager::Get().FinishCompilation(Pending);
    const int32 OriginalLODs = Cube->GetNumSourceModels();
    auto Session = MakeShared<FRXLODSession>(); FString E;
    if (!TestTrue(TEXT("Create isolated session"), Session->Initialize(Cube, E))) { AddError(E); return false; }
    if (!TestTrue(TEXT("Add LOD"), Session->Resize(2, E))) { return false; }
    Session->Preview->GetSourceModel(1).ReductionSettings.PercentTriangles = 0.5f;
    auto& Rows = Session->GetWeights(1); for (auto& W : Rows) { W.Weight = 0; W.bExcluded = true; W.bPreserveSilhouette = true; }
    if (!TestTrue(TEXT("Weighted edit preview"), Session->Rebuild(E))) { AddError(E); return false; }
    const FString Json = Session->ExportJson();
    Rows[0].Weight = 0.75f; Rows[0].bExcluded = false;
    if (!TestTrue(TEXT("JSON import"), Session->ImportJson(Json, E))) { AddError(E); return false; }
    TestEqual(TEXT("Weight restored"), Session->Weights[1][0].Weight, 0.0f);
    TestTrue(TEXT("Exclusion restored"), Session->Weights[1][0].bExcluded);
    TestTrue(TEXT("Silhouette preservation restored from JSON"), Session->Weights[1][0].bPreserveSilhouette);
    TSharedPtr<FJsonObject> V2; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), V2);
    V2->SetNumberField(TEXT("version"), 2);
    for (const auto& LOD : V2->GetArrayField(TEXT("lods")))
        for (const auto& Section : LOD->AsObject()->GetArrayField(TEXT("sections"))) { Section->AsObject()->RemoveField(TEXT("preserve_silhouette")); }
    FString V2Json; FJsonSerializer::Serialize(V2.ToSharedRef(), TJsonWriterFactory<>::Create(&V2Json));
    TestTrue(TEXT("Old v2 JSON without silhouette flag imports"), Session->ImportJson(V2Json, E));
    TestFalse(TEXT("Old v2 defaults silhouette preservation off"), Session->Weights[1][0].bPreserveSilhouette);
    TestEqual(TEXT("Reduction restored"), Session->Preview->GetSourceModel(1).ReductionSettings.PercentTriangles, 0.5f);
    TSharedPtr<FJsonObject> Legacy;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Legacy);
    Legacy->SetNumberField(TEXT("version"), 1); Legacy->RemoveField(TEXT("weight_semantics"));
    Legacy->GetArrayField(TEXT("lods"))[1]->AsObject()->GetArrayField(TEXT("sections"))[0]->AsObject()->SetNumberField(TEXT("weight"), 0.2);
    FString LegacyJson; FJsonSerializer::Serialize(Legacy.ToSharedRef(), TJsonWriterFactory<>::Create(&LegacyJson));
    if (TestTrue(TEXT("Import v1 JSON settings"), Session->ImportJson(LegacyJson, E)))
    { TestEqual(TEXT("v1 reduction weight migrates to v2 preservation weight"), Session->Weights[1][0].Weight, 0.8f); }
    const FString Before = Session->ExportJson();
    TSharedPtr<FJsonObject> Invalid; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Before), Invalid);
    Invalid->GetArrayField(TEXT("lods"))[1]->AsObject()->GetArrayField(TEXT("sections"))[0]->AsObject()->SetStringField(TEXT("preserve_silhouette"), TEXT("true"));
    FString InvalidJson; FJsonSerializer::Serialize(Invalid.ToSharedRef(), TJsonWriterFactory<>::Create(&InvalidJson));
    TestFalse(TEXT("Non-Boolean silhouette flag is rejected"), Session->ImportJson(InvalidJson, E));
    TestEqual(TEXT("Bad flag leaves session unchanged"), Session->ExportJson(), Before);
    TestFalse(TEXT("Malformed settings rejected"), Session->ImportJson(TEXT("{}"), E));
    TestEqual(TEXT("Failure preserves session"), Session->ExportJson(), Before);
    TestEqual(TEXT("Original LOD count unchanged"), Cube->GetNumSourceModels(), OriginalLODs);
    TestFalse(TEXT("Original and preview are separate"), Session->Preview.Get() == Cube);
    const FString Describe = URXSMLODGenLibrary::ExecuteRequest(TEXT("{\"op\":\"describe\"}"));
    TestTrue(TEXT("AI interface is discoverable"), Describe.Contains(TEXT("save_json")) && Describe.Contains(TEXT("source_id")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXSessionImpostorTest, "RX.SMLODGen.SessionImpostorReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXSessionImpostorTest::RunTest(const FString& Parameters)
{
    if (GUsingNullRHI) { AddWarning(TEXT("Requires rendering.")); return true; }
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Session = MakeShared<FRXLODSession>(); FString E;
    if (!TestTrue(TEXT("Create isolated session"), Session->Initialize(Cube, E))) { AddError(E); return false; }
    const int32 EditableCount = Session->Preview->GetNumSourceModels();
    Session->bImpostor = true;
    if (!TestTrue(TEXT("First impostor"), Session->Rebuild(E))) { AddError(E); return false; }
    const int32 Materials = Session->Display->GetStaticMaterials().Num();
    TestEqual(TEXT("Impostor is actual display LOD"), Session->Display->GetNumSourceModels(), EditableCount + 1);
    TestEqual(TEXT("Impostor is last"), Session->ImpostorLOD, EditableCount);
    TestTrue(TEXT("Impostor description in edit preview"), Session->Display->IsMeshDescriptionValid(EditableCount));
    if (!TestTrue(TEXT("Replace impostor"), Session->Rebuild(E))) { AddError(E); return false; }
    TestEqual(TEXT("Regeneration does not append"), Session->Display->GetNumSourceModels(), EditableCount + 1);
    TestEqual(TEXT("Regeneration does not duplicate material slot"), Session->Display->GetStaticMaterials().Num(), Materials);
    TestEqual(TEXT("Logical settings mesh preserved"), Session->Preview->GetNumSourceModels(), EditableCount);
    Session->bImpostor = false;
    if (!TestTrue(TEXT("Disable impostor"), Session->Rebuild(E))) { AddError(E); return false; }
    TestEqual(TEXT("Only generated last LOD removed"), Session->Display->GetNumSourceModels(), EditableCount);
    return true;
}

struct FRXWindowTestState
{
    FAutomationTestBase* Test;
    FString Filename;
    double PreviousWorldTime = -1;
    double PreviousTicks = -1;
    TArray<TSharedPtr<FJsonValue>> Frames;
    TSharedPtr<FJsonObject> Call(const TSharedRef<FJsonObject>& Request)
    {
        FString Input; FJsonSerializer::Serialize(Request, TJsonWriterFactory<>::Create(&Input));
        const FString Result = URXSMLODGenLibrary::ExecuteRequest(Input); TSharedPtr<FJsonObject> Reply;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Result), Reply);
        if (!Reply || !Reply->GetBoolField(TEXT("success"))) { Test->AddError(Result); }
        return Reply;
    }
    TSharedPtr<FJsonObject> Op(const TCHAR* Name)
    { auto Request = MakeShared<FJsonObject>(); Request->SetStringField(TEXT("op"), Name); return Call(Request); }
};

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FRXWindowStableFrame, TSharedPtr<FRXWindowTestState>, State, int32, Index);
bool FRXWindowStableFrame::Update()
{
    const auto Result = State->Op(TEXT("get"));
    if (Result && Result->GetBoolField(TEXT("success")))
    {
        const auto Viewport = Result->GetObjectField(TEXT("viewport"));
        const double Time = Viewport->GetNumberField(TEXT("world_time"));
        const double Ticks = Viewport->GetNumberField(TEXT("world_ticks"));
        State->Test->TestTrue(TEXT("Preview world time advances between frames"), Time > State->PreviousWorldTime && Time > 0);
        State->Test->TestTrue(TEXT("Preview world tick count advances"), Ticks > State->PreviousTicks && Ticks > 0);
        State->Test->TestTrue(TEXT("Preview uses fixed exposure"), Viewport->GetBoolField(TEXT("fixed_exposure")));
        State->PreviousWorldTime = Time; State->PreviousTicks = Ticks;
        auto Shot = MakeShared<FJsonObject>(); Shot->SetStringField(TEXT("op"), TEXT("snapshot")); Shot->SetStringField(TEXT("filename"), State->Filename + FString::Printf(TEXT("_frame_%02d.png"), Index));
        const auto Image = State->Call(Shot);
        auto Frame = MakeShared<FJsonObject>(); Frame->SetObjectField(TEXT("viewport"), Viewport);
        if (Image && Image->HasField(TEXT("path"))) { Frame->SetStringField(TEXT("path"), Image->GetStringField(TEXT("path"))); }
        State->Frames.Add(MakeShared<FJsonValueObject>(Frame));
    }
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRXWindowInspect, TSharedPtr<FRXWindowTestState>, State);
bool FRXWindowInspect::Update()
{
    auto Resize = MakeShared<FJsonObject>(); Resize->SetStringField(TEXT("op"), TEXT("resize")); Resize->SetNumberField(TEXT("count"), 3); State->Call(Resize);
    Resize->SetNumberField(TEXT("count"), 2); State->Call(Resize);
    auto Get = State->Op(TEXT("get"));
    if (Get && Get->GetBoolField(TEXT("success")))
    {
        auto Settings = Get->GetObjectField(TEXT("settings"));
        auto LODs = Settings->GetArrayField(TEXT("lods"));
        auto LOD1 = MakeShared<FJsonObject>(*LODs[1]->AsObject());
        LOD1->SetNumberField(TEXT("index"), 1); LOD1->SetStringField(TEXT("geometry_id"), FString());
        auto Reduction = MakeShared<FJsonObject>(*LOD1->GetObjectField(TEXT("reduction"))); Reduction->SetNumberField(TEXT("percentTriangles"), 0.5); Reduction->SetNumberField(TEXT("baseLODModel"), 0);
        LOD1->SetObjectField(TEXT("reduction"), Reduction); LODs[1] = MakeShared<FJsonValueObject>(LOD1); Settings->SetArrayField(TEXT("lods"), LODs);
        auto Set = MakeShared<FJsonObject>(); Set->SetStringField(TEXT("op"), TEXT("set")); Set->SetObjectField(TEXT("settings"), Settings);
        State->Call(Set); State->Op(TEXT("rebuild"));
        auto View = MakeShared<FJsonObject>(); View->SetStringField(TEXT("op"), TEXT("view_lod")); View->SetNumberField(TEXT("lod"), 1); State->Call(View);
        const auto Forced = State->Op(TEXT("get"));
        State->Test->TestEqual(TEXT("Viewport LOD selector forces LOD 1"), Forced->GetObjectField(TEXT("viewport"))->GetIntegerField(TEXT("forced_lod")), 1);
        View->SetNumberField(TEXT("lod"), -1); State->Call(View);
        const auto Auto = State->Op(TEXT("get"));
        State->Test->TestEqual(TEXT("Viewport LOD selector restores Auto"), Auto->GetObjectField(TEXT("viewport"))->GetIntegerField(TEXT("forced_lod")), -1);
        auto Save = MakeShared<FJsonObject>(); Save->SetStringField(TEXT("op"), TEXT("save_json")); Save->SetStringField(TEXT("filename"), State->Filename + TEXT(".json")); State->Call(Save);
        auto Load = MakeShared<FJsonObject>(); Load->SetStringField(TEXT("op"), TEXT("load_json")); Load->SetStringField(TEXT("filename"), State->Filename + TEXT(".json")); State->Call(Load);
    }
    return true;
}
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRXWindowCapture, TSharedPtr<FRXWindowTestState>, State);
bool FRXWindowCapture::Update()
{
    auto Shot = MakeShared<FJsonObject>(); Shot->SetStringField(TEXT("op"), TEXT("snapshot")); Shot->SetStringField(TEXT("filename"), State->Filename + TEXT(".png")); State->Call(Shot);
    auto Close = MakeShared<FJsonObject>(); Close->SetStringField(TEXT("op"), TEXT("close")); Close->SetBoolField(TEXT("confirm"), true); State->Call(Close);
    auto Report = MakeShared<FJsonObject>(); Report->SetArrayField(TEXT("frames"), State->Frames);
    FString Text; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text));
    FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectSavedDir() / TEXT("RX_SMLODGen/Previews") / (State->Filename + TEXT("_frames.json"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXWindowAPITest, "RX.SMLODGen.WindowJSONAPI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXWindowAPITest::RunTest(const FString& Parameters)
{
    if (GUsingNullRHI) { AddWarning(TEXT("Window screenshot requires rendering.")); return true; }
    const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UPackage* Package = CreatePackage(*(TEXT("/Game/_MCP_Temp/RX_SMLODGen/CubelessTests/") + Unique + TEXT("/SM_Cube")));
    UStaticMesh* Fixture = DuplicateObject<UStaticMesh>(Cube, Package, TEXT("SM_Cube"));
    Fixture->SetFlags(RF_Public | RF_Standalone);
    TArray<UStaticMesh*> Pending = { Fixture }; FStaticMeshCompilingManager::Get().FinishCompilation(Pending); Package->SetDirtyFlag(false);
    auto State = MakeShared<FRXWindowTestState>(); State->Test = this; State->Filename = TEXT("window_") + Unique;
    auto Open = MakeShared<FJsonObject>(); Open->SetStringField(TEXT("op"), TEXT("open")); Open->SetStringField(TEXT("asset"), Fixture->GetPathName());
    auto Reply = State->Call(Open); if (!Reply || !Reply->GetBoolField(TEXT("success"))) { return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FRXWindowInspect(State));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    for (int32 Index = 0; Index < 8; ++Index)
    {
        ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.15f));
        ADD_LATENT_AUTOMATION_COMMAND(FRXWindowStableFrame(State, Index));
    }
    ADD_LATENT_AUTOMATION_COMMAND(FRXWindowCapture(State));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXKoreanTooltipTest, "RX.SMLODGen.KoreanTooltips", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXKoreanTooltipTest::RunTest(const FString& Parameters)
{
    for (UScriptStruct* Type : { FMeshBuildSettings::StaticStruct(), FMeshReductionSettings::StaticStruct(), FPerPlatformFloat::StaticStruct(), FRXLODScreenSizeSettings::StaticStruct(), FRXLODCommonSettings::StaticStruct() })
    {
        for (TFieldIterator<FProperty> It(Type); It; ++It)
        {
            if (!It->HasAnyPropertyFlags(CPF_Edit)) { continue; }
            TestTrue(FString::Printf(TEXT("Korean description for %s.%s"), *Type->GetName(), *It->GetName()), RXLODTooltips::HasProperty(Type->GetFName(), It->GetFName()));
            TestFalse(TEXT("Description is not empty"), RXLODTooltips::Property(Type->GetFName(), It->GetFName()).IsEmpty());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXScreenSizeTest, "RX.SMLODGen.ScreenSizeValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXScreenSizeTest::RunTest(const FString& Parameters)
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Session = MakeShared<FRXLODSession>(); FString E;
    if (!TestTrue(TEXT("Open isolated screen size session"), Session->Initialize(Cube, E)) || !Session->Resize(3, E)) { AddError(E); return false; }
    Session->Preview->SetAutoComputeLODScreenSize(true);
    for (int32 LOD = 0; LOD < 3; ++LOD) { Session->Preview->GetSourceModel(LOD).ScreenSize.Default = 0.25f; }
    if (!TestTrue(TEXT("Build auto screen sizes"), Session->Rebuild(E))) { AddError(E); return false; }
    FPerPlatformFloat Computed[3]; bool DifferentFromSource = false;
    for (int32 LOD = 0; LOD < 3; ++LOD)
    {
        Computed[LOD] = Session->Display->GetRenderData()->ScreenSize[LOD];
        TestEqual(TEXT("Auto UI uses actual render threshold"), Session->GetScreenSize(LOD).Default, Computed[LOD].Default);
        DifferentFromSource |= Session->GetScreenSize(LOD).Default != 0.25f;
    }
    TestTrue(TEXT("Auto display does not repeat authoring default 0.25"), DifferentFromSource);
    FPerPlatformFloat Edited(0.63f); Edited.PerPlatform.Add(TEXT("Windows"), 0.55f);
    if (!TestTrue(TEXT("Direct edit is accepted while Auto is enabled"), Session->SetScreenSize(1, Edited, E))) { AddError(E); return false; }
    TestFalse(TEXT("Direct edit disables Auto"), Session->Preview->GetAutoComputeLODScreenSize());
    TestEqual(TEXT("Edited LOD keeps entered value"), Session->GetScreenSize(1).Default, 0.63f);
    TestEqual(TEXT("Other LOD keeps computed threshold"), Session->GetScreenSize(2).Default, Computed[2].Default);
    if (!TestTrue(TEXT("Rebuild after direct screen edit"), Session->Rebuild(E))) { AddError(E); return false; }
    TestEqual(TEXT("Entered screen value survives rebuilding"), Session->GetScreenSize(1).Default, 0.63f);
    FPerPlatformFloat Invalid(-1.0f);
    TestFalse(TEXT("Invalid screen edit is rejected"), Session->SetScreenSize(1, Invalid, E));
    TestEqual(TEXT("Rejected edit preserves prior value"), Session->GetScreenSize(1).Default, 0.63f);
    const float Values[] = { 1.0f, 0.63f, 0.17f };
    for (int32 LOD = 0; LOD < 3; ++LOD)
    {
        auto& Screen = Session->Preview->GetSourceModel(LOD).ScreenSize; Screen.Default = Values[LOD]; Screen.PerPlatform.Add(TEXT("Windows"), Values[LOD] * 0.9f);
    }
    const FString Json = Session->ExportJson();
    if (!TestTrue(TEXT("Screen sizes survive JSON import"), Session->ImportJson(Json, E))) { AddError(E); return false; }
    for (int32 LOD = 0; LOD < 3; ++LOD)
    {
        TestEqual(TEXT("Each LOD keeps its own default"), Session->GetScreenSize(LOD).Default, Values[LOD]);
        TestEqual(TEXT("Platform override keeps its own value"), Session->GetScreenSize(LOD).PerPlatform.FindChecked(TEXT("Windows")), Values[LOD] * 0.9f);
    }
    return true;
}
#endif
