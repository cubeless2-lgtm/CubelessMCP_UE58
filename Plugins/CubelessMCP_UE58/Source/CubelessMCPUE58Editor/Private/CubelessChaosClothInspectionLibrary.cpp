#include "CubelessChaosClothInspectionLibrary.h"
#include "Compatibility/UE58/CubelessChaosClothInspectionApi.h"
FString UCubelessChaosClothInspectionLibrary::InspectChaosClothAsset(FString ParamsJson)
{ return Cubeless::UE58::ChaosClothInspection::InspectAsset(ParamsJson); }
FString UCubelessChaosClothInspectionLibrary::InspectChaosClothSimulationMesh(FString ParamsJson)
{ return Cubeless::UE58::ChaosClothInspection::InspectSimulationMesh(ParamsJson); }
