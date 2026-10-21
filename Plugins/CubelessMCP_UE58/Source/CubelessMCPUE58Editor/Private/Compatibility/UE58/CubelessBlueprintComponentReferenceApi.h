#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::BlueprintComponentReference
{
Variable::FResult Add(const FString& BlueprintPath,const FString& ComponentName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraph);
}
