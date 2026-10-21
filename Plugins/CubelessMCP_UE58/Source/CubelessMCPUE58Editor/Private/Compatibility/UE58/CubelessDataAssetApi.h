#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::DataAsset
{
FString Get(const FString& AssetPath);
FString Set(const FString& AssetPath, const FString& PropertiesJson, bool bSave);
FString Create(const FString& AssetPath, const FString& ClassPath, bool bSave);
}
