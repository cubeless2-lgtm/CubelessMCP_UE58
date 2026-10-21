#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimGraphModifyCurveDemoLibrary.generated.h"
UCLASS()
class UCubelessAnimGraphModifyCurveDemoLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_anim_graph_modify_curve_demo"))
    static FString EnsureAnimGraphModifyCurveDemo(const FString& BlueprintName,const FString& CurveValuesJson,double Alpha,const FString& ApplyMode,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample);
};
