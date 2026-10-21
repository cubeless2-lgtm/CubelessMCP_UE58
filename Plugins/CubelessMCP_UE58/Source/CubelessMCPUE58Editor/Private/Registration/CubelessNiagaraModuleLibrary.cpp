#include "Registration/CubelessNiagaraModuleLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraModuleApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Operations/CubelessNiagaraModuleOperations.h"

FString UCubelessNiagaraModuleLibrary::InspectNiagaraStockModuleTarget(const FString& SystemPath,
    const FString& ModuleScriptPath, const FString& TargetUsage, const FString& TargetUsageId,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName)
{
    return Cubeless::Operations::InspectNiagaraStockModuleTarget(SystemPath, ModuleScriptPath, TargetUsage,
        TargetUsageId, TargetEmitterIndexJson, TargetEmitterName);
}
FString UCubelessNiagaraModuleLibrary::AddNiagaraStockModuleToStack(const FString& SystemPath,
    const FString& ModuleScriptPath, const FString& TargetUsage, const FString& TargetUsageId,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& TargetIndexJson,
    const FString& SuggestedName, bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::AddNiagaraStockModuleToStack(SystemPath, ModuleScriptPath, TargetUsage,
        TargetUsageId, TargetEmitterIndexJson, TargetEmitterName, TargetIndexJson, SuggestedName,
        bSkipIfDuplicate, bAllowSourceEdit, bSave, bRequestCompile);
}

FString UCubelessNiagaraModuleLibrary::SetNiagaraModuleInputsBatch(const FString& SystemPath, const FString& EditsJson, const FString& Operation,
    bool bOverwriteExisting, bool bContinueOnError, bool bAllowSourceEdit, bool bSave, bool bRequestCompile,
    bool bWaitForCompletion, double TimeoutSeconds, double PollIntervalSeconds)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::NiagaraModule::SetInputsBatch(SystemPath, EditsJson, Operation,
        bOverwriteExisting, bContinueOnError, bAllowSourceEdit, bSave, bRequestCompile, bWaitForCompletion, TimeoutSeconds, PollIntervalSeconds),
        TEXT("set_niagara_module_inputs_batch"));
}

FString UCubelessNiagaraModuleLibrary::InspectNiagaraModuleRapidIteration(const FString& SystemPath)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::NiagaraModule::InspectRapidIteration(SystemPath),
        TEXT("inspect_niagara_module_rapid_iteration"));
}
