#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimGraphSecondaryMotionLibrary.generated.h"
UCLASS()
class UCubelessAnimGraphSecondaryMotionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_anim_graph_secondary_motion_chain"))
    static FString EnsureAnimGraphSecondaryMotionChain(const FString& BlueprintName,const FString& DynamicsType,const FString& RootBone,const FString& ChainEndBone,double Alpha,const FString& GravityJson,double AngularLimitDegrees,const FString& BodyExtentsJson,double LinearDamping,double AngularDamping,double KawaiiDamping,double KawaiiStiffness,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample,bool bCompile,bool bSave);
};
