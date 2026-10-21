#include "Operations/CubelessNiagaraSimulationStageOperations.h"
#include "Compatibility/UE58/CubelessNiagaraSimulationStageApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Serialization/JsonSerializer.h"

namespace Cubeless::Operations
{
namespace
{
FString Encode(const UE58::Function::FResult& R, const TCHAR* Operation)
{
    const FString Base = EncodeFunctionResult(R, Operation); TSharedPtr<FJsonObject> J;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Base), J) || !J.IsValid()) { return Base; }
    constexpr const TCHAR* Flags[] = {TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"),
        TEXT("save_attempted"), TEXT("saved"), TEXT("save_deferred")};
    for (const TCHAR* Flag : Flags) { bool Value = false; if (R.Data->TryGetBoolField(Flag, Value)) { J->SetBoolField(Flag, Value); } }
    J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    FString Out; FJsonSerializer::Serialize(J.ToSharedRef(), TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
FString InspectNiagaraSimulationStageTarget(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName)
{
    return Encode(UE58::NiagaraSimulationStage::Inspect(SystemPath, EmitterIndexJson, EmitterName),
        TEXT("inspect_niagara_simulation_stage_target"));
}
FString AddNiagaraGridSimulationStage(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageName, const FString& DataInterfaceName, const FString& InsertIndexJson,
    bool bSkipIfDuplicate, bool bEnabled, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraSimulationStage::Add(SystemPath, EmitterIndexJson, EmitterName,
        StageName, DataInterfaceName, InsertIndexJson, bSkipIfDuplicate, bEnabled, OptionsJson,
        bAllowSourceEdit, bSave, bRequestCompile), TEXT("add_niagara_grid_simulation_stage"));
}
FString InspectNiagaraSimulationStageSettingsTarget(const FString& SystemPath, const FString& EmitterIndexJson,
    const FString& EmitterName, const FString& StageIndexJson, const FString& StageName)
{
    return Encode(UE58::NiagaraSimulationStage::InspectSettings(SystemPath, EmitterIndexJson, EmitterName,
        StageIndexJson, StageName), TEXT("inspect_niagara_simulation_stage_settings_target"));
}
FString SetNiagaraSimulationStageSettings(const FString& SystemPath, const FString& EmitterIndexJson,
    const FString& EmitterName, const FString& StageIndexJson, const FString& StageName, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraSimulationStage::SetSettings(SystemPath, EmitterIndexJson, EmitterName,
        StageIndexJson, StageName, OptionsJson, bAllowSourceEdit, bSave, bRequestCompile),
        TEXT("set_niagara_simulation_stage_settings"));
}
}
