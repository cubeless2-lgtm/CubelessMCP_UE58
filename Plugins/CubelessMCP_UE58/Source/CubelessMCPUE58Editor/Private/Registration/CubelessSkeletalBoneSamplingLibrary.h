#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessSkeletalBoneSamplingLibrary.generated.h"
UCLASS()
class UCubelessSkeletalBoneSamplingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="sample_skeletal_bones_in_sie"))
    static FString SampleSkeletalBonesInSie(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& BonesJson,const FString& SocketsJson,bool bPreferPIEWorld,bool bRequirePIEWorld);
};
