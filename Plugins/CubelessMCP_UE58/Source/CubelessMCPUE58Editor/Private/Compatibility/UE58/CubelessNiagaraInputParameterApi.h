#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::NiagaraInputParameters
{
FString InspectRapidIteration(const FString& ParamsJson);
FString SetRapidIteration(const FString& ParamsJson);
FString SetStaticSwitch(const FString& ParamsJson);
FString InspectModuleInputs(const FString& ParamsJson);
FString InspectDataInterfaces(const FString& ParamsJson);
FString SetInputValue(const FString& ParamsJson);
FString CreateInputOverride(const FString& ParamsJson);
}

class UCubelessNiagaraInputParameterJob;
namespace Cubeless::UE58::NiagaraInputParameterAsync
{
    struct FJobState;
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraInputParameterJob* StartStaticSwitch(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraInputParameterJob* StartInputValue(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraInputParameterJob* StartInputOverride(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraInputParameterJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraInputParameterJob* Job, const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
}
