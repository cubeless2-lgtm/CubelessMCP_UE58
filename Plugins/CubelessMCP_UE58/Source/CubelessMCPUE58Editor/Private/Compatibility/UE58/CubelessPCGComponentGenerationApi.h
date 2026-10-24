#pragma once
#include "CoreMinimal.h"

class UPCGGraph;
class UPCGGraphInstance;
class FJsonObject;

namespace Cubeless::UE58::PCGComponentGeneration
{
    // Original Native raw contracts only. Named external wait/fallback remain Python compatibility concerns.
    // GT shutdown: detach telemetry only; never cancel engine work.
    void ShutdownRefreshTracking();
    FString RefreshComponents(const FString& ParamsJson);
    FString BindGenerationSettings(const FString& ParamsJson);
    // Shared Editor-only logical observer; no UObject writer/refresh/normalization.
    bool ReadScalarUserParameters(const UPCGGraph* Graph, const UPCGGraphInstance* Instance, TSharedPtr<FJsonObject>& Out, FString& Error);
    bool SameScalarUserParameters(const TSharedPtr<FJsonObject>& Expected, const TSharedPtr<FJsonObject>& Actual, FString& Error);
}
