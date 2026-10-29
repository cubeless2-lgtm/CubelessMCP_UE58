#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraParticlePositions
{
// Editor inspection only: an already-paused solo instance with a completed tick.
// Does not change pause/mode, tick, wait, finalize, save or read back GPU data.
FString Inspect(const FString& ComponentPath,int32 EmitterIndex,int32 MaxParticles);
}
