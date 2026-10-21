#include "Registration/CubelessAudioLibrary.h"
#include "Compatibility/UE58/CubelessAudioApi.h"

FString UCubelessAudioLibrary::InspectAudioAsset(const FString& AssetPath)
{ return Cubeless::UE58::Audio::InspectAsset(AssetPath); }

FString UCubelessAudioLibrary::CreateSoundCueFromWave(const FString& Name, const FString& SoundWavePath, const FString& PackagePath)
{ return Cubeless::UE58::Audio::CreateSoundCueFromWave(Name, SoundWavePath, PackagePath); }
