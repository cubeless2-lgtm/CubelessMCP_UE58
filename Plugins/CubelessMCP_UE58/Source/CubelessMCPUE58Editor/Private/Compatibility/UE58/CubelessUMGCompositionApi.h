#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
class UWidget;
namespace Cubeless::UE58::UMGComposition
{
struct FNode { FString Name, Type, Parent; };
struct FKey { double Time=0; float X=0, Y=0; };
struct FTrack { FString Widget, Property; TArray<FKey> Keys; };
void Compose(const FString& AssetPath,const TArray<FNode>& Nodes,const TSharedRef<FJsonObject>& R);
void Animate(const FString& AssetPath,const FString& Name,double Duration,const TArray<FTrack>& Tracks,const TSharedRef<FJsonObject>& R);
void Inspect(const FString& AssetPath,const TSharedRef<FJsonObject>& R);
void Finalize(const FString& AssetPath,bool Save,const TSharedRef<FJsonObject>& R);
UWidget* SourceWidget(const FString& AssetPath,const FString& Name);
}
