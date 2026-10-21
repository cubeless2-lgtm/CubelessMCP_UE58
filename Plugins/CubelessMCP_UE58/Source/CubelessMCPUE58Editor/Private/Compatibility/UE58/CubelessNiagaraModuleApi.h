#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "Templates/Function.h"
class UNiagaraSystem;

namespace Cubeless::UE58::NiagaraModule
{
Function::FResult Inspect(const FString& SystemPath, const FString& ModuleScriptPath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
    const FString& TargetEmitterName);
Function::FResult Add(const FString& SystemPath, const FString& ModuleScriptPath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
    const FString& TargetEmitterName, const FString& TargetIndexJson, const FString& SuggestedName,
    bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
}

#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::NiagaraModule
{
Variable::FResult SetInputsBatch(const FString& SystemPath, const FString& EditsJson, const FString& Operation,
    bool bOverwriteExisting, bool bContinueOnError, bool bAllowSourceEdit, bool bSave, bool bRequestCompile,
    bool bWaitForCompletion, double TimeoutSeconds, double PollIntervalSeconds,
    const TFunction<bool(UNiagaraSystem*,const TCHAR*)>* OwnedAuthoringGuard=nullptr);
}

namespace Cubeless::UE58::NiagaraModule
{
Variable::FResult InspectRapidIteration(const FString& SystemPath);
}
