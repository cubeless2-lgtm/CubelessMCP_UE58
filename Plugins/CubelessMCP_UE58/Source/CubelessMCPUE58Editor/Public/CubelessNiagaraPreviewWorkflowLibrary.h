#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraPreviewWorkflowLibrary.generated.h"

class UCubelessNiagaraPreviewWorkflowJob;

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraPreviewWorkflowLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(ScriptName="open_niagara_preview_player"))
    static FString OpenNiagaraPreviewPlayer(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(ScriptName="get_niagara_preview_player_state"))
    static FString GetNiagaraPreviewPlayerState(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(ScriptName="preview_niagara_component_asset_swap"))
    static FString PreviewNiagaraComponentAssetSwap(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(ScriptName="sample_niagara_system_in_preview_lab"))
    static FString SampleNiagaraSystemInPreviewLab(const FString& ParamsJson);

    // Internal Named-MCP continuation only; no additional public tool arguments.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(AIIgnore, ScriptName="start_open_player_job"))
    static UCubelessNiagaraPreviewWorkflowJob* StartOpenPlayerJob(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(AIIgnore, ScriptName="start_preview_component_swap_job"))
    static UCubelessNiagaraPreviewWorkflowJob* StartPreviewComponentSwapJob(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(AIIgnore, ScriptName="start_sample_lab_job"))
    static UCubelessNiagaraPreviewWorkflowJob* StartSampleLabJob(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara|Preview", meta=(AIIgnore, ScriptName="start_preview_lab_job"))
    static UCubelessNiagaraPreviewWorkflowJob* StartPreviewLabJob(const FString& ParamsJson);
};
