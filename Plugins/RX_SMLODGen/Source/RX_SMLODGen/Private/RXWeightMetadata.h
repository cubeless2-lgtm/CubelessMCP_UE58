#pragma once

#include "CoreMinimal.h"
#include "MeshReductionSettings.h"
#include "RXSectionReduction.h"

struct FRXStoredLODWeights
{
    FMeshReductionSettings Settings;
    TArray<FRXSectionWeight> Sections;
};

namespace RXWeightMetadata
{
    bool Encode(const FString& SourceId, const TMap<int32, FRXStoredLODWeights>& LODs,
        int32 ImpostorLOD, int32 ImpostorMaterialSlot, FString& OutJson, FString& Error);
    bool Decode(const FString& Json, FString& OutSourceId, TMap<int32, FRXStoredLODWeights>& OutLODs,
        int32& OutImpostorLOD, int32& OutImpostorMaterialSlot, FString& Error);
    const TCHAR* Key();
}
