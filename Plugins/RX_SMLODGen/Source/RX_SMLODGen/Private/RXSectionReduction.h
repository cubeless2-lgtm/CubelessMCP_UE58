#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"
#include "MeshReductionSettings.h"

// MeshDescription polygon group IDs are source-local. Keep these records alongside
// the source mesh; a reimport or a different base LOD invalidates the mapping.
struct FRXSectionWeight
{
    FPolygonGroupID Group;
    // Preservation strength: 0 = normal reduction, 1 = keep the source section.
    float Weight = 0.0f;
    bool bExcluded = false;
    bool bPreserveOpenBoundary = false;
    bool bPreserveSharpEdges = false;
    bool bPreserveAttributeSeams = false;
    bool HasProtection() const { return bPreserveOpenBoundary || bPreserveSharpEdges || bPreserveAttributeSeams; }
    bool HasSameProtection(const FRXSectionWeight& Other) const
    { return bPreserveOpenBoundary == Other.bPreserveOpenBoundary && bPreserveSharpEdges == Other.bPreserveSharpEdges && bPreserveAttributeSeams == Other.bPreserveAttributeSeams; }
};

struct FRXSectionReductionResult
{
    FPolygonGroupID SourceGroup;
    int32 InputTriangles = 0;
    int32 OutputTriangles = 0;
    float TargetKeepFraction = 1.0f;
    bool bGlobalTarget = false;
};

namespace RXSectionReduction
{
    // Does not modify the source or output on failure. Different polygon groups
    // with different weights touching at identical positions are rejected until
    // shared boundaries can be constrained across independent reductions.
    bool Reduce(const FMeshDescription& Source, const FMeshReductionSettings& Settings,
        const TArray<FRXSectionWeight>& Sections, FMeshDescription& Output,
        TArray<FRXSectionReductionResult>& Results, FString& Error);
}
