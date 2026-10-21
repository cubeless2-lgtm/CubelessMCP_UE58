#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolCallAsyncResult.h"
#include "CubelessAsyncOperationResult.generated.h"

// Editor-only completion foundation. Python declares value of the existing
// standard11 result struct; this class has no asset or workflow methods.
UCLASS(BlueprintType, Blueprintable, Transient)
class CUBELESSMCPUE58EDITOR_API UCubelessAsyncOperationResult : public UToolCallAsyncResult
{
    GENERATED_BODY()
public:
    // Complete only after Python has populated value. A packed success=False
    // remains an ordinary typed result; this bool describes transport completion.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Async", meta=(AIIgnore, ScriptName="complete_value"))
    bool CompleteValue();

    UPROPERTY(BlueprintReadOnly, Category="Cubeless|Async")
    FString CompletionDiagnostic;
private:
    bool Reject(const FString& Reason);
};
