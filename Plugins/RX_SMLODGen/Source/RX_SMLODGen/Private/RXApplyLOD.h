#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "RXSectionReduction.h"
struct FRXImpostorPreview;

namespace RXApplyLOD
{
    // Fingerprint the serialized in-memory source; used to reject stale previews.
    bool Fingerprint(UStaticMesh* Mesh, FString& OutHash, FString& Error);

    // Mutates the original only after all requested weighted LODs have built.
    // Does not save. bMutationStarted remains true on any later failure.
    bool Apply(UStaticMesh* Original, UStaticMesh* Preview, const FString& BaselineHash,
        const TMap<int32, TArray<FRXSectionWeight>>& Weights, bool bIncludeImpostor,
        bool& bMutationStarted, TArray<FString>& OutCreatedPackages, FString& Error, const FRXImpostorPreview* PreparedImpostor = nullptr, int32 ImpostorDirections = 4);

    // Restores requested settings to a transient preview after reading metadata.
    bool RestorePreviewSettings(UStaticMesh* Original, UStaticMesh* Preview,
        TMap<int32, TArray<FRXSectionWeight>>& OutWeights, bool& bOutIncludeImpostor, FString& Error, int32* OutImpostorDirections = nullptr);
}
