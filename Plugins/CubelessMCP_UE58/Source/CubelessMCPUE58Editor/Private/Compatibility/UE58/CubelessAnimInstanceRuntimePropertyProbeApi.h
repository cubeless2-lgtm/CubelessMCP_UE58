#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimInstanceRuntimePropertyProbe
{
Variable::FResult Set(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& PropertiesJson,
    const FString& PropertyName,const FString& ValueJson,bool bTickAfterSet,int64 TickCount,double TickDeltaTime,bool bRefreshBoneTransforms,
    bool bIncludeSnapshotAfter,bool bIncludePreviousValues,const FString& StateMachineName,bool bIncludeStates,bool bIncludeStateWeights,
    bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,bool bIncludeInactiveTransitionProgress,bool bIncludeMontages,bool bIncludeCurves,
    const FString& CurveNamesJson,int64 MaxTransitionsPerMachine,bool bPreferPIEWorld,bool bRequirePIEWorld,const FString& AnimInstanceSource);
}
