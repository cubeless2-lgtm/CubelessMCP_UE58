#include "CubelessChaosLearningLibrary.h"
#include "Compatibility/UE58/CubelessChaosLearningApi.h"

FString UCubelessChaosLearningLibrary::CreateChaosDestructionLearningFixture(const FString& ParamsJson)
{
    return Cubeless::UE58::ChaosLearning::CreateFixture(ParamsJson);
}

FString UCubelessChaosLearningLibrary::RunChaosCacheSmoke(const FString& ParamsJson)
{
    return Cubeless::UE58::ChaosLearning::RunCacheSmoke(ParamsJson);
}
