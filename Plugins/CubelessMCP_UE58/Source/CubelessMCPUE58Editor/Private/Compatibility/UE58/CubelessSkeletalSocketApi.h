#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::SkeletalSocket
{
Variable::FResult Add(const FString& SkeletonPath,const FString& SocketName,const FString& BoneName,const FString& LocationJson,const FString& RotationJson,const FString& ScaleJson,bool bSave);
Variable::FResult Set(const FString& SkeletonPath,const FString& SocketName,const FString& BoneName,const FString& NewSocketName,const FString& LocationJson,const FString& RotationJson,const FString& ScaleJson,bool bSave);
Variable::FResult Remove(const FString& SkeletonPath,const FString& SocketName,bool bSave);
}
