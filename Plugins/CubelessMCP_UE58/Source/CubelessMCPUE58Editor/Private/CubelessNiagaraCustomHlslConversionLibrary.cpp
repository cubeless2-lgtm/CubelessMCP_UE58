#include "CubelessNiagaraCustomHlslConversionLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraCustomHlslConversionApi.h"

FString UCubelessNiagaraCustomHlslConversionLibrary::ReplaceNiagaraCustomHlsl(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCustomHlslConversion::ReplaceHlsl(ParamsJson);
}

FString UCubelessNiagaraCustomHlslConversionLibrary::ApplyNiagaraScratchPadGraphRecipe(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCustomHlslConversion::ApplyGraphRecipe(ParamsJson);
}
