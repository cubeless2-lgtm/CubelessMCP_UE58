#include "Registration/CubelessAnimGraphAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessAnimGraphAuthoringApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimGraphAuthoringLibrary::SetAnimGraphRigidbodySettings(const FString& BlueprintName,const FString& NodeId,const FString& AlphaJson,const FString& ExternalForceJson,const FString& SimulationSpace,const FString& EnableWorldGeometryJson,bool bAllowNonSample,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimGraphAuthoring::SetRigidBody(BlueprintName,NodeId,AlphaJson,ExternalForceJson,SimulationSpace,EnableWorldGeometryJson,bAllowNonSample,GraphName,GraphId,GraphType),TEXT("set_anim_graph_rigidbody_settings"));
}
FString UCubelessAnimGraphAuthoringLibrary::EnsureAnimGraphInputPosePassthrough(const FString& BlueprintName,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,const FString& InputNodePositionJson,bool bAllowNonSample)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimGraphAuthoring::EnsureInputPose(BlueprintName,GraphName,GraphId,GraphType,bReplaceExisting,InputNodePositionJson,bAllowNonSample),TEXT("ensure_anim_graph_input_pose_passthrough"));
}
