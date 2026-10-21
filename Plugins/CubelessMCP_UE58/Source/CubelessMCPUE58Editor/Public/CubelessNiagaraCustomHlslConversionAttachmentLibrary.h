#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraCustomHlslConversionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraCustomHlslConversionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_custom_hlsl_conversion_toolset"))
    static FString InstallNiagaraCustomHlslConversionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_custom_hlsl_conversion_toolset"))
    static FString VerifyNiagaraCustomHlslConversionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_custom_hlsl_conversion_toolset"))
    static FString RemoveNiagaraCustomHlslConversionToolset(UClass* ToolsetClass);
};
