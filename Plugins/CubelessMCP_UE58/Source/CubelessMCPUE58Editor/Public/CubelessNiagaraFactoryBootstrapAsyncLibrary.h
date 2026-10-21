#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraFactoryBootstrapAsyncLibrary.generated.h"
class UCubelessNiagaraFactoryBootstrapJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraFactoryBootstrapAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_create_empty_niagara_system_job"))
    static UCubelessNiagaraFactoryBootstrapJob* StartCreateEmptyNiagaraSystemJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_create_basic_sprite_niagara_system_job"))
    static UCubelessNiagaraFactoryBootstrapJob* StartCreateBasicSpriteNiagaraSystemJob(const FString& ParamsJson);
};
