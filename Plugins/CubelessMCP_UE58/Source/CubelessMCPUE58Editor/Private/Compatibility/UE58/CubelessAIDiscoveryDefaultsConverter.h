#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"

class FJsonObject;
class UClass;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::UE58
{
    // Only the merged AI toolset's two original empty FString defaults.
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeAIDiscoveryDefaultsConverter();
    bool PrepareAIDiscoveryDefaults(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
}
