#include "CubelessBlueprintPropertyGraphAttachmentLibrary.h"
#include "Registration/CubelessBlueprintPropertyGraphToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FBlueprintPropertyGraphAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString BlueprintPropertyGraphAttachmentReceipt(UClass* ToolsetClass, FBlueprintPropertyGraphAttachment Function, const TCHAR* Operation)
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
FString UCubelessBlueprintPropertyGraphAttachmentLibrary::InstallBlueprintPropertyGraphToolset(UClass* ToolsetClass)
{
    return BlueprintPropertyGraphAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallBlueprintPropertyGraphToolset, TEXT("install_blueprint_property_graph_toolset"));
}
FString UCubelessBlueprintPropertyGraphAttachmentLibrary::VerifyBlueprintPropertyGraphToolset(UClass* ToolsetClass)
{
    return BlueprintPropertyGraphAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyBlueprintPropertyGraphToolset, TEXT("verify_blueprint_property_graph_toolset"));
}
FString UCubelessBlueprintPropertyGraphAttachmentLibrary::RemoveBlueprintPropertyGraphToolset(UClass* ToolsetClass)
{
    return BlueprintPropertyGraphAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveBlueprintPropertyGraphToolset, TEXT("remove_blueprint_property_graph_toolset"));
}
