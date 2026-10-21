#include "CubelessContentReconstructionAttachmentLibrary.h"
#include "Registration/CubelessContentReconstructionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FContentReconstructionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ContentReconstructionAttachmentReceipt(UClass* ToolsetClass, FContentReconstructionAttachment Function, const TCHAR* Operation)
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
FString UCubelessContentReconstructionAttachmentLibrary::InstallContentReconstructionToolset(UClass* ToolsetClass)
{
    return ContentReconstructionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallContentReconstructionToolset, TEXT("install_content_reconstruction_toolset"));
}
FString UCubelessContentReconstructionAttachmentLibrary::VerifyContentReconstructionToolset(UClass* ToolsetClass)
{
    return ContentReconstructionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyContentReconstructionToolset, TEXT("verify_content_reconstruction_toolset"));
}
FString UCubelessContentReconstructionAttachmentLibrary::RemoveContentReconstructionToolset(UClass* ToolsetClass)
{
    return ContentReconstructionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveContentReconstructionToolset, TEXT("remove_content_reconstruction_toolset"));
}
