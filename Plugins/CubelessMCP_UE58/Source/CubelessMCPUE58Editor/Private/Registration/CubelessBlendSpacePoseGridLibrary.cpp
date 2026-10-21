#include "Registration/CubelessBlendSpacePoseGridLibrary.h"
#include "Compatibility/UE58/CubelessBlendSpacePoseGridApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlendSpacePoseGridLibrary::SampleBlendSpaceRuntimePoseGrid(const FString& SkeletalMesh,const FString& BlendSpacePath,const FString& BlendSpacesJson,const FString& SamplesJson,const FString& SampleBonesJson,const FString& SampleSocketsJson,const FString& ActorLabel,double SampleTime,int64 SettleTickCount,int64 TickCount,double TickDeltaTime,bool bIncludeAxisExtremes,bool bAllowMissingBones,bool bRefreshBoneTransforms,bool bCleanup,bool bPreferPIEWorld,bool bRequirePIEWorld)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlendSpaceRuntimePoseGrid::Sample(SkeletalMesh,BlendSpacePath,BlendSpacesJson,SamplesJson,SampleBonesJson,SampleSocketsJson,ActorLabel,SampleTime,SettleTickCount,TickCount,TickDeltaTime,bIncludeAxisExtremes,bAllowMissingBones,bRefreshBoneTransforms,bCleanup,bPreferPIEWorld,bRequirePIEWorld),TEXT("sample_blendspace_runtime_pose_grid"));
}
