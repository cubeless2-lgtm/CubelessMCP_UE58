#include "CubelessNiagaraParameterCollectionLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraParameterCollectionApi.h"

FString UCubelessNiagaraParameterCollectionLibrary::InspectNiagaraParameterCollectionValues(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraParameterCollections::InspectValues(ParamsJson);
}

FString UCubelessNiagaraParameterCollectionLibrary::SetNiagaraParameterCollectionValues(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraParameterCollections::SetValues(ParamsJson);
}

FString UCubelessNiagaraParameterCollectionLibrary::RepairNiagaraParameterCollection(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraParameterCollections::RepairDefaultStore(ParamsJson);
}

FString UCubelessNiagaraParameterCollectionLibrary::RemapNiagaraParameterCollectionReferences(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraParameterCollections::RemapSystem(ParamsJson);
}

FString UCubelessNiagaraParameterCollectionLibrary::RemapNiagaraScriptParameterCollectionReferences(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraParameterCollections::RemapScripts(ParamsJson);
}

FString UCubelessNiagaraParameterCollectionLibrary::RecompileNiagaraScriptsInRoot(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraParameterCollections::RecompileScripts(ParamsJson);
}
