#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEdGraphNode;
namespace Cubeless::UE58::Sequence
{
TSharedRef<FJsonObject> AddGuarded(const FString& GraphPath,int32 X,int32 Y,int32 OutputCount,const TFunction<bool(UEdGraphNode*)>& Guard,const TFunction<bool(UEdGraphNode*)>& FreezePins);
TSharedRef<FJsonObject> Add(const FString& GraphPath, int32 X, int32 Y, int32 OutputCount);
}
