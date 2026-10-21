#include "CubelessNiagaraModuleInputsBatchAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraModuleInputsBatchAsyncApi.h"
UCubelessNiagaraModuleInputsBatchJob* UCubelessNiagaraModuleInputsBatchAsyncLibrary::StartSetNiagaraModuleInputsBatchJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraModuleInputsBatchAsync::StartBatch(ParamsJson);}
