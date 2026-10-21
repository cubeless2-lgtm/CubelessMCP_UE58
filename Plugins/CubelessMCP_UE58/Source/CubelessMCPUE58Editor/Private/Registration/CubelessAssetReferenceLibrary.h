#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAssetReferenceLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAssetReferenceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Project", meta=(AIIgnore, ScriptName="replace_asset_references_mcp"))
    static FString ReplaceAssetReferencesMCP(const FString& ReplacementsJson, const FString& TargetRoot,
        const FString& TargetPathsJson, const FString& SourceRoot, const FString& StringReplacementsJson,
        const FString& NameReplacementsJson, bool bDryRun, bool bSaveAssets, bool bScanPaths,
        bool bRefreshMaterials, bool bIncludePackageObjects, bool bAllowNonGameTargets);
};
