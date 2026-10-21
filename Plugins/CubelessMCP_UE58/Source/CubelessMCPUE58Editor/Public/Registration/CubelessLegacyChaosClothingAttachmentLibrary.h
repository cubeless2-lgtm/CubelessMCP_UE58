#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessLegacyChaosClothingAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessLegacyChaosClothingAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_legacy_chaos_clothing_toolset"))
    static FString InstallLegacyChaosClothingToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_legacy_chaos_clothing_toolset"))
    static FString VerifyLegacyChaosClothingToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_legacy_chaos_clothing_toolset"))
    static FString RemoveLegacyChaosClothingToolset(UClass* ToolsetClass);
};
