#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessControlRigTransientProbeLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessControlRigTransientProbeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="controlrig_direct_gate_probe"))
    static FString ControlRigDirectGateProbe(const FString& ControlRigPath, const FString& ControlRigClass,
        const FString& CasesJson, const FString& SampleElementsJson, const FString& ControlValuesJson,
        const FString& ExecuteEventsJson, const FString& ShouldTraceProperty, const FString& InteractionLocationProperty);
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="sample_controlrig_pre_post_runtime_pose"))
    static FString SampleControlRigPrePostRuntimePose(const FString& ControlRigPath, const FString& ControlRigClass,
        const FString& InputDefaultsJson, const FString& CurveValuesJson, const FString& CasesJson,
        const FString& SampleElementsJson, const FString& ControlValuesJson, const FString& ExecuteEventsJson,
        const FString& ShouldTraceProperty, const FString& InteractionLocationProperty);
};
