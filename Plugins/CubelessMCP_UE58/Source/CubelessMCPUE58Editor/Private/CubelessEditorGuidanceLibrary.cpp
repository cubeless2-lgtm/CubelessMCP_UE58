#include "CubelessEditorGuidanceLibrary.h"
#include "EditorGuidance/EditorGuidance.h"

FString UCubelessEditorGuidanceLibrary::EditorGuidanceRequest(const FString& Action, const FString& Arguments)
{
    return Cubeless::EditorGuidance::Request(Action, Arguments);
}
