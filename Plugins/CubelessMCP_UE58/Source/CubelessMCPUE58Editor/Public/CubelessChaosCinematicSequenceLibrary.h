#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosCinematicSequenceLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessChaosCinematicSequenceLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="create_chaos_level_sequence"))
    static FString CreateChaosLevelSequence(FString ParamsJson);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="bind_chaos_cache_to_sequence"))
    static FString BindChaosCacheToSequence(FString ParamsJson);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="configure_chaos_sequence_timing"))
    static FString ConfigureChaosSequenceTiming(FString ParamsJson);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="inspect_chaos_cinematic_pipeline"))
    static FString InspectChaosCinematicPipeline(FString ParamsJson);
};
