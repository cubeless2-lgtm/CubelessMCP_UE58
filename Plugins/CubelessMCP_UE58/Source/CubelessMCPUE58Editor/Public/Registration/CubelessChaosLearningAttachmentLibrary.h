#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosLearningAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosLearningAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_learning_toolset"))
    static FString InstallChaosLearningToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_learning_toolset"))
    static FString VerifyChaosLearningToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_learning_toolset"))
    static FString RemoveChaosLearningToolset(UClass* ToolsetClass);
};
