#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"

class FJsonObject;
class UClass;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::UE58
{
    // Only the merged Blueprint toolset's exact skeletal mesh/anim class empty defaults.
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeBlueprintDefaultsConverter();
    bool PrepareBlueprintDefaults(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
}
