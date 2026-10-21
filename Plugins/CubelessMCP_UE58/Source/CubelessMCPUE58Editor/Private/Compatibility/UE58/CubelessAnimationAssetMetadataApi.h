#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::AnimationAssetMetadata
{
Variable::FResult Inspect(const FString& AssetPath, bool bIncludeNotifies, bool bIncludeNotifyDetails, bool bIncludeCurves, bool bIncludeCurveKeys, bool bIncludeSyncMarkers, bool bIncludeMontageSections, bool bIncludeMontageSlots, bool bIncludeMetadata, int64 MaxNotifies, int64 MaxCurves, int64 MaxCurveKeysPerCurve, int64 MaxMarkers, int64 MaxSections, int64 MaxSlots, int64 MaxSegmentsPerSlot, bool bRequireSupportedClass);
}
