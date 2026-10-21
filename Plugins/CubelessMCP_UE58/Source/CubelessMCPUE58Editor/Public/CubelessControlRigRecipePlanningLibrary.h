#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessControlRigRecipePlanningLibrary.generated.h"

// Internal RefSkeleton metadata and pure JSON preset delegate; public tools and evidence stay in Python.
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessControlRigRecipePlanningLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="profile_controlrig_skeleton_metadata"))
    static FString ProfileControlRigSkeleton(const FString& RequestJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|ControlRig", meta=(AIIgnore, ScriptName="make_controlrig_preset_recipe_data"))
    static FString MakeControlRigPresetRecipe(const FString& PresetJson);
};
