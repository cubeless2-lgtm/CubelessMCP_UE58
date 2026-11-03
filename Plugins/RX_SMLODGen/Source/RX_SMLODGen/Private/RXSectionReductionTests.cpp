#if WITH_DEV_AUTOMATION_TESTS

#include "RXSectionReduction.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

namespace
{
    FPolygonGroupID AddGrid(FMeshDescription& Mesh, float OffsetX)
    {
        FStaticMeshAttributes Attributes(Mesh);
        auto Positions = Attributes.GetVertexPositions();
        auto Normals = Attributes.GetVertexInstanceNormals();
        auto Tangents = Attributes.GetVertexInstanceTangents();
        auto BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
        auto UVs = Attributes.GetVertexInstanceUVs();
        auto Names = Attributes.GetPolygonGroupMaterialSlotNames();
        const FPolygonGroupID Group = Mesh.CreatePolygonGroup();
        Names[Group] = FName("SharedMaterial");
        FVertexID Vertices[9][9];
        for (int32 Y = 0; Y < 9; ++Y)
        {
            for (int32 X = 0; X < 9; ++X)
            {
                Vertices[Y][X] = Mesh.CreateVertex();
                Positions[Vertices[Y][X]] = FVector3f(OffsetX + X * 10.0f, Y * 10.0f, 0.0f);
            }
        }
        auto AddTriangle = [&](FVertexID A, FVertexID B, FVertexID C)
        {
            FVertexInstanceID Instances[] = { Mesh.CreateVertexInstance(A), Mesh.CreateVertexInstance(B), Mesh.CreateVertexInstance(C) };
            for (FVertexInstanceID Instance : Instances)
            {
                Normals[Instance] = FVector3f(0, 0, 1);
                Tangents[Instance] = FVector3f(1, 0, 0);
                BinormalSigns[Instance] = 1.0f;
                UVs.Set(Instance, 0, FVector2f::ZeroVector);
            }
            Mesh.CreateTriangle(Group, MakeArrayView(Instances));
        };
        for (int32 Y = 0; Y < 8; ++Y)
        {
            for (int32 X = 0; X < 8; ++X)
            {
                AddTriangle(Vertices[Y][X], Vertices[Y][X + 1], Vertices[Y + 1][X + 1]);
                AddTriangle(Vertices[Y][X], Vertices[Y + 1][X + 1], Vertices[Y + 1][X]);
            }
        }
        return Group;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXSectionReductionTest, "RX.SMLODGen.SectionReduction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXSectionReductionTest::RunTest(const FString& Parameters)
{
    FMeshDescription Source;
    FStaticMeshAttributes Attributes(Source);
    Attributes.Register();
    Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    const FPolygonGroupID First = AddGrid(Source, 0.0f);
    const FPolygonGroupID Second = AddGrid(Source, 200.0f);
    FMeshReductionSettings Settings;
    Settings.PercentTriangles = 0.5f;
    Settings.MaxNumOfTriangles = MAX_uint32;
    Settings.MaxDeviation = 0.0f;
    Settings.TerminationCriterion = EStaticMeshReductionTerimationCriterion::Triangles;
    TArray<FRXSectionWeight> Weights = { { First, 1.0f, false }, { Second, 0.0f, false } };
    FMeshDescription Output;
    TArray<FRXSectionReductionResult> Results;
    FString Error;
    if (!TestTrue(TEXT("Separate sections reduce"), RXSectionReduction::Reduce(Source, Settings, Weights, Output, Results, Error)))
    {
        AddError(Error);
        return false;
    }
    TestEqual(TEXT("Source unchanged"), Source.Triangles().Num(), 256);
    TestEqual(TEXT("Separate sections preserved even with same material"), Output.PolygonGroups().Num(), 2);
    TestEqual(TEXT("Weight one preserves every triangle"), Results[0].OutputTriangles, 128);
    TestTrue(TEXT("Weight zero applies normal reduction"), Results[1].OutputTriangles < 128);
    TestEqual(TEXT("Zero-preservation target"), Results[1].TargetKeepFraction, 0.5f);

    Weights[0].Weight = 0.5f;
    Weights[1].bExcluded = true;
    if (TestTrue(TEXT("Half weight and exclusion"), RXSectionReduction::Reduce(Source, Settings, Weights, Output, Results, Error)))
    {
        TestEqual(TEXT("Half-weight target"), Results[0].TargetKeepFraction, 0.75f);
        TestEqual(TEXT("Excluded section keeps every triangle"), Results[1].OutputTriangles, 128);
    }
    FMeshDescription Touching;
    FStaticMeshAttributes TouchingAttributes(Touching);
    TouchingAttributes.Register();
    TouchingAttributes.GetVertexInstanceUVs().SetNumChannels(1);
    const FPolygonGroupID TouchA = AddGrid(Touching, 0.0f);
    const FPolygonGroupID TouchB = AddGrid(Touching, 80.0f);
    const TArray<FRXSectionWeight> TouchWeights = { { TouchA, 1.0f, false }, { TouchB, 0.0f, false } };
    const int32 PreviousTriangles = Output.Triangles().Num();
    TestFalse(TEXT("Touching sections are rejected"), RXSectionReduction::Reduce(Touching, Settings, TouchWeights, Output, Results, Error));
    TestEqual(TEXT("Rejected call leaves output untouched"), Output.Triangles().Num(), PreviousTriangles);
    const TArray<FRXSectionWeight> UniformWeights = { { TouchA, 0.0f, false }, { TouchB, 0.0f, false } };
    if (TestTrue(TEXT("Uniform weights use engine reduction even at section boundaries"), RXSectionReduction::Reduce(Touching, Settings, UniformWeights, Output, Results, Error)))
    {
        TestTrue(TEXT("Uniform zero weight reduces the whole mesh"), Output.Triangles().Num() < Touching.Triangles().Num());
        TestTrue(TEXT("Uniform target is reported as global"), Results.Num() == 2 && Results[0].bGlobalTarget && Results[1].bGlobalTarget);
    }
    const TArray<FRXSectionWeight> PreserveWeights = { { TouchA, 1.0f, false }, { TouchB, 1.0f, false } };
    if (TestTrue(TEXT("Touching sections can be preserved without reduction"), RXSectionReduction::Reduce(Touching, Settings, PreserveWeights, Output, Results, Error)))
    {
        TestEqual(TEXT("All touching triangles preserved"), Output.Triangles().Num(), Touching.Triangles().Num());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXSilhouetteTest, "RX.SMLODGen.SilhouettePreservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXSilhouetteTest::RunTest(const FString&)
{
    FMeshDescription Source;
    FStaticMeshAttributes Attr(Source); Attr.Register(); Attr.GetVertexInstanceUVs().SetNumChannels(1);
    const auto Unused = Source.CreatePolygonGroup();
    const auto Group = AddGrid(Source, 0); Source.DeletePolygonGroup(Unused);
    auto Colors = Attr.GetVertexInstanceColors();
    for (const auto Instance : Source.VertexInstances().GetElementIDs()) { Colors[Instance] = FVector4f(0.2f, 0.4f, 0.6f, 1.0f); }
    FMeshReductionSettings Settings; Settings.PercentTriangles = 0.05f;
    TArray<FRXSectionWeight> Weights = { { Group, 0, false, true } };
    TestTrue(TEXT("Open-only selection leaves other protections off"), Weights[0].bPreserveOpenBoundary && !Weights[0].bPreserveSharpEdges && !Weights[0].bPreserveAttributeSeams);
    FMeshDescription Output; TArray<FRXSectionReductionResult> Results; FString E;
    if (!TestTrue(TEXT("Silhouette-only option triggers constrained reduction"), RXSectionReduction::Reduce(Source, Settings, Weights, Output, Results, E))) { AddError(E); return false; }
    TestTrue(TEXT("Planar interior reduces"), Output.Triangles().Num() < Source.Triangles().Num());
    TestTrue(TEXT("Protected outline may keep more than requested"), Output.Triangles().Num() > FMath::CeilToInt(Source.Triangles().Num() * Settings.PercentTriangles));
    const auto InPositions = FStaticMeshConstAttributes(Source).GetVertexPositions();
    const auto OutPositions = FStaticMeshConstAttributes(Output).GetVertexPositions();
    TSet<FVector3f> OutputPositions;
    for (const auto V : Output.Vertices().GetElementIDs()) { OutputPositions.Add(OutPositions[V]); }
    for (const auto V : Source.Vertices().GetElementIDs())
    {
        const auto P = InPositions[V];
        if (P.X == 0 || P.X == 80 || P.Y == 0 || P.Y == 80) { TestTrue(TEXT("Every outline vertex stays in place"), OutputPositions.Contains(P)); }
    }
    TestTrue(TEXT("Sparse section identity survives conversion"), Output.IsPolygonGroupValid(Group));
    TestEqual(TEXT("Material slot identity survives conversion"), FStaticMeshConstAttributes(Output).GetPolygonGroupMaterialSlotNames()[Group], FName(TEXT("SharedMaterial")));
    for (const auto Instance : Output.VertexInstances().GetElementIDs())
    { TestTrue(TEXT("Vertex colors retain linear values"), FStaticMeshConstAttributes(Output).GetVertexInstanceColors()[Instance].Equals(FVector4f(0.2f, 0.4f, 0.6f, 1.0f), 0.001f)); }
    Weights[0].bPreserveSharpEdges = true;
    Attr.GetVertexPositions()[FVertexID(40)].Z = 20;
    if (TestTrue(TEXT("Nonplanar feature reduces safely"), RXSectionReduction::Reduce(Source, Settings, Weights, Output, Results, E)))
    {
        bool FoundApex = false; const auto Positions = FStaticMeshConstAttributes(Output).GetVertexPositions();
        for (const auto V : Output.Vertices().GetElementIDs()) { FoundApex |= Positions[V] == FVector3f(40, 40, 20); }
        TestTrue(TEXT("Nonplanar feature apex is pinned"), FoundApex);
    }
    Settings.WeldingThreshold = 0.1f;
    TestFalse(TEXT("Silhouette mode rejects welding that could move contours"), RXSectionReduction::Reduce(Source, Settings, Weights, Output, Results, E));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXAttributeBoundaryTest, "RX.SMLODGen.AttributeBoundaryProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXAttributeBoundaryTest::RunTest(const FString&)
{
    FMeshDescription Source; FStaticMeshAttributes Attr(Source); Attr.Register();
    Attr.GetVertexInstanceUVs().SetNumChannels(1);
    const auto Group = AddGrid(Source, 0);
    const auto Positions = Attr.GetVertexPositions(); auto UVs = Attr.GetVertexInstanceUVs();
    for (const auto Triangle : Source.Triangles().GetElementIDs())
    {
        float CenterX = 0;
        for (const auto Vertex : Source.GetTriangleVertices(Triangle)) { CenterX += Positions[Vertex].X / 3; }
        for (const auto Instance : Source.GetTriangleVertexInstances(Triangle))
        {
            const auto P = Positions[Source.GetVertexInstanceVertex(Instance)];
            UVs.Set(Instance, 0, FVector2f(P.X / 80 + (CenterX > 40 ? 2.0f : 0), P.Y / 80));
        }
    }
    FMeshReductionSettings Settings; Settings.PercentTriangles = 0.05f;
    TArray<FRXSectionWeight> Weights = { { Group, 0, false, false, false, true } };
    FMeshDescription Output; TArray<FRXSectionReductionResult> Results; FString E;
    if (!TestTrue(TEXT("Attribute protection works with both geometry protections disabled"), RXSectionReduction::Reduce(Source, Settings, Weights, Output, Results, E))) { AddError(E); return false; }
    TSet<FVector3f> Found; const auto Reduced = FStaticMeshConstAttributes(Output).GetVertexPositions();
    for (const auto V : Output.Vertices().GetElementIDs()) { Found.Add(Reduced[V]); }
    for (int32 Y = 0; Y <= 80; Y += 10) { TestTrue(TEXT("UV split-line vertex stays fixed"), Found.Contains(FVector3f(40, Y, 0))); }
    TestTrue(TEXT("Unconstrained interior still reduces"), Output.Triangles().Num() < Source.Triangles().Num());

    // A material-only seam must stay fixed even without any UV/normal split.
    const auto Right = Source.CreatePolygonGroup();
    Attr.GetPolygonGroupMaterialSlotNames()[Right] = TEXT("RightMaterial");
    for (const auto Instance : Source.VertexInstances().GetElementIDs()) { UVs.Set(Instance, 0, FVector2f::ZeroVector); }
    TArray<FPolygonID> RightPolygons;
    for (const auto Polygon : Source.Polygons().GetElementIDs())
    {
        float CenterX = 0; const auto Vertices = Source.GetPolygonVertices(Polygon);
        for (const auto V : Vertices) { CenterX += Positions[V].X / Vertices.Num(); }
        if (CenterX > 40) { RightPolygons.Add(Polygon); }
    }
    for (const auto Polygon : RightPolygons) { Source.SetPolygonPolygonGroup(Polygon, Right); }
    Weights.Add({ Right, 0, false, false, false, true });
    if (!TestTrue(TEXT("Material-only boundary protection reduces"), RXSectionReduction::Reduce(Source, Settings, Weights, Output, Results, E))) { AddError(E); return false; }
    Found.Reset(); const auto MaterialPositions = FStaticMeshConstAttributes(Output).GetVertexPositions();
    for (const auto V : Output.Vertices().GetElementIDs()) { Found.Add(MaterialPositions[V]); }
    for (int32 Y = 0; Y <= 80; Y += 10) { TestTrue(TEXT("Material split-line vertex stays fixed"), Found.Contains(FVector3f(40, Y, 0))); }
    TestTrue(TEXT("Material sections retain their identity"), Output.IsPolygonGroupValid(Group) && Output.IsPolygonGroupValid(Right));
    TestTrue(TEXT("Material protection still allows reduction"), Output.Triangles().Num() < Source.Triangles().Num());
    return true;
}

#endif
