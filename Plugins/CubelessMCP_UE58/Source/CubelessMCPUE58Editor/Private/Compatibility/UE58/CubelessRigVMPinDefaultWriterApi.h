#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::RigVMPinDefaultWriter
{
Variable::FResult Set(const FString& ControlRigPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,
    const FString& PinDefaultsJson,const FString& DefaultsJson,bool bResizeArrays,bool bSetValueOnLinkedPins,
    bool bDryRun,bool bCompile,bool bSaveAsset,int64 MaxPins);
}
