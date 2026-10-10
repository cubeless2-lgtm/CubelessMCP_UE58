#include "Operations/CubelessEventOperations.h"
#include "Compatibility/UE58/CubelessEventApi.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
namespace Cubeless::Operations
{
namespace
{
FString Encode(const TSharedRef<FJsonObject>& Result)
{ FString Json; FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json)); return Json; }
}
FString InspectSavedActorParent(const FString& ClassPath) { return Encode(UE58::Event::InspectSavedActorParent(ClassPath)); }
FString AddBlueprintEventNode(const FString& GraphPath, const FString& EventName, int32 X, int32 Y)
{ return Encode(UE58::Event::AddOverride(GraphPath, EventName, X, Y)); }
FString AddBlueprintParentFunctionCall(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return Encode(UE58::Event::AddParentCall(GraphPath, EventNodePath, X, Y)); }
}
