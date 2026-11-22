#pragma once
#include "CubelessMocapSettings.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

inline TArray<FName> CBMRepresentativeBones(){return {TEXT("root"),TEXT("pelvis"),TEXT("spine_01"),TEXT("spine_02"),TEXT("spine_03"),TEXT("spine_04"),TEXT("spine_05"),TEXT("neck_01"),TEXT("head"),TEXT("clavicle_l"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),TEXT("clavicle_r"),TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),TEXT("ball_l"),TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),TEXT("ball_r")};}
inline bool CBMRepresentativeFrameMap(const TArray<int32>& F,int Total){if(F.Num()>Total||Total<3||Total>14400)return false;for(int I=1;I<F.Num();++I)if(F[I]<=F[I-1])return false;return true;}
inline FQuat CBMRepresentativeDelta(const UCubelessMocapSettings* S,FName Bone,double Frame){
 if(!S||S->RepresentativeFrames.IsEmpty())return FQuat::Identity;
 auto At=[&](int32 Sample){for(const auto& C:S->RepresentativeCorrections)if(C.Bone==Bone&&C.Sample==Sample)return C.Delta.Quaternion();return FQuat::Identity;};
 int32 B=0;while(B+1<S->RepresentativeFrames.Num()&&Frame>S->RepresentativeFrames[B+1])++B;
 if(B+1==S->RepresentativeFrames.Num())return At(B);
 const double A=S->RepresentativeFrames[B],Z=S->RepresentativeFrames[B+1];return FQuat::Slerp(At(B),At(B+1),FMath::Clamp((Frame-A)/FMath::Max(1.,Z-A),0.,1.)).GetNormalized();
}
inline FString CBMRepresentativeInput(const UCubelessMocapSettings* S){
 if(!S)return {};const FString P=FPaths::ConvertRelativePathToFull(S->Video.FilePath);
 return FString::Printf(TEXT("%s|%lld|%s|%.6f|%.6f|%d|%s"),*P,IFileManager::Get().FileSize(*P),*IFileManager::Get().GetTimeStamp(*P).ToIso8601(),S->StartSeconds,S->DurationSeconds,S->FrameRate,S->TargetMesh?*S->TargetMesh->GetPathName():TEXT(""));
}

inline FVector CBMRepresentativeVector(const UCubelessMocapSettings* S,FName Bone,double Frame,bool Scale){const FVector Identity=Scale?FVector::OneVector:FVector::ZeroVector;if(!S||S->RepresentativeFrames.IsEmpty())return Identity;auto At=[&](int I){for(const auto& C:S->RepresentativeCorrections)if(C.Sample==I&&C.Bone==Bone)return Scale?C.Scale:C.Translation;return Identity;};int I=0;while(I+1<S->RepresentativeFrames.Num()&&Frame>S->RepresentativeFrames[I+1])++I;if(I+1==S->RepresentativeFrames.Num())return At(I);const double A=S->RepresentativeFrames[I],Z=S->RepresentativeFrames[I+1];return FMath::Lerp(At(I),At(I+1),FMath::Clamp((Frame-A)/FMath::Max(1.,Z-A),0.,1.));}
inline bool CBMRepresentativeTransformValid(const FVector& T,const FRotator& R,const FVector& S){return !T.ContainsNaN()&&!R.ContainsNaN()&&!S.ContainsNaN()&&T.Size()<=100.&&R.Quaternion().AngularDistance(FQuat::Identity)<=FMath::DegreesToRadians(60.)&&S.GetMin()>=.1&&S.GetMax()<=10.;}
