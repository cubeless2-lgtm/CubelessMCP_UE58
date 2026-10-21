#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraEmitterConfigurationLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEmitterConfigurationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Emitter Configuration", meta=(ScriptName="inspect_niagara_emitter_properties"))
    static FString InspectNiagaraEmitterProperties(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Emitter Configuration", meta=(ScriptName="set_niagara_emitter_enabled"))
    static FString SetNiagaraEmitterEnabled(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Emitter Configuration", meta=(ScriptName="set_niagara_emitter_local_space"))
    static FString SetNiagaraEmitterLocalSpace(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Emitter Configuration", meta=(ScriptName="set_niagara_emitter_sim_target"))
    static FString SetNiagaraEmitterSimTarget(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Emitter Configuration", meta=(ScriptName="set_niagara_emitter_fixed_bounds"))
    static FString SetNiagaraEmitterFixedBounds(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Emitter Configuration", meta=(ScriptName="set_niagara_emitter_persistent_ids"))
    static FString SetNiagaraEmitterPersistentIds(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Emitter Configuration", meta=(ScriptName="remove_niagara_emitter"))
    static FString RemoveNiagaraEmitter(const FString& ParamsJson);
};
