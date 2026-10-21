#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessStateTreeMutationLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessStateTreeMutationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|StateTree", meta=(AIIgnore, ScriptName="add_state_tree_state"))
    static FString AddStateTreeState(const FString& AssetPath, const FString& StateName, const FString& ParentPath,
        const FString& StateType, const FString& SelectionBehavior, bool bDryRun);
    UFUNCTION(BlueprintCallable, Category="Cubeless|StateTree", meta=(AIIgnore, ScriptName="compile_state_tree"))
    static FString CompileStateTree(const FString& AssetPath, bool bSave);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore, ScriptName="install_state_tree_strict_dry_run_toolset"))
    static FString InstallStateTreeStrictDryRunToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore, ScriptName="verify_state_tree_strict_dry_run_toolset"))
    static FString VerifyStateTreeStrictDryRunToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore, ScriptName="remove_state_tree_strict_dry_run_toolset"))
    static FString RemoveStateTreeStrictDryRunToolset(UClass* ToolsetClass);
};
