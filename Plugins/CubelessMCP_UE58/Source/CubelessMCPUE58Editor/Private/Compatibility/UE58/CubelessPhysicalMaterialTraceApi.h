#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::PhysicalMaterialTrace
{
// Engine-only operation. JSON vector carriers belong to the hidden ABI, not the public schema.
Variable::FResult Trace(const FString& StartJson, const FString& EndJson, const FString& TraceChannel,
    bool bTraceComplex, bool bReturnPhysicalMaterial);
}
