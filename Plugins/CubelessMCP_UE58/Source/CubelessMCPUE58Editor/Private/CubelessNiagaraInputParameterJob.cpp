#include "CubelessNiagaraInputParameterJob.h"
#include "Compatibility/UE58/CubelessNiagaraInputParameterApi.h"
FString UCubelessNiagaraInputParameterJob::GetSnapshotJson() const
{
    return Cubeless::UE58::NiagaraInputParameterAsync::SnapshotJson(this);
}
bool UCubelessNiagaraInputParameterJob::CancelJob(const FString& Reason)
{
    return Cubeless::UE58::NiagaraInputParameterAsync::Cancel(this,Reason);
}
void UCubelessNiagaraInputParameterJob::BeginDestroy()
{
    Cubeless::UE58::NiagaraInputParameterAsync::Cancel(this,TEXT("hlsl_conversion_job_begin_destroy"));
    Super::BeginDestroy();
}
