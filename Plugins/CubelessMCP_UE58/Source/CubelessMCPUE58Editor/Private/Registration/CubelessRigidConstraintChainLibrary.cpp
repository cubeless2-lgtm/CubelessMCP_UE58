#include "Registration/CubelessRigidConstraintChainLibrary.h"
#include "Compatibility/UE58/CubelessRigidConstraintChainApi.h"
FString UCubelessRigidConstraintChainLibrary::EnsureBlueprintRigidConstraintChain(const FString& BlueprintPath, const FString& BodyComponentsJson,
        const FString& RootComponentName, const FString& ConstraintPrefix,
        double Swing1LimitDegrees, double Swing2LimitDegrees, double TwistLimitDegrees,
        double LinearDamping, double AngularDamping, double ProjectionLinearAlpha,
        double ProjectionAngularAlpha, double ProjectionLinearTolerance,
        double ProjectionAngularTolerance, const FString& RequiredRoot, bool bSave, bool bDryRun)
{
    return Cubeless::UE58::RigidConstraintChain::EnsureChain(BlueprintPath, BodyComponentsJson,
        RootComponentName, ConstraintPrefix, Swing1LimitDegrees, Swing2LimitDegrees, TwistLimitDegrees,
        LinearDamping, AngularDamping, ProjectionLinearAlpha, ProjectionAngularAlpha,
        ProjectionLinearTolerance, ProjectionAngularTolerance, RequiredRoot, bSave, bDryRun);
}
