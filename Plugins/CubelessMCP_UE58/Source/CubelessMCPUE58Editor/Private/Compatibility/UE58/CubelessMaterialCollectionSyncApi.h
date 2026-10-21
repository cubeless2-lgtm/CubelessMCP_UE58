#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::MaterialCollectionSync
{
// Root-owned editor-module lifetime: activate before registration, shutdown before unregister/unload.
void Activate();
void Shutdown();
Variable::FResult Control(const FString& Action,const FString& SourcePath,const FString& TargetPath,
    const FString& ParameterNamesJson,double IntervalSeconds,bool bDefaultsOnEnable,bool bSaveOnEnable);
}
