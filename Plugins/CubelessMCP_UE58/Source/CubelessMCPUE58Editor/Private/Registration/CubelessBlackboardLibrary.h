#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlackboardLibrary.generated.h"

/** Hidden Editor-only bridge. Public Python tools own operation journals. */
UCLASS()
class UCubelessBlackboardLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blackboard", meta=(AIIgnore))
    static FString CreateAIBlackboardAsset(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blackboard", meta=(AIIgnore))
    static FString AddAIBlackboardKey(const FString& BlackboardPath, const FString& KeyName, const FString& KeyType,
        const FString& BaseClass, const FString& EnumPath, bool bInstanceSynced, bool bSaveAsset);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blackboard", meta=(AIIgnore))
    static FString RemoveAIBlackboardKey(const FString& BlackboardPath, const FString& KeyName, bool bSaveAsset);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blackboard", meta=(AIIgnore))
    static FString InspectAIAssetStructure(const FString& AssetPath);
};
