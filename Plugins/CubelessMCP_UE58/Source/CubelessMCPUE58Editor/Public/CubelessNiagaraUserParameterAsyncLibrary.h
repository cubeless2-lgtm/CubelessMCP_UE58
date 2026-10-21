#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraUserParameterAsyncLibrary.generated.h"
class UCubelessNiagaraUserParameterJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraUserParameterAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Dedicated User-parameter removal only. ParamsJson retains the original public/raw contract.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_remove_user_parameters_job"))
    static UCubelessNiagaraUserParameterJob* StartRemoveUserParametersJob(const FString& ParamsJson);
};
