#include "Compatibility/UE58/CubelessNiagaraUserParameterJob.h"
#include "Compatibility/UE58/CubelessNiagaraUserParameterAsyncApi.h"
FString UCubelessNiagaraUserParameterJob::GetSnapshotJson() const { return IsInGameThread()?SnapshotJson:FString(); }
bool UCubelessNiagaraUserParameterJob::CancelJob(const FString& Reason) { return Cubeless::UE58::NiagaraUserParameterAsync::Cancel(this,Reason); }
void UCubelessNiagaraUserParameterJob::BeginDestroy()
{
    if(!bIsTerminal&&IsInGameThread()) { Cubeless::UE58::NiagaraUserParameterAsync::Cancel(this,TEXT("Transient Niagara User parameter job destroyed before terminal")); }
    Super::BeginDestroy();
}
