#pragma once
#include "CoreMinimal.h"

/** Finite Editor asset authoring through public runtime AIModule APIs. */
namespace Cubeless::UE58::Blackboard
{
    FString CreateAsset(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset);
    FString AddKey(const FString& BlackboardPath, const FString& KeyName, const FString& KeyType,
        const FString& BaseClass, const FString& EnumPath, bool bInstanceSynced, bool bSaveAsset);
    FString RemoveKey(const FString& BlackboardPath, const FString& KeyName, bool bSaveAsset);
    FString InspectAsset(const FString& AssetPath);
}
