#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimStateMachineVariantAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimStateMachineVariantAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_anim_state_machine_variant_toolset"))
    static FString InstallAnimStateMachineVariantToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_anim_state_machine_variant_toolset"))
    static FString VerifyAnimStateMachineVariantToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_anim_state_machine_variant_toolset"))
    static FString RemoveAnimStateMachineVariantToolset(UClass* ToolsetClass);
};
