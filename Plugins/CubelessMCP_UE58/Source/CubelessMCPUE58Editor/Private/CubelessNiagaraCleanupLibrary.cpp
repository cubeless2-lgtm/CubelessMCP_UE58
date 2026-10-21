#include "CubelessNiagaraCleanupLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraCleanupApi.h"

FString UCubelessNiagaraCleanupLibrary::RemoveNiagaraModules(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCleanup::RemoveModules(ParamsJson);
}

FString UCubelessNiagaraCleanupLibrary::RemoveNiagaraParameterMapSetNodes(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCleanup::RemoveMapSetNodes(ParamsJson);
}

FString UCubelessNiagaraCleanupLibrary::RemoveNiagaraOrphanParameterMapGetNodes(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCleanup::RemoveOrphanMapGetNodes(ParamsJson);
}

FString UCubelessNiagaraCleanupLibrary::RemoveNiagaraUserParameters(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCleanup::RemoveUserParameters(ParamsJson);
}

FString UCubelessNiagaraCleanupLibrary::RemoveNiagaraSimulationStage(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCleanup::RemoveSimulationStage(ParamsJson);
}

FString UCubelessNiagaraCleanupLibrary::RemoveNiagaraScratchPadScript(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCleanup::RemoveScratchPadScript(ParamsJson);
}
