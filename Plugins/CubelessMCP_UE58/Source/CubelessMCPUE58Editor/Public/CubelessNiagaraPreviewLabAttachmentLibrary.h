#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraPreviewLabAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewLabAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_preview_lab_toolset"))
    static FString InstallNiagaraPreviewLabToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_preview_lab_toolset"))
    static FString VerifyNiagaraPreviewLabToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_preview_lab_toolset"))
    static FString RemoveNiagaraPreviewLabToolset(UClass* ToolsetClass);
};
