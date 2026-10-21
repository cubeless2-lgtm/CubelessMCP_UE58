#include "CubelessPCGContractAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessPCGContractAuthoringApi.h"

FString UCubelessPCGContractAuthoringLibrary::AuditPCGGraphContract(const FString& ParamsJson)
{
    return Cubeless::UE58::PCGContractAuthoring::AuditGraphContract(ParamsJson);
}

FString UCubelessPCGContractAuthoringLibrary::SetPCGStaticMeshSpawnerEntries(const FString& ParamsJson)
{
    return Cubeless::UE58::PCGContractAuthoring::SetSpawnerEntries(ParamsJson);
}
