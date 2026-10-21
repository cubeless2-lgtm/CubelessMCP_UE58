#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAudioLibrary.generated.h"

/** Hidden Editor-only adapters for two original audio contracts. */
UCLASS()
class UCubelessAudioLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Audio", meta=(AIIgnore, ScriptName="inspect_audio_asset"))
    static FString InspectAudioAsset(const FString& AssetPath);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Audio", meta=(AIIgnore, ScriptName="create_sound_cue_from_wave"))
    static FString CreateSoundCueFromWave(const FString& Name, const FString& SoundWavePath, const FString& PackagePath);
};
