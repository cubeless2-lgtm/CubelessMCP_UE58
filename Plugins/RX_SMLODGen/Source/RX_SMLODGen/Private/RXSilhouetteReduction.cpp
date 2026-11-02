#include "RXSilhouetteReduction.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshSimplification.h"
#include "MeshConstraintsUtil.h"
#include "StaticMeshAttributes.h"

bool RXSilhouetteReduction::Reduce(const FMeshDescription& Source, float KeepFraction, FMeshDescription& Output, FString& Error)
{
    using namespace UE::Geometry;
    FDynamicMesh3 Mesh;
    FMeshDescriptionToDynamicMesh ToDynamic;
    ToDynamic.bTransformVertexColorsLinearToSRGB = false;
    ToDynamic.Convert(&Source, Mesh, true);
    if (Mesh.TriangleCount() != Source.Triangles().Num())
    { Error = TEXT("실루엣 보존용 메시 변환에서 삼각형이 누락됐습니다."); return false; }
    FMeshConstraints Constraints;
    FMeshConstraintsUtil::ConstrainAllBoundariesAndSeams(Constraints, Mesh,
        EEdgeRefineFlags::FullyConstrained, EEdgeRefineFlags::NoConstraint, EEdgeRefineFlags::FullyConstrained,
        false, false, false);
    // Every non-coplanar edge can be a silhouette from some direction. Pin both
    // endpoints; this deliberately trades reduction ratio for shape preservation.
    for (int Edge : Mesh.EdgeIndicesItr())
    {
        const auto Triangles = Mesh.GetEdgeT(Edge);
        if (Triangles.B == IndexConstants::InvalidID || Mesh.GetTriNormal(Triangles.A).Dot(Mesh.GetTriNormal(Triangles.B)) < 1.0 - 1e-8)
        {
            Constraints.SetOrUpdateEdgeConstraint(Edge, FEdgeConstraint::FullyConstrained());
            const auto Vertices = Mesh.GetEdgeV(Edge);
            Constraints.SetOrUpdateVertexConstraint(Vertices.A, FVertexConstraint::FullyConstrained());
            Constraints.SetOrUpdateVertexConstraint(Vertices.B, FVertexConstraint::FullyConstrained());
        }
    }
    FAttrMeshSimplification Simplifier(&Mesh);
    Simplifier.bPreserveBoundaryShape = true;
    Simplifier.bAllowSeamCollapse = false;
    Simplifier.SetExternalConstraints(MoveTemp(Constraints));
    Simplifier.SimplifyToTriangleCount(FMath::Max(1, FMath::CeilToInt(Source.Triangles().Num() * KeepFraction)));
    if (Mesh.TriangleCount() == Source.Triangles().Num()) { Output = Source; return true; }
    FMeshDescription Converted;
    FStaticMeshAttributes(Converted).Register();
    FDynamicMeshToMeshDescription ToDescription;
    ToDescription.ConversionOptions.bUpdateTangents = true;
    ToDescription.ConversionOptions.bUpdateUVs = true;
    ToDescription.ConversionOptions.bUpdateVtxColors = true;
    ToDescription.ConversionOptions.bTransformVtxColorsSRGBToLinear = false;
    ToDescription.Convert(&Mesh, Converted);
    const auto Names = FStaticMeshConstAttributes(Source).GetPolygonGroupMaterialSlotNames();
    auto OutNames = FStaticMeshAttributes(Converted).GetPolygonGroupMaterialSlotNames();
    TArray<FPolygonGroupID> Empty;
    for (const auto Group : Converted.PolygonGroups().GetElementIDs())
    {
        if (Converted.GetPolygonGroupTriangles(Group).IsEmpty()) { Empty.Add(Group); continue; }
        if (!Source.IsPolygonGroupValid(Group)) { Error = TEXT("실루엣 보존 중 섹션 식별자가 변경됐습니다."); return false; }
        OutNames[Group] = Names[Group];
    }
    for (const auto Group : Empty) { Converted.DeletePolygonGroup(Group); }
    if (Converted.Triangles().Num() == 0) { Error = TEXT("실루엣 보존 결과가 비어 있습니다."); return false; }
    Output = MoveTemp(Converted);
    return true;
}
