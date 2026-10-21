#pragma once
#include "CoreMinimal.h"
class UCubelessNiagaraGraphCleanupJob;
namespace Cubeless::UE58::NiagaraGraphCleanupAsync
{
    struct FJobState;
    // Finite original cleanup5 only. This is not a generic operation executor.
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraGraphCleanupJob* StartModules(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraGraphCleanupJob* StartMapSetNodes(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraGraphCleanupJob* StartOrphanMapGetNodes(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraGraphCleanupJob* StartSimulationStage(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraGraphCleanupJob* StartScratchPadScript(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraGraphCleanupJob* Job, const FString& Reason);
    // Retained plain snapshot lookup; no new target projection, start, save or mutation.
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraGraphCleanupJob* Job);
    // ModuleShutdown before registration/converter teardown. No asset restoration/compiler cancellation.
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
}
