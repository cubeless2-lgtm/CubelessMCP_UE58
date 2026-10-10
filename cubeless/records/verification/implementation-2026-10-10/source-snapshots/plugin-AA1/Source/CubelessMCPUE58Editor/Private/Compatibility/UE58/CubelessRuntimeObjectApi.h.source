#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"

namespace Cubeless::UE58::RuntimeObject
{
// Exact native runtime types only; does not load assets or instantiate a CDO.
bool IsSupportedClass(const UClass* Class);
UClass* FindSupportedClass(const FString& ExactPath);
bool IsSupportedPin(const FEdGraphPinType& Type);
}
