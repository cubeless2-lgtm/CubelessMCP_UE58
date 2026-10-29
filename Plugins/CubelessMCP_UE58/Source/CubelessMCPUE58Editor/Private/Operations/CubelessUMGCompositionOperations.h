#pragma once
#include "CoreMinimal.h"
namespace Cubeless::Operations::UMGComposition
{
FString Compose(const FString& AssetPath,const FString& NodesJson);
FString Animate(const FString& AssetPath,const FString& Name,double Duration,const FString& TracksJson);
FString Inspect(const FString& AssetPath);
FString Finalize(const FString& AssetPath,bool Save);
}
