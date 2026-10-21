#include "CubelessNiagaraInputParameterAsyncLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraInputParameterApi.h"
UCubelessNiagaraInputParameterJob* UCubelessNiagaraInputParameterAsyncLibrary::StartSetNiagaraModuleStaticSwitchJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraInputParameterAsync::StartStaticSwitch(ParamsJson);}
UCubelessNiagaraInputParameterJob* UCubelessNiagaraInputParameterAsyncLibrary::StartSetNiagaraModuleInputValueJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraInputParameterAsync::StartInputValue(ParamsJson);}
UCubelessNiagaraInputParameterJob* UCubelessNiagaraInputParameterAsyncLibrary::StartCreateNiagaraModuleInputOverrideJob(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraInputParameterAsync::StartInputOverride(ParamsJson);}
