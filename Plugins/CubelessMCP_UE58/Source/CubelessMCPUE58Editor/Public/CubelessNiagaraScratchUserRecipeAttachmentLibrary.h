#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraScratchUserRecipeAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraScratchUserRecipeAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_scratch_user_recipes_toolset"))
    static FString InstallNiagaraScratchUserRecipeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_scratch_user_recipes_toolset"))
    static FString VerifyNiagaraScratchUserRecipeToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_scratch_user_recipes_toolset"))
    static FString RemoveNiagaraScratchUserRecipeToolset(UClass* ToolsetClass);
};
