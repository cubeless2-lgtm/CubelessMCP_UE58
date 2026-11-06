#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::EditorData
{
FString Blueprint(UObject* Object);
FString World(bool bIncludePartitionActors,int32 ActorOffset,int32 ActorLimit);
FString AddActorsToDataLayer(const TArray<FString>& ActorPaths,const FString& InstanceName,bool Execute,bool Save);
FString Mesh(UObject* Object);
FString Component(UObject* Object);
FString Widget(UObject* Object);
FString PCG(UObject* Object);
FString Performance();
FString RefreshActor(UObject* Object,bool Execute);
FString RefreshComponent(UObject* Object,bool Execute);
FString PackageDirty(UObject* Object,bool Dirty,bool AllowClear);
FString Spawn(UClass* Class,const FString& Name,const FTransform& Transform,bool Execute);
}
