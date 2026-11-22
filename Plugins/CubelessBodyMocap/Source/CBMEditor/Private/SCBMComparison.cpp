#include "SCBMComparison.h"
#include "CBMDebugProvenance.h"
#include "CBMRepresentative.h"
#include "CBMPoseIK.h"
#include "Misc/PackageName.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "SceneManagement.h"
#include "HitProxies.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SCBMRangeSlider.h"
#include "SCBMPlaybackSlider.h"
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
#include "Widgets/Layout/SSpacer.h"
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
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Texture2D.h"
#include "Misc/SecureHash.h"

static const FText& CBMPreviewRealtimeReason(){static const FText Reason=NSLOCTEXT("CubelessBodyMocap","PreviewInference","Cubeless motion preview waiting for inference");return Reason;}
struct HCBMPoseBone : public HHitProxy {DECLARE_HIT_PROXY();FName Bone;explicit HCBMPoseBone(FName B):HHitProxy(HPP_Foreground),Bone(B){}};
IMPLEMENT_HIT_PROXY(HCBMPoseBone,HHitProxy);
struct HCBMPoseAxis : public HHitProxy {DECLARE_HIT_PROXY();int Axis;explicit HCBMPoseAxis(int A):HHitProxy(HPP_UI),Axis(A){}};
IMPLEMENT_HIT_PROXY(HCBMPoseAxis,HHitProxy);
struct FCBMPosePickPoint{FName Bone,Parent;FVector Position,ParentPosition;double Radius=3.;};
class FCBMPreviewViewportClient : public FEditorViewportClient {
public:
 TFunction<void(FPrimitiveDrawInterface*)> DrawPose;TFunction<void(FName)> PickPose;TFunction<bool()> PoseEditable;TFunction<FVector()> PoseLocation;TFunction<bool(FRotator)> RotatePose;
 TFunction<void(bool)> PoseGesture;
 TFunction<int()> PoseMode;TFunction<FQuat()> PoseBasis;TFunction<bool(FVector)> TranslatePose;TFunction<bool(int,double)> ScalePose;
 FVector2D LastDragPixel;
 int Tool()const{return PoseMode?PoseMode():1;}
 double GizmoRadius()const{return FMath::Clamp((PoseLocation()-GetViewLocation()).Size()*.075,12.,45.);}
 FVector GizmoAxis(int A)const{const FVector U=A==0?FVector::XAxisVector:A==1?FVector::YAxisVector:FVector::ZAxisVector;return Tool()==2&&PoseBasis?PoseBasis().RotateVector(U):U;}
 int HoverAxis=-1,DragTool=-1;bool PointerInside=false;FVector2D HoverPixel;FName DragBone;
 void EndGizmo(FViewport* VP){if(DragAxis>=0){DragAxis=-1;if(VP)VP->CaptureMouse(false);if(PoseGesture)PoseGesture(false);}HoverAxis=-1;Invalidate();}
 // Rendering and picking use the same shafts, arrowheads, rings and boxes.
 void GizmoSegments(TFunctionRef<void(int,const FVector&,const FVector&)> Segment)const{
  const FVector C=PoseLocation();const double R=GizmoRadius();
  auto Box=[&](int Axis,const FVector& P){for(int I=0;I<8;++I)for(int A=0;A<3;++A)if(!(I&(1<<A))){FVector X,Y;for(int K=0;K<3;++K){X[K]=(I&(1<<K))?R*.07:-R*.07;Y[K]=((I|(1<<A))&(1<<K))?R*.07:-R*.07;}Segment(Axis,P+X,P+Y);}};
  for(int A=0;A<3;++A){if(Tool()==1){const FVector U=A==0?FVector::YAxisVector:FVector::XAxisVector,W=A==2?FVector::YAxisVector:FVector::ZAxisVector;for(int I=0;I<64;++I){const double T=2*PI*I/64.,N=2*PI*(I+1)/64.;Segment(A,C+R*(U*FMath::Cos(T)+W*FMath::Sin(T)),C+R*(U*FMath::Cos(N)+W*FMath::Sin(N)));}}
   else{const FVector Axis=GizmoAxis(A),End=C+Axis*R;Segment(A,C+Axis*R*.12,End);if(Tool()==0){FVector Side=FVector::CrossProduct(Axis,GetViewRotation().Vector()).GetSafeNormal();if(Side.IsNearlyZero())Side=GetViewRotation().Quaternion().RotateVector(FVector::UpVector);Segment(A,End,End-Axis*R*.2+Side*R*.1);Segment(A,End,End-Axis*R*.2-Side*R*.1);}else Box(A,End);}}
  if(Tool()!=1)Box(3,C);
 }
 static double SegmentDistance(FVector2D P,FVector2D A,FVector2D B){const FVector2D D=B-A;const double T=FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(.0001,D.SizeSquared()),0.,1.);return (P-A-T*D).Size();}
 int GizmoHit(FViewport* VP,int X,int Y){if(Tool()<0||!PoseEditable||!PoseEditable()||!PoseLocation)return -1;FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(VP,GetScene(),EngineShowFlags));auto* V=CalcSceneView(&Family);if(!V)return -1;const FVector C=PoseLocation();FVector2D Center;if(!V->WorldToPixel(C,Center))return -1;const FVector2D Mouse(X,Y);if(Tool()!=1&&(Mouse-Center).Size()<=8)return 3;double Best=7.;int Axis=-1;
  GizmoSegments([&](int A,const FVector& From,const FVector& To){FVector2D P,Q;if(V->WorldToPixel(From,P)&&V->WorldToPixel(To,Q)){const double D=SegmentDistance(Mouse,P,Q);if(D<Best){Best=D;Axis=A;}}});
  return Axis;
 }
 bool TranslateAxis(int A,double Amount){return PoseEditable&&PoseEditable()&&TranslatePose&&A>=0&&A<3&&FMath::IsFinite(Amount)&&FMath::Abs(Amount)<=25&&TranslatePose(GizmoAxis(A)*Amount);}
 bool ScaleAxis(int A,double Factor){return PoseEditable&&PoseEditable()&&ScalePose&&A>=0&&A<=3&&FMath::IsFinite(Factor)&&Factor>=.5&&Factor<=2&&ScalePose(A,Factor);}

 TFunction<TArray<FCBMPosePickPoint>()> PosePoints;TFunction<FName()> SelectedPoseBone;bool PickingJoint=false;
 FName ResolvePoseBone(FSceneView& V,double X,double Y,bool Cycle,bool JointsOnly){
  if(!PosePoints)return NAME_None;const FVector2D Cursor(X,Y);struct FCandidate{FName Bone;double Distance,Depth;};TArray<FCandidate> Points;const auto Bones=PosePoints();const FVector Right=GetViewRotation().Quaternion().RotateVector(FVector::RightVector);
  for(const auto& B:Bones){FVector2D P,Q;if(!V.WorldToPixel(B.Position,P))continue;double Radius=6.;if(V.WorldToPixel(B.Position+Right*B.Radius,Q))Radius=FMath::Max(6.,(Q-P).Size()+2.);double D=(Cursor-P).Size();if(D<=Radius)Points.Add({B.Bone,D,(B.Position-GetViewLocation()).SizeSquared()});}
  Points.Sort([](const auto& A,const auto& B){const double AD=FMath::RoundToDouble(A.Distance*10),BD=FMath::RoundToDouble(B.Distance*10);if(AD!=BD)return AD<BD;if(A.Depth!=B.Depth)return A.Depth<B.Depth;return A.Bone.LexicalLess(B.Bone);});
  if(!Points.IsEmpty()){if(Cycle&&SelectedPoseBone){const int I=Points.IndexOfByPredicate([&](const auto& P){return P.Bone==SelectedPoseBone();});if(I!=INDEX_NONE)return Points[(I+1)%Points.Num()].Bone;}return Points[0].Bone;}
  if(JointsOnly)return NAME_None;
  // A segment runs from its parent joint to its child; it belongs to the parent bone.
  double Best=4.,Depth=TNumericLimits<double>::Max();FName Result;
  for(const auto& B:Bones){if(B.Parent.IsNone())continue;FVector2D A,Z;if(!V.WorldToPixel(B.ParentPosition,A)||!V.WorldToPixel(B.Position,Z))continue;const FVector2D D=Z-A;const double T=FMath::Clamp(FVector2D::DotProduct(Cursor-A,D)/FMath::Max(.0001,D.SizeSquared()),0.,1.);const double Distance=(Cursor-(A+T*D)).Size(),ZDepth=(FMath::Lerp(B.ParentPosition,B.Position,T)-GetViewLocation()).SizeSquared();if(Distance<Best-.05||(FMath::Abs(Distance-Best)<=.05&&ZDepth<Depth)){Best=Distance;Depth=ZDepth;Result=B.Parent;}}
  return Result;
 }
 bool SelectPoseAt(double X,double Y,bool Cycle,bool JointsOnly=false){if(!Viewport||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||X<0||Y<0||X>=Viewport->GetSizeXY().X||Y>=Viewport->GetSizeXY().Y)return false;FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Viewport,GetScene(),EngineShowFlags));auto* V=CalcSceneView(&Family);if(!V)return false;const FName B=ResolvePoseBone(*V,X,Y,Cycle,JointsOnly);if(B.IsNone()||!PickPose)return false;PickPose(B);Invalidate();return true;}

 int DragAxis=-1;FVector DragNormal,DragCenter,DragPrevious;bool HasDragVector=false;
 bool RotateAxis(int A,double Degrees){return PoseEditable&&PoseEditable()&&RotatePose&&A>=0&&A<3&&RotatePose(FQuat(A==0?FVector::XAxisVector:A==1?FVector::YAxisVector:FVector::ZAxisVector,FMath::DegreesToRadians(Degrees)).Rotator());}
 bool DragVector(FViewport* VP,int X,int Y,FVector& Out){FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(VP,GetScene(),EngineShowFlags));auto* V=CalcSceneView(&Family);if(!V)return false;FVector O,D;FSceneView::DeprojectScreenToWorld(FVector2D(X,Y),V->UnscaledViewRect,V->ViewMatrices.GetClipToWorld(),O,D);double Den=FVector::DotProduct(D,DragNormal);if(FMath::Abs(Den)<.0001)return false;double T=FVector::DotProduct(DragCenter-O,DragNormal)/Den;if(T<=0)return false;Out=(O+D*T-DragCenter).GetSafeNormal();return !Out.IsNearlyZero();}
 void Hover(FViewport* VP,int X,int Y){PointerInside=X>=0&&Y>=0&&X<VP->GetSizeXY().X&&Y<VP->GetSizeXY().Y;HoverPixel=FVector2D(X,Y);const int H=PointerInside?GizmoHit(VP,X,Y):-1;if(H!=HoverAxis){HoverAxis=H;Invalidate();}}
 bool Press(FViewport* VP,int X,int Y,bool Cycle=false){
  // Visible gizmos take priority. Shift explicitly selects/cycles overlapping joints.
  const int H=Cycle?-1:GizmoHit(VP,X,Y);
  if(H>=0){DragAxis=H;HoverAxis=H;DragTool=Tool();DragBone=SelectedPoseBone?SelectedPoseBone():NAME_None;DragCenter=PoseLocation();DragNormal=H<3?GizmoAxis(H):GetViewRotation().Vector();LastDragPixel=FVector2D(X,Y);HasDragVector=Tool()==1&&DragVector(VP,X,Y,DragPrevious);if(PoseGesture)PoseGesture(true);VP->CaptureMouse(true);Invalidate();return true;}
  if(SelectPoseAt(X,Y,Cycle,true)){PickingJoint=true;Hover(VP,X,Y);return true;}return false;
 }
 bool Release(FViewport* VP,int X,int Y){const bool Handled=PickingJoint||DragAxis>=0;PickingJoint=false;EndGizmo(VP);Hover(VP,X,Y);return Handled;}
 virtual bool InputKey(const FInputKeyEventArgs& E)override{
  if(E.Key==EKeys::LeftMouseButton){const bool Alt=E.Viewport->KeyState(EKeys::LeftAlt)||E.Viewport->KeyState(EKeys::RightAlt),Cycle=E.Viewport->KeyState(EKeys::LeftShift)||E.Viewport->KeyState(EKeys::RightShift);
   if(E.Event==IE_Pressed&&!Alt&&Press(E.Viewport,E.Viewport->GetMouseX(),E.Viewport->GetMouseY(),Cycle))return true;
   if(E.Event==IE_Released&&Release(E.Viewport,E.Viewport->GetMouseX(),E.Viewport->GetMouseY()))return true;}
  return FEditorViewportClient::InputKey(E);
 }
 void Drag(FViewport* VP,int X,int Y){if(DragAxis<0)return;if(!PoseEditable||!PoseEditable()||Tool()!=DragTool||(SelectedPoseBone&&SelectedPoseBone()!=DragBone)){EndGizmo(VP);return;}const FVector2D Pixel(X,Y),Diff=Pixel-LastDragPixel;LastDragPixel=Pixel;const double Precision=VP->KeyState(EKeys::LeftShift)||VP->KeyState(EKeys::RightShift)?.1:1.;
  if(Tool()==1){FVector Next;if(DragVector(VP,X,Y,Next)){if(HasDragVector){const double Angle=FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(DragNormal,FVector::CrossProduct(DragPrevious,Next)),FVector::DotProduct(DragPrevious,Next)));RotateAxis(DragAxis,FMath::Clamp(Angle*Precision,-15.,15.));}DragPrevious=Next;HasDragVector=true;}else RotateAxis(DragAxis,FMath::Clamp((Diff.X-Diff.Y)*.4*Precision,-15.,15.));return;}
  const FVector Center=PoseLocation();const double PixelWorld=2*(Center-GetViewLocation()).Size()*FMath::Tan(FMath::DegreesToRadians(ViewFOV*.5))/FMath::Max(1,VP->GetSizeXY().X);const FQuat ViewQ=GetViewRotation().Quaternion();const FVector Right=ViewQ.RotateVector(FVector::RightVector),Up=ViewQ.RotateVector(FVector::UpVector);
  if(DragAxis==3){if(Tool()==0&&TranslatePose)TranslatePose((Right*Diff.X-Up*Diff.Y)*PixelWorld*Precision);else if(Tool()==2)ScaleAxis(3,FMath::Clamp(FMath::Exp((Diff.X-Diff.Y)*.008*Precision),.5,2.));return;}
  FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(VP,GetScene(),EngineShowFlags));auto* V=CalcSceneView(&Family);FVector2D A,B;double D=-Diff.Y;if(V&&V->WorldToPixel(Center,A)&&V->WorldToPixel(Center+GizmoAxis(DragAxis)*GizmoRadius(),B)&&(B-A).Size()>4)D=FVector2D::DotProduct(Diff,(B-A).GetSafeNormal());
  if(Tool()==0)TranslateAxis(DragAxis,FMath::Clamp(D*PixelWorld*Precision,-25.,25.));else if(Tool()==2)ScaleAxis(DragAxis,FMath::Clamp(FMath::Exp(D*.008*Precision),.5,2.));
 }

 virtual bool InputAxis(const FInputKeyEventArgs& E)override{if(DragAxis>=0||PickingJoint)return true;return FEditorViewportClient::InputAxis(E);}
 virtual void MouseMove(FViewport* VP,int32 X,int32 Y)override{if(DragAxis>=0)Drag(VP,X,Y);else{Hover(VP,X,Y);FEditorViewportClient::MouseMove(VP,X,Y);}}
 virtual void CapturedMouseMove(FViewport* VP,int32 X,int32 Y)override{if(DragAxis>=0)Drag(VP,X,Y);else{HoverAxis=-1;FEditorViewportClient::CapturedMouseMove(VP,X,Y);}}
 virtual void MouseEnter(FViewport* VP,int32 X,int32 Y)override{FEditorViewportClient::MouseEnter(VP,X,Y);Hover(VP,X,Y);}
 virtual void MouseLeave(FViewport* VP)override{PointerInside=false;HoverAxis=-1;Invalidate();FEditorViewportClient::MouseLeave(VP);}
 virtual void LostFocus(FViewport* VP)override{PickingJoint=false;PointerInside=false;EndGizmo(VP);FEditorViewportClient::LostFocus(VP);}
 virtual void Tick(float DeltaSeconds)override{
  FEditorViewportClient::Tick(DeltaSeconds);
  if(DragAxis>=0&&(!PoseEditable||!PoseEditable()||Tool()!=DragTool||(SelectedPoseBone&&SelectedPoseBone()!=DragBone)))EndGizmo(Viewport);
  if(DragAxis<0&&PointerInside&&Viewport)Hover(Viewport,HoverPixel.X,HoverPixel.Y);
 }
 virtual void Draw(const FSceneView* V,FPrimitiveDrawInterface* PDI)override{
  FEditorViewportClient::Draw(V,PDI);if(DrawPose)DrawPose(PDI);if(!PoseEditable||!PoseEditable()||!PoseLocation||Tool()<0)return;
  const int Highlight=DragAxis>=0?DragAxis:HoverAxis;
  GizmoSegments([&](int A,const FVector& From,const FVector& To){
   const bool Lit=A==Highlight;const FLinearColor Base=A==0?FLinearColor(1,.08f,.08f):A==1?FLinearColor(.08f,1,.08f):A==2?FLinearColor(.1f,.35f,1):FLinearColor::White;
   const FLinearColor Color=Lit?(DragAxis>=0?FLinearColor(1,1,.65f):FLinearColor(1,.85f,.15f)):Base;
   PDI->SetHitProxy(new HCBMPoseAxis(A));PDI->DrawLine(From,To,Color,SDPG_Foreground,Lit?(DragAxis>=0?4.f:3.f):1.5f);PDI->SetHitProxy(nullptr);
  });
 }

 virtual void ProcessClick(FSceneView& V,HHitProxy* H,FKey Key,EInputEvent Event,uint32 X,uint32 Y)override{if(Key==EKeys::LeftMouseButton&&!Viewport->KeyState(EKeys::LeftAlt)&&!Viewport->KeyState(EKeys::RightAlt)){const FName B=ResolvePoseBone(V,X,Y,Viewport->KeyState(EKeys::LeftShift)||Viewport->KeyState(EKeys::RightShift),false);if(!B.IsNone()&&PickPose){PickPose(B);return;}}FEditorViewportClient::ProcessClick(V,H,Key,Event,X,Y);}

 virtual UE::Widget::EWidgetMode GetWidgetMode()const override{return UE::Widget::WM_None;}
 virtual FVector GetWidgetLocation()const override{return PoseLocation?PoseLocation():FVector::ZeroVector;}
 virtual FMatrix GetWidgetCoordSystem()const override{return FMatrix::Identity;}
 virtual bool InputWidgetDelta(FViewport* VP,EAxisList::Type Axis,FVector& D,FRotator& R,FVector& Scale)override{if(Tool()==1&&Axis!=EAxisList::None&&PoseEditable&&PoseEditable()&&RotatePose)return RotatePose(R);return FEditorViewportClient::InputWidgetDelta(VP,Axis,D,R,Scale);}
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
 TStrongObjectPtr<UDebugSkelMeshComponent> Mesh;
 UCubelessMocapSettings* PoseSettings=nullptr;double PoseTime=0;
 FCBMIKPose IKPose;
 bool IsIK()const{return PoseSettings&&PoseSettings->IKEnabled&&PoseSettings->IKSource==Current&&PoseSettings->IKMesh==PoseSettings->TargetMesh;}
 FName Selected()const{return PoseSettings?(IsIK()?PoseSettings->IKSelected:PoseSettings->SelectedPoseBone):NAME_None;}
 const FCBMIKHandle* Handle()const{return IKPose.Handles.FindByPredicate([this](const auto& H){return H.Name==Selected();});}
 bool HandleVisible(const FCBMIKHandle& H)const{const int F=PoseSettings->IKFilter;return F==3||(F==0&&!H.Finger)||(F==1&&H.Name.ToString().EndsWith(TEXT("_l"))&&(H.Finger||H.Name==TEXT("hand_l")))||(F==2&&H.Name.ToString().EndsWith(TEXT("_r"))&&(H.Finger||H.Name==TEXT("hand_r")));}
 const FCBMIKEdit* Edit()const{return PoseSettings->IKEdits.FindByPredicate([this](const auto& E){return E.Control==Selected()&&FMath::IsNearlyEqual(E.Start,PoseSettings->IKStart,1.e-6)&&FMath::IsNearlyEqual(E.End,PoseSettings->IKEnd,1.e-6);});}
 double EditWeight()const{FCBMIKEdit E;E.Start=PoseSettings->IKStart;E.End=PoseSettings->IKEnd;E.Ease=PoseSettings->IKEase;return CBMIKWeight(E,PoseTime,Current->GetPlayLength());}
 void Gesture(bool Begin){if(IsIK()){if(Begin)CBMIKBegin(PoseSettings);else CBMIKEnd(PoseSettings);}else if(Begin)UCubelessMocapLibrary::BeginRepresentativeEdit(PoseSettings);else UCubelessMocapLibrary::EndRepresentativeEdit(PoseSettings);}

 bool CanEditPose()const{if(IsIK())return !PoseSettings->IKPlaying&&!UCubelessMocapLibrary::IsBusy()&&Handle()&&EditWeight()>.001;return PoseSettings&&PoseSettings->ShowPoseSkeleton&&Current&&!UCubelessMocapLibrary::IsBusy()&&PoseSettings->RepresentativeInput==CBMRepresentativeInput(PoseSettings)&&PoseSettings->RepresentativeFrames.IsValidIndex(PoseSettings->SelectedRepresentative)&&FMath::Abs(PoseTime*PoseSettings->FrameRate-PoseSettings->RepresentativeFrames[PoseSettings->SelectedRepresentative])<1.1;}
 FRotator CurrentOffset()const{if(IsIK())return Edit()?Edit()->Rotation:FRotator::ZeroRotator;if(PoseSettings)for(const auto& E:PoseSettings->RepresentativeCorrections)if(E.Sample==PoseSettings->SelectedRepresentative&&E.Bone==PoseSettings->SelectedPoseBone)return E.Delta;return FRotator::ZeroRotator;}
 FVector CurrentVector(bool Scale)const{if(IsIK())return Scale?FVector::OneVector:Edit()?Edit()->Translation:FVector::ZeroVector;if(PoseSettings)for(const auto& E:PoseSettings->RepresentativeCorrections)if(E.Sample==PoseSettings->SelectedRepresentative&&E.Bone==PoseSettings->SelectedPoseBone)return Scale?E.Scale:E.Translation;return Scale?FVector::OneVector:FVector::ZeroVector;}
 bool SetTransform(FVector T,FRotator R,FVector S){if(IsIK()){FString M;return CanEditPose()&&UCubelessMocapLibrary::SetIKControl(PoseSettings,Selected(),T,R,M);}if(!CanEditPose())return false;FString Error;const bool OK=UCubelessMocapLibrary::SetRepresentativeTransform(PoseSettings,PoseSettings->SelectedRepresentative,PoseSettings->SelectedPoseBone,T,R,S,Error);Invalidate();return OK;}
 bool SetVectorComponent(int Axis,double V){auto T=CurrentVector(false),S=CurrentVector(true);auto R=CurrentOffset();if(PoseSettings->PoseGizmoMode==0)T[Axis]=V;else if(PoseSettings->PoseGizmoMode==2)S[Axis]=V;else if(Axis==0)R.Roll=V;else if(Axis==1)R.Pitch=V;else R.Yaw=V;return SetTransform(T,R,S);}
 bool SetOffset(FRotator R){if(IsIK())return SetTransform(CurrentVector(false),R,FVector::OneVector);if(!CanEditPose())return false;FString Error;const bool OK=UCubelessMocapLibrary::SetRepresentativeCorrection(PoseSettings,PoseSettings->SelectedRepresentative,PoseSettings->SelectedPoseBone,R,Error);Invalidate();return OK;}
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
  // Component/world bone positions; video projection is performed with the
  // immutable recorded camera, never this viewport's orbit/pan/FOV.
  TArray<FVector3f> Points;
  for(const TCHAR* Name:{TEXT("pelvis"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")}){
   if(Mesh->GetBoneIndex(Name)==INDEX_NONE)return {};Points.Add(FVector3f(Mesh->GetBoneLocation(Name)));
  }return Points;
 }
 FBox FrameBounds{ForceInit};
 FString CalibrationSignature;double NextCalibrationCheck=0;
 void ReadCalibration(UAnimSequence* Anim,UAnimSequence* Evidence=nullptr){
  const FString Run=CBMDebugRun(Anim,Evidence);
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
  Mesh.Reset(NewObject<UDebugSkelMeshComponent>());Scene->AddComponent(Mesh.Get(),FTransform::Identity);
  SEditorViewport::Construct(SEditorViewport::FArguments());
 }
 virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override {
  auto C=MakeShared<FCBMPreviewViewportClient>(Scene.Get(),SharedThis(this));Camera=C;
  C->PoseEditable=[this](){return CanEditPose();};C->PoseLocation=[this](){return IsIK()?(Handle()?Handle()->Position:FVector::ZeroVector):PoseSettings?Mesh->GetBoneLocation(PoseSettings->SelectedPoseBone):FVector::ZeroVector;};
  C->SelectedPoseBone=[this](){return Selected();};
  C->PosePoints=[this](){TArray<FCBMPosePickPoint> Out;if(IsIK()){for(const auto& H:IKPose.Handles)if(HandleVisible(H)){FCBMPosePickPoint P;P.Bone=H.Name;P.Position=H.Position;P.Radius=H.Finger?1.2:4.;Out.Add(P);}return Out;}if(!PoseSettings||!PoseSettings->ShowPoseSkeleton||!Mesh->GetSkeletalMeshAsset()||UCubelessMocapLibrary::IsBusy())return Out;const auto Bones=CBMRepresentativeBones();const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();for(FName B:Bones){const int I=Ref.FindBoneIndex(B);if(I<0)continue;FCBMPosePickPoint P;P.Bone=B;P.Position=Mesh->GetBoneLocation(B);int Parent=Ref.GetParentIndex(I);while(Parent>=0&&!Bones.Contains(Ref.GetBoneName(Parent)))Parent=Ref.GetParentIndex(Parent);if(Parent>=0){P.Parent=Ref.GetBoneName(Parent);P.ParentPosition=Mesh->GetBoneLocation(P.Parent);}Out.Add(P);}return Out;};
  C->PickPose=[this](FName Bone){if(PoseSettings){if(IsIK())PoseSettings->IKSelected=Bone;else PoseSettings->SelectedPoseBone=Bone;}Invalidate();};
  C->PoseMode=[this](){return PoseSettings?PoseSettings->PoseGizmoMode:1;};C->PoseBasis=[this](){return IsIK()?(Handle()?Handle()->Rotation:FQuat::Identity):PoseSettings?Mesh->GetBoneQuaternion(PoseSettings->SelectedPoseBone):FQuat::Identity;};
  C->TranslatePose=[this](FVector Delta){if(IsIK())return SetTransform(CurrentVector(false)+Delta/FMath::Max(.001,EditWeight()),CurrentOffset(),FVector::OneVector);if(!CanEditPose())return false;const int I=Mesh->GetBoneIndex(PoseSettings->SelectedPoseBone),P=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetParentIndex(I);const FTransform Parent=P>=0?Mesh->GetBoneTransform(P):Mesh->GetComponentTransform();return SetTransform(CurrentVector(false)+Parent.InverseTransformVector(Delta),CurrentOffset(),CurrentVector(true));};
  C->ScalePose=[this](int Axis,double Factor){if(IsIK())return false;auto S=CurrentVector(true);if(Axis==3)S*=Factor;else S[Axis]*=Factor;return SetTransform(CurrentVector(false),CurrentOffset(),S);};
  C->PoseGesture=[this](bool Begin){Gesture(Begin);};
  C->RotatePose=[this](FRotator R){if(IsIK()){const FQuat Step=FQuat::Slerp(FQuat::Identity,R.Quaternion(),1./FMath::Max(.001,EditWeight()));return SetOffset((Step*CurrentOffset().Quaternion()).Rotator());}const FQuat World=Mesh->GetBoneQuaternion(PoseSettings->SelectedPoseBone);return SetOffset((CurrentOffset().Quaternion()*World.Inverse()*R.Quaternion()*World).Rotator());};
  C->DrawPose=[this](FPrimitiveDrawInterface* PDI){
   if(IsIK()){
    const FQuat V=Camera->GetViewRotation().Quaternion();const FVector Right=V.RotateVector(FVector::RightVector),Up=V.RotateVector(FVector::UpVector);
    for(const auto& H:IKPose.Handles)if(HandleVisible(H)){const double Radius=H.Finger?1.2:4.;const FLinearColor Color=H.Name==Selected()?FLinearColor::Yellow:H.Name.ToString().EndsWith(TEXT("_l"))?FLinearColor(.05f,.85f,1):FLinearColor(1,.4f,.08f);PDI->SetHitProxy(new HCBMPoseBone(H.Name));
     for(int K=0;K<24;++K){const double A=2*PI*K/24.,B=2*PI*(K+1)/24.;PDI->DrawLine(H.Position+Radius*(Right*FMath::Cos(A)+Up*FMath::Sin(A)),H.Position+Radius*(Right*FMath::Cos(B)+Up*FMath::Sin(B)),Color,SDPG_Foreground,PDI->IsHitTesting()?14.f:2.f,0,true);}
     PDI->DrawLine(H.Position-Right*Radius,H.Position+Right*Radius,Color,SDPG_Foreground,1,0,true);PDI->DrawLine(H.Position-Up*Radius,H.Position+Up*Radius,Color,SDPG_Foreground,1,0,true);PDI->SetHitProxy(nullptr);
     const FString Name=H.Name.ToString();if(Name.StartsWith(TEXT("knee_"))||Name.StartsWith(TEXT("elbow_"))){const FName Bone(*(Name.Replace(TEXT("knee_"),TEXT("calf_")).Replace(TEXT("elbow_"),TEXT("lowerarm_"))));PDI->DrawLine(H.Position,Mesh->GetBoneLocation(Bone),Color,SDPG_Foreground,.7f,0,true);}
    }return;
   }
   if(!PoseSettings||!PoseSettings->ShowPoseSkeleton||!Mesh->GetSkeletalMeshAsset())return;
   const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();const FQuat ViewQ=Camera->GetViewRotation().Quaternion();const FVector Right=ViewQ.RotateVector(FVector::RightVector),Up=ViewQ.RotateVector(FVector::UpVector);
   // Baseline joint half-size 1cm -> 3cm; baseline one-pixel links -> three strokes.
   constexpr double JointRadius=3.;
   for(FName B:CBMRepresentativeBones()){const int I=Ref.FindBoneIndex(B);if(I<0)continue;PDI->SetHitProxy(new HCBMPoseBone(B));const FVector Pos=Mesh->GetBoneLocation(B);const auto Color=B==PoseSettings->SelectedPoseBone?FLinearColor::Yellow:FLinearColor(1,.18f,.1f);const int Parent=Ref.GetParentIndex(I);
    if(Parent>=0){const FVector End=Mesh->GetBoneLocation(Ref.GetBoneName(Parent)),Mid=(Pos+End)*.5;const FVector Side=FVector::CrossProduct(End-Pos,Camera->GetViewLocation()-Mid).GetSafeNormal();const double Pixel=2*(Camera->GetViewLocation()-Mid).Size()*FMath::Tan(FMath::DegreesToRadians(Camera->ViewFOV*.5))/FMath::Max(1,Camera->Viewport?Camera->Viewport->GetSizeXY().X:1);PDI->SetHitProxy(new HCBMPoseBone(Ref.GetBoneName(Parent)));const auto LinkColor=Ref.GetBoneName(Parent)==PoseSettings->SelectedPoseBone?FLinearColor::Yellow:FLinearColor(1,.18f,.1f);for(int Stroke=-1;Stroke<=1;++Stroke)PDI->DrawLine(Pos+Side*Pixel*Stroke,End+Side*Pixel*Stroke,LinkColor,SDPG_Foreground,PDI->IsHitTesting()?12.f:0.f,0.f,true);}
    PDI->SetHitProxy(new HCBMPoseBone(B));for(int K=0;K<16;++K){const double A=2*PI*K/16.,Z=2*PI*(K+1)/16.;PDI->DrawLine(Pos+JointRadius*(Right*FMath::Cos(A)+Up*FMath::Sin(A)),Pos+JointRadius*(Right*FMath::Cos(Z)+Up*FMath::Sin(Z)),Color,SDPG_Foreground,PDI->IsHitTesting()?18.f:0.f,0.f,true);}
    PDI->DrawLine(Pos-Right*JointRadius,Pos+Right*JointRadius,Color,SDPG_Foreground,PDI->IsHitTesting()?18.f:0.f,0.f,true);PDI->DrawLine(Pos-Up*JointRadius,Pos+Up*JointRadius,Color,SDPG_Foreground,PDI->IsHitTesting()?18.f:0.f,0.f,true);PDI->SetHitProxy(nullptr);
   }
  };
  C->SetViewLocation(FVector(0,230,95));C->SetViewRotation(FRotator(0,-90,0));C->ViewFOV=38;
  C->SetViewMode(VMI_Lit);C->SetRealtime(true);C->EngineShowFlags.SetSelectionOutline(false);
  C->EngineShowFlags.SetGrid(false);
  return C;
 }
 void Pose(USkeletalMesh* Target,UAnimSequence* Anim,double Time,UAnimSequence* Evidence=nullptr){
  ReadCalibration(Anim,Evidence);
  if(Mesh->GetSkeletalMeshAsset()!=Target){Mesh->SetSkeletalMeshAsset(Target);NeedsFit=true;}
  PoseTime=Time;
  if(Current!=Anim){if(!Current)NeedsFit=true;Current=Anim;Mesh->EnablePreview(true,Anim);}
  if(Mesh->PreviewInstance){Mesh->PreviewInstance->ResetModifiedBone();if(!IsIK()&&PoseSettings&&Anim&&PoseSettings->RepresentativeInput==CBMRepresentativeInput(PoseSettings)&&Anim!=PoseSettings->RepresentativeAppliedAnimation)for(FName B:CBMRepresentativeBones()){const FQuat Q=CBMRepresentativeDelta(PoseSettings,B,Time*PoseSettings->FrameRate);if(Q.AngularDistance(FQuat::Identity)>.000001){auto& N=Mesh->PreviewInstance->ModifyBone(B);N.Rotation=Q.Rotator();N.RotationMode=BMM_Additive;N.RotationSpace=BCS_BoneSpace;N.TranslationMode=BMM_Ignore;N.ScaleMode=BMM_Ignore;}}}
  if(!IsIK()&&Mesh->PreviewInstance&&PoseSettings&&Anim&&PoseSettings->RepresentativeInput==CBMRepresentativeInput(PoseSettings)&&Anim!=PoseSettings->RepresentativeAppliedAnimation)for(FName B:CBMRepresentativeBones()){const auto T=CBMRepresentativeVector(PoseSettings,B,Time*PoseSettings->FrameRate,false),S=CBMRepresentativeVector(PoseSettings,B,Time*PoseSettings->FrameRate,true);if(!T.IsNearlyZero(.000001)||!S.Equals(FVector::OneVector,.000001)){auto& N=Mesh->PreviewInstance->ModifyBone(B);N.Translation=T;N.TranslationMode=BMM_Additive;N.TranslationSpace=BCS_ParentBoneSpace;N.Scale=S;N.ScaleMode=BMM_Additive;N.ScaleSpace=BCS_ParentBoneSpace;}}
  IKPose=FCBMIKPose();if(IsIK()&&CBMEvaluateIK(PoseSettings,Time,IKPose)&&IKPose.Changed&&Mesh->PreviewInstance){const auto& Ref=Target->GetRefSkeleton();for(int B=0;B<Ref.GetNum();++B){const FName Name=Ref.GetBoneName(B);auto& N=Mesh->PreviewInstance->ModifyBone(Name);N.Rotation=IKPose.Local[B].Rotator();N.RotationMode=BMM_Replace;N.RotationSpace=BCS_ParentBoneSpace;N.Translation=IKPose.Local[B].GetTranslation();N.TranslationMode=BMM_Replace;N.TranslationSpace=BCS_ParentBoneSpace;N.Scale=IKPose.Local[B].GetScale3D();N.ScaleMode=BMM_Replace;N.ScaleSpace=BCS_ParentBoneSpace;}}
  Mesh->SetVisibility(!(PoseSettings&&PoseSettings->ShowPoseSkeleton&&PoseSettings->HidePoseMesh));
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
 auto Angle=[this](const TCHAR* Label,int Axis){return SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(Label))]
  +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(75)[SNew(SSpinBox<double>).MinValue_Lambda([this](){return Settings->PoseGizmoMode==2?.1:Settings->PoseGizmoMode==0?-100.:-60.;}).MaxValue_Lambda([this](){return Settings->PoseGizmoMode==2?10.:Settings->PoseGizmoMode==0?100.:60.;}).Delta_Lambda([this](){return Settings->PoseGizmoMode==2?.01:1.;}).IsEnabled_Lambda([this](){return View&&View->CanEditPose()&&Settings->PoseGizmoMode>=0;}).Value_Lambda([this,Axis](){if(!View)return 0.;if(Settings->PoseGizmoMode==0||Settings->PoseGizmoMode==2)return View->CurrentVector(Settings->PoseGizmoMode==2)[Axis];const auto R=View->CurrentOffset();return Axis==0?R.Roll:Axis==1?R.Pitch:R.Yaw;}).OnBeginSliderMovement_Lambda([this](){UCubelessMocapLibrary::BeginRepresentativeEdit(Settings);}).OnEndSliderMovement_Lambda([this](double){UCubelessMocapLibrary::EndRepresentativeEdit(Settings);}).OnValueChanged_Lambda([this,Axis](double V){if(View)View->SetVectorComponent(Axis,V);})]];};
 auto DisplayMode=[this](){return SNew(SComboButton).ToolTipText(FText::FromString(TEXT("3D 뷰 표시: 본만 / 본과 모델 / 모델만"))).IsEnabled_Lambda([](){return !UCubelessMocapLibrary::IsBusy();}).ButtonContent()[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(!Settings->ShowPoseSkeleton?TEXT("모델만 보기"):Settings->HidePoseMesh?TEXT("본만 보기"):TEXT("본 보기"));})].OnGetMenuContent_Lambda([this](){FMenuBuilder M(true,nullptr);const TCHAR* Names[]={TEXT("본만 보기"),TEXT("본 보기"),TEXT("모델만 보기")};for(int I=0;I<3;++I)M.AddMenuEntry(FText::FromString(Names[I]),FText::GetEmpty(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this,I](){FString Text;Control(TEXT("pose_display"),I,Text);}),FCanExecuteAction(),FIsActionChecked::CreateLambda([this,I](){return I==(!Settings->ShowPoseSkeleton?2:Settings->HidePoseMesh?0:1);})),NAME_None,EUserInterfaceActionType::RadioButton);return M.MakeWidget();});};
 Player.Reset(NewObject<UMediaPlayer>());Player->PlayOnOpen=false;Player->SetLooping(false);
 Texture.Reset(NewObject<UMediaTexture>());Texture->SetMediaPlayer(Player.Get());Texture->UpdateResource();
 Brush.SetResourceObject(Texture.Get());Brush.ImageSize=FVector2D(490,750);Brush.DrawAs=ESlateBrushDrawType::Image;
 ChildSlot[SNew(SVerticalBox)
  +SVerticalBox::Slot().FillHeight(1)[SAssignNew(VideoSplitter,SSplitter)
   +SSplitter::Slot().Value(.5f)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(TEXT("원본 영상")))]
     +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){return FText::FromString(Settings->DebugBones?TEXT("본 디버그 끄기"):TEXT("본 디버그 켜기"));}).ToolTipText(FText::FromString(TEXT("초록은 영상 검출, 자홍은 모델 추정 카메라의 원시 본, 청록은 결과 본을 원본 영상의 고정 카메라로 투영한 선입니다. 프리뷰 카메라 이동·회전·확대와 독립적입니다. 캐시만 사용하며 모캡 추론이나 에셋을 변경하지 않습니다."))).OnClicked_Lambda([this](){Settings->DebugBones=!Settings->DebugBones;return FReply::Handled();})]]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Visibility_Lambda([this](){return Settings->DebugBones?EVisibility::Visible:EVisibility::Collapsed;}).Text_Lambda([this](){return FText::FromString(DebugOverlay?DebugOverlay->Status:FString());})]
    +SVerticalBox::Slot().FillHeight(1)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride_Lambda([this](){return 750*Aspect;}).HeightOverride(750)[SNew(SOverlay)+SOverlay::Slot()[SNew(SImage).Image(&Brush)]+SOverlay::Slot()[SAssignNew(DebugOverlay,SCBMDebugOverlay).Visibility(EVisibility::HitTestInvisible).Clipping(EWidgetClipping::ClipToBounds)]]]]]
   +SSplitter::Slot().Value(.5f)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([this](){if(ReviewSource)return FText::FromString(TEXT("언리얼 애니메이션 - 원본 구간 선택 중"));if(!Settings->PreviewAnimation)return FText::FromString(TEXT("언리얼 애니메이션 - 계산 전"));FString Label=TEXT("선택된 결과");
     if(Settings->PreviewAnimation==Settings->CorrectedHandAnimation&&Settings->PreviewAnimation)Label=TEXT("HandFlow + 손가락 보정");
     else if(Settings->PreviewAnimation==Settings->CorrectedBodyAnimation&&Settings->PreviewAnimation)Label=TEXT("전신 + 손가락 보정");
     else if(Settings->PreviewAnimation==Settings->HandFlowAnimation&&Settings->PreviewAnimation)Label=TEXT("HandFlow");
     else if(Settings->PreviewAnimation==Settings->OriginalAnimation&&Settings->PreviewAnimation)Label=TEXT("전신 원본");
     return FText::FromString(TEXT("언리얼 애니메이션 - ")+Label);}).ToolTipText_Lambda([this](){return FText::FromString(Settings->PreviewAnimation?Settings->PreviewAnimation->GetPathName():TEXT("No animation selected"));})]+SHorizontalBox::Slot().AutoWidth()[DisplayMode()]]
    +SVerticalBox::Slot().FillHeight(1)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride_Lambda([this](){return 750*Aspect;}).HeightOverride(750)[SAssignNew(View,SCBMAnimationViewport)]]]]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){return FText::FromString(CBMIKActive(Settings)?TEXT("IK 편집 중 · 현재 자세 유지"):TEXT("현재 자세로 IK 편집"));}).IsEnabled_Lambda([this](){return Settings->PreviewAnimation&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){FString M;Control(TEXT("ik_begin"),0,M);Notice=M;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SComboButton).ButtonContent()[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(TEXT("핸들: ")+Settings->IKSelected.ToString());})].OnGetMenuContent_Lambda([this](){FMenuBuilder Menu(true,nullptr);if(View)for(const auto& H:View->IKPose.Handles)Menu.AddMenuEntry(FText::FromName(H.Name),FText::GetEmpty(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this,Name=H.Name](){Settings->IKSelected=Name;Settings->IKFilter=Name.ToString().Contains(TEXT("_tip_"))?(Name.ToString().EndsWith(TEXT("_l"))?1:2):0;Settings->PreviewHand=Settings->IKFilter;})));return Menu.MakeWidget();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SComboButton).ButtonContent()[SNew(STextBlock).Text_Lambda([this](){const TCHAR* Names[]={TEXT("몸 핸들"),TEXT("왼손 확대"),TEXT("오른손 확대"),TEXT("전체 핸들")};return FText::FromString(Names[FMath::Clamp(Settings->IKFilter,0,3)]);})].OnGetMenuContent_Lambda([this](){FMenuBuilder Menu(true,nullptr);const TCHAR* Names[]={TEXT("몸 핸들"),TEXT("왼손 확대"),TEXT("오른손 확대"),TEXT("전체 핸들")};for(int I=0;I<4;++I)Menu.AddMenuEntry(FText::FromString(Names[I]),FText::GetEmpty(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this,I](){FString M;Control(TEXT("ik_filter"),I,M);})));return Menu.MakeWidget();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString(TEXT("이동 W"))).OnClicked_Lambda([this](){Settings->PoseGizmoMode=0;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("회전 E"))).OnClicked_Lambda([this](){Settings->PoseGizmoMode=1;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString(TEXT("새 애니메이션 저장"))).IsEnabled_Lambda([this](){return Settings->IKEnabled&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){FString M;const FString P=Settings->OutputFolder/TEXT("AN_Quinn_IK_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);UCubelessMocapLibrary::SaveIKAnimation(Settings,P,M);Notice=M;return FReply::Handled();})]
  ]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox).Visibility_Lambda([this](){return Settings->IKEnabled?EVisibility::Visible:EVisibility::Collapsed;})
   +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("보정 구간  시작 ")))]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(65)[SNew(SSpinBox<double>).MinValue(0.).MaxValue_Lambda([this](){return Settings->IKEnd-.001;}).Delta(.033333).Value_Lambda([this](){return Settings->IKStart;}).OnBeginSliderMovement_Lambda([this](){CBMIKBegin(Settings);}).OnEndSliderMovement_Lambda([this](double){CBMIKEnd(Settings);}).OnValueChanged_Lambda([this](double V){FString M;UCubelessMocapLibrary::SetIKRange(Settings,V,Settings->IKEnd,FMath::Min(Settings->IKEase,(Settings->IKEnd-V)*.5),M);Notice=M;})]]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("현재/마커→시작"))).OnClicked_Lambda([this](){FString M;Control(TEXT("ik_range_start"),DisplayTime(),M);Notice=M;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("  끝 ")))]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(65)[SNew(SSpinBox<double>).MinValue_Lambda([this](){return Settings->IKStart+.001;}).MaxValue_Lambda([this](){return Settings->IKSource?double(Settings->IKSource->GetPlayLength()):0.;}).Delta(.033333).Value_Lambda([this](){return Settings->IKEnd;}).OnBeginSliderMovement_Lambda([this](){CBMIKBegin(Settings);}).OnEndSliderMovement_Lambda([this](double){CBMIKEnd(Settings);}).OnValueChanged_Lambda([this](double V){FString M;UCubelessMocapLibrary::SetIKRange(Settings,Settings->IKStart,V,FMath::Min(Settings->IKEase,(V-Settings->IKStart)*.5),M);Notice=M;})]]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("현재/마커→끝"))).OnClicked_Lambda([this](){FString M;Control(TEXT("ik_range_end"),DisplayTime(),M);Notice=M;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("  연결(초) ")))]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(65)[SNew(SSpinBox<double>).MinValue(.001).MaxValue_Lambda([this](){return (Settings->IKEnd-Settings->IKStart)*.5;}).Delta(.05).Value_Lambda([this](){return Settings->IKEase;}).OnBeginSliderMovement_Lambda([this](){CBMIKBegin(Settings);}).OnEndSliderMovement_Lambda([this](double){CBMIKEnd(Settings);}).OnValueChanged_Lambda([this](double V){FString M;Control(TEXT("ik_ease"),V,M);Notice=M;})]]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Ctrl+Z 되돌리기"))).OnClicked_Lambda([this](){FString M;Control(TEXT("undo_pose"),0,M);Notice=M;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Ctrl+Y 다시 실행"))).OnClicked_Lambda([this](){FString M;Control(TEXT("redo_pose"),0,M);Notice=M;return FReply::Handled();})]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("구간 안에서만 부드럽게 보정 · 발/손 이동, 무릎/팔꿈치 방향 · 손 확대 후 손끝 선택")))]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox).Visibility_Lambda([this](){return Settings->IKEnabled||Settings->RepresentativeFrames.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;})
   +SHorizontalBox::Slot().AutoWidth()[SNew(SComboButton).ButtonContent()[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("기준 자세 %d / %d"),Settings->SelectedRepresentative+1,Settings->RepresentativeFrames.Num()));})].OnGetMenuContent_Lambda([this](){FMenuBuilder M(true,nullptr);for(int I=0;I<Settings->RepresentativeFrames.Num();++I){M.AddMenuEntry(FText::FromString(FString::Printf(TEXT("%s 자세 %d : %.3fs"),TEXT("수동"),I+1,double(Settings->RepresentativeFrames[I])/Settings->FrameRate)),FText::GetEmpty(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this,I](){FString E;Control(TEXT("representative"),I,E);})));}return M.MakeWidget();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SComboButton).ButtonContent()[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(TEXT("선택 본: ")+Settings->SelectedPoseBone.ToString());}).Font(FAppStyle::GetFontStyle("BoldFont")).ColorAndOpacity(FLinearColor::Yellow)].OnGetMenuContent_Lambda([this](){FMenuBuilder M(true,nullptr);for(FName B:CBMRepresentativeBones())M.AddMenuEntry(FText::FromName(B),FText::GetEmpty(),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([this,B](){Settings->SelectedPoseBone=B;})));return M.MakeWidget();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("선택 Q"))).OnClicked_Lambda([this](){FString M;Control(TEXT("pose_tool"),-1,M);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("이동 W"))).ButtonColorAndOpacity_Lambda([this](){return Settings->PoseGizmoMode==0?FLinearColor(.25f,.5f,1.f):FLinearColor::White;}).OnClicked_Lambda([this](){FString M;Control(TEXT("pose_tool"),0,M);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("회전 E"))).ButtonColorAndOpacity_Lambda([this](){return Settings->PoseGizmoMode==1?FLinearColor(.25f,.5f,1.f):FLinearColor::White;}).OnClicked_Lambda([this](){FString M;Control(TEXT("pose_tool"),1,M);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("스케일 R"))).ButtonColorAndOpacity_Lambda([this](){return Settings->PoseGizmoMode==2?FLinearColor(.25f,.5f,1.f):FLinearColor::White;}).OnClicked_Lambda([this](){FString M;Control(TEXT("pose_tool"),2,M);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[Angle(TEXT("X "),0)]+SHorizontalBox::Slot().AutoWidth()[Angle(TEXT("Y "),1)]+SHorizontalBox::Slot().AutoWidth()[Angle(TEXT("Z "),2)]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SButton).Text(FText::FromString(TEXT("이 본 보정 초기화"))).OnClicked_Lambda([this](){View->SetTransform(FVector::ZeroVector,FRotator::ZeroRotator,FVector::OneVector);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("되돌리기 Ctrl+Z"))).OnClicked_Lambda([this](){FString E;Control(TEXT("undo_pose"),0,E);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("다시 실행 Ctrl+Y"))).OnClicked_Lambda([this](){FString E;Control(TEXT("redo_pose"),0,E);return FReply::Handled();})]
   +SHorizontalBox::Slot().FillWidth(1).Padding(8,0)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return FText::FromString(TEXT("관절점 클릭으로 본 선택 · Shift+클릭: 겹친 본 순환 · 선택한 마커에서 회전 보정"));})]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text_Lambda([this](){return FText::FromString(Player->IsPlaying()||PlayAfterSeek?TEXT("일시 정지"):TEXT("재생"));}).IsEnabled_Lambda([this](){return Player->IsReady()&&!Waiting&&!InferenceRenderingSuspended&&!UCubelessMocapLibrary::IsBusy()&&OpenedFile==Settings->Video.FilePath;}).OnClicked_Lambda([this](){FString M;Control(Player->IsPlaying()||PlayAfterSeek?TEXT("pause"):TEXT("play"),0,M);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SButton).Text(FText::FromString(TEXT("정지"))).IsEnabled_Lambda([this](){return Player->IsReady()&&!Waiting&&!UCubelessMocapLibrary::IsBusy()&&OpenedFile==Settings->Video.FilePath;}).OnClicked_Lambda([this](){FString M;Control(TEXT("stop"),0,M);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("◀ 1f"))).ToolTipText(FText::FromString(TEXT("원본 영상 FPS 기준 이전 프레임으로 탐색합니다. 디코더 seek 후 표시됩니다."))).IsEnabled_Lambda([this](){return Player->IsReady()&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){StepFrame(-1);return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("1f ▶"))).ToolTipText(FText::FromString(TEXT("원본 영상 FPS 기준 다음 프레임으로 탐색합니다."))).IsEnabled_Lambda([this](){return Player->IsReady()&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){StepFrame(1);return FReply::Handled();})]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SSpacer)]
   +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("재생 속도 ")))]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(64)[SNew(SSpinBox<float>).MinValue(.1f).MaxValue(4.f).Delta(.05f).ToolTipText(FText::FromString(TEXT("미리보기 영상/애니메이션의 재생 속도입니다. 내보내기 RateScale과 별개입니다."))).Value_Lambda([this](){return Settings->PreviewPlayRate;}).OnValueChanged_Lambda([this](float V){FString M;Control(TEXT("rate"),V,M);})]]]
  +SVerticalBox::Slot().AutoHeight().Padding(4,0)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("현재 위치 마킹"))).ToolTipText(FText::FromString(TEXT("현재 분석 프레임에 수동 보정 키를 추가합니다. 기존 보정 곡선을 유지하며 재추론하지 않습니다."))).IsEnabled_Lambda([this](){return Player->IsReady()&&!Waiting&&!Clock.Pending&&!UCubelessMocapLibrary::IsBusy();}).OnClicked_Lambda([this](){FString M;Control(TEXT("mark"),0,M);Notice=M;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(4,0)[SNew(SButton).Text(FText::FromString(TEXT("수동 마킹 삭제"))).IsEnabled_Lambda([this](){const int F=FMath::RoundToInt((PlaybackStart+DisplayTime()-Settings->StartSeconds)*Settings->FrameRate);return Player->IsReady()&&!Clock.Pending&&!UCubelessMocapLibrary::IsBusy()&&Settings->RepresentativeInput==CBMRepresentativeInput(Settings)&&Settings->RepresentativeFrames.Contains(F);}).OnClicked_Lambda([this](){FString M;Control(TEXT("unmark"),0,M);Notice=M;return FReply::Handled();})]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).ColorAndOpacity(FLinearColor(1,.43f,.04f)).Text(FText::FromString(TEXT("□ M 수동")))]
   +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(TEXT("노랑: 선택 | 흰 점: 보정 있음 | 마커 클릭으로 이동")))]]
  +SVerticalBox::Slot().AutoHeight().Padding(4)[SNew(SCBMPlaybackSlider).ToolTipText(FText::FromString(TEXT("빨간 핸들: 영상과 결과의 재생 시각. 드래그 후 일시 정지합니다. 방향키는 1프레임, Home/End는 시작/끝으로 이동합니다."))).IsEnabled_Lambda([this](){return Player->IsReady()&&!Waiting&&Length>0&&!UCubelessMocapLibrary::IsBusy();}).Duration_Lambda([this](){return Length;}).Position_Lambda([this](){return Clock.Position;}).FrameDuration_Lambda([this](){return 1./SourceFPS();}).Markers_Lambda([this](){TArray<FCBMPlaybackMarker> Out;if(Settings->RepresentativeInput!=CBMRepresentativeInput(Settings))return Out;int M=0;for(int I=0;I<Settings->RepresentativeFrames.Num();++I){FCBMPlaybackMarker Marker;Marker.Index=I;Marker.Seconds=Settings->StartSeconds+double(Settings->RepresentativeFrames[I])/Settings->FrameRate-PlaybackStart;Marker.Selected=I==Settings->SelectedRepresentative;Marker.Edited=Settings->RepresentativeCorrections.ContainsByPredicate([I](const auto& C){return C.Sample==I;});Marker.Label=FString::Printf(TEXT("M%d"),++M);Out.Add(Marker);}return Out;}).OnMarkerSelected_Lambda([this](int I){FString M;Control(TEXT("representative"),I,M);}).OnCaptureBegin_Lambda([this](){Player->Pause();PlayAfterSeek=false;Clock.Scrubbing=true;}).OnCaptureEnd_Lambda([this](){Clock.Scrubbing=false;}).OnChanged_Lambda([this](double T){Seek(T);})]
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
 View->PoseSettings=Settings;
}
SCBMComparison::~SCBMComparison(){if(ThumbnailProcess.IsValid()){if(FPlatformProcess::IsProcRunning(ThumbnailProcess))FPlatformProcess::TerminateProc(ThumbnailProcess);FPlatformProcess::CloseProc(ThumbnailProcess);}if(InferenceRenderingSuspended&&View.IsValid()&&View->Camera.IsValid())View->Camera->RemoveRealtimeOverride(CBMPreviewRealtimeReason(),false);Player->Close();Brush.SetResourceObject(nullptr);}

