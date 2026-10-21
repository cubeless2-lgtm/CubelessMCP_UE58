#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialCollectionSyncLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessMaterialCollectionSyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="set_material_parameter_collection_sync"))
    static FString SetMaterialParameterCollectionSync(const FString& Action,const FString& SourceCollectionPath,
        const FString& TargetCollectionPath,const FString& ParameterNamesJson,double IntervalSeconds,
        bool bSetAssetDefaultsOnEnable,bool bSaveOnEnable);

    UFUNCTION(BlueprintCallable,Category="Cubeless|Registration",meta=(AIIgnore,ScriptName="prepare_material_collection_sync_toolset"))
    static FString PrepareMaterialCollectionSyncToolset(UClass* ToolsetClass);
};
