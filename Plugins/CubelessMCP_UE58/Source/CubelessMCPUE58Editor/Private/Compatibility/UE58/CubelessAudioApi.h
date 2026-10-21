#pragma once
#include "CoreMinimal.h"
class FJsonObject;

namespace Cubeless::UE58::Audio
{
    FString InspectAsset(const FString& AssetPath);
    FString CreateSoundCueFromWave(const FString& Name, const FString& SoundWavePath, const FString& PackagePath);
    // Same Editor module only: exact native audio asset classes, no new UFUNCTION/tool.
    bool ValidateNativeAssetReferences(UObject* Asset, TSharedRef<FJsonObject>& Observation, FString& Error);
    // One already-saved exact Game audio file only. No directory scan or in-memory wait.
    bool RefreshSavedAssetRegistry(UObject* Asset, TSharedRef<FJsonObject>& Observation, FString& Error);
}
