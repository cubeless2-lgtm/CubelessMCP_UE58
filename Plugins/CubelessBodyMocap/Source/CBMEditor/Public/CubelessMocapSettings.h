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
 UPROPERTY(EditAnywhere, Category="Lower Body", meta=(ToolTip="Saved animation to preserve, including HandFlow fingers. Empty uses the current preview. A separate output is created.")) TObjectPtr<UAnimSequence> FootInputAnimation;
 UPROPERTY(EditAnywhere, Category="Lower Body", meta=(Units="cm", ToolTip="Horizontal ground plane for optional contact refinement, in target animation component space.")) float FootGroundHeight=0;
 UPROPERTY(EditAnywhere, Category="Lower Body", meta=(DisplayName="Refine Pelvis and Legs from Video (Experimental)", ToolTip="Independent lower-body fit using existing video keypoints; no FootMR model. Preserves ankle WORLD orientation by local counter-rotation against the changed calf. Changes pelvis and leg motion; compensates spine_01 to preserve upper-body world motion and HandFlow fingers. No joint-angle limits. Off preserves all existing body tracks. Uses an approximate camera projection; review before use.")) bool FootVideoLowerBody=false;
 UPROPERTY(VisibleAnywhere, Category="Lower Body|Comparison") bool FootResultHasLowerBody=false;
 UPROPERTY(EditAnywhere, Category="Lower Body", meta=(EditCondition="FootVideoLowerBody", DisplayName="Refine Supported Ankle and Forefoot", ToolTip="Optional model-free shoe contact fitting. Allows ankle and forefoot rotation only in support/transition frames; airborne world orientation stays preserved unless Stabilize Foot Rotation Outliers is enabled; hands stay preserved. No joint-angle limits. Off keeps ankle world orientation for every frame.")) bool FootSupportRotation=false;
 UPROPERTY(VisibleAnywhere, Category="Lower Body|Comparison") bool FootResultHasSupportRotation=false;
 UPROPERTY(EditAnywhere, Category="Lower Body", meta=(EditCondition="FootVideoLowerBody && FootSupportRotation", DisplayName="Stabilize Foot Rotation Outliers", ToolTip="Optional centred temporal cleanup of detected uneven foot rotations, including flight. Preserves other airborne orientations, upper body and HandFlow hands. No joint-angle limits.")) bool FootTemporalCleanup=false;
 UPROPERTY(VisibleAnywhere, Category="Lower Body|Comparison") bool FootResultHasTemporalCleanup=false;
 UPROPERTY(EditAnywhere, Category="Lower Body|Setup", meta=(DisplayName="Lower Body Python", ToolTip="Independent CPU-only LowerBody environment with numpy, scipy and psutil. Prepare with Tools/prepare_lower_body.py using standalone Python 3.12. No hand runtime or model weights are required.")) FFilePath FootPython;
 UPROPERTY(EditAnywhere, Category="Lower Body|Setup", meta=(ToolTip="Optional override: body run folder containing FootMRInput.json. Empty resolves the Body_<run> animation name.")) FDirectoryPath FootBodyData;
 UPROPERTY(EditAnywhere, Category="Lower Body", meta=(FilePathFilter="json")) FFilePath FootResult;
 UPROPERTY(VisibleAnywhere, Category="Lower Body|Comparison") TObjectPtr<UAnimSequence> FootComparisonBase;
 UPROPERTY(VisibleAnywhere, Category="Lower Body|Comparison") TObjectPtr<UAnimSequence> LowerBodyAnimation;
 UPROPERTY(EditAnywhere, Category="Hands", meta=(ToolTip="Existing target animation to preserve. Refine changes finger and metacarpal rotations in a new asset; body and wrists stay unchanged.")) TObjectPtr<UAnimSequence> OriginalAnimation;
 UPROPERTY(VisibleAnywhere, Category="Hands") TObjectPtr<UAnimSequence> HandEnhancedAnimation;
 UPROPERTY(EditAnywhere, Category="Hands", meta=(DisplayName="Hand Enhancement", EditCondition="!EnableLegacyHandDiagnostics", ToolTip="Choose original hands or HandFlow / HaMeR temporal inference. Existing animations and cached diagnostic results remain available.")) ECBMFingerBackend FingerBackend=ECBMFingerBackend::HandFlow;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", AdvancedDisplay, meta=(ToolTip="Enable legacy WiLoR/MediaPipe inference and comparison controls only for diagnosis. This overrides Hand Enhancement until disabled.")) bool EnableLegacyHandDiagnostics=false;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) ECBMDiagnosticFingerBackend DiagnosticFingerBackend=ECBMDiagnosticFingerBackend::WiLoR;
 ECBMFingerBackend SelectedFingerBackend() const {return EnableLegacyHandDiagnostics?(DiagnosticFingerBackend==ECBMDiagnosticFingerBackend::WiLoR?ECBMFingerBackend::WiLoR:ECBMFingerBackend::MediaPipe):FingerBackend;}
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow") FFilePath HandFlowPython;
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow") FDirectoryPath HandFlowDirectory;
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow", meta=(ClampMin="1",ClampMax="32")) int32 HandFlowSteps=3;
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow", meta=(ClampMin="1",ClampMax="8")) int32 HandFlowBatch=2;
 UPROPERTY(EditAnywhere, Category="Hands|HandFlow", meta=(ClampMin="0")) int32 HandFlowSeed=0;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(DisplayName="Write Model Diagnostics")) bool HandDiagnostics=false;
 UPROPERTY(VisibleAnywhere, Category="Hands|Comparison") TObjectPtr<UAnimSequence> HandComparisonBase;
 UPROPERTY(VisibleAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) TObjectPtr<UAnimSequence> WiLoRAnimation;
 UPROPERTY(VisibleAnywhere, Category="Hands|Comparison") TObjectPtr<UAnimSequence> HandFlowAnimation;
 UPROPERTY(VisibleAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) TObjectPtr<UAnimSequence> MediaPipeAnimation;
 UPROPERTY(VisibleAnywhere, Category="Hands") FString HandEnhancedBackend;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) FFilePath MediaPipePython;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) FFilePath MediaPipeModel;
 UPROPERTY(EditAnywhere, Category="Hands", meta=(FilePathFilter="json", ToolTip="Previously generated hands.json. Reapply with a new Hand Strength without video/model inference.")) FFilePath HandResult;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) FFilePath HandPython;
 UPROPERTY(EditAnywhere, Category="Hands|Diagnostics", meta=(EditCondition="EnableLegacyHandDiagnostics", EditConditionHides)) FDirectoryPath WiLoRDirectory;
 UPROPERTY(EditAnywhere, Category="Hands", meta=(ClampMin="0",ClampMax="1")) float HandStrength=1;
 UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="-180",ClampMax="180",Units="deg")) float PreviewYawOffset=0;
 UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="0.5",ClampMax="2.0")) float PreviewZoom=1;
 UPROPERTY(EditAnywhere, Category="Preview", meta=(ClampMin="0",ClampMax="2")) int32 PreviewHand=0;
};
UCLASS()
class CBMEDITOR_API UCubelessMocapLibrary : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static void OpenWindow();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static void LoadPreview();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool SavePreviewScreenshot();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static FString GetJobStatus();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static void Cancel();
 static bool IsBusy();
 static void ShutdownJobs();
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool Extract(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool RefineHands(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool ApplyHandResult(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static FString GetHandReadiness(UCubelessMocapSettings* Settings);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool RefineFeet(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static bool ApplyFootResult(UCubelessMocapSettings* Settings, FString& Result);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Body Mocap") static FString GetFootReadiness(UCubelessMocapSettings* Settings);
};
