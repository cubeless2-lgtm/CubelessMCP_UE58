#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimGraphModifyCurveDemo
{
Variable::FResult EnsureModifyCurveDemo(const FString& BlueprintName,const FString& CurveValuesJson,double Alpha,const FString& ApplyMode,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample);
}
