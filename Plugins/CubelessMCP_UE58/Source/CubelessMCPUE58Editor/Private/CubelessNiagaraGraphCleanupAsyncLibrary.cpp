#include "CubelessNiagaraGraphCleanupAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraGraphCleanupAsyncApi.h"
UCubelessNiagaraGraphCleanupJob* UCubelessNiagaraGraphCleanupAsyncLibrary::StartRemoveNiagaraModulesJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraGraphCleanupAsync::StartModules(ParamsJson);}
UCubelessNiagaraGraphCleanupJob* UCubelessNiagaraGraphCleanupAsyncLibrary::StartRemoveNiagaraParameterMapSetNodesJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraGraphCleanupAsync::StartMapSetNodes(ParamsJson);}
UCubelessNiagaraGraphCleanupJob* UCubelessNiagaraGraphCleanupAsyncLibrary::StartRemoveNiagaraOrphanParameterMapGetNodesJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraGraphCleanupAsync::StartOrphanMapGetNodes(ParamsJson);}
UCubelessNiagaraGraphCleanupJob* UCubelessNiagaraGraphCleanupAsyncLibrary::StartRemoveNiagaraSimulationStageJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraGraphCleanupAsync::StartSimulationStage(ParamsJson);}
UCubelessNiagaraGraphCleanupJob* UCubelessNiagaraGraphCleanupAsyncLibrary::StartRemoveNiagaraScratchPadScriptJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraGraphCleanupAsync::StartScratchPadScript(ParamsJson);}
