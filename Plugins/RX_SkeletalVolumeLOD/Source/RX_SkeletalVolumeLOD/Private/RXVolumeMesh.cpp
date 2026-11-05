#include "RXVolumeMesh.h"

#include "BoneWeights.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/SkeletalMesh.h"
#include "Implicit/Solidify.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "MeshSimplification.h"
#include "Operations/TransferBoneWeights.h"
#include "SkeletalMeshAttributes.h"
#include "Spatial/FastWinding.h"
#include "StaticMeshOperations.h"
#include "XAtlasWrapper.h"

#define LOCTEXT_NAMESPACE "RXVolumeMesh"

namespace RXVolumeLOD
{
namespace
{
using namespace UE::Geometry;

void WriteReferenceBones(FDynamicMesh3& Mesh, const FReferenceSkeleton& Skeleton)
{
    FDynamicMeshAttributeSet* Attributes = Mesh.Attributes();
    Attributes->EnableBones(Skeleton.GetRawBoneNum());
    for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetRawBoneNum(); ++BoneIndex)
    {
        const FMeshBoneInfo& Bone = Skeleton.GetRawRefBoneInfo()[BoneIndex];
        Attributes->GetBoneNames()->SetValue(BoneIndex, Bone.Name);
        Attributes->GetBoneParentIndices()->SetValue(BoneIndex, Bone.ParentIndex);
        Attributes->GetBonePoses()->SetValue(BoneIndex, Skeleton.GetRawRefBonePose()[BoneIndex]);
    }
}

bool CreateUVAtlas(FDynamicMesh3& Mesh)
{
    TArray<FVector3f> Vertices;
    Vertices.Reserve(Mesh.VertexCount());
    for (const int32 VertexID : Mesh.VertexIndicesItr())
    {
        const FVector3f Vertex(Mesh.GetVertex(VertexID));
        if (VertexID != Vertices.Num() || Vertex.ContainsNaN())
        {
            return false;
        }
        Vertices.Add(Vertex);
    }

    TArray<int32> Indices;
    Indices.Reserve(Mesh.TriangleCount() * 3);
    for (const int32 TriangleID : Mesh.TriangleIndicesItr())
    {
        const FIndex3i Triangle = Mesh.GetTriangle(TriangleID);
        if (Mesh.GetTriArea(TriangleID) <= 0.0)
        {
            return false;
        }

        // Match the winding passed to XAtlas by UE's own parameterization operator.
        Indices.Add(Triangle.B);
        Indices.Add(Triangle.A);
        Indices.Add(Triangle.C);
    }

    XAtlasWrapper::XAtlasChartOptions ChartOptions;
    XAtlasWrapper::XAtlasPackOptions PackOptions;
    PackOptions.Resolution = 1024;
    PackOptions.Padding = 8;
    TArray<FVector2D> UVs;
    TArray<int32> UVIndices;
    TArray<int32> VertexRemap;
    if (!XAtlasWrapper::ComputeUVs(Indices, Vertices, ChartOptions, PackOptions, UVs, UVIndices, VertexRemap)
        || UVIndices.Num() != Indices.Num() || UVs.Num() != VertexRemap.Num())
    {
        return false;
    }

    FDynamicMeshUVOverlay* Overlay = Mesh.Attributes()->PrimaryUV();
    TArray<int32> UVElements;
    UVElements.Reserve(UVs.Num());
    for (const FVector2D& UV : UVs)
    {
        if (!FMath::IsFinite(UV.X) || !FMath::IsFinite(UV.Y) || UV.X < 0.0 || UV.X > 1.0 || UV.Y < 0.0 || UV.Y > 1.0)
        {
            return false;
        }
        UVElements.Add(Overlay->AppendElement(FVector2f(UV)));
    }

    for (int32 Index = 0; Index < UVIndices.Num(); Index += 3)
    {
        const FIndex3i UVTriangle(UVIndices[Index], UVIndices[Index + 1], UVIndices[Index + 2]);
        if (!UVElements.IsValidIndex(UVTriangle.A) || !UVElements.IsValidIndex(UVTriangle.B)
            || !UVElements.IsValidIndex(UVTriangle.C))
        {
            return false;
        }

        const FIndex3i VerticesOfFace(VertexRemap[UVTriangle.A], VertexRemap[UVTriangle.B], VertexRemap[UVTriangle.C]);
        if (!Vertices.IsValidIndex(VerticesOfFace.A) || !Vertices.IsValidIndex(VerticesOfFace.B)
            || !Vertices.IsValidIndex(VerticesOfFace.C))
        {
            return false;
        }
        const int32 TriangleID = Mesh.FindTriangle(VerticesOfFace.A, VerticesOfFace.B, VerticesOfFace.C);
        if (TriangleID == FDynamicMesh3::InvalidID || Overlay->IsSetTriangle(TriangleID))
        {
            return false;
        }

        const FVector2D UVDirectionA = UVs[UVTriangle.B] - UVs[UVTriangle.A];
        const FVector2D UVDirectionB = UVs[UVTriangle.C] - UVs[UVTriangle.A];
        if (FMath::Abs(UVDirectionA.X * UVDirectionB.Y - UVDirectionA.Y * UVDirectionB.X) <= 1.e-12)
        {
            return false;
        }

        const FIndex3i Triangle = Mesh.GetTriangle(TriangleID);
        FIndex3i CornerUVs;
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            const int32 UVCorner = VerticesOfFace.IndexOf(Triangle[Corner]);
            if (UVCorner == INDEX_NONE)
            {
                return false;
            }
            CornerUVs[Corner] = UVElements[UVTriangle[UVCorner]];
        }
        if (Overlay->SetTriangle(TriangleID, CornerUVs) != EMeshResult::Ok)
        {
            return false;
        }
    }
    return true;
}
}

