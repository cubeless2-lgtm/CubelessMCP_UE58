#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimationAssetMetadataLibrary.generated.h"
UCLASS()
class UCubelessAnimationAssetMetadataLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, meta=(AIIgnore,ScriptName="inspect_animation_asset_metadata_guarded"))
    static FString InspectAnimationAssetMetadataGuarded(const FString& AssetPath, bool bIncludeNotifies, bool bIncludeNotifyDetails, bool bIncludeCurves, bool bIncludeCurveKeys, bool bIncludeSyncMarkers, bool bIncludeMontageSections, bool bIncludeMontageSlots, bool bIncludeMetadata, int64 MaxNotifies, int64 MaxCurves, int64 MaxCurveKeysPerCurve, int64 MaxMarkers, int64 MaxSections, int64 MaxSlots, int64 MaxSegmentsPerSlot, bool bRequireSupportedClass);
};
