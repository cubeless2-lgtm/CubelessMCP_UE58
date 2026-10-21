#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGInspectionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPCGInspectionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_pcg_inspection_toolset"))
    static FString InstallPCGInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_pcg_inspection_toolset"))
    static FString VerifyPCGInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_pcg_inspection_toolset"))
    static FString RemovePCGInspectionToolset(UClass* ToolsetClass);
};
