#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosCacheSessionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosCacheSessionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Chaos", meta=(ScriptName="record_chaos_cache"))
    static FString RecordChaosCache(FString ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Chaos", meta=(ScriptName="validate_chaos_cache"))
    static FString ValidateChaosCache(FString ParamsJson);
};
