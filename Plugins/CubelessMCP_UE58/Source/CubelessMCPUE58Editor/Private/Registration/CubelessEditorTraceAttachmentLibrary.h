#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessEditorTraceAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessEditorTraceAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no world trace or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_editor_trace_toolset"))
    static FString InstallEditorTraceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_editor_trace_toolset"))
    static FString VerifyEditorTraceToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_editor_trace_toolset"))
    static FString RemoveEditorTraceToolset(UClass* ToolsetClass);
};
