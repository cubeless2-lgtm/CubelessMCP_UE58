#include "Registration/CubelessEditorLevelSessionLibrary.h"
#include "Compatibility/UE58/CubelessEditorLevelSessionApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessEditorLevelSessionLibrary::OpenEditorLevel(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::EditorLevelSession::Open(ParamsJson),TEXT("open_editor_level"));
}
FString UCubelessEditorLevelSessionLibrary::GetEditorLevelOpenStatus(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::EditorLevelSession::Status(ParamsJson),TEXT("get_editor_level_open_status"));
}
FString UCubelessEditorLevelSessionLibrary::ClearDirtyPackageFlags(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::EditorLevelSession::ClearDirty(ParamsJson),TEXT("clear_dirty_package_flags"));
}
FString UCubelessEditorLevelSessionLibrary::SafeNewPreviewMap(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::EditorLevelSession::NewPreview(ParamsJson),TEXT("safe_new_preview_map"));
}
