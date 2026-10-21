#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessFoliageInstanceAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessFoliageInstanceAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_foliage_instances_toolset"))
    static FString InstallFoliageInstanceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_foliage_instances_toolset"))
    static FString VerifyFoliageInstanceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_foliage_instances_toolset"))
    static FString RemoveFoliageInstanceToolset(UClass* ToolsetClass);
};
