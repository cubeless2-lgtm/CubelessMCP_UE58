#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::NiagaraScratch
{
Function::FResult Inspect(const FString& SystemPath, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& EmitterIndexJson, const FString& EmitterName);
Function::FResult Create(const FString& TargetSystemPath, const FString& SourceScriptPath, const FString& SourceSystemPath,
    const FString& SourceOwnerKind, const FString& SourceScriptIndexJson, const FString& SourceScratchPadName,
    const FString& SourceEmitterIndexJson, const FString& SourceEmitterName, const FString& TargetOwnerKind,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& NewScriptName,
    const FString& TargetSupportedUsage, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
Function::FResult Write(const FString& SystemPath, const FString& ParameterName, const FString& ValueJson,
    const FString& ParameterType, const FString& DefaultValue, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& EmitterIndexJson,
    const FString& EmitterName, bool bSkipIfExists, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
Function::FResult Insert(const FString& TargetSystemPath, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& ScriptEmitterIndexJson,
    const FString& ScriptEmitterName, const FString& TargetUsage, const FString& TargetUsageId,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& TargetIndexJson,
    const FString& SuggestedName, bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
Function::FResult Hlsl(const FString& SystemPath,const FString& TargetPinName,const FString& Expression,
    const FString& ScriptPath,const FString& OwnerKind,const FString& ScriptIndexJson,const FString& ScriptName,
    const FString& EmitterIndexJson,const FString& EmitterName,const FString& TargetNodeGuid,const FString& TargetNodeName,
    const FString& TargetNodeIndexJson,const FString& PreserveExistingLinkAs,const FString& OutputPinName,const FString& SignatureName,
    const FString& CustomHlslScriptUsage,const FString& InputsJson,const FString& UserParameterInputsJson,
    bool bSkipIfAlreadyInserted,bool bReplaceExistingCustom,bool bRebuildExistingCustom,bool bRequiresContext,
    bool bDeleteUnlinkedCustomNodes,bool bDeleteUnlinkedCustomInputSourceNodes,bool bAllowSourceEdit,bool bSave,
    bool bRequestCompile,const FString& ParameterInputsJson);
Function::FResult Rewire(const FString& SystemPath,const FString& InputName,const FString& ScriptName,
    const FString& SignatureName,const FString& ModuleParameterName,bool bDeleteOrphanedSourceNode,
    bool bAllowSourceEdit,bool bSave,bool bRequestCompile);
}
