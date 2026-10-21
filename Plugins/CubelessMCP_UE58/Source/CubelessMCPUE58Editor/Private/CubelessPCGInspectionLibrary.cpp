#include "CubelessPCGInspectionLibrary.h"
#include "Compatibility/UE58/CubelessPCGInspectionApi.h"

FString UCubelessPCGInspectionLibrary::ResolvePcgGraph(const FString& ParamsJson)
{
    return Cubeless::UE58::PCGInspection::ResolveGraph(ParamsJson);
}

FString UCubelessPCGInspectionLibrary::ListPcgGraphNodes(const FString& ParamsJson)
{
    return Cubeless::UE58::PCGInspection::ListNodes(ParamsJson);
}

FString UCubelessPCGInspectionLibrary::InspectPcgCustomHlslNodes(const FString& ParamsJson)
{
    return Cubeless::UE58::PCGInspection::InspectCustomHlsl(ParamsJson);
}

FString UCubelessPCGInspectionLibrary::InspectPcgComponentOutputs(const FString& ParamsJson)
{
    return Cubeless::UE58::PCGInspection::InspectComponentOutputs(ParamsJson);
}
