#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraPreviewWorkflowAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewWorkflowAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_preview_workflows_toolset"))
    static FString InstallNiagaraPreviewWorkflowToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_preview_workflows_toolset"))
    static FString VerifyNiagaraPreviewWorkflowToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_preview_workflows_toolset"))
    static FString RemoveNiagaraPreviewWorkflowToolset(UClass* ToolsetClass);
};
