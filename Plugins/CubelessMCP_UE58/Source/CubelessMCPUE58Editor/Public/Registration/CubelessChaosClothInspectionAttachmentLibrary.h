#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosClothInspectionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosClothInspectionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_cloth_inspection_toolset"))
    static FString InstallChaosClothInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_cloth_inspection_toolset"))
    static FString VerifyChaosClothInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_cloth_inspection_toolset"))
    static FString RemoveChaosClothInspectionToolset(UClass* ToolsetClass);
};
