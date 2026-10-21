#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessDataAssetLibrary.generated.h"
UCLASS(meta=(AIIgnore))
class UCubelessDataAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="get_data_asset"))
    static FString GetDataAsset(const FString& AssetPath);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="set_data_asset_properties"))
    static FString SetDataAssetProperties(const FString& AssetPath, const FString& PropertiesJson, bool bSave);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="create_data_asset"))
    static FString CreateDataAsset(const FString& AssetPath, const FString& ClassPath, bool bSave);
};
