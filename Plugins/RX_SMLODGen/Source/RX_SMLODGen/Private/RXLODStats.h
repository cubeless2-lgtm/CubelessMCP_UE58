#pragma once
#include "CoreMinimal.h"

struct FStaticMeshLODResources;
struct FRXSectionStats
{
    uint32 Triangles = 0;
    uint32 Vertices = 0;
};
namespace RXLODStats
{
    // Render vertices referenced by this section, including UV/normal splits.
    // Shared vertices count once within each section, never by min/max range.
    FRXSectionStats Section(const FStaticMeshLODResources& LOD, int32 SectionIndex);
}
