#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialCacheLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessMaterialCacheLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="refresh_material_cached_expression_data"))
    static FString RefreshMaterialCachedExpressionData(const FString& MaterialPath,bool bSave);
};
