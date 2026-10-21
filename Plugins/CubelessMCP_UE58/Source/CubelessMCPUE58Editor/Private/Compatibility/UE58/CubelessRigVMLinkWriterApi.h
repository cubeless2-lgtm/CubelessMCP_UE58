#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::RigVMLinkWriter
{
Variable::FResult Add(const FString& ControlRigPath, const FString& ControlRigAssetPath, const FString& ControlRigClassPath,
    const FString& LinksJson, const FString& UnitLinksJson, bool bAllowTypeCasting, bool bDryRun,
    bool bCompile, bool bSaveAsset, int64 MaxLinks);
}
