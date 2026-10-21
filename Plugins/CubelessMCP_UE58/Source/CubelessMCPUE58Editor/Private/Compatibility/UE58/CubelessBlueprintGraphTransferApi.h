#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::BlueprintGraphTransfer
{
Variable::FResult Import(const FString& BlueprintPath,const FString& Text,const FString& TextFilePath,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bClearGraph,bool bKeepRootNode,bool bAllowNonSample,bool bValidateOnly,bool bDryRun,bool bCompile,bool bSaveAsset);
Variable::FResult Repair(const FString& BlueprintPath,bool bAllowNonSample,bool bValidateOnly,bool bDryRun,bool bCompile,bool bSaveAsset);
}
