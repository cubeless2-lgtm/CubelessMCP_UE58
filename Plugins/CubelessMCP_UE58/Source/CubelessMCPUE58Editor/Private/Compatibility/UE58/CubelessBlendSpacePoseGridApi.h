#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::BlendSpaceRuntimePoseGrid
{
Variable::FResult Sample(const FString& SkeletalMesh,const FString& BlendSpacePath,const FString& BlendSpacesJson,const FString& SamplesJson,const FString& SampleBonesJson,const FString& SampleSocketsJson,const FString& ActorLabel,double SampleTime,int64 SettleTickCount,int64 TickCount,double TickDeltaTime,bool bIncludeAxisExtremes,bool bAllowMissingBones,bool bRefreshBoneTransforms,bool bCleanup,bool bPreferPIEWorld,bool bRequirePIEWorld);
}
