#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessContentReconstructionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessContentReconstructionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_content_reconstruction_toolset"))
    static FString InstallContentReconstructionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_content_reconstruction_toolset"))
    static FString VerifyContentReconstructionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_content_reconstruction_toolset"))
    static FString RemoveContentReconstructionToolset(UClass* ToolsetClass);
};
