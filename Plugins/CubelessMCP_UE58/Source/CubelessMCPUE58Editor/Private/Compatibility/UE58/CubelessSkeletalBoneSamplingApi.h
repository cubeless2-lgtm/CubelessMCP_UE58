#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::SkeletalBoneSampling
{
Variable::FResult Sample(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& BonesJson,const FString& SocketsJson,bool bPreferPIEWorld,bool bRequirePIEWorld);
}
