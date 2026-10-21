#include "Registration/CubelessPostProcessAnimDemoVariantAttachmentLibrary.h"
#include "Registration/CubelessPostProcessAnimDemoVariantToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FPostProcessAnimDemoVariantAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString PostProcessAnimDemoVariantAttachmentReceipt(UClass* ToolsetClass, FPostProcessAnimDemoVariantAttachment Function, const TCHAR* Operation)
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
FString UCubelessPostProcessAnimDemoVariantAttachmentLibrary::InstallPostProcessAnimDemoVariantToolset(UClass* ToolsetClass)
{
    return PostProcessAnimDemoVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallPostProcessAnimDemoVariantToolset, TEXT("install_postprocess_anim_demo_variant_toolset"));
}
FString UCubelessPostProcessAnimDemoVariantAttachmentLibrary::VerifyPostProcessAnimDemoVariantToolset(UClass* ToolsetClass)
{
    return PostProcessAnimDemoVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyPostProcessAnimDemoVariantToolset, TEXT("verify_postprocess_anim_demo_variant_toolset"));
}
FString UCubelessPostProcessAnimDemoVariantAttachmentLibrary::RemovePostProcessAnimDemoVariantToolset(UClass* ToolsetClass)
{
    return PostProcessAnimDemoVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemovePostProcessAnimDemoVariantToolset, TEXT("remove_postprocess_anim_demo_variant_toolset"));
}
