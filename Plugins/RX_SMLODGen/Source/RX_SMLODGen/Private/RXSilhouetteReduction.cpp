#include "RXSilhouetteReduction.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshSimplification.h"
#include "MeshConstraints.h"
#include "StaticMeshAttributes.h"
#include "MeshElementRemappings.h"

bool RXSilhouetteReduction::Reduce(const FMeshDescription& Source, float KeepFraction, const FRXSectionWeight& Options, FMeshDescription& Output, FString& Error)
{
    using namespace UE::Geometry;
    FDynamicMesh3 Mesh;
    // MeshDescriptionToDynamicMesh uses vertex IDs as DynamicMesh indices.
    // A section extracted by deleting other polygons can have sparse IDs.
    FMeshDescription CompactSource(Source);
    FElementIDRemappings Remappings;
    CompactSource.Compact(Remappings);
    TArray<int32> GroupToOriginal; GroupToOriginal.Init(INDEX_NONE, CompactSource.PolygonGroups().Num());
    for (const auto Group : Source.PolygonGroups().GetElementIDs())
    { GroupToOriginal[Remappings.GetRemappedPolygonGroupID(Group).GetValue()] = Group.GetValue(); }
    FMeshDescriptionToDynamicMesh ToDynamic;
    ToDynamic.bTransformVertexColorsLinearToSRGB = false;
    ToDynamic.SetPolygonGroupToMaterialIndexMap(GroupToOriginal);
    ToDynamic.Convert(&CompactSource, Mesh, true);
    if (Mesh.TriangleCount() != Source.Triangles().Num())
    { Error = TEXT("실루엣 보존용 메시 변환에서 삼각형이 누락됐습니다."); return false; }
    FMeshConstraints Constraints;
    // Pin only explicitly selected categories. Attribute seams and geometric
    // creases must not silently turn on when only open boundaries are selected.
    for (int Edge : Mesh.EdgeIndicesItr())
    {
        const auto Triangles = Mesh.GetEdgeT(Edge);
        const bool Open = Triangles.B == IndexConstants::InvalidID;
        const bool Sharp = !Open && Mesh.GetTriNormal(Triangles.A).Dot(Mesh.GetTriNormal(Triangles.B)) < 1.0 - 1e-8;
        bool AttributeBoundary = false;
        if (Options.bPreserveAttributeSeams && !Open && Mesh.HasAttributes())
        {
            bool UV = false, Normal = false, Color = false, Tangent = false;
            Mesh.Attributes()->IsSeamEdge(Edge, UV, Normal, Color, Tangent);
            AttributeBoundary = UV || Normal;
            if (Mesh.Attributes()->HasMaterialID())
            {
                const auto* Materials = Mesh.Attributes()->GetMaterialID();
                AttributeBoundary |= Materials->GetValue(Triangles.A) != Materials->GetValue(Triangles.B);
            }
        }
        if ((Options.bPreserveOpenBoundary && Open) || (Options.bPreserveSharpEdges && Sharp) || AttributeBoundary)
        {
            Constraints.SetOrUpdateEdgeConstraint(Edge, FEdgeConstraint::FullyConstrained());
            const auto Vertices = Mesh.GetEdgeV(Edge);
            Constraints.SetOrUpdateVertexConstraint(Vertices.A, FVertexConstraint::FullyConstrained());
            Constraints.SetOrUpdateVertexConstraint(Vertices.B, FVertexConstraint::FullyConstrained());
        }
    }
    FAttrMeshSimplification Simplifier(&Mesh);
    // The engine re-evaluates these flags after each successful collapse. Its
    // constructor leaves them uninitialized, even with external constraints.
    Simplifier.MeshBoundaryConstraint = Options.bPreserveOpenBoundary ? EEdgeRefineFlags::FullyConstrained : EEdgeRefineFlags::NoConstraint;
    Simplifier.GroupBoundaryConstraint = EEdgeRefineFlags::NoConstraint;
    Simplifier.MaterialBoundaryConstraint = Options.bPreserveAttributeSeams ? EEdgeRefineFlags::FullyConstrained : EEdgeRefineFlags::NoConstraint;
    Simplifier.bPreserveBoundaryShape = Options.bPreserveOpenBoundary;
    // Explicit seam constraints above carry the selected protection. Leave the
    // global seam switch open, as it also covers unselected open boundaries.
    Simplifier.bAllowSeamCollapse = true;
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
