#pragma once
#include "CoreMinimal.h"
class UCubelessNiagaraSpritePresetJob;
namespace Cubeless::UE58::NiagaraSpritePresetAsync
{
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraSpritePresetJob* Start(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraSpritePresetJob* Job, const FString& Reason);
    // ModuleShutdown must call this before registration/converter teardown. No save/undo/compiler cancellation.
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
    // Existing synchronous GU forwarder must use this before any creation/SDK dispatch.
    CUBELESSMCPUE58EDITOR_API FString RejectSynchronous(const FString& ParamsJson);
}
