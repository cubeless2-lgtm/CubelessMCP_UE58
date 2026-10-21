#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimGraphBoneDemoLibrary.generated.h"
UCLASS()
class UCubelessAnimGraphBoneDemoLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="ensure_anim_graph_modify_bone_demo"))
    static FString EnsureAnimGraphModifyBoneDemo(const FString& BlueprintName,const FString& BoneName,const FString& RotationJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bReplaceExisting,bool bAllowNonSample);
};
