// The named API delivers the dedicated async Job. Synchronous/RawBatch calls reject before creation.
#include "Compatibility/UE58/CubelessNiagaraSpritePresetApi.h"
#include "Compatibility/UE58/CubelessNiagaraSpritePresetAsyncApi.h"
namespace Cubeless::UE58::NiagaraSpritePreset
{
FString CreatePreset(const FString& ParamsJson){return NiagaraSpritePresetAsync::RejectSynchronous(ParamsJson);}
}
