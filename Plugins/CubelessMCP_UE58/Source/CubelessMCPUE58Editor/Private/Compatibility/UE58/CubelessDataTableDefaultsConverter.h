#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"

class FJsonObject;
class UClass;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::UE58
{
    // Only the mounted DataTable toolset List row_name_contains empty default.
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeDataTableDefaultsConverter();
    bool PrepareDataTableDefaults(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
}
