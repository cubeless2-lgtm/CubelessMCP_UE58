#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraParameterCollectionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraParameterCollectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Parameter Collections", meta=(ScriptName="inspect_niagara_parameter_collection_values"))
    static FString InspectNiagaraParameterCollectionValues(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Parameter Collections", meta=(ScriptName="set_niagara_parameter_collection_values"))
    static FString SetNiagaraParameterCollectionValues(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Parameter Collections", meta=(ScriptName="repair_niagara_parameter_collection"))
    static FString RepairNiagaraParameterCollection(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Parameter Collections", meta=(ScriptName="remap_niagara_parameter_collection_references"))
    static FString RemapNiagaraParameterCollectionReferences(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Parameter Collections", meta=(ScriptName="remap_niagara_script_parameter_collection_references"))
    static FString RemapNiagaraScriptParameterCollectionReferences(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Parameter Collections", meta=(ScriptName="recompile_niagara_scripts_in_root"))
    static FString RecompileNiagaraScriptsInRoot(const FString& ParamsJson);
};
