#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMTopologyLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMTopologyLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="inspect_controlrig_rigvm_topology"))
    static FString InspectControlRigRigVMTopology(const FString& ControlRigPath, const FString& ControlRigAssetPath,
        const FString& ControlRigClassPath, bool bIncludePins, bool bIncludeSubPins,
        bool bIncludeLinks, bool bIncludePinLinks, int64 MaxNodes, int64 MaxPinsPerNode,
        int64 MaxSubPinsPerPin, int64 MaxLinks);
};
