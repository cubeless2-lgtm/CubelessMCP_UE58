#include "CubelessNiagaraInputParameterLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraInputParameterApi.h"

FString UCubelessNiagaraInputParameterLibrary::InspectNiagaraRapidIterationParameters(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraInputParameters::InspectRapidIteration(ParamsJson);
}

FString UCubelessNiagaraInputParameterLibrary::SetNiagaraRapidIterationParameter(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraInputParameters::SetRapidIteration(ParamsJson);
}

FString UCubelessNiagaraInputParameterLibrary::SetNiagaraModuleStaticSwitch(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraInputParameters::SetStaticSwitch(ParamsJson);
}

FString UCubelessNiagaraInputParameterLibrary::InspectNiagaraModuleInputs(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraInputParameters::InspectModuleInputs(ParamsJson);
}

FString UCubelessNiagaraInputParameterLibrary::InspectNiagaraDataInterfaceOverrides(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraInputParameters::InspectDataInterfaces(ParamsJson);
}

FString UCubelessNiagaraInputParameterLibrary::SetNiagaraModuleInputValue(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraInputParameters::SetInputValue(ParamsJson);
}

FString UCubelessNiagaraInputParameterLibrary::CreateNiagaraModuleInputOverride(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraInputParameters::CreateInputOverride(ParamsJson);
}
