#include "Registration/CubelessAnimInstanceRuntimePropertyProbeLibrary.h"
#include "Compatibility/UE58/CubelessAnimInstanceRuntimePropertyProbeApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimInstanceRuntimePropertyProbeLibrary::SetAnimInstanceRuntimePropertyForProbe(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& PropertiesJson,
    const FString& PropertyName,const FString& ValueJson,bool bTickAfterSet,int64 TickCount,double TickDeltaTime,bool bRefreshBoneTransforms,
    bool bIncludeSnapshotAfter,bool bIncludePreviousValues,const FString& StateMachineName,bool bIncludeStates,bool bIncludeStateWeights,
    bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,bool bIncludeInactiveTransitionProgress,bool bIncludeMontages,bool bIncludeCurves,
    const FString& CurveNamesJson,int64 MaxTransitionsPerMachine,bool bPreferPIEWorld,bool bRequirePIEWorld,const FString& AnimInstanceSource)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimInstanceRuntimePropertyProbe::Set(ActorLabel,ActorName,ActorPath,ComponentName,PropertiesJson,PropertyName,ValueJson,bTickAfterSet,TickCount,TickDeltaTime,bRefreshBoneTransforms,bIncludeSnapshotAfter,bIncludePreviousValues,StateMachineName,bIncludeStates,bIncludeStateWeights,bIncludeRelevantAnimTimes,bIncludeTransitionProgress,bIncludeInactiveTransitionProgress,bIncludeMontages,bIncludeCurves,CurveNamesJson,MaxTransitionsPerMachine,bPreferPIEWorld,bRequirePIEWorld,AnimInstanceSource), TEXT("set_anim_instance_runtime_property_for_probe"));
}
