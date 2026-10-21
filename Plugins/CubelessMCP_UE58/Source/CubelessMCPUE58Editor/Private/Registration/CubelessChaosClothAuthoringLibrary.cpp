#include "CubelessChaosClothAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessChaosClothAuthoringApi.h"
FString UCubelessChaosClothAuthoringLibrary::ConfigureChaosClothDataflowNode(FString ParamsJson)
{ return Cubeless::UE58::ChaosClothAuthoring::ConfigureNode(ParamsJson); }
FString UCubelessChaosClothAuthoringLibrary::ConfigureChaosClothWeightMap(FString ParamsJson)
{ return Cubeless::UE58::ChaosClothAuthoring::ConfigureWeightMap(ParamsJson); }
