#include "Registration/CubelessNiagaraModuleInputLibrary.h"
#include "Operations/CubelessNiagaraModuleInputOperations.h"

FString UCubelessNiagaraModuleInputLibrary::InspectNiagaraModuleInputTarget(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& ModuleIndexJson, const FString& ModuleName, const FString& ModuleNodeGuid, const FString& InputName)
{
    return Cubeless::Operations::InspectNiagaraModuleInputTarget(SystemPath, EmitterIndexJson, EmitterName, ModuleIndexJson, ModuleName, ModuleNodeGuid, InputName);
}
FString UCubelessNiagaraModuleInputLibrary::SetNiagaraModuleInputLiteralOverride(const FString& SystemPath, const FString& InputName, const FString& ValueJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::SetNiagaraModuleInputLiteralOverride(SystemPath, InputName, ValueJson, EmitterIndexJson, EmitterName, ModuleIndexJson,
        ModuleName, ModuleNodeGuid, bOverwriteExisting, bAllowSourceEdit, bSave, bRequestCompile);
}
FString UCubelessNiagaraModuleInputLibrary::SetNiagaraDataInterfaceModuleInput(const FString& SystemPath, const FString& InputName, const FString& PropertiesJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::SetNiagaraDataInterfaceModuleInput(SystemPath, InputName, PropertiesJson, EmitterIndexJson, EmitterName, ModuleIndexJson,
        ModuleName, ModuleNodeGuid, bOverwriteExisting, bAllowSourceEdit, bSave, bRequestCompile);
}
FString UCubelessNiagaraModuleInputLibrary::SetNiagaraModuleInputLinkedParameter(const FString& SystemPath, const FString& InputName, const FString& LinkedParameterName,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::SetNiagaraModuleInputLinkedParameter(SystemPath, InputName, LinkedParameterName, EmitterIndexJson, EmitterName, ModuleIndexJson,
        ModuleName, ModuleNodeGuid, bOverwriteExisting, bAllowSourceEdit, bSave, bRequestCompile);
}
