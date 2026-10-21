#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPhysicalMaterialTraceLibrary.generated.h"

UCLASS()
class UCubelessPhysicalMaterialTraceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="line_trace_physical_material"))
    static FString LineTracePhysicalMaterial(const FString& StartJson, const FString& EndJson,
        const FString& TraceChannel, bool bTraceComplex, bool bReturnPhysicalMaterial);
};
