#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlendSpacePoseGridLibrary.generated.h"
UCLASS()
class UCubelessBlendSpacePoseGridLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="sample_blendspace_runtime_pose_grid"))
    static FString SampleBlendSpaceRuntimePoseGrid(const FString& SkeletalMesh,const FString& BlendSpacePath,const FString& BlendSpacesJson,const FString& SamplesJson,const FString& SampleBonesJson,const FString& SampleSocketsJson,const FString& ActorLabel,double SampleTime,int64 SettleTickCount,int64 TickCount,double TickDeltaTime,bool bIncludeAxisExtremes,bool bAllowMissingBones,bool bRefreshBoneTransforms,bool bCleanup,bool bPreferPIEWorld,bool bRequirePIEWorld);
};
