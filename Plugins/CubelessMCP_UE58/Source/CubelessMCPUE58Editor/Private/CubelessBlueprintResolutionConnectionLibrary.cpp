#include "CubelessBlueprintResolutionConnectionLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintResolutionConnectionApi.h"

FString UCubelessBlueprintResolutionConnectionLibrary::ConnectBlueprintGraphPins(const FString& ParamsJson)
{
    return Cubeless::UE58::BlueprintResolutionConnection::ConnectGraphPins(ParamsJson);
}

FString UCubelessBlueprintResolutionConnectionLibrary::ResolveBlueprint(const FString& ParamsJson)
{
    return Cubeless::UE58::BlueprintResolutionConnection::Resolve(ParamsJson);
}

FString UCubelessBlueprintResolutionConnectionLibrary::ConnectBlueprintNodes(const FString& ParamsJson)
{
    return Cubeless::UE58::BlueprintResolutionConnection::Connect(ParamsJson);
}
