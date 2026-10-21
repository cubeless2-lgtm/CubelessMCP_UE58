#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimStateMachineInspection
{
Variable::FResult Inspect(const FString& BlueprintName,const FString& StateMachineName,bool bIncludePins,bool bIncludeRuleGraphNodes,bool bIncludeStateNodes,int64 MaxRuleGraphNodes);
}
