#include "Registration/CubelessBlendSpaceVariantAttachmentLibrary.h"
#include "Registration/CubelessBlendSpaceVariantToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FBlendSpaceVariantAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString BlendSpaceVariantAttachmentReceipt(UClass* ToolsetClass, FBlendSpaceVariantAttachment Function, const TCHAR* Operation)
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
FString UCubelessBlendSpaceVariantAttachmentLibrary::InstallBlendSpaceVariantToolset(UClass* ToolsetClass)
{
    return BlendSpaceVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallBlendSpaceVariantToolset, TEXT("install_blendspace_variant_toolset"));
}
FString UCubelessBlendSpaceVariantAttachmentLibrary::VerifyBlendSpaceVariantToolset(UClass* ToolsetClass)
{
    return BlendSpaceVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyBlendSpaceVariantToolset, TEXT("verify_blendspace_variant_toolset"));
}
FString UCubelessBlendSpaceVariantAttachmentLibrary::RemoveBlendSpaceVariantToolset(UClass* ToolsetClass)
{
    return BlendSpaceVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveBlendSpaceVariantToolset, TEXT("remove_blendspace_variant_toolset"));
}
