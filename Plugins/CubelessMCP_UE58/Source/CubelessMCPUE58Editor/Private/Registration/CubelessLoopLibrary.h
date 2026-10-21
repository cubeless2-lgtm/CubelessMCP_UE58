#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessLoopLibrary.generated.h"

/** Internal Editor-only StandardMacros catalog and finite ForLoop/typed int-bool ForEach construction.
 * Public tool registration is provided separately by Python Toolsets. */
UCLASS()
class UCubelessLoopLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Reads only exact stock ForLoop/ForEachLoop source graphs and public pins.
     * Default is find-only. Explicit true allows one exact StandardMacros load;
     * normal engine load/post-load side effects are reported separately.
     * No construction, callbacks, compile/save or cache-populating latency probe.
     */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Diagnostics", meta=(AIIgnore))
    static FString InspectNativeSupport(bool bAllowStockMacroLoad = false);
    /** Finite ForLoop and typed int/bool ForEach node-only operation; append-only descriptors preserve old positions. Existing loaded
     * stock macro and target graph required; no implicit wires/compile/save.
     * Nullable index JSON preserves unspecified vs explicitly supplied defaults.
     */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintLoopNode(const FString& BlueprintPath, const FString& LoopType,
        const FString& FirstIndexJson, const FString& LastIndexJson, const FString& NodePositionJson,
        const FString& GraphName, const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing,
        const FString& ElementType, const FString& TypeObject, const FString& EnumType);
};
