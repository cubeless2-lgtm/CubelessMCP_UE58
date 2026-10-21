#include "CubelessChaosGeometryAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessChaosGeometryAuthoringApi.h"

FString UCubelessChaosGeometryAuthoringLibrary::CreateGeometryCollectionFromMesh(FString ParamsJson)
{
    return Cubeless::UE58::ChaosGeometryAuthoring::Create(ParamsJson);
}

FString UCubelessChaosGeometryAuthoringLibrary::ConfigureBuildingFracture(FString ParamsJson)
{
    return Cubeless::UE58::ChaosGeometryAuthoring::ConfigureFracture(ParamsJson);
}

FString UCubelessChaosGeometryAuthoringLibrary::ConfigureAnchorField(FString ParamsJson)
{
    return Cubeless::UE58::ChaosGeometryAuthoring::ConfigureAnchor(ParamsJson);
}
