#include "CubelessLegacyChaosClothingLibrary.h"
#include "Compatibility/UE58/CubelessLegacyChaosClothingApi.h"

FString UCubelessLegacyChaosClothingLibrary::CreateLegacyChaosClothingFromSkeletalSections(FString ParamsJson)
{
    return Cubeless::UE58::LegacyChaosClothing::CreateFromSections(ParamsJson);
}
