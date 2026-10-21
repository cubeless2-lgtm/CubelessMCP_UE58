#include "Registration/CubelessLegacyChaosClothingAttachmentLibrary.h"
#include "Registration/CubelessLegacyChaosClothingToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FLegacyChaosClothingAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString LegacyChaosClothingAttachmentReceipt(UClass* ToolsetClass, FLegacyChaosClothingAttachment Function, const TCHAR* Operation)
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
FString UCubelessLegacyChaosClothingAttachmentLibrary::InstallLegacyChaosClothingToolset(UClass* ToolsetClass)
{
    return LegacyChaosClothingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallLegacyChaosClothingToolset, TEXT("install_legacy_chaos_clothing_toolset"));
}
FString UCubelessLegacyChaosClothingAttachmentLibrary::VerifyLegacyChaosClothingToolset(UClass* ToolsetClass)
{
    return LegacyChaosClothingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyLegacyChaosClothingToolset, TEXT("verify_legacy_chaos_clothing_toolset"));
}
FString UCubelessLegacyChaosClothingAttachmentLibrary::RemoveLegacyChaosClothingToolset(UClass* ToolsetClass)
{
    return LegacyChaosClothingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveLegacyChaosClothingToolset, TEXT("remove_legacy_chaos_clothing_toolset"));
}
