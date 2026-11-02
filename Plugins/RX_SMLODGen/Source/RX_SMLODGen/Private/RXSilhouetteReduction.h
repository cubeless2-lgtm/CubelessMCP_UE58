#pragma once
#include "MeshDescription.h"

namespace RXSilhouetteReduction
{
    // Constrain geometric contours, simplify only their planar interiors.
    bool Reduce(const FMeshDescription& Source, float KeepFraction, FMeshDescription& Output, FString& Error);
}
