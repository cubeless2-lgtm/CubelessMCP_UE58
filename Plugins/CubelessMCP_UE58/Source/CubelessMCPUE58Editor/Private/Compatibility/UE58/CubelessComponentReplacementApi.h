#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::ComponentReplacement
{
FString ReplaceComponentClass(const FString& BlueprintPath, const FString& ComponentName,
    const FString& NewComponentClass, double SphereRadius, bool bDryRun, bool bCompile,
    bool bSave, bool bRefreshNodes);
}
