#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimGraphInspectionLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimGraphInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="inspect_anim_graph_node_settings"))
    static FString InspectAnimGraphNodeSettings(const FString& BlueprintName,const FString& NodeId,const FString& NodeType,const FString& TitleContains,bool bIncludePins,int64 MaxDepth,const FString& GraphName,const FString& GraphId,const FString& GraphType);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="inspect_anim_graph_protected_topology"))
    static FString InspectAnimGraphProtectedTopology(const FString& BlueprintName,const FString& NodeId,const FString& NodeType,const FString& TitleContains,bool bIncludePins,bool bIncludePosePins,bool bIncludeLinks,bool bIncludeNonPoseLinks,bool bIncludeSettings,int64 MaxDepth,int64 MaxNodes,int64 MaxLinks,const FString& GraphName,const FString& GraphId,const FString& GraphType);
};
