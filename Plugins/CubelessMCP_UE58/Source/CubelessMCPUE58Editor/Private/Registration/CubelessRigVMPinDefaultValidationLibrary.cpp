#include "Registration/CubelessRigVMPinDefaultValidationLibrary.h"
#include "Compatibility/UE58/CubelessRigVMPinDefaultValidationApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessRigVMPinDefaultValidationLibrary::ValidateControlRigRigVMPinDefaults(const FString& ControlRigPath, const FString& ControlRigAssetPath,
    const FString& ControlRigClassPath, const FString& PinDefaultsJson, const FString& DefaultsJson,
    const FString& PinsJson, int64 MaxPins)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::RigVMPinDefaultValidation::Validate(
        ControlRigPath, ControlRigAssetPath, ControlRigClassPath, PinDefaultsJson, DefaultsJson, PinsJson, MaxPins),
        TEXT("validate_controlrig_rigvm_pin_defaults"));
}
