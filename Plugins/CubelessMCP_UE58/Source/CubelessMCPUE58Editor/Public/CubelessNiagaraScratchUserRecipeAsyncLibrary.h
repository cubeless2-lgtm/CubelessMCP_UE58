#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraScratchUserRecipeAsyncLibrary.generated.h"
class UCubelessNiagaraScratchUserRecipeJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_compile_status_job"))
    static UCubelessNiagaraScratchUserRecipeJob* StartCompileStatusJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_scalar_binding_job"))
    static UCubelessNiagaraScratchUserRecipeJob* StartScalarBindingJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_array_binding_job"))
    static UCubelessNiagaraScratchUserRecipeJob* StartArrayBindingJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Niagara",meta=(AIIgnore,ScriptName="start_remove_niagara_unused_module_input_path_job"))
    static UCubelessNiagaraScratchUserRecipeJob* StartRemoveNiagaraUnusedModuleInputPathJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Niagara",meta=(AIIgnore,ScriptName="start_remap_niagara_script_parameter_collection_references_job"))
    static UCubelessNiagaraScratchUserRecipeJob* StartRemapNiagaraScriptParameterCollectionReferencesJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Niagara",meta=(AIIgnore,ScriptName="start_cleanup_niagara_renderers_job"))
    static UCubelessNiagaraScratchUserRecipeJob* StartCleanupNiagaraRenderersJob(const FString& ParamsJson);
};
