#include "Registration/CubelessEditorTraceAttachmentLibrary.h"
#include "Registration/CubelessEditorTraceToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FEditorTraceAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString EditorTraceAttachmentReceipt(UClass* ToolsetClass, FEditorTraceAttachment Function, const TCHAR* Operation)
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
FString UCubelessEditorTraceAttachmentLibrary::InstallEditorTraceToolset(UClass* ToolsetClass)
{
    return EditorTraceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallEditorTraceToolset, TEXT("install_editor_trace_toolset"));
}
FString UCubelessEditorTraceAttachmentLibrary::VerifyEditorTraceToolset(UClass* ToolsetClass)
{
    return EditorTraceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyEditorTraceToolset, TEXT("verify_editor_trace_toolset"));
}
FString UCubelessEditorTraceAttachmentLibrary::RemoveEditorTraceToolset(UClass* ToolsetClass)
{
    return EditorTraceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveEditorTraceToolset, TEXT("remove_editor_trace_toolset"));
}
