#pragma once
#include "CoreMinimal.h"

class AActor;
class UWorld;
class UGeometryCollectionComponent;

namespace Cubeless::UE58::ChaosCacheSessions
{
FString Record(const FString& ParamsJson);
FString Validate(const FString& ParamsJson);
// Editor-internal read-only lookup; returns no mutable session/list and registers no MCP tool.
TWeakObjectPtr<UGeometryCollectionComponent> FindActiveRecordedComponent(const AActor* Owner,const UWorld* ExpectedWorld);
// Owned ticker cancellation precedes guarded semantic restoration; no forced save on shutdown.
void Shutdown();
// Editor-internal metadata-only lease query; no MCP tool or SDK getter.
bool HasActiveSessionForWorld(const UWorld* World);
}
