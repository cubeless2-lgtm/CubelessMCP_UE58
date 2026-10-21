#include "CubelessChaosCacheSessionLibrary.h"
#include "Compatibility/UE58/CubelessChaosCacheSessionApi.h"

FString UCubelessChaosCacheSessionLibrary::RecordChaosCache(FString ParamsJson)
{
    return Cubeless::UE58::ChaosCacheSessions::Record(ParamsJson);
}

FString UCubelessChaosCacheSessionLibrary::ValidateChaosCache(FString ParamsJson)
{
    return Cubeless::UE58::ChaosCacheSessions::Validate(ParamsJson);
}
