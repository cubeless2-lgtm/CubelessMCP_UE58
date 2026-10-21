#include "CubelessNiagaraHlslConversionAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraCustomHlslConversionApi.h"
UCubelessNiagaraHlslConversionJob* UCubelessNiagaraHlslConversionAsyncLibrary::StartReplaceNiagaraCustomHlslJob(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCustomHlslConversionAsync::StartReplace(ParamsJson);
}
UCubelessNiagaraHlslConversionJob* UCubelessNiagaraHlslConversionAsyncLibrary::StartApplyNiagaraScratchPadGraphRecipeJob(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraCustomHlslConversionAsync::StartRecipe(ParamsJson);
}
