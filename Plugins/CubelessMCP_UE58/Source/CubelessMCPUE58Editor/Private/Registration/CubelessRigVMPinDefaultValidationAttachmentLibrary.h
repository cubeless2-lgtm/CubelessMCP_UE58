#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMPinDefaultValidationAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMPinDefaultValidationAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no pin-default validation or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_rigvm_pin_default_validation_toolset"))
    static FString InstallRigVMPinDefaultValidationToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_rigvm_pin_default_validation_toolset"))
    static FString VerifyRigVMPinDefaultValidationToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_rigvm_pin_default_validation_toolset"))
    static FString RemoveRigVMPinDefaultValidationToolset(UClass* ToolsetClass);
};
