#include "CubelessNiagaraAuthoringInspectionLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraAuthoringInspectionApi.h"

FString UCubelessNiagaraAuthoringInspectionLibrary::InspectNiagaraStack(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraAuthoringInspection::InspectStack(ParamsJson);
}

FString UCubelessNiagaraAuthoringInspectionLibrary::InspectNiagaraGraph(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraAuthoringInspection::InspectGraph(ParamsJson);
}

FString UCubelessNiagaraAuthoringInspectionLibrary::InspectNiagaraGraphActions(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraAuthoringInspection::InspectGraphActions(ParamsJson);
}

FString UCubelessNiagaraAuthoringInspectionLibrary::InspectNiagaraCustomHlslNodes(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraAuthoringInspection::InspectCustomHlsl(ParamsJson);
}

FString UCubelessNiagaraAuthoringInspectionLibrary::InspectNiagaraScratchPadInterface(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraAuthoringInspection::InspectScratchInterface(ParamsJson);
}

FString UCubelessNiagaraAuthoringInspectionLibrary::InspectNiagaraCompileStatus(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraAuthoringInspection::InspectCompileStatus(ParamsJson);
}
