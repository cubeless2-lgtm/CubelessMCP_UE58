#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessFoliageInstanceLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessFoliageInstanceLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="list_foliage_types"))
    static FString ListFoliageTypes();
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="add_foliage_instances"))
    static FString AddFoliageInstances(FString FoliageTypePath,FString TransformsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="remove_foliage_instances"))
    static FString RemoveFoliageInstances(FString FoliageTypePath,FString InstanceIndicesJson,bool bRemoveAll,int64 MaxRemoveCount,FString PrivateContextJson);
};
