#include "CubelessPCGInspectionAttachmentLibrary.h"
#include "Registration/CubelessPCGInspectionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FPCGInspectionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString PCGInspectionAttachmentReceipt(UClass* ToolsetClass, FPCGInspectionAttachment Function, const TCHAR* Operation)
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
FString UCubelessPCGInspectionAttachmentLibrary::InstallPCGInspectionToolset(UClass* ToolsetClass)
{
    return PCGInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallPCGInspectionToolset, TEXT("install_pcg_inspection_toolset"));
}
FString UCubelessPCGInspectionAttachmentLibrary::VerifyPCGInspectionToolset(UClass* ToolsetClass)
{
    return PCGInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyPCGInspectionToolset, TEXT("verify_pcg_inspection_toolset"));
}
FString UCubelessPCGInspectionAttachmentLibrary::RemovePCGInspectionToolset(UClass* ToolsetClass)
{
    return PCGInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemovePCGInspectionToolset, TEXT("remove_pcg_inspection_toolset"));
}
