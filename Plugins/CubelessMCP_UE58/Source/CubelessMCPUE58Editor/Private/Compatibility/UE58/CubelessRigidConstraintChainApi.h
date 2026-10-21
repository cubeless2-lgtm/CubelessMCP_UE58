#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::RigidConstraintChain
{
FString EnsureChain(const FString& BlueprintPath, const FString& BodyComponentsJson,
        const FString& RootComponentName, const FString& ConstraintPrefix,
        double Swing1LimitDegrees, double Swing2LimitDegrees, double TwistLimitDegrees,
        double LinearDamping, double AngularDamping, double ProjectionLinearAlpha,
        double ProjectionAngularAlpha, double ProjectionLinearTolerance,
        double ProjectionAngularTolerance, const FString& RequiredRoot, bool bSave, bool bDryRun);
}
