#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraSpritePresetAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraSpritePresetAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_niagara_sprite_preset_authoring_toolset"))
    static FString InstallNiagaraSpritePresetToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_niagara_sprite_preset_authoring_toolset"))
    static FString VerifyNiagaraSpritePresetToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_niagara_sprite_preset_authoring_toolset"))
    static FString RemoveNiagaraSpritePresetToolset(UClass* ToolsetClass);
};
