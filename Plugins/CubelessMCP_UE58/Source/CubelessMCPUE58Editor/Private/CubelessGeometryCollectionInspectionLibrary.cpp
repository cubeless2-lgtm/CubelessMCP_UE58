#include "CubelessGeometryCollectionInspectionLibrary.h"
#include "Compatibility/UE58/CubelessGeometryCollectionInspectionApi.h"

FString UCubelessGeometryCollectionInspectionLibrary::InspectGeometryCollectionPerformanceGuarded(FString ParamsJson)
{
    return Cubeless::UE58::GeometryCollectionInspection::InspectPerformance(ParamsJson);
}
FString UCubelessGeometryCollectionInspectionLibrary::InspectGeometryCollectionRuntimeState(FString ParamsJson)
{
    return Cubeless::UE58::GeometryCollectionInspection::InspectRuntime(ParamsJson);
}
