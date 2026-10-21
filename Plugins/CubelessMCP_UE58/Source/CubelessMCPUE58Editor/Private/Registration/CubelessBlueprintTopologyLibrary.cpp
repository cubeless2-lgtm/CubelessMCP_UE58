#include "Registration/CubelessBlueprintTopologyLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintTopologyApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlueprintTopologyLibrary::InspectBlueprintGraphCallTopology(const FString& BlueprintName,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& GraphNameContains,const FString& NodeType,const FString& TitleContains,const FString& ReferenceContains,bool bIncludePins,bool bIncludeLinks,bool bIncludeReferences,int64 MaxGraphs,int64 MaxNodesPerGraph,int64 MaxLinksPerGraph,int64 MaxReferencesPerNode)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintTopology::Inspect(BlueprintName,GraphName,GraphId,GraphType,GraphNameContains,NodeType,TitleContains,ReferenceContains,bIncludePins,bIncludeLinks,bIncludeReferences,MaxGraphs,MaxNodesPerGraph,MaxLinksPerGraph,MaxReferencesPerNode),TEXT("inspect_blueprint_graph_call_topology"));
}
