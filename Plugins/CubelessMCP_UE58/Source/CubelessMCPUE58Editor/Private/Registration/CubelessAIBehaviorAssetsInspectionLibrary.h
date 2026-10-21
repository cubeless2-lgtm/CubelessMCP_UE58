#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAIBehaviorAssetsInspectionLibrary.generated.h"

/** Hidden Editor-only adapter for the original paired AI asset inspection. */
UCLASS()
class UCubelessAIBehaviorAssetsInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|AI", meta=(AIIgnore, ScriptName="inspect_ai_behavior_assets"))
    static FString InspectAIBehaviorAssets(const FString& BehaviorTreePath, const FString& BlackboardPath);
};
