#if WITH_DEV_AUTOMATION_TESTS

#include "RXImpostorMesh.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshAttributes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXImpostorMeshTest, "RX.SMLODGen.ImpostorGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXImpostorMeshTest::RunTest(const FString& Parameters)
{
    const FBox Bounds(FVector(-80.0, -30.0, -20.0), FVector(120.0, 50.0, 180.0));
    FMeshDescription Mesh;
    FString Error;
    if (!TestTrue(TEXT("Generate fixed impostor planes"), RXImpostorMesh::Build(Bounds, 4, FIntPoint(128, 128), 2, Mesh, Error)))
    {
        AddError(Error);
        return false;
    }
    TestEqual(TEXT("Four subdivided planes"), Mesh.Triangles().Num(), 4 * 4 * 4 * 2);
    TestEqual(TEXT("One material section"), Mesh.PolygonGroups().Num(), 1);
    FStaticMeshConstAttributes Attributes(Mesh);
    auto Positions = Attributes.GetVertexPositions();
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto UVs = Attributes.GetVertexInstanceUVs();
    FBox Actual(ForceInit);
    for (const FVertexID Vertex : Mesh.Vertices().GetElementIDs())
    {
        Actual += FVector(Positions[Vertex]);
    }
    TestTrue(TEXT("Preserves source bounds and pivot"), Actual.Min.Equals(Bounds.Min, 0.001) && Actual.Max.Equals(Bounds.Max, 0.001));
    for (const FVertexInstanceID Instance : Mesh.VertexInstances().GetElementIDs())
    {
        const FVector3f Normal = Normals[Instance];
        const FVector2f UV = UVs.Get(Instance, 0);
        const int32 Plane = Instance.GetValue() / 25;
        const float TileMin = (Plane * 132.0f + 2.0f) / 528.0f;
        const float TileMax = (Plane * 132.0f + 130.0f) / 528.0f;
        if (!Normal.Equals(FVector3f(0, 0, 1), KINDA_SMALL_NUMBER) || UV.X < TileMin || UV.X > TileMax || UV.Y < 0.0f || UV.Y > 1.0f)
        {
            AddError(TEXT("Impostor normal must be exactly local up; atlas UV must remain in its tile"));
            break;
        }
    }
    FMeshDescription Two;
    if (TestTrue(TEXT("Build front and side cards"), RXImpostorMesh::Build(Bounds, 4, FIntPoint(128, 128), 2, Two, Error, 2)))
    {
        TestEqual(TEXT("Two cards halve triangles"), Two.Triangles().Num(), 2 * 4 * 4 * 2);
        FStaticMeshConstAttributes A(Two); FBox TwoBounds(ForceInit);
        for (auto V : Two.Vertices().GetElementIDs()) { TwoBounds += FVector(A.GetVertexPositions()[V]); }
        TestTrue(TEXT("Two cards preserve bounds"), TwoBounds.Min.Equals(Bounds.Min, 0.001) && TwoBounds.Max.Equals(Bounds.Max, 0.001));
        for (auto V : Two.Vertices().GetElementIDs())
        {
            const FVector P(A.GetVertexPositions()[V]); const int32 Plane = V.GetValue() / 25;
            if (!FMath::IsNearlyEqual(Plane == 0 ? P.X : P.Y, Plane == 0 ? Bounds.GetCenter().X : Bounds.GetCenter().Y, 0.001)) { AddError(TEXT("Two planes must face +X and +Y")); break; }
        }
        for (auto I : Two.VertexInstances().GetElementIDs())
        {
            const int32 Plane = I.GetValue() / 25; const auto UV = A.GetVertexInstanceUVs().Get(I, 0);
            if (!A.GetVertexInstanceNormals()[I].Equals(FVector3f(0, 0, 1), 0.001f) || UV.X < (Plane * 132.0f + 2) / 264 || UV.X > (Plane * 132.0f + 130) / 264) { AddError(TEXT("Two-card up normals and atlas tiles must match")); break; }
        }
    }
    TestFalse(TEXT("Reject unsupported three-direction mode"), RXImpostorMesh::Build(Bounds, 4, FIntPoint(128, 128), 2, Two, Error, 3));
    const int32 PreviousTriangles = Mesh.Triangles().Num();
    TestFalse(TEXT("Rejects flat source bounds"), RXImpostorMesh::Build(FBox(FVector::ZeroVector, FVector(1, 1, 0)), 4, FIntPoint(128, 128), 2, Mesh, Error));
    TestEqual(TEXT("Invalid bounds leave output unchanged"), Mesh.Triangles().Num(), PreviousTriangles);
    return true;
}

#endif
