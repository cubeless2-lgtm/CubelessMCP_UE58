#include "Registration/CubelessMaterialCollectionParametersLibrary.h"
#include "Compatibility/UE58/CubelessMaterialCollectionParametersApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessMaterialCollectionParametersLibrary::ReplaceMaterialCollectionParameters(const FString& Path, bool Runtime, bool Save)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::MaterialCollectionParameters::Replace(Path, Runtime, Save), TEXT("replace_material_collection_parameters")); }
