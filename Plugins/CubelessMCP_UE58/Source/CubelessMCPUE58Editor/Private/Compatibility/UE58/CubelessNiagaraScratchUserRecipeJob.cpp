#include "CubelessNiagaraScratchUserRecipeJob.h"
#include "Compatibility/UE58/CubelessNiagaraScratchUserRecipeAsyncApi.h"
FString UCubelessNiagaraScratchUserRecipeJob::GetSnapshotJson() const{return Cubeless::UE58::NiagaraScratchUserRecipeAsync::SnapshotJson(this);}
bool UCubelessNiagaraScratchUserRecipeJob::CancelJob(const FString& Reason){return Cubeless::UE58::NiagaraScratchUserRecipeAsync::Cancel(this,Reason);}
void UCubelessNiagaraScratchUserRecipeJob::BeginDestroy(){Cubeless::UE58::NiagaraScratchUserRecipeAsync::Cancel(this,TEXT("HN leaf Job destroyed before terminal; no asset replay."));Super::BeginDestroy();}
