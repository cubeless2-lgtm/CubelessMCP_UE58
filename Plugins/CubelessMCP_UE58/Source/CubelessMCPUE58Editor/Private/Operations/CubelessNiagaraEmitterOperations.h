#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString InspectNiagaraEmitterAttachTarget(const FString& TargetSystemPath, const FString& SourceEmitterPath,
    const FString& SourceSystemPath, const FString& SourceEmitterIndexJson,
    const FString& SourceEmitterName, const FString& NewEmitterName);
FString DuplicateOrAttachEmitterFromSource(const FString& TargetSystemPath, const FString& SourceEmitterPath,
    const FString& SourceSystemPath, const FString& SourceEmitterIndexJson,
    const FString& SourceEmitterName, const FString& NewEmitterName, const FString& EnabledJson,
    bool bCopySourceUserParameters, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
}
