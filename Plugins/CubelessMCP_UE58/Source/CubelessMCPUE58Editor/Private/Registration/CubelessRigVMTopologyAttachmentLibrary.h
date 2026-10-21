#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMTopologyAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMTopologyAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no topology inspection or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_rigvm_topology_toolset"))
    static FString InstallRigVMTopologyToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_rigvm_topology_toolset"))
    static FString VerifyRigVMTopologyToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_rigvm_topology_toolset"))
    static FString RemoveRigVMTopologyToolset(UClass* ToolsetClass);
};
