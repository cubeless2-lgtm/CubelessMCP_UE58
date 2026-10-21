#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMLinkValidationAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMLinkValidationAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no link validation or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_rigvm_link_validation_toolset"))
    static FString InstallRigVMLinkValidationToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_rigvm_link_validation_toolset"))
    static FString VerifyRigVMLinkValidationToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_rigvm_link_validation_toolset"))
    static FString RemoveRigVMLinkValidationToolset(UClass* ToolsetClass);
};
