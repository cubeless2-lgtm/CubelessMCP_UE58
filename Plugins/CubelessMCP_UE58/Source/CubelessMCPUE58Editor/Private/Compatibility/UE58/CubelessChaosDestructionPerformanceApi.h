#pragma once
#include "CoreMinimal.h"
class UWorld;

namespace Cubeless::UE58::ChaosDestructionPerformance
{
FString Run(const FString& ParamsJson);
void Shutdown();
// Editor-internal weak/session metadata only. This is not a reflected MCP tool.
bool HasActiveSessionForWorld(const UWorld* World);
}
