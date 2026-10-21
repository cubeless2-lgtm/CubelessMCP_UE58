#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessRigidConstraintChainLibrary.generated.h"
UCLASS()
class UCubelessRigidConstraintChainLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="ensure_blueprint_rigid_constraint_chain"))
    static FString EnsureBlueprintRigidConstraintChain(const FString& BlueprintPath, const FString& BodyComponentsJson,
        const FString& RootComponentName, const FString& ConstraintPrefix,
        double Swing1LimitDegrees, double Swing2LimitDegrees, double TwistLimitDegrees,
        double LinearDamping, double AngularDamping, double ProjectionLinearAlpha,
        double ProjectionAngularAlpha, double ProjectionLinearTolerance,
        double ProjectionAngularTolerance, const FString& RequiredRoot, bool bSave, bool bDryRun);
};
