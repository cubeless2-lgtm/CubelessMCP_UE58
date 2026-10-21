#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::MaterialCollectionMutation
{
// Editor-only, find-only exact MPC mutations. No inherited override/private
// default setter, persistent ticker, asset loader or renderer operation.
Function::FResult SetValues(const FString& CollectionPath, const FString& ScalarsJson, const FString& VectorsJson,
    bool bSetAssetDefaults, bool bSetRuntime, bool bSave);
Function::FResult Mirror(const FString& SourcePath, const FString& TargetPath, const FString& ParameterNamesJson,
    bool bCopyDefaults, bool bPreserveIds, bool bSave);
}
