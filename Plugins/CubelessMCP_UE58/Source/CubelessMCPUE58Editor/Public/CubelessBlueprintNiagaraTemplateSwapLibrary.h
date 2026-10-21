#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintNiagaraTemplateSwapLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintNiagaraTemplateSwapLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint|Niagara", meta=(ScriptName="inspect_blueprint_niagara_component_template"))
    static FString InspectBlueprintNiagaraComponentTemplate(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint|Niagara", meta=(ScriptName="dry_run_blueprint_niagara_component_template_asset_swap"))
    static FString DryRunBlueprintNiagaraComponentTemplateAssetSwap(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint|Niagara", meta=(ScriptName="execute_blueprint_niagara_component_template_asset_swap_canary"))
    static FString ExecuteBlueprintNiagaraComponentTemplateAssetSwapCanary(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint|Niagara", meta=(ScriptName="execute_blueprint_niagara_component_template_asset_swap_durable"))
    static FString ExecuteBlueprintNiagaraComponentTemplateAssetSwapDurable(const FString& ParamsJson);
};
