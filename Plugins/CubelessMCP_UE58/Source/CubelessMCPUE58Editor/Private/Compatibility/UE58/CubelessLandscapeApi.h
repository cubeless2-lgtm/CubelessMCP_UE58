#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::LandscapeAuthoring
{
Variable::FResult List();
Variable::FResult Inspect(const FString& ActorLabel,const FString& PrivateContextJson);
Variable::FResult Create(const FString& ActorLabel,const FString& MaterialPath,const FString& LocationJson,const FString& ScaleJson,
    int64 ComponentCountX,int64 ComponentCountY,int64 SectionsPerComponent,int64 QuadsPerSection,int64 HeightValue,
    bool bAllowNonTempLevel,const FString& RequiredRoot,const FString& PrivateContextJson);
Variable::FResult SetMaterial(const FString& MaterialPath,const FString& ActorLabel,const FString& PrivateContextJson);
}
