#include "CubelessNiagaraEmitterEventAuthoringJob.h"
#include "Compatibility/UE58/CubelessNiagaraEmitterEventAuthoringAsyncApi.h"
FString UCubelessNiagaraEmitterEventAuthoringJob::GetSnapshotJson() const{return Cubeless::UE58::NiagaraEmitterEventAuthoringAsync::SnapshotJson(this);}
bool UCubelessNiagaraEmitterEventAuthoringJob::CancelJob(const FString& Reason){return Cubeless::UE58::NiagaraEmitterEventAuthoringAsync::Cancel(this,Reason);}
void UCubelessNiagaraEmitterEventAuthoringJob::BeginDestroy(){Cubeless::UE58::NiagaraEmitterEventAuthoringAsync::Cancel(this,TEXT("emitter_event_authoring_job_begin_destroy"));Super::BeginDestroy();}
