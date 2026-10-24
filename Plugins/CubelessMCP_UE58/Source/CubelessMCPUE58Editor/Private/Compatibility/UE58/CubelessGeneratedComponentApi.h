#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Cubeless::UE58::GeneratedComponent
{
/** Inspect an already loaded class. No class resolution, compilation or saving. */
TSharedRef<FJsonObject> InspectSavedClass(UClass* Class);
/** Same saved/runtime checks for an Actor Blueprint and all generated parents. */
TSharedRef<FJsonObject> InspectSavedActorParent(UClass* Class);
/** Opt-in saved pure UObject-root parent profile; Actor/component readers are unchanged. */
TSharedRef<FJsonObject> InspectSavedObjectParent(UClass* Class);
}
