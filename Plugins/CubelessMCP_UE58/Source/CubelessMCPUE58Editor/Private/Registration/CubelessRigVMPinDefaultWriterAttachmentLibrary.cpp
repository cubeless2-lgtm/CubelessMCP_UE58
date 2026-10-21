#include "Registration/CubelessRigVMPinDefaultWriterAttachmentLibrary.h"
#include "Registration/CubelessRigVMPinDefaultWriterToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FRigVMPinDefaultWriterAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString RigVMPinDefaultWriterAttachmentReceipt(UClass* ToolsetClass, FRigVMPinDefaultWriterAttachment Function, const TCHAR* Operation)
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
FString UCubelessRigVMPinDefaultWriterAttachmentLibrary::InstallRigVMPinDefaultWriterToolset(UClass* ToolsetClass)
{
    return RigVMPinDefaultWriterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallRigVMPinDefaultWriterToolset, TEXT("install_rigvm_pin_default_writer_toolset"));
}
FString UCubelessRigVMPinDefaultWriterAttachmentLibrary::VerifyRigVMPinDefaultWriterToolset(UClass* ToolsetClass)
{
    return RigVMPinDefaultWriterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyRigVMPinDefaultWriterToolset, TEXT("verify_rigvm_pin_default_writer_toolset"));
}
FString UCubelessRigVMPinDefaultWriterAttachmentLibrary::RemoveRigVMPinDefaultWriterToolset(UClass* ToolsetClass)
{
    return RigVMPinDefaultWriterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveRigVMPinDefaultWriterToolset, TEXT("remove_rigvm_pin_default_writer_toolset"));
}
