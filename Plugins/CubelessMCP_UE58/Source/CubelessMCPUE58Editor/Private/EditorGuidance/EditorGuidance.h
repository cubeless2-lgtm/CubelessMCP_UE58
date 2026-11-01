#pragma once
#include "CoreMinimal.h"

// Editor UI service. No MCP, ToolsetRegistry, Python, transport or reflected API dependency.
namespace Cubeless::EditorGuidance
{
    FString Request(const FString& Action, const FString& Arguments);
    void Shutdown();
}
