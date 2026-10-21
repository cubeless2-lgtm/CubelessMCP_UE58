#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPostProcessAnimDemoVariantAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPostProcessAnimDemoVariantAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_postprocess_anim_demo_variant_toolset"))
    static FString InstallPostProcessAnimDemoVariantToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_postprocess_anim_demo_variant_toolset"))
    static FString VerifyPostProcessAnimDemoVariantToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_postprocess_anim_demo_variant_toolset"))
    static FString RemovePostProcessAnimDemoVariantToolset(UClass* ToolsetClass);
};
