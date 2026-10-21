#include "Registration/CubelessLinkedAnimGraphVariantAttachmentLibrary.h"
#include "Registration/CubelessLinkedAnimGraphVariantToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FLinkedAnimGraphVariantAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString LinkedAnimGraphVariantAttachmentReceipt(UClass* ToolsetClass, FLinkedAnimGraphVariantAttachment Function, const TCHAR* Operation)
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
FString UCubelessLinkedAnimGraphVariantAttachmentLibrary::InstallLinkedAnimGraphVariantToolset(UClass* ToolsetClass)
{
    return LinkedAnimGraphVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallLinkedAnimGraphVariantToolset, TEXT("install_linked_anim_graph_variant_toolset"));
}
FString UCubelessLinkedAnimGraphVariantAttachmentLibrary::VerifyLinkedAnimGraphVariantToolset(UClass* ToolsetClass)
{
    return LinkedAnimGraphVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyLinkedAnimGraphVariantToolset, TEXT("verify_linked_anim_graph_variant_toolset"));
}
FString UCubelessLinkedAnimGraphVariantAttachmentLibrary::RemoveLinkedAnimGraphVariantToolset(UClass* ToolsetClass)
{
    return LinkedAnimGraphVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveLinkedAnimGraphVariantToolset, TEXT("remove_linked_anim_graph_variant_toolset"));
}
