#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessArrayLibrary.generated.h"

/** Internal Editor-only finite array signature adapter. */
UCLASS()
class UCubelessArrayLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Reads exact already-loaded Engine array and two break signatures.
     * No invocation, nodes, asset selectors, loads, CDO creation or changes.
     * This method is hidden from the official AI Toolset Registry.
     */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Diagnostics", meta=(AIIgnore))
    static FString InspectNativeSupport();
    /** Create one unlinked int/bool array-function node in an existing graph.
     * Initial nine-operation cohort only. No implicit wire, compile or save.
     * Python owns named registration and durable mutation accounting.
     */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintArrayFunctionNode(const FString& BlueprintPath, const FString& Operation,
        const FString& ElementType, const FString& ParamDefaultsJson, const FString& TypeObject, const FString& EnumType,
        const FString& NodePositionJson, const FString& GraphName, const FString& GraphId, const FString& GraphType,
        bool bCreateGraphIfMissing);
};
