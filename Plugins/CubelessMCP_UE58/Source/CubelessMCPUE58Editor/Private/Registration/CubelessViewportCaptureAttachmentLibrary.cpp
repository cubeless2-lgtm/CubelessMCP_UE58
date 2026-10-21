#include "Registration/CubelessViewportCaptureAttachmentLibrary.h"
#include "Registration/CubelessViewportCaptureToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FViewportCaptureAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ViewportCaptureAttachmentReceipt(UClass* ToolsetClass, FViewportCaptureAttachment Function, const TCHAR* Operation)
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
FString UCubelessViewportCaptureAttachmentLibrary::InstallViewportCaptureToolset(UClass* ToolsetClass)
{
    return ViewportCaptureAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallViewportCaptureToolset, TEXT("install_viewport_capture_toolset"));
}
FString UCubelessViewportCaptureAttachmentLibrary::VerifyViewportCaptureToolset(UClass* ToolsetClass)
{
    return ViewportCaptureAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyViewportCaptureToolset, TEXT("verify_viewport_capture_toolset"));
}
FString UCubelessViewportCaptureAttachmentLibrary::RemoveViewportCaptureToolset(UClass* ToolsetClass)
{
    return ViewportCaptureAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveViewportCaptureToolset, TEXT("remove_viewport_capture_toolset"));
}
