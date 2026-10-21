#include "CubelessNiagaraScratchStackAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraScratchStackAuthoringApi.h"
FString UCubelessNiagaraScratchStackAuthoringLibrary::AddNiagaraScratchPadModuleInput(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchStackAuthoring::AddModuleInput(ParamsJson);}
FString UCubelessNiagaraScratchStackAuthoringLibrary::AddNiagaraScratchPadAssignmentModuleToStack(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchStackAuthoring::AddAssignmentModule(ParamsJson);}
FString UCubelessNiagaraScratchStackAuthoringLibrary::WrapNiagaraScratchPadOutputWithStackContext(const FString& ParamsJson)
{return Cubeless::UE58::NiagaraScratchStackAuthoring::WrapStackContext(ParamsJson);}
