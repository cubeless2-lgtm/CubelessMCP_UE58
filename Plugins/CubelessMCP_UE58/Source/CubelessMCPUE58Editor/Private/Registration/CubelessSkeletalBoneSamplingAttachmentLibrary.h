#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessSkeletalBoneSamplingAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessSkeletalBoneSamplingAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_skeletal_bone_sampling_toolset"))
    static FString InstallSkeletalBoneSamplingToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_skeletal_bone_sampling_toolset"))
    static FString VerifySkeletalBoneSamplingToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_skeletal_bone_sampling_toolset"))
    static FString RemoveSkeletalBoneSamplingToolset(UClass* ToolsetClass);
};
