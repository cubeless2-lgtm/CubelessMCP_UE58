#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"

namespace Cubeless::UE58::RuntimeObject
{
// Exact native runtime types only; does not load assets or instantiate a CDO.
bool IsSupportedClass(const UClass* Class);
UClass* FindSupportedClass(const FString& ExactPath);
bool IsSupportedPin(const FEdGraphPinType& Type);
// Separate own scalar Class-constraint gate; does not widen object/call/array admission.
bool IsSupportedClassPin(const FEdGraphPinType& Type);
}
