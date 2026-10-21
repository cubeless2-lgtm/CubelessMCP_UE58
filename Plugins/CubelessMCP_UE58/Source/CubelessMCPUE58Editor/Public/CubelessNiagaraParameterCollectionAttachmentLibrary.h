#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraParameterCollectionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraParameterCollectionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_parameter_collections_toolset"))
    static FString InstallNiagaraParameterCollectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_parameter_collections_toolset"))
    static FString VerifyNiagaraParameterCollectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_parameter_collections_toolset"))
    static FString RemoveNiagaraParameterCollectionToolset(UClass* ToolsetClass);
};
