#include "CubelessNiagaraSystemAnalysisLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraSystemAnalysisApi.h"
FString UCubelessNiagaraSystemAnalysisLibrary::InspectNiagaraSystemAnalysisMetadata(const FString& ParamsJson)
{
 return Cubeless::UE58::NiagaraSystemAnalysis::InspectMetadata(ParamsJson);
}
