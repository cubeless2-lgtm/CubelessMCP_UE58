#include "CubelessNiagaraFactoryBootstrapJob.h"
#include "Compatibility/UE58/CubelessNiagaraFactoryBootstrapApi.h"
FString UCubelessNiagaraFactoryBootstrapJob::GetSnapshotJson() const
{
    return Cubeless::UE58::NiagaraFactoryBootstrapAsync::SnapshotJson(this);
}
bool UCubelessNiagaraFactoryBootstrapJob::CancelJob(const FString& Reason)
{
    return Cubeless::UE58::NiagaraFactoryBootstrapAsync::Cancel(this,Reason);
}
void UCubelessNiagaraFactoryBootstrapJob::BeginDestroy()
{
    Cubeless::UE58::NiagaraFactoryBootstrapAsync::Cancel(this,TEXT("factory_job_begin_destroy"));
    Super::BeginDestroy();
}
