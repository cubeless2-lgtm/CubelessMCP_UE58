#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessGraphLibrary.generated.h"

/** Internal editor helpers; never reference this class from game assets. */
UCLASS()
class UCubelessGraphLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReplaceExecLink(const FString& GraphPath, const FString& SourceNodePath,
        const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName,
        const FString& ExpectedOldTargetNodePath, const FString& ExpectedOldTargetPinName,
        int32 ExpectedSourceLinkCount, const FString& ExpectedSchemaResponse);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString AddSequenceNode(const FString& GraphPath, int32 X, int32 Y, int32 OutputCount);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReadNodeIdentities(const FString& GraphPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReadMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReadExternalMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString SetBlueprintPinDefault(const FString& GraphPath, const FString& NodePath, const FString& PinName, const FString& ValueJson, const FString& Direction);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Graph", meta=(AIIgnore))
    static FString ReadConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
        const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName);
};
