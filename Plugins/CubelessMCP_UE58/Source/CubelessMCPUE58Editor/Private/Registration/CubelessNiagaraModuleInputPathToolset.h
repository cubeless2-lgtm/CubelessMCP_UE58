#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace Cubeless::Registration
{
    bool InstallNiagaraModuleInputPathToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyNiagaraModuleInputPathToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool RemoveNiagaraModuleInputPathToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    // Module teardown: only the captured original subsystem and exact retained handlers.
    void ShutdownNiagaraModuleInputPathToolset();
}
