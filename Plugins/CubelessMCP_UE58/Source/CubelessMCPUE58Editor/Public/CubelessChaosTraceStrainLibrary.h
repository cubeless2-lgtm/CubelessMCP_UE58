#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosTraceStrainLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosTraceStrainLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Chaos", meta=(ScriptName="trigger_trace_external_strain"))
    static FString TriggerTraceExternalStrain(FString ParamsJson);
};
