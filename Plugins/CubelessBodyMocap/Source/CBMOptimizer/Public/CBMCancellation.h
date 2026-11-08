#pragma once
#include "CoreMinimal.h"
namespace Cubeless { CBMOPTIMIZER_API void SetOptimizerCancellation(TFunction<bool()> Callback);CBMOPTIMIZER_API bool OptimizerCancelled(); }
