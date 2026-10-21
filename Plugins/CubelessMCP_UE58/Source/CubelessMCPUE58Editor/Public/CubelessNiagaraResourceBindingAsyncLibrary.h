#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraResourceBindingAsyncLibrary.generated.h"
class UCubelessNiagaraResourceBindingJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraResourceBindingAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_set_niagara_render_target2d_module_input_job"))
    static UCubelessNiagaraResourceBindingJob* StartSetNiagaraRenderTarget2DModuleInputJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_set_niagara_texture2d_module_input_job"))
    static UCubelessNiagaraResourceBindingJob* StartSetNiagaraTexture2DModuleInputJob(const FString& ParamsJson);
};
