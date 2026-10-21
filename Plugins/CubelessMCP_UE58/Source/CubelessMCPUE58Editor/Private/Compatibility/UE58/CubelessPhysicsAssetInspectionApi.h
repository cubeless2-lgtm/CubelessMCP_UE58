#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::PhysicsAssetInspection
{
Variable::FResult Inspect(const FString& PhysicsAssetPath,const FString& SkeletalMeshPath,bool bIncludeBodies,bool bIncludeConstraints,bool bIncludeProfiles,bool bIncludeSolverSettings,bool bIncludeCollisionSummary,int64 MaxBodies,int64 MaxConstraints,int64 MaxProfilesPerItem,int64 MaxShapesPerBody,bool bRequireSupportedClass);
}
