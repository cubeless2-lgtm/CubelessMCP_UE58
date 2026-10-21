#include "Registration/CubelessLinkedAnimGraphVariantLibrary.h"
#include "Compatibility/UE58/CubelessLinkedAnimGraphVariantApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessLinkedAnimGraphVariantLibrary::EnsureLinkedAnimGraphSampleVariant(FString SourceAnimBPPath,FString LinkedAnimBPPath,FString TargetAnimBPPath,FString TargetRoot,FString VariantName,FString InsertMode,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bReplaceExistingLinks,bool bRequireInputPosePin,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,FString PrivateGraphContextJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::LinkedAnimGraphVariant::Ensure(SourceAnimBPPath,LinkedAnimBPPath,TargetAnimBPPath,TargetRoot,VariantName,InsertMode,bAllowExistingTargetReuse,bOverwriteExisting,bReplaceExistingLinks,bRequireInputPosePin,bCompile,bSaveAsset,bDryRun,bValidateOnly,PrivateGraphContextJson),TEXT("ensure_linked_anim_graph_sample_variant"));
}
