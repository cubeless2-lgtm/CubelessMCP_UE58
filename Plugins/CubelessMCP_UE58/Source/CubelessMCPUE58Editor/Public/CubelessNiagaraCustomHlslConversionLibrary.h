#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraCustomHlslConversionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraCustomHlslConversionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="replace_niagara_custom_hlsl"))
    static FString ReplaceNiagaraCustomHlsl(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="apply_niagara_scratch_pad_graph_recipe"))
    static FString ApplyNiagaraScratchPadGraphRecipe(const FString& ParamsJson);
};
