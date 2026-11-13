#include "SCBMComparison.h"
#include "SCBMRangeSlider.h"
#include "CBMClipRange.h"
#include "Widgets/Layout/SSplitter.h"
#include "SCBMDebugOverlay.h"
#include "Widgets/SOverlay.h"
#include "CubelessMocapSettings.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "PreviewScene.h"
#include "Engine/TextureCube.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "Components/DirectionalLightComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static const FText& CBMPreviewRealtimeReason(){static const FText Reason=NSLOCTEXT("CubelessBodyMocap","PreviewInference","Cubeless motion preview waiting for inference");return Reason;}
class FCBMPreviewViewportClient : public FEditorViewportClient {
public:
 int32 LastAA=-1,LastGI=-1,LastReflection=-1;
 double RenderedHFOV=0,RenderedVFOV=0;
 FCBMPreviewViewportClient(FPreviewScene* Scene,const TSharedRef<SEditorViewport>& Widget)
  :FEditorViewportClient(nullptr,Scene,Widget){
  EngineShowFlags.SetLumenGlobalIllumination(false);
  EngineShowFlags.SetLumenReflections(false);
  EngineShowFlags.SetScreenSpaceReflections(false);
  EngineShowFlags.SetTemporalAA(false);
  EngineShowFlags.SetMotionBlur(false);
  EngineShowFlags.SetBloom(false);
 }
 virtual void OverridePostProcessSettings(FSceneView& View) override {
  auto& P=View.FinalPostProcessSettings;
  P.DynamicGlobalIlluminationMethod=EDynamicGlobalIlluminationMethod::None;
  P.ReflectionMethod=EReflectionMethod::None;
  P.MotionBlurAmount=0;
  P.AutoExposureMethod=AEM_Manual;
  P.AutoExposureApplyPhysicalCameraExposure=false;
  P.AutoExposureBias=0;
  P.LocalExposureHighlightContrastScale=1;
  P.LocalExposureShadowContrastScale=1;
 }
 virtual FSceneView* CalcSceneView(FSceneViewFamily* Family,const int32 StereoViewIndex=INDEX_NONE) override {
  auto* V=FEditorViewportClient::CalcSceneView(Family,StereoViewIndex);
  if(V&&IsPerspective()&&V->UnscaledViewRect.Width()>0&&V->UnscaledViewRect.Height()>0){
   // This comparison defines ViewFOV horizontally, independently of the
   // level editor's global MaintainX/MaintainY preference.
   FMatrix P=V->ProjectionMatrixUnadjustedForRHI;P.M[0][0]=1./FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(double(ViewFOV),1.,170.)*.5));
   P.M[1][1]=P.M[0][0]*V->UnscaledViewRect.Width()/V->UnscaledViewRect.Height();V->UpdateProjectionMatrix(P);
   RenderedHFOV=FMath::RadiansToDegrees(2*FMath::Atan(1./P.M[0][0]));RenderedVFOV=FMath::RadiansToDegrees(2*FMath::Atan(1./P.M[1][1]));
  }
  if(V){V->AntiAliasingMethod=AAM_FXAA;LastAA=int32(V->AntiAliasingMethod);LastGI=int32(V->FinalPostProcessSettings.DynamicGlobalIlluminationMethod);LastReflection=int32(V->FinalPostProcessSettings.ReflectionMethod);}
  return V;
 }
};

