#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimInstanceRuntimePropertyProbeLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimInstanceRuntimePropertyProbeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Animation", meta=(AIIgnore, ScriptName="set_anim_instance_runtime_property_for_probe"))
    static FString SetAnimInstanceRuntimePropertyForProbe(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& PropertiesJson,
    const FString& PropertyName,const FString& ValueJson,bool bTickAfterSet,int64 TickCount,double TickDeltaTime,bool bRefreshBoneTransforms,
    bool bIncludeSnapshotAfter,bool bIncludePreviousValues,const FString& StateMachineName,bool bIncludeStates,bool bIncludeStateWeights,
    bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,bool bIncludeInactiveTransitionProgress,bool bIncludeMontages,bool bIncludeCurves,
    const FString& CurveNamesJson,int64 MaxTransitionsPerMachine,bool bPreferPIEWorld,bool bRequirePIEWorld,const FString& AnimInstanceSource);
};
