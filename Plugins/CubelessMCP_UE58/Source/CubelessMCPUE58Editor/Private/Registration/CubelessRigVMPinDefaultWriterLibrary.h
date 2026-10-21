#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMPinDefaultWriterLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMPinDefaultWriterLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="set_controlrig_rigvm_pin_defaults"))
    static FString SetControlRigRigVMPinDefaults(const FString& ControlRigPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,
        const FString& PinDefaultsJson,const FString& DefaultsJson,bool bResizeArrays,bool bSetValueOnLinkedPins,
        bool bDryRun,bool bCompile,bool bSaveAsset,int64 MaxPins);
};
