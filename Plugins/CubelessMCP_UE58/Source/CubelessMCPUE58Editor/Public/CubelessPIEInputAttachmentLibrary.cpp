#include "CubelessPIEInputAttachmentLibrary.h"
#include "Registration/CubelessPIEInputToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FPIEInputAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString PIEInputAttachmentReceipt(UClass* ToolsetClass, FPIEInputAttachment Function, const TCHAR* Operation)
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
FString UCubelessPIEInputAttachmentLibrary::InstallPIEInputToolset(UClass* ToolsetClass)
{
    return PIEInputAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallPIEInputToolset, TEXT("install_pie_input_toolset"));
}
FString UCubelessPIEInputAttachmentLibrary::VerifyPIEInputToolset(UClass* ToolsetClass)
{
    return PIEInputAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyPIEInputToolset, TEXT("verify_pie_input_toolset"));
}
FString UCubelessPIEInputAttachmentLibrary::RemovePIEInputToolset(UClass* ToolsetClass)
{
    return PIEInputAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemovePIEInputToolset, TEXT("remove_pie_input_toolset"));
}
