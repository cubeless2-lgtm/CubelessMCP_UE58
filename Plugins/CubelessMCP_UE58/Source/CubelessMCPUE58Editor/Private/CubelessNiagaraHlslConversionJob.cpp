#include "CubelessNiagaraHlslConversionJob.h"
#include "Compatibility/UE58/CubelessNiagaraCustomHlslConversionApi.h"
FString UCubelessNiagaraHlslConversionJob::GetSnapshotJson() const
{
    return Cubeless::UE58::NiagaraCustomHlslConversionAsync::SnapshotJson(this);
}
bool UCubelessNiagaraHlslConversionJob::CancelJob(const FString& Reason)
{
    return Cubeless::UE58::NiagaraCustomHlslConversionAsync::Cancel(this,Reason);
}
void UCubelessNiagaraHlslConversionJob::BeginDestroy()
{
    Cubeless::UE58::NiagaraCustomHlslConversionAsync::Cancel(this,TEXT("hlsl_conversion_job_begin_destroy"));
    Super::BeginDestroy();
}
