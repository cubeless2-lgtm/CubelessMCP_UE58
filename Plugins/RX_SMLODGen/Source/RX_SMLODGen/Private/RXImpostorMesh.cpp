#include "RXImpostorMesh.h"

#include "StaticMeshAttributes.h"

bool RXImpostorMesh::Build(const FBox& SourceBounds, int32 Subdivisions, FIntPoint TileSize, int32 Padding,
    FMeshDescription& Output, FString& Error)
{
    Error.Reset();
    if (!SourceBounds.IsValid || SourceBounds.Min.ContainsNaN() || SourceBounds.Max.ContainsNaN() ||
        Subdivisions < 2 || Subdivisions > 32 || TileSize.X <= 0 || TileSize.Y <= 0 ||
        Padding < 0 || Padding > 64 || (int64(TileSize.X) + 2 * Padding) * 4 > 8192 ||
        int64(TileSize.Y) + 2 * Padding > 8192)
    {
        Error = TEXT("Invalid bounds, subdivisions, atlas tile size, or padding.");
        return false;
    }
    const FVector Center = SourceBounds.GetCenter();
    const FVector Extent = SourceBounds.GetExtent();
    const double PaddedWidth = double(TileSize.X) + 2.0 * Padding;
    const double PaddedHeight = double(TileSize.Y) + 2.0 * Padding;
    if (Extent.X <= UE_SMALL_NUMBER || Extent.Y <= UE_SMALL_NUMBER || Extent.Z <= UE_SMALL_NUMBER)
    {
        Error = TEXT("The source bounds must have nonzero size on all axes.");
        return false;
    }

    FMeshDescription Mesh;
    FStaticMeshAttributes Attributes(Mesh);
    Attributes.Register();
    auto Positions = Attributes.GetVertexPositions();
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto Tangents = Attributes.GetVertexInstanceTangents();
    auto BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    auto UVs = Attributes.GetVertexInstanceUVs();
    auto Names = Attributes.GetPolygonGroupMaterialSlotNames();
    UVs.SetNumChannels(1);
    const FPolygonGroupID Group = Mesh.CreatePolygonGroup();
    Names[Group] = FName("Impostor");

    for (int32 Plane = 0; Plane < 4; ++Plane)
    {
        const double Angle = UE_PI * double(Plane) / 4.0;
        // Unreal's forward direction is +X. Each plane's positive side faces
        // forward, forward-right, right, then rear-right respectively.
        const FVector Facing(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
        const FVector Along(Facing.Y, -Facing.X, 0.0);
        const double LimitX = FMath::Abs(Along.X) > UE_SMALL_NUMBER ? Extent.X / FMath::Abs(Along.X) : TNumericLimits<double>::Max();
        const double LimitY = FMath::Abs(Along.Y) > UE_SMALL_NUMBER ? Extent.Y / FMath::Abs(Along.Y) : TNumericLimits<double>::Max();
        const double HalfWidth = FMath::Min(LimitX, LimitY);
        // Match an orthographic capture that contains the projected source
        // bounds. Crop the image to this plane's physical width and height.
        const double Aspect = double(TileSize.X) / TileSize.Y;
        const double ProjectedHalfWidth = FMath::Abs(Along.X) * Extent.X + FMath::Abs(Along.Y) * Extent.Y;
        const double OrthoWidth = 2.1 * FMath::Max(ProjectedHalfWidth, Extent.Z * Aspect);
        const double OrthoHeight = OrthoWidth / Aspect;
        TArray<FVertexID> Vertices;
        TArray<FVertexInstanceID> Instances;
        const int32 Side = Subdivisions + 1;
        Vertices.SetNum(Side * Side);
        Instances.SetNum(Side * Side);
        for (int32 Y = 0; Y < Side; ++Y)
        {
            const double Height = double(Y) / double(Subdivisions);
            for (int32 X = 0; X < Side; ++X)
            {
                const double U = double(X) / double(Subdivisions);
                const double Horizontal = U * 2.0 - 1.0;
                const FVector Position = Center + Along * (Horizontal * HalfWidth) + FVector(0, 0, (Height * 2.0 - 1.0) * Extent.Z);
                const FVector Normal = FVector::UpVector;
                const int32 Index = Y * Side + X;
                Vertices[Index] = Mesh.CreateVertex();
                Positions[Vertices[Index]] = FVector3f(Position);
                Instances[Index] = Mesh.CreateVertexInstance(Vertices[Index]);
                Normals[Instances[Index]] = FVector3f(Normal);
                // The projected tangent remains perpendicular to the custom normal.
                const FVector Tangent = (Along - Normal * FVector::DotProduct(Along, Normal)).GetSafeNormal();
                Tangents[Instances[Index]] = FVector3f(Tangent.IsNearlyZero() ? Facing : Tangent);
                BinormalSigns[Instances[Index]] = 1.0f;
                const double CaptureU = 0.5 + Horizontal * HalfWidth / OrthoWidth;
                const double CaptureV = 0.5 - (Height * 2.0 - 1.0) * Extent.Z / OrthoHeight;
                const double AtlasU = (Plane * PaddedWidth + Padding + 0.5 + CaptureU * (TileSize.X - 1)) / (4.0 * PaddedWidth);
                const double AtlasV = (Padding + 0.5 + CaptureV * (TileSize.Y - 1)) / PaddedHeight;
                UVs.Set(Instances[Index], 0, FVector2f(float(AtlasU), float(AtlasV)));
            }
        }
        for (int32 Y = 0; Y < Subdivisions; ++Y)
        {
            for (int32 X = 0; X < Subdivisions; ++X)
            {
                const FVertexInstanceID A = Instances[Y * Side + X];
                const FVertexInstanceID B = Instances[Y * Side + X + 1];
                const FVertexInstanceID C = Instances[(Y + 1) * Side + X + 1];
                const FVertexInstanceID D = Instances[(Y + 1) * Side + X];
                const FVertexInstanceID TriangleA[] = { A, C, B };
                const FVertexInstanceID TriangleB[] = { A, D, C };
                Mesh.CreateTriangle(Group, MakeArrayView(TriangleA));
                Mesh.CreateTriangle(Group, MakeArrayView(TriangleB));
            }
        }
    }
    Output = MoveTemp(Mesh);
    return true;
}
