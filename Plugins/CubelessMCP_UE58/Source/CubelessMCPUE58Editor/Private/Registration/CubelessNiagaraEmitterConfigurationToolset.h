#pragma once
#include "CoreMinimal.h"
class FJsonObject;
namespace Cubeless::Registration
{
    bool InstallNiagaraEmitterConfigurationToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyNiagaraEmitterConfigurationToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool RemoveNiagaraEmitterConfigurationToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    // Module teardown: only the captured original subsystem and exact retained handlers.
    void ShutdownNiagaraEmitterConfigurationToolset();
}
