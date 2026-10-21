#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimInstanceRuntimePropertyProbeAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimInstanceRuntimePropertyProbeAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no runtime observation or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_anim_instance_runtime_property_probe_toolset"))
    static FString InstallAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_anim_instance_runtime_property_probe_toolset"))
    static FString VerifyAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_anim_instance_runtime_property_probe_toolset"))
    static FString RemoveAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass);
};
