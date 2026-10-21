#pragma once
#include "CoreMinimal.h"

class UWorld;

namespace Cubeless::UE58::ChaosLearning
{
FString CreateFixture(const FString& ParamsJson);
FString RunCacheSmoke(const FString& ParamsJson);
void Shutdown();
// Editor-internal metadata-only lease query; no MCP tool or SDK getter.
bool HasActiveSessionForWorld(const UWorld* World);
}
