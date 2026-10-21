#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraResourceBindingAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraResourceBindingAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_resource_bindings_toolset"))
    static FString InstallNiagaraResourceBindingToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_resource_bindings_toolset"))
    static FString VerifyNiagaraResourceBindingToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_resource_bindings_toolset"))
    static FString RemoveNiagaraResourceBindingToolset(UClass* ToolsetClass);
};
