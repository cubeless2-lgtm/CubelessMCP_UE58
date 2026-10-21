#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"

class FJsonObject;
class UClass;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::UE58
{
    // Only the appended Float method's exact binding_id/actor_label empty defaults.
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeSequencerFloatDefaultsConverter();
    bool PrepareSequencerFloatDefaults(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
}
