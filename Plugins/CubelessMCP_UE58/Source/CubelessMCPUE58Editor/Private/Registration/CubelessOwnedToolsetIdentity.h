#pragma once
#include "Templates/SharedPointer.h"
namespace UE::ToolsetRegistry { class FToolset; class FToolsetRegistry; }
namespace Cubeless::Registration
{
// Shutdown only: caller has disabled its call state on the game thread.
// Find hides disabled handlers, while SDK removal is name-only. Inspect the
// retained object's visibility briefly, restore it and require exact identity.
bool HasRetainedToolsetIdentity(UE::ToolsetRegistry::FToolsetRegistry& Registry,
    const TSharedPtr<UE::ToolsetRegistry::FToolset>& Retained);
}
