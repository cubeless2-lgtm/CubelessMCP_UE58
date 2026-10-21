#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialSamplerLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessMaterialSamplerLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="set_material_texture_sample_sampler_type_mcp"))
    static FString SetMaterialTextureSampleSamplerTypeMcp(const FString& MaterialPath,const FString& TexturePath,const FString& SamplerType,bool bDryRun,bool bSave,bool bCompile);
};
