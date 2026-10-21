#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString InspectNiagaraModuleInputTarget(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& ModuleIndexJson, const FString& ModuleName, const FString& ModuleNodeGuid, const FString& InputName);
FString SetNiagaraModuleInputLiteralOverride(const FString& SystemPath, const FString& InputName, const FString& ValueJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
FString SetNiagaraDataInterfaceModuleInput(const FString& SystemPath, const FString& InputName, const FString& PropertiesJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
FString SetNiagaraModuleInputLinkedParameter(const FString& SystemPath, const FString& InputName, const FString& LinkedParameterName,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
}
