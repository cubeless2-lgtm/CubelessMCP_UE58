#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlendSpaceVariantAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlendSpaceVariantAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_blendspace_variant_toolset"))
    static FString InstallBlendSpaceVariantToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_blendspace_variant_toolset"))
    static FString VerifyBlendSpaceVariantToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_blendspace_variant_toolset"))
    static FString RemoveBlendSpaceVariantToolset(UClass* ToolsetClass);
};
