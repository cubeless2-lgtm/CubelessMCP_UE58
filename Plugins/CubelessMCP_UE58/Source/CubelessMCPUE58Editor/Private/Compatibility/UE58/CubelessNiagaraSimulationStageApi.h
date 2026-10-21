#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::NiagaraSimulationStage
{
Function::FResult Inspect(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName);
Function::FResult Add(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageName, const FString& DataInterfaceName, const FString& InsertIndexJson,
    bool bSkipIfDuplicate, bool bEnabled, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
Function::FResult InspectSettings(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageIndexJson, const FString& StageName);
Function::FResult SetSettings(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageIndexJson, const FString& StageName, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
}
