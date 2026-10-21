#include "Compatibility/UE58/CubelessVolumeTextureJob.h"
#include "Compatibility/UE58/CubelessVolumeTextureApi.h"
FString UCubelessVolumeTextureJob::GetSnapshotJson() const { return IsInGameThread()?SnapshotJson:FString(); }
bool UCubelessVolumeTextureJob::CancelJob(const FString& Reason) { return Cubeless::UE58::VolumeTexture::Cancel(this,Reason); }
void UCubelessVolumeTextureJob::BeginDestroy()
{
    if(!bIsTerminal&&IsInGameThread()) { Cubeless::UE58::VolumeTexture::Cancel(this,TEXT("Transient Editor texture job destroyed before terminal")); }
    Super::BeginDestroy();
}
