#include "Operations/CubelessNiagaraModuleInputOperations.h"
#include "Compatibility/UE58/CubelessNiagaraModuleInputApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Serialization/JsonSerializer.h"

namespace Cubeless::Operations
{
namespace
{
FString Encode(const UE58::Function::FResult& R, const TCHAR* Operation)
{
    const FString Base = EncodeFunctionResult(R, Operation); TSharedPtr<FJsonObject> J;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Base), J) || !J.IsValid()) { return Base; }
    for (const TCHAR* Key : {TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"),
        TEXT("save_attempted"), TEXT("saved"), TEXT("save_deferred")})
    { bool Value = false; if (R.Data->TryGetBoolField(Key, Value)) { J->SetBoolField(Key, Value); } }
    J->SetBoolField(TEXT("outcome_unknown"), !R.bChangeStateKnown);
    J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    FString Out; FJsonSerializer::Serialize(J.ToSharedRef(), TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
FString InspectNiagaraModuleInputTarget(const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& ModuleIndexJson, const FString& ModuleName, const FString& ModuleNodeGuid, const FString& InputName)
{
    return Encode(UE58::NiagaraModuleInput::Inspect(SystemPath, EmitterIndexJson, EmitterName, ModuleIndexJson, ModuleName, ModuleNodeGuid, InputName), TEXT("inspect_niagara_module_input_target"));
}
FString SetNiagaraModuleInputLiteralOverride(const FString& SystemPath, const FString& InputName, const FString& ValueJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraModuleInput::SetLiteral(SystemPath, InputName, ValueJson, EmitterIndexJson, EmitterName, ModuleIndexJson,
        ModuleName, ModuleNodeGuid, bOverwriteExisting, bAllowSourceEdit, bSave, bRequestCompile), TEXT("set_niagara_module_input_literal_override"));
}
FString SetNiagaraDataInterfaceModuleInput(const FString& SystemPath, const FString& InputName, const FString& PropertiesJson,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraModuleInput::SetDataInterface(SystemPath, InputName, PropertiesJson, EmitterIndexJson, EmitterName, ModuleIndexJson,
        ModuleName, ModuleNodeGuid, bOverwriteExisting, bAllowSourceEdit, bSave, bRequestCompile), TEXT("set_niagara_data_interface_module_input"));
}
FString SetNiagaraModuleInputLinkedParameter(const FString& SystemPath, const FString& InputName, const FString& LinkedParameterName,
    const FString& EmitterIndexJson, const FString& EmitterName, const FString& ModuleIndexJson,
    const FString& ModuleName, const FString& ModuleNodeGuid, bool bOverwriteExisting, bool bAllowSourceEdit,
    bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraModuleInput::SetLinked(SystemPath, InputName, LinkedParameterName, EmitterIndexJson, EmitterName, ModuleIndexJson,
        ModuleName, ModuleNodeGuid, bOverwriteExisting, bAllowSourceEdit, bSave, bRequestCompile), TEXT("set_niagara_module_input_linked_parameter"));
}
}
