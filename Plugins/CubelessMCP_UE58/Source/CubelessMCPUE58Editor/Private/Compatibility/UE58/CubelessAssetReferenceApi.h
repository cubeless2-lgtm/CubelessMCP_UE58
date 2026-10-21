#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AssetReference
{
// Engine operation only. Nullable4 are JSON values carried through the hidden Editor binding.
Variable::FResult Replace(const FString& ReplacementsJson, const FString& TargetRoot, const FString& TargetPathsJson,
    const FString& SourceRoot, const FString& StringReplacementsJson, const FString& NameReplacementsJson,
    bool bDryRun, bool bSaveAssets, bool bScanPaths, bool bRefreshMaterials,
    bool bIncludePackageObjects, bool bAllowNonGameTargets);
}
