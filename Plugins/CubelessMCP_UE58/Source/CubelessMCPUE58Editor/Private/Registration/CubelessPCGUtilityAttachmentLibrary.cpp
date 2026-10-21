#include "CubelessPCGUtilityAttachmentLibrary.h"
#include "Registration/CubelessPCGUtilityToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FPCGUtilityAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString PCGUtilityAttachmentReceipt(UClass* ToolsetClass, FPCGUtilityAttachment Function, const TCHAR* Operation)
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
FString UCubelessPCGUtilityAttachmentLibrary::InstallPCGUtilityToolset(UClass* ToolsetClass)
{
    return PCGUtilityAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallPCGUtilityToolset, TEXT("install_pcg_utilities_toolset"));
}
FString UCubelessPCGUtilityAttachmentLibrary::VerifyPCGUtilityToolset(UClass* ToolsetClass)
{
    return PCGUtilityAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyPCGUtilityToolset, TEXT("verify_pcg_utilities_toolset"));
}
FString UCubelessPCGUtilityAttachmentLibrary::RemovePCGUtilityToolset(UClass* ToolsetClass)
{
    return PCGUtilityAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemovePCGUtilityToolset, TEXT("remove_pcg_utilities_toolset"));
}
