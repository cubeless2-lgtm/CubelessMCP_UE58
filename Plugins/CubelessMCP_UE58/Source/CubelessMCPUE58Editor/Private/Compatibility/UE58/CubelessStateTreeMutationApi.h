#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::StateTreeMutation
{
Variable::FResult Add(const FString& AssetPath, const FString& StateName, const FString& ParentPath,
    const FString& StateType, const FString& SelectionBehavior, bool bDryRun);
Variable::FResult Compile(const FString& AssetPath, bool bSave);
}
