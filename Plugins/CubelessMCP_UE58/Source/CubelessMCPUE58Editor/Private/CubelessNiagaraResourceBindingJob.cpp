#include "CubelessNiagaraResourceBindingJob.h"
#include "Compatibility/UE58/CubelessNiagaraResourceBindingApi.h"
FString UCubelessNiagaraResourceBindingJob::GetSnapshotJson() const
{
    return Cubeless::UE58::NiagaraResourceBindingAsync::SnapshotJson(this);
}
bool UCubelessNiagaraResourceBindingJob::CancelJob(const FString& Reason)
{
    return Cubeless::UE58::NiagaraResourceBindingAsync::Cancel(this,Reason);
}
void UCubelessNiagaraResourceBindingJob::BeginDestroy()
{
    Cubeless::UE58::NiagaraResourceBindingAsync::Cancel(this,TEXT("hlsl_conversion_job_begin_destroy"));
    Super::BeginDestroy();
}
