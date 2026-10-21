#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraModuleInputPathLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraModuleInputPathLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Module Input Paths", meta=(ScriptName="remove_niagara_unused_module_input_path"))
    static FString RemoveNiagaraUnusedModuleInputPath(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Module Input Paths", meta=(ScriptName="set_niagara_module_input_user_parameter"))
    static FString SetNiagaraModuleInputUserParameter(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Module Input Paths", meta=(ScriptName="set_niagara_array_data_interface_module_input_user_parameter"))
    static FString SetNiagaraArrayDataInterfaceModuleInputUserParameter(const FString& ParamsJson);
};
