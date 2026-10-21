#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimStateMachineVariantLibrary.generated.h"
UCLASS(meta=(AIIgnore))
class UCubelessAnimStateMachineVariantLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_anim_state_machine_sample_variant"))
    static FString EnsureAnimStateMachineSampleVariant(FString SourceAnimBPPath,FString StatesProjectionJson,FString TransitionsProjectionJson,FString TargetAnimBPPath,FString TargetRoot,FString VariantName,FString StateMachineName,FString EntryState,FString AnimationAssetPath,bool bConnectDemoMachineToOutput,bool bReplaceExistingRootLink,bool bReplaceExistingEntryLink,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,int64 MaxStates,int64 MaxTransitions,FString PrivateContextJson);
};