double SCBMComparison::DisplayTime()const{return SourceFrame.IsValid()?FMath::Clamp(DisplayedSourceSeconds-PlaybackStart,0.,Length):Clock.Position;}
void SCBMComparison::UpdateSourceFrame(){
 // Use the identical decoded image that supplied the 2D observations. WMF may
 // report a successful paused seek without presenting any media-texture sample.
 const FString Run=CBMDebugRun(DisplayAnimation(),Settings->OriginalAnimation);
 const FString Identity=Run+TEXT("|")+OpenedFile+TEXT("|")+FString::SanitizeFloat(Settings->StartSeconds);
 if(Identity!=SourceCacheIdentity){SourceCacheIdentity=Identity;SourceCacheDirectory.Empty();SourceCacheFPS=0;SourceCacheFrames=0;SourceFrameKey.Empty();SourceFrame.Reset();FString Text,Video,Stamp;double Size=0,Start=0,FPS=0,Frames=0;TSharedPtr<FJsonObject> J;const FString Dir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/Run;
  if(!Run.IsEmpty()&&FFileHelper::LoadFileToString(Text,*(Dir/TEXT("DebugBones.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J)&&J&&J->TryGetStringField(TEXT("video"),Video)&&J->TryGetStringField(TEXT("video_stamp"),Stamp)&&J->TryGetNumberField(TEXT("video_size"),Size)&&J->TryGetNumberField(TEXT("start"),Start)&&J->TryGetNumberField(TEXT("fps"),FPS)&&J->TryGetNumberField(TEXT("frames"),Frames)&&FPS>0&&Frames>0&&FPaths::IsSamePath(Video,FPaths::ConvertRelativePathToFull(OpenedFile))&&IFileManager::Get().FileSize(*OpenedFile)==int64(Size)&&IFileManager::Get().GetTimeStamp(*OpenedFile).ToIso8601()==Stamp){SourceCacheDirectory=Dir/TEXT("Frames");SourceCacheStart=Start;SourceCacheFPS=FPS;SourceCacheFrames=int(Frames);}
 }
 const double Absolute=PlaybackStart+Clock.Position;FString File;double Sample=Absolute;
 const int Frame=SourceCacheFPS>0?FMath::RoundToInt((Absolute-SourceCacheStart)*SourceCacheFPS):-1;
 if(Frame>=0&&Frame<SourceCacheFrames){File=SourceCacheDirectory/FString::Printf(TEXT("%08d.png"),Frame);Sample=SourceCacheStart+Frame/SourceCacheFPS;if(!FPaths::FileExists(File))File.Empty();}
 if(ThumbnailProcess.IsValid()&&!FPlatformProcess::IsProcRunning(ThumbnailProcess)){FPlatformProcess::CloseProc(ThumbnailProcess);ThumbnailProcess.Reset();}
 if(File.IsEmpty()&&!Player->IsPlaying()&&!UCubelessMocapLibrary::IsBusy()&&FPaths::FileExists(OpenedFile)&&!OpenedFile.Contains(TEXT("\""))){
  const FString Key=OpenedFile+FString::Printf(TEXT("|%lld|%s|%.6f"),IFileManager::Get().FileSize(*OpenedFile),*IFileManager::Get().GetTimeStamp(*OpenedFile).ToIso8601(),Absolute);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap/PreviewFrames");const FString Desired=Dir/(FMD5::HashAnsiString(*Key)+TEXT(".png"));
  if(FPaths::FileExists(Desired)){File=Desired;Sample=Absolute;}
  else if(!ThumbnailProcess.IsValid()&&FPaths::FileExists(Settings->FFmpeg.FilePath)){IFileManager::Get().MakeDirectory(*Dir,true);ThumbnailFile=Desired;ThumbnailSeconds=Absolute;const FString Args=FString::Printf(TEXT("-nostdin -v error -ss %.6f -i \"%s\" -frames:v 1 -y \"%s\""),Absolute,*OpenedFile,*Desired);ThumbnailProcess=FPlatformProcess::CreateProc(*Settings->FFmpeg.FilePath,*Args,false,true,true,nullptr,0,nullptr,nullptr);}
 }
 if(!File.IsEmpty()&&File!=SourceFrameKey){TGuardValue<ITransaction*> NoPreviewTransaction(GUndo,nullptr);FImage Image;if(FImageUtils::LoadImage(*File,Image)){Image.ChangeFormat(ERawImageFormat::BGRA8,EGammaSpace::sRGB);if(!SourceFrame.IsValid()||SourceFrame->GetSizeX()!=Image.SizeX||SourceFrame->GetSizeY()!=Image.SizeY){SourceFrame.Reset(UTexture2D::CreateTransient(Image.SizeX,Image.SizeY,PF_B8G8R8A8));SourceFrame->SRGB=true;SourceFrame->NeverStream=true;SourceFrame->UpdateResource();}auto* Pixels=new uint8[Image.RawData.Num()];FMemory::Memcpy(Pixels,Image.RawData.GetData(),Image.RawData.Num());auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Image.SizeX,Image.SizeY);SourceFrame->UpdateTextureRegions(0,1,Region,Image.SizeX*4,4,Pixels,[](uint8* Data,const FUpdateTextureRegion2D* R){delete[] Data;delete R;});SourceFrameKey=File;DisplayedSourceSeconds=Sample;Aspect=float(Image.SizeX)/Image.SizeY;Brush.SetResourceObject(SourceFrame.Get());}}
 if(File.IsEmpty()){SourceFrame.Reset();SourceFrameKey.Empty();Brush.SetResourceObject(Texture.Get());DisplayedSourceSeconds=Player->GetTime().GetTotalSeconds();}
}
bool SCBMComparison::PickPoseBoneAt(FVector2D Pixel,bool Cycle,FString& Message){if(!View||Clock.Pending||UCubelessMocapLibrary::IsBusy()){Message=TEXT("pose_not_ready");return false;}const bool OK=StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera)->SelectPoseAt(Pixel.X,Pixel.Y,Cycle);Message=OK?View->Selected().ToString():TEXT("no_bone_at_pixel");return OK;}
bool SCBMComparison::PosePointer(const FString& Action,FVector2D Pixel,FString& Message){
 if(!View||!View->Camera->Viewport){Message=TEXT("viewport_unavailable");return false;}
 auto C=StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera);auto* VP=C->Viewport;
 // Cleanup must remain available while playback/inference changes editability.
 if(Action==TEXT("cancel")){C->PickingJoint=false;C->PointerInside=false;C->EndGizmo(VP);Message=TEXT("gesture_ended");return true;}
 if(Action==TEXT("leave")){C->PointerInside=false;C->HoverAxis=-1;C->Invalidate();Message=TEXT("hover_cleared");return true;}
 if(!FMath::IsFinite(Pixel.X)||!FMath::IsFinite(Pixel.Y)){Message=TEXT("finite_pixels_required");return false;}
 if(Action==TEXT("release")){C->Release(VP,Pixel.X,Pixel.Y);Message=TEXT("gesture_ended");return true;}
 if(Action==TEXT("hover")){C->Hover(VP,Pixel.X,Pixel.Y);Message=TEXT("hover_updated");return true;}
 if(Clock.Pending||UCubelessMocapLibrary::IsBusy()||!View->CanEditPose()){Message=TEXT("pose_not_editable");return false;}
 if(Action==TEXT("press")){if(C->DragAxis>=0||C->PickingJoint){Message=TEXT("gesture_already_active");return false;}if(Pixel.X<0||Pixel.Y<0||Pixel.X>=VP->GetSizeXY().X||Pixel.Y>=VP->GetSizeXY().Y){Message=TEXT("pixel_outside_viewport");return false;}const bool OK=C->Press(VP,Pixel.X,Pixel.Y);Message=OK?TEXT("pointer_pressed"):TEXT("no_handle_at_pixel");return OK;}
 if(Action==TEXT("drag")){if(C->DragAxis<0){Message=TEXT("no_active_gizmo");return false;}C->Drag(VP,Pixel.X,Pixel.Y);C->Invalidate();Message=TEXT("gizmo_dragged");return C->DragAxis>=0;}
 Message=TEXT("unknown_pointer_action");return false;
}
FString SCBMComparison::GetPoseGizmoState()const{
 auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Segments;const bool Visible=View&&View->Camera->Viewport&&View->CanEditPose()&&Settings->PoseGizmoMode>=0;
 O->SetBoolField(TEXT("visible"),Visible);O->SetNumberField(TEXT("hover_axis"),-1);O->SetNumberField(TEXT("active_axis"),-1);
 if(View&&View->Camera->Viewport){auto C=StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera);O->SetNumberField(TEXT("hover_axis"),Visible?C->HoverAxis:-1);O->SetNumberField(TEXT("active_axis"),C->DragAxis);O->SetNumberField(TEXT("width"),C->Viewport->GetSizeXY().X);O->SetNumberField(TEXT("height"),C->Viewport->GetSizeXY().Y);
  if(Visible){FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(C->Viewport,C->GetScene(),C->EngineShowFlags));const auto* V=C->CalcSceneView(&Family);if(V){FVector2D Center;if(V->WorldToPixel(C->PoseLocation(),Center)){O->SetNumberField(TEXT("center_x"),Center.X);O->SetNumberField(TEXT("center_y"),Center.Y);}C->GizmoSegments([&](int Axis,const FVector& A,const FVector& B){FVector2D P,Q;if(V->WorldToPixel(A,P)&&V->WorldToPixel(B,Q)){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("axis"),Axis);J->SetNumberField(TEXT("x1"),P.X);J->SetNumberField(TEXT("y1"),P.Y);J->SetNumberField(TEXT("x2"),Q.X);J->SetNumberField(TEXT("y2"),Q.Y);Segments.Add(MakeShared<FJsonValueObject>(J));}});}}
 }O->SetArrayField(TEXT("segments"),Segments);FString Text;auto Writer=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(O,Writer);return Text;
}
FReply SCBMComparison::PoseHistoryKey(const FGeometry&,const FKeyEvent& E){
 if(!E.IsControlDown()&&!E.IsAltDown()){auto Focus=FSlateApplication::Get().GetKeyboardFocusedWidget();const bool Text=Focus&&Focus->GetTypeAsString().Contains(TEXT("EditableText"));if(!Text){const int Tool=E.GetKey()==EKeys::Q?-1:E.GetKey()==EKeys::W?0:E.GetKey()==EKeys::E?1:E.GetKey()==EKeys::R?2:-2;if(Tool>=-1){FString M;Control(TEXT("pose_tool"),Tool,M);return FReply::Handled();}}}

 if(!E.IsControlDown()||E.IsAltDown()||(E.GetKey()!=EKeys::Z&&E.GetKey()!=EKeys::Y))return FReply::Unhandled();
 FString Message;Control(E.GetKey()==EKeys::Y||E.IsShiftDown()?TEXT("redo_pose"):TEXT("undo_pose"),0,Message);Notice=Message;return FReply::Handled();
}
void SCBMComparison::UpdateDebugProjection(double Time,bool Synchronized){
 ViewPoints=Settings->DebugBones&&!ReviewSource?View->ProjectVisibleBones():TArray<FVector3f>();
 DebugOverlay->Update(Settings->DebugBones&&!ReviewSource,Settings->PreviewAnimation,Settings->Video.FilePath,PlaybackStart,Time,Synchronized,ViewPoints,CBMDebugRun(Settings->PreviewAnimation,Settings->OriginalAnimation));
}
bool SCBMComparison::PrepareScreenshot(){
 if(!View||!View->Camera||!View->Camera->Viewport)return false;
 // Slate screenshot painting alone can reuse the previous viewport texture
 // when the editor is background-throttled. Evaluate and draw the requested
 // pose/camera explicitly before reading pixels for comparison evidence.
 UpdateSourceFrame();Settings->IKPlaying=Player->IsPlaying()||Clock.Pending;const double Time=DisplayTime();
 View->Pose(Settings->TargetMesh,DisplayAnimation(),Time,Settings->OriginalAnimation);
 View->Frame(Aspect,Settings->PreviewYawOffset,Settings->PreviewZoom,Settings->PreviewHand,Settings->MatchVideoCamera);
 UpdateDebugProjection(Time,!Clock.Pending&&!Clock.Scrubbing&&OpenedFile==Settings->Video.FilePath);
 View->Camera->Viewport->Draw();FlushRenderingCommands();
 auto C=StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera);auto O=MakeShared<FJsonObject>();
 O->SetBoolField(TEXT("review_source"),ReviewSource);O->SetNumberField(TEXT("playback_start"),PlaybackStart);O->SetNumberField(TEXT("selection_start"),Settings->StartSeconds);O->SetNumberField(TEXT("selection_end_exclusive"),Settings->StartSeconds+Settings->DurationSeconds);O->SetBoolField(TEXT("media_playing"),Player->IsPlaying());O->SetNumberField(TEXT("source_fps"),SourceFPS());O->SetBoolField(TEXT("inference_rendering_suspended"),InferenceRenderingSuspended);O->SetBoolField(TEXT("preview_realtime"),View->Camera->IsRealtime());O->SetNumberField(TEXT("preview_seconds"),Time);O->SetNumberField(TEXT("evaluated_animation_seconds"),View->Mesh->GetSingleNodeInstance()?View->Mesh->GetSingleNodeInstance()->GetCurrentTime():-1.);
 O->SetNumberField(TEXT("media_seconds"),Player->GetTime().GetTotalSeconds());O->SetBoolField(TEXT("scrubbing"),Clock.Scrubbing);O->SetBoolField(TEXT("seek_pending"),Clock.Pending);
 O->SetNumberField(TEXT("displayed_source_seconds"),DisplayedSourceSeconds);O->SetStringField(TEXT("source_display_mode"),SourceFrame.IsValid()?TEXT("decoded_frame"):TEXT("media_texture"));O->SetStringField(TEXT("displayed_frame_file"),SourceFrameKey);O->SetBoolField(TEXT("gizmo_visible"),View->CanEditPose()&&Settings->PoseGizmoMode>=0);O->SetNumberField(TEXT("pose_tool"),Settings->PoseGizmoMode);
 {FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(C->Viewport,C->GetScene(),C->EngineShowFlags));auto* SV=C->CalcSceneView(&Family);TArray<TSharedPtr<FJsonValue>> Points;if(SV&&C->PosePoints)for(const auto& B:C->PosePoints()){FVector2D P;const bool Front=SV->WorldToPixel(B.Position,P);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("bone"),B.Bone.ToString());J->SetNumberField(TEXT("x"),P.X);J->SetNumberField(TEXT("y"),P.Y);J->SetNumberField(TEXT("depth"),(B.Position-C->GetViewLocation()).Size());J->SetBoolField(TEXT("in_front"),Front);Points.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("pickable_bones"),Points);O->SetNumberField(TEXT("viewport_width"),C->Viewport->GetSizeXY().X);O->SetNumberField(TEXT("viewport_height"),C->Viewport->GetSizeXY().Y);}
 {const auto T=View->Mesh->GetBoneTransform(Settings->SelectedPoseBone);const auto P=T.GetLocation(),Z=T.GetScale3D();O->SetArrayField(TEXT("pose_selected_world_position"),{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)});O->SetArrayField(TEXT("pose_selected_world_scale"),{MakeShared<FJsonValueNumber>(Z.X),MakeShared<FJsonValueNumber>(Z.Y),MakeShared<FJsonValueNumber>(Z.Z)});}
 O->SetBoolField(TEXT("rendered_mesh_visible"),View->Mesh->IsVisible());O->SetBoolField(TEXT("rendered_bones_visible"),Settings->ShowPoseSkeleton);
 O->SetNumberField(TEXT("bone_display_scale"),3.);O->SetNumberField(TEXT("bone_pick_scale"),3.);O->SetBoolField(TEXT("pose_editable"),View->CanEditPose());O->SetStringField(TEXT("pose_selected_bone"),Settings->SelectedPoseBone.ToString());{const auto Q=View->Mesh->GetBoneQuaternion(Settings->SelectedPoseBone);O->SetArrayField(TEXT("pose_selected_world_quat"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});}
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
 Player->Close();OpenedFile=Settings->Video.FilePath;Clock.Reset();Length=0;PlayAfterSeek=false;RecoveringSeek=false;SeekRetries=0;OpenBegan=FPlatformTime::Seconds();
 View->NeedsFit=true;
 Waiting=Player->OpenFile(OpenedFile);Notice=Waiting?TEXT("Opening video..."):TEXT("Cannot open video");
 return FReply::Handled();
}
bool SCBMComparison::Seek(double Time){
 if(!Player->IsReady())return false;
 const double F=SourceFPS();Time=FMath::RoundToDouble((PlaybackStart+Time)*F)/F-PlaybackStart;
 Clock.Request(Time,Length);SeekTarget=PlaybackStart+Clock.Position;SeekBegan=FPlatformTime::Seconds();SeekRetries=0;SeekFailed=false;SeekExact=false;SeekError=0;
 // WMF can accept Seek after end-of-stream without moving its clock. Reopen
 // the same media source while retaining the selected animation/range/intent.
 const double Actual=Player->GetTime().GetTotalSeconds();
 if(Actual>=SourceDuration()-2./F&&FMath::Abs(Actual-SeekTarget)>1./F)return ReopenForSeek();
 Player->Pause();
 if(!Player->Seek(FTimespan::FromSeconds(SeekTarget))){Clock.Pending=false;SeekFailed=true;Notice=TEXT("Video seek unavailable");}
 View->Pose(Settings->TargetMesh,DisplayAnimation(),Clock.Position,Settings->OriginalAnimation);
 return !SeekFailed;
}
bool SCBMComparison::ReopenForSeek(){
 Player->Close();RecoveringSeek=true;SeekRetries++;OpenBegan=FPlatformTime::Seconds();Waiting=Player->OpenFile(OpenedFile);
 if(!Waiting){RecoveringSeek=false;Clock.Pending=false;PlayAfterSeek=false;SeekFailed=true;}
 Notice=Waiting?TEXT("영상 디코더를 다시 열어 요청한 시각으로 이동합니다."):TEXT("영상 디코더를 다시 열지 못했습니다.");return Waiting;
}
bool SCBMComparison::ResumePlayback(){
 if(Player->Play()&&Player->SetRate(Settings->PreviewPlayRate))return true;
 Player->Pause();PlayAfterSeek=false;Notice=TEXT("영상 디코더가 재생 요청을 거절했습니다.");return false;
}
void SCBMComparison::Tick(const FGeometry& G,double T,float D){
 SCompoundWidget::Tick(G,T,D);
 if(Waiting&&(Player->HasError()||FPlatformTime::Seconds()-OpenBegan>8)){Waiting=false;RecoveringSeek=false;Clock.Pending=false;PlayAfterSeek=false;SeekFailed=true;Notice=TEXT("Video decoder error or open timeout");}
 if(Waiting&&Player->IsReady()){
  Waiting=false;const FIntPoint Size=Player->GetVideoTrackDimensions(INDEX_NONE,INDEX_NONE);
  if(Size.X>0&&Size.Y>0)Aspect=float(Size.X)/Size.Y;
  if(RecoveringSeek){RecoveringSeek=false;SeekBegan=FPlatformTime::Seconds();Player->Pause();if(!Player->Seek(FTimespan::FromSeconds(SeekTarget))){Clock.Pending=false;PlayAfterSeek=false;SeekFailed=true;}}
  else Seek(0);
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
   if(!FirstSample&&Elapsed>=1.5&&SeekRetries==0){ReopenForSeek();}
   else if(FirstSample||Elapsed>=3){Clock.Pending=false;Clock.Observe(Actual-PlaybackStart,Length,Frame*.5);SeekError=Actual-SeekTarget;SeekExact=false;SeekFailed=!FirstSample;if(SeekFailed)PlayAfterSeek=false;}
  }
  if(PlayAfterSeek&&!Clock.Pending&&!Clock.Scrubbing&&!Waiting){PlayAfterSeek=false;ResumePlayback();}
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
 UpdateSourceFrame();Settings->IKPlaying=Player->IsPlaying()||Clock.Pending;View->Pose(Settings->TargetMesh,DisplayAnimation(),DisplayTime(),Settings->OriginalAnimation);
 View->Frame(Aspect,Settings->PreviewYawOffset,Settings->PreviewZoom,Settings->PreviewHand,Settings->MatchVideoCamera);
 UpdateDebugProjection(DisplayTime(),Player->IsReady()&&!Clock.Pending&&!Clock.Scrubbing&&OpenedFile==Settings->Video.FilePath);
}

