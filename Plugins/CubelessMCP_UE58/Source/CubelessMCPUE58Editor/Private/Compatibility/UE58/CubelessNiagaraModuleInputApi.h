#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::NiagaraModuleInput
{
Function::FResult Inspect(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& ModuleIndexJson, const FString& ModuleName, const FString& ModuleNodeGuid, const FString& InputName);
Function::FResult SetLiteral(const FString& SystemPath, const FString& InputName, const FString& ValueJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
Function::FResult SetDataInterface(const FString& SystemPath, const FString& InputName, const FString& PropertiesJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
Function::FResult SetLinked(const FString& SystemPath, const FString& InputName, const FString& LinkedParameterName,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile);
}
