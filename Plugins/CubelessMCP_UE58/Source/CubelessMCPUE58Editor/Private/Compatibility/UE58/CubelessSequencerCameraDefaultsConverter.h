#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"

class FJsonObject;
class UClass;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::UE58
{
    // Only the appended CameraCut method's exact binding_id/actor_label empty defaults.
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MakeSequencerCameraDefaultsConverter();
    bool PrepareSequencerCameraDefaults(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
}
