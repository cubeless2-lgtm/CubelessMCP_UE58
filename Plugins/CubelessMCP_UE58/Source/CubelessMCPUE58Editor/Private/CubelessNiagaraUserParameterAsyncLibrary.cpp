#include "CubelessNiagaraUserParameterAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraUserParameterAsyncApi.h"
UCubelessNiagaraUserParameterJob* UCubelessNiagaraUserParameterAsyncLibrary::StartRemoveUserParametersJob(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraUserParameterAsync::Start(ParamsJson);
}
