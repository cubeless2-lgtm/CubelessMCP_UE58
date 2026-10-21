#include "Registration/CubelessComponentReplacementLibrary.h"
#include "Compatibility/UE58/CubelessComponentReplacementApi.h"
FString UCubelessComponentReplacementLibrary::ReplaceBlueprintComponentClass(const FString& BlueprintPath,
    const FString& ComponentName, const FString& NewComponentClass, double SphereRadius, bool bDryRun,
    bool bCompile, bool bSave, bool bRefreshNodes)
{
    return Cubeless::UE58::ComponentReplacement::ReplaceComponentClass(BlueprintPath, ComponentName,
        NewComponentClass, SphereRadius, bDryRun, bCompile, bSave, bRefreshNodes);
}
