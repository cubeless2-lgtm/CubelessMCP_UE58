#pragma once
#include "CoreMinimal.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::MaterialSampler
{
Variable::FResult SetSamplerType(const FString& MaterialPath,const FString& TexturePath,const FString& SamplerType,bool Dry,bool Save,bool Compile);
}
