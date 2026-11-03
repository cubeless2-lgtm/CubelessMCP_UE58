#if WITH_DEV_AUTOMATION_TESTS

#include "RXImpostorPreview.h"
#include "Misc/AutomationTest.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "StaticMeshResources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXImpostorPreviewTest, "RX.SMLODGen.ImpostorPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXImpostorPreviewTest::RunTest(const FString& Parameters)
{
    if (GUsingNullRHI)
    {
        AddWarning(TEXT("GPU preview test is not run with NullRHI."));
        return true;
    }
    UStaticMesh* Source = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Source cube exists"), Source))
    {
        return false;
    }
    const bool bWasDirty = Source->GetOutermost()->IsDirty();
    FRXImpostorPreview Preview;
    FString Error;
    if (!TestTrue(TEXT("Build transient impostor preview"), RXImpostorPreview::Build(Source, FIntPoint(64, 64), 2, 4, Preview, Error)))
    {
        AddError(Error);
        return false;
    }
    TestTrue(TEXT("Preview mesh is transient"), Preview.Mesh.IsValid() && Preview.Mesh->HasAnyFlags(RF_Transient));
    TestTrue(TEXT("Preview uses generated material"), Preview.Mesh->GetStaticMaterials().Num() == 1 &&
        Preview.Mesh->GetMaterial(0) == Preview.Material.Material.Get());
    TestEqual(TEXT("Source package dirty state preserved"), Source->GetOutermost()->IsDirty(), bWasDirty);
    const FBox SourceBounds = Source->GetBoundingBox();
    const FBox PreviewBounds = Preview.Mesh->GetBoundingBox();
    TestTrue(TEXT("Preview bounds match source"), SourceBounds.Min.Equals(PreviewBounds.Min, 0.01) && SourceBounds.Max.Equals(PreviewBounds.Max, 0.01));
    const auto& Vertices = Preview.Mesh->GetRenderData()->LODResources[0].VertexBuffers.StaticMeshVertexBuffer;
    for (uint32 Index = 0; Index < Vertices.GetNumVertices(); ++Index)
    {
        const FVector4f Normal = Vertices.VertexTangentZ(Index);
        if (!FVector3f(Normal.X, Normal.Y, Normal.Z).Equals(FVector3f(0, 0, 1), 0.01f)) { AddError(TEXT("Rendered vertex normal must point up")); break; }
    }
    if (TestTrue(TEXT("Build two-view GPU preview"), RXImpostorPreview::Build(Source, FIntPoint(64, 64), 2, 4, Preview, Error, 2)))
    {
        TestEqual(TEXT("Two-view atlas width"), Preview.Material.Texture->Source.GetSizeX(), int64(136));
        TestEqual(TEXT("Two-view geometry"), Preview.Geometry->Triangles().Num(), 64);
        TestEqual(TEXT("Prepared preview carries selected mode"), Preview.Directions, 2);
        const FBox B = Preview.Mesh->GetBoundingBox();
        TestTrue(TEXT("Two-view rendered bounds match"), SourceBounds.Min.Equals(B.Min, 0.01) && SourceBounds.Max.Equals(B.Max, 0.01));
    }
    else { AddError(Error); }
    Preview.Mesh.Reset();
    Preview.Material.Material.Reset();
    Preview.Material.Texture.Reset();
    FlushRenderingCommands();
    return true;
}

#endif
