#pragma once
#include "CoreMinimal.h"
class UCubelessNiagaraUserParameterJob;
namespace Cubeless::UE58::NiagaraUserParameterAsync
{
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraUserParameterJob* Start(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraUserParameterJob* Job, const FString& Reason);
    // ModuleShutdown must call this before registration/converter teardown. No save/undo/compiler cancellation.
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
    CUBELESSMCPUE58EDITOR_API FString StatusJson();
}
