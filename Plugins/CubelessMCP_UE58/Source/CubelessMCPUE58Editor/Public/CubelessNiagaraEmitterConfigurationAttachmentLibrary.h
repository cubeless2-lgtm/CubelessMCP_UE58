#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraEmitterConfigurationAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEmitterConfigurationAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_emitter_configuration_toolset"))
    static FString InstallNiagaraEmitterConfigurationToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_emitter_configuration_toolset"))
    static FString VerifyNiagaraEmitterConfigurationToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_emitter_configuration_toolset"))
    static FString RemoveNiagaraEmitterConfigurationToolset(UClass* ToolsetClass);
};
