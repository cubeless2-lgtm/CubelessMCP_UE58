#if WITH_DEV_AUTOMATION_TESTS

#include "RXWeightedPreview.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshResources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXWeightedPreviewTest, "RX.SMLODGen.WeightedPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXWeightedPreviewTest::RunTest(const FString& Parameters)
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Engine cube exists"), Cube) || Cube->IsCompiling())
    {
        return false;
    }
    TStrongObjectPtr<UStaticMesh> Source(DuplicateObject<UStaticMesh>(Cube, GetTransientPackage()));
    Source->SetNumSourceModels(2);
    Source->GetSourceModel(1).ReductionSettings.PercentTriangles = 0.5f;
    Source->GetSourceModel(1).ReductionSettings.BaseLODModel = 0;
    const FMeshDescription* Base = Source->GetMeshDescription(0);
    if (!TestNotNull(TEXT("Cube LOD0 source description exists"), Base))
    {
        return false;
    }
    TArray<FRXSectionWeight> Weights;
    for (FPolygonGroupID Group : Base->PolygonGroups().GetElementIDs())
    {
        Weights.Add({ Group, 1.0f, false });
    }
    TStrongObjectPtr<UStaticMesh> Display;
    TArray<FRXSectionReductionResult> Results;
    FString Error;
    if (!TestTrue(TEXT("Build weighted display mesh"), RXWeightedPreview::Build(Source.Get(), 1, Weights, Display, Results, Error)))
    {
        AddError(Error);
        return false;
    }
    TestTrue(TEXT("Display mesh is a separate transient object"), Display.IsValid() && Display.Get() != Source.Get() && Display->HasAnyFlags(RF_Transient));
    TestEqual(TEXT("Original reduction setting remains intact"), Source->GetSourceModel(1).ReductionSettings.PercentTriangles, 0.5f);
    TestTrue(TEXT("Display contains built target LOD"), Display->GetRenderData() && Display->GetRenderData()->LODResources.Num() > 1);
    for (const FRXSectionReductionResult& Result : Results)
    {
        TestEqual(TEXT("Zero weight preserves triangles"), Result.OutputTriangles, Result.InputTriangles);
    }
    return true;
}

#endif
