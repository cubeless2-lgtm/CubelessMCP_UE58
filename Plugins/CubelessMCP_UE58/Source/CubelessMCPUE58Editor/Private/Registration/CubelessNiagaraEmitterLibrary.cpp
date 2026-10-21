#include "Registration/CubelessNiagaraEmitterLibrary.h"
#include "Operations/CubelessNiagaraEmitterOperations.h"

FString UCubelessNiagaraEmitterLibrary::InspectNiagaraEmitterAttachTarget(const FString& TargetSystemPath,
    const FString& SourceEmitterPath, const FString& SourceSystemPath,
    const FString& SourceEmitterIndexJson, const FString& SourceEmitterName, const FString& NewEmitterName)
{
    return Cubeless::Operations::InspectNiagaraEmitterAttachTarget(TargetSystemPath, SourceEmitterPath,
        SourceSystemPath, SourceEmitterIndexJson, SourceEmitterName, NewEmitterName);
}
FString UCubelessNiagaraEmitterLibrary::DuplicateOrAttachEmitterFromSource(const FString& TargetSystemPath,
    const FString& SourceEmitterPath, const FString& SourceSystemPath,
    const FString& SourceEmitterIndexJson, const FString& SourceEmitterName, const FString& NewEmitterName,
    const FString& EnabledJson, bool bCopySourceUserParameters, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::DuplicateOrAttachEmitterFromSource(TargetSystemPath, SourceEmitterPath,
        SourceSystemPath, SourceEmitterIndexJson, SourceEmitterName, NewEmitterName, EnabledJson,
        bCopySourceUserParameters, bAllowSourceEdit, bSave, bRequestCompile);
}
