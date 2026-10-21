#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialCollectionMutationLibrary.generated.h"

/** Internal Editor MPC adapters. Public Python names are registered separately;
 * these functions must never be serialized into authored gameplay graphs. */
UCLASS()
class UCubelessMaterialCollectionMutationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString SetMaterialParameterCollectionValues(const FString& CollectionPath, const FString& ScalarsJson, const FString& VectorsJson,
        bool bSetAssetDefaults, bool bSetRuntime, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString MirrorMaterialParameterCollection(const FString& SourcePath, const FString& TargetPath, const FString& ParameterNamesJson,
        bool bCopyDefaults, bool bPreserveIds, bool bSave);
};
