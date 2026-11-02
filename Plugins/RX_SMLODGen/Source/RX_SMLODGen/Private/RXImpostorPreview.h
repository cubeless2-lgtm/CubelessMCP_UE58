#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "RXImpostorMaterial.h"
#include "UObject/StrongObjectPtr.h"

struct FRXImpostorPreview
{
    TStrongObjectPtr<UStaticMesh> Mesh;
    FRXImpostorPreviewMaterial Material;
    TUniquePtr<FMeshDescription> Geometry;
};

namespace RXImpostorPreview
{
    // Transient preview assets only; does not alter, register, or save Source.
    bool Build(UStaticMesh* Source, FIntPoint TileSize, int32 Padding, int32 Subdivisions,
        FRXImpostorPreview& Output, FString& Error);
}