double SCBMComparison::SourceDuration()const{return Player->IsReady()?FMath::Max(0.,Player->GetDuration().GetTotalSeconds()):0.;}
double SCBMComparison::SourceFPS()const{const float F=Player->GetVideoTrackFrameRate(INDEX_NONE,INDEX_NONE);return F>0?F:double(FMath::Max(Settings->FrameRate,1));}
UAnimSequence* SCBMComparison::DisplayAnimation()const{return ReviewSource?nullptr:Settings->PreviewAnimation.Get();}
bool SCBMComparison::CanEditRange()const{return ReviewSource&&Settings->FrameDirectory.Path.IsEmpty()&&Player->IsReady()&&OpenedFile==Settings->Video.FilePath&&!UCubelessMocapLibrary::IsBusy()&&FCBMClipRange::Normalize(0,SourceDuration(),0,SourceDuration(),Settings->FrameRate).Y>0;}
void SCBMComparison::OpenSourceVideo(){if(OpenedFile!=Settings->Video.FilePath||!Player->IsReady())Load();Player->Pause();PlayAfterSeek=false;ReviewSource=true;PlaybackStart=0;Length=SourceDuration();Seek(0);}
bool SCBMComparison::ReviewVideo(){Player->Pause();PlayAfterSeek=false;const double Absolute=PlaybackStart+Clock.Position;ReviewSource=true;PlaybackStart=0;Length=SourceDuration();return Seek(Absolute);}
bool SCBMComparison::ShowResult(bool PreserveTime){if(!Player->IsReady())return false;const bool WasSource=ReviewSource;const double Absolute=PlaybackStart+Clock.Position;if(!Settings->OriginalAnimation&&!Settings->HandFlowAnimation){ResultStart=Settings->StartSeconds;ResultDuration=Settings->DurationSeconds;}Player->Pause();PlayAfterSeek=false;ReviewSource=false;PlaybackStart=ResultStart;Length=FMath::Min(ResultDuration,FMath::Max(0.,SourceDuration()-PlaybackStart));if(DisplayAnimation())Length=FMath::Min(Length,double(DisplayAnimation()->GetPlayLength()));if(PreserveTime&&!WasSource&&Clock.Position<=Length){View->Pose(Settings->TargetMesh,DisplayAnimation(),Clock.Position,Settings->OriginalAnimation);return true;}return Seek(PreserveTime?Absolute-PlaybackStart:0);}
bool SCBMComparison::StepFrame(int32 Direction){Player->Pause();PlayAfterSeek=false;const double F=SourceFPS();return Seek((FMath::RoundToDouble((PlaybackStart+Clock.Position)*F)+Direction)/F-PlaybackStart);}
void SCBMComparison::SetRange(double A,double B){
 if(!CanEditRange()||!FMath::IsFinite(A)||!FMath::IsFinite(B))return;
 const FVector2D Bounds=FCBMClipRange::Normalize(A,B,Settings->StartSeconds,SourceDuration(),Settings->FrameRate);
 Settings->StartSeconds=Bounds.X;Settings->DurationSeconds=Bounds.Y-Bounds.X;
}

