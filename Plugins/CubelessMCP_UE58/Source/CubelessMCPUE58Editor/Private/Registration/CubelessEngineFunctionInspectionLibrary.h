#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessEngineFunctionInspectionLibrary.generated.h"

/** Finite internal Editor evidence adapter; never reference from game assets. */
UCLASS()
class UCubelessEngineFunctionInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Reads one hundred and forty-eight exact already-loaded Engine/Niagara/AssetRegistry functions plus Pawn.BaseEyeHeight
     * and ChildActorComponent.ChildActor, including six by-value Vector/Rotator
     * interpolation targets, four PrimitiveComponent physics-authoring targets and
     * two camera-facing math, four scene/camera/provider targets and four additional
     * scene location/scale targets, two Box extent/component bounds targets and five
     * activation/tick/visibility/absolute control targets, plus three PrimitiveComponent
     * collision/overlap/translucency and six NiagaraComponent FName setter/solo/reinitialize
     * targets and five Actor hidden/recent-render/relative-location/rotation targets and seven MID/LinearColor and four material assignment/mesh parameter graph targets plus exact MakeVector4 and Position/Vector4 Niagara array setters and exact RT2D creation/base render-target Niagara setter plus SetAsset and reflected ActorComponent Activate/Deactivate and finger trace/native HitResult break plus three SceneComponent world-unit-axis getters and ActorComponent tick prerequisite and CreateAssetData/GetAsset/GetDataTableRowNames plus GetActorOfClass/SetViewTargetWithBlend and GetComponentByClass/ComponentHasTag/GetComponentsByTag and exact SceneComponent.DoesSocketExist/GetSocketLocation plus SkinnedMeshComponent.GetParentBone and exact GetPathName/InverseTransformLocation/InverseTransformDirection/Divide_VectorFloat plus exact BlueprintMapLibrary Map_Clear/Map_Find/Map_Add as metadata only, plus exact GetAssetRegistry/MakeARFilter/GetAssets/GetAssetsByPackageName interface/filter/search metadata and PresetLoader String/Text normalization4 with native ERoundingMode/ESearchCase metadata and exact Name-to-string/Append2 metadata (raw flags remain unobserved until capture).
     * Returns exact parameter flags, schema pin types/directions, depth-one
     * array-inner and exact search set-element/interface-wrapper metadata, fixed Map placeholder key/value and terminal metadata, bounded enum values (including the new trace enum only up to 128 entries) and CPP default metadata.
     * No calls, nodes, asset loading, CDO creation,
     * compilation or saving. Not a Toolset.
     */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Diagnostics", meta=(AIIgnore))
    static FString InspectCaptureSignatures();
};
