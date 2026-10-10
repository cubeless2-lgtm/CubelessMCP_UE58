#include "Operations/CubelessGraphOperations.h"
#include "Compatibility/UE58/CubelessGraphApi.h"
#include "Compatibility/UE58/CubelessPinDefaultApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
namespace Cubeless::Operations
{
namespace
{
FString Encode(const TSharedRef<FJsonObject>& Value)
{ FString Json; FJsonSerializer::Serialize(Value, TJsonWriterFactory<>::Create(&Json)); return Json; }
}
FString ReadGraphNodeIdentities(const FString& GraphPath)
{ return Encode(UE58::Graph::ReadNodeIdentities(GraphPath)); }
FString ReadGraphMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter)
{ return Encode(UE58::Graph::ReadMemberVariablePlan(GraphPath, VariableName, bSetter)); }
FString ReadExternalGraphMemberVariablePlan(const FString& GraphPath, const FString& VariableName, bool bSetter, const FString& TargetClassPath)
{ return Encode(UE58::Graph::ReadExternalMemberVariablePlan(GraphPath, VariableName, bSetter, TargetClassPath)); }
FString SetBlueprintPinDefault(const FString& GraphPath, const FString& NodePath, const FString& PinName, const FString& ValueJson, const FString& Direction)
{ return EncodeFunctionResult(UE58::PinDefault::Set(GraphPath, NodePath, PinName, ValueJson, Direction), TEXT("set_blueprint_pin_default")); }
FString ReadGraphConnectionPlan(const FString& GraphPath, const FString& SourceNodePath,
    const FString& SourcePinName, const FString& TargetNodePath, const FString& TargetPinName)
{ return Encode(UE58::Graph::ReadConnectionPlan(GraphPath, SourceNodePath, SourcePinName, TargetNodePath, TargetPinName)); }
}
