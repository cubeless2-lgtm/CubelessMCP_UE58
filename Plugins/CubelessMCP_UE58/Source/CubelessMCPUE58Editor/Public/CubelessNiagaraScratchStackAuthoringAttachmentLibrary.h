#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraScratchStackAuthoringAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchStackAuthoringAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_scratch_stack_authoring_toolset"))
    static FString InstallNiagaraScratchStackAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_scratch_stack_authoring_toolset"))
    static FString VerifyNiagaraScratchStackAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_scratch_stack_authoring_toolset"))
    static FString RemoveNiagaraScratchStackAuthoringToolset(UClass* ToolsetClass);
};
