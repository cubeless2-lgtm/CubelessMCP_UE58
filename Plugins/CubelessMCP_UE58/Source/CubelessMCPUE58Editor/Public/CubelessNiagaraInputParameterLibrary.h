#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraInputParameterLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraInputParameterLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Input Parameters", meta=(ScriptName="inspect_niagara_rapid_iteration_parameters"))
    static FString InspectNiagaraRapidIterationParameters(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Input Parameters", meta=(ScriptName="set_niagara_rapid_iteration_parameter"))
    static FString SetNiagaraRapidIterationParameter(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Input Parameters", meta=(ScriptName="set_niagara_module_static_switch"))
    static FString SetNiagaraModuleStaticSwitch(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Input Parameters", meta=(ScriptName="inspect_niagara_module_inputs"))
    static FString InspectNiagaraModuleInputs(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Input Parameters", meta=(ScriptName="inspect_niagara_data_interface_overrides"))
    static FString InspectNiagaraDataInterfaceOverrides(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Input Parameters", meta=(ScriptName="set_niagara_module_input_value"))
    static FString SetNiagaraModuleInputValue(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Input Parameters", meta=(ScriptName="create_niagara_module_input_override"))
    static FString CreateNiagaraModuleInputOverride(const FString& ParamsJson);
};
