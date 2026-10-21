#include "Operations/CubelessNiagaraScratchOperations.h"
#include "Compatibility/UE58/CubelessNiagaraScratchApi.h"
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
} // anonymous namespace
FString InsertNiagaraScratchPadCustomHlslForPin(const FString& SystemPath,const FString& TargetPinName,const FString& Expression,
    const FString& ScriptPath,const FString& OwnerKind,const FString& ScriptIndexJson,const FString& ScriptName,
    const FString& EmitterIndexJson,const FString& EmitterName,const FString& TargetNodeGuid,const FString& TargetNodeName,
    const FString& TargetNodeIndexJson,const FString& PreserveExistingLinkAs,const FString& OutputPinName,const FString& SignatureName,
    const FString& CustomHlslScriptUsage,const FString& InputsJson,const FString& UserParameterInputsJson,
    bool bSkipIfAlreadyInserted,bool bReplaceExistingCustom,bool bRebuildExistingCustom,bool bRequiresContext,
    bool bDeleteUnlinkedCustomNodes,bool bDeleteUnlinkedCustomInputSourceNodes,bool bAllowSourceEdit,bool bSave,
    bool bRequestCompile,const FString& ParameterInputsJson)
{
    return Encode(UE58::NiagaraScratch::Hlsl(SystemPath, TargetPinName, Expression, ScriptPath, OwnerKind, ScriptIndexJson, ScriptName, EmitterIndexJson, EmitterName, TargetNodeGuid, TargetNodeName, TargetNodeIndexJson, PreserveExistingLinkAs, OutputPinName, SignatureName, CustomHlslScriptUsage, InputsJson, UserParameterInputsJson, bSkipIfAlreadyInserted, bReplaceExistingCustom, bRebuildExistingCustom, bRequiresContext, bDeleteUnlinkedCustomNodes, bDeleteUnlinkedCustomInputSourceNodes, bAllowSourceEdit, bSave, bRequestCompile, ParameterInputsJson), TEXT("insert_niagara_scratch_pad_custom_hlsl_for_pin"));
}
FString RewireNiagaraCustomHlslInputToModuleParameter(const FString& SystemPath,const FString& InputName,const FString& ScriptName,
    const FString& SignatureName,const FString& ModuleParameterName,bool bDeleteOrphanedSourceNode,
    bool bAllowSourceEdit,bool bSave,bool bRequestCompile)
{
    return Encode(UE58::NiagaraScratch::Rewire(SystemPath, InputName, ScriptName, SignatureName, ModuleParameterName, bDeleteOrphanedSourceNode, bAllowSourceEdit, bSave, bRequestCompile), TEXT("rewire_niagara_custom_hlsl_input_to_module_parameter"));
}
FString InspectNiagaraScratchPadTarget(const FString& SystemPath, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& EmitterIndexJson, const FString& EmitterName)
{
    return Encode(UE58::NiagaraScratch::Inspect(SystemPath, ScriptPath, OwnerKind, ScriptIndexJson, ScriptName, EmitterIndexJson, EmitterName), TEXT("inspect_niagara_scratch_pad_target"));
}
FString CreateOrDuplicateScratchPadModule(const FString& TargetSystemPath, const FString& SourceScriptPath, const FString& SourceSystemPath,
    const FString& SourceOwnerKind, const FString& SourceScriptIndexJson, const FString& SourceScratchPadName,
    const FString& SourceEmitterIndexJson, const FString& SourceEmitterName, const FString& TargetOwnerKind,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& NewScriptName,
    const FString& TargetSupportedUsage, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraScratch::Create(TargetSystemPath, SourceScriptPath, SourceSystemPath, SourceOwnerKind, SourceScriptIndexJson, SourceScratchPadName, SourceEmitterIndexJson, SourceEmitterName, TargetOwnerKind, TargetEmitterIndexJson, TargetEmitterName, NewScriptName, TargetSupportedUsage, bAllowSourceEdit, bSave, bRequestCompile), TEXT("create_or_duplicate_scratch_pad_module"));
}
FString AddNiagaraScratchPadParameterWrite(const FString& SystemPath, const FString& ParameterName, const FString& ValueJson,
    const FString& ParameterType, const FString& DefaultValue, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& EmitterIndexJson,
    const FString& EmitterName, bool bSkipIfExists, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraScratch::Write(SystemPath, ParameterName, ValueJson, ParameterType, DefaultValue, ScriptPath, OwnerKind, ScriptIndexJson, ScriptName, EmitterIndexJson, EmitterName, bSkipIfExists, bAllowSourceEdit, bSave, bRequestCompile), TEXT("add_niagara_scratch_pad_parameter_write"));
}
FString AddScratchPadModuleToStack(const FString& TargetSystemPath, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& ScriptEmitterIndexJson,
    const FString& ScriptEmitterName, const FString& TargetUsage, const FString& TargetUsageId,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& TargetIndexJson,
    const FString& SuggestedName, bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraScratch::Insert(TargetSystemPath, ScriptPath, OwnerKind, ScriptIndexJson, ScriptName, ScriptEmitterIndexJson, ScriptEmitterName, TargetUsage, TargetUsageId, TargetEmitterIndexJson, TargetEmitterName, TargetIndexJson, SuggestedName, bSkipIfDuplicate, bAllowSourceEdit, bSave, bRequestCompile), TEXT("add_scratch_pad_module_to_stack"));
}
}
