#include "CubelessChaosCinematicSequenceLibrary.h"
#include "Compatibility/UE58/CubelessChaosCinematicSequenceApi.h"

FString UCubelessChaosCinematicSequenceLibrary::CreateChaosLevelSequence(FString ParamsJson)
{
    return Cubeless::UE58::ChaosCinematicSequence::CreateSequence(ParamsJson);
}

FString UCubelessChaosCinematicSequenceLibrary::BindChaosCacheToSequence(FString ParamsJson)
{
    return Cubeless::UE58::ChaosCinematicSequence::BindCache(ParamsJson);
}

FString UCubelessChaosCinematicSequenceLibrary::ConfigureChaosSequenceTiming(FString ParamsJson)
{
    return Cubeless::UE58::ChaosCinematicSequence::ConfigureTiming(ParamsJson);
}

FString UCubelessChaosCinematicSequenceLibrary::InspectChaosCinematicPipeline(FString ParamsJson)
{
    return Cubeless::UE58::ChaosCinematicSequence::InspectPipeline(ParamsJson);
}
