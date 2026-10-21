#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString InspectNiagaraSimulationStageTarget(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName);
FString AddNiagaraGridSimulationStage(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageName, const FString& DataInterfaceName, const FString& InsertIndexJson,
    bool bSkipIfDuplicate, bool bEnabled, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
FString InspectNiagaraSimulationStageSettingsTarget(const FString& SystemPath, const FString& EmitterIndexJson,
    const FString& EmitterName, const FString& StageIndexJson, const FString& StageName);
FString SetNiagaraSimulationStageSettings(const FString& SystemPath, const FString& EmitterIndexJson,
    const FString& EmitterName, const FString& StageIndexJson, const FString& StageName, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
}
