#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::ControlRigRecipeAuthoring
{
    FString ValidateRecipe(const FString& ParamsJson);
    FString DryRunRecipe(const FString& ParamsJson);
    FString BuildRecipe(const FString& ParamsJson);
}
