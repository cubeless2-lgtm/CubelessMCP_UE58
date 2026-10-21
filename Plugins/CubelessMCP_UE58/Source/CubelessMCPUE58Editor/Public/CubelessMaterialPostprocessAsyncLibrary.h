#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialPostprocessAsyncLibrary.generated.h"
class UCubelessMaterialPostprocessJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessMaterialPostprocessAsyncLibrary:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="start_compile_and_save_material_job"))
    static UCubelessMaterialPostprocessJob* StartCompileAndSaveMaterialJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="start_refresh_material_cached_expression_data_job"))
    static UCubelessMaterialPostprocessJob* StartRefreshMaterialCachedExpressionDataJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="start_replace_material_collection_references_job"))
    static UCubelessMaterialPostprocessJob* StartReplaceMaterialCollectionReferencesJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="start_replace_material_texture_references_job"))
    static UCubelessMaterialPostprocessJob* StartReplaceMaterialTextureReferencesJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="start_replace_material_collection_parameters_job"))
    static UCubelessMaterialPostprocessJob* StartReplaceMaterialCollectionParametersJob(const FString& ParamsJson);
};
