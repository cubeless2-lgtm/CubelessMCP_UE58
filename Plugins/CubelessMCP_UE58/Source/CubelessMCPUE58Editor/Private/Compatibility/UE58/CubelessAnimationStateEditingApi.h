#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::AnimationStateEditing
{
// Shared Editor-only authored binding snapshot, not a reflected/runtime API.
bool CaptureNodeBindings(UObject* Node,const FString& Prefix,TMap<FString,FString>& Values);
FString Inspect(UObject* Blueprint,const FString& GraphName);
FString Edit(UObject* Blueprint,const FString& Action,const FString& GraphName,
 const FString& Arguments,bool Execute,bool Compile,bool Save,bool AllowDirty,bool AllowDestructive);
}
