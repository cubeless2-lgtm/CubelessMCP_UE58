#pragma once
#include "CoreMinimal.h"
class UCubelessNiagaraEmitterEventAuthoringJob;
namespace Cubeless::UE58::NiagaraEmitterEventAuthoringAsync
{
    enum class EJobKind : uint8 { PersistentIds, EventHandler, SimulationStageAdd, SimulationStageSettings };
    struct FJobAccess;
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEmitterEventAuthoringJob* StartPersistentIds(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEmitterEventAuthoringJob* StartEventHandler(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEmitterEventAuthoringJob* StartSimulationStageAdd(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEmitterEventAuthoringJob* StartSimulationStageSettings(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraEmitterEventAuthoringJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraEmitterEventAuthoringJob* Job, const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
    // Internal cache publication for the existing concrete emitter/event/stage controllers only.
    CUBELESSMCPUE58EDITOR_API void RegisterJob(UCubelessNiagaraEmitterEventAuthoringJob* Job, EJobKind Kind, const FGuid& JobId);
    CUBELESSMCPUE58EDITOR_API void Publish(UCubelessNiagaraEmitterEventAuthoringJob* Job, const FString& Snapshot, bool bTerminal, bool bObjectPublicationSafe);
    CUBELESSMCPUE58EDITOR_API bool IsStopping();
}
