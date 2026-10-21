#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBehaviorTreeLibrary.generated.h"

/** Hidden Editor bridge; Python operations retain the public operation journal. */
UCLASS()
class UCubelessBehaviorTreeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|BehaviorTree", meta=(AIIgnore))
    static FString CreateAIBehaviorTreeAsset(const FString& AssetName, const FString& AssetRoot, const FString& BlackboardPath, bool bSaveAsset);
    UFUNCTION(BlueprintCallable, Category="Cubeless|BehaviorTree", meta=(AIIgnore))
    static FString SetAIBehaviorTreeBlackboard(const FString& BehaviorTreePath, const FString& BlackboardPath, bool bSaveAsset);
    UFUNCTION(BlueprintCallable, Category="Cubeless|BehaviorTree", meta=(AIIgnore))
    static FString InspectAIAssetStructure(const FString& AssetPath);
};
