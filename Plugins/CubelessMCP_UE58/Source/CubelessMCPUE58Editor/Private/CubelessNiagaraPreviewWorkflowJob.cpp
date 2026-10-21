#include "CubelessNiagaraPreviewWorkflowJob.h"
#include "Compatibility/UE58/CubelessNiagaraPreviewWorkflowApi.h"
namespace Cubeless::UE58::NiagaraPreviewWorkflows { FString ReadRetainedSnapshot(const UCubelessNiagaraPreviewWorkflowJob* Job); }
FString UCubelessNiagaraPreviewWorkflowJob::GetSnapshotJson() const { FString Retained=Cubeless::UE58::NiagaraPreviewWorkflows::ReadRetainedSnapshot(this);return Retained.IsEmpty()?SnapshotJson:Retained; }
bool UCubelessNiagaraPreviewWorkflowJob::CancelJob(const FString& Reason) { return Cubeless::UE58::NiagaraPreviewWorkflows::Cancel(this,Reason); }
void UCubelessNiagaraPreviewWorkflowJob::BeginDestroy() { if(!bIsTerminal)Cubeless::UE58::NiagaraPreviewWorkflows::Cancel(this,TEXT("job_begin_destroy"));Super::BeginDestroy(); }
