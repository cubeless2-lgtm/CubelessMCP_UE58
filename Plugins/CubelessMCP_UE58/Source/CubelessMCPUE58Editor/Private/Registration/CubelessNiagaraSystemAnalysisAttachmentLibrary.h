#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraSystemAnalysisAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraSystemAnalysisAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_system_analysis_toolset"))
    static FString InstallNiagaraSystemAnalysisToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_system_analysis_toolset"))
    static FString VerifyNiagaraSystemAnalysisToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_system_analysis_toolset"))
    static FString RemoveNiagaraSystemAnalysisToolset(UClass* ToolsetClass);
};
