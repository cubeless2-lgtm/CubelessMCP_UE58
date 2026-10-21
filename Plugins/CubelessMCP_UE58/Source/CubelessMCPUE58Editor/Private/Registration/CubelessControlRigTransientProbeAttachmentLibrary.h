#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessControlRigTransientProbeAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessControlRigTransientProbeAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no transient initialization, execution or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_controlrig_transient_probe_toolset"))
    static FString InstallControlRigTransientProbeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_controlrig_transient_probe_toolset"))
    static FString VerifyControlRigTransientProbeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_controlrig_transient_probe_toolset"))
    static FString RemoveControlRigTransientProbeToolset(UClass* ToolsetClass);
};
