#include "Registration/CubelessRigVMPinDefaultValidationAttachmentLibrary.h"
#include "Registration/CubelessRigVMPinDefaultValidationToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FRigVMPinDefaultValidationAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString RigVMPinDefaultValidationAttachmentReceipt(UClass* ToolsetClass, FRigVMPinDefaultValidationAttachment Function, const TCHAR* Operation)
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
FString UCubelessRigVMPinDefaultValidationAttachmentLibrary::InstallRigVMPinDefaultValidationToolset(UClass* ToolsetClass)
{
    return RigVMPinDefaultValidationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallRigVMPinDefaultValidationToolset, TEXT("install_rigvm_pin_default_validation_toolset"));
}
FString UCubelessRigVMPinDefaultValidationAttachmentLibrary::VerifyRigVMPinDefaultValidationToolset(UClass* ToolsetClass)
{
    return RigVMPinDefaultValidationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyRigVMPinDefaultValidationToolset, TEXT("verify_rigvm_pin_default_validation_toolset"));
}
FString UCubelessRigVMPinDefaultValidationAttachmentLibrary::RemoveRigVMPinDefaultValidationToolset(UClass* ToolsetClass)
{
    return RigVMPinDefaultValidationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveRigVMPinDefaultValidationToolset, TEXT("remove_rigvm_pin_default_validation_toolset"));
}
