#include "Registration/CubelessAnimGraphSecondaryMotionLibrary.h"
#include "Compatibility/UE58/CubelessAnimGraphSecondaryMotionApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimGraphSecondaryMotionLibrary::EnsureAnimGraphSecondaryMotionChain(const FString& BlueprintName,const FString& DynamicsType,const FString& RootBone,const FString& ChainEndBone,double Alpha,const FString& GravityJson,double AngularLimitDegrees,const FString& BodyExtentsJson,double LinearDamping,double AngularDamping,double KawaiiDamping,double KawaiiStiffness,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample,bool bCompile,bool bSave)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimGraphSecondaryMotion::EnsureSecondaryMotionChain(BlueprintName,DynamicsType,RootBone,ChainEndBone,Alpha,GravityJson,AngularLimitDegrees,BodyExtentsJson,LinearDamping,AngularDamping,KawaiiDamping,KawaiiStiffness,GraphName,GraphId,GraphType,bReplaceExisting,bAllowNonSample,bCompile,bSave),TEXT("ensure_anim_graph_secondary_motion_chain"));
}
