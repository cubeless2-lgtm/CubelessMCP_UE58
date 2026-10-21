#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosSolverSettingsAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosSolverSettingsAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_solver_settings_toolset"))
    static FString InstallChaosSolverSettingsToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_solver_settings_toolset"))
    static FString VerifyChaosSolverSettingsToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_solver_settings_toolset"))
    static FString RemoveChaosSolverSettingsToolset(UClass* ToolsetClass);
};
