#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraInputParameterAsyncLibrary.generated.h"
class UCubelessNiagaraInputParameterJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraInputParameterAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_set_niagara_module_static_switch_job"))
    static UCubelessNiagaraInputParameterJob* StartSetNiagaraModuleStaticSwitchJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_set_niagara_module_input_value_job"))
    static UCubelessNiagaraInputParameterJob* StartSetNiagaraModuleInputValueJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_create_niagara_module_input_override_job"))
    static UCubelessNiagaraInputParameterJob* StartCreateNiagaraModuleInputOverrideJob(const FString& ParamsJson);
};
