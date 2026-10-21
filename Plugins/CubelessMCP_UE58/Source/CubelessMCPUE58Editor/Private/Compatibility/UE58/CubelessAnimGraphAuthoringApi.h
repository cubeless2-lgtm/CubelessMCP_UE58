#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimGraphAuthoring
{
Variable::FResult SetRigidBody(const FString& BlueprintName,const FString& NodeId,const FString& AlphaJson,const FString& ExternalForceJson,const FString& SimulationSpace,const FString& EnableWorldGeometryJson,bool bAllowNonSample,const FString& GraphName,const FString& GraphId,const FString& GraphType);
Variable::FResult EnsureInputPose(const FString& BlueprintName,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,const FString& InputNodePositionJson,bool bAllowNonSample);
}
