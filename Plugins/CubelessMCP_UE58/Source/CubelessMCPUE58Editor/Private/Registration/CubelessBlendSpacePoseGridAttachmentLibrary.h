#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlendSpacePoseGridAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlendSpacePoseGridAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_blendspace_pose_grid_toolset"))
    static FString InstallBlendSpacePoseGridToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_blendspace_pose_grid_toolset"))
    static FString VerifyBlendSpacePoseGridToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_blendspace_pose_grid_toolset"))
    static FString RemoveBlendSpacePoseGridToolset(UClass* ToolsetClass);
};
