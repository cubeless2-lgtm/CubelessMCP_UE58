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
        int32 ImpostorLOD, int32 ImpostorMaterialSlot, FString& OutJson, FString& Error, int32 ImpostorDirections = 4);
    bool Decode(const FString& Json, FString& OutSourceId, TMap<int32, FRXStoredLODWeights>& OutLODs,
        int32& OutImpostorLOD, int32& OutImpostorMaterialSlot, FString& Error, int32* OutImpostorDirections = nullptr);
    const TCHAR* Key();
}
