#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimGraphControlRig
{
Variable::FResult SetInputDefaults(const FString& BlueprintName,const FString& InputDefaultsJson,const FString& ExposeInputPinsJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeName,const FString& TitleContains,const FString& ControlRigClass,bool bDisconnectExistingLinks,bool bAllowNonSample);
Variable::FResult EnsureForcedDriver(const FString& BlueprintName,const FString& CurveValuesJson,const FString& InputDefaultsJson,double Alpha,const FString& ApplyMode,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeName,const FString& TitleContains,const FString& ControlRigClass,bool bReplaceExisting,bool bDisconnectExistingLinks,bool bAllowNonSample);
}
