#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessEditorLevelSessionLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessEditorLevelSessionLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="open_editor_level"))
    static FString OpenEditorLevel(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="get_editor_level_open_status"))
    static FString GetEditorLevelOpenStatus(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="clear_dirty_package_flags"))
    static FString ClearDirtyPackageFlags(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="safe_new_preview_map"))
    static FString SafeNewPreviewMap(FString ParamsJson);
};
