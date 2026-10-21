#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString SetMaterialParameterCollectionValues(const FString& CollectionPath, const FString& ScalarsJson, const FString& VectorsJson,
    bool bSetAssetDefaults, bool bSetRuntime, bool bSave);
FString MirrorMaterialParameterCollection(const FString& SourcePath, const FString& TargetPath, const FString& ParameterNamesJson,
    bool bCopyDefaults, bool bPreserveIds, bool bSave);
}
