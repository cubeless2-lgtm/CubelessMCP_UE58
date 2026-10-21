#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessControlRigSampleLibrary.generated.h"
UCLASS()
class UCubelessControlRigSampleLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_anim_graph_controlrig_node_sample"))
    static FString EnsureAnimGraphControlRigNodeSample(const FString& SourceAnimBPPath,const FString& ControlRigAssetPath,const FString& ControlRigClassPath,const FString& TargetAnimBPPath,const FString& TargetRoot,const FString& VariantName,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bReplaceExistingLinks,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_controlrig_sample_asset"))
    static FString EnsureControlRigSampleAsset(const FString& TargetControlRigPath,const FString& SkeletalMeshPath,const FString& SkeletonPath,const FString& PreviewSkeletalMeshPath,const FString& SetupMode,const FString& CurvesJson,const FString& NullsJson,const FString& ControlsJson,const FString& UnitNodesJson,int64 MaxDirectCurves,int64 MaxDirectNulls,int64 MaxDirectControls,int64 MaxUnitNodes,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bModularRig,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly);
};
