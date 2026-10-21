#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimInstanceRuntimeState
{
Variable::FResult Inspect(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& StateMachineName,
    bool bIncludeStates,bool bIncludeStateWeights,bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,bool bIncludeInactiveTransitionProgress,
    bool bIncludeMontages,bool bIncludeCurves,const FString& CurveNamesJson,int64 MaxStateMachines,int64 MaxStateMachineInstancesToProbe,
    int64 MaxStatesPerMachine,int64 MaxTransitionsPerMachine,int64 MaxCurves,bool bPreferPIEWorld,bool bRequirePIEWorld);
}
