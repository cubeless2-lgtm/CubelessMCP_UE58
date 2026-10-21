#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::EditorLevelSession
{
Variable::FResult Open(const FString& ParamsJson);
Variable::FResult Status(const FString& ParamsJson);
Variable::FResult ClearDirty(const FString& ParamsJson);
Variable::FResult NewPreview(const FString& ParamsJson);
// Call once on the Editor game thread before module unload. It cancels only this namespace owned ticker.
void Shutdown();
}
