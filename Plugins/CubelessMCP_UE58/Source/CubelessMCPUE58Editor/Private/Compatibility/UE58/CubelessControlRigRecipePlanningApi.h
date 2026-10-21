#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::ControlRigRecipePlanning
{
    // Original request object JSON, without a public {request:...} envelope. No asset writer/compile/save.
    FString ProfileSkeleton(const FString& RequestJson);
}
