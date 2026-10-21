#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimStateRuntimeResponseLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimStateRuntimeResponseLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Animation", meta=(AIIgnore, ScriptName="sample_anim_state_machine_runtime_response"))
    static FString SampleAnimStateMachineRuntimeResponse(const FString& ActorLabel,
    const FString& ActorName,
    const FString& ActorPath,
    const FString& ComponentName,
    const FString& CasesJson,
    const FString& PropertiesJson,
    const FString& Name,
    int64 TickCount,
    double TickDeltaTime,
    bool bRefreshBoneTransforms,
    bool bRestoreAfterCase,
    bool bIncludeBaseline,
    const FString& StateMachineName,
    bool bIncludeStates,
    bool bIncludeStateWeights,
    bool bIncludeRelevantAnimTimes,
    bool bIncludeTransitionProgress,
    bool bIncludeInactiveTransitionProgress,
    bool bIncludeMontages,
    bool bIncludeCurves,
    const FString& CurveNamesJson,
    int64 MaxStateMachines,
    int64 MaxStateMachineInstancesToProbe,
    int64 MaxStatesPerMachine,
    int64 MaxTransitionsPerMachine,
    int64 MaxCurves,
    bool bPreferPIEWorld,
    bool bRequirePIEWorld,
    const FString& AnimInstanceSource);
};
