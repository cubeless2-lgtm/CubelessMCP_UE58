#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::BlueprintQueryLayout
{
Variable::FResult Find(const FString& BlueprintName,const FString& NodeTypeJson,const FString& EventTypeJson,const FString& GraphName,const FString& GraphId,const FString& GraphType);
Variable::FResult SetPositions(const FString& BlueprintName,const FString& GraphName,const FString& UpdatesJson,const FString& GraphId,const FString& GraphType,bool bDryRun,bool bCompile,bool bSave);
}
