#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimGraphAuthoringLibrary.generated.h"
UCLASS()
class UCubelessAnimGraphAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="set_anim_graph_rigidbody_settings"))
    static FString SetAnimGraphRigidbodySettings(const FString& BlueprintName,const FString& NodeId,const FString& AlphaJson,const FString& ExternalForceJson,const FString& SimulationSpace,const FString& EnableWorldGeometryJson,bool bAllowNonSample,const FString& GraphName,const FString& GraphId,const FString& GraphType);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_anim_graph_input_pose_passthrough"))
    static FString EnsureAnimGraphInputPosePassthrough(const FString& BlueprintName,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,const FString& InputNodePositionJson,bool bAllowNonSample);
};
