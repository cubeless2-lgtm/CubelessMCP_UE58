#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessControlRigRecipeAuthoringAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessControlRigRecipeAuthoringAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_controlrig_recipe_authoring_toolset"))
    static FString InstallControlRigRecipeAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_controlrig_recipe_authoring_toolset"))
    static FString VerifyControlRigRecipeAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_controlrig_recipe_authoring_toolset"))
    static FString RemoveControlRigRecipeAuthoringToolset(UClass* ToolsetClass);
};
