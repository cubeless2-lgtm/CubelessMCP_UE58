#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessEditorLevelSessionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessEditorLevelSessionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_editor_level_session_toolset"))
    static FString InstallEditorLevelSessionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_editor_level_session_toolset"))
    static FString VerifyEditorLevelSessionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_editor_level_session_toolset"))
    static FString RemoveEditorLevelSessionToolset(UClass* ToolsetClass);
};
