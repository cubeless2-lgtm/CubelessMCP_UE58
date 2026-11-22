#include "KimodoSession.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "AnimPreviewInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Paths.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"

UKimodoSession::UKimodoSession()
{
    OutputFolder.Path=TEXT("/Game/Kimodo");
}

void UKimodoSession::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    if (Event.GetMemberPropertyName()==GET_MEMBER_NAME_CHECKED(UKimodoSession,OutputFolder))
        SaveConfig();
}

UAnimSequence* UKimodoSession::SnapshotMotion(UAnimSequence* Source)
{
    if (!Source) return nullptr;
    const FName Name=MakeUniqueObjectName(GetTransientPackage(),Source->GetClass(),TEXT("KimodoSnapshot"));
    UAnimSequence* Copy=DuplicateObject<UAnimSequence>(Source,GetTransientPackage(),Name);
    Copy->ClearFlags(RF_Public|RF_Standalone);
    Copy->SetFlags(RF_Transient);
    return Copy;
}

bool UKimodoSession::CopyMotionTracks(UAnimSequence* Source, UAnimSequence* Destination)
{
    if (!Source || !Destination || Source==Destination || Source->GetSkeleton()!=Destination->GetSkeleton())
        return false;
    const IAnimationDataModel* Model=Source->GetDataModel();
    const IAnimationDataModel* Target=Destination->GetDataModel();
    if (!Model || !Target || Model->GetFrameRate()!=Target->GetFrameRate()
        || Model->GetNumberOfFrames()!=Target->GetNumberOfFrames())
        return false;
    TArray<FName> Names;
    Model->GetBoneTrackNames(Names);
    for (const FName Name : Names)
        if (!Target->IsValidBoneTrackName(Name)) return false;
    IAnimationDataController& Controller=Destination->GetController();
    IAnimationDataController::FScopedBracket Bracket(Controller, NSLOCTEXT("Kimodo","UpdateMotion","Update Kimodo motion"), false);
    for (const FName Name : Names)
    {
        TArray<FTransform> Keys;
        Model->GetBoneTrackTransforms(Name, Keys);
        TArray<FVector> Positions, Scales;
        TArray<FQuat> Rotations;
        for (const FTransform& Key : Keys)
        {
            Positions.Add(Key.GetTranslation());
            Rotations.Add(Key.GetRotation());
            Scales.Add(Key.GetScale3D());
        }
        if (!Controller.SetBoneTrackKeys(Name,Positions,Rotations,Scales,false)) return false;
    }
    return !Names.IsEmpty();
}

namespace {
UAnimPreviewInstance* Player(const UAnimSequence* Clip)
{
    if (Clip)
        for (TObjectIterator<UDebugSkelMeshComponent> It; It; ++It)
            if (It->IsRegistered() && It->PreviewInstance && It->PreviewInstance->GetCurrentAsset()==Clip)
                return It->PreviewInstance;
    return nullptr;
}
}
UKimodoSession* UKimodoSession::GetSession()
{
    return GetMutableDefault<UKimodoSession>();
}
void UKimodoSession::Preview(UAnimSequence* Clip)
{
    if (!Clip) return;
    PreviewClip=Clip;
    GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Clip);
}
int32 UKimodoSession::CurrentFrame() const
{
    auto* P=Player(PreviewClip);
    return P ? FMath::Clamp(FMath::RoundToInt(P->GetCurrentTime()*PreviewClip->GetSamplingFrameRate().AsDecimal()),0,PreviewClip->GetNumberOfSampledKeys()-1) : INDEX_NONE;
}
void UKimodoSession::TogglePlayback()
{
    if (auto* P=Player(PreviewClip)) P->SetPlaying(!P->IsPlaying());
    else Preview(PreviewClip);
}
void UKimodoSession::SeekFrame(int32 Frame)
{
    if (auto* P=Player(PreviewClip))
    {
        P->SetPlaying(false);
        P->SetPosition(FMath::Clamp(Frame,0,PreviewClip->GetNumberOfSampledKeys()-1)/PreviewClip->GetSamplingFrameRate().AsDecimal(),false);
    }
}
int32 UKimodoSession::MarkCurrentFrame()
{
    const int32 F=CurrentFrame();
    if(F<0){Status=TEXT("결과 미리보기를 먼저 여세요."); return F;}
    SeekFrame(F);
    FKimodoFrameMarker& M=Markers.AddDefaulted_GetRef();
    M.Id=FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
    M.StartFrame=M.EndFrame=M.AnchorFrame=F;
    return F;
}


void UKimodoSession::EvaluateComparisonFrame(USkeletalMeshComponent* Component, float Seconds)
{
    if (!IsValid(Component) || !Component->IsRegistered() || !Component->GetSingleNodeInstance())
        return;
    // An editor world can skip normal skeletal ticks even while single-node playback is active.
    Component->SetPosition(Seconds, false);
    Component->TickAnimation(0.f, false);
    Component->RefreshBoneTransforms();
    Component->MarkRenderDynamicDataDirty();
    if (FLevelEditorViewportClient* View = GCurrentLevelEditingViewportClient)
    {
        View->RequestRealTimeFrames(2);
        if (View->Viewport)
        {
            View->Viewport->InvalidateDisplay();
            View->Viewport->Draw();
        }
    }
}

