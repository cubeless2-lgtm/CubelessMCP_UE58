#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGNativeCanaryLibrary.generated.h"

// Internal isolated diagnostic only. No UObject is exposed to Python or saved.
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPCGNativeCanaryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Diagnostics", meta=(AIIgnore, ScriptName="begin_pcg_native_allocation_canary"))
    static FString BeginPCGNativeAllocationCanary();
    UFUNCTION(BlueprintCallable, Category="Cubeless|Diagnostics", meta=(AIIgnore, ScriptName="inspect_pcg_native_allocation_canary"))
    static FString InspectPCGNativeAllocationCanary();
    UFUNCTION(BlueprintCallable, Category="Cubeless|Diagnostics", meta=(AIIgnore, ScriptName="release_pcg_native_allocation_canary"))
    static FString ReleasePCGNativeAllocationCanary();
};

namespace Cubeless::UE58::PCGNativeCanary { void Shutdown(); }
