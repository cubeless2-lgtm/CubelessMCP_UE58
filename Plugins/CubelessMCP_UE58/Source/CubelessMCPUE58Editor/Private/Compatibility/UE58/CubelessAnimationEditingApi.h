#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::AnimationEditing
{
// Editor-only work logic. No registry or server dependency.
FString Edit(UObject* Asset, const FString& Action, const FString& ArgumentsJson,
    bool Execute, bool Save, bool AllowSaveDirtyPackage, const FString& RequestToken=FString(), bool UnobservedAssetLoad=false);
// Bounded same-call outcome recovery; never executes or retries a mutation.
FString TakeReceipt(const FString& RequestToken);
// Reuse the existing bounded authored animation/model/notify snapshot for input preservation.
// Editor-only read helper; never loads, edits, compiles or saves.
bool CaptureReadOnlySource(UObject* Asset,TMap<FString,FString>& Out,FString& Error);
// Acquisition-only observation. These helpers never load, Modify or save assets.
FString BeginAcquisition(const FString& ObjectPath, const FString& RequestToken);
FString EndAcquisition(const FString& RequestToken, bool LoadAttempted);
}
