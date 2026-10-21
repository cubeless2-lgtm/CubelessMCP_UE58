#include "CubelessNiagaraEventStageLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraEventStageApi.h"

FString UCubelessNiagaraEventStageLibrary::InspectNiagaraSimulationStages(const FString& ParamsJson){return Cubeless::UE58::NiagaraEventStage::InspectSimulationStages(ParamsJson);}
FString UCubelessNiagaraEventStageLibrary::InspectNiagaraEventHandlers(const FString& ParamsJson){return Cubeless::UE58::NiagaraEventStage::InspectEventHandlers(ParamsJson);}
FString UCubelessNiagaraEventStageLibrary::AddNiagaraEventHandler(const FString& ParamsJson){return Cubeless::UE58::NiagaraEventStage::AddEventHandler(ParamsJson);}
