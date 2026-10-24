#pragma once

#include "CoreMinimal.h"

namespace Cubeless::Registration
{
// Two fixed existing Python classes only. No tool mapping or domain dispatch is owned here.
FString InstallLocalFacadeDefaults(UClass* ToolsetClass);
FString VerifyLocalFacadeDefaults(UClass* ToolsetClass);
FString RemoveLocalFacadeDefaults(UClass* ToolsetClass, bool RestoreOriginal = true);
void ShutdownLocalFacadeDefaults();
}
