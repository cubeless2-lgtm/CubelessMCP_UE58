#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessControlRigRecipePlanningAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessControlRigRecipePlanningAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_controlrig_recipe_planning_toolset"))
    static FString InstallControlRigRecipePlanningToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_controlrig_recipe_planning_toolset"))
    static FString VerifyControlRigRecipePlanningToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_controlrig_recipe_planning_toolset"))
    static FString RemoveControlRigRecipePlanningToolset(UClass* ToolsetClass);
};
