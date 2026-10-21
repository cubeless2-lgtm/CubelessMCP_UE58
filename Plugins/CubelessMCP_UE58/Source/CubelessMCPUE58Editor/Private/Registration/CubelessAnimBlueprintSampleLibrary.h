#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimBlueprintSampleLibrary.generated.h"
UCLASS()
class UCubelessAnimBlueprintSampleLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="create_anim_blueprint_sample"))
    static FString CreateAnimBlueprintSample(const FString& TargetAnimBPPath,const FString& SkeletonPath,const FString& SkeletalMeshPath,const FString& PreviewSkeletalMeshPath,const FString& ParentClassPath,bool bOverwriteExisting,bool bCompile,bool bSaveAsset,bool bDryRun,bool bValidateOnly);
};
