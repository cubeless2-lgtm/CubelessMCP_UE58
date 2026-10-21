#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraModuleInputPathAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraModuleInputPathAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_module_input_paths_toolset"))
    static FString InstallNiagaraModuleInputPathToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_module_input_paths_toolset"))
    static FString VerifyNiagaraModuleInputPathToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_module_input_paths_toolset"))
    static FString RemoveNiagaraModuleInputPathToolset(UClass* ToolsetClass);
};
