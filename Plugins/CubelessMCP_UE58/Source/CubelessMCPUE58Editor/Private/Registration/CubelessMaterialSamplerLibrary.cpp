#include "Registration/CubelessMaterialSamplerLibrary.h"
#include "Compatibility/UE58/CubelessMaterialSamplerApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessMaterialSamplerLibrary::SetMaterialTextureSampleSamplerTypeMcp(const FString& MaterialPath,const FString& TexturePath,const FString& SamplerType,bool bDryRun,bool bSave,bool bCompile)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::MaterialSampler::SetSamplerType(MaterialPath,TexturePath,SamplerType,bDryRun,bSave,bCompile),TEXT("set_material_texture_sample_sampler_type_mcp"));
}