class FCBMPreviewScene : public FPreviewScene {
public:
 FCBMPreviewScene():FPreviewScene(FPreviewScene::ConstructionValues()){
  // Do not inherit the asset editor's mutable lighting/post-process profile.
  SetSkyCubemap(LoadObject<UTextureCube>(nullptr,TEXT("/Engine/EditorMaterials/AssetViewer/EpicQuadPanorama_CC+EV1.EpicQuadPanorama_CC+EV1")));
  // Independent studio lights keep metallic Manny readable without Lumen GI.
  SetLightDirection(FRotator(-35,-90,0));SetLightBrightness(4.f);SetSkyBrightness(1.5f);
  DirectionalLight->SetForwardShadingPriority(1);
  for(const auto& Pair:{TPair<FRotator,float>(FRotator(-20,35,0),2.f),TPair<FRotator,float>(FRotator(-30,150,0),1.5f)}){
   auto* Light=NewObject<UDirectionalLightComponent>();Light->SetIntensity(Pair.Value);Light->SetCastShadows(false);
   AddComponent(Light,FTransform(Pair.Key));
  }
  UpdateCaptureContents();
 }
 virtual FLinearColor GetBackgroundColor() const override{return FLinearColor(.16f,.18f,.21f);}
};
class SCBMAnimationViewport : public SEditorViewport {
public:
 SLATE_BEGIN_ARGS(SCBMAnimationViewport){} SLATE_END_ARGS()
 TUniquePtr<FPreviewScene> Scene;
 TStrongObjectPtr<USkeletalMeshComponent> Mesh;
 UAnimSequence* Current=nullptr;
 TSharedPtr<FEditorViewportClient> Camera;
 bool NeedsFit=true;
 float LastAspect=0,LastYaw=0,LastZoom=0;
 int32 LastHand=0;
 bool LastMatch=false,CalibrationAvailable=false,FramingAvailable=false,UsingVideoCamera=false;
 float VideoFocal=0,VideoCX=0,VideoCY=0,VideoPitch=0,VideoRoll=0;
 FVector4f VideoBox;FString CalibrationRun;TArray<float> FittedCamera;FVector ExportOrigin=FVector::ZeroVector;bool ExactCamera=false;FVector MatchedLocation=FVector::ZeroVector;FRotator MatchedRotation=FRotator::ZeroRotator;float MatchedFOV=0;
 bool IsMatchedView()const{return UsingVideoCamera&&ExactCamera&&FMath::IsNearlyZero(LastYaw)&&FMath::IsNearlyEqual(LastZoom,1.f)&&Camera->GetViewLocation().Equals(MatchedLocation,.01)&&Camera->GetViewRotation().Equals(MatchedRotation,.01)&&FMath::IsNearlyEqual(Camera->ViewFOV,MatchedFOV,.001f);}
 bool IsExactCamera()const{return IsMatchedView();}
 FString CameraMode()const{return IsMatchedView()?TEXT("영상 추정"):TEXT("수동 / 자유 시점");}
 TArray<FVector3f> ProjectVisibleBones(){
  TArray<FVector3f> Points;
  if(!Camera||!Camera->Viewport||Camera->Viewport->GetSizeXY().GetMin()<=0)return Points;
  FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Camera->Viewport,Camera->GetScene(),Camera->EngineShowFlags));
  const FSceneView* SV=Camera->CalcSceneView(&Family);if(!SV)return Points;
  for(const TCHAR* Name:{TEXT("pelvis"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")}){
   FVector3f V(0,0,0);FVector2D P;const FVector World=Mesh->GetBoneLocation(Name);
   if(Mesh->GetBoneIndex(Name)!=INDEX_NONE&&SV->WorldToScreen(World).W>FMath::Max(.001f,SV->NearClippingDistance)&&SV->WorldToPixel(World,P)&&FMath::IsFinite(P.X)&&FMath::IsFinite(P.Y))
    V=FVector3f((P.X-SV->UnscaledViewRect.Min.X)/SV->UnscaledViewRect.Width(),(P.Y-SV->UnscaledViewRect.Min.Y)/SV->UnscaledViewRect.Height(),1);
   Points.Add(V);
  }return Points;
 }
 FBox FrameBounds{ForceInit};
 FString CalibrationSignature;double NextCalibrationCheck=0;
 void ReadCalibration(UAnimSequence* Anim){
  const FString Name=Anim?Anim->GetName():FString();const FString Run=Name.StartsWith(TEXT("Body_"))&&Name.Len()>=37?Name.Mid(5,32):FString();
  const bool NewRun=Run!=CalibrationRun;const double Now=FPlatformTime::Seconds();if(!NewRun&&Now<NextCalibrationCheck)return;NextCalibrationCheck=Now+1.;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/Run;FString Stamp;
  for(const TCHAR* File:{TEXT("Preview_OptimizedCamera.f32"),TEXT("Preview_Camera.f32"),TEXT("DebugBones.json")})Stamp+=IFileManager::Get().GetTimeStamp(*(Dir/File)).ToIso8601();
  if(!NewRun&&Stamp==CalibrationSignature)return;CalibrationSignature=Stamp;
  NeedsFit=NeedsFit||NewRun||IsMatchedView();CalibrationRun=Run;CalibrationAvailable=false;FramingAvailable=false;ExactCamera=false;FittedCamera.Reset();VideoFocal=0;if(Run.IsEmpty())return;
  TArray<uint8> Data;
  ExactCamera=false;ExportOrigin=FVector::ZeroVector;
  FittedCamera.Reset();if(FFileHelper::LoadFileToArray(Data,*(Dir/TEXT("Preview_OptimizedCamera.f32")))&&Data.Num()==48){FittedCamera.SetNumUninitialized(12);FMemory::Memcpy(FittedCamera.GetData(),Data.GetData(),48);for(float X:FittedCamera)if(!FMath::IsFinite(X)){FittedCamera.Reset();break;}}
  if(FittedCamera.Num()==12){FString Text;TSharedPtr<FJsonObject> Meta;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;double Version=0;
   if(FFileHelper::LoadFileToString(Text,*(Dir/TEXT("DebugBones.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Meta)&&Meta.IsValid()&&Meta->TryGetNumberField(TEXT("version"),Version)&&Version==2&&Meta->TryGetArrayField(TEXT("camera"),Values)&&Values->Num()==10){double X,Y,Z;if((*Values)[7]->TryGetNumber(X)&&(*Values)[8]->TryGetNumber(Y)&&(*Values)[9]->TryGetNumber(Z)&&FMath::IsFinite(X)&&FMath::IsFinite(Y)&&FMath::IsFinite(Z)){ExportOrigin=FVector(X,Y,Z);ExactCamera=true;}}}
  if(!FFileHelper::LoadFileToArray(Data,*(Dir/TEXT("Preview_Camera.f32"))))FFileHelper::LoadFileToArray(Data,*(Dir/TEXT("Camera_Ground.f32")));
  if(Data.Num()<20)return;float V[5];FMemory::Memcpy(V,Data.GetData(),20);for(float X:V)if(!FMath::IsFinite(X))return;
  VideoFocal=V[0];VideoCX=V[1];VideoCY=V[2];VideoPitch=V[3];VideoRoll=V[4];CalibrationAvailable=VideoFocal>1&&VideoCX>1&&VideoCY>1;
  if(FFileHelper::LoadFileToArray(Data,*(Dir/TEXT("Preview_Boxes.f32"))))for(int32 I=0;I+16<=Data.Num();I+=16){float B[4];FMemory::Memcpy(B,Data.GetData()+I,16);bool Valid=true;for(float X:B)Valid=Valid&&FMath::IsFinite(X);if(Valid&&B[2]>B[0]+1&&B[3]>B[1]+1){VideoBox=FVector4f(B[0],B[1],B[2],B[3]);FramingAvailable=true;break;}}
 }
 void Construct(const FArguments&){
  Scene=MakeUnique<FCBMPreviewScene>();
  Mesh.Reset(NewObject<USkeletalMeshComponent>());Scene->AddComponent(Mesh.Get(),FTransform::Identity);
  SEditorViewport::Construct(SEditorViewport::FArguments());
 }
 virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override {
  auto C=MakeShared<FCBMPreviewViewportClient>(Scene.Get(),SharedThis(this));Camera=C;
  C->SetViewLocation(FVector(0,230,95));C->SetViewRotation(FRotator(0,-90,0));C->ViewFOV=38;
  C->SetViewMode(VMI_Lit);C->SetRealtime(true);C->EngineShowFlags.SetSelectionOutline(false);
  C->EngineShowFlags.SetGrid(false);
  return C;
 }
 void Pose(USkeletalMesh* Target,UAnimSequence* Anim,double Time){
  ReadCalibration(Anim);
  if(Mesh->GetSkeletalMeshAsset()!=Target){Mesh->SetSkeletalMeshAsset(Target);NeedsFit=true;}
  if(Current!=Anim){if(!Current)NeedsFit=true;Current=Anim;Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);Mesh->SetAnimation(Anim);}
  Mesh->SetPosition(Time,false);Mesh->SetPlayRate(0);Mesh->Stop();
  Mesh->TickAnimation(0,false);Mesh->RefreshBoneTransforms();Mesh->UpdateComponentToWorld();
  // This preview world is not advanced by gameplay Tick. Publish the manually
  // evaluated bones to the render proxy even while the video is paused/scrubbed.
  Mesh->UpdateBounds();Mesh->MarkRenderTransformDirty();Mesh->MarkRenderDynamicDataDirty();
  // Texture/resource readiness can defer the initial sky capture. Service its
  // pending update just as standard preview-scene tickers do.
  Scene->UpdateCaptureContents();
  Scene->GetWorld()->SendAllEndOfFrameUpdates();
  Invalidate();
 }
 void Frame(float Aspect,float Yaw,float Zoom,int32 Hand,bool MatchVideo){
  if(!Mesh->GetSkeletalMeshAsset())return;
  if(LastHand!=Hand){NeedsFit=true;LastHand=Hand;}
  if(Hand){
   // Track the chosen hand without changing the animation/component transform.
   FrameBounds=FBox(ForceInit);
   const TCHAR* Side=Hand==1?TEXT("l"):TEXT("r");
   FrameBounds+=Mesh->GetBoneLocation(FName(*FString::Printf(TEXT("hand_%s"),Side)));
   for(const TCHAR* Finger:{TEXT("index"),TEXT("middle"),TEXT("pinky"),TEXT("ring"),TEXT("thumb")})for(int32 I=1;I<=3;++I)
    FrameBounds+=Mesh->GetBoneLocation(FName(*FString::Printf(TEXT("%s_%02d_%s"),Finger,I,Side)));
   FrameBounds=FrameBounds.ExpandBy(5.);
  }else if(NeedsFit){
   // A fixed first-frame fit keeps legacy/free-camera framing independent of selection time.
   FrameBounds=FBox(ForceInit);const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();TArray<FTransform> Global=Ref.GetRefBonePose();
   const auto* Model=Current?Current->GetDataModel():nullptr;
   for(int B=0;B<Ref.GetNum();++B){if(Model&&Model->IsValidBoneTrackName(Ref.GetBoneName(B)))Global[B]=Model->GetBoneTrackTransform(Ref.GetBoneName(B),FFrameNumber(0));if(B>0)Global[B]=Global[B]*Global[Ref.GetParentIndex(B)];}
   for(const TCHAR* Name:{TEXT("pelvis"),TEXT("head"),TEXT("foot_l"),TEXT("foot_r"),TEXT("hand_l"),TEXT("hand_r"),TEXT("upperarm_l"),TEXT("upperarm_r")})
    if(const int B=Ref.FindBoneIndex(Name);B!=INDEX_NONE)FrameBounds+=Global[B].GetTranslation();
   if(!FrameBounds.IsValid)FrameBounds=Mesh->Bounds.GetBox();
   FrameBounds=FrameBounds.ExpandBy(FVector(10,10,14));
  }
  // A viewport resize must not reset a manually positioned camera. FOV is
  // horizontal and the renderer already derives the vertical field of view.
  if(Hand||NeedsFit||(Aspect!=LastAspect&&IsMatchedView())||Yaw!=LastYaw||Zoom!=LastZoom||LastMatch!=MatchVideo){
   const FVector Center=FrameBounds.GetCenter(),Extent=FrameBounds.GetExtent();
   const double Angle=FMath::DegreesToRadians(90.+Yaw);
   const FVector Out(FMath::Cos(Angle),FMath::Sin(Angle),0),Right(-Out.Y,Out.X,0);
   const double HalfWidth=FMath::Abs(Right.X)*Extent.X+FMath::Abs(Right.Y)*Extent.Y;
   const double Depth=FMath::Abs(Out.X)*Extent.X+FMath::Abs(Out.Y)*Extent.Y;
   const double Distance=FMath::Max(HalfWidth,Extent.Z*Aspect)/FMath::Tan(FMath::DegreesToRadians(19.))/.86/FMath::Clamp(Zoom,.5f,2.f)+Depth;
   UsingVideoCamera=MatchVideo&&CalibrationAvailable&&!Hand;Camera->ViewFOV=38;
   if(UsingVideoCamera){
    // Same world->camera basis as the body optimizer (SMPL X/Y/Z -> UE X/Z/Y).
    // CV camera right/down/forward become the UE view's right/-up/forward.
    const double CP=FMath::Cos(-VideoPitch),SP=FMath::Sin(-VideoPitch),CR=FMath::Cos(VideoRoll),SR=FMath::Sin(VideoRoll);
    const FQuat Orbit(FVector::UpVector,FMath::DegreesToRadians(Yaw));
    FVector O=Orbit.RotateVector(FVector(-SR,CP*CR,-SP*CR));
    FVector R=Orbit.RotateVector(FVector(CR,CP*SR,-SP*SR));
    FVector U=Orbit.RotateVector(FVector(0,SP,CP));
    if(FittedCamera.Num()==12){const auto& F=FittedCamera;O=Orbit.RotateVector(-FVector(F[6],F[8],F[7]));R=Orbit.RotateVector(FVector(F[0],F[2],F[1]));U=Orbit.RotateVector(-FVector(F[3],F[5],F[4]));}
    auto Span=[&](const FVector& A){return FMath::Abs(A.X)*Extent.X+FMath::Abs(A.Y)*Extent.Y+FMath::Abs(A.Z)*Extent.Z;};
    const double TanH=VideoCX/VideoFocal,TanV=TanH/FMath::Max(Aspect,.01f);
    Camera->ViewFOV=FMath::RadiansToDegrees(2*FMath::Atan(TanH));
    const double Fill=FramingAvailable?FMath::Clamp(double((VideoBox.W-VideoBox.Y)/(2*VideoCY)),.1,.95):.86;
    const double D=Span(U)/(Fill*TanV)/FMath::Clamp(Zoom,.5f,2.f)+Span(O);
    FVector Aim=Center;
    if(FramingAvailable){const double X=(VideoBox.X+VideoBox.Z)/(2*VideoCX)-1,Y=1-(VideoBox.Y+VideoBox.W)/(2*VideoCY);Aim-=R*(X*D*TanH)+U*(Y*D*TanV);}
    FVector Location=Aim+O*D;
    if(ExactCamera){const auto& F=FittedCamera;
     const FVector C(-(F[0]*F[9]+F[3]*F[10]+F[6]*F[11])-ExportOrigin.X,-(F[1]*F[9]+F[4]*F[10]+F[7]*F[11])-ExportOrigin.Y,-(F[2]*F[9]+F[5]*F[10]+F[8]*F[11])-ExportOrigin.Z);
     Location=Orbit.RotateVector(FVector(C.X,C.Z,C.Y)*100.-Center)+Center;
     Camera->ViewFOV=FMath::RadiansToDegrees(2*FMath::Atan(TanH/FMath::Clamp(Zoom,.5f,2.f)));
    }
    Camera->SetViewLocation(Location);Camera->SetViewRotation(FRotationMatrix::MakeFromXY(-O,R).Rotator());MatchedLocation=Camera->GetViewLocation();MatchedRotation=Camera->GetViewRotation();MatchedFOV=Camera->ViewFOV;
   }else{Camera->SetViewLocation(Center+Out*Distance);Camera->SetViewRotation((-Out).Rotation());}
   LastMatch=MatchVideo;
   LastAspect=Aspect;LastYaw=Yaw;LastZoom=Zoom;NeedsFit=false;Invalidate();
  }
 }
};
void SCBMComparison::Construct(const FArguments& Args){
 Settings=Args._Settings;Settings->PreviewHand=0;ResultStart=Settings->StartSeconds;ResultDuration=Settings->DurationSeconds;
 Player.Reset(NewObject<UMediaPlayer>());Player->PlayOnOpen=false;Player->SetLooping(false);
 Texture.Reset(NewObject<UMediaTexture>());Texture->SetMediaPlayer(Player.Get());Texture->UpdateResource();
 Brush.SetResourceObject(Texture.Get());Brush.ImageSize=FVector2D(490,750);Brush.DrawAs=ESlateBrushDrawType::Image;
 ChildSlot[SNew(SVerticalBox)
  +SVerticalBox::Slot().FillHeight(1)[SAssignNew(VideoSplitter,SSplitter)
   +SSplitter::Slot().Value(.5f)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(TEXT("원본 영상")))]
     +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){return FText::FromString(Settings->DebugBones?TEXT("본 디버그 끄기"):TEXT("본 디버그 켜기"));}).ToolTipText(FText::FromString(TEXT("초록은 영상 검출, 자홍은 모델 추정 카메라의 원시 본, 청록은 오른쪽 3D 뷰에 실제 표시되는 본입니다. 수동 카메라 이동도 청록에 즉시 반영됩니다. 캐시만 사용하며 모캡 추론이나 에셋을 변경하지 않습니다."))).OnClicked_Lambda([this](){Settings->DebugBones=!Settings->DebugBones;return FReply::Handled();})]]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Visibility_Lambda([this](){return Settings->DebugBones?EVisibility::Visible:EVisibility::Collapsed;}).Text_Lambda([this](){return FText::FromString(DebugOverlay?DebugOverlay->Status:FString());})]
    +SVerticalBox::Slot().FillHeight(1)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride_Lambda([this](){return 750*Aspect;}).HeightOverride(750)[SNew(SOverlay)+SOverlay::Slot()[SNew(SImage).Image(&Brush)]+SOverlay::Slot()[SAssignNew(DebugOverlay,SCBMDebugOverlay).Visibility(EVisibility::HitTestInvisible).Clipping(EWidgetClipping::ClipToBounds)]]]]]
   +SSplitter::Slot().Value(.5f)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){if(ReviewSource)return FText::FromString(TEXT("언리얼 애니메이션 - 원본 구간 선택 중"));if(!Settings->PreviewAnimation)return FText::FromString(TEXT("언리얼 애니메이션 - 계산 전"));FString Label=TEXT("선택된 결과");
     if(Settings->PreviewAnimation==Settings->CorrectedHandAnimation&&Settings->PreviewAnimation)Label=TEXT("HandFlow + 손가락 보정");
     else if(Settings->PreviewAnimation==Settings->CorrectedBodyAnimation&&Settings->PreviewAnimation)Label=TEXT("전신 + 손가락 보정");
     else if(Settings->PreviewAnimation==Settings->HandFlowAnimation&&Settings->PreviewAnimation)Label=TEXT("HandFlow");
     else if(Settings->PreviewAnimation==Settings->OriginalAnimation&&Settings->PreviewAnimation)Label=TEXT("전신 원본");
     return FText::FromString(TEXT("언리얼 애니메이션 - ")+Label);}).ToolTipText_Lambda([this](){return FText::FromString(Settings->PreviewAnimation?Settings->PreviewAnimation->GetPathName():TEXT("No animation selected"));})]
    +SVerticalBox::Slot().FillHeight(1)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride_Lambda([this](){return 750*Aspect;}).HeightOverride(750)[SAssignNew(View,SCBMAnimationViewport)]]]]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){return FText::FromString(Player->IsPlaying()?TEXT("일시 정지"):TEXT("재생"));}).IsEnabled_Lambda([this](){return Player->IsReady()&&!Waiting&&!InferenceRenderingSuspended&&OpenedFile==Settings->Video.FilePath;}).OnClicked_Lambda([this](){FString M;Control(Player->IsPlaying()?TEXT("pause"):TEXT("play"),0,M);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SButton).Text(FText::FromString(TEXT("정지"))).OnClicked_Lambda([this](){FString M;Control(TEXT("stop"),0,M);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("◀ 1f"))).ToolTipText(FText::FromString(TEXT("원본 영상 FPS 기준 이전 프레임으로 탐색합니다. 디코더 seek 후 표시됩니다."))).IsEnabled_Lambda([this](){return Player->IsReady()&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){StepFrame(-1);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("1f ▶"))).ToolTipText(FText::FromString(TEXT("원본 영상 FPS 기준 다음 프레임으로 탐색합니다."))).IsEnabled_Lambda([this](){return Player->IsReady()&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){StepFrame(1);return FReply::Handled();})]
   +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SNew(SSlider).ToolTipText(FText::FromString(TEXT("영상과 표시 결과의 시점을 이동합니다. 원본 전체 모드에서 계산 구간을 선택하세요."))).PreventThrottling(true).IsEnabled_Lambda([this](){return Player->IsReady()&&Length>0&&!UCubelessMocapLibrary::IsBusy();}).Value_Lambda([this](){return Length>0?Clock.Position/Length:0.f;}).OnMouseCaptureBegin_Lambda([this](){Player->Pause();PlayAfterSeek=false;Clock.Scrubbing=true;}).OnMouseCaptureEnd_Lambda([this](){Clock.Scrubbing=false;Seek(Clock.Position);}).OnControllerCaptureBegin_Lambda([this](){Player->Pause();PlayAfterSeek=false;Clock.Scrubbing=true;}).OnControllerCaptureEnd_Lambda([this](){Clock.Scrubbing=false;Seek(Clock.Position);}).OnValueChanged_Lambda([this](float V){Player->Pause();PlayAfterSeek=false;Seek(V*Length);})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("재생 속도 ")))]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(64)[SNew(SSpinBox<float>).MinValue(.1f).MaxValue(4.f).Delta(.05f).ToolTipText(FText::FromString(TEXT("미리보기 영상/애니메이션의 재생 속도입니다. 내보내기 RateScale과 별개입니다."))).Value_Lambda([this](){return Settings->PreviewPlayRate;}).OnValueChanged_Lambda([this](float V){FString M;Control(TEXT("rate"),V,M);})]]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SCBMRangeSlider).ToolTipText(FText::FromString(TEXT("양쪽 파란 핸들로 계산할 시작/끝을 지정합니다. 끝은 제외 경계이며 최대 120초입니다. 분석 FPS 격자에 맞춥니다."))).IsEnabled_Lambda([this](){return CanEditRange();}).Duration_Lambda([this](){return SourceDuration();}).Start_Lambda([this](){return double(Settings->StartSeconds);}).End_Lambda([this](){return double(Settings->StartSeconds+Settings->DurationSeconds);}).OnChanged_Lambda([this](double A,double B){SetRange(A,B);})]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){return FText::FromString(ReviewSource?(Settings->PreviewAnimation?TEXT("결과 구간 보기"):TEXT("선택 구간 미리보기")):TEXT("원본 전체 / 구간 선택"));}).IsEnabled_Lambda([this](){return Player->IsReady()&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){if(ReviewSource)ShowResult();else ReviewVideo();return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("시작 ")))]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(85)[SNew(SSpinBox<double>).MinValue(0).Delta(.1).Value_Lambda([this](){return double(Settings->StartSeconds);}).IsEnabled_Lambda([this](){return CanEditRange();}).OnValueCommitted_Lambda([this](double V,ETextCommit::Type){SetRange(V,Settings->StartSeconds+Settings->DurationSeconds);})]]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SButton).Text(FText::FromString(TEXT("현재를 시작"))).IsEnabled_Lambda([this](){return CanEditRange();}).OnClicked_Lambda([this](){SetRange(PlaybackStart+Clock.Position,Settings->StartSeconds+Settings->DurationSeconds);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(STextBlock).Text(FText::FromString(TEXT("끝 ")))]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(85)[SNew(SSpinBox<double>).MinValue(0).Delta(.1).Value_Lambda([this](){return double(Settings->StartSeconds+Settings->DurationSeconds);}).IsEnabled_Lambda([this](){return CanEditRange();}).OnValueCommitted_Lambda([this](double V,ETextCommit::Type){SetRange(Settings->StartSeconds,V);})]]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SButton).Text(FText::FromString(TEXT("현재까지 포함"))).IsEnabled_Lambda([this](){return CanEditRange();}).OnClicked_Lambda([this](){SetRange(Settings->StartSeconds,PlaybackStart+Clock.Position+1./FMath::Max(Settings->FrameRate,1));return FReply::Handled();})]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("카메라 초기화"))).OnClicked_Lambda([this](){Settings->PreviewHand=0;Settings->PreviewYawOffset=0;Settings->PreviewZoom=1;View->NeedsFit=true;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("카메라 방향")))]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<float>).ToolTipText(FText::FromString(TEXT("비교 카메라의 수평 방향을 회전합니다. 애니메이션은 변경하지 않습니다."))).MinValue(-180).MaxValue(180).Value_Lambda([this](){return Settings->PreviewYawOffset;}).OnValueChanged_Lambda([this](float V){Settings->PreviewYawOffset=V;})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("확대")))]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<float>).ToolTipText(FText::FromString(TEXT("비교 뷰를 확대하거나 축소합니다."))).MinValue(.5f).MaxValue(2.f).Delta(.05f).Value_Lambda([this](){return Settings->PreviewZoom;}).OnValueChanged_Lambda([this](float V){Settings->PreviewZoom=V;})]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).ToolTipText(FText::FromString(TEXT("가로 FOV = 2 × atan(영상 폭 / (2 × 초점거리))입니다. 첫 프레임에서 모델이 추정한 초점거리를 사용하며 실제 렌즈 정보가 아닙니다. 확대 조절은 표시 FOV만 바꿉니다."))).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("카메라: %s | 가로 FOV %.2f°"),*View->CameraMode(),View->Camera->ViewFOV));})]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("원본 %.3fs · %df / %.2ffps  |  구간 %.2f / %.2fs\n%s"),PlaybackStart+Clock.Position,FMath::RoundToInt((PlaybackStart+Clock.Position)*SourceFPS()),SourceFPS(),Clock.Position,Length,*Notice));})]];
}
SCBMComparison::~SCBMComparison(){if(InferenceRenderingSuspended&&View.IsValid()&&View->Camera.IsValid())View->Camera->RemoveRealtimeOverride(CBMPreviewRealtimeReason(),false);Player->Close();Brush.SetResourceObject(nullptr);}
void SCBMComparison::UpdateDebugProjection(double Time,bool Synchronized){
 ViewPoints=Settings->DebugBones&&!ReviewSource?View->ProjectVisibleBones():TArray<FVector3f>();
 DebugOverlay->Update(Settings->DebugBones&&!ReviewSource,Settings->PreviewAnimation,Settings->Video.FilePath,PlaybackStart,Time,Synchronized,ViewPoints,View->CameraMode());
}
bool SCBMComparison::PrepareScreenshot(){
 if(!View||!View->Camera||!View->Camera->Viewport)return false;
 // Slate screenshot painting alone can reuse the previous viewport texture
 // when the editor is background-throttled. Evaluate and draw the requested
 // pose/camera explicitly before reading pixels for comparison evidence.
 const double Time=Player->IsReady()&&!Clock.Scrubbing&&!Clock.Pending?FMath::Clamp(Player->GetTime().GetTotalSeconds()-PlaybackStart,0.,Length):Clock.Position;
 View->Pose(Settings->TargetMesh,DisplayAnimation(),Time);
 View->Frame(Aspect,Settings->PreviewYawOffset,Settings->PreviewZoom,Settings->PreviewHand,Settings->MatchVideoCamera);
 UpdateDebugProjection(Time,!Clock.Pending&&!Clock.Scrubbing&&OpenedFile==Settings->Video.FilePath);
 View->Camera->Viewport->Draw();FlushRenderingCommands();
 auto C=StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera);auto O=MakeShared<FJsonObject>();
 O->SetBoolField(TEXT("review_source"),ReviewSource);O->SetNumberField(TEXT("playback_start"),PlaybackStart);O->SetNumberField(TEXT("selection_start"),Settings->StartSeconds);O->SetNumberField(TEXT("selection_end_exclusive"),Settings->StartSeconds+Settings->DurationSeconds);O->SetBoolField(TEXT("media_playing"),Player->IsPlaying());O->SetNumberField(TEXT("source_fps"),SourceFPS());O->SetBoolField(TEXT("inference_rendering_suspended"),InferenceRenderingSuspended);O->SetBoolField(TEXT("preview_realtime"),View->Camera->IsRealtime());O->SetNumberField(TEXT("preview_seconds"),Time);O->SetNumberField(TEXT("evaluated_animation_seconds"),View->Mesh->GetSingleNodeInstance()?View->Mesh->GetSingleNodeInstance()->GetCurrentTime():-1.);
 O->SetNumberField(TEXT("media_seconds"),Player->GetTime().GetTotalSeconds());O->SetBoolField(TEXT("scrubbing"),Clock.Scrubbing);O->SetBoolField(TEXT("seek_pending"),Clock.Pending);
 O->SetNumberField(TEXT("aa_method"),C->LastAA);O->SetNumberField(TEXT("gi_method"),C->LastGI);O->SetNumberField(TEXT("reflection_method"),C->LastReflection);
 O->SetBoolField(TEXT("video_camera_matched"),View->UsingVideoCamera);O->SetBoolField(TEXT("exact_video_camera"),View->IsExactCamera());const FVector CL=View->Camera->GetViewLocation();O->SetNumberField(TEXT("camera_x"),CL.X);O->SetNumberField(TEXT("camera_y"),CL.Y);O->SetNumberField(TEXT("camera_z"),CL.Z);O->SetBoolField(TEXT("video_framing_available"),View->FramingAvailable);O->SetNumberField(TEXT("camera_pitch_radians"),View->VideoPitch);O->SetNumberField(TEXT("camera_roll_radians"),View->VideoRoll);O->SetNumberField(TEXT("horizontal_fov_degrees"),View->Camera->ViewFOV);
 const FRotator ActualRotation=View->Camera->GetViewRotation();O->SetNumberField(TEXT("actual_camera_pitch_degrees"),ActualRotation.Pitch);O->SetNumberField(TEXT("actual_camera_yaw_degrees"),ActualRotation.Yaw);O->SetNumberField(TEXT("actual_camera_roll_degrees"),ActualRotation.Roll);O->SetStringField(TEXT("display_camera_mode"),View->CameraMode());
 O->SetNumberField(TEXT("source_focal_pixels"),View->VideoFocal);O->SetNumberField(TEXT("rendered_horizontal_fov_degrees"),C->RenderedHFOV);O->SetNumberField(TEXT("vertical_fov_degrees"),C->RenderedVFOV);
 {TArray<TSharedPtr<FJsonValue>> Points;const auto Canvas=DebugOverlay->GetCanvasSize();if(Canvas.X>0&&Canvas.Y>0)for(const auto& P:DebugOverlay->GetAppliedPoints()){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("x"),P.X/Canvas.X);J->SetNumberField(TEXT("y"),P.Y/Canvas.Y);J->SetBoolField(TEXT("in_front"),P.Z>.5f);Points.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("overlay_applied_uv"),Points);}
 O->SetBoolField(TEXT("lumen_gi"),C->EngineShowFlags.LumenGlobalIllumination);O->SetBoolField(TEXT("lumen_reflections"),C->EngineShowFlags.LumenReflections);O->SetBoolField(TEXT("temporal_aa"),C->EngineShowFlags.TemporalAA);O->SetBoolField(TEXT("motion_blur"),C->EngineShowFlags.MotionBlur);
 // Record rendered projection independently of the overlay for camera parity checks.
 {FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(C->Viewport,C->GetScene(),C->EngineShowFlags));FSceneView* SV=C->CalcSceneView(&Family);TArray<TSharedPtr<FJsonValue>> Points;
  if(SV)for(const TCHAR* Name:{TEXT("pelvis"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")}){FVector2D P=FVector2D::ZeroVector;const bool Visible=SV->WorldToPixel(View->Mesh->GetBoneLocation(Name),P);auto Point=MakeShared<FJsonObject>();Point->SetStringField(TEXT("bone"),Name);Point->SetBoolField(TEXT("in_front"),Visible);Point->SetNumberField(TEXT("x"),(P.X-SV->UnscaledViewRect.Min.X)/FMath::Max(1,SV->UnscaledViewRect.Width()));Point->SetNumberField(TEXT("y"),(P.Y-SV->UnscaledViewRect.Min.Y)/FMath::Max(1,SV->UnscaledViewRect.Height()));Points.Add(MakeShared<FJsonValueObject>(Point));}O->SetArrayField(TEXT("rendered_bone_uv"),Points);}
 FString Json;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap/ComparisonPreview.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 return true;
}
FReply SCBMComparison::Load(){
 if(!OpenedFile.IsEmpty()&&OpenedFile!=Settings->Video.FilePath){Settings->PreviewAnimation=nullptr;Settings->OriginalAnimation=nullptr;Settings->HandFlowAnimation=nullptr;Settings->CorrectedBodyAnimation=nullptr;Settings->CorrectedHandAnimation=nullptr;Settings->ExportedSequence=nullptr;}
 ReviewSource=!UCubelessMocapLibrary::IsBusy()&&!Settings->PreviewAnimation;
 if(!ReviewSource){ResultStart=Settings->StartSeconds;ResultDuration=Settings->DurationSeconds;}
 PlaybackStart=ReviewSource?0.:ResultStart;
 Player->Close();OpenedFile=Settings->Video.FilePath;Clock.Reset();Length=0;PlayAfterSeek=false;
 View->NeedsFit=true;
 Waiting=Player->OpenFile(OpenedFile);Notice=Waiting?TEXT("Opening video..."):TEXT("Cannot open video");
 return FReply::Handled();
}
bool SCBMComparison::Seek(double Time){
 if(!Player->IsReady())return false;
 const double F=SourceFPS();Time=FMath::RoundToDouble((PlaybackStart+Time)*F)/F-PlaybackStart;
 Clock.Request(Time,Length);SeekTarget=PlaybackStart+Clock.Position;SeekBegan=FPlatformTime::Seconds();SeekRetries=0;SeekFailed=false;SeekExact=false;SeekError=0;
 if(!Player->Seek(FTimespan::FromSeconds(SeekTarget))){Clock.Pending=false;SeekFailed=true;Notice=TEXT("Video seek unavailable");}
 View->Pose(Settings->TargetMesh,DisplayAnimation(),Clock.Position);
 return !SeekFailed;
}
void SCBMComparison::Tick(const FGeometry& G,double T,float D){
 SCompoundWidget::Tick(G,T,D);
 if(Waiting&&Player->HasError()){Waiting=false;Notice=TEXT("Video decoder error");}
 if(Waiting&&Player->IsReady()){
  Waiting=false;const FIntPoint Size=Player->GetVideoTrackDimensions(INDEX_NONE,INDEX_NONE);
  if(Size.X>0&&Size.Y>0)Aspect=float(Size.X)/Size.Y;
  Seek(0);
 }
 if(Player->IsReady()){
  Length=ReviewSource?SourceDuration():FMath::Min(ResultDuration,FMath::Max(0.,SourceDuration()-PlaybackStart));
  if(DisplayAnimation())Length=FMath::Min(Length,double(DisplayAnimation()->GetPlayLength()));
  const float MediaFPS=Player->GetVideoTrackFrameRate(INDEX_NONE,INDEX_NONE);
  const double Frame=1./(MediaFPS>0?MediaFPS:FMath::Max(Settings->FrameRate,1));const bool WasPending=Clock.Pending;
  Clock.Advance(Player->GetTime().GetTotalSeconds()-PlaybackStart,Length,Frame,D*Settings->PreviewPlayRate,Player->IsPlaying());
  if(WasPending&&!Clock.Pending){SeekError=Player->GetTime().GetTotalSeconds()-SeekTarget;SeekExact=true;}
  if(Clock.Pending&&!Clock.Scrubbing&&!Player->IsPlaying()&&!UCubelessMocapLibrary::IsBusy()){
   const double Elapsed=FPlatformTime::Seconds()-SeekBegan,Actual=Player->GetTime().GetTotalSeconds();
   // Some decoders expose their first sample at +1/+2 frames. Only at the
   // source start, acknowledge that actual sample explicitly (never as exact).
   const bool FirstSample=Elapsed>=1.5&&SeekTarget<Frame*.5&&Actual>=0&&Actual<=Frame*2.1;
   if(FirstSample||Elapsed>=3){Clock.Pending=false;Clock.Observe(Actual-PlaybackStart,Length,Frame*.5);SeekError=Actual-SeekTarget;SeekExact=false;SeekFailed=!FirstSample;if(SeekFailed)PlayAfterSeek=false;}
  }
  if(PlayAfterSeek&&!Clock.Pending&&!Clock.Scrubbing){PlayAfterSeek=false;Player->SetRate(Settings->PreviewPlayRate);}
  if(Player->IsPlaying()&&Player->GetTime().GetTotalSeconds()-PlaybackStart>=Length-.001)Player->Pause();
  Notice=ReviewSource?(Settings->FrameDirectory.Path.IsEmpty()?TEXT("원본 전체를 확인하며 파란 핸들로 계산 구간을 선택하세요."):TEXT("기존 프레임 폴더가 지정되어 구간 편집이 잠겼습니다. 영상 구간을 선택하려면 고급 설정에서 비우세요.")):(Settings->PreviewAnimation?TEXT("계산한 구간과 선택 결과를 동기화합니다. 위 결과 선택기로 비교하세요."):TEXT("선택한 원본 구간을 검토합니다. 모캡을 실행하면 3D 결과가 표시됩니다."));
  if(SeekFailed)Notice=TEXT("영상 탐색을 완료하지 못했습니다. 실제 디코더 시각을 표시합니다. 다시 탐색하세요.");
  else if(!SeekExact&&!Clock.Pending&&FMath::Abs(SeekError)>Frame*.5)Notice=FString::Printf(TEXT("디코더의 첫 표시 프레임으로 이동했습니다. 요청과 차이 %.3f초입니다."),SeekError);
 }
 const bool SuspendRendering=Settings->PauseEditorRendering&&UCubelessMocapLibrary::IsBusy()&&!Settings->PreviewAnimation;
 if(SuspendRendering){
  if(!InferenceRenderingSuspended){View->Camera->AddRealtimeOverride(false,CBMPreviewRealtimeReason());Player->Pause();PlayAfterSeek=false;InferenceRenderingSuspended=true;}
  Notice=TEXT("모캡 계산 중에는 결과 대기 중인 미리보기를 잠시 멈춥니다.");return;
 }
 if(InferenceRenderingSuspended){View->Camera->RemoveRealtimeOverride(CBMPreviewRealtimeReason(),false);InferenceRenderingSuspended=false;}
 if(Settings->PreviewAnimation&&Settings->TargetMesh&&Settings->PreviewAnimation->GetSkeleton()!=Settings->TargetMesh->GetSkeleton()){
  Player->Pause();Notice=TEXT("Preview animation skeleton does not match target mesh");return;
 }
 if(Player->IsReady()&&OpenedFile!=Settings->Video.FilePath){Player->Pause();Notice=TEXT("Video changed: click Load comparison before playback");}
 View->Pose(Settings->TargetMesh,DisplayAnimation(),Clock.Position);
 View->Frame(Aspect,Settings->PreviewYawOffset,Settings->PreviewZoom,Settings->PreviewHand,Settings->MatchVideoCamera);
 UpdateDebugProjection(Clock.Position,Player->IsReady()&&!Clock.Pending&&!Clock.Scrubbing&&OpenedFile==Settings->Video.FilePath);
}

double SCBMComparison::SourceDuration()const{return Player->IsReady()?FMath::Max(0.,Player->GetDuration().GetTotalSeconds()):0.;}
double SCBMComparison::SourceFPS()const{const float F=Player->GetVideoTrackFrameRate(INDEX_NONE,INDEX_NONE);return F>0?F:double(FMath::Max(Settings->FrameRate,1));}
UAnimSequence* SCBMComparison::DisplayAnimation()const{return ReviewSource?nullptr:Settings->PreviewAnimation.Get();}
bool SCBMComparison::CanEditRange()const{return ReviewSource&&Settings->FrameDirectory.Path.IsEmpty()&&Player->IsReady()&&OpenedFile==Settings->Video.FilePath&&!UCubelessMocapLibrary::IsBusy()&&FCBMClipRange::Normalize(0,SourceDuration(),0,SourceDuration(),Settings->FrameRate).Y>0;}
void SCBMComparison::OpenSourceVideo(){if(OpenedFile!=Settings->Video.FilePath||!Player->IsReady())Load();Player->Pause();PlayAfterSeek=false;ReviewSource=true;PlaybackStart=0;Length=SourceDuration();Seek(0);}
bool SCBMComparison::ReviewVideo(){Player->Pause();PlayAfterSeek=false;const double Absolute=PlaybackStart+Clock.Position;ReviewSource=true;PlaybackStart=0;Length=SourceDuration();return Seek(Absolute);}
bool SCBMComparison::ShowResult(bool PreserveTime){if(!Player->IsReady())return false;const bool WasSource=ReviewSource;const double Absolute=PlaybackStart+Clock.Position;if(!Settings->OriginalAnimation&&!Settings->HandFlowAnimation){ResultStart=Settings->StartSeconds;ResultDuration=Settings->DurationSeconds;}Player->Pause();PlayAfterSeek=false;ReviewSource=false;PlaybackStart=ResultStart;Length=FMath::Min(ResultDuration,FMath::Max(0.,SourceDuration()-PlaybackStart));if(DisplayAnimation())Length=FMath::Min(Length,double(DisplayAnimation()->GetPlayLength()));if(PreserveTime&&!WasSource&&Clock.Position<=Length){View->Pose(Settings->TargetMesh,DisplayAnimation(),Clock.Position);return true;}return Seek(PreserveTime?Absolute-PlaybackStart:0);}
bool SCBMComparison::StepFrame(int32 Direction){Player->Pause();PlayAfterSeek=false;const double F=SourceFPS();return Seek((FMath::RoundToDouble((PlaybackStart+Clock.Position)*F)+Direction)/F-PlaybackStart);}
void SCBMComparison::SetRange(double A,double B){
 if(!CanEditRange()||!FMath::IsFinite(A)||!FMath::IsFinite(B))return;
 const FVector2D Bounds=FCBMClipRange::Normalize(A,B,Settings->StartSeconds,SourceDuration(),Settings->FrameRate);
 Settings->StartSeconds=Bounds.X;Settings->DurationSeconds=Bounds.Y-Bounds.X;
}

bool SCBMComparison::Control(const FString& Action,double Value,FString& Message){
 if(!FMath::IsFinite(Value)){Message=TEXT("nonfinite_value");return false;}
 if(UCubelessMocapLibrary::IsBusy()&&Action!=TEXT("pause")){Message=TEXT("busy");return false;}
 if(Action==TEXT("rate")){
  if(Value<.1||Value>4||(Player->IsReady()&&!Player->SupportsRate(Value,false))){Message=TEXT("unsupported_preview_rate");return false;}
  if(Player->IsPlaying()&&!Player->SetRate(Value)){Message=TEXT("playback_rate_rejected");return false;}Settings->PreviewPlayRate=Value;Message=TEXT("preview_rate_set");return true;
 }
 if(!Player->IsReady()||Waiting||OpenedFile!=Settings->Video.FilePath){Message=TEXT("media_not_ready_or_video_changed");return false;}
 bool Applied=true;
 if(Action==TEXT("source"))Applied=ReviewVideo();
 else if(Action==TEXT("result"))Applied=ShowResult();
 else if(Action==TEXT("pause")){Player->Pause();PlayAfterSeek=false;}
 else if(Action==TEXT("stop")){Player->Pause();PlayAfterSeek=false;Applied=Seek(0);}
 else if(Action==TEXT("play")){
  if(InferenceRenderingSuspended){Message=TEXT("inference_rendering_suspended");return false;}
  if(Clock.Position>=Length-.02&&!Seek(0)){Message=TEXT("seek_rejected");return false;}
  if(Clock.Pending)PlayAfterSeek=true;
  else if(!Player->SetRate(Settings->PreviewPlayRate)){Message=TEXT("playback_rate_rejected");return false;}
 }
 else if(Action==TEXT("seek")){Player->Pause();PlayAfterSeek=false;Applied=Seek(Value-PlaybackStart);}
 else if(Action==TEXT("step")){
  if(Value!=FMath::RoundToDouble(Value)||FMath::Abs(Value)>10000){Message=TEXT("step_requires_integer_frames_up_to_10000");return false;}
  Applied=StepFrame(int32(Value));
 }else{Message=TEXT("unknown_action: source,result,play,pause,stop,seek,step,rate");return false;}
 Message=!Applied?TEXT("seek_rejected"):Clock.Pending?TEXT("seek_accepted; poll seek_pending and seek_failed"):TEXT("applied");return Applied;
}
bool SCBMComparison::EditRange(double A,double B,FString& Message){
 if(!FMath::IsFinite(A)||!FMath::IsFinite(B)||B<=A){Message=TEXT("range_requires_finite_start_before_exclusive_end");return false;}
 if(!Player->IsReady()||Waiting||OpenedFile!=Settings->Video.FilePath||UCubelessMocapLibrary::IsBusy()||!Settings->FrameDirectory.Path.IsEmpty()){
  Message=TEXT("range_unavailable: media_not_ready,video_changed,busy,or_frame_directory");return false;
 }
 if(FCBMClipRange::Normalize(0,SourceDuration(),0,SourceDuration(),Settings->FrameRate).Y<=0){Message=TEXT("source_too_short");return false;}
 if(!ReviewSource)ReviewVideo();
 if(!CanEditRange()){Message=TEXT("source_too_short");return false;}
 SetRange(A,B);Message=TEXT("range_set; see normalized state bounds");return true;
}
void SCBMComparison::SetVideoFraction(float Value){VideoSplitter->SlotAt(0).SetSizeValue(Value);VideoSplitter->SlotAt(1).SetSizeValue(1-Value);}
TSharedRef<FJsonObject> SCBMComparison::ReadState()const{
 auto O=MakeShared<FJsonObject>();const bool Ready=Player->IsReady();
 const double Time=Ready&&!Clock.Scrubbing&&!Clock.Pending?FMath::Clamp(Player->GetTime().GetTotalSeconds()-PlaybackStart,0.,Length):Clock.Position;
 O->SetBoolField(TEXT("media_ready"),Ready&&!Waiting&&OpenedFile==Settings->Video.FilePath);O->SetBoolField(TEXT("media_error"),Player->HasError());
 O->SetBoolField(TEXT("review_source"),ReviewSource);O->SetBoolField(TEXT("media_playing"),Player->IsPlaying());O->SetBoolField(TEXT("seek_pending"),Clock.Pending);O->SetBoolField(TEXT("scrubbing"),Clock.Scrubbing);
 O->SetBoolField(TEXT("seek_failed"),SeekFailed);O->SetBoolField(TEXT("seek_exact"),SeekExact);O->SetNumberField(TEXT("seek_error_seconds"),SeekError);O->SetNumberField(TEXT("seek_retry_count"),SeekRetries);O->SetBoolField(TEXT("play_after_seek"),PlayAfterSeek);O->SetNumberField(TEXT("media_rate"),Player->GetRate());
 O->SetNumberField(TEXT("source_duration"),SourceDuration());O->SetNumberField(TEXT("source_fps"),SourceFPS());O->SetNumberField(TEXT("playback_start"),PlaybackStart);O->SetNumberField(TEXT("playback_length"),Length);
 O->SetNumberField(TEXT("preview_seconds"),Time);O->SetNumberField(TEXT("media_seconds"),Player->GetTime().GetTotalSeconds());O->SetNumberField(TEXT("requested_source_seconds"),SeekTarget);O->SetNumberField(TEXT("result_start"),ResultStart);O->SetNumberField(TEXT("result_duration"),ResultDuration);
 O->SetNumberField(TEXT("source_frame"),FMath::RoundToInt64((PlaybackStart+Time)*SourceFPS()));
 O->SetNumberField(TEXT("evaluated_animation_seconds"),View->Mesh->GetSingleNodeInstance()?View->Mesh->GetSingleNodeInstance()->GetCurrentTime():-1.);
 O->SetNumberField(TEXT("selection_start"),Settings->StartSeconds);O->SetNumberField(TEXT("selection_end_exclusive"),Settings->StartSeconds+Settings->DurationSeconds);
 O->SetBoolField(TEXT("range_editable"),CanEditRange());O->SetBoolField(TEXT("inference_rendering_suspended"),InferenceRenderingSuspended);
 O->SetNumberField(TEXT("video_fraction"),VideoSplitter->SlotAt(0).GetSizeValue());O->SetStringField(TEXT("notice"),Notice);return O;
}
