#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessEventLibrary.generated.h"
/** Hidden editor-only adapters; Python owns named tools and durable mutation journals. */
UCLASS()
class UCubelessEventLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString InspectSavedActorParent(const FString& ClassPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintEventNode(const FString& GraphPath, const FString& EventName, int32 X, int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintParentFunctionCall(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintCustomEventNode(const FString& GraphPath, const FString& CustomEventName, int32 X, int32 Y,
        bool bCallInEditor, bool bHasCallInEditor);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintInputKeyNode(const FString& GraphPath, const FString& KeyName, int32 X, int32 Y,
        bool bControl, bool bAlt, bool bShift, bool bCommand, bool bConsumeInput, bool bExecuteWhenPaused, bool bOverrideParentBinding);
};
