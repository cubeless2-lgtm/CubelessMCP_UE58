#include "CubelessChaosDestructionPerformanceLibrary.h"
#include "Compatibility/UE58/CubelessChaosDestructionPerformanceApi.h"
FString UCubelessChaosDestructionPerformanceLibrary::RunChaosDestructionPerformanceSmoke(const FString& ParamsJson)
{
    return Cubeless::UE58::ChaosDestructionPerformance::Run(ParamsJson);
}
