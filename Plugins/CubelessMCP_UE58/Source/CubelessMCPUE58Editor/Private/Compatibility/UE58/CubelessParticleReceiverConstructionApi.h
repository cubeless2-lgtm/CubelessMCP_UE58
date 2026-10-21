#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::ParticleReceiver
{
Function::FResult ReadPlan(const FString& GraphPath);
Function::FResult ImplementInterface(const FString& GraphPath);
// Generic named interface adapter; first concrete native UMG event scope.
Function::FResult ImplementBlueprintInterface(const FString& BlueprintPath,const FString& InterfaceClass);
Function::FResult AddEvent(const FString& GraphPath, int32 X, int32 Y);
Function::FResult AddArrayLength(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
Function::FResult AddArrayGetCopy(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
Function::FResult AddBreak(const FString& GraphPath, int32 X, int32 Y);
}
