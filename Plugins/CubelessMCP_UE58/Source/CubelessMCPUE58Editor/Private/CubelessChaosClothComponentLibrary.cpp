#include "CubelessChaosClothComponentLibrary.h"
#include "Compatibility/UE58/CubelessChaosClothComponentApi.h"

FString UCubelessChaosClothComponentLibrary::InspectChaosClothComponent(FString ParamsJson)
{
    return Cubeless::UE58::ChaosClothComponent::Inspect(ParamsJson);
}

FString UCubelessChaosClothComponentLibrary::ConfigureChaosClothComponent(FString ParamsJson)
{
    return Cubeless::UE58::ChaosClothComponent::Configure(ParamsJson);
}
