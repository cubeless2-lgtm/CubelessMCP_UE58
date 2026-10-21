#include "Registration/CubelessEditorLevelSessionAttachmentLibrary.h"
#include "Registration/CubelessEditorLevelSessionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FEditorLevelSessionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString EditorLevelSessionAttachmentReceipt(UClass* ToolsetClass, FEditorLevelSessionAttachment Function, const TCHAR* Operation)
{
    auto Report = MakeShared<FJsonObject>(); FString Error;
    const bool Ok = Function(ToolsetClass, Report, Error);
    Report->SetStringField(TEXT("operation"), Operation);
    // Admission-only Verify deliberately returns false; do not promote it to general readiness.
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok);
    Report->SetStringField(TEXT("error"), Error);
    Report->SetBoolField(TEXT("asset_mutation"), false); Report->SetBoolField(TEXT("metadata_mutation"), false);
    Report->SetBoolField(TEXT("actual_named_wire_verified"), false);
    FString Out; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
FString UCubelessEditorLevelSessionAttachmentLibrary::InstallEditorLevelSessionToolset(UClass* ToolsetClass)
{
    return EditorLevelSessionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallEditorLevelSessionToolset, TEXT("install_editor_level_session_toolset"));
}
FString UCubelessEditorLevelSessionAttachmentLibrary::VerifyEditorLevelSessionToolset(UClass* ToolsetClass)
{
    return EditorLevelSessionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyEditorLevelSessionToolset, TEXT("verify_editor_level_session_toolset"));
}
FString UCubelessEditorLevelSessionAttachmentLibrary::RemoveEditorLevelSessionToolset(UClass* ToolsetClass)
{
    return EditorLevelSessionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveEditorLevelSessionToolset, TEXT("remove_editor_level_session_toolset"));
}
