#include "Registration/CubelessMaterialCacheLibrary.h"
#include "Compatibility/UE58/CubelessMaterialCacheApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessMaterialCacheLibrary::RefreshMaterialCachedExpressionData(const FString& MaterialPath,bool bSave)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::MaterialCache::Refresh(MaterialPath,bSave),TEXT("refresh_material_cached_expression_data"));
}
