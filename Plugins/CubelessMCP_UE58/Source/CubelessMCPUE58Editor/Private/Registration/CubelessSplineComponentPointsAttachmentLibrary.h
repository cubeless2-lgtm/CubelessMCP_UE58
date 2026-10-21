#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessSplineComponentPointsAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessSplineComponentPointsAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_spline_component_points_toolset"))
    static FString InstallSplineComponentPointsToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_spline_component_points_toolset"))
    static FString VerifySplineComponentPointsToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_spline_component_points_toolset"))
    static FString RemoveSplineComponentPointsToolset(UClass* ToolsetClass);
};
