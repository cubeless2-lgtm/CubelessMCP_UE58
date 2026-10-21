#include "Compatibility/UE58/CubelessAnimStateRuntimeResponseApi.h"
namespace Cubeless::UE58::AnimStateRuntimeResponse
{
Variable::FResult Sample(const FString& ActorLabel,
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
    return AnimInstanceRuntimePropertyProbe::SampleCases(ActorLabel, ActorName, ActorPath, ComponentName, CasesJson, PropertiesJson, Name, TickCount, TickDeltaTime, bRefreshBoneTransforms, bRestoreAfterCase, bIncludeBaseline, StateMachineName, bIncludeStates, bIncludeStateWeights, bIncludeRelevantAnimTimes, bIncludeTransitionProgress, bIncludeInactiveTransitionProgress, bIncludeMontages, bIncludeCurves, CurveNamesJson, MaxStateMachines, MaxStateMachineInstancesToProbe, MaxStatesPerMachine, MaxTransitionsPerMachine, MaxCurves, bPreferPIEWorld, bRequirePIEWorld, AnimInstanceSource);
}
}
