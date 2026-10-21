#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosClothComponentAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosClothComponentAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_cloth_component_toolset"))
    static FString InstallChaosClothComponentToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_cloth_component_toolset"))
    static FString VerifyChaosClothComponentToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_cloth_component_toolset"))
    static FString RemoveChaosClothComponentToolset(UClass* ToolsetClass);
};
