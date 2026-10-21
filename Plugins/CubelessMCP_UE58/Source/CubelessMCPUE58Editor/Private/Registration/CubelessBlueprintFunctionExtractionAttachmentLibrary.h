#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintFunctionExtractionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintFunctionExtractionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_blueprint_function_extraction_toolset"))
    static FString InstallBlueprintFunctionExtractionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_blueprint_function_extraction_toolset"))
    static FString VerifyBlueprintFunctionExtractionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_blueprint_function_extraction_toolset"))
    static FString RemoveBlueprintFunctionExtractionToolset(UClass* ToolsetClass);
};
