#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimStateMachineVariant
{
Variable::FResult Ensure(const FString& SourceAnimBPPath,const FString& StatesProjectionJson,const FString& TransitionsProjectionJson,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,const FString& StateMachineName,const FString& EntryState,const FString& AnimationAssetPath,bool bConnectDemoMachineToOutput,bool bReplaceExistingRootLink,bool bReplaceExistingEntryLink,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,int64 MaxStates,int64 MaxTransitions,const FString& PrivateContextJson);
}
