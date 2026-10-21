#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessTextureMaterialAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessTextureMaterialAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Import requires the owned standalone Named ticker; Apply retains the idle Editor context.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore, ScriptName="import_texture_file"))
    static FString ImportTextureFile(const FString& ImagePath, const FString& UnrealFolder, const FString& AssetName);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore, ScriptName="apply_texture_material"))
    static FString ApplyTextureMaterial(const FString& Target, const FString& MaterialAssetPath, int32 MaterialSlot);
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_texture_material_authoring_toolset"))
    static FString InstallTextureMaterialToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_texture_material_authoring_toolset"))
    static FString VerifyTextureMaterialToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_texture_material_authoring_toolset"))
    static FString RemoveTextureMaterialToolset(UClass* ToolsetClass);
};
namespace Cubeless::Registration { bool TextureMaterialStandaloneImportAdmitted(); }
