#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraPromotionManifestAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPromotionManifestAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_promotion_manifest_toolset"))
    static FString InstallNiagaraPromotionManifestToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_promotion_manifest_toolset"))
    static FString VerifyNiagaraPromotionManifestToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_promotion_manifest_toolset"))
    static FString RemoveNiagaraPromotionManifestToolset(UClass* ToolsetClass);
};
