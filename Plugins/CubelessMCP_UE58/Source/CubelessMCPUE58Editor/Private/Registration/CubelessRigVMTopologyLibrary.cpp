#include "Registration/CubelessRigVMTopologyLibrary.h"
#include "Compatibility/UE58/CubelessRigVMTopologyApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessRigVMTopologyLibrary::InspectControlRigRigVMTopology(const FString& ControlRigPath,
    const FString& ControlRigAssetPath, const FString& ControlRigClassPath, bool bIncludePins,
    bool bIncludeSubPins, bool bIncludeLinks, bool bIncludePinLinks, int64 MaxNodes,
    int64 MaxPinsPerNode, int64 MaxSubPinsPerPin, int64 MaxLinks)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::RigVMTopology::Inspect(
        ControlRigPath, ControlRigAssetPath, ControlRigClassPath, bIncludePins, bIncludeSubPins,
        bIncludeLinks, bIncludePinLinks, MaxNodes, MaxPinsPerNode, MaxSubPinsPerPin, MaxLinks),
        TEXT("inspect_controlrig_rigvm_topology"));
}
