#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMPinDefaultValidationLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMPinDefaultValidationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="validate_controlrig_rigvm_pin_defaults"))
    static FString ValidateControlRigRigVMPinDefaults(const FString& ControlRigPath, const FString& ControlRigAssetPath,
    const FString& ControlRigClassPath, const FString& PinDefaultsJson, const FString& DefaultsJson,
    const FString& PinsJson, int64 MaxPins);
};
