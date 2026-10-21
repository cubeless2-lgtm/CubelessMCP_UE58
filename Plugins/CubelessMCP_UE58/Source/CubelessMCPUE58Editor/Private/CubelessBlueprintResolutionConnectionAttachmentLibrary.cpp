#include "CubelessBlueprintResolutionConnectionAttachmentLibrary.h"
#include "Registration/CubelessBlueprintResolutionConnectionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FBlueprintResolutionConnectionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString BlueprintResolutionConnectionAttachmentReceipt(UClass* ToolsetClass, FBlueprintResolutionConnectionAttachment Function, const TCHAR* Operation)
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
FString UCubelessBlueprintResolutionConnectionAttachmentLibrary::InstallBlueprintResolutionConnectionToolset(UClass* ToolsetClass)
{
    return BlueprintResolutionConnectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallBlueprintResolutionConnectionToolset, TEXT("install_blueprint_resolution_connection_toolset"));
}
FString UCubelessBlueprintResolutionConnectionAttachmentLibrary::VerifyBlueprintResolutionConnectionToolset(UClass* ToolsetClass)
{
    return BlueprintResolutionConnectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyBlueprintResolutionConnectionToolset, TEXT("verify_blueprint_resolution_connection_toolset"));
}
FString UCubelessBlueprintResolutionConnectionAttachmentLibrary::RemoveBlueprintResolutionConnectionToolset(UClass* ToolsetClass)
{
    return BlueprintResolutionConnectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveBlueprintResolutionConnectionToolset, TEXT("remove_blueprint_resolution_connection_toolset"));
}
