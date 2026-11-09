#pragma once
#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "Retargeter/IKRetargeter.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMocapSettings.generated.h"
class UAnimSequence;
UENUM()
enum class ECBMFingerBackend : uint8 {
 MediaPipe=0 UMETA(Hidden),
 WiLoR=1 UMETA(Hidden),
 HandFlow=2 UMETA(DisplayName="HandFlow / HaMeR"),
 None=3 UMETA(DisplayName="None (original hands)")
};
UENUM()
enum class ECBMDiagnosticFingerBackend : uint8 {
 WiLoR UMETA(DisplayName="WiLoR + MediaPipe (diagnostic)"),
 MediaPipe UMETA(DisplayName="MediaPipe only (diagnostic)")
};
UCLASS()
class CBMEDITOR_API UCubelessMocapSettings : public UObject {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, Category="Preview") bool DebugBones=false;
 UPROPERTY(EditAnywhere, Category="Workflow") bool AutoBoneCorrection=true;
 UPROPERTY(EditAnywhere, Category="Workflow") bool SaveHandFlow=true;
 UPROPERTY(EditAnywhere, Category="Workflow") bool SaveBoneCorrection=true;
 UPROPERTY(EditAnywhere, Category="Workflow") float SavePlayRate=1;
 UPROPERTY(EditAnywhere, Category="Workflow") float PreviewPlayRate=1;
 UPROPERTY(EditAnywhere, Category="Output", meta=(DisplayName="애니메이션 이름",ToolTip="최종 저장할 애니메이션 이름입니다. 옵션을 바꿔도 이 이름과 경로의 에셋을 갱신합니다.")) FString SequenceName="MocapAnimation";
 UPROPERTY(EditAnywhere, Category="Workflow") TObjectPtr<UAnimSequence> CorrectedBodyAnimation;
 UPROPERTY(EditAnywhere, Category="Workflow") TObjectPtr<UAnimSequence> CorrectedHandAnimation;
 UPROPERTY(EditAnywhere, Category="Workflow") TObjectPtr<UAnimSequence> ExportedSequence;
 UPROPERTY(EditAnywhere, Category="Input", meta=(FilePathFilter="mp4;mov;mkv;avi")) FFilePath Video;
 UPROPERTY(EditAnywhere, Category="Input") FFilePath FFmpeg;
 UPROPERTY(EditAnywhere, Category="Input") FDirectoryPath FrameDirectory;
 UPROPERTY(EditAnywhere, Category="Input", meta=(ClampMin="0")) float StartSeconds=0;
 UPROPERTY(EditAnywhere, Category="Input", meta=(ClampMin="0.1",ClampMax="120")) float DurationSeconds=10;
 UPROPERTY(EditAnywhere, Category="Input", meta=(ClampMin="1",ClampMax="120")) int32 FrameRate=30;
 UPROPERTY(EditAnywhere, Category="Solve") bool FootLocking=true;
 UPROPERTY(EditAnywhere, Category="Solve", meta=(DisplayName="Refine Body from Video (Experimental)", EditCondition="FootLocking", ToolTip="Optional lower-body refinement using confident video keypoints, support-leg depth evidence and foot contacts. Fits global camera height; calibrated rotation and scale stay fixed. No joint-angle limits. Some clips retain foot penetration or projection errors. Off preserves the previous body optimizer.")) bool RefineBody=false;
 UPROPERTY(EditAnywhere, Category="Solve") bool ReuseCheckpoints=true;
 // Preserve weights, resolution and all frames; seeded camera calibration is shared by both modes.
 UPROPERTY(EditAnywhere, Category="Solve", meta=(DisplayName="Fast Mode (Off: Original)", ToolTip="On: fused CHMR attention and compact SAM2 transfers. Off: original CHMR and SAM2 models. Both retain the same weights, resolution, all frames and seeded camera calibration. Small floating-point differences remain; original is a reference, not a guarantee of ground-truth accuracy. Missing optimized assets or GPU runtime use the corresponding original model.")) bool UseFusedCHMR=true;
 UPROPERTY(EditAnywhere, Category="Output") TObjectPtr<USkeletalMesh> TargetMesh;
 UPROPERTY(EditAnywhere, Category="Output") TObjectPtr<UIKRetargeter> Retargeter;
 UPROPERTY(EditAnywhere, Category="Output") FString OutputFolder="/Game/CubelessMocap";
 UPROPERTY(EditAnywhere, Category="Preview") TObjectPtr<UAnimSequence> PreviewAnimation;
 UPROPERTY(EditAnywhere, Category="Preview", meta=(ToolTip="Match the body's estimated camera pitch/roll/FOV and approximate initial person framing. This changes only the comparison camera, never animation. Falls back to the standard front view if calibration is unavailable.")) bool MatchVideoCamera=true;
 UPROPERTY(EditAnywhere, Category="Hands", meta=(ToolTip="Existing target animation to preserve. Refine changes finger and metacarpal rotations in a new asset; body and wrists stay unchanged.")) TObjectPtr<UAnimSequence> OriginalAnimation;
 UPROPERTY(VisibleAnywhere, Category="Hands") TObjectPtr<UAnimSequence> HandEnhancedAnimation;
 UPROPERTY(EditAnywhere, Category="Solve", meta=(DisplayName="Enhance Hands (HandFlow)", ToolTip="Checked by default. Start Mocap saves the body, then automatically applies HandFlow / HaMeR to fingers in a separate animation. Uncheck for body only. Model quality settings are unchanged.")) bool EnhanceHands=true;
 // Retain the serialized legacy field for existing scripts/results; normal Start uses EnhanceHands only.
 UPROPERTY(EditAnywhere, Category="Workflow") ECBMFingerBackend FingerBackend=ECBMFingerBackend::HandFlow;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", AdvancedDisplay, meta=(ToolTip="Enable legacy WiLoR/MediaPipe inference and comparison controls only for diagnosis. This affects only Run hand diagnostic; Start Mocap still follows the HandFlow checkbox.")) bool EnableLegacyHandDiagnostics=false;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) ECBMDiagnosticFingerBackend DiagnosticFingerBackend=ECBMDiagnosticFingerBackend::WiLoR;
 ECBMFingerBackend SelectedFingerBackend() const {return EnableLegacyHandDiagnostics?(DiagnosticFingerBackend==ECBMDiagnosticFingerBackend::WiLoR?ECBMFingerBackend::WiLoR:ECBMFingerBackend::MediaPipe):(EnhanceHands?ECBMFingerBackend::HandFlow:ECBMFingerBackend::None);}
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow") FFilePath HandFlowPython;
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow") FDirectoryPath HandFlowDirectory;
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow", meta=(ClampMin="1",ClampMax="32")) int32 HandFlowSteps=3;
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow", meta=(ClampMin="1",ClampMax="8")) int32 HandFlowBatch=2;
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow", meta=(ClampMin="0")) int32 HandFlowSeed=0;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(DisplayName="Write Model Diagnostics")) bool HandDiagnostics=false;
 UPROPERTY(EditAnywhere, Category="Hands|Comparison") TObjectPtr<UAnimSequence> HandComparisonBase;
 UPROPERTY(VisibleAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) TObjectPtr<UAnimSequence> WiLoRAnimation;
 UPROPERTY(EditAnywhere, Category="Hands|Comparison") TObjectPtr<UAnimSequence> HandFlowAnimation;
 UPROPERTY(VisibleAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) TObjectPtr<UAnimSequence> MediaPipeAnimation;
 UPROPERTY(VisibleAnywhere, Category="Hands") FString HandEnhancedBackend;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) FFilePath MediaPipePython;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) FFilePath MediaPipeModel;
 UPROPERTY(EditAnywhere, Category="Hands", meta=(FilePathFilter="json", ToolTip="Previously generated hands.json. Reapply with a new Hand Strength without video/model inference.")) FFilePath HandResult;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) FFilePath HandPython;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) FDirectoryPath WiLoRDirectory;
 UPROPERTY(EditAnywhere, Category="Hands", meta=(ClampMin="0",ClampMax="1")) float HandStrength=1;
 UPROPERTY(EditAnywhere, Category="Postprocess", meta=(DisplayName="Enable Joint Correction", ToolTip="Legacy direct API switch; the UI Start Mocap uses AutoBoneCorrection. Corrects finger joint range and isolated temporal outliers in a new asset. No model inference; body, wrists and metacarpals stay unchanged.")) bool EnableJointCorrection=false;
 UPROPERTY(EditAnywhere, Category="Postprocess", meta=(ClampMin="0",ClampMax="1",ToolTip="1 applies the full animation envelope; lower values partially blend the correction and may leave range violations. Zero selects the source unchanged.")) float JointCorrectionStrength=1;
 UPROPERTY(EditAnywhere, Category="Postprocess", meta=(EditCondition="EnableJointCorrection",ToolTip="Saved, uncorrected Manny animation. Empty uses the current HandFlow result, then Preview Animation. Repeated applications always use this source.")) TObjectPtr<UAnimSequence> JointCorrectionSource;
 UPROPERTY(VisibleAnywhere, Category="Postprocess") TObjectPtr<UAnimSequence> JointComparisonSource;
 UPROPERTY(VisibleAnywhere, Category="Postprocess") TObjectPtr<UAnimSequence> JointCorrectedAnimation;
 UPROPERTY(VisibleAnywhere, Category="Postprocess") FFilePath JointCorrectionReport;
 UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="-180",ClampMax="180",Units="deg")) float PreviewYawOffset=0;
 UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="0.5",ClampMax="2.0")) float PreviewZoom=1;
 UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="0",ClampMax="2")) int32 PreviewHand=0;
};
UCLASS()
class CBMEDITOR_API UCubelessMocapLibrary : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static void OpenWindow();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool StartMocap(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool SaveAnimationSequence(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static UAnimSequence* ResolveSaveAnimation(UCubelessMocapSettings* Settings);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static void LoadPreview();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool SavePreviewScreenshot();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static FString GetJobStatus();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static void Cancel();
 static bool IsBusy();
 static void ShutdownJobs();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool Extract(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool RefineHands(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool ApplyHandResult(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool CorrectFingerJoints(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static FString GetHandReadiness(UCubelessMocapSettings* Settings);
};
