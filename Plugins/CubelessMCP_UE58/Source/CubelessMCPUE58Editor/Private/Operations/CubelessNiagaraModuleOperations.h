#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString InspectNiagaraStockModuleTarget(const FString& SystemPath, const FString& ModuleScriptPath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
    const FString& TargetEmitterName);
FString AddNiagaraStockModuleToStack(const FString& SystemPath, const FString& ModuleScriptPath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
    const FString& TargetEmitterName, const FString& TargetIndexJson, const FString& SuggestedName,
    bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
}
