#include "Registration/CubelessAnimInstanceRuntimeStateLibrary.h"
#include "Compatibility/UE58/CubelessAnimInstanceRuntimeStateApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimInstanceRuntimeStateLibrary::InspectAnimInstanceRuntimeState(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& StateMachineName,
    bool bIncludeStates,bool bIncludeStateWeights,bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,bool bIncludeInactiveTransitionProgress,
    bool bIncludeMontages,bool bIncludeCurves,const FString& CurveNamesJson,int64 MaxStateMachines,int64 MaxStateMachineInstancesToProbe,
    int64 MaxStatesPerMachine,int64 MaxTransitionsPerMachine,int64 MaxCurves,bool bPreferPIEWorld,bool bRequirePIEWorld)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimInstanceRuntimeState::Inspect(
        ActorLabel,ActorName,ActorPath,ComponentName,StateMachineName,bIncludeStates,bIncludeStateWeights,bIncludeRelevantAnimTimes,
        bIncludeTransitionProgress,bIncludeInactiveTransitionProgress,bIncludeMontages,bIncludeCurves,CurveNamesJson,MaxStateMachines,
        MaxStateMachineInstancesToProbe,MaxStatesPerMachine,MaxTransitionsPerMachine,MaxCurves,bPreferPIEWorld,bRequirePIEWorld),
        TEXT("inspect_anim_instance_runtime_state"));
}
