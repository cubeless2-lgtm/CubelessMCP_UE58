#include "CubelessNiagaraModuleInputPathLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraModuleInputPathApi.h"

FString UCubelessNiagaraModuleInputPathLibrary::RemoveNiagaraUnusedModuleInputPath(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraModuleInputPaths::RemoveUnusedPath(ParamsJson);
}

FString UCubelessNiagaraModuleInputPathLibrary::SetNiagaraModuleInputUserParameter(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraModuleInputPaths::BindUserParameter(ParamsJson);
}

FString UCubelessNiagaraModuleInputPathLibrary::SetNiagaraArrayDataInterfaceModuleInputUserParameter(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraModuleInputPaths::BindArrayUserParameter(ParamsJson);
}
