#pragma once
#include "CoreMinimal.h"
class UCubelessNiagaraModuleInputsBatchJob;
namespace Cubeless::UE58::NiagaraModuleInputsBatchAsync
{
    struct FJobState;
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraModuleInputsBatchJob* StartBatch(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraModuleInputsBatchJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraModuleInputsBatchJob* Job, const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
}
