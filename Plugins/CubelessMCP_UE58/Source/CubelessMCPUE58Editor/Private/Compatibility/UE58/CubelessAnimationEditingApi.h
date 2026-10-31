#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::AnimationEditing
{
// Editor-only work logic. No registry or server dependency.
FString Edit(UObject* Asset, const FString& Action, const FString& ArgumentsJson,
    bool Execute, bool Save, bool AllowSaveDirtyPackage, const FString& RequestToken=FString(), bool UnobservedAssetLoad=false);
// Bounded same-call outcome recovery; never executes or retries a mutation.
FString TakeReceipt(const FString& RequestToken);
}
