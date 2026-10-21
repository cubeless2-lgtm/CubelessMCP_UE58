#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraResourceBindingLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraResourceBindingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="set_niagara_render_target2d_module_input"))
    static FString SetNiagaraRenderTarget2DModuleInput(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="set_niagara_texture2d_module_input"))
    static FString SetNiagaraTexture2DModuleInput(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="set_niagara_scratch_pad_function_input_default"))
    static FString SetNiagaraScratchPadFunctionInputDefault(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(ScriptName="link_niagara_scratch_pad_pin_to_user_parameter"))
    static FString LinkNiagaraScratchPadPinToUserParameter(const FString& ParamsJson);
};
