#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace Cubeless::Registration
{
    bool InstallAnimStateMachineVariantToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyAnimStateMachineVariantToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool RemoveAnimStateMachineVariantToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    // Module teardown: only the captured original subsystem and exact retained handlers.
    void ShutdownAnimStateMachineVariantToolset();
}
