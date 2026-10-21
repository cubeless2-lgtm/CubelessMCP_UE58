#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"

class FJsonObject;
class UClass;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::UE58
{
    // Only the appended MakeStruct method's exact graph_name/graph_id/graph_type empty defaults.
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeBlueprintMakeStructDefaultsConverter();
    bool PrepareBlueprintMakeStructDefaults(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
}
