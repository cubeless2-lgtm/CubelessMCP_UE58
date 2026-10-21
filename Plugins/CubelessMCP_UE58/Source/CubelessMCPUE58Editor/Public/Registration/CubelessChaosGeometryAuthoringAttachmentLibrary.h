#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessChaosGeometryAuthoringAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessChaosGeometryAuthoringAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_chaos_geometry_authoring_toolset"))
    static FString InstallChaosGeometryAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_chaos_geometry_authoring_toolset"))
    static FString VerifyChaosGeometryAuthoringToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_chaos_geometry_authoring_toolset"))
    static FString RemoveChaosGeometryAuthoringToolset(UClass* ToolsetClass);
};
