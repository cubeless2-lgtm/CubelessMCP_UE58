#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::BlueprintFunctionExtraction
{
Variable::FResult Collapse(const FString& BlueprintName,const FString& GraphName,const FString& FunctionName,const FString& NodeIdsJson,const FString& GraphType,int64 ExpectedNodeCount,bool bAllowNonTemp,bool bDryRun,bool bCompile,bool bSaveAsset,const FString& PrivateContextJson);
}
