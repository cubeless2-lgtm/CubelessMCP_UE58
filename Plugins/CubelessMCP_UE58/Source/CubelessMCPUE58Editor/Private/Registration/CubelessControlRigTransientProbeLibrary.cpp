#include "Registration/CubelessControlRigTransientProbeLibrary.h"
#include "Compatibility/UE58/CubelessControlRigTransientProbeApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessControlRigTransientProbeLibrary::ControlRigDirectGateProbe(const FString& ControlRigPath, const FString& ControlRigClass,
    const FString& CasesJson, const FString& SampleElementsJson, const FString& ControlValuesJson,
    const FString& ExecuteEventsJson, const FString& ShouldTraceProperty, const FString& InteractionLocationProperty)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ControlRigTransientProbe::Direct(ControlRigPath,
        ControlRigClass, CasesJson, SampleElementsJson, ControlValuesJson, ExecuteEventsJson,
        ShouldTraceProperty, InteractionLocationProperty), TEXT("controlrig_direct_gate_probe"));
}

FString UCubelessControlRigTransientProbeLibrary::SampleControlRigPrePostRuntimePose(const FString& ControlRigPath, const FString& ControlRigClass,
    const FString& InputDefaultsJson, const FString& CurveValuesJson, const FString& CasesJson,
    const FString& SampleElementsJson, const FString& ControlValuesJson, const FString& ExecuteEventsJson,
    const FString& ShouldTraceProperty, const FString& InteractionLocationProperty)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ControlRigTransientProbe::PrePost(ControlRigPath,
        ControlRigClass, InputDefaultsJson, CurveValuesJson, CasesJson, SampleElementsJson, ControlValuesJson,
        ExecuteEventsJson, ShouldTraceProperty, InteractionLocationProperty), TEXT("sample_controlrig_pre_post_runtime_pose"));
}
