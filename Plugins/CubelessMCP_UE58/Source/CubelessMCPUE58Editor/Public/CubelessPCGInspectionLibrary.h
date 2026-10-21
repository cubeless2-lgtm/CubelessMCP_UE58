#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGInspectionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPCGInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|PCG Inspection", meta=(ScriptName="resolve_pcg_graph"))
    static FString ResolvePcgGraph(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|PCG Inspection", meta=(ScriptName="list_pcg_graph_nodes"))
    static FString ListPcgGraphNodes(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|PCG Inspection", meta=(ScriptName="inspect_pcg_custom_hlsl_nodes"))
    static FString InspectPcgCustomHlslNodes(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|PCG Inspection", meta=(ScriptName="inspect_pcg_component_outputs"))
    static FString InspectPcgComponentOutputs(const FString& ParamsJson);
};
