#include "Registration/CubelessRigVMLinkWriterAttachmentLibrary.h"
#include "Registration/CubelessRigVMLinkWriterToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FRigVMLinkWriterAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString RigVMLinkWriterAttachmentReceipt(UClass* ToolsetClass, FRigVMLinkWriterAttachment Function, const TCHAR* Operation)
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
FString UCubelessRigVMLinkWriterAttachmentLibrary::InstallRigVMLinkWriterToolset(UClass* ToolsetClass)
{
    return RigVMLinkWriterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallRigVMLinkWriterToolset, TEXT("install_rigvm_link_writer_toolset"));
}
FString UCubelessRigVMLinkWriterAttachmentLibrary::VerifyRigVMLinkWriterToolset(UClass* ToolsetClass)
{
    return RigVMLinkWriterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyRigVMLinkWriterToolset, TEXT("verify_rigvm_link_writer_toolset"));
}
FString UCubelessRigVMLinkWriterAttachmentLibrary::RemoveRigVMLinkWriterToolset(UClass* ToolsetClass)
{
    return RigVMLinkWriterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveRigVMLinkWriterToolset, TEXT("remove_rigvm_link_writer_toolset"));
}
