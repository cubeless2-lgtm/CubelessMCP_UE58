#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialReferenceLibrary.generated.h"

/** Hidden Editor operations. The selected public tool/domain owns argument packing and journaling. */
UCLASS()
class UCubelessMaterialReferenceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore, ScriptName="replace_material_collection_references"))
    static FString ReplaceMaterialCollectionReferences(const FString& MaterialPath, const FString& TargetCollectionPath,
        const FString& SourceCollectionPath, const FString& GraphType, bool bSave);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore, ScriptName="replace_material_texture_references"))
    static FString ReplaceMaterialTextureReferences(const FString& MaterialPath, const FString& ReplacementsJson,
        const FString& GraphType, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="prepare_material_reference_toolset"))
    static FString PrepareMaterialReferenceToolset(UClass* ToolsetClass);

};
