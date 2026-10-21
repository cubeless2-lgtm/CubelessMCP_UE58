#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::ContentReconstruction
{
FString Recreate(const FString& ParamsJson);
FString Postprocess(const FString& ParamsJson);
FString RepairWorld(const FString& ParamsJson);
FString RunValidationPipeline(const FString& ParamsJson);
}
