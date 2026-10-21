#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraRuntimeInspectionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraRuntimeInspectionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_runtime_inspection_toolset"))
    static FString InstallNiagaraRuntimeInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_runtime_inspection_toolset"))
    static FString VerifyNiagaraRuntimeInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_runtime_inspection_toolset"))
    static FString RemoveNiagaraRuntimeInspectionToolset(UClass* ToolsetClass);
};
