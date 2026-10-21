#include "CubelessNiagaraFactoryBootstrapLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraFactoryBootstrapApi.h"
FString UCubelessNiagaraFactoryBootstrapLibrary::InspectNiagaraFactoryBootstrapDefaults(const FString& ParamsJson){return Cubeless::UE58::NiagaraFactoryBootstrap::InspectDefaults(ParamsJson);}
FString UCubelessNiagaraFactoryBootstrapLibrary::CreateEmptyNiagaraSystem(const FString& ParamsJson){return Cubeless::UE58::NiagaraFactoryBootstrap::CreateEmptySystem(ParamsJson);}
FString UCubelessNiagaraFactoryBootstrapLibrary::CreateEmptyNiagaraEmitter(const FString& ParamsJson){return Cubeless::UE58::NiagaraFactoryBootstrap::CreateEmptyEmitter(ParamsJson);}
FString UCubelessNiagaraFactoryBootstrapLibrary::CreateBasicSpriteNiagaraSystem(const FString& ParamsJson){return Cubeless::UE58::NiagaraFactoryBootstrap::CreateBasicSpriteSystem(ParamsJson);}
