#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::EngineFunctionInspection
{
// Finite read-only evidence for the next capture compatibility slice.
// No selectors, native calls, asset loading or UObject/CDO creation.
Function::FResult InspectCaptureSignatures();
}
