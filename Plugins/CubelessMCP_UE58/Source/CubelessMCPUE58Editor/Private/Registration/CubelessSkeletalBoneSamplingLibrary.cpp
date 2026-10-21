#include "Registration/CubelessSkeletalBoneSamplingLibrary.h"
#include "Compatibility/UE58/CubelessSkeletalBoneSamplingApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessSkeletalBoneSamplingLibrary::SampleSkeletalBonesInSie(const FString& ActorLabel,const FString& ActorName,const FString& ActorPath,const FString& ComponentName,const FString& BonesJson,const FString& SocketsJson,bool bPreferPIEWorld,bool bRequirePIEWorld)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::SkeletalBoneSampling::Sample(ActorLabel,ActorName,ActorPath,ComponentName,BonesJson,SocketsJson,bPreferPIEWorld,bRequirePIEWorld),TEXT("sample_skeletal_bones_in_sie"));
}
