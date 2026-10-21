#include "CubelessChaosSolverSettingsLibrary.h"
#include "Compatibility/UE58/CubelessChaosSolverSettingsApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessChaosSolverSettingsLibrary::InspectChaosSolverSettings(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ChaosSolverSettings::Inspect(ParamsJson),TEXT("inspect_chaos_solver_settings"));
}
FString UCubelessChaosSolverSettingsLibrary::ConfigureChaosSolverSettings(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ChaosSolverSettings::Configure(ParamsJson),TEXT("configure_chaos_solver_settings"));
}
