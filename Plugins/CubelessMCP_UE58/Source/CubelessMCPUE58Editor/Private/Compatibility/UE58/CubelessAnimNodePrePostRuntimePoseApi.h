#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimNodePrePostRuntimePose
{
Variable::FResult Sample(const FString& BlueprintName,const FString& AnimBlueprint,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& NodeId,const FString& NodeType,const FString& TitleContains,const FString& SampleBonesJson,const FString& SampleSocketsJson,const FString& Mode,const FString& SkeletalMesh,const FString& TempRoot,bool bCleanup,bool bOverwriteExistingTempAssets,const FString& RunId,const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& AnimInstanceSource,bool bPreferPieWorld,bool bRequirePieWorld,int64 TickCount,double TickDeltaTime,int64 SettleTickCount,bool bRefreshBoneTransforms,bool bAllowMissingBones,int64 PoseLinkMaxDepth,const FString& InputPoseMode,const FString& InputPoseFieldPath,int64 InputPoseIndex,bool bIncludePins,int64 MaxDepth,bool bDryRun,const FString& PrivateContextJson);
}
