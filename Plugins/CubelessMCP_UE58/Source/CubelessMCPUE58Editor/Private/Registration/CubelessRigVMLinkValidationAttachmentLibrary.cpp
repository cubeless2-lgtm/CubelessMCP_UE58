#include "Registration/CubelessRigVMLinkValidationAttachmentLibrary.h"
#include "Registration/CubelessRigVMLinkValidationToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FRigVMLinkValidationAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString RigVMLinkValidationAttachmentReceipt(UClass* ToolsetClass, FRigVMLinkValidationAttachment Function, const TCHAR* Operation)
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
FString UCubelessRigVMLinkValidationAttachmentLibrary::InstallRigVMLinkValidationToolset(UClass* ToolsetClass)
{
    return RigVMLinkValidationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallRigVMLinkValidationToolset, TEXT("install_rigvm_link_validation_toolset"));
}
FString UCubelessRigVMLinkValidationAttachmentLibrary::VerifyRigVMLinkValidationToolset(UClass* ToolsetClass)
{
    return RigVMLinkValidationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyRigVMLinkValidationToolset, TEXT("verify_rigvm_link_validation_toolset"));
}
FString UCubelessRigVMLinkValidationAttachmentLibrary::RemoveRigVMLinkValidationToolset(UClass* ToolsetClass)
{
    return RigVMLinkValidationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveRigVMLinkValidationToolset, TEXT("remove_rigvm_link_validation_toolset"));
}
