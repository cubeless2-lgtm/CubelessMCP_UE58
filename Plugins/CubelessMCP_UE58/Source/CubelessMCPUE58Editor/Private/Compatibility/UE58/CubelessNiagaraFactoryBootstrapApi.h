#pragma once

#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraFactoryBootstrap
{
    FString InspectDefaults(const FString& ParamsJson);
    FString CreateEmptySystem(const FString& ParamsJson);
    FString CreateEmptyEmitter(const FString& ParamsJson);
    FString CreateBasicSpriteSystem(const FString& ParamsJson);
}

class UCubelessNiagaraFactoryBootstrapJob;
namespace Cubeless::UE58::NiagaraFactoryBootstrapAsync
{
    struct FJobState;
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraFactoryBootstrapJob* StartEmptySystem(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraFactoryBootstrapJob* StartBasicSpriteSystem(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraFactoryBootstrapJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraFactoryBootstrapJob* Job, const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
}
