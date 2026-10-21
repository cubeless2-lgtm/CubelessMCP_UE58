#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessLinkedAnimGraphVariantAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessLinkedAnimGraphVariantAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_linked_anim_graph_variant_toolset"))
    static FString InstallLinkedAnimGraphVariantToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_linked_anim_graph_variant_toolset"))
    static FString VerifyLinkedAnimGraphVariantToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_linked_anim_graph_variant_toolset"))
    static FString RemoveLinkedAnimGraphVariantToolset(UClass* ToolsetClass);
};
