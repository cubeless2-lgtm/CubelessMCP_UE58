#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPhysicsAssetInspectionLibrary.generated.h"
UCLASS()
class UCubelessPhysicsAssetInspectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="inspect_physics_asset_constraints_guarded"))
    static FString InspectPhysicsAssetConstraintsGuarded(const FString& PhysicsAssetPath,const FString& SkeletalMeshPath,bool bIncludeBodies,bool bIncludeConstraints,bool bIncludeProfiles,bool bIncludeSolverSettings,bool bIncludeCollisionSummary,int64 MaxBodies,int64 MaxConstraints,int64 MaxProfilesPerItem,int64 MaxShapesPerBody,bool bRequireSupportedClass);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="prepare_physics_asset_inspection_toolset"))
    static FString PreparePhysicsAssetInspectionToolset(UClass* Class);
};
