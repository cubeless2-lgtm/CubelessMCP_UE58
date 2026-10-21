#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::NiagaraCustomHlslConversion
{
    FString ReplaceHlsl(const FString& ParamsJson);
    FString ApplyGraphRecipe(const FString& ParamsJson);
}

class UCubelessNiagaraHlslConversionJob;
namespace Cubeless::UE58::NiagaraCustomHlslConversionAsync
{
    struct FJobState;
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraHlslConversionJob* StartReplace(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraHlslConversionJob* StartRecipe(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraHlslConversionJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraHlslConversionJob* Job, const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
}
