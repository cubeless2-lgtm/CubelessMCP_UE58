#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations::ControlRigPresetRecipes
{
    // Original JSON-only preset generation. Returns the original plain legacy report, not an asset operation.
    FString MakePresetRecipeJson(const FString& PresetJson);
}
