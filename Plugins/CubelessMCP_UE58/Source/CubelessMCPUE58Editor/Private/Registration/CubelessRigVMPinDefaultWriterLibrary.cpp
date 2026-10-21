#include "Registration/CubelessRigVMPinDefaultWriterLibrary.h"
#include "Compatibility/UE58/CubelessRigVMPinDefaultWriterApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessRigVMPinDefaultWriterLibrary::SetControlRigRigVMPinDefaults(const FString& ControlRigPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,
    const FString& PinDefaultsJson,const FString& DefaultsJson,bool bResizeArrays,bool bSetValueOnLinkedPins,
    bool bDryRun,bool bCompile,bool bSaveAsset,int64 MaxPins)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::RigVMPinDefaultWriter::Set(
        ControlRigPath,ControlRigAssetPath,ControlRigClassPath,PinDefaultsJson,DefaultsJson,bResizeArrays,bSetValueOnLinkedPins,bDryRun,bCompile,bSaveAsset,MaxPins),
        TEXT("set_controlrig_rigvm_pin_defaults"));
}
