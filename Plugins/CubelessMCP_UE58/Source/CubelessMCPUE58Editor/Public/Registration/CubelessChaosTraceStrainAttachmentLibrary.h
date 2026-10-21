#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosTraceStrainAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosTraceStrainAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_trace_strain_toolset"))
    static FString InstallChaosTraceStrainToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_trace_strain_toolset"))
    static FString VerifyChaosTraceStrainToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_trace_strain_toolset"))
    static FString RemoveChaosTraceStrainToolset(UClass* ToolsetClass);
};
