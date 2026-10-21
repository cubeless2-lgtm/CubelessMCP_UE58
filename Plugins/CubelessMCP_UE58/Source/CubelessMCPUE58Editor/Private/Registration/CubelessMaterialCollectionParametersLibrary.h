#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialCollectionParametersLibrary.generated.h"

/** Hidden Material-only node-baking operation; canonical domain owns journaling. */
UCLASS()
class UCubelessMaterialCollectionParametersLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore, ScriptName="replace_material_collection_parameters"))
    static FString ReplaceMaterialCollectionParameters(const FString& MaterialPath, bool bUseRuntimeValues, bool bSave);
};
