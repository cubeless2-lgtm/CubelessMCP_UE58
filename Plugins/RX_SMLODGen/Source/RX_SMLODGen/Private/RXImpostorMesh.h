#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"

namespace RXImpostorMesh
{
    // Four fixed, vertical planes in source-local space. Atlas layout is a
    // single row of four tiles; opposite faces reuse the same tile. Geometry
    // generation does not capture textures or create a material.
    bool Build(const FBox& SourceBounds, int32 Subdivisions, FIntPoint TileSize, int32 Padding,
        FMeshDescription& Output, FString& Error);
}
