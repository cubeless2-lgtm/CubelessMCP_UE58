#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"

namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }
class UClass;

namespace Cubeless::UE58
{
    // Exact Python-generated Material tool parameters only. No general array override.
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeMaterialArrayConverter();
    // Validate both exact declared defaults; normalize only the empty-array metadata.
    bool PrepareMaterialArrayDefaults(UClass* ToolsetClass, FString& Error, bool& bNormalized);
}
