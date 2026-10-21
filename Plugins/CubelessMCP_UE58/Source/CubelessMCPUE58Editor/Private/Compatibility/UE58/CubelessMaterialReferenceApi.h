#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::MaterialReference
{
Variable::FResult ReplaceCollections(const FString& MaterialPath, const FString& TargetCollectionPath,
    const FString& SourceCollectionPath, const FString& GraphType, bool bSave);
Variable::FResult ReplaceTextures(const FString& MaterialPath, const FString& ReplacementsJson,
    const FString& GraphType, bool bSave);
}
