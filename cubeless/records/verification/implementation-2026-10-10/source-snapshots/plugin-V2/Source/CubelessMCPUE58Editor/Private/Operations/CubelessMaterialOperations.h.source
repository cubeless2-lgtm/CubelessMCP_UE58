#pragma once

#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessMaterialApi.h"

namespace Cubeless::Operations
{
// Called only by the named reflection helpers. Durable journal ownership is in
// the public Python tools; no generic executor is exposed to MCP.
FString RunMaterialOperation(UE58::Material::EOperation Operation, const TSharedPtr<FJsonObject>& Params, int32 MaxGraphNodes);
}
