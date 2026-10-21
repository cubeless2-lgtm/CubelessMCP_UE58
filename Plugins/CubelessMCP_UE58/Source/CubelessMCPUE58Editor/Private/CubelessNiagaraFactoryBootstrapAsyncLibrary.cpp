#include "CubelessNiagaraFactoryBootstrapAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraFactoryBootstrapApi.h"
UCubelessNiagaraFactoryBootstrapJob* UCubelessNiagaraFactoryBootstrapAsyncLibrary::StartCreateEmptyNiagaraSystemJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraFactoryBootstrapAsync::StartEmptySystem(ParamsJson);}
UCubelessNiagaraFactoryBootstrapJob* UCubelessNiagaraFactoryBootstrapAsyncLibrary::StartCreateBasicSpriteNiagaraSystemJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraFactoryBootstrapAsync::StartBasicSpriteSystem(ParamsJson);}
