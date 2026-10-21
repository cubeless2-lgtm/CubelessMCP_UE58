#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAIDiscoveryLibrary.generated.h"

/** Hidden Editor-only adapter for the original named loaded-class search. */
UCLASS()
class UCubelessAIDiscoveryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="prepare_ai_discovery_toolset"))
    static FString PrepareAIDiscoveryToolset(UClass* ToolsetClass);

    UFUNCTION(BlueprintCallable, Category="Cubeless|AI", meta=(AIIgnore, ScriptName="search_ai_behavior_tree_node_classes"))
    static FString SearchAIBehaviorTreeNodeClasses(const FString& Category, const FString& Query,
        bool bIncludeAbstract, int32 MaxResults);
};
