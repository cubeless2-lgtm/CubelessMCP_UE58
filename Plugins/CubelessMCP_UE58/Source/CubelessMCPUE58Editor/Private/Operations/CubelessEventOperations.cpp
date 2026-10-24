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
FString InspectSavedObjectParent(const FString& ClassPath) { return Encode(UE58::Event::InspectSavedObjectParent(ClassPath)); }
FString AddBlueprintEventInResolvedGraph(const FString& RequestJson) { return Encode(UE58::Event::AddResolved(RequestJson)); }
FString AddBlueprintEventNode(const FString& GraphPath, const FString& EventName, int32 X, int32 Y)
{ return Encode(UE58::Event::AddOverride(GraphPath, EventName, X, Y)); }
FString AddBlueprintParentFunctionCall(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return Encode(UE58::Event::AddParentCall(GraphPath, EventNodePath, X, Y)); }
FString AddBlueprintCustomEventNode(const FString& GraphPath, const FString& Name, int32 X, int32 Y, bool bCallInEditor, bool bHasCallInEditor)
{ return Encode(UE58::Event::AddCustom(GraphPath, Name, X, Y, bCallInEditor, bHasCallInEditor)); }
FString AddBlueprintInputKeyNode(const FString& GraphPath, const FString& Key, int32 X, int32 Y,
    bool bControl, bool bAlt, bool bShift, bool bCommand, bool bConsumeInput, bool bExecuteWhenPaused, bool bOverrideParentBinding)
{ return Encode(UE58::Event::AddInputKey(GraphPath, Key, X, Y, bControl, bAlt, bShift, bCommand, bConsumeInput, bExecuteWhenPaused, bOverrideParentBinding)); }
}
