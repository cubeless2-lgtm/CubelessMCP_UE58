#include "Compatibility/UE58/CubelessNiagaraGraphCleanupJob.h"
#include "Compatibility/UE58/CubelessNiagaraGraphCleanupAsyncApi.h"
#include "CoreGlobals.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
FString UCubelessNiagaraGraphCleanupJob::GetSnapshotJson() const
{
    if(!IsInGameThread())return {};
    const FString Retained=Cubeless::UE58::NiagaraGraphCleanupAsync::SnapshotJson(this);
    if(!Retained.IsEmpty())return Retained;
    if(GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())return {};
    return SnapshotJson;
}
bool UCubelessNiagaraGraphCleanupJob::CancelJob(const FString& Reason)
{return Cubeless::UE58::NiagaraGraphCleanupAsync::Cancel(this,Reason);}
void UCubelessNiagaraGraphCleanupJob::BeginDestroy()
{
    if(IsInGameThread())Cubeless::UE58::NiagaraGraphCleanupAsync::Cancel(this,TEXT("Transient graph cleanup job destroyed before terminal"));
    Super::BeginDestroy();
}
