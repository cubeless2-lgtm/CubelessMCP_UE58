#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
class UBlueprint;
namespace Cubeless::UE58::BlueprintTopology
{
Variable::FResult Inspect(const FString& BlueprintName,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& GraphNameContains,const FString& NodeType,const FString& TitleContains,const FString& ReferenceContains,bool bIncludePins,bool bIncludeLinks,bool bIncludeReferences,int64 MaxGraphs,int64 MaxNodesPerGraph,int64 MaxLinksPerGraph,int64 MaxReferencesPerNode);
}

// HL metadata-only bridge. Reuses original resolver/graph5 helpers; no node/pin observer.
namespace Cubeless::UE58::BlueprintTopology
{
UBlueprint* ResolveBlueprintForMetadata(const FString& BlueprintName,Variable::FResult& Result);
Variable::FResult ListGraphMetadata(const FString& BlueprintName,const FString& GraphType);
}

// HM shares the existing resolver/selector/metadata5 core, without the full topology observer.
class UEdGraph;
namespace Cubeless::UE58::BlueprintTopology
{
Variable::FResult ResolveBlueprintLegacy(const FString& BlueprintName);
UEdGraph* FindGraphForMetadata(UBlueprint* Blueprint,const FString& GraphId,const FString& GraphName,const FString& GraphType,Variable::FResult& Result);
TSharedPtr<FJsonObject> DescribeGraphMetadata(UBlueprint* Blueprint,UEdGraph* Graph,Variable::FResult& Result);
}
