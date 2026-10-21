#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessActorTransformAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessActorTransformAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_actor_transform_toolset"))
    static FString InstallActorTransformToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_actor_transform_toolset"))
    static FString VerifyActorTransformToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_actor_transform_toolset"))
    static FString RemoveActorTransformToolset(UClass* ToolsetClass);
};
