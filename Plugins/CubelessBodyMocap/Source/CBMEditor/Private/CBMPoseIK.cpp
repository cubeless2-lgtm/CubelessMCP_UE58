#include "CBMPoseIK.h"
#include "CBMDebugProvenance.h"
#include "CBMRepresentative.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Rigs/FKControlRig.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

namespace {
bool IKSaving=false;
struct FSnapshot {
 TArray<FCBMIKEdit> Edits;
 TArray<int32> Markers;
 TArray<FCBMRepresentativeCorrection> BoneEdits;
 double Start=0,End=0,Ease=.2;
 FName Selected;
 int Marker=INDEX_NONE,Filter=0,Hand=0;
};
struct FHistory { TArray<FSnapshot> Undo,Redo; TOptional<FSnapshot> Gesture;double NextIdentityCheck=0;bool ValidIdentity=true; };
struct FSession { TStrongObjectPtr<UAnimSequence> Source;TStrongObjectPtr<USkeletalMesh> Mesh;FSnapshot State;FHistory History; };
struct FSourceSessions { FString Active;TMap<FString,FSession> Saved; };
TMap<TWeakObjectPtr<UCubelessMocapSettings>,FSourceSessions> SourceSessions;
FSnapshot Capture(UCubelessMocapSettings* S){return {S->IKEdits,S->RepresentativeFrames,S->RepresentativeCorrections,S->IKStart,S->IKEnd,S->IKEase,S->IKSelected,S->SelectedRepresentative,S->IKFilter,S->PreviewHand};}
void Restore(UCubelessMocapSettings* S,const FSnapshot& V){S->IKEdits=V.Edits;S->RepresentativeFrames=V.Markers;S->RepresentativeCorrections=V.BoneEdits;S->IKStart=V.Start;S->IKEnd=V.End;S->IKEase=V.Ease;S->IKSelected=V.Selected;S->SelectedRepresentative=V.Marker;S->IKFilter=V.Filter;S->PreviewHand=V.Hand;}

TMap<TWeakObjectPtr<UCubelessMocapSettings>,FHistory> Histories;
bool Ready(UCubelessMocapSettings* S) {
 if(!S||!S->IKEnabled||!S->IKSource||S->IKMesh!=S->TargetMesh||S->PreviewAnimation!=S->IKSource||!S->IKSource->GetDataModel()||S->IKSource->GetPackage()->IsDirty())return false;
 // Hash once per second while viewing; every mutation/export also validates immediately.
 auto& H=Histories.FindOrAdd(S);const double Now=FPlatformTime::Seconds();if(Now>=H.NextIdentityCheck){H.NextIdentityCheck=Now+1.;H.ValidIdentity=S->IKSource->GetDataModel()->GenerateGuid()==S->IKSourceGuid;}return H.ValidIdentity;
}
bool Idle(){return !UCubelessMocapLibrary::IsBusy()&&!(GEditor&&GEditor->PlayWorld);}
FString Json(TSharedRef<FJsonObject> O){FString T;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&T));return T;}
TArray<TSharedPtr<FJsonValue>> Vec(FVector V){return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
bool Same(const FSnapshot& A,const FSnapshot& B){if(A.Start!=B.Start||A.End!=B.End||A.Ease!=B.Ease||A.Markers!=B.Markers||A.BoneEdits.Num()!=B.BoneEdits.Num()||A.Edits.Num()!=B.Edits.Num())return false;for(int I=0;I<A.BoneEdits.Num();++I){const auto& X=A.BoneEdits[I];const auto& Y=B.BoneEdits[I];if(X.Sample!=Y.Sample||X.Bone!=Y.Bone||!X.Delta.Equals(Y.Delta,1.e-8)||!X.Translation.Equals(Y.Translation,1.e-8)||!X.Scale.Equals(Y.Scale,1.e-8))return false;}for(int I=0;I<A.Edits.Num();++I){const auto& X=A.Edits[I];const auto& Y=B.Edits[I];if(X.Control!=Y.Control||X.Start!=Y.Start||X.End!=Y.End||X.Ease!=Y.Ease||!X.Translation.Equals(Y.Translation,1.e-8)||!X.Rotation.Equals(Y.Rotation,1.e-8))return false;}return true;}
UMovieSceneControlRigParameterSection* RigSection(UAnimSequence* A){
 if(auto* Seq=Cast<UMovieSceneSequence>(A->GetDataModelInterface().GetObject()))if(auto* Scene=Seq->GetMovieScene())for(auto* Track:Scene->GetTracks())for(auto* Section:Track->GetAllSections())if(auto* Rig=Cast<UMovieSceneControlRigParameterSection>(Section))return Rig;
 return nullptr;
}
bool PreserveBoundaryChannels(UAnimSequence* Source,UAnimSequence* Out,FName Bone,const TArray<int>& Frames,const FTransformParameterNameAndCurves* Reference=nullptr){
 auto* Original=RigSection(Source);auto* Result=RigSection(Out);if(!Original&&!Result)return true;if(!Original||!Result)return false;
 const FName Control=UFKControlRig::GetControlName(Bone,ERigElementType::Bone);
 const auto* Before=Original->GetTransformParameterNamesAndCurves().FindByPredicate([&](const auto& V){return V.ParameterName==Control;});
 auto* After=Result->GetTransformParameterNamesAndCurves().FindByPredicate([&](const auto& V){return V.ParameterName==Control;});if(!Before)Before=Reference;if(!Before||!After)return false;
 const auto* Movie=Original->GetTypedOuter<UMovieScene>();if(!Movie)return false;const auto FPS=Source->GetDataModel()->GetFrameRate();
 for(int Frame:Frames){const FFrameNumber Tick=FFrameRate::TransformTime(FFrameTime(Frame),FPS,Movie->GetTickResolution()).RoundToFrame();for(int Axis=0;Axis<3;++Axis){
  const FMovieSceneFloatChannel* Src[]={&Before->Translation[Axis],&Before->Rotation[Axis],&Before->Scale[Axis]};FMovieSceneFloatChannel* Dst[]={&After->Translation[Axis],&After->Rotation[Axis],&After->Scale[Axis]};
  for(int Kind=0;Kind<3;++Kind){float Value=Kind==2?1.f:0.f;Src[Kind]->Evaluate(Tick,Value);FMovieSceneFloatValue Key(Value);Key.InterpMode=RCIM_Linear;Dst[Kind]->GetData().UpdateOrAddKey(Tick,Key);}
 }}return true;
}

}

