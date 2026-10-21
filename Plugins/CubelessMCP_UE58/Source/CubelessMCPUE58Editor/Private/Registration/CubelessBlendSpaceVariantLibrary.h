#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlendSpaceVariantLibrary.generated.h"
UCLASS()
class UCubelessBlendSpaceVariantLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_blendspace_sample_variant"))
    static FString EnsureBlendSpaceSampleVariant(const FString& SourceBlendSpace,const FString& VariantName,const FString& TargetRoot,const FString& TargetBlendSpace,const FString& AxisEditsJson,const FString& SampleEditsJson,const FString& AddSamplesJson,bool bOverwriteExisting,bool bSave,bool bDryRun,bool bValidateOnly,bool bAllowNonSample,bool bForceResample);
};
