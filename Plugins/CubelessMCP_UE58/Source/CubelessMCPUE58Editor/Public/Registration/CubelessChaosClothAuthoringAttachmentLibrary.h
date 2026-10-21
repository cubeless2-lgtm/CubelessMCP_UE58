#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosClothAuthoringAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosClothAuthoringAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_cloth_authoring_toolset"))
    static FString InstallChaosClothAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_cloth_authoring_toolset"))
    static FString VerifyChaosClothAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_cloth_authoring_toolset"))
    static FString RemoveChaosClothAuthoringToolset(UClass* ToolsetClass);
};
