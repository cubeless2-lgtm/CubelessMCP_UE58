#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace Cubeless::Registration
{
    bool InstallNiagaraCleanupToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyNiagaraCleanupToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool RemoveNiagaraCleanupToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    // Module teardown: only the captured original subsystem and exact retained handlers.
    void ShutdownNiagaraCleanupToolset();
}
