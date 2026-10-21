#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimGraphTrailDemoLibrary.generated.h"
UCLASS()
class UCubelessAnimGraphTrailDemoLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_anim_graph_trail_demo"))
    static FString EnsureAnimGraphTrailDemo(const FString& BlueprintName,const FString& TrailBone,const FString& BaseJoint,int64 ChainLength,const FString& ChainBoneAxis,double Alpha,const FString& FakeVelocityJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample,bool bInvertChainBoneAxis,bool bReorientParentToChild,bool bActorSpaceFakeVelocity,double RelaxationSpeedScale,bool bLimitStretch,double StretchLimit,double MaxDeltaTime);
};
