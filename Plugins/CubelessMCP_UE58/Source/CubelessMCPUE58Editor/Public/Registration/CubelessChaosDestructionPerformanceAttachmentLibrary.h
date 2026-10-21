#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosDestructionPerformanceAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosDestructionPerformanceAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_destruction_performance_toolset"))
    static FString InstallChaosDestructionPerformanceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_destruction_performance_toolset"))
    static FString VerifyChaosDestructionPerformanceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_destruction_performance_toolset"))
    static FString RemoveChaosDestructionPerformanceToolset(UClass* ToolsetClass);
};
