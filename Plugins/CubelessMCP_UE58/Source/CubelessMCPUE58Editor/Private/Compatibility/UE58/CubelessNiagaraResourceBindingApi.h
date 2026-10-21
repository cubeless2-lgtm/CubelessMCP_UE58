#pragma once

#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraResourceBindings
{
    FString SetRenderTarget2D(const FString& ParamsJson);
    FString SetTexture2D(const FString& ParamsJson);
    FString SetScratchDefault(const FString& ParamsJson);
    FString LinkScratchUser(const FString& ParamsJson);
}

class UCubelessNiagaraResourceBindingJob;
namespace Cubeless::UE58::NiagaraResourceBindingAsync
{
    struct FJobState;
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraResourceBindingJob* StartRenderTargetInput(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraResourceBindingJob* StartTextureInput(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraResourceBindingJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraResourceBindingJob* Job, const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
}
