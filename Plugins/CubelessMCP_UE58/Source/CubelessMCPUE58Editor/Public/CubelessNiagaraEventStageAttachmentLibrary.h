#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraEventStageAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraEventStageAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_event_stage_authoring_toolset"))
    static FString InstallNiagaraEventStageToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_event_stage_authoring_toolset"))
    static FString VerifyNiagaraEventStageToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_event_stage_authoring_toolset"))
    static FString RemoveNiagaraEventStageToolset(UClass* ToolsetClass);
};
