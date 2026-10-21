#include "CubelessMaterialPostprocessJob.h"
#include "Compatibility/UE58/CubelessMaterialPostprocessAsyncApi.h"
FString UCubelessMaterialPostprocessJob::GetSnapshotJson()const
{ return Cubeless::UE58::MaterialPostprocessAsync::SnapshotJson(this); }
bool UCubelessMaterialPostprocessJob::CancelJob(const FString& Reason)
{ return Cubeless::UE58::MaterialPostprocessAsync::Cancel(this,Reason); }
void UCubelessMaterialPostprocessJob::BeginDestroy()
{
    Cubeless::UE58::MaterialPostprocessAsync::Cancel(this,TEXT("material_postprocess_job_begin_destroy"));
    Super::BeginDestroy();
}
