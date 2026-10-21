#pragma once
#include "CoreMinimal.h"

/** Original blank EQS creation and bounded summary inspection through public Engine APIs. */
namespace Cubeless::UE58::EnvQuery
{
    FString CreateAsset(const FString& AssetName, const FString& AssetRoot, bool bSaveAsset);
    FString InspectAsset(const FString& AssetPath);
}
