#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraRenderers
{
FString Inspect(const FString& ParamsJson);
FString SetMaterial(const FString& ParamsJson);
FString SetSpriteSettings(const FString& ParamsJson);
FString AddSprite(const FString& ParamsJson);
FString AddMesh(const FString& ParamsJson);
FString SetEnabled(const FString& ParamsJson);
FString Cleanup(const FString& ParamsJson);
}
