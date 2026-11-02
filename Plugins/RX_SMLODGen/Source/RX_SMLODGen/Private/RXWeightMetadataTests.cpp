#if WITH_DEV_AUTOMATION_TESTS
#include "RXWeightMetadata.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXWeightMetadataTest, "RX.SMLODGen.WeightMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXWeightMetadataTest::RunTest(const FString& Parameters)
{
    FRXStoredLODWeights LOD;
    LOD.Settings.PercentTriangles = 0.5f;
    LOD.Sections = { { FPolygonGroupID(2), 0.25f, false }, { FPolygonGroupID(9), 0.0f, true } };
    TMap<int32, FRXStoredLODWeights> Before;
    Before.Add(1, LOD);
    FString Json, Error, SourceId;
    if (!TestTrue(TEXT("Encode weight metadata"), RXWeightMetadata::Encode(TEXT("source-id"), Before, 2, 3, Json, Error)))
    { AddError(Error); return false; }
    TMap<int32, FRXStoredLODWeights> After;
    int32 ImpostorLOD = INDEX_NONE, MaterialSlot = INDEX_NONE;
    if (!TestTrue(TEXT("Decode weight metadata"), RXWeightMetadata::Decode(Json, SourceId, After, ImpostorLOD, MaterialSlot, Error)))
    { AddError(Error); return false; }
    TestEqual(TEXT("Source identity round trips"), SourceId, FString(TEXT("source-id")));
    TestEqual(TEXT("Reduction option round trips"), After[1].Settings.PercentTriangles, 0.5f);
    TestEqual(TEXT("Weight round trips"), After[1].Sections[0].Weight, 0.25f);
    TestTrue(TEXT("Excluded section round trips"), After[1].Sections[1].bExcluded);
    TestEqual(TEXT("Impostor LOD round trips"), ImpostorLOD, 2);
    TestEqual(TEXT("Impostor slot round trips"), MaterialSlot, 3);
    const FString Legacy = Json.Replace(TEXT("\"version\": 2"), TEXT("\"version\": 1")).Replace(TEXT("\"version\":2"), TEXT("\"version\":1"));
    if (TestTrue(TEXT("Legacy reduction-strength metadata imports"), RXWeightMetadata::Decode(Legacy, SourceId, After, ImpostorLOD, MaterialSlot, Error)))
    { TestEqual(TEXT("Legacy strength converts to preservation without changing reduction"), After[1].Sections[0].Weight, 0.75f); }
    TestFalse(TEXT("Invalid metadata rejected"), RXWeightMetadata::Decode(TEXT("{\"version\":99}"), SourceId, After, ImpostorLOD, MaterialSlot, Error));
    TestTrue(TEXT("Invalid metadata leaves previous output unchanged"), After.Contains(1));
    return true;
}
#endif
