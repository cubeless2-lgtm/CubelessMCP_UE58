#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraPromotionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPromotionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_promotion_toolset"))
    static FString InstallNiagaraPromotionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_promotion_toolset"))
    static FString VerifyNiagaraPromotionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_promotion_toolset"))
    static FString RemoveNiagaraPromotionToolset(UClass* ToolsetClass);
};
