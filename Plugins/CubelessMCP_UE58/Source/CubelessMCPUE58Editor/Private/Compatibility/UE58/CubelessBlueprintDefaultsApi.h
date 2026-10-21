#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::BlueprintDefaults
{
FString SetActorTickSettings(const FString& BlueprintPath, double TickHz, double TickInterval,
    bool bCanEverTick, bool bStartWithTickEnabled, bool bAllowTickOnDedicatedServer,
    bool bTickEvenWhenPaused, bool bDryRun, bool bCompile, bool bSave);
FString SetSkeletalAnimDefaults(const FString& BlueprintPath, const FString& ComponentName,
    const FString& SkeletalMesh, const FString& AnimClass, const FString& PhysicsAssetJson,
    const FString& SimulatePhysicsJson, const FString& CollisionEnabledJson,
    const FString& CollisionResponseJson, const FString& EnableGravityJson,
    const FString& VisibilityBasedAnimTickOptionJson, bool bCompile, bool bSave);
}
