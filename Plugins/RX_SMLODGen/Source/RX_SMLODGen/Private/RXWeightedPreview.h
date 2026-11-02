#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "RXSectionReduction.h"
#include "UObject/StrongObjectPtr.h"

namespace RXWeightedPreview
{
    // Builds a display-only duplicate. Source settings and source mesh are not
    // modified; the resulting custom LOD contains already reduced geometry.
    bool Build(UStaticMesh* Source, int32 TargetLOD, const TArray<FRXSectionWeight>& Weights,
        TStrongObjectPtr<UStaticMesh>& Output, TArray<FRXSectionReductionResult>& Results, FString& Error);
}
