#pragma once
#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::Pawn
{
Variable::FResult SetProperties(const FString& BlueprintPath, const FString& AutoPossessPlayer,
    const FString& YawJson, const FString& PitchJson, const FString& RollJson, const FString& CanBeDamagedJson);
}
