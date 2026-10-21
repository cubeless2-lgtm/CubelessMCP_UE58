#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::SkeletalPhysicsAsset
{
Variable::FResult Create(const FString& SkeletalMeshPath,const FString& TargetPhysicsAssetPath,const FString& ExpectedBonesJson,
 const FString& KinematicRootBone,bool bRootKinematic,bool bDisableCollision,double Swing1LimitDegrees,double Swing2LimitDegrees,
 double TwistLimitDegrees,double LinearDamping,double AngularDamping,double TipMassScale,bool bOverwriteExisting,bool bSave,bool bDryRun);
}
