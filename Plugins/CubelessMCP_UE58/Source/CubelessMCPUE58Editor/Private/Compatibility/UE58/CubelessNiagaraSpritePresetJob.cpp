#include "Compatibility/UE58/CubelessNiagaraSpritePresetJob.h"
#include "Compatibility/UE58/CubelessNiagaraSpritePresetAsyncApi.h"
FString UCubelessNiagaraSpritePresetJob::GetSnapshotJson() const { return IsInGameThread()?SnapshotJson:FString(); }
bool UCubelessNiagaraSpritePresetJob::CancelJob(const FString& Reason) { return Cubeless::UE58::NiagaraSpritePresetAsync::Cancel(this,Reason); }
void UCubelessNiagaraSpritePresetJob::BeginDestroy()
{
    if(!bIsTerminal&&IsInGameThread()) { Cubeless::UE58::NiagaraSpritePresetAsync::Cancel(this,TEXT("Transient Niagara Sprite preset job destroyed before terminal")); }
    Super::BeginDestroy();
}
