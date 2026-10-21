#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintResolutionConnectionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintResolutionConnectionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_blueprint_resolution_connection_toolset"))
    static FString InstallBlueprintResolutionConnectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_blueprint_resolution_connection_toolset"))
    static FString VerifyBlueprintResolutionConnectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_blueprint_resolution_connection_toolset"))
    static FString RemoveBlueprintResolutionConnectionToolset(UClass* ToolsetClass);
};
