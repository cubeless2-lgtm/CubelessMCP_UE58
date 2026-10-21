#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimStateMachineInspectionLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimStateMachineInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="inspect_anim_state_machine_transitions"))
    static FString InspectAnimStateMachineTransitions(const FString& BlueprintName,const FString& StateMachineName,bool bIncludePins,bool bIncludeRuleGraphNodes,bool bIncludeStateNodes,int64 MaxRuleGraphNodes);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="prepare_anim_state_machine_inspection_toolset"))
    static FString PrepareAnimStateMachineInspectionToolset(UClass* Class);
};
