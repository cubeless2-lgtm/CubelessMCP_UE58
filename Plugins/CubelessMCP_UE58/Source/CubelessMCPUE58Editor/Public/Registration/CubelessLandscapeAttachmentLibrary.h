#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessLandscapeAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessLandscapeAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_landscape_toolset"))
    static FString InstallLandscapeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_landscape_toolset"))
    static FString VerifyLandscapeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_landscape_toolset"))
    static FString RemoveLandscapeToolset(UClass* ToolsetClass);
};
