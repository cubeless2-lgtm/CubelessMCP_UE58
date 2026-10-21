#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::GASAuthoring
{
FString Inspect(const FString& ParamsJson);
FString CreateAbility(const FString& ParamsJson);
FString CreateEffect(const FString& ParamsJson);
FString CreateAttributeSet(const FString& ParamsJson);
FString AddAbilitySystemComponent(const FString& ParamsJson);
FString AddGameplayAttribute(const FString& ParamsJson);
}