bool SCBMComparison::Control(const FString& Action,double Value,FString& Message){
 if(!FMath::IsFinite(Value)){Message=TEXT("nonfinite_value");return false;}
 if(UCubelessMocapLibrary::IsBusy()&&Action!=TEXT("pause")){Message=TEXT("busy");return false;}
 if(Action==TEXT("ik_begin")){Player->Pause();PlayAfterSeek=false;if(Player->IsReady())ShowResult(true);const bool OK=UCubelessMocapLibrary::BeginIKEdit(Settings,Message);if(OK){ReviewSource=false;View->Pose(Settings->TargetMesh,Settings->PreviewAnimation,DisplayTime(),Settings->OriginalAnimation);}return OK;}
 if(Action==TEXT("ik_filter")){if(Value!=int(Value)||Value<0||Value>3){Message=TEXT("invalid_filter");return false;}Settings->IKFilter=int(Value);Settings->PreviewHand=Value==1||Value==2?int(Value):0;if(const auto* H=View->Handle();H&&!View->HandleVisible(*H))Settings->IKSelected=Value==1?TEXT("hand_l"):Value==2?TEXT("hand_r"):H->Name.ToString().EndsWith(TEXT("_l"))?TEXT("hand_l"):TEXT("hand_r");Message=TEXT("IK handle filter set");return true;}
 if(Action==TEXT("ik_ease"))return UCubelessMocapLibrary::SetIKRange(Settings,Settings->IKStart,Settings->IKEnd,Value,Message);
 if(Action==TEXT("ik_range_start")||Action==TEXT("ik_range_end")){const double A=Action==TEXT("ik_range_start")?Value:Settings->IKStart,B=Action==TEXT("ik_range_end")?Value:Settings->IKEnd;return UCubelessMocapLibrary::SetIKRange(Settings,A,B,FMath::Min(Settings->IKEase,(B-A)*.5),Message);}
 if(Settings->IKEnabled&&(Action==TEXT("pose_begin")||Action==TEXT("pose_end"))){if(Action==TEXT("pose_begin"))CBMIKBegin(Settings);else CBMIKEnd(Settings);return true;}
 if(Settings->IKEnabled&&(Action==TEXT("undo_pose")||Action==TEXT("redo_pose")))return CBMIKUndo(Settings,Action==TEXT("redo_pose"),Message);
 if(Action==TEXT("pose_tool")){if(Value!=FMath::FloorToDouble(Value)||Value< -1||Value>2){Message=TEXT("pose_tool_requires_minus1_select_0_move_1_rotate_2_scale");return false;}StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera)->EndGizmo(View->Camera->Viewport);if(Settings->IKEnabled&&Value==2){Message=TEXT("IK preserves bone lengths; use move or rotate");return false;}Settings->PoseGizmoMode=int(Value);Message=TEXT("pose_tool_set");return true;}
 if(Action.StartsWith(TEXT("pose_translate_"))){const int A=Action==TEXT("pose_translate_x")?0:Action==TEXT("pose_translate_y")?1:Action==TEXT("pose_translate_z")?2:-1;return A>=0&&StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera)->TranslateAxis(A,Value);}
 if(Action.StartsWith(TEXT("pose_scale_"))){const int A=Action==TEXT("pose_scale_x")?0:Action==TEXT("pose_scale_y")?1:Action==TEXT("pose_scale_z")?2:Action==TEXT("pose_scale_uniform")?3:-1;return A>=0&&StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera)->ScaleAxis(A,Value);}
 if(Action==TEXT("pose_display")){if(Value!=FMath::FloorToDouble(Value)||Value<0||Value>2){Message=TEXT("pose_display_requires_0_bones_1_both_2_mesh");return false;}Settings->ShowPoseSkeleton=Value!=2;Settings->HidePoseMesh=Value==0;Message=TEXT("pose_display_set");return true;}
 if(Action==TEXT("pose_begin"))return UCubelessMocapLibrary::BeginRepresentativeEdit(Settings);
 if(Action==TEXT("pose_end")){UCubelessMocapLibrary::EndRepresentativeEdit(Settings);Message=TEXT("pose_gesture_ended");return true;}
 if(Action==TEXT("undo_pose")||Action==TEXT("redo_pose")){const bool OK=UCubelessMocapLibrary::UndoRepresentativeEdit(Settings,Action==TEXT("redo_pose"),Message);if(OK&&Player->IsReady()){FString SeekMessage;Control(TEXT("representative"),Settings->SelectedRepresentative,SeekMessage);}return OK;}
 if(Action.StartsWith(TEXT("pose_rotate_"))){const int A=Action==TEXT("pose_rotate_x")?0:Action==TEXT("pose_rotate_y")?1:Action==TEXT("pose_rotate_z")?2:-1;const bool OK=A>=0&&FMath::Abs(Value)<=15&&StaticCastSharedPtr<FCBMPreviewViewportClient>(View->Camera)->RotateAxis(A,Value);Message=OK?TEXT("pose_rotated"):TEXT("pose_rotation_unavailable");return OK;}
 if(Action==TEXT("rate")){
  if(Value<.1||Value>4||(Player->IsReady()&&!Player->SupportsRate(Value,false))){Message=TEXT("unsupported_preview_rate");return false;}
  if(Player->IsPlaying()&&!Player->SetRate(Value)){Message=TEXT("playback_rate_rejected");return false;}Settings->PreviewPlayRate=Value;Message=TEXT("preview_rate_set");return true;
 }
 if(!Player->IsReady()||Waiting||OpenedFile!=Settings->Video.FilePath){Message=TEXT("media_not_ready_or_video_changed");return false;}
 bool Applied=true;
 if(Action==TEXT("mark")||Action==TEXT("unmark")){
  if(Clock.Pending||Clock.Scrubbing){Message=TEXT("wait_for_seek_before_marking");return false;}
  const int F=FMath::RoundToInt((PlaybackStart+DisplayTime()-Settings->StartSeconds)*Settings->FrameRate);Player->Pause();PlayAfterSeek=false;
  const bool OK=Action==TEXT("mark")?UCubelessMocapLibrary::AddRepresentativeMarker(Settings,F,Message):UCubelessMocapLibrary::RemoveRepresentativeMarker(Settings,F,Message);
  if(OK&&Settings->SelectedRepresentative>=0){FString SeekMessage;Control(TEXT("representative"),Settings->SelectedRepresentative,SeekMessage);}return OK;
 }
 if(Action==TEXT("representative")){
  const int I=int(Value);if(Value!=I||!Settings->RepresentativeFrames.IsValidIndex(I)||Settings->RepresentativeInput!=CBMRepresentativeInput(Settings)){Message=TEXT("invalid_representative");return false;}
  Player->Pause();PlayAfterSeek=false;ReviewSource=false;PlaybackStart=Settings->StartSeconds;ResultStart=PlaybackStart;ResultDuration=Settings->DurationSeconds;Length=FMath::Min(ResultDuration,FMath::Max(0.,SourceDuration()-PlaybackStart));if(DisplayAnimation())Length=FMath::Min(Length,double(DisplayAnimation()->GetPlayLength()));Settings->SelectedRepresentative=I;Applied=Seek(double(Settings->RepresentativeFrames[I])/Settings->FrameRate);
 }
 else if(Action==TEXT("source"))Applied=ReviewVideo();
 else if(Action==TEXT("result"))Applied=ShowResult();
 else if(Action==TEXT("pause")){Player->Pause();PlayAfterSeek=false;}
 else if(Action==TEXT("stop")){Player->Pause();PlayAfterSeek=false;Applied=Seek(0);}
 else if(Action==TEXT("play")){
  if(InferenceRenderingSuspended){Message=TEXT("inference_rendering_suspended");return false;}
  if(Length<=0){Message=TEXT("empty_playback_range");return false;}
  if((Clock.Position>=Length-.02||Player->GetTime().GetTotalSeconds()-PlaybackStart>=Length-.02)&&!Seek(0)){Message=TEXT("seek_rejected");return false;}
  if(Clock.Pending)PlayAfterSeek=true;
  else if(!ResumePlayback()){Message=TEXT("playback_rate_rejected");return false;}
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
 const double Time=DisplayTime();
 O->SetNumberField(TEXT("displayed_source_seconds"),DisplayedSourceSeconds);O->SetStringField(TEXT("source_display_mode"),SourceFrame.IsValid()?TEXT("decoded_frame"):TEXT("media_texture"));O->SetNumberField(TEXT("displayed_frame_rate"),SourceCacheFPS);
 O->SetBoolField(TEXT("media_ready"),Ready&&!Waiting&&OpenedFile==Settings->Video.FilePath);O->SetBoolField(TEXT("media_error"),Player->HasError());
 O->SetBoolField(TEXT("review_source"),ReviewSource);O->SetBoolField(TEXT("media_playing"),Player->IsPlaying());O->SetBoolField(TEXT("seek_pending"),Clock.Pending);O->SetBoolField(TEXT("scrubbing"),Clock.Scrubbing);
 O->SetBoolField(TEXT("seek_failed"),SeekFailed);O->SetBoolField(TEXT("seek_exact"),SeekExact);O->SetNumberField(TEXT("seek_error_seconds"),SeekError);O->SetNumberField(TEXT("seek_retry_count"),SeekRetries);O->SetBoolField(TEXT("play_after_seek"),PlayAfterSeek);O->SetNumberField(TEXT("media_rate"),Player->GetRate());
 O->SetNumberField(TEXT("source_duration"),SourceDuration());O->SetNumberField(TEXT("source_fps"),SourceFPS());O->SetNumberField(TEXT("playback_start"),PlaybackStart);O->SetNumberField(TEXT("playback_length"),Length);
 O->SetNumberField(TEXT("preview_seconds"),Time);O->SetNumberField(TEXT("media_seconds"),Player->GetTime().GetTotalSeconds());O->SetNumberField(TEXT("requested_source_seconds"),SeekTarget);O->SetNumberField(TEXT("result_start"),ResultStart);O->SetNumberField(TEXT("result_duration"),ResultDuration);
 O->SetNumberField(TEXT("source_frame"),FMath::RoundToInt64((PlaybackStart+Time)*SourceFPS()));
 O->SetNumberField(TEXT("evaluated_animation_seconds"),View->Mesh->GetSingleNodeInstance()?View->Mesh->GetSingleNodeInstance()->GetCurrentTime():-1.);
 O->SetNumberField(TEXT("selection_start"),Settings->StartSeconds);O->SetNumberField(TEXT("selection_end_exclusive"),Settings->StartSeconds+Settings->DurationSeconds);
 O->SetBoolField(TEXT("range_editable"),CanEditRange());O->SetBoolField(TEXT("inference_rendering_suspended"),InferenceRenderingSuspended);
 O->SetNumberField(TEXT("pose_tool"),Settings->PoseGizmoMode);
 O->SetStringField(TEXT("pose_display_mode"),!Settings->ShowPoseSkeleton?TEXT("mesh_only"):Settings->HidePoseMesh?TEXT("bones_only"):TEXT("bones_with_mesh"));
 TArray<TSharedPtr<FJsonValue>> ScreenHandles;if(View&&View->IsIK()&&View->Camera->Viewport){FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(View->Camera->Viewport,View->Camera->GetScene(),View->Camera->EngineShowFlags));const auto* V=View->Camera->CalcSceneView(&Family);if(V)for(const auto& H:View->IKPose.Handles)if(View->HandleVisible(H)){FVector2D Pixel;if(V->WorldToPixel(H.Position,Pixel)){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("control"),H.Name.ToString());J->SetNumberField(TEXT("x"),Pixel.X);J->SetNumberField(TEXT("y"),Pixel.Y);ScreenHandles.Add(MakeShared<FJsonValueObject>(J));}}}O->SetArrayField(TEXT("ik_screen_handles"),ScreenHandles);
 O->SetNumberField(TEXT("video_fraction"),VideoSplitter->SlotAt(0).GetSizeValue()/FMath::Max(.001f,VideoSplitter->SlotAt(0).GetSizeValue()+VideoSplitter->SlotAt(1).GetSizeValue()));O->SetStringField(TEXT("notice"),Notice);O->SetStringField(TEXT("ik_state"),UCubelessMocapLibrary::GetIKState(Settings,DisplayTime()));return O;
}
