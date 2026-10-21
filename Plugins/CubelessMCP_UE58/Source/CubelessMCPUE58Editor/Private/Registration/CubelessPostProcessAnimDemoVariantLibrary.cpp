#include "Registration/CubelessPostProcessAnimDemoVariantLibrary.h"
#include "Compatibility/UE58/CubelessPostProcessAnimDemoVariantApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessPostProcessAnimDemoVariantLibrary::EnsurePostprocessAnimDemoVariant(const FString& SourceBlueprintName,const FString& SourceSkeletalMesh,const FString& VariantName,const FString& TargetRoot,const FString& TargetBlueprintNameJson,const FString& TargetSkeletalMeshJson,const FString& BoneName,const FString& RotationJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bOverwriteExisting,bool bCompile,bool bSave,bool bDryRun,bool bAllowNonSample)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::PostProcessAnimDemoVariant::Ensure(SourceBlueprintName,SourceSkeletalMesh,VariantName,TargetRoot,TargetBlueprintNameJson,TargetSkeletalMeshJson,BoneName,RotationJson,GraphName,GraphId,GraphType,bReplaceExisting,bOverwriteExisting,bCompile,bSave,bDryRun,bAllowNonSample),TEXT("ensure_postprocess_anim_demo_variant"));
}
