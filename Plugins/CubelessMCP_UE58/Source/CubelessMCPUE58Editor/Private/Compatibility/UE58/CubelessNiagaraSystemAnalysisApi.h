#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraSystemAnalysis
{
    // Original System load/canonical path/count plus SDK TryGet option normalization; no aggregate/compile/save.
    FString InspectMetadata(const FString& ParamsJson);
}
