#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintTopologyLibrary.generated.h"
UCLASS()
class UCubelessBlueprintTopologyLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="inspect_blueprint_graph_call_topology"))
    static FString InspectBlueprintGraphCallTopology(const FString& BlueprintName,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& GraphNameContains,const FString& NodeType,const FString& TitleContains,const FString& ReferenceContains,bool bIncludePins,bool bIncludeLinks,bool bIncludeReferences,int64 MaxGraphs,int64 MaxNodesPerGraph,int64 MaxLinksPerGraph,int64 MaxReferencesPerNode);
};
