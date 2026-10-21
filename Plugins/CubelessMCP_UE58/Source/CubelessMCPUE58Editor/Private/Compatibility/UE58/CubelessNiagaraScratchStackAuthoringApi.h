#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::NiagaraScratchStackAuthoring
{
    FString AddModuleInput(const FString& ParamsJson);
    FString AddAssignmentModule(const FString& ParamsJson);
    FString WrapStackContext(const FString& ParamsJson);
}

class UCubelessNiagaraScratchStackAuthoringJob;
namespace Cubeless::UE58::NiagaraScratchStackAuthoringAsync
{
    struct FJobState;
    CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchStackAuthoringJob* StartModuleInput(const FString& ParamsJson);
    CUBELESSMCPUE58EDITOR_API FString SnapshotJson(const UCubelessNiagaraScratchStackAuthoringJob* Job);
    CUBELESSMCPUE58EDITOR_API bool Cancel(UCubelessNiagaraScratchStackAuthoringJob* Job, const FString& Reason);
    CUBELESSMCPUE58EDITOR_API void Shutdown(const FString& Reason);
}
