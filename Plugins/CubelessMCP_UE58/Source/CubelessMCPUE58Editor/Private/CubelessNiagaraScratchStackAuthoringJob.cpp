#include "CubelessNiagaraScratchStackAuthoringJob.h"
#include "Compatibility/UE58/CubelessNiagaraScratchStackAuthoringApi.h"
FString UCubelessNiagaraScratchStackAuthoringJob::GetSnapshotJson() const
{
    return Cubeless::UE58::NiagaraScratchStackAuthoringAsync::SnapshotJson(this);
}
bool UCubelessNiagaraScratchStackAuthoringJob::CancelJob(const FString& Reason)
{
    return Cubeless::UE58::NiagaraScratchStackAuthoringAsync::Cancel(this,Reason);
}
void UCubelessNiagaraScratchStackAuthoringJob::BeginDestroy()
{
    Cubeless::UE58::NiagaraScratchStackAuthoringAsync::Cancel(this,TEXT("scratch_stack_authoring_job_begin_destroy"));
    Super::BeginDestroy();
}
