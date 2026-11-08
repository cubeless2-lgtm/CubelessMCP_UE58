#pragma once
#include "CoreMinimal.h"
#include "NNERuntimeRunSync.h"
class UNNEModelData;
namespace Cubeless {
using FModelFactory=TUniqueFunction<TSharedPtr<UE::NNE::IModelInstanceRunSync>()>;
// Prepare on game thread; returned closure owns plain NNE models, never UObjects.
CBMSEGMENTATION_API FModelFactory PrepareModel(UNNEModelData* Data,bool SetShape=false,bool CPUOnly=false);
}
