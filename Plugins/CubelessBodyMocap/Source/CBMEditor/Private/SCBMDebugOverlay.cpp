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
void SCBMDebugOverlay::Update(bool Enabled,UAnimSequence* Anim,USkeletalMeshComponent* Mesh,const FString& Video,double Start,double Time,bool Sync){
 Show=false;if(!Enabled)return;
 const FString Name=Anim?Anim->GetName():FString();const FString Run=Name.StartsWith(TEXT("Body_"))&&Name.Len()>=37?Name.Mid(5,32):FString();
 const FString NewKey=Run+TEXT("|")+FPaths::ConvertRelativePathToFull(Video)+FString::Printf(TEXT("|%.6f"),Start);
 if(NewKey!=Key){if(Pending.IsValid()&&!Pending.IsReady()){Status=TEXT("이전 캐시 읽기 완료 대기 중");return;}if(Pending.IsValid())Pending.Get();Key=NewKey;Data.Reset();const FString Dir=FPaths::ProjectSavedDir()/TEXT("CubelessBodyMocap")/Run;
 Pending=Async(EAsyncExecution::ThreadPool,[Dir,Video,Start]()->TSharedPtr<FData>{auto D=MakeShared<FData>();FString Text;TSharedPtr<FJsonObject> J;TArray<uint8> Bytes;
 if(!FFileHelper::LoadFileToString(Text,*(Dir/TEXT("DebugBones.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J)||!J.IsValid())return D;
 double Version=0,Frames=0;const TArray<TSharedPtr<FJsonValue>>* Camera=nullptr;
 if(!J->TryGetNumberField(TEXT("version"),Version)||Version!=1||!J->TryGetNumberField(TEXT("frames"),Frames)||Frames<1||Frames>14400||!J->TryGetNumberField(TEXT("fps"),D->FPS)||D->FPS<=0||!J->TryGetNumberField(TEXT("start"),D->Start)||!J->TryGetStringField(TEXT("video"),D->Video)||!J->TryGetStringField(TEXT("video_stamp"),D->Stamp)||!J->TryGetNumberField(TEXT("video_size"),D->Size)||!J->TryGetArrayField(TEXT("camera"),Camera)||Camera->Num()!=10)return D;
 const FString Path=FPaths::ConvertRelativePathToFull(Video);if(!FPaths::IsSamePath(Path,D->Video)||FMath::Abs(Start-D->Start)>1.e-5||IFileManager::Get().FileSize(*Path)!=int64(D->Size)||IFileManager::Get().GetTimeStamp(*Path).ToIso8601()!=D->Stamp)return D;
 for(const auto& X:*Camera){double V;if(!X->TryGetNumber(V)||!FMath::IsFinite(V))return D;D->Camera.Add(V);}D->Frames=int32(Frames);
 if(D->Camera[0]<=0||D->Camera[1]<=0||D->Camera[2]<=0||IFileManager::Get().FileSize(*(Dir/TEXT("DebugBones.f32")))!=int64(D->Frames)*13*6*4||!FFileHelper::LoadFileToArray(Bytes,*(Dir/TEXT("DebugBones.f32"))))return D;
 D->Points.SetNumUninitialized(Bytes.Num()/4);FMemory::Memcpy(D->Points.GetData(),Bytes.GetData(),Bytes.Num());for(float X:D->Points)if(!FMath::IsFinite(X))return D;D->Valid=true;return D;});}
 if(Pending.IsValid()){if(!Pending.IsReady()){Status=TEXT("디버그 캐시 읽는 중 (추론 없음)");return;}Data=Pending.Get();Pending=TFuture<TSharedPtr<FData>>();}
 if(!Data||!Data->Valid){Status=TEXT("일치하는 디버그 캐시 없음 — 자동 재추론하지 않습니다");return;}
 if(!Sync){Status=TEXT("영상 이동 중 — 프레임 일치 후 표시");return;}
 if(!Mesh||!Anim||Time<0||Time*Data->FPS>Data->Frames-1+.01){Status=TEXT("결과 시간 범위를 확인하세요");return;}
 int32 F=FMath::Clamp(FMath::RoundToInt(Time*Data->FPS),0,Data->Frames-1);Canvas=FVector2D(Data->Camera[1],Data->Camera[2]);Observed.Reset();Model.Reset();Applied.Reset();
 const TCHAR* Names[]={TEXT("pelvis"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r")};
 const auto& C=Data->Camera;const double CP=FMath::Cos(-C[3]),SP=FMath::Sin(-C[3]),CR=FMath::Cos(C[4]),SR=FMath::Sin(C[4]);
 for(int I=0;I<13;++I){const float* P=Data->Points.GetData()+(F*13+I)*6;Observed.Add(FVector3f(P[0],P[1],P[2]));Model.Add(FVector3f(P[3],P[4],P[5]));FVector3f UV(0,0,0);
 if(Mesh->GetBoneIndex(Names[I])!=INDEX_NONE){FVector V=Mesh->GetBoneLocation(Names[I],EBoneSpaces::ComponentSpace)/100.;double X=V.X+C[7],Y=V.Z+C[8]-C[5],Z=V.Y+C[9];double CX=CR*X-SP*SR*Y+CP*SR*Z,CY=-CP*Y-SP*Z+C[6],CZ=SR*X+SP*CR*Y-CP*CR*Z;if(CZ>.01)UV=FVector3f(C[0]*CX/CZ+C[1]*.5,C[0]*CY/CZ+C[2]*.5,1);}
 Applied.Add(UV);}
 Show=true;Status=FString::Printf(TEXT("초록: 2D 검출  |  자홍: 모델 추정  |  청록: 선택 애니메이션  |  %d 프레임\n검출은 정답이 아닙니다. 체형·관절 정의 차이가 포함됩니다."),F);Invalidate(EInvalidateWidgetReason::Paint);
}
int32 SCBMDebugOverlay::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const{
 if(!Show)return Layer;const int Edges[][2]={{0,1},{1,2},{2,3},{0,4},{4,5},{5,6},{7,8},{8,9},{10,11},{11,12},{7,10},{7,0},{10,0}};
 auto Draw=[&](const TArray<FVector3f>& P,FLinearColor Color,float Min){for(const auto& E:Edges){if(P[E[0]].Z<Min||P[E[1]].Z<Min)continue;TArray<FVector2D> Line;for(int I:E)Line.Add(FVector2D(P[I].X/Canvas.X*G.GetLocalSize().X,P[I].Y/Canvas.Y*G.GetLocalSize().Y));FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Line,ESlateDrawEffect::None,Color,true,2.f);}};
 Draw(Model,FLinearColor(1,0,1),.5f);Draw(Applied,FLinearColor(0,1,1),.5f);Draw(Observed,FLinearColor(0,1,0),.6f);return Layer+1;
}
