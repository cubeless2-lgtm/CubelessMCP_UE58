#pragma once
#include "CoreMinimal.h"

namespace Cubeless::Operations
{
FString ReadParticleReceiverPlan(const FString& GraphPath);
FString ImplementParticleCallbackInterface(const FString& GraphPath);
FString AddParticleDataEventNode(const FString& GraphPath, int32 X, int32 Y);
FString AddParticleDataArrayLengthNode(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
FString AddParticleDataArrayGetCopyNode(const FString& GraphPath, const FString& EventNodePath, int32 X, int32 Y);
FString AddBasicParticleDataBreakNode(const FString& GraphPath, int32 X, int32 Y);
}
