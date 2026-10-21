#include "CubelessNiagaraEmitterConfigurationLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraEmitterConfigurationApi.h"

FString UCubelessNiagaraEmitterConfigurationLibrary::InspectNiagaraEmitterProperties(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraEmitterConfiguration::Inspect(ParamsJson);
}

FString UCubelessNiagaraEmitterConfigurationLibrary::SetNiagaraEmitterEnabled(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraEmitterConfiguration::SetEnabled(ParamsJson);
}

FString UCubelessNiagaraEmitterConfigurationLibrary::SetNiagaraEmitterLocalSpace(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraEmitterConfiguration::SetLocalSpace(ParamsJson);
}

FString UCubelessNiagaraEmitterConfigurationLibrary::SetNiagaraEmitterSimTarget(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraEmitterConfiguration::SetSimTarget(ParamsJson);
}

FString UCubelessNiagaraEmitterConfigurationLibrary::SetNiagaraEmitterFixedBounds(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraEmitterConfiguration::SetFixedBounds(ParamsJson);
}

FString UCubelessNiagaraEmitterConfigurationLibrary::SetNiagaraEmitterPersistentIds(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraEmitterConfiguration::SetPersistentIds(ParamsJson);
}

FString UCubelessNiagaraEmitterConfigurationLibrary::RemoveNiagaraEmitter(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraEmitterConfiguration::Remove(ParamsJson);
}
