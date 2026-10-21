#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessControlRigRecipeAuthoringLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessControlRigRecipeAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="validate_controlrig_recipe"))
    static FString ValidateControlRigRecipe(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="dry_run_controlrig_recipe"))
    static FString DryRunControlRigRecipe(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="build_controlrig_recipe"))
    static FString BuildControlRigRecipe(const FString& ParamsJson);
};
