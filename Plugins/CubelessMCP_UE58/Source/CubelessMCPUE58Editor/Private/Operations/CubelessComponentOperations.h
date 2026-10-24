#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString ReadBlueprintComponents(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName,
    bool bReadProperty, int32 MaxComponents, int32 MaxValueChars);
FString AddBlueprintComponent(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName,
    const FString& TransformJson, const FString& ParentComponent, const FString& ComponentPropertiesJson, bool bCompile);
FString SetBlueprintComponentProperty(const FString& BlueprintPath, const FString& ComponentName,
    const FString& PropertyName, const FString& ValueJson);
FString SetBlueprintComponentPhysics(const FString& BlueprintPath, const FString& ComponentName,
    bool bSimulatePhysics, bool bGravityEnabled, double Mass, double LinearDamping, double AngularDamping);
FString SetBlueprintStaticMeshProperties(const FString& BlueprintPath, const FString& ComponentName,
    const FString& StaticMeshPath, const FString& MaterialPath);
}

namespace Cubeless::Operations
{
FString ConfigureBlueprintPCGTemplate(const FString& ParamsJson);
}

namespace Cubeless::Operations
{
FString SetBlueprintStaticMeshPropertiesRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& AssetsJson);
}

namespace Cubeless::Operations
{
FString SetBlueprintComponentPhysicsRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertiesJson);

FString AddBlueprintComponentRaw(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName, const FString& TransformJson, const FString& ParentComponent);
}
