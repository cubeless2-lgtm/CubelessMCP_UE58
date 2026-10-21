#include "Registration/CubelessAnimStateRuntimeResponseLibrary.h"
#include "Compatibility/UE58/CubelessAnimStateRuntimeResponseApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimStateRuntimeResponseLibrary::SampleAnimStateMachineRuntimeResponse(const FString& ActorLabel,
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
    const FString& AnimInstanceSource)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimStateRuntimeResponse::Sample(ActorLabel, ActorName, ActorPath, ComponentName, CasesJson, PropertiesJson, Name, TickCount, TickDeltaTime, bRefreshBoneTransforms, bRestoreAfterCase, bIncludeBaseline, StateMachineName, bIncludeStates, bIncludeStateWeights, bIncludeRelevantAnimTimes, bIncludeTransitionProgress, bIncludeInactiveTransitionProgress, bIncludeMontages, bIncludeCurves, CurveNamesJson, MaxStateMachines, MaxStateMachineInstancesToProbe, MaxStatesPerMachine, MaxTransitionsPerMachine, MaxCurves, bPreferPIEWorld, bRequirePIEWorld, AnimInstanceSource), TEXT("sample_anim_state_machine_runtime_response"));
}
