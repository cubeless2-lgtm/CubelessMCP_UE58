#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::FoliageInstances
{
Variable::FResult List();
Variable::FResult Add(const FString& FoliageTypePath,const FString& TransformsJson);
Variable::FResult Remove(const FString& FoliageTypePath,const FString& InstanceIndicesJson,bool bRemoveAll,int64 MaxRemoveCount,const FString& PrivateContextJson);
}
