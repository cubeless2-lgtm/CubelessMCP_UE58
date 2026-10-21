#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraModuleInputsBatchAsyncLibrary.generated.h"
class UCubelessNiagaraModuleInputsBatchJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraModuleInputsBatchAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_set_niagara_module_inputs_batch_job"))
    static UCubelessNiagaraModuleInputsBatchJob* StartSetNiagaraModuleInputsBatchJob(const FString& ParamsJson);
};
