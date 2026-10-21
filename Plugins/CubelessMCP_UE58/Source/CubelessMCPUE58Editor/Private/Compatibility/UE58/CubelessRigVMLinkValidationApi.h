#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::RigVMLinkValidation
{
Variable::FResult Validate(const FString& ControlRigPath, const FString& ControlRigAssetPath,
    const FString& ControlRigClassPath, const FString& LinksJson, const FString& UnitLinksJson,
    bool bAllowTypeCasting, int64 MaxLinks);
}
