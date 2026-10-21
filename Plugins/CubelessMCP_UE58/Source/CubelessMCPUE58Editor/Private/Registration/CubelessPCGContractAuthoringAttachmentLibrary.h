#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGContractAuthoringAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPCGContractAuthoringAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_pcg_contract_authoring_toolset"))
    static FString InstallPCGContractAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_pcg_contract_authoring_toolset"))
    static FString VerifyPCGContractAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_pcg_contract_authoring_toolset"))
    static FString RemovePCGContractAuthoringToolset(UClass* ToolsetClass);
};
