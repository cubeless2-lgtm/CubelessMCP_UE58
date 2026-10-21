#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessSequencerLibrary.generated.h"

class ULevelSequence;

/** Hidden Editor-only finite native fallbacks; no public Toolset registration here. */
UCLASS()
class UCubelessSequencerLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="resolve_level_sequence_asset"))
    static ULevelSequence* ResolveLevelSequenceAsset(const FString& SequencePath,FString& OutReceiptJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore))
    static FString ApplyLevelSequencePlaybackRange(const FString& SequencePath,
        double PlaybackStartFrame, double PlaybackEndFrame);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore))
    static FString AddLevelSequenceTransformKeys(const FString& SequencePath, const FString& KeysJson,
        const FString& BindingId, const FString& ActorLabel);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="add_level_sequence_float_property_keys"))
    static FString AddLevelSequenceFloatPropertyKeys(const FString& SequencePath, const FString& PropertyPath,
        const FString& KeysJson, const FString& BindingId, const FString& ActorLabel);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="prepare_sequencer_float_toolset"))
    static FString PrepareSequencerFloatToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="set_level_sequence_playback_range"))
    static FString SetLevelSequencePlaybackRange(const FString& SequencePath, double PlaybackStartFrame, double PlaybackEndFrame, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="remove_level_sequence_binding"))
    static FString RemoveLevelSequenceBinding(const FString& SequencePath, const FString& BindingId, const FString& ActorLabel, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="prepare_sequencer_maintenance_toolset"))
    static FString PrepareSequencerMaintenanceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="add_level_sequence_subsequence"))
    static FString AddLevelSequenceSubsequence(const FString& SequencePath, const FString& SubsequencePath, double StartFrame, double DurationFrames, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="add_level_sequence_camera_cut"))
    static FString AddLevelSequenceCameraCut(const FString& SequencePath, double StartFrame, double EndFrame, const FString& BindingId, const FString& ActorLabel, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="prepare_sequencer_camera_toolset"))
    static FString PrepareSequencerCameraToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Sequencer", meta=(AIIgnore, ScriptName="open_level_sequence_in_editor"))
    static FString OpenLevelSequenceInEditor(const FString& SequencePath);
};
