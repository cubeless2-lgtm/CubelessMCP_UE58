#include "CubelessNiagaraResourceBindingLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraResourceBindingApi.h"

FString UCubelessNiagaraResourceBindingLibrary::SetNiagaraRenderTarget2DModuleInput(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraResourceBindings::SetRenderTarget2D(ParamsJson);}
FString UCubelessNiagaraResourceBindingLibrary::SetNiagaraTexture2DModuleInput(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraResourceBindings::SetTexture2D(ParamsJson);}
FString UCubelessNiagaraResourceBindingLibrary::SetNiagaraScratchPadFunctionInputDefault(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraResourceBindings::SetScratchDefault(ParamsJson);}
FString UCubelessNiagaraResourceBindingLibrary::LinkNiagaraScratchPadPinToUserParameter(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraResourceBindings::LinkScratchUser(ParamsJson);}
