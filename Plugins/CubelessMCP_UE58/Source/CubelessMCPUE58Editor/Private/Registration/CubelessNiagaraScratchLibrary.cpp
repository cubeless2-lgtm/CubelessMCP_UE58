#include "Registration/CubelessNiagaraScratchLibrary.h"
#include "Operations/CubelessNiagaraScratchOperations.h"

FString UCubelessNiagaraScratchLibrary::InspectNiagaraScratchPadTarget(const FString& SystemPath, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& EmitterIndexJson, const FString& EmitterName)
{
    return Cubeless::Operations::InspectNiagaraScratchPadTarget(SystemPath, ScriptPath, OwnerKind, ScriptIndexJson, ScriptName, EmitterIndexJson, EmitterName);
}
FString UCubelessNiagaraScratchLibrary::CreateOrDuplicateScratchPadModule(const FString& TargetSystemPath, const FString& SourceScriptPath, const FString& SourceSystemPath,
    const FString& SourceOwnerKind, const FString& SourceScriptIndexJson, const FString& SourceScratchPadName,
    const FString& SourceEmitterIndexJson, const FString& SourceEmitterName, const FString& TargetOwnerKind,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& NewScriptName,
    const FString& TargetSupportedUsage, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::CreateOrDuplicateScratchPadModule(TargetSystemPath, SourceScriptPath, SourceSystemPath, SourceOwnerKind, SourceScriptIndexJson, SourceScratchPadName, SourceEmitterIndexJson, SourceEmitterName, TargetOwnerKind, TargetEmitterIndexJson, TargetEmitterName, NewScriptName, TargetSupportedUsage, bAllowSourceEdit, bSave, bRequestCompile);
}
FString UCubelessNiagaraScratchLibrary::AddNiagaraScratchPadParameterWrite(const FString& SystemPath, const FString& ParameterName, const FString& ValueJson,
    const FString& ParameterType, const FString& DefaultValue, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& EmitterIndexJson,
    const FString& EmitterName, bool bSkipIfExists, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::AddNiagaraScratchPadParameterWrite(SystemPath, ParameterName, ValueJson, ParameterType, DefaultValue, ScriptPath, OwnerKind, ScriptIndexJson, ScriptName, EmitterIndexJson, EmitterName, bSkipIfExists, bAllowSourceEdit, bSave, bRequestCompile);
}
FString UCubelessNiagaraScratchLibrary::AddScratchPadModuleToStack(const FString& TargetSystemPath, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& ScriptEmitterIndexJson,
    const FString& ScriptEmitterName, const FString& TargetUsage, const FString& TargetUsageId,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& TargetIndexJson,
    const FString& SuggestedName, bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::AddScratchPadModuleToStack(TargetSystemPath, ScriptPath, OwnerKind, ScriptIndexJson, ScriptName, ScriptEmitterIndexJson, ScriptEmitterName, TargetUsage, TargetUsageId, TargetEmitterIndexJson, TargetEmitterName, TargetIndexJson, SuggestedName, bSkipIfDuplicate, bAllowSourceEdit, bSave, bRequestCompile);
}
FString UCubelessNiagaraScratchLibrary::InsertNiagaraScratchPadCustomHlslForPin(const FString& SystemPath,const FString& TargetPinName,const FString& Expression,
    const FString& ScriptPath,const FString& OwnerKind,const FString& ScriptIndexJson,const FString& ScriptName,
    const FString& EmitterIndexJson,const FString& EmitterName,const FString& TargetNodeGuid,const FString& TargetNodeName,
    const FString& TargetNodeIndexJson,const FString& PreserveExistingLinkAs,const FString& OutputPinName,const FString& SignatureName,
    const FString& CustomHlslScriptUsage,const FString& InputsJson,const FString& UserParameterInputsJson,
    bool bSkipIfAlreadyInserted,bool bReplaceExistingCustom,bool bRebuildExistingCustom,bool bRequiresContext,
    bool bDeleteUnlinkedCustomNodes,bool bDeleteUnlinkedCustomInputSourceNodes,bool bAllowSourceEdit,bool bSave,
    bool bRequestCompile,const FString& ParameterInputsJson)
{
    return Cubeless::Operations::InsertNiagaraScratchPadCustomHlslForPin(SystemPath, TargetPinName, Expression, ScriptPath, OwnerKind, ScriptIndexJson, ScriptName, EmitterIndexJson, EmitterName, TargetNodeGuid, TargetNodeName, TargetNodeIndexJson, PreserveExistingLinkAs, OutputPinName, SignatureName, CustomHlslScriptUsage, InputsJson, UserParameterInputsJson, bSkipIfAlreadyInserted, bReplaceExistingCustom, bRebuildExistingCustom, bRequiresContext, bDeleteUnlinkedCustomNodes, bDeleteUnlinkedCustomInputSourceNodes, bAllowSourceEdit, bSave, bRequestCompile, ParameterInputsJson);
}
FString UCubelessNiagaraScratchLibrary::RewireNiagaraCustomHlslInputToModuleParameter(const FString& SystemPath,const FString& InputName,const FString& ScriptName,
    const FString& SignatureName,const FString& ModuleParameterName,bool bDeleteOrphanedSourceNode,
    bool bAllowSourceEdit,bool bSave,bool bRequestCompile)
{
    return Cubeless::Operations::RewireNiagaraCustomHlslInputToModuleParameter(SystemPath, InputName, ScriptName, SignatureName, ModuleParameterName, bDeleteOrphanedSourceNode, bAllowSourceEdit, bSave, bRequestCompile);
}
