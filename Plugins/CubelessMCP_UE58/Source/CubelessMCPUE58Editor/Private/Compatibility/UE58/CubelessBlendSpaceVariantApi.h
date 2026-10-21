#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::BlendSpaceSampleVariant
{
Variable::FResult Ensure(const FString& SourceBlendSpace,const FString& VariantName,const FString& TargetRoot,const FString& TargetBlendSpace,const FString& AxisEditsJson,const FString& SampleEditsJson,const FString& AddSamplesJson,bool bOverwriteExisting,bool bSave,bool bDryRun,bool bValidateOnly,bool bAllowNonSample,bool bForceResample);
}
