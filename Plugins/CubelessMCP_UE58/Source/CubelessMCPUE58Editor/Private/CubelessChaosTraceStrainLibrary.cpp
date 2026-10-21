#include "CubelessChaosTraceStrainLibrary.h"
#include "Compatibility/UE58/CubelessChaosTraceStrainApi.h"

FString UCubelessChaosTraceStrainLibrary::TriggerTraceExternalStrain(FString ParamsJson)
{
    return Cubeless::UE58::ChaosTraceStrain::Trigger(ParamsJson);
}
