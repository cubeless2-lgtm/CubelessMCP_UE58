#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::UserDefinedEnumEditing
{
FString Inspect(const FString& AssetPath);
FString Edit(const FString& AssetPath,const FString& EditsJson,bool bDryRun,bool bSave,bool bAllowRemove);
}
