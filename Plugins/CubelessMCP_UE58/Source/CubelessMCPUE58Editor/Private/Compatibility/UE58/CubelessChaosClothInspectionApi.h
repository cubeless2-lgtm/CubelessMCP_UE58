#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::ChaosClothInspection
{
    // Editor/game thread observation only. Public SDK may synchronously wait or evaluate upstream Dataflow.
    FString InspectAsset(const FString& ParamsJson);
    FString InspectSimulationMesh(const FString& ParamsJson);
}
