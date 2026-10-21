#include "Registration/CubelessAnimGraphControlRigLibrary.h"
#include "Compatibility/UE58/CubelessAnimGraphControlRigApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimGraphControlRigLibrary::SetAnimGraphControlRigInputDefaults(const FString& BlueprintName,const FString& InputDefaultsJson,const FString& ExposeInputPinsJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeName,const FString& TitleContains,const FString& ControlRigClass,bool bDisconnectExistingLinks,bool bAllowNonSample)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimGraphControlRig::SetInputDefaults(BlueprintName,InputDefaultsJson,ExposeInputPinsJson,GraphName,GraphId,GraphType,NodeId,NodeName,TitleContains,ControlRigClass,bDisconnectExistingLinks,bAllowNonSample),TEXT("set_anim_graph_controlrig_input_defaults"));
}
FString UCubelessAnimGraphControlRigLibrary::EnsureControlRigForcedDriverAnimBP(const FString& BlueprintName,const FString& CurveValuesJson,const FString& InputDefaultsJson,double Alpha,const FString& ApplyMode,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeName,const FString& TitleContains,const FString& ControlRigClass,bool bReplaceExisting,bool bDisconnectExistingLinks,bool bAllowNonSample)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimGraphControlRig::EnsureForcedDriver(BlueprintName,CurveValuesJson,InputDefaultsJson,Alpha,ApplyMode,GraphName,GraphId,GraphType,NodeId,NodeName,TitleContains,ControlRigClass,bReplaceExisting,bDisconnectExistingLinks,bAllowNonSample),TEXT("ensure_controlrig_forced_driver_animbp"));
}
