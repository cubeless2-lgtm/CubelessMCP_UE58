#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimGraphControlRigLibrary.generated.h"
UCLASS()
class UCubelessAnimGraphControlRigLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="set_anim_graph_controlrig_input_defaults"))
    static FString SetAnimGraphControlRigInputDefaults(const FString& BlueprintName,const FString& InputDefaultsJson,const FString& ExposeInputPinsJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeName,const FString& TitleContains,const FString& ControlRigClass,bool bDisconnectExistingLinks,bool bAllowNonSample);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_controlrig_forced_driver_animbp"))
    static FString EnsureControlRigForcedDriverAnimBP(const FString& BlueprintName,const FString& CurveValuesJson,const FString& InputDefaultsJson,double Alpha,const FString& ApplyMode,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeName,const FString& TitleContains,const FString& ControlRigClass,bool bReplaceExisting,bool bDisconnectExistingLinks,bool bAllowNonSample);
};
