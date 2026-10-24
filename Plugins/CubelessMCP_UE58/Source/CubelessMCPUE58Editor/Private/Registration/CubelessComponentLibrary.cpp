#include "Registration/CubelessComponentLibrary.h"
#include "Operations/CubelessComponentOperations.h"

FString UCubelessComponentLibrary::ListBlueprintComponents(const FString& BlueprintPath, const FString& ComponentName, int32 MaxComponents)
{
    return Cubeless::Operations::ReadBlueprintComponents(BlueprintPath, ComponentName, FString(), false, MaxComponents, 65536);
}

FString UCubelessComponentLibrary::GetComponentProperty(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName, int32 MaxComponents, int32 MaxValueChars)
{
    return Cubeless::Operations::ReadBlueprintComponents(BlueprintPath, ComponentName, PropertyName, true, MaxComponents, MaxValueChars);
}

FString UCubelessComponentLibrary::AddComponentToBlueprint(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName,
    const FString& TransformJson, const FString& ParentComponent, const FString& ComponentPropertiesJson, bool bCompile)
{
    return Cubeless::Operations::AddBlueprintComponent(BlueprintPath, ComponentType, ComponentName, TransformJson, ParentComponent, ComponentPropertiesJson, bCompile);
}

FString UCubelessComponentLibrary::SetComponentProperty(const FString& BlueprintPath, const FString& ComponentName,
    const FString& PropertyName, const FString& ValueJson)
{
    return Cubeless::Operations::SetBlueprintComponentProperty(BlueprintPath, ComponentName, PropertyName, ValueJson);
}

FString UCubelessComponentLibrary::SetPhysicsProperties(const FString& BlueprintPath, const FString& ComponentName,
    bool bSimulatePhysics, bool bGravityEnabled, double Mass, double LinearDamping, double AngularDamping)
{
    return Cubeless::Operations::SetBlueprintComponentPhysics(BlueprintPath, ComponentName,
        bSimulatePhysics, bGravityEnabled, Mass, LinearDamping, AngularDamping);
}

FString UCubelessComponentLibrary::SetStaticMeshProperties(const FString& BlueprintPath, const FString& ComponentName,
    const FString& StaticMeshPath, const FString& MaterialPath)
{
    return Cubeless::Operations::SetBlueprintStaticMeshProperties(BlueprintPath, ComponentName, StaticMeshPath, MaterialPath);
}

FString UCubelessComponentLibrary::ConfigureBlueprintPCGTemplate(const FString& ParamsJson)
{
    return Cubeless::Operations::ConfigureBlueprintPCGTemplate(ParamsJson);
}

FString UCubelessComponentLibrary::SetStaticMeshPropertiesRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& AssetsJson)
{
    return Cubeless::Operations::SetBlueprintStaticMeshPropertiesRaw(BlueprintPath, ComponentName, AssetsJson);
}

FString UCubelessComponentLibrary::SetPhysicsPropertiesRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertiesJson)
{
    return Cubeless::Operations::SetBlueprintComponentPhysicsRaw(BlueprintPath, ComponentName, PropertiesJson);
}

FString UCubelessComponentLibrary::AddComponentToBlueprintRaw(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName, const FString& TransformJson, const FString& ParentComponent)
{ return Cubeless::Operations::AddBlueprintComponentRaw(BlueprintPath, ComponentType, ComponentName, TransformJson, ParentComponent); }
