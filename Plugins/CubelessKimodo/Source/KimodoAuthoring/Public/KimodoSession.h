#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "KimodoSession.generated.h"

UENUM()
enum class EKimodoEffector : uint8 { Hand_R, Hand_L, Foot_R, Foot_L };
UENUM()
enum class EKimodoMode : uint8 { Offset, Pin };
USTRUCT(BlueprintType)
struct FKimodoFrameMarker
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="IK") FName Id;
    UPROPERTY(EditAnywhere, Category="IK") EKimodoEffector Effector = EKimodoEffector::Hand_R;
    UPROPERTY(EditAnywhere, Category="IK") EKimodoMode Mode = EKimodoMode::Offset;
    UPROPERTY(EditAnywhere, Category="IK") int32 StartFrame = 0;
    UPROPERTY(EditAnywhere, Category="IK") int32 EndFrame = 0;
    UPROPERTY(EditAnywhere, Category="IK") int32 AnchorFrame = 0;
    UPROPERTY(EditAnywhere, Category="IK") FVector OffsetCm = FVector(0,0,5);
    UPROPERTY(EditAnywhere, Category="IK", meta=(ClampMin="0",ClampMax="1")) float Weight = 1;
    UPROPERTY(EditAnywhere, Category="IK", meta=(ClampMin="0")) int32 BlendInFrames = 0;
    UPROPERTY(EditAnywhere, Category="IK", meta=(ClampMin="0")) int32 BlendOutFrames = 0;
};
UCLASS(Config=EditorPerProjectUserSettings)
class UKimodoSession : public UObject
{
    GENERATED_BODY()
public:
    UKimodoSession();
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
    UPROPERTY(Config,EditAnywhere,Category="Generation",meta=(ContentDir,DisplayName="저장 폴더"))
    FDirectoryPath OutputFolder;
    UFUNCTION(BlueprintCallable,Category="Kimodo")
    static bool CopyMotionTracks(UAnimSequence* Source, UAnimSequence* Destination);
    UFUNCTION(BlueprintCallable,Category="Kimodo")
    static UAnimSequence* SnapshotMotion(UAnimSequence* Source);
    UPROPERTY(EditAnywhere, Category="Generation", meta=(MultiLine="true")) FString Prompt;
    UPROPERTY(EditAnywhere, Category="Generation",meta=(ClampMin="1",ClampMax="30")) float Duration = 2;
    UPROPERTY(EditAnywhere, Category="Generation") int32 Seed = 1218;
    UPROPERTY(VisibleAnywhere, Category="Preview") TObjectPtr<UAnimSequence> SourceClip;
    UPROPERTY(VisibleAnywhere, Category="Preview") TObjectPtr<UAnimSequence> PreviewClip;
    UPROPERTY(EditAnywhere, Transient, Category="Preview", meta=(DisplayName="비교할 클립", ToolTip="비워 두면 현재 미리보기 클립을 비교합니다.")) TObjectPtr<UAnimSequence> ComparisonClip;
    UPROPERTY(EditAnywhere, Category="IK") TArray<FKimodoFrameMarker> Markers;
    UPROPERTY(EditAnywhere, Category="JSON") FString JsonFile;
    UPROPERTY(VisibleAnywhere, Category="Preview") FString Status;
    UFUNCTION(BlueprintCallable,Category="Kimodo") static UKimodoSession* GetSession();
    UFUNCTION(BlueprintCallable,Category="Kimodo") static void EvaluateComparisonFrame(USkeletalMeshComponent* Component, float Seconds);
    UFUNCTION(BlueprintCallable,Category="Kimodo") void SetMessage(const FString& Message) { Status=Message; }
    UFUNCTION(BlueprintCallable,Category="Kimodo") void SetSourceAsset(UAnimSequence* Clip) { SourceClip=PreviewClip=Clip; }
    UFUNCTION(BlueprintCallable,Category="Kimodo") void Preview(UAnimSequence* Clip);
    UFUNCTION(BlueprintCallable,Category="Kimodo") int32 MarkCurrentFrame();
    UFUNCTION(BlueprintCallable,Category="Kimodo") void TogglePlayback();
    UFUNCTION(BlueprintCallable,Category="Kimodo") int32 CurrentFrame() const;
    UFUNCTION(BlueprintCallable,Category="Kimodo") void SeekFrame(int32 Frame);
};



