#include "CubelessNiagaraSpritePresetLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraSpritePresetApi.h"

FString UCubelessNiagaraSpritePresetLibrary::CreateSpriteNiagaraPresetSystem(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraSpritePreset::CreatePreset(ParamsJson);
}
