#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::ChaosSolverSettings
{
Variable::FResult Inspect(const FString& ParamsJson);
Variable::FResult Configure(const FString& ParamsJson);
// Call on the game thread before Editor module teardown. No blocking physics wait is performed.
void Shutdown();
}
