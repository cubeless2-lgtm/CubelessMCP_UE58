#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessControlRigSolverIntentsAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessControlRigSolverIntentsAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no solver validation or engine operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_controlrig_solver_intents_toolset"))
    static FString InstallControlRigSolverIntentsToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_controlrig_solver_intents_toolset"))
    static FString VerifyControlRigSolverIntentsToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_controlrig_solver_intents_toolset"))
    static FString RemoveControlRigSolverIntentsToolset(UClass* ToolsetClass);
};
