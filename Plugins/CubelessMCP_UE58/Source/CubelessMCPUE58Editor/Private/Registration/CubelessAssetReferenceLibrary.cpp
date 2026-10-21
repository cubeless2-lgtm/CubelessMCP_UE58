#include "Registration/CubelessAssetReferenceLibrary.h"
#include "Compatibility/UE58/CubelessAssetReferenceApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAssetReferenceLibrary::ReplaceAssetReferencesMCP(const FString& ReplacementsJson,
    const FString& TargetRoot, const FString& TargetPathsJson, const FString& SourceRoot,
    const FString& StringReplacementsJson, const FString& NameReplacementsJson,
    bool bDryRun, bool bSaveAssets, bool bScanPaths, bool bRefreshMaterials,
    bool bIncludePackageObjects, bool bAllowNonGameTargets)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AssetReference::Replace(
        ReplacementsJson, TargetRoot, TargetPathsJson, SourceRoot, StringReplacementsJson, NameReplacementsJson,
        bDryRun, bSaveAssets, bScanPaths, bRefreshMaterials, bIncludePackageObjects, bAllowNonGameTargets),
        TEXT("replace_asset_references_mcp"));
}
