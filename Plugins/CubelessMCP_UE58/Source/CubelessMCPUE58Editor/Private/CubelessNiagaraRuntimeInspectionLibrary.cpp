#include "CubelessNiagaraRuntimeInspectionLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraRuntimeInspectionApi.h"

FString UCubelessNiagaraRuntimeInspectionLibrary::InspectNiagaraComponentRuntimeState(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRuntimeInspection::InspectRuntimeState(ParamsJson);
}

FString UCubelessNiagaraRuntimeInspectionLibrary::InspectNiagaraComponentParameterStore(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRuntimeInspection::InspectParameterStore(ParamsJson);
}

FString UCubelessNiagaraRuntimeInspectionLibrary::InspectNiagaraComponentGrid2DRuntime(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRuntimeInspection::InspectGrid2DRuntime(ParamsJson);
}

FString UCubelessNiagaraRuntimeInspectionLibrary::SampleRenderTarget2DRegionStats(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRuntimeInspection::SampleRenderTargetRegionStats(ParamsJson);
}
