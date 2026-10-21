#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimGraphInspection
{
Variable::FResult Settings(const FString& BlueprintName,const FString& NodeId,const FString& NodeType,const FString& TitleContains,bool bIncludePins,int64 MaxDepth,const FString& GraphName,const FString& GraphId,const FString& GraphType);
Variable::FResult Topology(const FString& BlueprintName,const FString& NodeId,const FString& NodeType,const FString& TitleContains,bool bIncludePins,bool bIncludePosePins,bool bIncludeLinks,bool bIncludeNonPoseLinks,bool bIncludeSettings,int64 MaxDepth,int64 MaxNodes,int64 MaxLinks,const FString& GraphName,const FString& GraphId,const FString& GraphType);
}
