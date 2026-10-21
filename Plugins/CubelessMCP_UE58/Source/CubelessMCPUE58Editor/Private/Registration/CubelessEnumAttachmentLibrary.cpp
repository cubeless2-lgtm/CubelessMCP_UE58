#include "Registration/CubelessEnumAttachmentLibrary.h"
#include "Registration/CubelessEnumToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FEnumAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString EnumAttachmentReceipt(UClass* ToolsetClass, FEnumAttachment Function, const TCHAR* Operation)
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
FString UCubelessEnumAttachmentLibrary::InstallEnumToolset(UClass* ToolsetClass)
{
    return EnumAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallEnumToolset, TEXT("install_enum_toolset"));
}
FString UCubelessEnumAttachmentLibrary::VerifyEnumToolset(UClass* ToolsetClass)
{
    return EnumAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyEnumToolset, TEXT("verify_enum_toolset"));
}
FString UCubelessEnumAttachmentLibrary::RemoveEnumToolset(UClass* ToolsetClass)
{
    return EnumAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveEnumToolset, TEXT("remove_enum_toolset"));
}
