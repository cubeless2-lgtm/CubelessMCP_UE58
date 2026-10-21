#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessSplineComponentPointsLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessSplineComponentPointsLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="set_spline_component_points"))
    static FString SetSplineComponentPoints(FString PointsJson,FString ActorLabel,FString ActorName,FString ActorTag,FString ActorLabelPrefix,FString ComponentName,FString ComponentTag,bool bSelectedOnly,bool bClosedLoop,FString PrivateContextJson);
};
