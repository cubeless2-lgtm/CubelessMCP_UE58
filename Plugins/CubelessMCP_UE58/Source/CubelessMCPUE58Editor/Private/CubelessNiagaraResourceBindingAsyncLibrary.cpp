#include "CubelessNiagaraResourceBindingAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraResourceBindingApi.h"
UCubelessNiagaraResourceBindingJob* UCubelessNiagaraResourceBindingAsyncLibrary::StartSetNiagaraRenderTarget2DModuleInputJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraResourceBindingAsync::StartRenderTargetInput(ParamsJson);}
UCubelessNiagaraResourceBindingJob* UCubelessNiagaraResourceBindingAsyncLibrary::StartSetNiagaraTexture2DModuleInputJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraResourceBindingAsync::StartTextureInput(ParamsJson);}
