#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Cubeless::UE58::Component
{
struct FMutationResult
{
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
    FString Stage = TEXT("preflight"), ErrorCode, Error;
    bool bSuccess = false, bMutationAttempted = false, bChanged = false, bChangeStateKnown = true;
    bool bPostconditionPassed = false, bCompileRequested = false, bCompileAttempted = false;
    TOptional<bool> Compiled;
};

FMutationResult AddLocalComponent(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName,
    const FString& TransformJson, const FString& ParentComponent, const FString& ComponentPropertiesJson, bool bCompile);
FMutationResult SetLocalComponentProperty(const FString& BlueprintPath, const FString& ComponentName,
    const FString& PropertyName, const FString& ValueJson);
FMutationResult SetLocalPhysicsProperties(const FString& BlueprintPath, const FString& ComponentName,
    bool bSimulatePhysics, bool bGravityEnabled, double Mass, double LinearDamping, double AngularDamping);
FMutationResult SetLocalStaticMeshProperties(const FString& BlueprintPath, const FString& ComponentName,
    const FString& StaticMeshPath, const FString& MaterialPath);
}

namespace Cubeless::UE58::Component
{
FMutationResult ConfigureBlueprintPCGTemplate(const FString& ParamsJson);
}

namespace Cubeless::UE58::Component
{
// Exact original command only. Named full-path prevalidation remains separate.
FMutationResult SetLocalStaticMeshPropertiesRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& AssetsJson);
}

namespace Cubeless::UE58::Component
{
FMutationResult SetLocalPhysicsPropertiesRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertiesJson);

FMutationResult AddLocalComponentRaw(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName, const FString& TransformJson, const FString& ParentComponent);
}
