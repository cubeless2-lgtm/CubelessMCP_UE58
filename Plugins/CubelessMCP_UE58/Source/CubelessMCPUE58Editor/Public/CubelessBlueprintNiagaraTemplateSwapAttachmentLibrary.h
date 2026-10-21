#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintNiagaraTemplateSwapAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintNiagaraTemplateSwapAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_blueprint_niagara_template_swap_toolset"))
    static FString InstallBlueprintNiagaraTemplateSwapToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_blueprint_niagara_template_swap_toolset"))
    static FString VerifyBlueprintNiagaraTemplateSwapToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_blueprint_niagara_template_swap_toolset"))
    static FString RemoveBlueprintNiagaraTemplateSwapToolset(UClass* ToolsetClass);
};
