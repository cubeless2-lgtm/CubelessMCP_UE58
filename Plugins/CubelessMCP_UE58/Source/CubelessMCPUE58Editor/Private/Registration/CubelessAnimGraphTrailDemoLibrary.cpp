#include "Registration/CubelessAnimGraphTrailDemoLibrary.h"
#include "Compatibility/UE58/CubelessAnimGraphTrailDemoApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimGraphTrailDemoLibrary::EnsureAnimGraphTrailDemo(const FString& BlueprintName,const FString& TrailBone,const FString& BaseJoint,int64 ChainLength,const FString& ChainBoneAxis,double Alpha,const FString& FakeVelocityJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample,bool bInvertChainBoneAxis,bool bReorientParentToChild,bool bActorSpaceFakeVelocity,double RelaxationSpeedScale,bool bLimitStretch,double StretchLimit,double MaxDeltaTime)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimGraphTrailDemo::EnsureTrailDemo(BlueprintName,TrailBone,BaseJoint,ChainLength,ChainBoneAxis,Alpha,FakeVelocityJson,GraphName,GraphId,GraphType,bReplaceExisting,bAllowNonSample,bInvertChainBoneAxis,bReorientParentToChild,bActorSpaceFakeVelocity,RelaxationSpeedScale,bLimitStretch,StretchLimit,MaxDeltaTime),TEXT("ensure_anim_graph_trail_demo"));
}
