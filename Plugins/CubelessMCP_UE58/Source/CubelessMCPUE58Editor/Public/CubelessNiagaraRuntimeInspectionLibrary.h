#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraRuntimeInspectionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraRuntimeInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Runtime Inspection", meta=(ScriptName="inspect_niagara_component_runtime_state"))
    static FString InspectNiagaraComponentRuntimeState(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Runtime Inspection", meta=(ScriptName="inspect_niagara_component_parameter_store"))
    static FString InspectNiagaraComponentParameterStore(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Runtime Inspection", meta=(ScriptName="inspect_niagara_component_grid2d_runtime"))
    static FString InspectNiagaraComponentGrid2DRuntime(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Runtime Inspection", meta=(ScriptName="sample_render_target2d_region_stats"))
    static FString SampleRenderTarget2DRegionStats(const FString& ParamsJson);
};
