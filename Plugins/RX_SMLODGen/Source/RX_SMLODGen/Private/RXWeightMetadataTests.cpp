#if WITH_DEV_AUTOMATION_TESTS
#include "RXWeightMetadata.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXWeightMetadataTest, "RX.SMLODGen.WeightMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXWeightMetadataTest::RunTest(const FString& Parameters)
{
    FRXStoredLODWeights LOD;
    LOD.Settings.PercentTriangles = 0.5f;
    LOD.Sections = { { FPolygonGroupID(2), 0.25f, false, true }, { FPolygonGroupID(9), 0.0f, true } };
    TMap<int32, FRXStoredLODWeights> Before;
    Before.Add(1, LOD);
    FString Json, Error, SourceId;
    if (!TestTrue(TEXT("Encode weight metadata"), RXWeightMetadata::Encode(TEXT("source-id"), Before, 2, 3, Json, Error, 2)))
    { AddError(Error); return false; }
    TMap<int32, FRXStoredLODWeights> After;
    int32 ImpostorLOD = INDEX_NONE, MaterialSlot = INDEX_NONE, Directions = 0;
    if (!TestTrue(TEXT("Decode weight metadata"), RXWeightMetadata::Decode(Json, SourceId, After, ImpostorLOD, MaterialSlot, Error, &Directions)))
    { AddError(Error); return false; }
    TestEqual(TEXT("Two-direction metadata round trips"), Directions, 2);
    TestEqual(TEXT("Source identity round trips"), SourceId, FString(TEXT("source-id")));
    TestEqual(TEXT("Reduction option round trips"), After[1].Settings.PercentTriangles, 0.5f);
    TestEqual(TEXT("Weight round trips"), After[1].Sections[0].Weight, 0.25f);
    TestTrue(TEXT("Silhouette preservation round trips"), After[1].Sections[0].bPreserveOpenBoundary);
    TestTrue(TEXT("Excluded section round trips"), After[1].Sections[1].bExcluded);
    TestEqual(TEXT("Impostor LOD round trips"), ImpostorLOD, 2);
    TestEqual(TEXT("Impostor slot round trips"), MaterialSlot, 3);
    TSharedPtr<FJsonObject> Old; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Old);
    Old->SetNumberField(TEXT("version"), 4); Old->RemoveField(TEXT("impostor_directions")); FString V4;
    FJsonSerializer::Serialize(Old.ToSharedRef(), TJsonWriterFactory<>::Create(&V4));
    TestTrue(TEXT("V4 metadata without mode imports"), RXWeightMetadata::Decode(V4, SourceId, After, ImpostorLOD, MaterialSlot, Error, &Directions));
    TestEqual(TEXT("V4 defaults to four directions"), Directions, 4);
    Old->SetNumberField(TEXT("version"), 1); FString Legacy;
    FJsonSerializer::Serialize(Old.ToSharedRef(), TJsonWriterFactory<>::Create(&Legacy));
    if (TestTrue(TEXT("Legacy reduction-strength metadata imports"), RXWeightMetadata::Decode(Legacy, SourceId, After, ImpostorLOD, MaterialSlot, Error, &Directions)))
    { TestEqual(TEXT("Legacy strength converts to preservation without changing reduction"), After[1].Sections[0].Weight, 0.75f); }
    TestFalse(TEXT("Invalid metadata rejected"), RXWeightMetadata::Decode(TEXT("{\"version\":99}"), SourceId, After, ImpostorLOD, MaterialSlot, Error));
    TestTrue(TEXT("Invalid metadata leaves previous output unchanged"), After.Contains(1));
    return true;
}
#endif
