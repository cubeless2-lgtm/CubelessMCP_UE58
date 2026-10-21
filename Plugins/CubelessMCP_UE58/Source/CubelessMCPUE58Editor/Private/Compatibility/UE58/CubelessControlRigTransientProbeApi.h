#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::ControlRigTransientProbe
{
Variable::FResult Direct(const FString& ControlRigPath, const FString& ControlRigClass,
    const FString& CasesJson, const FString& SampleElementsJson, const FString& ControlValuesJson,
    const FString& ExecuteEventsJson, const FString& ShouldTraceProperty, const FString& InteractionLocationProperty);
Variable::FResult PrePost(const FString& ControlRigPath, const FString& ControlRigClass,
    const FString& InputDefaultsJson, const FString& CurveValuesJson, const FString& CasesJson,
    const FString& SampleElementsJson, const FString& ControlValuesJson, const FString& ExecuteEventsJson,
    const FString& ShouldTraceProperty, const FString& InteractionLocationProperty);
}
