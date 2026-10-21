#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimInstanceRuntimeStateLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimInstanceRuntimeStateLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Animation", meta=(AIIgnore, ScriptName="inspect_anim_instance_runtime_state"))
    static FString InspectAnimInstanceRuntimeState(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& StateMachineName,
        bool bIncludeStates,bool bIncludeStateWeights,bool bIncludeRelevantAnimTimes,bool bIncludeTransitionProgress,bool bIncludeInactiveTransitionProgress,
        bool bIncludeMontages,bool bIncludeCurves,const FString& CurveNamesJson,int64 MaxStateMachines,int64 MaxStateMachineInstancesToProbe,
        int64 MaxStatesPerMachine,int64 MaxTransitionsPerMachine,int64 MaxCurves,bool bPreferPIEWorld,bool bRequirePIEWorld);
};
