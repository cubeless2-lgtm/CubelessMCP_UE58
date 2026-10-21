#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraFactoryBootstrapAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraFactoryBootstrapAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_factory_bootstrap_toolset"))
    static FString InstallNiagaraFactoryBootstrapToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_factory_bootstrap_toolset"))
    static FString VerifyNiagaraFactoryBootstrapToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_factory_bootstrap_toolset"))
    static FString RemoveNiagaraFactoryBootstrapToolset(UClass* ToolsetClass);
};
