#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessSkeletalLibrary.generated.h"
/** Editor-only adapters for original skeleton-backed reads. */
UCLASS()
class UCubelessSkeletalLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="list_skeleton_bones"))
 static FString ListSkeletonBones(const FString& SkeletonPath,const FString& BoneNameContains,int32 MaxBones);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="list_skeletal_sockets"))
 static FString ListSkeletalSockets(const FString& SkeletonPath);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Registration",meta=(AIIgnore,ScriptName="prepare_skeletal_read_toolset"))
 static FString PrepareSkeletalReadToolset(UClass* ToolsetClass);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="add_skeletal_socket"))
 static FString AddSkeletalSocket(const FString& SkeletonPath,const FString& SocketName,const FString& BoneName,const FString& LocationJson,const FString& RotationJson,const FString& ScaleJson,bool bSave);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="set_skeletal_socket"))
 static FString SetSkeletalSocket(const FString& SkeletonPath,const FString& SocketName,const FString& BoneName,const FString& NewSocketName,const FString& LocationJson,const FString& RotationJson,const FString& ScaleJson,bool bSave);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="remove_skeletal_socket"))
 static FString RemoveSkeletalSocket(const FString& SkeletonPath,const FString& SocketName,bool bSave);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Registration",meta=(AIIgnore,ScriptName="prepare_skeletal_socket_toolset"))
 static FString PrepareSkeletalSocketToolset(UClass* ToolsetClass);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="set_bone_translation_retargeting"))
 static FString SetBoneTranslationRetargeting(const FString& SkeletonPath,const FString& BoneName,const FString& Mode,bool bRecursive,bool bSave);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="create_skeletal_physics_asset"))
 static FString CreateSkeletalPhysicsAsset(const FString& SkeletalMeshPath,const FString& TargetPhysicsAssetPath,const FString& ExpectedBonesJson,const FString& KinematicRootBone,bool bRootKinematic,bool bDisableCollision,double Swing1LimitDegrees,double Swing2LimitDegrees,double TwistLimitDegrees,double LinearDamping,double AngularDamping,double TipMassScale,bool bOverwriteExisting,bool bSave,bool bDryRun);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="create_skeletal_bone_constraint_fixture"))
 static FString CreateSkeletalBoneConstraintFixture(const FString& SkeletalMeshPath,const FString& ActorName,const FString& BoneName1,const FString& BoneName2,const FString& WorldContext,const FString& LocationJson,const FString& ConstraintComponentName,double Swing1LimitDegrees,double Swing2LimitDegrees,double TwistLimitDegrees,bool bDisableCollision,bool bSimulateAllBodies,bool bDryRun);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Skeletal",meta=(AIIgnore,ScriptName="inspect_skeletal_bone_constraint_fixture"))
 static FString InspectSkeletalBoneConstraintFixture(const FString& ActorName,const FString& WorldContext,const FString& ConstraintComponentName);
};
