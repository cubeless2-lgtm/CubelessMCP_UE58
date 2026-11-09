#include "SCBMComparison.h"
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
 Settings=Args._Settings;Settings->PreviewHand=0;
 Player.Reset(NewObject<UMediaPlayer>());Player->PlayOnOpen=false;Player->SetLooping(false);
 Texture.Reset(NewObject<UMediaTexture>());Texture->SetMediaPlayer(Player.Get());Texture->UpdateResource();
 Brush.SetResourceObject(Texture.Get());Brush.ImageSize=FVector2D(490,750);Brush.DrawAs=ESlateBrushDrawType::Image;
 ChildSlot[SNew(SVerticalBox)
  +SVerticalBox::Slot().FillHeight(1)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1).Padding(4)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(TEXT("원본 영상")))]
     +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){return FText::FromString(Settings->DebugBones?TEXT("본 디버그 끄기"):TEXT("본 디버그 켜기"));}).ToolTipText(FText::FromString(TEXT("초록은 영상 검출, 자홍은 모델 추정 카메라의 원시 본, 청록은 오른쪽 3D 뷰에 실제 표시되는 본입니다. 수동 카메라 이동도 청록에 즉시 반영됩니다. 캐시만 사용하며 모캡 추론이나 에셋을 변경하지 않습니다."))).OnClicked_Lambda([this](){Settings->DebugBones=!Settings->DebugBones;return FReply::Handled();})]]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Visibility_Lambda([this](){return Settings->DebugBones?EVisibility::Visible:EVisibility::Collapsed;}).Text_Lambda([this](){return FText::FromString(DebugOverlay?DebugOverlay->Status:FString());})]
    +SVerticalBox::Slot().FillHeight(1)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride_Lambda([this](){return 750*Aspect;}).HeightOverride(750)[SNew(SOverlay)+SOverlay::Slot()[SNew(SImage).Image(&Brush)]+SOverlay::Slot()[SAssignNew(DebugOverlay,SCBMDebugOverlay).Visibility(EVisibility::HitTestInvisible).Clipping(EWidgetClipping::ClipToBounds)]]]]]
   +SHorizontalBox::Slot().FillWidth(1).Padding(4)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){FString Label=TEXT("선택된 결과");
     if(Settings->PreviewAnimation==Settings->CorrectedHandAnimation&&Settings->PreviewAnimation)Label=TEXT("HandFlow + 본 보정");
     else if(Settings->PreviewAnimation==Settings->CorrectedBodyAnimation&&Settings->PreviewAnimation)Label=TEXT("전신 + 본 보정");
     else if(Settings->PreviewAnimation==Settings->HandFlowAnimation&&Settings->PreviewAnimation)Label=TEXT("HandFlow");
     else if(Settings->PreviewAnimation==Settings->OriginalAnimation&&Settings->PreviewAnimation)Label=TEXT("전신 원본");
     return FText::FromString(TEXT("언리얼 애니메이션 - ")+Label);}).ToolTipText_Lambda([this](){return FText::FromString(Settings->PreviewAnimation?Settings->PreviewAnimation->GetPathName():TEXT("No animation selected"));})]
    +SVerticalBox::Slot().FillHeight(1)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride_Lambda([this](){return 750*Aspect;}).HeightOverride(750)[SAssignNew(View,SCBMAnimationViewport)]]]]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("영상 열기"))).OnClicked(this,&SCBMComparison::Load)]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SButton).Text_Lambda([this](){return FText::FromString(Player->IsPlaying()?TEXT("일시 정지"):TEXT("재생"));}).IsEnabled_Lambda([this](){return Player->IsReady()&&Settings->PreviewAnimation!=nullptr&&!Waiting;}).OnClicked_Lambda([this](){if(Player->IsPlaying()){Player->Pause();PlayAfterSeek=false;}else{if(Clock.Position>=Length-.02)Seek(0);if(Clock.Pending)PlayAfterSeek=true;else Player->SetRate(Settings->PreviewPlayRate);}return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("정지"))).OnClicked_Lambda([this](){Player->Pause();PlayAfterSeek=false;Seek(0);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("재생 속도")))]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(80)[SNew(SSpinBox<float>).MinValue(.1f).MaxValue(4.f).Delta(.05f).ToolTipText(FText::FromString(TEXT("영상과 애니메이션을 함께 재생하는 속도입니다. 저장 플레이 레이트에는 영향을 주지 않습니다. 디코더가 지원하지 않는 속도는 적용하지 않습니다."))).Value_Lambda([this](){return Settings->PreviewPlayRate;}).OnValueChanged_Lambda([this](float V){if(Player->IsReady()&&!Player->SupportsRate(V,false)){Notice=TEXT("영상 디코더가 지원하지 않는 재생 속도입니다.");return;}Settings->PreviewPlayRate=V;if(Player->IsPlaying())Player->SetRate(V);})]]
   +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SNew(SSlider).ToolTipText(FText::FromString(TEXT("드래그하는 동안 영상과 애니메이션의 시점을 함께 이동합니다."))).PreventThrottling(true).IsEnabled_Lambda([this](){return Player->IsReady()&&Length>0;}).Value_Lambda([this](){return Length>0?Clock.Position/Length:0.f;}).OnMouseCaptureBegin_Lambda([this](){Player->Pause();PlayAfterSeek=false;Clock.Scrubbing=true;}).OnMouseCaptureEnd_Lambda([this](){Clock.Scrubbing=false;Seek(Clock.Position);}).OnControllerCaptureBegin_Lambda([this](){Player->Pause();PlayAfterSeek=false;Clock.Scrubbing=true;}).OnControllerCaptureEnd_Lambda([this](){Clock.Scrubbing=false;Seek(Clock.Position);}).OnValueChanged_Lambda([this](float V){Player->Pause();PlayAfterSeek=false;Seek(V*Length);})]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("카메라 초기화"))).OnClicked_Lambda([this](){Settings->PreviewHand=0;Settings->PreviewYawOffset=0;Settings->PreviewZoom=1;View->NeedsFit=true;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("카메라 방향")))]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<float>).ToolTipText(FText::FromString(TEXT("비교 카메라의 수평 방향을 회전합니다. 애니메이션은 변경하지 않습니다."))).MinValue(-180).MaxValue(180).Value_Lambda([this](){return Settings->PreviewYawOffset;}).OnValueChanged_Lambda([this](float V){Settings->PreviewYawOffset=V;})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("확대")))]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<float>).ToolTipText(FText::FromString(TEXT("비교 뷰를 확대하거나 축소합니다."))).MinValue(.5f).MaxValue(2.f).Delta(.05f).Value_Lambda([this](){return Settings->PreviewZoom;}).OnValueChanged_Lambda([this](float V){Settings->PreviewZoom=V;})]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).ToolTipText(FText::FromString(TEXT("가로 FOV = 2 × atan(영상 폭 / (2 × 초점거리))입니다. 첫 프레임에서 모델이 추정한 초점거리를 사용하며 실제 렌즈 정보가 아닙니다. 확대 조절은 표시 FOV만 바꿉니다."))).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("카메라: %s | 가로 FOV %.2f°"),*View->CameraMode(),View->Camera->ViewFOV));})]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("%.2f / %.2f s  |  %s"),Clock.Position,Length,*Notice));})]];
}
SCBMComparison::~SCBMComparison(){Player->Close();Brush.SetResourceObject(nullptr);}
void SCBMComparison::UpdateDebugProjection(double Time,bool Synchronized){
 ViewPoints=Settings->DebugBones?View->ProjectVisibleBones():TArray<FVector3f>();
 DebugOverlay->Update(Settings->DebugBones,Settings->PreviewAnimation,Settings->Video.FilePath,Settings->StartSeconds,Time,Synchronized,ViewPoints,View->CameraMode());
}
bool SCBMComparison::PrepareScreenshot(){
 if(!View||!View->Camera||!View->Camera->Viewport)return false;
 // Slate screenshot painting alone can reuse the previous viewport texture
 // when the editor is background-throttled. Evaluate and draw the requested
 // pose/camera explicitly before reading pixels for comparison evidence.
 const double Time=Player->IsReady()&&!Clock.Scrubbing&&!Clock.Pending?FMath::Clamp(Player->GetTime().GetTotalSeconds()-Settings->StartSeconds,0.,Length):Clock.Position;
 View->Pose(Settings->TargetMesh,Settings->PreviewAnimation,Time);
 View->Frame(Aspect,Settings->PreviewYawOffset,Settings->PreviewZoom,Settings->PreviewHand,Settings->MatchVideoCamera);
 UpdateDebugProjection(Time,!Clock.Pending&&!Clock.Scrubbing&&OpenedFile==Settings->Video.FilePath);
 View->Camera->Viewport->Draw();FlushRenderingCommands();
 auto C=StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera);auto O=MakeShared<FJsonObject>();
 O->SetNumberField(TEXT("preview_seconds"),Time);O->SetNumberField(TEXT("evaluated_animation_seconds"),View->Mesh->GetSingleNodeInstance()?View->Mesh->GetSingleNodeInstance()->GetCurrentTime():-1.);
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
 Player->Close();OpenedFile=Settings->Video.FilePath;Clock.Reset();Length=0;PlayAfterSeek=false;
 View->NeedsFit=true;
 Waiting=Player->OpenFile(OpenedFile);Notice=Waiting?TEXT("Opening video..."):TEXT("Cannot open video");
 return FReply::Handled();
}
void SCBMComparison::Seek(double Time){
 if(!Player->IsReady())return;
 Clock.Request(Time,Length);if(!Player->Seek(FTimespan::FromSeconds(Settings->StartSeconds+Clock.Position))){Clock.Pending=false;Notice=TEXT("Video seek unavailable");}
 View->Pose(Settings->TargetMesh,Settings->PreviewAnimation,Clock.Position);
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
  Length=FMath::Min(double(Settings->DurationSeconds),FMath::Max(0.,Player->GetDuration().GetTotalSeconds()-Settings->StartSeconds));
  if(Settings->PreviewAnimation)Length=FMath::Min(Length,double(Settings->PreviewAnimation->GetPlayLength()));
  const float MediaFPS=Player->GetVideoTrackFrameRate(INDEX_NONE,INDEX_NONE);
  Clock.Advance(Player->GetTime().GetTotalSeconds()-Settings->StartSeconds,Length,1./(MediaFPS>0?MediaFPS:FMath::Max(Settings->FrameRate,1)),D*Settings->PreviewPlayRate,Player->IsPlaying());
  if(PlayAfterSeek&&!Clock.Pending&&!Clock.Scrubbing){PlayAfterSeek=false;Player->SetRate(Settings->PreviewPlayRate);}
  if(Player->IsPlaying()&&Player->GetTime().GetTotalSeconds()-Settings->StartSeconds>=Length-.001)Player->Pause();
  Notice=Settings->PreviewAnimation?TEXT("영상과 동기화됩니다. 저장 옵션으로 비교 결과를 전환하세요."):TEXT("모캡 완료 후 결과가 표시됩니다.");
 }
 if(Settings->PreviewAnimation&&Settings->TargetMesh&&Settings->PreviewAnimation->GetSkeleton()!=Settings->TargetMesh->GetSkeleton()){
  Player->Pause();Notice=TEXT("Preview animation skeleton does not match target mesh");return;
 }
 if(Player->IsReady()&&OpenedFile!=Settings->Video.FilePath){Player->Pause();Notice=TEXT("Video changed: click Load comparison before playback");}
 View->Pose(Settings->TargetMesh,Settings->PreviewAnimation,Clock.Position);
 View->Frame(Aspect,Settings->PreviewYawOffset,Settings->PreviewZoom,Settings->PreviewHand,Settings->MatchVideoCamera);
 UpdateDebugProjection(Clock.Position,Player->IsReady()&&!Clock.Pending&&!Clock.Scrubbing&&OpenedFile==Settings->Video.FilePath);
}
