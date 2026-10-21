#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"

namespace Cubeless::UE58::ParticleReceiver
{
// Finite evidence only: no selectors, asset/native callbacks, node creation,
// class/module loading, object/CDO creation, compilation or saving.
Function::FResult InspectNativeSignatures();
}