bool BuildVolumeMesh(
    const USkeletalMesh& SourceMesh,
    const FMeshDescription& SourceDescription,
    int32 VoxelCount,
    int32 TargetTriangleCount,
    FDynamicMesh3& OutTargetMesh,
    FMeshDescription& OutLODDescription,
    FText& Error)
{
    const FReferenceSkeleton& Skeleton = SourceMesh.GetRefSkeleton();
    if (VoxelCount < 16 || VoxelCount > 256 || TargetTriangleCount < 4)
    {
        Error = LOCTEXT("InvalidSettings", "Use a voxel count between 16 and 256 and a triangle target of at least four.");
        return false;
    }
    if (SourceDescription.Triangles().Num() == 0 || Skeleton.GetRawBoneNum() == 0)
    {
        Error = LOCTEXT("InvalidSource", "LOD0 must contain source triangles and a reference skeleton.");
        return false;
    }

    const FSkeletalMeshConstAttributes SourceAttributes(SourceDescription);
    const TVertexAttributesConstRef<FVector3f> Positions = SourceAttributes.GetVertexPositions();
    for (const FVertexID VertexID : SourceDescription.Vertices().GetElementIDs())
    {
        if (Positions[VertexID].ContainsNaN())
        {
            Error = LOCTEXT("InvalidSourcePosition", "LOD0 contains a non-finite vertex position.");
            return false;
        }
    }

    FDynamicMesh3 Source;
    FMeshDescriptionToDynamicMesh SourceConverter;
    SourceConverter.Convert(&SourceDescription, Source);
    const FName WeightProfile = FSkeletalMeshAttributes::DefaultSkinWeightProfileName;
    if (Source.TriangleCount() == 0 || !Source.HasAttributes() || !Source.Attributes()->GetSkinWeightsAttribute(WeightProfile))
    {
        Error = LOCTEXT("MissingSourceSkinWeights", "LOD0 could not be converted with its default skin weights.");
        return false;
    }
    if (!Source.Attributes()->HasBones() || Source.Attributes()->GetNumBones() == 0)
    {
        // Older imported descriptions may omit bone metadata; their weights index the mesh reference skeleton.
        WriteReferenceBones(Source, Skeleton);
    }

    FDynamicMeshAABBTree3 SourceTree(&Source);
    const FAxisAlignedBox3d Bounds = SourceTree.GetBoundingBox();
    const double SourceSize = Bounds.MaxDim();
    if (!FMath::IsFinite(SourceSize) || SourceSize <= UE_DOUBLE_SMALL_NUMBER)
    {
        Error = LOCTEXT("InvalidSourceBounds", "LOD0 has invalid bounds for volume remeshing.");
        return false;
    }

    TFastWindingTree<FDynamicMesh3> Winding(&SourceTree);
    TImplicitSolidify<FDynamicMesh3> Solidify(&Source, &SourceTree, &Winding);
    Solidify.SetCellSizeAndExtendBounds(Bounds, 3.0 * SourceSize / VoxelCount, VoxelCount);
    FDynamicMesh3 Target(&Solidify.Generate());
    if (Target.TriangleCount() < 4 || !Target.IsClosed())
    {
        Error = LOCTEXT("InvalidVolume", "Volume remeshing did not produce a closed shell. Check LOD0 winding or use a finer voxel grid.");
        return false;
    }

    if (Target.TriangleCount() > TargetTriangleCount)
    {
        FVolPresMeshSimplification Simplifier(&Target);
        Simplifier.SimplifyToTriangleCount(TargetTriangleCount);
    }
    if (Target.TriangleCount() < 4 || Target.TriangleCount() > TargetTriangleCount || !Target.IsClosed())
    {
        Error = LOCTEXT("SimplifyFailed", "Could not reach the triangle target while keeping a closed volume.");
        return false;
    }

    FDynamicMesh3 CompactTarget;
    CompactTarget.CompactCopy(Target);
    Target = MoveTemp(CompactTarget);
    Target.EnableAttributes();
    Target.Attributes()->DisableMaterialID();
    Target.Attributes()->SetNumUVLayers(1);
    FMeshNormals::InitializeOverlayToPerVertexNormals(Target.Attributes()->PrimaryNormals());
    if (!CreateUVAtlas(Target))
    {
        Error = LOCTEXT("UVFailed", "XAtlas could not generate a valid UV0 atlas for the new shell.");
        return false;
    }

    WriteReferenceBones(Target, Skeleton);
    FTransferBoneWeights Transfer(&Source, WeightProfile, &SourceTree);
    Transfer.MaxNumInfluences = 8;
    if (!Transfer.TransferWeightsToMesh(Target, WeightProfile))
    {
        Error = LOCTEXT("SkinTransferFailed", "Could not transfer skin weights from the LOD0 reference surface.");
        return false;
    }

    const FDynamicMeshVertexSkinWeightsAttribute* SkinWeights = Target.Attributes()->GetSkinWeightsAttribute(WeightProfile);
    if (!SkinWeights)
    {
        Error = LOCTEXT("MissingTargetSkinWeights", "Skin transfer did not produce the default skin weight profile.");
        return false;
    }
    for (const int32 VertexID : Target.VertexIndicesItr())
    {
        UE::AnimationCore::FBoneWeights VertexWeights;
        SkinWeights->GetValue(VertexID, VertexWeights);
        float Sum = 0.0f;
        for (const UE::AnimationCore::FBoneWeight& Influence : VertexWeights)
        {
            if (Influence.GetBoneIndex() >= Skeleton.GetRawBoneNum())
            {
                Error = LOCTEXT("InvalidBoneIndex", "Skin transfer produced an influence outside the reference skeleton.");
                return false;
            }
            Sum += Influence.GetWeight();
        }
        if (!FMath::IsNearlyEqual(Sum, 1.0f, 0.001f))
        {
            Error = LOCTEXT("InvalidBoneWeights", "Skin transfer produced an unweighted vertex or invalid weight sum.");
            return false;
        }
    }

    FMeshDescription LODDescription;
    FSkeletalMeshAttributes OutputAttributes(LODDescription);
    OutputAttributes.Register();
    OutputAttributes.RegisterColorAttribute();
    FDynamicMeshToMeshDescription TargetConverter;
    TargetConverter.ConversionOptions.bSetPolyGroups = false;
    TargetConverter.Convert(&Target, LODDescription);
    if (LODDescription.PolygonGroups().Num() != 1 || LODDescription.Triangles().Num() != Target.TriangleCount())
    {
        Error = LOCTEXT("LODConversionFailed", "The volume mesh could not be converted to a single skeletal material section.");
        return false;
    }

    // Use the same imported Mikk basis for the bake and the resulting skeletal LOD.
    FStaticMeshOperations::ComputeMikktTangents(LODDescription, true);
    FSkeletalMeshAttributes LODAttributes(LODDescription);
    const TVertexInstanceAttributesRef<FVector3f> Tangents = LODAttributes.GetVertexInstanceTangents();
    const TVertexInstanceAttributesRef<float> BinormalSigns = LODAttributes.GetVertexInstanceBinormalSigns();
    const auto Normals = LODAttributes.GetVertexInstanceNormals();
    for (const FVertexInstanceID InstanceID : LODDescription.VertexInstances().GetElementIDs())
    {
        if (Tangents[InstanceID].ContainsNaN() || Tangents[InstanceID].SizeSquared() < 0.5f
            || !FMath::IsNearlyEqual(FMath::Abs(BinormalSigns[InstanceID]), 1.0f))
        {
            const FVector3f Normal = Normals[InstanceID].GetSafeNormal();
            if (Normal.ContainsNaN() || Normal.IsNearlyZero())
            {
                Error = LOCTEXT("InvalidTangents", "The new UV0 contains a corner without a usable normal or tangent basis.");
                return false;
            }
            FVector3f Tangent, Bitangent;
            Normal.FindBestAxisVectors(Tangent, Bitangent);
            Tangents[InstanceID] = Tangent;
            BinormalSigns[InstanceID] = FVector3f::DotProduct(FVector3f::CrossProduct(Normal, Tangent), Bitangent) < 0 ? -1.f : 1.f;
        }
    }
    FMeshDescriptionToDynamicMesh BakeTargetConverter;
    FDynamicMesh3 BakeTarget;
    BakeTargetConverter.Convert(&LODDescription, BakeTarget, /*bCopyTangents=*/true);
    if (BakeTarget.TriangleCount() != Target.TriangleCount() || BakeTarget.VertexCount() != Target.VertexCount()
        || !BakeTarget.HasAttributes() || !BakeTarget.Attributes()->PrimaryUV() || !BakeTarget.Attributes()->HasTangentSpace())
    {
        Error = LOCTEXT("BakeTargetFailed", "Could not prepare the skeletal LOD with its UV0 tangent basis for baking.");
        return false;
    }

    OutTargetMesh = MoveTemp(BakeTarget);
    OutLODDescription = MoveTemp(LODDescription);
    Error = FText::GetEmpty();
    return true;
}
}

#undef LOCTEXT_NAMESPACE
