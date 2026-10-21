#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraDataChannelAuthoringAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraDataChannelAuthoringAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_data_channel_authoring_toolset"))
    static FString InstallNiagaraDataChannelAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_data_channel_authoring_toolset"))
    static FString VerifyNiagaraDataChannelAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_data_channel_authoring_toolset"))
    static FString RemoveNiagaraDataChannelAuthoringToolset(UClass* ToolsetClass);
};
