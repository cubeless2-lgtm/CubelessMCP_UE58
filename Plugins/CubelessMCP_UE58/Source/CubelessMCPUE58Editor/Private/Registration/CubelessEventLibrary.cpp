#include "Registration/CubelessEventLibrary.h"
#include "Operations/CubelessEventOperations.h"
FString UCubelessEventLibrary::InspectSavedActorParent(const FString& ClassPath)
{ return Cubeless::Operations::InspectSavedActorParent(ClassPath); }
FString UCubelessEventLibrary::AddBlueprintEventNode(const FString& GraphPath, const FString& EventName, int32 X, int32 Y)
{ return Cubeless::Operations::AddBlueprintEventNode(GraphPath, EventName, X, Y); }
FString UCubelessEventLibrary::AddBlueprintParentFunctionCall(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y)
{ return Cubeless::Operations::AddBlueprintParentFunctionCall(GraphPath, EventNodePath, X, Y); }
FString UCubelessEventLibrary::AddBlueprintCustomEventNode(const FString& GraphPath, const FString& Name, int32 X, int32 Y, bool bCallInEditor, bool bHasCallInEditor)
{ return Cubeless::Operations::AddBlueprintCustomEventNode(GraphPath, Name, X, Y, bCallInEditor, bHasCallInEditor); }
FString UCubelessEventLibrary::AddBlueprintInputKeyNode(const FString& GraphPath, const FString& Key, int32 X, int32 Y,
    bool bControl, bool bAlt, bool bShift, bool bCommand, bool bConsumeInput, bool bExecuteWhenPaused, bool bOverrideParentBinding)
{ return Cubeless::Operations::AddBlueprintInputKeyNode(GraphPath, Key, X, Y, bControl, bAlt, bShift, bCommand, bConsumeInput, bExecuteWhenPaused, bOverrideParentBinding); }
