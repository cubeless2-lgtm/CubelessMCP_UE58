#include "Registration/CubelessStateTreeReadLibrary.h"
#include "Compatibility/UE58/CubelessStateTreeReadApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessStateTreeReadLibrary::ListStateTrees(const FString& Directory, bool bRecursive, int64 MaxResults, int64 Offset)
{
    return Cubeless::Operations::EncodeVariableResult(
        Cubeless::UE58::StateTreeRead::List(Directory, bRecursive, MaxResults, Offset), TEXT("list_state_trees"));
}

FString UCubelessStateTreeReadLibrary::InspectStateTree(const FString& AssetPath, int64 MaxStates)
{
    return Cubeless::Operations::EncodeVariableResult(
        Cubeless::UE58::StateTreeRead::Inspect(AssetPath, MaxStates), TEXT("inspect_state_tree"));
}
