#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGGraphAuthoringAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPCGGraphAuthoringAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_pcg_graph_authoring_toolset"))
    static FString InstallPCGGraphAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_pcg_graph_authoring_toolset"))
    static FString VerifyPCGGraphAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_pcg_graph_authoring_toolset"))
    static FString RemovePCGGraphAuthoringToolset(UClass* ToolsetClass);
};
