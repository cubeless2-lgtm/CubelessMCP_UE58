#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPythonExecutionLibrary.generated.h"

// Editor-only synchronous internal seam for the existing domain/Batch action.
// This is not a separately registered MCP tool or a generated-game asset API.
UCLASS(meta=(AIIgnore))
class UCubelessPythonExecutionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static FString ExecutePython(const FString& Code, const FString& Mode = TEXT("ExecuteFile"),
        bool bDeferToTicker = false, bool bAllowUnsafeEditorScriptingDuringPIE = false,
        const FString& InterchangePolicy = TEXT("auto"), const FString& TransactionPolicy = TEXT("command"),
        bool bAllowUnsafePCGSelectorPython = false);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static FString BeginBatchTransaction(const FString& OperationId);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Editor|Internal", meta=(AIIgnore))
    static FString ObserveBatchTransaction(const FString& ScopeId, const FString& Action);
};
