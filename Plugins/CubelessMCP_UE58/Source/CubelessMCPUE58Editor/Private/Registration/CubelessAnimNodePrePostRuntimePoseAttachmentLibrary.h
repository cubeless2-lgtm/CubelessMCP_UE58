#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimNodePrePostRuntimePoseAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessAnimNodePrePostRuntimePoseAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_anim_node_pre_post_runtime_pose_toolset"))
    static FString InstallAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_anim_node_pre_post_runtime_pose_toolset"))
    static FString VerifyAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_anim_node_pre_post_runtime_pose_toolset"))
    static FString RemoveAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass);
};
