#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::UserDefinedEnum
{
// Only the hidden binding carries arrays/null as JSON. Public tool arguments remain typed.
Variable::FResult Create(const FString& AssetPath, const FString& DisplayNamesJson,
    const FString& TooltipsJson, bool bDryRun, bool bSave, bool bSkipIfExists);
}
