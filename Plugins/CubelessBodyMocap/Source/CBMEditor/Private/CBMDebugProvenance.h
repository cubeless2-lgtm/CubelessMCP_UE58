#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
inline FString CBMDebugRun(const UAnimSequence* Animation,const UAnimSequence* Evidence=nullptr){
 auto Resolve=[](const UAnimSequence* A){
  if(!A)return FString();const FString Name=A->GetName();
  FString Run=Name.StartsWith(TEXT("Body_"))&&Name.Len()>=37?Name.Mid(5,32):A->GetPackage()->GetMetaData().GetValue(A,TEXT("CubelessMocapDebugRun"));
  if(Run.Len()!=32)return FString();for(TCHAR C:Run)if(!FChar::IsHexDigit(C))return FString();return Run;
 };
 const FString Run=Resolve(Animation);return Run.IsEmpty()?Resolve(Evidence):Run;
}
