#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::SplineComponentPoints
{
Variable::FResult Set(const FString& PointsJson,const FString& ActorLabel,const FString& ActorName,const FString& ActorTag,const FString& ActorLabelPrefix,const FString& ComponentName,const FString& ComponentTag,bool bSelectedOnly,bool bClosedLoop,const FString& PrivateContextJson);
}
