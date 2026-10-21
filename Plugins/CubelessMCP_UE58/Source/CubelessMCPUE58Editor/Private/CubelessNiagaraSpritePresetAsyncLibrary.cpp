#include "CubelessNiagaraSpritePresetAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraSpritePresetAsyncApi.h"
UCubelessNiagaraSpritePresetJob* UCubelessNiagaraSpritePresetAsyncLibrary::StartSpritePresetJob(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraSpritePresetAsync::Start(ParamsJson);
}
