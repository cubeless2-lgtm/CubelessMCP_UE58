#include "Registration/CubelessFoliageInstanceLibrary.h"
#include "Compatibility/UE58/CubelessFoliageInstancesApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessFoliageInstanceLibrary::ListFoliageTypes()
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::FoliageInstances::List(),TEXT("list_foliage_types"));
}
FString UCubelessFoliageInstanceLibrary::AddFoliageInstances(FString FoliageTypePath,FString TransformsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::FoliageInstances::Add(FoliageTypePath,TransformsJson),TEXT("add_foliage_instances"));
}
FString UCubelessFoliageInstanceLibrary::RemoveFoliageInstances(FString FoliageTypePath,FString InstanceIndicesJson,bool bRemoveAll,int64 MaxRemoveCount,FString PrivateContextJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::FoliageInstances::Remove(FoliageTypePath,InstanceIndicesJson,bRemoveAll,MaxRemoveCount,PrivateContextJson),TEXT("remove_foliage_instances"));
}
