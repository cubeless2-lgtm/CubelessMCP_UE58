#include "Registration/CubelessAnimGraphModifyCurveDemoLibrary.h"
#include "Compatibility/UE58/CubelessAnimGraphModifyCurveDemoApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessAnimGraphModifyCurveDemoLibrary::EnsureAnimGraphModifyCurveDemo(const FString& BlueprintName,const FString& CurveValuesJson,double Alpha,const FString& ApplyMode,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::AnimGraphModifyCurveDemo::EnsureModifyCurveDemo(BlueprintName,CurveValuesJson,Alpha,ApplyMode,GraphName,GraphId,GraphType,bReplaceExisting,bAllowNonSample),TEXT("ensure_anim_graph_modify_curve_demo"));
}
