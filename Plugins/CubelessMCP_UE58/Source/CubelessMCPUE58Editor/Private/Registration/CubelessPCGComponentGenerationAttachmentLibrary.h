#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGComponentGenerationAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPCGComponentGenerationAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_pcg_component_generation_toolset"))
    static FString InstallPCGComponentGenerationToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_pcg_component_generation_toolset"))
    static FString VerifyPCGComponentGenerationToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_pcg_component_generation_toolset"))
    static FString RemovePCGComponentGenerationToolset(UClass* ToolsetClass);
};
