#pragma once
#include "CoreMinimal.h"
#include "JsonObjectWrapper.h"
#include "ToolsetRegistry/ToolCallAsyncResult.h"
#include "CubelessAILiveResult.generated.h"

// Editor-only completion value for the two original fixed AI live tools.
// This UObject is never placed in an asset or used as a gameplay Blueprint node.
UCLASS(BlueprintType)
class UCubelessAILiveResult : public UToolCallAsyncResult
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="Cubeless|AI")
    FJsonObjectWrapper Value;

    UFUNCTION(BlueprintCallable, Category="Cubeless|AI", meta=(AIIgnore))
    bool SetValue(const FString& TerminalReportJson);
};
