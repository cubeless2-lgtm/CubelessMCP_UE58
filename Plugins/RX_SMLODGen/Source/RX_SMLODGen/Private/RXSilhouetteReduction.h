#pragma once
#include "MeshDescription.h"
#include "RXSectionReduction.h"

namespace RXSilhouetteReduction
{
    // Constrain only the selected contour categories while simplifying.
    bool Reduce(const FMeshDescription& Source, float KeepFraction, const FRXSectionWeight& Options, FMeshDescription& Output, FString& Error);
}
