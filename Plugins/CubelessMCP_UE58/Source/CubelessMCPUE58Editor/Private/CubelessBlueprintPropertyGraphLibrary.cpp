#include "CubelessBlueprintPropertyGraphLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintPropertyGraphApi.h"

FString UCubelessBlueprintPropertyGraphLibrary::SetBlueprintProperty(const FString& ParamsJson)
{
    return Cubeless::UE58::BlueprintPropertyGraph::SetProperty(ParamsJson);
}

FString UCubelessBlueprintPropertyGraphLibrary::ListBlueprintGraphs(const FString& ParamsJson)
{
    return Cubeless::UE58::BlueprintPropertyGraph::ListGraphs(ParamsJson);
}
