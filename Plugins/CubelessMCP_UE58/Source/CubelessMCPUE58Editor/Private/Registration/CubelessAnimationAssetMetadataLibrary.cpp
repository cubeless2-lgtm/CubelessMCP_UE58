#include "Registration/CubelessAnimationAssetMetadataLibrary.h"
#include "Compatibility/UE58/CubelessAnimationAssetMetadataApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimationAssetMetadataLibrary::InspectAnimationAssetMetadataGuarded(const FString& AssetPath, bool bIncludeNotifies, bool bIncludeNotifyDetails, bool bIncludeCurves, bool bIncludeCurveKeys, bool bIncludeSyncMarkers, bool bIncludeMontageSections, bool bIncludeMontageSlots, bool bIncludeMetadata, int64 MaxNotifies, int64 MaxCurves, int64 MaxCurveKeysPerCurve, int64 MaxMarkers, int64 MaxSections, int64 MaxSlots, int64 MaxSegmentsPerSlot, bool bRequireSupportedClass)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimationAssetMetadata::Inspect(AssetPath,bIncludeNotifies,bIncludeNotifyDetails,bIncludeCurves,bIncludeCurveKeys,bIncludeSyncMarkers,bIncludeMontageSections,bIncludeMontageSlots,bIncludeMetadata,MaxNotifies,MaxCurves,MaxCurveKeysPerCurve,MaxMarkers,MaxSections,MaxSlots,MaxSegmentsPerSlot,bRequireSupportedClass),TEXT("inspect_animation_asset_metadata_guarded"));
}
