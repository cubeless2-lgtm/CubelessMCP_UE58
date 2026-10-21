#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMapLibrary.generated.h"

/** Editor adapter only. Authored nodes reference Engine.BlueprintMapLibrary. */
UCLASS()
class UCubelessMapLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Creates one unlinked exact Map_Clear/Find/Add stock call node.
     * Its actual wildcard pins are inspected; String -> Vector specialization
     * remains pending until a separately requested typed Map connection.
     * No map invocation, implicit wire, compile, save or retry is performed.
     * Python owns the existing named Toolset and durable mutation journal.
     */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintMapFunctionNode(const FString& BlueprintPath, const FString& Operation,
        const FString& KeyType, const FString& ValueType, const FString& ParamDefaultsJson,
        const FString& NodePositionJson, const FString& GraphName, const FString& GraphId,
        const FString& GraphType, bool bCreateGraphIfMissing);
};
