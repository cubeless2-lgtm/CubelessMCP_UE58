#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraDataChannelAuthoringLibrary.generated.h"

// Editor-only transient invocation handle. Never referenced by a Niagara asset or runtime graph.
UCLASS(Transient, NotBlueprintable, meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraDataChannelVariadicJob : public UObject
{
    GENERATED_BODY()
};

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraDataChannelAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Data Channel Authoring", meta=(ScriptName="configure_niagara_data_channel_asset"))
    static FString ConfigureNiagaraDataChannelAsset(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Data Channel Authoring", meta=(ScriptName="create_niagara_ndc_reader_scratch_pad_module"))
    static FString CreateNiagaraNdcReaderScratchPadModule(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Data Channel Authoring", meta=(ScriptName="add_niagara_scratch_pad_di_function_call"))
    static FString AddNiagaraScratchPadDiFunctionCall(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Niagara Data Channel Authoring", meta=(ScriptName="add_niagara_scratch_pad_di_variadic_input"))
    static FString AddNiagaraScratchPadDiVariadicInput(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Internal", meta=(AIIgnore, ScriptName="start_niagara_scratch_pad_di_variadic_input_job"))
    static UCubelessNiagaraDataChannelVariadicJob* StartNiagaraScratchPadDiVariadicInputJob(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Internal", meta=(AIIgnore, ScriptName="get_niagara_scratch_pad_di_variadic_input_job_snapshot"))
    static FString GetNiagaraScratchPadDiVariadicInputJobSnapshot(UCubelessNiagaraDataChannelVariadicJob* Job);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Internal", meta=(AIIgnore, ScriptName="cancel_niagara_scratch_pad_di_variadic_input_job"))
    static bool CancelNiagaraScratchPadDiVariadicInputJob(UCubelessNiagaraDataChannelVariadicJob* Job,const FString& Reason);
};
