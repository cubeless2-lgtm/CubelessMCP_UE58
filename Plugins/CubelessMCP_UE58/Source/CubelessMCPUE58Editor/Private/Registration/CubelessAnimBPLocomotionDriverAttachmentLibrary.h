#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimBPLocomotionDriverAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimBPLocomotionDriverAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_anim_bp_locomotion_driver_toolset"))
    static FString InstallAnimBPLocomotionDriverToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_anim_bp_locomotion_driver_toolset"))
    static FString VerifyAnimBPLocomotionDriverToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_anim_bp_locomotion_driver_toolset"))
    static FString RemoveAnimBPLocomotionDriverToolset(UClass* ToolsetClass);
};
