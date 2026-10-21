#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::RigVMTopology
{
Variable::FResult Inspect(const FString& ControlRigPath, const FString& ControlRigAssetPath,
    const FString& ControlRigClassPath, bool bIncludePins, bool bIncludeSubPins,
    bool bIncludeLinks, bool bIncludePinLinks, int64 MaxNodes, int64 MaxPinsPerNode,
    int64 MaxSubPinsPerPin, int64 MaxLinks);
}
