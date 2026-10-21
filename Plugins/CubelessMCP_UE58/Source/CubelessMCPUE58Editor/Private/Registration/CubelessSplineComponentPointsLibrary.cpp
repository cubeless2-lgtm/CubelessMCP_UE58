#include "Registration/CubelessSplineComponentPointsLibrary.h"
#include "Compatibility/UE58/CubelessSplineComponentPointsApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessSplineComponentPointsLibrary::SetSplineComponentPoints(FString PointsJson,FString ActorLabel,FString ActorName,FString ActorTag,FString ActorLabelPrefix,FString ComponentName,FString ComponentTag,bool bSelectedOnly,bool bClosedLoop,FString PrivateContextJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SplineComponentPoints::Set(PointsJson,ActorLabel,ActorName,ActorTag,ActorLabelPrefix,ComponentName,ComponentTag,bSelectedOnly,bClosedLoop,PrivateContextJson),TEXT("set_spline_component_points"));
}
