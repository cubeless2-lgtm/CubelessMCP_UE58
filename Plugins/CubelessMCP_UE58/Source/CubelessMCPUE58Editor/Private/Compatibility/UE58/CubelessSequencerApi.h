#pragma once
#include "CoreMinimal.h"
class ULevelSequence;

/** Finite public MovieScene fallbacks. Python owns public tools, journals and exact saves. */
namespace Cubeless::UE58::Sequencer
{
    ULevelSequence* ResolveAsset(const FString& SequencePath,FString& OutReceiptJson);
    FString ApplyPlaybackRange(const FString& SequencePath, double PlaybackStartFrame, double PlaybackEndFrame);
    FString AddTransformKeys(const FString& SequencePath, const FString& KeysJson,
        const FString& BindingId, const FString& ActorLabel);
    FString AddFloatPropertyKeys(const FString& SequencePath, const FString& PropertyPath,
        const FString& KeysJson, const FString& BindingId, const FString& ActorLabel);
    // Original maintenance leaves own their requested forced save and strict terminal contract.
    FString SetPlaybackRange(const FString& SequencePath, double PlaybackStartFrame, double PlaybackEndFrame, bool bSave);
    FString RemoveBinding(const FString& SequencePath, const FString& BindingId, const FString& ActorLabel, bool bSave);
    FString AddSubsequence(const FString& SequencePath, const FString& SubsequencePath, double StartFrame, double DurationFrames, bool bSave);
    FString AddCameraCut(const FString& SequencePath, double StartFrame, double EndFrame, const FString& BindingId, const FString& ActorLabel, bool bSave);
    FString OpenInEditor(const FString& SequencePath);
}
