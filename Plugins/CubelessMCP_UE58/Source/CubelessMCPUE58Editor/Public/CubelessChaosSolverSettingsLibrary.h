#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosSolverSettingsLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessChaosSolverSettingsLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="inspect_chaos_solver_settings"))
    static FString InspectChaosSolverSettings(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="configure_chaos_solver_settings"))
    static FString ConfigureChaosSolverSettings(FString ParamsJson);
};
