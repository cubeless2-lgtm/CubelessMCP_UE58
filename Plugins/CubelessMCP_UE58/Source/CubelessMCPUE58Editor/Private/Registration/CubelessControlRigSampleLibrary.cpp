#include "Registration/CubelessControlRigSampleLibrary.h"
#include "Compatibility/UE58/CubelessControlRigSampleApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessControlRigSampleLibrary::EnsureAnimGraphControlRigNodeSample(const FString& SourceAnimBPPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bReplaceExistingLinks,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ControlRigSample::EnsureAnimGraphNodeSample(SourceAnimBPPath,ControlRigAssetPath,ControlRigClassPath,TargetAnimBPPath,TargetRoot,VariantName,bAllowExistingTargetReuse,bOverwriteExisting,bReplaceExistingLinks,bCompile,bSaveAsset,bDryRun,bValidateOnly),TEXT("ensure_anim_graph_controlrig_node_sample"));
}
FString UCubelessControlRigSampleLibrary::EnsureControlRigSampleAsset(const FString& TargetControlRigPath,const FString& SkeletalMeshPath,const FString& SkeletonPath,const FString& PreviewSkeletalMeshPath,const FString& SetupMode,const FString& CurvesJson,const FString& NullsJson,const FString& ControlsJson,const FString& UnitNodesJson,int64 MaxDirectCurves,int64 MaxDirectNulls,int64 MaxDirectControls,int64 MaxUnitNodes,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bModularRig,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ControlRigSample::EnsureSampleAsset(TargetControlRigPath,SkeletalMeshPath,SkeletonPath,PreviewSkeletalMeshPath,SetupMode,CurvesJson,NullsJson,ControlsJson,UnitNodesJson,MaxDirectCurves,MaxDirectNulls,MaxDirectControls,MaxUnitNodes,bAllowExistingTargetReuse,bOverwriteExisting,bModularRig,bCompile,bSaveAsset,bDryRun,bValidateOnly),TEXT("ensure_controlrig_sample_asset"));
}
