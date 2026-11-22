#include "SCBMDebugOverlay.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Rendering/DrawElements.h"
void SCBMDebugOverlay::Update(bool Enabled,UAnimSequence* Anim,const FString& Video,double Start,double Time,bool Sync,TConstArrayView<FVector3f> ViewPoints,const FString& EvidenceRun){
 Show=false;if(!Enabled){Key.Empty();return;}
 const FString Run=EvidenceRun;
 const FString Identity=Run+TEXT("|")+FPaths::ConvertRelativePathToFull(Video)+FString::Printf(TEXT("|%.6f"),Start);
 if(Identity!=FileIdentity||FPlatformTime::Seconds()>=NextFileCheck){FileIdentity=Identity;NextFileCheck=FPlatformTime::Seconds()+1.;FileStamp.Reset();const FString Dir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/Run;
  for(const FString& P:{Dir/TEXT("DebugBones.json"),Dir/TEXT("DebugBones.f32"),Video})FileStamp+=FString::Printf(TEXT("|%lld|%s"),IFileManager::Get().FileSize(*P),*IFileManager::Get().GetTimeStamp(*P).ToIso8601());}
 const FString NewKey=Identity+FileStamp;
 if(NewKey!=Key){if(Pending.IsValid()&&!Pending.IsReady()){Status=TEXT("이전 캐시 읽기 완료 대기 중");return;}if(Pending.IsValid())Pending.Get();Key=NewKey;Data.Reset();const FString Dir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/Run;
 Pending=Async(EAsyncExecution::ThreadPool,[Dir,Video,Start]()->TSharedPtr<FData>{auto D=MakeShared<FData>();FString Text;TSharedPtr<FJsonObject> J;TArray<uint8> Bytes;
 if(!FFileHelper::LoadFileToString(Text,*(Dir/TEXT("DebugBones.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J)||!J.IsValid())return D;
 double Version=0,Frames=0;const TArray<TSharedPtr<FJsonValue>>* Camera=nullptr;
 J->TryGetBoolField(TEXT("representative_draft"),D->Draft);
 if(!J->TryGetNumberField(TEXT("version"),Version)||Version!=2||!J->TryGetNumberField(TEXT("frames"),Frames)||Frames<1||Frames>14400||!J->TryGetNumberField(TEXT("fps"),D->FPS)||D->FPS<=0||!J->TryGetNumberField(TEXT("start"),D->Start)||!J->TryGetStringField(TEXT("video"),D->Video)||!J->TryGetStringField(TEXT("video_stamp"),D->Stamp)||!J->TryGetNumberField(TEXT("video_size"),D->Size)||!J->TryGetArrayField(TEXT("camera"),Camera)||Camera->Num()!=10)return D;
 const FString Path=FPaths::ConvertRelativePathToFull(Video);if(!FPaths::IsSamePath(Path,D->Video)||FMath::Abs(Start-D->Start)>1.e-5||IFileManager::Get().FileSize(*Path)!=int64(D->Size)||IFileManager::Get().GetTimeStamp(*Path).ToIso8601()!=D->Stamp)return D;
 for(const auto& X:*Camera){double V;if(!X->TryGetNumber(V)||!FMath::IsFinite(V))return D;D->Camera.Add(V);}D->Frames=int32(Frames);
 const TArray<TSharedPtr<FJsonValue>>* Fit=nullptr;if(!J->TryGetArrayField(TEXT("world_to_camera"),Fit)||Fit->Num()!=12)return D;for(const auto& X:*Fit){double V;if(!X->TryGetNumber(V)||!FMath::IsFinite(V))return D;D->Fit.Add(V);}
 if(D->Camera[0]<=0||D->Camera[1]<=0||D->Camera[2]<=0||IFileManager::Get().FileSize(*(Dir/TEXT("DebugBones.f32")))!=int64(D->Frames)*13*6*4||!FFileHelper::LoadFileToArray(Bytes,*(Dir/TEXT("DebugBones.f32"))))return D;
 D->Points.SetNumUninitialized(Bytes.Num()/4);FMemory::Memcpy(D->Points.GetData(),Bytes.GetData(),Bytes.Num());for(float X:D->Points)if(!FMath::IsFinite(X))return D;D->Valid=true;return D;});}
 if(Pending.IsValid()){if(!Pending.IsReady()){Status=TEXT("디버그 캐시 읽는 중 (추론 없음)");return;}Data=Pending.Get();Pending=TFuture<TSharedPtr<FData>>();}
 if(!Data||!Data->Valid){Status=TEXT("일치하는 디버그 캐시 없음 — 자동 재추론하지 않습니다");return;}
 if(!Sync){Status=TEXT("영상 이동 중 — 프레임 일치 후 표시");return;}
 if(!Anim||Time<0||Time*Data->FPS>Data->Frames-1+.01){Status=TEXT("결과 시간 범위를 확인하세요");return;}
 if(ViewPoints.Num()!=13){Status=TEXT("결과 본 준비 중");return;}
 int32 F=FMath::Clamp(FMath::RoundToInt(Time*Data->FPS),0,Data->Frames-1);Canvas=FVector2D(Data->Camera[1],Data->Camera[2]);Observed.Reset();Model.Reset();Applied.Reset();
 // Project the current result bones into the fixed, recorded video camera.
 // This changes with animation time, but never with the 3D preview camera.
 for(int I=0;I<13;++I){const float* P=Data->Points.GetData()+(F*13+I)*6;Observed.Add(FVector3f(P[0],P[1],P[2]));Model.Add(FVector3f(P[3],P[4],P[5]));
  if(Data->Draft){Model.Last().Z=0;Applied.Add(FVector3f(0,0,0));continue;}
  const auto& V=ViewPoints[I];const double X=V.X*.01+Data->Camera[7],Y=V.Z*.01+Data->Camera[8],Z=V.Y*.01+Data->Camera[9];const auto& R=Data->Fit;
  const double CX=R[0]*X+R[1]*Y+R[2]*Z+R[9],CY=R[3]*X+R[4]*Y+R[5]*Z+R[10],CZ=R[6]*X+R[7]*Y+R[8]*Z+R[11];
  Applied.Add(CZ>.001?FVector3f(Data->Camera[0]*CX/CZ+Canvas.X*.5,Data->Camera[0]*CY/CZ+Canvas.Y*.5,1):FVector3f(0,0,0));}
 Show=true;Status=Data->Draft?TEXT("대표 자세: 초록 2D 검출만 표시합니다. 오른쪽은 자세 편집용 자유 카메라입니다. 촬영 카메라/이동 정합은 전체 계산에서 수행합니다."):FString::Printf(TEXT("초록: 2D 검출 | 자홍: 모델 추정 | 청록: 결과 본 (영상 카메라 고정) | %d 프레임\n청록선은 프리뷰 카메라 회전·이동·확대와 독립적입니다. 검출은 정답이 아닙니다."),F);Invalidate(EInvalidateWidgetReason::Paint);
}
int32 SCBMDebugOverlay::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const{
 if(!Show)return Layer;const int Edges[][2]={{0,1},{1,2},{2,3},{0,4},{4,5},{5,6},{7,8},{8,9},{10,11},{11,12},{7,10},{7,0},{10,0}};
 auto Draw=[&](const TArray<FVector3f>& P,FLinearColor Color,float Min){for(const auto& E:Edges){if(P[E[0]].Z<Min||P[E[1]].Z<Min)continue;TArray<FVector2D> Line;for(int I:E)Line.Add(FVector2D(P[I].X/Canvas.X*G.GetLocalSize().X,P[I].Y/Canvas.Y*G.GetLocalSize().Y));FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Line,ESlateDrawEffect::None,Color,true,2.f);}};
 Draw(Model,FLinearColor(1,0,1),.5f);Draw(Applied,FLinearColor(0,1,1),.5f);Draw(Observed,FLinearColor(0,1,0),.6f);return Layer+1;
}
