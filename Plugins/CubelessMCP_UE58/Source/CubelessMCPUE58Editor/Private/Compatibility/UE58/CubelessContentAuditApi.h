#pragma once
#include "Operations/CubelessVariableOperations.h"
namespace Cubeless::UE58::ContentAudit
{
    Variable::FResult Audit(const FString& RootPath, bool bRecursive, bool bScanPaths, bool bIncludeDependencies,
        bool bIncludeAssets, bool bWriteReport, const FString& ForbiddenPrefixesJson, const FString& RequiredAssetPathsJson,
        const FString& ExpectedAssetCountJson, const FString& ExpectedClassCountsJson, int64 MaxSamples,
        bool bFailOnDirtyPackages, bool bFailOnRedirectors, bool bFailOnForbiddenDependencies);
}
