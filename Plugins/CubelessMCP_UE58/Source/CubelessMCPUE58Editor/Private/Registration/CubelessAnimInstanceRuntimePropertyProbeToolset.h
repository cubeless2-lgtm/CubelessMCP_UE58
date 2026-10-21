#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace Cubeless::Registration
{
    bool InstallAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool RemoveAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    // Module teardown: only the captured original subsystem and exact retained handlers.
    void ShutdownAnimInstanceRuntimePropertyProbeToolset();
}
