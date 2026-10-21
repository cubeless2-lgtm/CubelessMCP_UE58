#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraScratchLibrary.generated.h"

/** Hidden Editor-only scratch leaves. Python owns the durable journal and
 * official registration; generated assets reference native Niagara types only. */
UCLASS()
class UCubelessNiagaraScratchLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString InspectNiagaraScratchPadTarget(const FString& SystemPath, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& EmitterIndexJson, const FString& EmitterName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString CreateOrDuplicateScratchPadModule(const FString& TargetSystemPath, const FString& SourceScriptPath, const FString& SourceSystemPath,
    const FString& SourceOwnerKind, const FString& SourceScriptIndexJson, const FString& SourceScratchPadName,
    const FString& SourceEmitterIndexJson, const FString& SourceEmitterName, const FString& TargetOwnerKind,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& NewScriptName,
    const FString& TargetSupportedUsage, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString AddNiagaraScratchPadParameterWrite(const FString& SystemPath, const FString& ParameterName, const FString& ValueJson,
    const FString& ParameterType, const FString& DefaultValue, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& EmitterIndexJson,
    const FString& EmitterName, bool bSkipIfExists, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString AddScratchPadModuleToStack(const FString& TargetSystemPath, const FString& ScriptPath, const FString& OwnerKind,
    const FString& ScriptIndexJson, const FString& ScriptName, const FString& ScriptEmitterIndexJson,
    const FString& ScriptEmitterName, const FString& TargetUsage, const FString& TargetUsageId,
    const FString& TargetEmitterIndexJson, const FString& TargetEmitterName, const FString& TargetIndexJson,
    const FString& SuggestedName, bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString InsertNiagaraScratchPadCustomHlslForPin(const FString& SystemPath,const FString& TargetPinName,const FString& Expression,
    const FString& ScriptPath,const FString& OwnerKind,const FString& ScriptIndexJson,const FString& ScriptName,
    const FString& EmitterIndexJson,const FString& EmitterName,const FString& TargetNodeGuid,const FString& TargetNodeName,
    const FString& TargetNodeIndexJson,const FString& PreserveExistingLinkAs,const FString& OutputPinName,const FString& SignatureName,
    const FString& CustomHlslScriptUsage,const FString& InputsJson,const FString& UserParameterInputsJson,
    bool bSkipIfAlreadyInserted,bool bReplaceExistingCustom,bool bRebuildExistingCustom,bool bRequiresContext,
    bool bDeleteUnlinkedCustomNodes,bool bDeleteUnlinkedCustomInputSourceNodes,bool bAllowSourceEdit,bool bSave,
    bool bRequestCompile,const FString& ParameterInputsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore))
    static FString RewireNiagaraCustomHlslInputToModuleParameter(const FString& SystemPath,const FString& InputName,const FString& ScriptName,
    const FString& SignatureName,const FString& ModuleParameterName,bool bDeleteOrphanedSourceNode,
    bool bAllowSourceEdit,bool bSave,bool bRequestCompile);
};
