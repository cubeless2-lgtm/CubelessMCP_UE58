#include "Registration/CubelessBlendSpaceVariantLibrary.h"
#include "Compatibility/UE58/CubelessBlendSpaceVariantApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlendSpaceVariantLibrary::EnsureBlendSpaceSampleVariant(const FString& SourceBlendSpace,const FString& VariantName,const FString& TargetRoot,const FString& TargetBlendSpace,const FString& AxisEditsJson,const FString& SampleEditsJson,const FString& AddSamplesJson,bool bOverwriteExisting,bool bSave,bool bDryRun,bool bValidateOnly,bool bAllowNonSample,bool bForceResample)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlendSpaceSampleVariant::Ensure(SourceBlendSpace,VariantName,TargetRoot,TargetBlendSpace,AxisEditsJson,SampleEditsJson,AddSamplesJson,bOverwriteExisting,bSave,bDryRun,bValidateOnly,bAllowNonSample,bForceResample),TEXT("ensure_blendspace_sample_variant"));
}
