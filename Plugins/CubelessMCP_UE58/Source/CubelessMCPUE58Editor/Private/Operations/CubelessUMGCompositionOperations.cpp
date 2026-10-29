#include "Operations/CubelessUMGCompositionOperations.h"
#include "Compatibility/UE58/CubelessUMGCompositionApi.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/PackageName.h"
namespace Cubeless::Operations::UMGComposition
{
namespace
{
auto Result(const TCHAR* Operation)
{
 auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("operation"),Operation);R->SetBoolField(TEXT("success"),false);
 R->SetStringField(TEXT("stage"),TEXT("preflight"));R->SetBoolField(TEXT("mutation_attempted"),false);R->SetBoolField(TEXT("changed"),false);
 R->SetBoolField(TEXT("compile_attempted"),false);R->SetBoolField(TEXT("save_attempted"),false);R->SetBoolField(TEXT("saved"),false);
 R->SetBoolField(TEXT("atomic_rollback"),false);R->SetBoolField(TEXT("automatic_retry"),false);return R;
}
void Error(const TSharedRef<FJsonObject>& R,const TCHAR* Message) { R->SetStringField(TEXT("error"),Message); }
FString Encode(const TSharedRef<FJsonObject>& R) { FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S; }
bool NameOK(const FString& S)
{
 if(S.IsEmpty()||S.Len()>64||!FChar::IsAlpha(S[0]))return false;
 for(TCHAR C:S)if(C>127||(!FChar::IsAlnum(C)&&C!=TEXT('_')))return false;
 // NAME_None requests an automatically generated UObject name, not this identifier.
 return !FName(*S).IsNone();
}
bool PathOK(const FString& P) { return P.Len()<256&&P.StartsWith(TEXT("/Game/"))&&FPackageName::IsValidLongPackageName(P)&&NameOK(FPackageName::GetLongPackageAssetName(P)); }
bool Array(const FString& S,TArray<TSharedPtr<FJsonValue>>& A) { return S.Len()<=256*1024&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),A); }
bool KeysOnly(const TSharedPtr<FJsonObject>& O,const TSet<FString>& Keys) { for(const auto& P:O->Values)if(!Keys.Contains(FString(P.Key)))return false;return true; }
}
FString Compose(const FString& P,const FString& Json)
{
 auto R=Result(TEXT("compose_widget_blueprint"));TArray<TSharedPtr<FJsonValue>> A;TArray<UE58::UMGComposition::FNode> Nodes;TMap<FName,FString> Types;TMap<FName,int32> Counts;
 const TSet<FString> Allowed={TEXT("CanvasPanel"),TEXT("Overlay"),TEXT("Border"),TEXT("Image"),TEXT("TextBlock"),TEXT("Button"),TEXT("ProgressBar"),TEXT("SizeBox"),TEXT("HorizontalBox"),TEXT("VerticalBox"),TEXT("SafeZone"),TEXT("BackgroundBlur")};
 const TSet<FString> Leaves={TEXT("Image"),TEXT("TextBlock"),TEXT("ProgressBar")};const TSet<FString> Single={TEXT("Border"),TEXT("Button"),TEXT("SizeBox"),TEXT("SafeZone"),TEXT("BackgroundBlur")};
 if(!PathOK(P)||!Array(Json,A)||A.IsEmpty()||A.Num()>192){Error(R,TEXT("Valid /Game path and ordered array of1..192 nodes required"));return Encode(R);}
 for(int32 I=0;I<A.Num();++I)
 {
  const auto O=A[I]->Type==EJson::Object?A[I]->AsObject():nullptr;UE58::UMGComposition::FNode N;
  if(!O||!KeysOnly(O,{TEXT("name"),TEXT("type"),TEXT("parent")})||!O->TryGetStringField(TEXT("name"),N.Name)||!O->TryGetStringField(TEXT("type"),N.Type)||!NameOK(N.Name)||!Allowed.Contains(N.Type)||Types.Contains(FName(*N.Name))){Error(R,TEXT("Native widget type, unique ASCII name and known node keys required"));return Encode(R);}
  O->TryGetStringField(TEXT("parent"),N.Parent);
  if(I==0){if(!N.Parent.IsEmpty()||N.Type!=TEXT("CanvasPanel")){Error(R,TEXT("First node must be the only parentless CanvasPanel"));return Encode(R);}}
  else
  {
   const FString* ParentType=Types.Find(FName(*N.Parent));if(!ParentType||Leaves.Contains(*ParentType)){Error(R,TEXT("Parent must be an earlier native panel"));return Encode(R);}
   int32& Count=Counts.FindOrAdd(FName(*N.Parent));++Count;if(Single.Contains(*ParentType)&&Count>1){Error(R,TEXT("Content widgets accept exactly one child"));return Encode(R);}
  }
  Types.Add(FName(*N.Name),N.Type);Nodes.Add(N);
 }
 UE58::UMGComposition::Compose(P,Nodes,R);return Encode(R);
}
FString Animate(const FString& P,const FString& Name,double Duration,const FString& Json)
{
 auto R=Result(TEXT("add_widget_animation"));TArray<TSharedPtr<FJsonValue>> A;TArray<UE58::UMGComposition::FTrack> Tracks;TSet<FString> Used;
 if(!PathOK(P)||!NameOK(Name)||!FMath::IsFinite(Duration)||Duration<=0||Duration>30||!Array(Json,A)||A.IsEmpty()||A.Num()>64){Error(R,TEXT("Valid path/name, duration(0,30] and1..64 tracks required"));return Encode(R);}
 for(const auto& V:A)
 {
  const auto O=V->Type==EJson::Object?V->AsObject():nullptr;UE58::UMGComposition::FTrack T;const TArray<TSharedPtr<FJsonValue>>* Keys=nullptr;
  if(!O||!KeysOnly(O,{TEXT("widget"),TEXT("property"),TEXT("keys")})||!O->TryGetStringField(TEXT("widget"),T.Widget)||!NameOK(T.Widget)||!O->TryGetStringField(TEXT("property"),T.Property)||(T.Property!=TEXT("opacity")&&T.Property!=TEXT("translation"))||!O->TryGetArrayField(TEXT("keys"),Keys)||Keys->Num()<2||Keys->Num()>256||Used.Contains(T.Widget.ToLower()+TEXT("/")+T.Property)){Error(R,TEXT("Unique widget opacity/translation tracks with2..256 numeric keys required"));return Encode(R);}
  double Previous=-1;int32 PreviousFrame=-1;
  for(const auto& KV:*Keys)
  {
   if(KV->Type!=EJson::Array){Error(R,TEXT("Key must be an array"));return Encode(R);}const auto& K=KV->AsArray();const int32 Width=T.Property==TEXT("opacity")?2:3;
   if(K.Num()!=Width){Error(R,TEXT("Keys are [time,opacity] or [time,x,y]"));return Encode(R);}for(const auto& N:K)if(N->Type!=EJson::Number||!FMath::IsFinite(N->AsNumber())){Error(R,TEXT("Finite numeric keys required"));return Encode(R);}
   const double Time=K[0]->AsNumber(),X=K[1]->AsNumber(),Y=Width==3?K[2]->AsNumber():0;
   if(Time<0||Time>Duration||Time<=Previous||(T.Property==TEXT("opacity")&&(X<0||X>1))||FMath::Abs(X)>8192||FMath::Abs(Y)>8192){Error(R,TEXT("Increasing key times within duration and bounded values required"));return Encode(R);}
   const int32 Frame=FMath::RoundToInt(Time*24000);if(Frame<=PreviousFrame){Error(R,TEXT("Key times must remain distinct after24000Hz quantization"));return Encode(R);}PreviousFrame=Frame;
   Previous=Time;UE58::UMGComposition::FKey Key;Key.Time=Time;Key.X=static_cast<float>(X);Key.Y=static_cast<float>(Y);T.Keys.Add(Key);
  }
  Used.Add(T.Widget.ToLower()+TEXT("/")+T.Property);Tracks.Add(T);
 }
 UE58::UMGComposition::Animate(P,Name,Duration,Tracks,R);return Encode(R);
}
FString Inspect(const FString& P) { auto R=Result(TEXT("inspect_widget_blueprint"));if(!PathOK(P))Error(R,TEXT("Valid /Game path required"));else UE58::UMGComposition::Inspect(P,R);return Encode(R); }
FString Finalize(const FString& P,bool Save) { auto R=Result(TEXT("finalize_widget_blueprint"));if(!PathOK(P))Error(R,TEXT("Valid /Game path required"));else UE58::UMGComposition::Finalize(P,Save,R);return Encode(R); }
}
