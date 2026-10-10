#include "Registration/CubelessFunctionLibrary.h"
#include "Operations/CubelessFunctionOperations.h"
using namespace Cubeless;
FString UCubelessFunctionLibrary::ResolveBlueprintGraph(const FString& B, const FString& N, const FString& I, const FString& T, bool C)
{ return Operations::EncodeFunctionResult(UE58::Function::Resolve(B, N, I, T, C), TEXT("resolve_blueprint_graph")); }
FString UCubelessFunctionLibrary::AddBlueprintFunctionParameter(const FString& B, const FString& N, const FString& T,
    const FString& D, const FString& V, const FString& O, bool A, const FString& GN, const FString& GI, const FString& GT, bool C)
{ return Operations::EncodeFunctionResult(UE58::Function::AddParameter(B, N, T, D, V, O, A, GN, GI, GT, C), TEXT("add_blueprint_function_parameter")); }
FString UCubelessFunctionLibrary::AddBlueprintCallFunctionNode(const FString& B, const FString& C, const FString& N,
    const FString& D, const FString& P, const FString& GN, const FString& GI, const FString& GT, bool CG, bool L, bool E, bool W, bool DP, bool I)
{ return Operations::EncodeFunctionResult(UE58::Function::AddCall(B, C, N, D, P, GN, GI, GT, CG, L, E, W, DP, I), TEXT("add_blueprint_call_function_node")); }
