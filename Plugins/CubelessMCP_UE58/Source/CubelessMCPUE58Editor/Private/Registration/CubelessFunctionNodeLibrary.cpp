#include "Registration/CubelessFunctionNodeLibrary.h"
#include "Compatibility/UE58/CubelessFunctionNodeApi.h"
#include "Operations/CubelessFunctionOperations.h"

FString UCubelessFunctionNodeLibrary::AddBlueprintFunctionNode(const FString& B, const FString& T,
    const FString& F, const FString& P, const FString& X, const FString& GN, const FString& GI,
    const FString& GT, bool C)
{
    return Cubeless::Operations::EncodeFunctionResult(
        Cubeless::UE58::FunctionNode::Add(B, T, F, P, X, GN, GI, GT, C), TEXT("add_blueprint_function_node"));
}
