#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessViewportCaptureAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessViewportCaptureAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_viewport_capture_toolset"))
    static FString InstallViewportCaptureToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_viewport_capture_toolset"))
    static FString VerifyViewportCaptureToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_viewport_capture_toolset"))
    static FString RemoveViewportCaptureToolset(UClass* ToolsetClass);
};
