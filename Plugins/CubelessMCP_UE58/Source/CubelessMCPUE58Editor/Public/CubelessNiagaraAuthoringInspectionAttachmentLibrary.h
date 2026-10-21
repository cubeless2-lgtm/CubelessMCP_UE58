#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraAuthoringInspectionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraAuthoringInspectionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_authoring_inspection_toolset"))
    static FString InstallNiagaraAuthoringInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_authoring_inspection_toolset"))
    static FString VerifyNiagaraAuthoringInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_authoring_inspection_toolset"))
    static FString RemoveNiagaraAuthoringInspectionToolset(UClass* ToolsetClass);
};
