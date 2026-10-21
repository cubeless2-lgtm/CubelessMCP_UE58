#include "Registration/CubelessStateTreeCreateLibrary.h"
#include "Compatibility/UE58/CubelessStateTreeCreateApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessStateTreeCreateLibrary::CreateStateTree(const FString& Name, const FString& PackagePath, const FString& SchemaType)
{
    return Cubeless::Operations::EncodeVariableResult(
        Cubeless::UE58::StateTreeCreate::Create(Name, PackagePath, SchemaType), TEXT("create_state_tree"));
}
