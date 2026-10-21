#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::RigVMPinDefaultValidation
{
Variable::FResult Validate(const FString& ControlRigPath, const FString& ControlRigAssetPath,
    const FString& ControlRigClassPath, const FString& PinDefaultsJson, const FString& DefaultsJson,
    const FString& PinsJson, int64 MaxPins);
}
