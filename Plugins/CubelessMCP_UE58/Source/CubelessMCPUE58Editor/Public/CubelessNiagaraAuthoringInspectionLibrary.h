#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraAuthoringInspectionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraAuthoringInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Authoring Inspection", meta=(ScriptName="inspect_niagara_stack"))
    static FString InspectNiagaraStack(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Authoring Inspection", meta=(ScriptName="inspect_niagara_graph"))
    static FString InspectNiagaraGraph(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Authoring Inspection", meta=(ScriptName="inspect_niagara_graph_actions"))
    static FString InspectNiagaraGraphActions(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Authoring Inspection", meta=(ScriptName="inspect_niagara_custom_hlsl_nodes"))
    static FString InspectNiagaraCustomHlslNodes(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Authoring Inspection", meta=(ScriptName="inspect_niagara_scratch_pad_interface"))
    static FString InspectNiagaraScratchPadInterface(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Authoring Inspection", meta=(ScriptName="inspect_niagara_compile_status"))
    static FString InspectNiagaraCompileStatus(const FString& ParamsJson);
};
