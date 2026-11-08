#include "CBMCancellation.h"
namespace Cubeless { thread_local TFunction<bool()> OptimizerCancel;void SetOptimizerCancellation(TFunction<bool()> C){OptimizerCancel=MoveTemp(C);}bool OptimizerCancelled(){return OptimizerCancel&&OptimizerCancel();} }
