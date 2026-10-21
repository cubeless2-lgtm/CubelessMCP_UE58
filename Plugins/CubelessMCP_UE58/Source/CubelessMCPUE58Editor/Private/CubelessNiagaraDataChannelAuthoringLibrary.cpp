#include "CubelessNiagaraDataChannelAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraDataChannelAuthoringApi.h"

FString UCubelessNiagaraDataChannelAuthoringLibrary::ConfigureNiagaraDataChannelAsset(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraDataChannelAuthoring::ConfigureChannel(ParamsJson);
}

FString UCubelessNiagaraDataChannelAuthoringLibrary::CreateNiagaraNdcReaderScratchPadModule(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraDataChannelAuthoring::CreateReader(ParamsJson);
}

FString UCubelessNiagaraDataChannelAuthoringLibrary::AddNiagaraScratchPadDiFunctionCall(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraDataChannelAuthoring::AddFunctionCall(ParamsJson);
}

FString UCubelessNiagaraDataChannelAuthoringLibrary::AddNiagaraScratchPadDiVariadicInput(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraDataChannelAuthoring::AddVariadicInput(ParamsJson);
}

UCubelessNiagaraDataChannelVariadicJob* UCubelessNiagaraDataChannelAuthoringLibrary::StartNiagaraScratchPadDiVariadicInputJob(const FString& ParamsJson)
{ return Cubeless::UE58::NiagaraDataChannelAuthoring::StartVariadicInputJob(ParamsJson); }
FString UCubelessNiagaraDataChannelAuthoringLibrary::GetNiagaraScratchPadDiVariadicInputJobSnapshot(UCubelessNiagaraDataChannelVariadicJob* Job)
{ return Cubeless::UE58::NiagaraDataChannelAuthoring::VariadicInputJobSnapshot(Job); }
bool UCubelessNiagaraDataChannelAuthoringLibrary::CancelNiagaraScratchPadDiVariadicInputJob(UCubelessNiagaraDataChannelVariadicJob* Job,const FString& Reason)
{ return Cubeless::UE58::NiagaraDataChannelAuthoring::CancelVariadicInputJob(Job,Reason); }
