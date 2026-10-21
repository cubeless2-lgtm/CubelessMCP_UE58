#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAIBehaviorAssetsLibrary.generated.h"

/** Hidden Editor bridge; Python retains the original public operation journal. */
UCLASS()
class UCubelessAIBehaviorAssetsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|AI", meta=(AIIgnore))
    static FString CreateAIPatrolChaseBehaviorAssets(const FString& AssetRoot,
        const FString& BehaviorTreeName, const FString& BlackboardName,
        const FString& TargetActorKey, const FString& PatrolLocationKey,
        const FString& TreeShape, double AcceptableRadius, double AlertWaitSeconds,
        double FacePrecisionDegrees, bool bReplaceExisting, bool bSaveAssets);
};
