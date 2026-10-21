#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraCleanupAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraCleanupAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_cleanup_toolset"))
    static FString InstallNiagaraCleanupToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_cleanup_toolset"))
    static FString VerifyNiagaraCleanupToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_cleanup_toolset"))
    static FString RemoveNiagaraCleanupToolset(UClass* ToolsetClass);
};
