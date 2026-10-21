#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigVMLinkWriterAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessRigVMLinkWriterAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no link writing or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_rigvm_link_writer_toolset"))
    static FString InstallRigVMLinkWriterToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_rigvm_link_writer_toolset"))
    static FString VerifyRigVMLinkWriterToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_rigvm_link_writer_toolset"))
    static FString RemoveRigVMLinkWriterToolset(UClass* ToolsetClass);
};
