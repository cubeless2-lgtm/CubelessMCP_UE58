#include "CubelessNiagaraModuleInputsBatchJob.h"
#include "Compatibility/UE58/CubelessNiagaraModuleInputsBatchAsyncApi.h"
FString UCubelessNiagaraModuleInputsBatchJob::GetSnapshotJson() const
{
    return Cubeless::UE58::NiagaraModuleInputsBatchAsync::SnapshotJson(this);
}
bool UCubelessNiagaraModuleInputsBatchJob::CancelJob(const FString& Reason)
{
    return Cubeless::UE58::NiagaraModuleInputsBatchAsync::Cancel(this,Reason);
}
void UCubelessNiagaraModuleInputsBatchJob::BeginDestroy()
{
    Cubeless::UE58::NiagaraModuleInputsBatchAsync::Cancel(this,TEXT("module_inputs_batch_job_begin_destroy"));
    Super::BeginDestroy();
}
