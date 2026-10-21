#include "Registration/CubelessMaterialCollectionMutationLibrary.h"
#include "Operations/CubelessMaterialCollectionMutationOperations.h"
FString UCubelessMaterialCollectionMutationLibrary::SetMaterialParameterCollectionValues(const FString& CollectionPath,
    const FString& ScalarsJson, const FString& VectorsJson, bool bSetAssetDefaults, bool bSetRuntime, bool bSave)
{ return Cubeless::Operations::SetMaterialParameterCollectionValues(CollectionPath, ScalarsJson, VectorsJson, bSetAssetDefaults, bSetRuntime, bSave); }
FString UCubelessMaterialCollectionMutationLibrary::MirrorMaterialParameterCollection(const FString& SourcePath,
    const FString& TargetPath, const FString& ParameterNamesJson, bool bCopyDefaults, bool bPreserveIds, bool bSave)
{ return Cubeless::Operations::MirrorMaterialParameterCollection(SourcePath, TargetPath, ParameterNamesJson, bCopyDefaults, bPreserveIds, bSave); }
