#if WITH_DEV_AUTOMATION_TESTS
#include "RXLODStats.h"
#include "StaticMeshResources.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXSectionStatsTest, "RX.SMLODGen.SectionStatistics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXSectionStatsTest::RunTest(const FString&)
{
    FStaticMeshLODResources LOD(false);
    const TArray<uint32> Indices = { 2, 8, 20, 2, 20, 8, 8, 40, 60 };
    LOD.IndexBuffer.SetIndices(Indices, EIndexBufferStride::Force32Bit);
    auto& First = LOD.Sections.AddDefaulted_GetRef(); First.FirstIndex = 0; First.NumTriangles = 2;
    auto& Second = LOD.Sections.AddDefaulted_GetRef(); Second.FirstIndex = 6; Second.NumTriangles = 1;
    TestEqual(TEXT("First section triangles"), RXLODStats::Section(LOD, 0).Triangles, 2u);
    TestEqual(TEXT("Sparse indices count referenced vertices, not min/max span"), RXLODStats::Section(LOD, 0).Vertices, 3u);
    TestEqual(TEXT("Second section counts its shared vertex once"), RXLODStats::Section(LOD, 1).Vertices, 3u);
    TestEqual(TEXT("Invalid section is empty"), RXLODStats::Section(LOD, 2).Vertices, 0u);
    return true;
}
#endif
