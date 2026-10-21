#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::MaterialFunctionExpansion
{
Variable::FResult Expand(const FString& MaterialPath,const FString& NodeId,bool bRecursive,bool bExcludeEngineFunctions,int32 MaxPasses,bool bSave,bool bAllowPartialSave);
}
