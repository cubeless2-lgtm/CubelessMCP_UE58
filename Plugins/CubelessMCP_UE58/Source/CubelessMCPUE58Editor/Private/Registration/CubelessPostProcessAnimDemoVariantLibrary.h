#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPostProcessAnimDemoVariantLibrary.generated.h"
UCLASS()
class UCubelessPostProcessAnimDemoVariantLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_postprocess_anim_demo_variant"))
    static FString EnsurePostprocessAnimDemoVariant(const FString& SourceBlueprintName,const FString& SourceSkeletalMesh,const FString& VariantName,const FString& TargetRoot,const FString& TargetBlueprintNameJson,const FString& TargetSkeletalMeshJson,const FString& BoneName,const FString& RotationJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bOverwriteExisting,bool bCompile,bool bSave,bool bDryRun,bool bAllowNonSample);
};
