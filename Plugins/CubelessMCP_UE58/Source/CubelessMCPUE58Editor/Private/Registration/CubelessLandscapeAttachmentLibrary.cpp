#include "Registration/CubelessLandscapeAttachmentLibrary.h"
#include "Registration/CubelessLandscapeToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FLandscapeAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString LandscapeAttachmentReceipt(UClass* ToolsetClass, FLandscapeAttachment Function, const TCHAR* Operation)
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
FString UCubelessLandscapeAttachmentLibrary::InstallLandscapeToolset(UClass* ToolsetClass)
{
    return LandscapeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallLandscapeToolset, TEXT("install_landscape_toolset"));
}
FString UCubelessLandscapeAttachmentLibrary::VerifyLandscapeToolset(UClass* ToolsetClass)
{
    return LandscapeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyLandscapeToolset, TEXT("verify_landscape_toolset"));
}
FString UCubelessLandscapeAttachmentLibrary::RemoveLandscapeToolset(UClass* ToolsetClass)
{
    return LandscapeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveLandscapeToolset, TEXT("remove_landscape_toolset"));
}
