#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::SkeletalSimulation
{
Variable::FResult CreateFixture(const FString& SkeletalMeshPath, const FString& ActorName, const FString& BoneName1,
    const FString& BoneName2, const FString& WorldContext, const FString& LocationJson, const FString& ConstraintName,
    double Swing1, double Swing2, double Twist, bool bDisableCollision, bool bSimulateAllBodies, bool bDryRun);
Variable::FResult InspectFixture(const FString& ActorName, const FString& WorldContext, const FString& ConstraintName);
}
