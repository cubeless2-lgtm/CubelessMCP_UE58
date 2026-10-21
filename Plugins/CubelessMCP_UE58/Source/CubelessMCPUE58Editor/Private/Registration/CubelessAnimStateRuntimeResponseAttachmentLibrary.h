#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimStateRuntimeResponseAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimStateRuntimeResponseAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no runtime observation or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_anim_state_machine_runtime_response_toolset"))
    static FString InstallAnimStateRuntimeResponseToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_anim_state_machine_runtime_response_toolset"))
    static FString VerifyAnimStateRuntimeResponseToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_anim_state_machine_runtime_response_toolset"))
    static FString RemoveAnimStateRuntimeResponseToolset(UClass* ToolsetClass);
};
