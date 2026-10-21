#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraRendererAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraRendererAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_renderers_toolset"))
    static FString InstallNiagaraRendererToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_renderers_toolset"))
    static FString VerifyNiagaraRendererToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_renderers_toolset"))
    static FString RemoveNiagaraRendererToolset(UClass* ToolsetClass);
};
