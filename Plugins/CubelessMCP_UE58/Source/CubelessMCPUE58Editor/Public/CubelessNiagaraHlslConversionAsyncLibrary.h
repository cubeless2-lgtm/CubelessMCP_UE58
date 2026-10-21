#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraHlslConversionAsyncLibrary.generated.h"
class UCubelessNiagaraHlslConversionJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraHlslConversionAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_replace_niagara_custom_hlsl_job"))
    static UCubelessNiagaraHlslConversionJob* StartReplaceNiagaraCustomHlslJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_apply_niagara_scratch_pad_graph_recipe_job"))
    static UCubelessNiagaraHlslConversionJob* StartApplyNiagaraScratchPadGraphRecipeJob(const FString& ParamsJson);
};
