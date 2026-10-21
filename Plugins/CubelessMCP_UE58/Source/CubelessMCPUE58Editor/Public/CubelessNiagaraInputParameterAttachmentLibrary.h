#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraInputParameterAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraInputParameterAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_input_parameters_toolset"))
    static FString InstallNiagaraInputParameterToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_input_parameters_toolset"))
    static FString VerifyNiagaraInputParameterToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_input_parameters_toolset"))
    static FString RemoveNiagaraInputParameterToolset(UClass* ToolsetClass);
};
