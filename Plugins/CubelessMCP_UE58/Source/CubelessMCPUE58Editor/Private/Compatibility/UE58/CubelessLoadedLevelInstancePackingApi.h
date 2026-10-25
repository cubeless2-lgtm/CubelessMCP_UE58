#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58
{
// Editor game-thread only. Exact existing source/target actors; no creation or implicit load.
FString PackLoadedLevelInstance(const FString& RequestJson);
}
