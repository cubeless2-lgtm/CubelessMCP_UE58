#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGUtilityAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPCGUtilityAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_pcg_utilities_toolset"))
    static FString InstallPCGUtilityToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_pcg_utilities_toolset"))
    static FString VerifyPCGUtilityToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_pcg_utilities_toolset"))
    static FString RemovePCGUtilityToolset(UClass* ToolsetClass);
};
