#include "Registration/CubelessAnimGraphBoneDemoLibrary.h"
#include "Compatibility/UE58/CubelessAnimGraphBoneDemoApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimGraphBoneDemoLibrary::EnsureAnimGraphModifyBoneDemo(const FString& BlueprintName,const FString& BoneName,const FString& RotationJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimGraphBoneDemo::EnsureModifyBoneDemo(BlueprintName,BoneName,RotationJson,GraphName,GraphId,GraphType,bReplaceExisting,bAllowNonSample),TEXT("ensure_anim_graph_modify_bone_demo"));
}
