#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMakeArrayLibrary.generated.h"

/** Internal Editor-only typed collection producer. Public named tools are Python registrations. */
UCLASS()
class UCubelessMakeArrayLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Creates only one int/bool MakeArray node in an existing loaded graph.
     * Native count/default/pin observations are reported; no wires, compile or save.
     * Other descriptors/graph creation remain explicit incomplete contracts.
     */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore))
    static FString AddBlueprintMakeArrayNode(const FString& BlueprintPath, const FString& ElementType,
        const FString& InputCountJson, const FString& ValuesJson, const FString& TypeObject,
        const FString& EnumType, const FString& NodePositionJson, const FString& GraphName,
        const FString& GraphId, const FString& GraphType, bool bCreateGraphIfMissing);
};
