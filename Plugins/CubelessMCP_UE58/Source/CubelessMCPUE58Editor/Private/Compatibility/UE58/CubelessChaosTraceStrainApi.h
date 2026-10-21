#pragma once
#include "CoreMinimal.h"

class UWorld;

namespace Cubeless::UE58::ChaosTraceStrain
{
FString Trigger(const FString& ParamsJson);
// Stop owned ticker before guarded cleanup; sent physics commands are not cancelled or rolled back.
void Shutdown();
// Editor-internal metadata-only lease query; no MCP tool or SDK getter.
bool HasActiveSessionForWorld(const UWorld* World);
}
