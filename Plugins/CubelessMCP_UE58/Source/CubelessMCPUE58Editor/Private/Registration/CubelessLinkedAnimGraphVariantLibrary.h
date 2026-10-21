#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessLinkedAnimGraphVariantLibrary.generated.h"
UCLASS(meta=(AIIgnore))
class UCubelessLinkedAnimGraphVariantLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_linked_anim_graph_sample_variant"))
    static FString EnsureLinkedAnimGraphSampleVariant(FString SourceAnimBPPath,FString LinkedAnimBPPath,FString TargetAnimBPPath,FString TargetRoot,FString VariantName,FString InsertMode,bool bAllowExistingTargetReuse,bool bOverwriteExisting,bool bReplaceExistingLinks,bool bRequireInputPosePin,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly,FString PrivateGraphContextJson);
};
