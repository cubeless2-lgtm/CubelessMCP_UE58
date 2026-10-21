#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimInstanceRuntimeAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimInstanceRuntimeAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no runtime observation or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_anim_instance_runtime_toolset"))
    static FString InstallAnimInstanceRuntimeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_anim_instance_runtime_toolset"))
    static FString VerifyAnimInstanceRuntimeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_anim_instance_runtime_toolset"))
    static FString RemoveAnimInstanceRuntimeToolset(UClass* ToolsetClass);
};
