#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMPinDefaultWriterAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMPinDefaultWriterAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no pin-default writing or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_rigvm_pin_default_writer_toolset"))
    static FString InstallRigVMPinDefaultWriterToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_rigvm_pin_default_writer_toolset"))
    static FString VerifyRigVMPinDefaultWriterToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_rigvm_pin_default_writer_toolset"))
    static FString RemoveRigVMPinDefaultWriterToolset(UClass* ToolsetClass);
};