bool CBMIKBusy(){return IKSaving;}
bool CBMIKActive(UCubelessMocapSettings* S){check(IsInGameThread());return Ready(S);}
void CBMIKHistoryCounts(UCubelessMocapSettings* S,int& Undo,int& Redo,bool& Gesture){const auto* H=Histories.Find(S);Undo=H?H->Undo.Num():0;Redo=H?H->Redo.Num():0;Gesture=H&&H->Gesture.IsSet();}
void CBMIKShutdown(){check(IsInGameThread());check(!IKSaving);Histories.Reset();SourceSessions.Reset();}

double CBMIKWeight(const FCBMIKEdit& E,double T,double Duration){
 if(T<E.Start||T>E.End)return 0;
 const double Ease=FMath::Min(E.Ease,(E.End-E.Start)*.5);
 if(Ease<=0)return 1;
 auto Smooth=[](double X){X=FMath::Clamp(X,0.,1.);return X*X*(3-2*X);};
 return (E.Start>1.e-6?Smooth((T-E.Start)/Ease):1.)*(E.End<Duration-1.e-6?Smooth((E.End-T)/Ease):1.);
}
void CBMIKBegin(UCubelessMocapSettings* S){if(!Ready(S)||!Idle())return;auto& H=Histories.FindOrAdd(S);if(!H.Gesture.IsSet())H.Gesture=Capture(S);}
void CBMIKEnd(UCubelessMocapSettings* S){if(!S)return;auto* H=Histories.Find(S);if(!H||!H->Gesture.IsSet())return;if(!Same(H->Gesture.GetValue(),Capture(S))){H->Undo.Add(H->Gesture.GetValue());if(H->Undo.Num()>100)H->Undo.RemoveAt(0);H->Redo.Reset();}H->Gesture.Reset();}
bool CBMIKUndo(UCubelessMocapSettings* S,bool Redo,FString& R){if(!Ready(S)||!Idle()){R=TEXT("IK session unavailable");return false;}CBMIKEnd(S);auto& H=Histories.FindOrAdd(S);auto& A=Redo?H.Redo:H.Undo;auto& B=Redo?H.Undo:H.Redo;if(A.IsEmpty()){R=TEXT("No IK history");return false;}const auto OldMarkers=S->RepresentativeFrames;B.Add(Capture(S));Restore(S,A.Pop());
 if(OldMarkers!=S->RepresentativeFrames&&S->RepresentativeAnimation){const FString Run=CBMDebugRun(S->RepresentativeAnimation);if(!Run.IsEmpty())FFileHelper::SaveStringToFile(UCubelessMocapLibrary::GetRepresentativeState(S),*(FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/Run/TEXT("representative_edits.json")));}
 R=TEXT("IK and manual-marker history restored");return true;}

