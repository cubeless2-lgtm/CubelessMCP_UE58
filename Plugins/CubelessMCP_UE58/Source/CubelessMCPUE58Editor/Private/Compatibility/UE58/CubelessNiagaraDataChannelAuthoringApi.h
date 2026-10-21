#pragma once
#include "CoreMinimal.h"

class UCubelessNiagaraDataChannelVariadicJob;

namespace Cubeless::UE58::NiagaraDataChannelAuthoring
{
FString ConfigureChannel(const FString& ParamsJson);
FString CreateReader(const FString& ParamsJson);
FString AddFunctionCall(const FString& ParamsJson);
FString AddVariadicInput(const FString& ParamsJson);
UCubelessNiagaraDataChannelVariadicJob* StartVariadicInputJob(const FString& ParamsJson);
FString VariadicInputJobSnapshot(UCubelessNiagaraDataChannelVariadicJob* Job);
bool CancelVariadicInputJob(UCubelessNiagaraDataChannelVariadicJob* Job,const FString& Reason);
void ShutdownVariadicInputJobs();
}
