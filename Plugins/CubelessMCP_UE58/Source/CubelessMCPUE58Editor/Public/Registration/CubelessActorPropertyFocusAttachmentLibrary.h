#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessActorPropertyFocusAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessActorPropertyFocusAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_actor_property_focus_toolset"))
    static FString InstallActorPropertyFocusToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_actor_property_focus_toolset"))
    static FString VerifyActorPropertyFocusToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_actor_property_focus_toolset"))
    static FString RemoveActorPropertyFocusToolset(UClass* ToolsetClass);
};
