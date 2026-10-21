#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimNodePrePostRuntimePoseLibrary.generated.h"
UCLASS(meta=(AIIgnore))
class UCubelessAnimNodePrePostRuntimePoseLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="sample_anim_node_pre_post_runtime_pose"))
    static FString SampleAnimNodePrePostRuntimePose(FString BlueprintName,FString AnimBlueprint,FString GraphName,FString GraphId,FString GraphType,FString NodeId,FString NodeType,FString TitleContains,FString SampleBonesJson,FString SampleSocketsJson,FString Mode,FString SkeletalMesh,FString TempRoot,bool bCleanup,bool bOverwriteExistingTempAssets,FString RunId,FString ActorLabel,FString ActorName,FString ActorPath,FString ComponentName,FString AnimInstanceSource,bool bPreferPieWorld,bool bRequirePieWorld,int64 TickCount,double TickDeltaTime,int64 SettleTickCount,bool bRefreshBoneTransforms,bool bAllowMissingBones,int64 PoseLinkMaxDepth,FString InputPoseMode,FString InputPoseFieldPath,int64 InputPoseIndex,bool bIncludePins,int64 MaxDepth,bool bDryRun,FString PrivateContextJson);
};