bool CBMEvaluateIK(UCubelessMocapSettings* S,double T,FCBMIKPose& P){
 P=FCBMIKPose();if(!Ready(S)||!FMath::IsFinite(T))return false;
 const auto& Ref=S->TargetMesh->GetRefSkeleton();const auto* M=S->IKSource->GetDataModel();const double Duration=S->IKSource->GetPlayLength();
 double Frame=FMath::Clamp(T*M->GetFrameRate().AsDecimal(),0.,double(M->GetNumberOfKeys()-1));if(FMath::Abs(Frame-FMath::RoundToDouble(Frame))<1.e-7)Frame=FMath::RoundToDouble(Frame);const int A=FMath::FloorToInt(Frame),B=FMath::Min(A+1,M->GetNumberOfKeys()-1);
 TArray<FTransform> Bind=Ref.GetRefBonePose();for(int I=1;I<Bind.Num();++I)Bind[I]=Bind[I]*Bind[Ref.GetParentIndex(I)];
 P.Local=Ref.GetRefBonePose();for(int I=0;I<Ref.GetNum();++I)if(M->IsValidBoneTrackName(Ref.GetBoneName(I))){const FTransform X=M->GetBoneTrackTransform(Ref.GetBoneName(I),FFrameNumber(A));if(Frame==A)P.Local[I]=X;else P.Local[I].Blend(X,M->GetBoneTrackTransform(Ref.GetBoneName(I),FFrameNumber(B)),Frame-A);}
 auto FK=[&](){P.Global=P.Local;for(int I=1;I<Ref.GetNum();++I)P.Global[I]=P.Global[I]*P.Global[Ref.GetParentIndex(I)];};FK();const auto Original=P.Global;
 auto Index=[&](const FString& Name){return Ref.FindBoneIndex(FName(*Name));};
 auto Delta=[&](FName Name){FTransform D;for(const auto& E:S->IKEdits)if(E.Control==Name){const double W=CBMIKWeight(E,T,Duration);D.AddToTranslation(E.Translation*W);D.SetRotation((FQuat::Slerp(FQuat::Identity,E.Rotation.Quaternion(),W)*D.GetRotation()).GetNormalized());}return D;};
 auto Active=[&](FName Name){const FTransform D=Delta(Name);return !D.GetTranslation().IsNearlyZero(1.e-8)||D.GetRotation().AngularDistance(FQuat::Identity)>1.e-8;};
 auto SetGlobalRotation=[&](int I,FQuat Q){const int Parent=Ref.GetParentIndex(I);P.Local[I].SetRotation((Parent>=0?P.Global[Parent].GetRotation().Inverse()*Q:Q).GetNormalized());FK();};
 const int Pelvis=Index(TEXT("pelvis"));if(Pelvis<0)return false;
 const FTransform PelvisDelta=Delta(TEXT("pelvis"));const bool BodyMoved=Active(TEXT("pelvis"));
 if(BodyMoved){FTransform X=P.Global[Pelvis];X.AddToTranslation(PelvisDelta.GetTranslation());X.SetRotation((PelvisDelta.GetRotation()*X.GetRotation()).GetNormalized());const int Parent=Ref.GetParentIndex(Pelvis);P.Local[Pelvis]=Parent>=0?X.GetRelativeTransform(P.Global[Parent]):X;FK();P.Changed=true;}
 auto AddHandle=[&](FName Name,FVector Pos,FQuat Rot,bool Finger=false){P.Handles.Add({Name,Pos,Rot,Finger});};
 AddHandle(TEXT("pelvis"),P.Global[Pelvis].GetLocation(),P.Global[Pelvis].GetRotation());
 for(const TCHAR* Side:{TEXT("l"),TEXT("r")})for(bool Arm:{false,true}){
  const FString Suffix=FString(TEXT("_"))+Side;const int Root=Index((Arm?TEXT("upperarm"):TEXT("thigh"))+Suffix),Mid=Index((Arm?TEXT("lowerarm"):TEXT("calf"))+Suffix),End=Index((Arm?TEXT("hand"):TEXT("foot"))+Suffix);
  if(Root<0||Mid<0||End<0)return false;
  const FName Target(*((Arm?TEXT("hand"):TEXT("foot"))+Suffix)),Pole(*((Arm?TEXT("elbow"):TEXT("knee"))+Suffix));
  const FVector O=Original[Root].GetLocation(),K=Original[Mid].GetLocation(),E=Original[End].GetLocation(),Axis=(E-O).GetSafeNormal();
  FVector Bend=(K-O)-Axis*FVector::DotProduct(K-O,Axis);if(!Bend.Normalize())Bend=Original[Root].GetRotation().RotateVector(FVector::YAxisVector);const FVector PoleBase=K+Bend*(Arm?22.:32.);
  const FTransform TD=Delta(Target),PD=Delta(Pole);FVector Goal=Original[End].GetLocation()+TD.GetTranslation(),PolePos=O+PD.GetRotation().RotateVector(PoleBase-O)+PD.GetTranslation();
  const bool Solve=Active(Target)||Active(Pole)||(!Arm&&BodyMoved);
  // Arms follow the pelvis until explicitly pinned/edited; feet remain at their source targets.
  if(Arm&&!Solve)Goal=P.Global[End].GetLocation();
  if(Solve){
   const FVector Start=P.Global[Root].GetLocation();const double L1=(P.Global[Mid].GetLocation()-Start).Size(),L2=(P.Global[End].GetLocation()-P.Global[Mid].GetLocation()).Size();
   if(L1>1.e-5&&L2>1.e-5){const FVector Direction=(Goal-Start).GetSafeNormal(UE_SMALL_NUMBER,Axis);const double Dist=FMath::Clamp((Goal-Start).Size(),FMath::Abs(L1-L2)+1.e-4,L1+L2-1.e-4);
    FVector Plane=(PolePos-Start)-Direction*FVector::DotProduct(PolePos-Start,Direction);if(!Plane.Normalize())Plane=Bend;
    const double Along=(Dist*Dist+L1*L1-L2*L2)/(2*Dist);const FVector Knee=Start+Direction*Along+Plane*FMath::Sqrt(FMath::Max(0.,L1*L1-Along*Along));Goal=Start+Direction*Dist;
    SetGlobalRotation(Root,FQuat::FindBetweenVectors(P.Global[Mid].GetLocation()-Start,Knee-Start)*P.Global[Root].GetRotation());
    SetGlobalRotation(Mid,FQuat::FindBetweenVectors(P.Global[End].GetLocation()-P.Global[Mid].GetLocation(),Goal-P.Global[Mid].GetLocation())*P.Global[Mid].GetRotation());
    SetGlobalRotation(End,TD.GetRotation()*Original[End].GetRotation());P.Changed=true;
   }
  }
  AddHandle(Target,P.Global[End].GetLocation(),P.Global[End].GetRotation());AddHandle(Pole,PolePos,FQuat::Identity);
 }
 for(const TCHAR* Side:{TEXT("l"),TEXT("r")})for(const TCHAR* Finger:{TEXT("thumb"),TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky")}){
  int J[3];for(int K=0;K<3;++K)J[K]=Index(FString::Printf(TEXT("%s_%02d_%s"),Finger,K+1,Side));if(J[0]<0||J[1]<0||J[2]<0)continue;
  const FName Name(*FString::Printf(TEXT("%s_tip_%s"),Finger,Side));
  const FVector TipLocal=Bind[J[2]].InverseTransformVectorNoScale(Bind[J[2]].GetLocation()-Bind[J[1]].GetLocation()).GetSafeNormal(UE_SMALL_NUMBER,FVector::XAxisVector)*FMath::Max(1.,P.Local[J[2]].GetTranslation().Size()*.8);
  FVector Points[4]={P.Global[J[0]].GetLocation(),P.Global[J[1]].GetLocation(),P.Global[J[2]].GetLocation(),P.Global[J[2]].TransformPosition(TipLocal)};
  const FTransform D=Delta(Name);const FVector Goal=Points[3]+D.GetTranslation();
  if(Active(Name)){
   double L[3];for(int K=0;K<3;++K)L[K]=(Points[K+1]-Points[K]).Size();const FVector Anchor=Points[0];
   if((Goal-Anchor).Size()>=L[0]+L[1]+L[2]){const FVector Dir=(Goal-Anchor).GetSafeNormal();for(int K=0;K<3;++K)Points[K+1]=Points[K]+Dir*L[K];}
   else for(int Iter=0;Iter<24;++Iter){Points[3]=Goal;for(int K=2;K>=0;--K)Points[K]=Points[K+1]+(Points[K]-Points[K+1]).GetSafeNormal()*L[K];Points[0]=Anchor;for(int K=0;K<3;++K)Points[K+1]=Points[K]+(Points[K+1]-Points[K]).GetSafeNormal()*L[K];if((Points[3]-Goal).Size()<.001)break;}
   for(int K=0;K<3;++K){const FVector End=K<2?P.Global[J[K+1]].GetLocation():P.Global[J[2]].TransformPosition(TipLocal);SetGlobalRotation(J[K],FQuat::FindBetweenVectors(End-P.Global[J[K]].GetLocation(),Points[K+1]-Points[K])*P.Global[J[K]].GetRotation());}
   SetGlobalRotation(J[2],D.GetRotation()*P.Global[J[2]].GetRotation());P.Changed=true;
  }
  AddHandle(Name,P.Global[J[2]].TransformPosition(TipLocal),P.Global[J[2]].GetRotation(),true);
 }
 // Standard mannequin IK helper bones follow the edited endpoints. Never change them outside edits.
 if(P.Changed)for(const TCHAR* Side:{TEXT("l"),TEXT("r")})for(const TCHAR* Limb:{TEXT("foot"),TEXT("hand")}){const FString End=FString(Limb)+TEXT("_")+Side;const int I=Index(TEXT("ik_")+End),BIndex=Index(End);if(I>=0&&BIndex>=0){const int Parent=Ref.GetParentIndex(I);P.Local[I]=Parent>=0?P.Global[BIndex].GetRelativeTransform(P.Global[Parent]):P.Global[BIndex];FK();}}
 return true;
}

bool UCubelessMocapLibrary::BeginIKEdit(UCubelessMocapSettings* S,FString& R){
 check(IsInGameThread());if(!S||!Idle()||!S->TargetMesh||!S->PreviewAnimation||!S->PreviewAnimation->GetDataModel()||S->PreviewAnimation->GetSkeleton()!=S->TargetMesh->GetSkeleton()||S->PreviewAnimation->GetPackage()->IsDirty()||!S->RepresentativeCorrections.IsEmpty()||S->TargetMesh->GetPackage()->IsDirty()){R=TEXT("Need saved animation, matching mesh and idle editor; bake pending bone corrections first");return false;}
 const FGuid Guid=S->PreviewAnimation->GetDataModel()->GenerateGuid();const FString Input=CBMRepresentativeInput(S);
 const FString Key=S->PreviewAnimation->GetPathName()+TEXT("|")+S->TargetMesh->GetPathName()+TEXT("|")+Guid.ToString()+TEXT("|")+Input;
 for(auto It=SourceSessions.CreateIterator();It;++It)if(!It.Key().IsValid()){Histories.Remove(It.Key());It.RemoveCurrent();}
 auto& Sessions=SourceSessions.FindOrAdd(S);
 if(Sessions.Active==Key&&Ready(S)){R=TEXT("IK session already active");return true;}
 if(!Sessions.Active.IsEmpty()&&S->IKSource&&S->IKMesh){CBMIKEnd(S);auto& Previous=Sessions.Saved.FindOrAdd(Sessions.Active);Previous.Source.Reset(S->IKSource);Previous.Mesh.Reset(S->IKMesh);Previous.State=Capture(S);Previous.History=Histories.FindRef(S);}
 const bool KeepMarkers=S->RepresentativeInput==Input&&S->RepresentativeTotalFrames==S->PreviewAnimation->GetDataModel()->GetNumberOfKeys()&&CBMRepresentativeFrameMap(S->RepresentativeFrames,S->RepresentativeTotalFrames);const auto Markers=KeepMarkers?S->RepresentativeFrames:TArray<int32>();const int MarkerSelection=S->SelectedRepresentative;
 S->IKSource=S->PreviewAnimation;S->IKMesh=S->TargetMesh;S->IKSourceGuid=Guid;S->IKEnabled=true;S->IKPlaying=false;S->IKEdits.Reset();S->IKStart=0;S->IKEnd=S->IKSource->GetPlayLength();S->IKEase=.2;S->PoseGizmoMode=0;S->IKSelected=TEXT("foot_l");Histories.Remove(S);
 S->RepresentativeAppliedSource=nullptr;S->RepresentativeAppliedAnimation=nullptr;S->RepresentativeCorrections.Reset();S->RepresentativeFrames=Markers;S->RepresentativeAnimation=S->IKSource;S->RepresentativeInput=Input;S->RepresentativeTotalFrames=S->IKSource->GetDataModel()->GetNumberOfKeys();S->SelectedRepresentative=Markers.IsEmpty()?INDEX_NONE:FMath::Clamp(MarkerSelection,0,Markers.Num()-1);
 const auto* Saved=Sessions.Saved.Find(Key);if(Saved){Restore(S,Saved->State);Histories.Add(S,Saved->History);auto& H=Histories.FindChecked(S);H.NextIdentityCheck=0;H.ValidIdentity=true;}
 Sessions.Active=Key;
 R=Saved?TEXT("소스별 미저장 IK 보정과 마커·Undo 이력을 복원했습니다."):TEXT("현재 자세에서 IK 편집을 시작했습니다. 원본은 유지합니다.");return true;
}
bool UCubelessMocapLibrary::SetIKRange(UCubelessMocapSettings* S,double A,double B,double Ease,FString& R){
 check(IsInGameThread());if(!Ready(S)||!Idle()||!FMath::IsFinite(A)||!FMath::IsFinite(B)||!FMath::IsFinite(Ease)||A<0||B>S->IKSource->GetPlayLength()+1.e-6||B<=A||Ease<0||Ease>(B-A)*.5){R=TEXT("Need 0 <= start < end <= duration and ease <= half range");return false;}auto& H=Histories.FindOrAdd(S);const bool Own=!H.Gesture.IsSet();if(Own)CBMIKBegin(S);
 for(auto& E:S->IKEdits)if(FMath::IsNearlyEqual(E.Start,A,1.e-6)&&FMath::IsNearlyEqual(E.End,B,1.e-6))E.Ease=Ease;
 S->IKStart=A;S->IKEnd=B;S->IKEase=Ease;if(Own)CBMIKEnd(S);R=TEXT("IK 구간과 해당 구간의 연결 시간을 반영했습니다.");return true;
}
bool UCubelessMocapLibrary::SetIKControl(UCubelessMocapSettings* S,FName Name,FVector Translation,FRotator Rotation,FString& R){
 check(IsInGameThread());FCBMIKPose P;if(!S||!Ready(S)||!Idle()||S->IKSource->GetDataModel()->GenerateGuid()!=S->IKSourceGuid||!CBMEvaluateIK(S,(S->IKStart+S->IKEnd)*.5,P)||!P.Handles.ContainsByPredicate([&](const auto& H){return H.Name==Name;})||Translation.ContainsNaN()||Rotation.ContainsNaN()||Translation.Size()>100||Rotation.Quaternion().AngularDistance(FQuat::Identity)>PI*.75){R=TEXT("Invalid IK control, source identity or delta");return false;}
 auto& H=Histories.FindOrAdd(S);const bool Own=!H.Gesture.IsSet();if(Own)CBMIKBegin(S);
 S->IKEdits.RemoveAll([&](const auto& E){return E.Control==Name&&FMath::IsNearlyEqual(E.Start,S->IKStart,1.e-6)&&FMath::IsNearlyEqual(E.End,S->IKEnd,1.e-6);});
 if(!Translation.IsNearlyZero(1.e-8)||!Rotation.IsNearlyZero(1.e-8)){FCBMIKEdit E;E.Control=Name;E.Translation=Translation;E.Rotation=Rotation;E.Start=S->IKStart;E.End=S->IKEnd;E.Ease=S->IKEase;S->IKEdits.Add(E);}
 S->IKSelected=Name;if(Own)CBMIKEnd(S);R=TEXT("IK layer updated; source unchanged");return true;
}
FString UCubelessMocapLibrary::GetIKState(UCubelessMocapSettings* S,double T){
 check(IsInGameThread());auto O=MakeShared<FJsonObject>();FCBMIKPose P;const bool Valid=CBMEvaluateIK(S,T,P);O->SetBoolField(TEXT("active"),Valid);O->SetBoolField(TEXT("changed"),P.Changed);O->SetNumberField(TEXT("seconds"),T);if(!S)return Json(O);
 O->SetStringField(TEXT("source"),S->IKSource?S->IKSource->GetPathName():TEXT(""));O->SetStringField(TEXT("source_guid"),S->IKSourceGuid.ToString());O->SetStringField(TEXT("selected"),S->IKSelected.ToString());O->SetNumberField(TEXT("start"),S->IKStart);O->SetNumberField(TEXT("end"),S->IKEnd);O->SetNumberField(TEXT("ease"),S->IKEase);O->SetNumberField(TEXT("filter"),S->IKFilter);
 O->SetNumberField(TEXT("cached_source_sessions"),SourceSessions.Contains(S)?SourceSessions.FindChecked(S).Saved.Num():0);auto* H=Histories.Find(S);O->SetNumberField(TEXT("undo_steps"),H?H->Undo.Num():0);O->SetNumberField(TEXT("redo_steps"),H?H->Redo.Num():0);
 TArray<TSharedPtr<FJsonValue>> Handles,Edits;for(const auto& X:P.Handles){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),X.Name.ToString());V->SetArrayField(TEXT("position"),Vec(X.Position));V->SetBoolField(TEXT("finger"),X.Finger);Handles.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("handles"),Handles);
 for(const auto& E:S->IKEdits){auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("control"),E.Control.ToString());V->SetArrayField(TEXT("translation"),Vec(E.Translation));V->SetArrayField(TEXT("rotation"),Vec(E.Rotation.Euler()));V->SetNumberField(TEXT("start"),E.Start);V->SetNumberField(TEXT("end"),E.End);V->SetNumberField(TEXT("ease"),E.Ease);V->SetNumberField(TEXT("weight"),CBMIKWeight(E,T,S->IKSource?S->IKSource->GetPlayLength():0));Edits.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("edits"),Edits);return Json(O);
}
UAnimSequence* UCubelessMocapLibrary::SaveIKAnimation(UCubelessMocapSettings* S,const FString& Package,FString& R){
 check(IsInGameThread());auto Fail=[&](const FString& E)->UAnimSequence*{R=E;return nullptr;};if(!Ready(S)||!Idle()||S->IKSource->GetDataModel()->GenerateGuid()!=S->IKSourceGuid||S->IKSource->GetPackage()->IsDirty())return Fail(TEXT("Need unchanged saved IK source and idle editor"));
 if(!Package.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(Package)||FPackageName::DoesPackageExist(Package)||FindPackage(nullptr,*Package))return Fail(TEXT("Choose a new /Game asset path"));
 CBMIKEnd(S);TGuardValue<bool> Saving(IKSaving,true);TStrongObjectPtr<UAnimSequence> Source(S->IKSource);const auto* M=Source->GetDataModel();const auto& Ref=S->TargetMesh->GetRefSkeleton();const int N=M->GetNumberOfKeys();TArray<FCBMIKPose> Poses;bool Changed=false;
 for(int F=0;F<N;++F){FCBMIKPose P;if(!CBMEvaluateIK(S,F/M->GetFrameRate().AsDecimal(),P))return Fail(TEXT("IK evaluation failed before output creation"));Changed|=P.Changed;Poses.Add(MoveTemp(P));}
 auto* Out=DuplicateObject<UAnimSequence>(Source.Get(),CreatePackage(*Package),*FPackageName::GetLongPackageAssetName(Package));Out->SetFlags(RF_Public|RF_Standalone);TStrongObjectPtr<UAnimSequence> Hold(Out);
 if(Changed){auto& C=Out->GetController();C.OpenBracket(FText::FromString(TEXT("Bake interval IK pose layer")),false);bool OK=true;
  for(int B=0;B<Ref.GetNum();++B){const FName Name=Ref.GetBoneName(B);bool BoneChanged=false;for(int F=0;F<N;++F)if(Poses[F].Changed){const FTransform Before=M->IsValidBoneTrackName(Name)?M->GetBoneTrackTransform(Name,FFrameNumber(F)):Ref.GetRefBonePose()[B];if(!Before.Equals(Poses[F].Local[B],1.e-7)){BoneChanged=true;break;}}if(!BoneChanged)continue;
   TOptional<FTransformParameterNameAndCurves> ReferenceChannels;if(!M->IsValidBoneTrackName(Name)){OK=C.AddBoneCurve(Name,false)&&OK;TArray<FVector> V,Z;TArray<FQuat> Q;for(int F=0;F<N;++F){V.Add(Ref.GetRefBonePose()[B].GetTranslation());Q.Add(Ref.GetRefBonePose()[B].GetRotation());Z.Add(Ref.GetRefBonePose()[B].GetScale3D());}OK=C.SetBoneTrackKeys(Name,V,Q,Z,false)&&OK;
    if(auto* Section=RigSection(Out)){const FName Control=UFKControlRig::GetControlName(Name,ERigElementType::Bone);if(const auto* Curve=Section->GetTransformParameterNamesAndCurves().FindByPredicate([&](const auto& V){return V.ParameterName==Control;}))ReferenceChannels=*Curve;}
   }
   // Update only edited spans; never round-trip or normalize protected source keys.
   TArray<int> GuardFrames;int Start=0;while(Start<N){while(Start<N&&!Poses[Start].Changed)++Start;if(Start==N)break;int End=Start;while(End<N&&Poses[End].Changed)++End;if(Start>0)GuardFrames.AddUnique(Start-1);if(End<N)GuardFrames.AddUnique(End);TArray<FVector> V,Z;TArray<FQuat> Q;for(int F=Start;F<End;++F){const FTransform& X=Poses[F].Local[B];V.Add(X.GetTranslation());Z.Add(X.GetScale3D());Q.Add(X.GetRotation());}OK=C.UpdateBoneTrackKeys(Name,FInt32Range(Start,End),V,Q,Z,false)&&OK;Start=End;}
   OK=PreserveBoundaryChannels(Source.Get(),Out,Name,GuardFrames,ReferenceChannels.IsSet()?&ReferenceChannels.GetValue():nullptr)&&OK;

  }C.CloseBracket(false);if(!OK)return Fail(TEXT("IK track write failed; unsaved output retained"));
 }
 if(Source->GetDataModel()->GenerateGuid()!=S->IKSourceGuid)return Fail(TEXT("Source changed during export"));
 Out->SetPreviewMesh(S->TargetMesh,false);Out->GetPackage()->GetMetaData().SetValue(Out,TEXT("CubelessIKSource"),*Source->GetPathName());Out->GetPackage()->GetMetaData().SetValue(Out,TEXT("CubelessIKEdits"),*GetIKState(S,0));Out->GetPackage()->GetMetaData().SetValue(Out,TEXT("CubelessMocapDebugRun"),*CBMDebugRun(Source.Get(),S->OriginalAnimation));
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;if(!UPackage::SavePackage(Out->GetPackage(),Out,*FPackageName::LongPackageNameToFilename(Package,FPackageName::GetAssetPackageExtension()),Args))return Fail(TEXT("IK output save failed"));FAssetRegistryModule::AssetCreated(Out);
 const double RangeStart=S->IKStart,RangeEnd=S->IKEnd,Ease=S->IKEase;const FName Selected=S->IKSelected;
 S->ExportedSequence=Out;S->PreviewAnimation=Out;S->SequenceName=FPackageName::GetLongPackageAssetName(Package);S->OutputFolder=FPackageName::GetLongPackagePath(Package);S->IKEnabled=false;R=TEXT("Saved new animation; original preserved: ")+Out->GetPathName();FString Init;
 IKSaving=false;if(BeginIKEdit(S,Init)){SetIKRange(S,RangeStart,RangeEnd,Ease,Init);SetIKControl(S,Selected,FVector::ZeroVector,FRotator::ZeroRotator,Init);}else R+=TEXT(" | IK reinitialization: ")+Init;
 return Out;
}
