#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraFactoryBootstrapLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraFactoryBootstrapLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="inspect_niagara_factory_bootstrap_defaults"))
    static FString InspectNiagaraFactoryBootstrapDefaults(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="create_empty_niagara_system"))
    static FString CreateEmptyNiagaraSystem(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="create_empty_niagara_emitter"))
    static FString CreateEmptyNiagaraEmitter(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="create_basic_sprite_niagara_system"))
    static FString CreateBasicSpriteNiagaraSystem(const FString& ParamsJson);
};
