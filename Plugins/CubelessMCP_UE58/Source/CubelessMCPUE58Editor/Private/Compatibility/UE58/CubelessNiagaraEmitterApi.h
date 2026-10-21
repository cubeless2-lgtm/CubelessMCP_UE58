#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::NiagaraEmitter
{
Function::FResult Inspect(const FString& TargetSystemPath, const FString& SourceEmitterPath,
    const FString& SourceSystemPath, const FString& SourceEmitterIndexJson,
    const FString& SourceEmitterName, const FString& NewEmitterName);
Function::FResult Attach(const FString& TargetSystemPath, const FString& SourceEmitterPath,
    const FString& SourceSystemPath, const FString& SourceEmitterIndexJson,
    const FString& SourceEmitterName, const FString& NewEmitterName, const FString& EnabledJson,
    bool bCopySourceUserParameters, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
}
