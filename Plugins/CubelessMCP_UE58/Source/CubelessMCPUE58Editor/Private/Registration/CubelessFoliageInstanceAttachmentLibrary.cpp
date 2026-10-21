#include "Registration/CubelessFoliageInstanceAttachmentLibrary.h"
#include "Registration/CubelessFoliageInstanceToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FFoliageInstanceAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString FoliageInstanceAttachmentReceipt(UClass* ToolsetClass, FFoliageInstanceAttachment Function, const TCHAR* Operation)
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
FString UCubelessFoliageInstanceAttachmentLibrary::InstallFoliageInstanceToolset(UClass* ToolsetClass)
{
    return FoliageInstanceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallFoliageInstanceToolset, TEXT("install_foliage_instances_toolset"));
}
FString UCubelessFoliageInstanceAttachmentLibrary::VerifyFoliageInstanceToolset(UClass* ToolsetClass)
{
    return FoliageInstanceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyFoliageInstanceToolset, TEXT("verify_foliage_instances_toolset"));
}
FString UCubelessFoliageInstanceAttachmentLibrary::RemoveFoliageInstanceToolset(UClass* ToolsetClass)
{
    return FoliageInstanceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveFoliageInstanceToolset, TEXT("remove_foliage_instances_toolset"));
}
